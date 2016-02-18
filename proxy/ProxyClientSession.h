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

#include "libts.h"
#include "P_Net.h"
#include "InkAPIInternal.h"

// Emit a debug message conditional on whether this particular client session
// has debugging enabled. This should only be called from within a client session
// member function.
#define DebugSsn(ssn, tag, ...) DebugSpecific((ssn)->debug(), tag, __VA_ARGS__)

class ProxyClientSession : public VConnection
{
public:
  ProxyClientSession();

  virtual void destroy();
  virtual void start() = 0;

  virtual void new_connection(NetVConnection *new_vc, MIOBuffer *iobuf, IOBufferReader *reader, bool backdoor) = 0;

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

  /** Secondary event handling while hooks are active.

      This is called for events that are not handled by this class in @c state_api_callout.  To
      handle such events override the @c state_api_callout method and chain.

      This is needed because frequently the data needed to deal with the event is in the subclass
      and not this one.

      @return @c true if the event was handled / expected, @c false otherwise.
  */
  virtual bool handle_api_event(int event, void* data) { return false; }

  static int64_t next_connection_id();

protected:
  // XXX Consider using a bitwise flags variable for the following flags, so that we can make the best
  // use of internal alignment padding.

  HttpHookState hook_state;
  
  // Session specific debug flag.
  bool debug_on;
  bool hooks_on;

protected:
  /** Handle events while hooks are active.

      Subclasses should override this only if the current handling of an event is unacceptable
      either because it is incorrect or insufficient. In the latter case the subclass should chain
      this method.

      For events that are not handled use @c handle_api_event

      @return 0
  */
  virtual int state_api_callout(int event, void *edata);

private:
  APIHook const* cur_hook;
  HttpAPIHooks api_hooks;
  void *user_args[HTTP_SSN_TXN_MAX_USER_ARG];

  ProxyClientSession(ProxyClientSession &);                  // noncopyable
  ProxyClientSession &operator=(const ProxyClientSession &); // noncopyable

  void handle_api_return(int event);

  friend void TSHttpSsnDebugSet(TSHttpSsn, int);
};

#endif // __PROXY_CLIENT_SESSION_H__
