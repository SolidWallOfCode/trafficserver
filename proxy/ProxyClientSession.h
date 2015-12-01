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

#ifndef __PROXY_CLIENT_SESSION_H__
#define __PROXY_CLIENT_SESSION_H__

#include "ts/ink_platform.h"
#include "ts/ink_resolver.h"
#include "P_Net.h"
#include "InkAPIInternal.h"
#include "http/HttpServerSession.h"

// Emit a debug message conditional on whether this particular client session
// has debugging enabled. This should only be called from within a client session
// member function.
#define DebugSsn(ssn, tag, ...) DebugSpecific((ssn)->debug(), tag, __VA_ARGS__)

class ProxyClientTransaction;
struct AclRecord;

class ProxyClientSession : public VConnection
{
public:
  ProxyClientSession();

  virtual void destroy() = 0;
  virtual void free();
  virtual void start() = 0;

  virtual void new_connection(NetVConnection *new_vc, MIOBuffer *iobuf, IOBufferReader *reader, bool backdoor) = 0;

  virtual NetVConnection *get_netvc() const = 0;
  virtual void release_netvc()              = 0;

  virtual int get_transact_count() const = 0;

  virtual const char *get_protocol_string() const = 0;

  virtual void
  hook_add(TSHttpHookID id, INKContInternal *cont, int priority)
  {
    this->api_hooks.add(id, cont, priority);
  }

  virtual NetVConnection *get_netvc() const = 0;
  virtual void release_netvc() = 0;

  APIHook const*
  hook_get(TSHttpHookID id) const
  {
    return this->api_hooks.get(id);
  }

  void *
  get_user_arg(unsigned ix) const
  {
    ink_assert(ix < countof(user_args));
    return this->user_args[ix];
  }

  void
  set_user_arg(unsigned ix, void *arg)
  {
    ink_assert(ix < countof(user_args));
    user_args[ix] = arg;
  }

  void
  set_debug(bool flag)
  {
    debug_on = flag;
  }

  // Return whether debugging is enabled for this session.
  bool
  debug() const
  {
    return this->debug_on;
  }

  bool
  hooks_enabled() const
  {
    return this->hooks_on;
  }

  bool
  has_hooks() const
  {
    return this->api_hooks.has_hooks() || http_global_hooks->has_hooks();
  }

  HttpAPIHooks const* feature_hooks() const { return &api_hooks; }
  void ssn_priority_threshold_set(int priority);
  void ssn_hook_priority_threshold_set(TSHttpHookID id, int priority);
  void ssn_plugin_enable(PluginInfo const* pi, bool enable_p);

  // Initiate an API hook invocation.
  void do_api_callout(TSHttpHookID id);

  // Override if your session protocol allows this.
  virtual bool
  is_transparent_passthrough_allowed() const
  {
    return false;
  }

  // Override if your session protocol cares.
  virtual void
  set_half_close_flag(bool flag)
  {
  }

  virtual bool
  get_half_close_flag() const
  {
    return false;
  }

  // Indicate we are done with a transaction.
  virtual void release(ProxyClientTransaction *trans) = 0;

  int64_t
  connection_id() const
  {
    return con_id;
  }

  virtual void
  attach_server_session(HttpServerSession *ssession, bool transaction_done = true)
  {
  }

  virtual HttpServerSession *
  get_server_session() const
  {
    return NULL;
  }

  TSHttpHookID
  get_hookid() const
  {
    return api_hookid;
  }

  virtual void
  set_active_timeout(ink_hrtime timeout_in)
  {
  }

  virtual void
  set_inactivity_timeout(ink_hrtime timeout_in)
  {
  }

  virtual void
  cancel_inactivity_timeout()
  {
  }

  bool
  is_client_closed() const
  {
    return get_netvc() == NULL;
  }

  virtual int
  populate_protocol(ts::StringView *result, int size) const
  {
    auto vc = this->get_netvc();
    return vc ? vc->populate_protocol(result, size) : 0;
  }

  virtual const char *
  protocol_contains(ts::StringView tag_prefix) const
  {
    auto vc = this->get_netvc();
    return vc ? vc->protocol_contains(tag_prefix) : nullptr;
  }

  void set_session_active();
  void clear_session_active();

  static int64_t next_connection_id();

  /// acl record - cache IpAllow::match() call
  const AclRecord *acl_record;

  /// DNS resolution preferences.
  HostResStyle host_res_style;

  ink_hrtime ssn_start_time;
  ink_hrtime ssn_last_txn_time;

  virtual sockaddr const *
  get_client_addr()
  {
    NetVConnection *netvc = get_netvc();
    return netvc ? netvc->get_remote_addr() : nullptr;
  }
  virtual sockaddr const *
  get_local_addr()
  {
    NetVConnection *netvc = get_netvc();
    return netvc ? netvc->get_local_addr() : nullptr;
  }

protected:
  // XXX Consider using a bitwise flags variable for the following flags, so
  // that we can make the best use of internal alignment padding.

  HttpHookState hook_state;

  // Session specific debug flag.
  bool debug_on;
  bool hooks_on;
  bool in_destroy;

  int64_t con_id;
  Event *schedule_event;

private:
  APIHook const* cur_hook;
  HttpAPIHooks api_hooks;
  void *user_args[HTTP_SSN_TXN_MAX_USER_ARG];

  // for DI. An active connection is one that a request has
  // been successfully parsed (PARSE_DONE) and it remains to
  // be active until the transaction goes through or the client
  // aborts.
  bool m_active;
};

#endif // __PROXY_CLIENT_SESSION_H__
