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

ProxyClientSession::ProxyClientSession()
  : VConnection(nullptr),
    acl_record(nullptr),
    host_res_style(HOST_RES_IPV4),
    debug_on(false),
    hooks_on(true),
    in_destroy(false),
    con_id(0),
    m_active(false),
    f_proxy_port(nullptr)
{
  ink_zero(this->user_args);
}

void
ProxyClientSession::set_session_active()
{
  if (!m_active) {
    m_active = true;
    HTTP_INCREMENT_DYN_STAT(http_current_active_client_connections_stat);
  }
}

void
ProxyClientSession::clear_session_active()
{
  if (m_active) {
    m_active = false;
    HTTP_DECREMENT_DYN_STAT(http_current_active_client_connections_stat);
  }
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
ProxyClientSession::free()
{
  if (schedule_event) {
    schedule_event->cancel();
    schedule_event = nullptr;
  }
  this->api_hooks.clear();
  this->mutex.clear();
}

int
ProxyClientSession::state_api_callout(int event, void *data)
{
  Event *e = static_cast<Event *>(data);
  if (e == schedule_event) {
    schedule_event = nullptr;
  }

  switch (event) {
  case EVENT_NONE:
  case EVENT_INTERVAL:
  case TS_EVENT_HTTP_CONTINUE:
    if (nullptr == cur_hook) {
      /// Get the next hook to invoke from HttpHookState
      cur_hook = hook_state.getNext();
    }
    if (nullptr != cur_hook) {
      APIHook const *hook = cur_hook;

      MUTEX_TRY_LOCK(lock, hook->m_cont->mutex, mutex->thread_holding);

      // Have a mutex but didn't get the lock, reschedule
      if (!lock.is_locked()) {
        SET_HANDLER(&ProxyClientSession::state_api_callout);
        mutex->thread_holding->schedule_in(this, HRTIME_MSECONDS(1));
        return 0;
      }

      cur_hook = nullptr; // mark current callback at dispatched.
      hook->invoke(eventmap[hook_state.id()], this);

      return 0;
    }

    handle_api_return(event);
    break;

  case TS_EVENT_HTTP_ERROR:
    this->handle_api_return(event);
    break;

  // coverity[unterminated_default]
  default:
    ink_release_assert(false);
  }

  return 0;
}

void
ProxyClientSession::do_api_callout(TSHttpHookID id)
{
  ink_assert(id == TS_HTTP_SSN_START_HOOK || id == TS_HTTP_SSN_CLOSE_HOOK);
  hook_state.init(id, http_global_hooks, &api_hooks);
  /// Verify if there is any hook to invoke
  cur_hook = hook_state.getNext();
  if (hooks_on && nullptr != cur_hook) {
    SET_HANDLER(&ProxyClientSession::state_api_callout);
    this->state_api_callout(EVENT_NONE, nullptr);
  } else {
    this->handle_api_return(TS_EVENT_HTTP_CONTINUE);
  }
}

void
ProxyClientSession::handle_api_return(int event)
{
  TSHttpHookID hookid = hook_state.id();

  SET_HANDLER(&ProxyClientSession::state_api_callout);

  cur_hook = nullptr;

  switch (hookid) {
  case TS_HTTP_SSN_START_HOOK:
    if (event == TS_EVENT_HTTP_ERROR) {
      this->do_io_close();
    } else {
      this->start();
    }
    break;
  case TS_HTTP_SSN_CLOSE_HOOK: {
    free(); // You can now clean things up
    break;
  }
  default:
    Fatal("received invalid session hook %s (%d)", HttpDebugNames::get_api_hook_name(hookid), hookid);
    break;
  }
}
