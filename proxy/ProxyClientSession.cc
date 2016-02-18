/** @file

  ProxyClientSession - Base class for protocol client sessions.

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

#include "HttpConfig.h"
#include "HttpDebugNames.h"
#include "ProxyClientSession.h"

static int64_t next_cs_id = 0;

ProxyClientSession::ProxyClientSession() : VConnection(NULL), debug_on(false), hooks_on(true)
{
  ink_zero(this->user_args);
}

int64_t
ProxyClientSession::next_connection_id()
{
  return ink_atomic_increment(&next_cs_id, 1);
}

static const TSEvent eventmap[TS_HTTP_LAST_HOOK + 1] = {
  TS_EVENT_HTTP_READ_REQUEST_HDR,      // TS_HTTP_READ_REQUEST_HDR_HOOK
  TS_EVENT_HTTP_OS_DNS,                // TS_HTTP_OS_DNS_HOOK
  TS_EVENT_HTTP_SEND_REQUEST_HDR,      // TS_HTTP_SEND_REQUEST_HDR_HOOK
  TS_EVENT_HTTP_READ_CACHE_HDR,        // TS_HTTP_READ_CACHE_HDR_HOOK
  TS_EVENT_HTTP_READ_RESPONSE_HDR,     // TS_HTTP_READ_RESPONSE_HDR_HOOK
  TS_EVENT_HTTP_SEND_RESPONSE_HDR,     // TS_HTTP_SEND_RESPONSE_HDR_HOOK
  TS_EVENT_HTTP_REQUEST_TRANSFORM,     // TS_HTTP_REQUEST_TRANSFORM_HOOK
  TS_EVENT_HTTP_RESPONSE_TRANSFORM,    // TS_HTTP_RESPONSE_TRANSFORM_HOOK
  TS_EVENT_HTTP_SELECT_ALT,            // TS_HTTP_SELECT_ALT_HOOK
  TS_EVENT_HTTP_TXN_START,             // TS_HTTP_TXN_START_HOOK
  TS_EVENT_HTTP_TXN_CLOSE,             // TS_HTTP_TXN_CLOSE_HOOK
  TS_EVENT_HTTP_SSN_START,             // TS_HTTP_SSN_START_HOOK
  TS_EVENT_HTTP_SSN_CLOSE,             // TS_HTTP_SSN_CLOSE_HOOK
  TS_EVENT_HTTP_CACHE_LOOKUP_COMPLETE, // TS_HTTP_CACHE_LOOKUP_COMPLETE_HOOK
  TS_EVENT_HTTP_PRE_REMAP,             // TS_HTTP_PRE_REMAP_HOOK
  TS_EVENT_HTTP_POST_REMAP,            // TS_HTTP_POST_REMAP_HOOK
  TS_EVENT_NONE,                       // TS_HTTP_RESPONSE_CLIENT_HOOK
  TS_EVENT_NONE,                       // TS_HTTP_LAST_HOOK
};

void
ProxyClientSession::destroy()
{
  this->api_hooks.clear();
  this->mutex.clear();
}

void
ProxyClientSession::ssn_priority_threshold_set(int priority)
{
  api_hooks.set_threshold(priority);
  hook_state.setThreshold(priority, HttpHookState::SESSION);
}

void
ProxyClientSession::ssn_hook_priority_threshold_set(TSHttpHookID id, int priority)
{
  api_hooks.set_threshold(id, priority);
  hook_state.setThreshold(id, priority, HttpHookState::SESSION);
}

void
ProxyClientSession::ssn_plugin_enable(PluginInfo const* pi, bool enable_p)
{
  hook_state.enable(pi, enable_p);
}

int
ProxyClientSession::state_api_callout(int event, void *data)
{
  switch (event) {
  case EVENT_NONE:
  case EVENT_INTERVAL:
  case TS_EVENT_HTTP_CONTINUE:
    if (NULL == cur_hook) cur_hook = hook_state.getNext();
    if (NULL != cur_hook) {
      bool plugin_lock = false;
      APIHook const * hook = cur_hook;
      
      Ptr<ProxyMutex> plugin_mutex;

      if (hook->m_cont->mutex) {
        plugin_mutex = hook->m_cont->mutex;
        plugin_lock = MUTEX_TAKE_TRY_LOCK(hook->m_cont->mutex, mutex->thread_holding);
        if (!plugin_lock) {
          SET_HANDLER(&ProxyClientSession::state_api_callout);
          mutex->thread_holding->schedule_in(this, HRTIME_MSECONDS(10));
          return 0;
        }
      }

      cur_hook = NULL; // mark current callback as dispatched.
      hook->invoke(eventmap[hook_state.id()], this);

      if (plugin_lock) {
        Mutex_unlock(plugin_mutex, this_ethread());
      }

      return 0;
    }

    handle_api_return(event);
    break;

  case TS_EVENT_HTTP_ERROR:
    this->handle_api_return(event);
    break;

  default:
    bool handled_p = this->handle_api_event(event, data);
    ink_assert(handled_p);
    break;
  }

  return 0;
}

void
ProxyClientSession::do_api_callout(TSHttpHookID id)
{
  ink_assert(id == TS_HTTP_SSN_START_HOOK || id == TS_HTTP_SSN_CLOSE_HOOK);
  if (hooks_on) {
    hook_state.init(id, http_global_hooks, &api_hooks);
    cur_hook = hook_state.getNext();
    if (NULL != cur_hook) {
      SET_HANDLER(&ProxyClientSession::state_api_callout);
      this->state_api_callout(EVENT_NONE, NULL);
    } else {
      this->handle_api_return(TS_EVENT_HTTP_CONTINUE);
    }
  }
}

void
ProxyClientSession::handle_api_return(int event)
{
  TSHttpHookID hookid = hook_state.id();

  SET_HANDLER(&ProxyClientSession::state_api_callout);

  cur_hook = NULL;

  switch (hookid) {
  case TS_HTTP_SSN_START_HOOK:
    if (event == TS_EVENT_HTTP_ERROR) {
      this->do_io_close();
    } else {
      this->start();
    }
    break;
  case TS_HTTP_SSN_CLOSE_HOOK: {
    NetVConnection *vc = this->get_netvc();
    if (vc) {
      vc->do_io_close();
      this->release_netvc();
    }
    this->destroy();
    break;
  }
  default:
    Fatal("received invalid session hook %s (%d)", HttpDebugNames::get_api_hook_name(hookid), hookid);
    break;
  }
}
