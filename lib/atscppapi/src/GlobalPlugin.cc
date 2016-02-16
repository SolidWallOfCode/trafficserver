
/**
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

/**
 * @file GlobalPlugin.cc
 */
#include "atscppapi/GlobalPlugin.h"
#include <ts/ts.h>
#include <cstddef>
#include <cassert>
#include <map>
#include "atscppapi/noncopyable.h"
#include "utils_internal.h"
#include "logging_internal.h"

using namespace atscppapi;

/**
 * @private
 */
struct atscppapi::GlobalPluginState : noncopyable {
  TSCont cont_;
  GlobalPlugin *global_plugin_;
  bool ignore_internal_transactions_;

  std::multimap<int, GlobalPlugin::LifecycleCallback> lifecycle_cbs_;

  GlobalPluginState(GlobalPlugin *global_plugin, bool ignore_internal_transactions)
    : global_plugin_(global_plugin), ignore_internal_transactions_(ignore_internal_transactions)
  {
  }
};

namespace
{
static int
handleGlobalPluginEvents(TSCont cont, TSEvent event, void *edata)
{
  TSHttpTxn txn = static_cast<TSHttpTxn>(edata);
  GlobalPluginState *state = static_cast<GlobalPluginState *>(TSContDataGet(cont));
  if (state->ignore_internal_transactions_ && (TSHttpTxnIsInternal(txn) == TS_SUCCESS)) {
    LOG_DEBUG("Ignoring event %d on internal transaction %p for global plugin %p", event, txn, state->global_plugin_);
    TSHttpTxnReenable(txn, TS_EVENT_HTTP_CONTINUE);
  } else if (event == TS_EVENT_LIFECYCLE_PLUGINS_LOADED) {
    auto range = state->lifecycle_cbs_.equal_range(TS_LIFECYCLE_PLUGINS_LOADED_HOOK);
    for ( auto spot = range.first ; spot != range.second ; ++spot) {
      (state->global_plugin_->*(spot->second))(edata);
    }
  } else {
    LOG_DEBUG("Invoking global plugin %p for event %d on transaction %p", state->global_plugin_, event, txn);
    utils::internal::invokePluginForEvent(state->global_plugin_, txn, event);
  }
  return 0;
}

} /* anonymous namespace */

GlobalPlugin::GlobalPlugin(bool ignore_internal_transactions)
{
  utils::internal::initTransactionManagement();
  state_ = new GlobalPluginState(this, ignore_internal_transactions);
  TSMutex mutex = NULL;
  state_->cont_ = TSContCreate(handleGlobalPluginEvents, mutex);
  TSContDataSet(state_->cont_, static_cast<void *>(state_));
}

GlobalPlugin::~GlobalPlugin()
{
  TSContDestroy(state_->cont_);
  delete state_;
}

void
GlobalPlugin::registerHook(Plugin::HookType hook_type)
{
  TSHttpHookID hook_id = utils::internal::convertInternalHookToTsHook(hook_type);
  TSHttpHookAdd(hook_id, state_->cont_);
  LOG_DEBUG("Registered global plugin %p for hook %s", this, HOOK_TYPE_STRINGS[hook_type].c_str());
}

void
GlobalPlugin::registerHook(LifecycleHookType id, LifecycleCallback cb)
{
  TSLifecycleHookID ts_id = utils::internal::convertInternalHookToTsHook(id);
  state_->lifecycle_cbs_.insert(std::make_pair(ts_id, cb));
  TSLifecycleHookAdd(ts_id, state_->cont_);
}
