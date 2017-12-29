/** @file

   Stripe operations.

   Primary implementation file for class @c Vol.

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

#include "I_IOBuffer.h"
#include "P_Cache.h"
#include "P_CacheVol.h"

std::atomic<size_t> Vol::_last_id{0}; ///< Last ID allocated to a stripe instance.
thread_local std::vector<Vol::LockData> Vol::lock_queue;

Event*
Vol::LockData::enqueue(Vol* vol, EThread* t, CacheVC* cachevc)
{
  Event* e = EVENT_ALLOC(eventAllocator, t);
  e->init(cachevc);
  this->queue.enqueue(e);
  this->update_trigger(vol, t);
  return e;
}

void
Vol::LockData::update_trigger(Vol* vol, EThread* t)
{
  // Update dispatch trigger event. Should be scheduled iff there are waiting CacheVCs.
  if (this->queue.empty()) {
    this->trigger->cancel();
    this->trigger = nullptr;
  } else if (!this->trigger) {
    // just make sure it's in the next event loop, not this one.
    this->trigger = t->schedule_in_local(this, 1, CACHE_EVENT_STRIPE_LOCK_READY);
  }
}

void
Vol::LockData::dispatch(Vol* vol, EThread* t)
{
  LockQueue local;
  std::swap(local, this->queue); // grab all events from the main queue.
  // Dispatch them.
  while (Event* evt = local.pop()) {
    MUTEX_TRY_LOCK(lock, evt->mutex, t);
    if (lock.is_locked()) {
      if (!evt->cancelled) {
        evt->continuation->handleEvent(CACHE_EVENT_STRIPE_LOCK_READY, evt);
      }
      EVENT_FREE(evt, eventAllocator, t);
    } else {
      this->queue.enqueue(evt); // can't dispatch, put it back for later.
    }
  }
  this->update_trigger(vol, t);
}

CacheOpState
Vol::do_open_write(CacheVC* cachevc)
{
  EThread *t = this_ethread();
  if (lock_queue.size() <= _last_id) lock_queue.resize(_last_id+1);
  LockData& ld = lock_queue[id];

  CACHE_TRY_LOCK(lock, this->mutex, t);
  if (lock.is_locked()) {
    ld.dispatch(this, t);

    if (!cachevc->f.remove && !cachevc->f.update && agg_todo_size > cache_config_agg_write_backlog) {
      CacheIncrementDynStat(this, t, cache_write_backlog_failure_stat);
      return { CacheOpResult::ERROR, ECACHE_WRITE_FAIL };
    }
    ink_assert(nullptr == cachevc->od);
    cachevc->od = this->open_dir.open_entry(this, cachevc->first_key, true);
#ifdef CACHE_STAT_PAGES
    ink_assert(cont->mutex->thread_holding == this_ethread());
    ink_assert(!cont->stat_link.next && !cont->stat_link.prev);
    stat_cache_vcs.enqueue(cont, cont->stat_link);
#endif
    return { CacheOpResult::DONE };
  } else {
    Event* evt = ld.enqueue(this, t, cachevc);
    return { CacheOpResult::WAIT, evt };
  }
}

CacheOpState
Vol::do_with_lock(CacheVC* cachevc)
{
  EThread *t = this_ethread();
  if (lock_queue.size() <= _last_id) lock_queue.resize(_last_id+1);
  LockData& ld = lock_queue[id];

  CACHE_TRY_LOCK(lock, this->mutex, t);
  if (lock.is_locked()) {
    ld.dispatch(this, t);
    cachevc->handleEvent(CACHE_EVENT_STRIPE_LOCK_READY, nullptr);
    return { CacheOpResult::DONE };
  } else {
    *cacheVC = new_CacheVC(xx);
    Event* evt = ld.enqueue(this, t, *cachevc);
    return { CacheOpResult::WAIT, evt };
  }
}
