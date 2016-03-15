/** @file

  A brief file description

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

#include "P_EventSystem.h" /* MAGIC_EDITING_TAG */
#include <sched.h>
#if TS_USE_HWLOC
#include <alloca.h>
#include <hwloc.h>
#endif
#include "ts/ink_defs.h"

/// Global singleton.
class EventProcessor eventProcessor;

class ThreadAffinityInitializer : public Continuation
{
  typedef ThreadAffinityInitializer self;

public:
  /// Default construct.
  ThreadAffinityInitializer() { SET_HANDLER(&self::set_affinity); }

  /// Load up basic affinity data.
  void init();
  /// Set the affinity for the current thread.
  int set_affinity(int, Event *);

#if defined(HAVE_HWLOC_OBJ_PU)
private:
  hwloc_ob_type _type;
  int _count;
  char const *_name;
#endif
};

ThreadAffinityInitializer Thread_Affinity_Initializer;

namespace
{
class ThreadInitByFunc : public Continuation
{
public:
  ThreadInitByFunc() { SET_HANDLER(&ThreadInitByFunc::invoke); }
  int
  invoke(int, Event *ev)
  {
    void (*f)(EThread*) = reinterpret_cast< void (*)(EThread*) > (ev->cookie);
    f(ev->ethread);
    return 0;
  }
} Thread_Init_Func;

}

void
ThreadAffinityInitializer::init()
{
#if TS_USE_HWLOC
  int affinity = 1;
  REC_ReadConfigInteger(affinity, "proxy.config.exec_thread.affinity");

  switch (affinity) {
  case 4: // assign threads to logical processing units
// Older versions of libhwloc (eg. Ubuntu 10.04) don't have HWLOC_OBJ_PU.
#if HAVE_HWLOC_OBJ_PU
    _type = HWLOC_OBJ_PU;
    _name = "Logical Processor";
    break;
#endif
  case 3: // assign threads to real cores
    _type = HWLOC_OBJ_CORE;
    _name = "Core";
    break;
  case 1: // assign threads to NUMA nodes (often 1:1 with sockets)
    _type = HWLOC_OBJ_NODE;
    _name = "NUMA Node";
    if (hwloc_get_nbobjs_by_type(ink_get_topology(), _type) > 0) {
      break;
    }
  case 2: // assign threads to sockets
    _type = HWLOC_OBJ_SOCKET;
    _name = "Socket";
    break;
  default: // assign threads to the machine as a whole (a level below SYSTEM)
    _type = HWLOC_OBJ_MACHINE;
    _name = "Machine";
  }

  _count = hwloc_get_nbobjs_by_type(ink_get_topology(), _type);
  Debug("iocore_thread", "Affinity: %d %ss: %d PU: %d", affinity, obj_name, obj_count, ink_number_of_processors());
#endif
}

int
ThreadAffinityInitializer::set_affinity(int, Event *)
{
#if TS_USE_HWLOC
  hwloc_obj_t obj;
  EThread *t = this_ethread();

  if (_count > 0) {
    obj = hwloc_get_obj_by_type(ink_get_topology(), _type, t->id % _count);
#if HWLOC_API_VERSION >= 0x00010100
    int cpu_mask_len = hwloc_bitmap_snprintf(NULL, 0, obj->cpuset) + 1;
    char *cpu_mask = (char *)alloca(cpu_mask_len);
    hwloc_bitmap_snprintf(cpu_mask, cpu_mask_len, obj->cpuset);
    Debug("iocore_thread", "EThread: %d %s: %d CPU Mask: %s\n", i, _name, obj->logical_index, cpu_mask);
#else
    Debug("iocore_thread", "EThread: %d %s: %d\n", i, _name, obj->logical_index);
#endif // HWLOC_API_VERSION
    hwloc_set_thread_cpubind(ink_get_topology(), t->tid, obj->cpuset, HWLOC_CPUBIND_STRICT);
  } else {
    Warning("hwloc returned an unexpected number of objects -- CPU affinity disabled");
  }
#endif // TS_USE_HWLOC
  return 0;
}

Event *
EventProcessor::schedule_spawn(Continuation *c, EventType ev_type, int event, void *cookie)
{
  Event *e = eventAllocator.alloc();
  ink_assert(ev_type < MAX_EVENT_TYPES);

  e->callback_event = event;
  e->cookie = cookie;
  e->init(c, 0, 0);
  thread_group[ev_type]._spawnQueue.enqueue(e);

  return e;
}

Event *
EventProcessor::schedule_spawn(void (*f)(EThread*), EventType ev_type)
{
  Event *e = eventAllocator.alloc();
  ink_assert(ev_type < MAX_EVENT_TYPES);

  e->callback_event = EVENT_IMMEDIATE;
  e->cookie = reinterpret_cast<void*>(f);
  e->init(&Thread_Init_Func, 0, 0);
  thread_group[ev_type]._spawnQueue.enqueue(e);

  return e;
}

EventType
EventProcessor::registerEventType(char const *name)
{
  ThreadGroupDescriptor *tg = &(thread_group[n_thread_groups++]);
  ink_release_assert(n_thread_groups <= MAX_EVENT_TYPES); // check for overflow

  tg->_name = ats_strdup(name);
  return n_thread_groups - 1;
}

EventType
EventProcessor::spawn_event_threads(int n_threads, char const *name, size_t stacksize)
{
  int ev_type = this->registerEventType(name);
  this->spawn_event_threads(ev_type, n_threads, stacksize);
  return ev_type;
}

EventType
EventProcessor::spawn_event_threads(EventType ev_type, int n_threads, size_t stacksize)
{
  char thr_name[MAX_THREAD_NAME_LENGTH];
  int i;
  ThreadGroupDescriptor *tg = &(thread_group[ev_type]);

  ink_release_assert(n_threads > 0);
  ink_release_assert((n_ethreads + n_threads) <= MAX_EVENT_THREADS);
  ink_release_assert(ev_type < MAX_EVENT_TYPES);

  for (i = 0; i < n_threads; ++i) {
    EThread *t = new EThread(REGULAR, n_ethreads + i);
    all_ethreads[n_ethreads + i] = t;
    tg->_thread[i] = t;
    t->set_event_type(ev_type);
    t->schedule_spawn(&thread_initializer);
  }
  tg->_count = n_threads;

  for (i = 0; i < n_threads; i++) {
    snprintf(thr_name, MAX_THREAD_NAME_LENGTH, "[%s %d]", tg->_name.get(), i);
    tg->_thread[i]->start(thr_name, stacksize);
  }

  n_ethreads += n_threads;
  Debug("iocore_thread", "Created thread group '%s' id %d with %d threads", tg->_name.get(), ev_type, n_threads);

  return ev_type; // useless but not sure what would be better.
}

void
EventProcessor::initThreadState(EThread *t)
{
  for (int i = 0; i < MAX_EVENT_TYPES; ++i) {
    if (t->is_event_type(i)) {
      for (Event *ev = thread_group[i]._spawnQueue.head; NULL != ev; ev = ev->link.next)
        ev->continuation->handleEvent(ev->callback_event, ev);
    }
  }
}

int
EventProcessor::start(int n_event_threads, size_t stacksize)
{
  // do some sanity checking.
  static int started = 0;
  ink_release_assert(!started);
  ink_release_assert(n_event_threads > 0 && n_event_threads <= MAX_EVENT_THREADS);
  started = 1;

  Thread_Affinity_Initializer.init();
  this->schedule_spawn(&Thread_Affinity_Initializer, ET_CALL);
  this->spawn_event_threads(ET_CALL, n_event_threads, stacksize);

  Debug("iocore_thread", "Cereated event thread group id %d with %d threads", ET_CALL, n_event_threads);
  return 0;
}

void
EventProcessor::shutdown()
{
}

Event *
EventProcessor::spawn_thread(Continuation *cont, const char *thr_name, size_t stacksize)
{
  ink_release_assert(n_dthreads < MAX_EVENT_THREADS);
  Event *e = eventAllocator.alloc();

  e->init(cont, 0, 0);
  all_dthreads[n_dthreads] = new EThread(DEDICATED, e);
  e->ethread = all_dthreads[n_dthreads];
  e->mutex = e->continuation->mutex = all_dthreads[n_dthreads]->mutex;
  n_dthreads++;
  e->ethread->start(thr_name, stacksize);

  return e;
}
