.. Licensed to the Apache Software Foundation (ASF) under one
   or more contributor license agreements.  See the NOTICE file
   distributed with this work for additional information
   regarding copyright ownership.  The ASF licenses this file
   to you under the Apache License, Version 2.0 (the
   "License"); you may not use this file except in compliance
   with the License.  You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing,
   software distributed under the License is distributed on an
   "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
   KIND, either express or implied.  See the License for the
   specific language governing permissions and limitations
   under the License.

.. include:: ../../../common.defs

.. _developer-plugins-priority:

Plugin Priority
***************

Plugins have a priority system to provide a two key features needed in more
complex deployments. If you do not have a need for these you can ignore
priorities.

*  Controllling the order of plugin invocations.
*  Transaction level control of which plugins run.

Plugins are identified by the name passed in to :c:func:`TSPluginInit`. Names must be unique across
all plugins. In addition to a name each plugin is assigned two values of :term:`priority`, a
maximum priority and a effective priority. This is controlled by :file:`plugin.config`. The effective
priority is the priority used for setting callbacks unless a different value is explicitly specified. The maximum priority
is a limit on operations that take a priority value - using a value higher than the maximum will
result in an error.

A maximum priority can be configured in :file:`records.config` for plugins which do not have a
maximum priority specified in :file:`plugin.config`. This makes the case where priorities are not
useful easy - all plugins get the same maximum and effective priorities and no ordering or
control is imposed on the plugins.

Basics
======

*  Plugins are identified by name, as set in :c:func:`TSPluginInit`.
*  Plugins have a maximum and effective priority which can be set individually in :file:`plugin.config` or :file:`records.config` for all plugins.
*  The lifecycle hook `TS_LIFECYCLE_PLUGINS_LOADED_HOOK` is invoked after all plugins are loaded.
*  Plugins can examine the priorities of other plugins.
*  Each callback in each hook has a priority. This is the plugin effective priority unless explicitly specified. A callback priority can never be larger than the maximum priority of the plugin.
*  When a hook is invokved the callbacks are called in priority order, largest to smallest.
*  A threshold can be set for hook callbacks in which case only callbacks with a priority *larger* than the threshold will be called. A plugin cannot set a threshold larger than its maximum priority.
*  Callbacks can be removed from a hook via the API.
*  All callbacks from a specific plugin can be disabled if the disabling plugin has a higher priority than the disabled plugin.

As a result of this, a higher priority plugin will (in general) run before lower priority ones and
can disable the lower priority plugins from running at all. A plugin can disable itself, but it
cannot (in general) disable a higher priority plugin. The exception is if a plugin adds a callback
to a hook at a lower priority. Creating a callback with a lower priority can be used to have the
callback invoked after another plugin.

The effective priority is intended to provide a space between that and the maximum priority of a
plugin to allow another plugin to schedule a callback earlier. Otherwise a set of plugins at the same
priority could not do this cooperatively. That is important in situations where priority is used
primiarly for ordering callback invocations rather than controllling which callbacks are invoked.

Plugins have no control over their maximum priority, that is set by the administrator in :file:`plugin.config`. The effective priority can be changed but can never be larger than the maximum priority.

The priority threshold can be set at multiple levels. There is a global value that can be changed
and is the effective threshold for all hooks unless overrridden by more specific thresholds at the session and transaction level.

A particular hook can have a threshold which determines the callbacks which are invoked from that
hook. Such a threshold is set at the session or transaction level[#fn1]_. A threshold for a session
applies unless overridden for a specific transaction[#fn1]_. Thresholds set for a transaction are
the most specific and override any other value. Note a plugin cannot set any threshold larger than
that plugin's maximum priority. Because a callback needs a priority larger than the threshold a
plugin can disable itself.

Care must be taken because the effective priority is in general lower than the maximum priority. This
has implications. The first is a callback for a higher priority plugin can be disabled by a lower
priority one if the maximum priority of the latter is still higher than the effective priority of the
former. The desirabilty of this depends on local cicrumstances so the administrator must adjust
values to suit his particular situation. To avoid this behavior the administrator can place plugins
in priority tiers that don't overlap or the plugins can adjust the priority for a given callback.

.. note:: An example of a beneficial use of this is having plugins set the priority on their cleanup
   hook to be higher than their effective. This would enable a master plugin to disable most
   callbacks without disabling the cleanup callback.

Use Cases
=========

The first use case is the most common one, where plugin A wants to run before or after plugin B. In
this case A can run in the `TS_LIFECYCLE_PLUGINS_LOADED_HOOK` at which point it can find the
priority of B and set its effective priority to be one less than that. Or, if there are only specific
hooks in which this is required it can add those looks with the lower priority. To run before A can
set its effective priority to be one more than B. This works best when there is a key plugin on
which other plugins coordinate, or there is only one hook that requires ordering.

Alternatively the admistrator can determine a correct ordering and set the priorities for the plugins
appropriately in :file:`plugins.config`. If multiple plugins need coordination this is likely to be
the easiest and most robust solution.

The other major use case is a master plugin that controls the operation of the other installed
plugins. This can be set to run at a higher priority than other plugins which provides it with the
ability to run before or after (or both) relative to other plugins. It can also, using the threshold
mechanism, disable plugins from running in a specific transaction. This is best done by allowing or
inhibiting all callbacks on a transaction rather than selected hooks. This can also be used in
multiple tiers of ordinary, important, and master plugins where the threshold can be adjusted 

Issues
======

Successful use of this mechanism requires a certain amount of cooperation among plugins. In
particular it is dangerous to not have a hook on which all plugins are allowed to run in order to
clean up any resources (e.g. `TS_HTTP_TXN_CLOSE_HOOK`). Mechanisms to avoid this were considered but
if plugin A can prevent a callbackfrom plugin B from running then this problem will exist.

The before and after mechanism requires cooperation as well since to use it plugins must be allowed
to set callbacks at different priorities but that in itself can break the mechanism.

In general because of the very dynamic nature of setting callbacks on hooks (it is quite possible to
add additional callbacks to a hook that is currently executing) it is not possible to solve the
general case so it was deemed better to not pretend to do so. Ultimately the administrator
installing Traffic Server will need to provide the coordination necessary.

API
===

.. c:function:: int TSPluginCountGet()
   Return the number of plugins.
   
.. c:function:: TSReturnCode TSPluginGetInfoByIndex(int idx, TSPluginInfo* info)
   Get information for the plugin at index :arg:`idx`. An error is returned if there is no plugin with that index.
   
.. c:function:: TSReturnCode TSPluginInfoGetByName(char const* name, TSPluginInfo* info)
   Get information for the plugin :arg:`name`. An error is returned if there is no plugin with that :arg:`name`.
   
.. c:function:: int TSPluginMaxPriorityGet(char const* name)
   Return the maximum priority for a plugin. If :arg:`name` is :const:`NULL` the value for the current plugin is returned.
   A negative value indicates the plugin :arg:`name` was not found.
   
.. c:function:: int TSPlugineffectivePriorityGet(char const* name)
   Return the effective priority for a plugin. If :arg:`name` is :const:`NULL` the value for the current plugin is returned.
   A negative value indicates the plugin :arg:`name` was not found.
   
.. c:function:: TSReturnCode TSPluginDefaulPrioritySet(int pri)
   Set the effective priority for this plugin to :arg:`pri`. It is an error to set :arg:`pri` to be larger than the maximum priority of the plugin.
   
.. c:function:: int TSHttpHookThresholdGet(TSHttpHookId hook)
   Return the current global threshold for hook callbacks.
   
.. c:function:: TSReturnCode TSHttpHookThresholdSet(int pri)
   Set the current global threshold for hook callbacks to :arg:`pri`. It is an error to use a value that is larger than the calling plugin's maximum priority.
   
.. c:function:: int TSHttpSsnThresholdGet(TSHttpSsn ssnp)
   Return the current hook callback threshold for the session :arg:`ssnp`.
   
.. c:function:: TSReturnCode TSHttpSsnThresholdSet(TSHttpSsn ssnp, TSHttpHookId hook, int pri)
   Set the current session threshold for hook callbacks to :arg:`pri`. It is an error to use a value that is larger than the calling plugin's maximum priority.
   
.. c:function:: int TSHttpSsnHookThresholdGet(TSHttpSsn ssnp, TSHttpHookId hook)
   Return the current callback threshold for :arg:`hook` for the session :arg:`ssnp`.
   
.. c:function:: TSReturnCode TSHttpSsnHookThresholdSet(TSHttpSsn ssnp, TSHttpHookId hook, int pri)
   Set the threshold for :arg:`hook` to :arg:`pri` in session :arg:`ssnp`. It is an error to use a value that is larger than the calling plugin's maximum priority.
   
.. c:function:: int TSHttpTxnThresholdGet(TSHttpTxn txnp)
   Return the current callback threshold for the transaction :arg:`txnp`.
   
.. c:function:: int TSHttpTxnThresholdSet(TSHttpTxn txnp)
   Set the current transaction threshold for callbacks in transaction :arg:`txnp` to :arg:`pri`. It is an error to use a value that is larger than the calling plugin's maximum priority.
   
.. c:function:: int TSHttpTxnHookThresholdGet(TSHttpTxn txnp, TSHttpHookId hook)
   Return the current hook callback threshold for the :arg:`hook` in transaction :arg:`txnp`.
   
.. c:function:: int TSHttpTxnHookThresholdSet(TSHttpTxn txnp, TSHttpHookId hook)
   Set the current transaction threshold for the hook :arg:`hook` to :arg:`pri` for transaction :arg:`txnp`. It is an error to use a value that is larger than the calling plugin's maximum priority.
   
.. c:function:: int TSHttpTxnCallbackCountGet(TSHttpTxn txnp, TSHttpHookId hook)
   Get the number of callbacks set for :arg:`hook` in transaction :arg:`txnp`.
   
.. c:function:: char const* TSHttpTxnCallbackNameGetByIndex(TSHttpTxn txnp, TSHttpHookId hook, int idx)
   Get the name of the plugin for the :arg:`idx` th callback in :arg:`hook` in the transaction :arg:`txnp`. A return value of
   :const:`NULL` indicates :arg:`idx` was out of bounds.
   
.. c:function:: char const* TSHttpTxnCallbackPriorityGetByIndex(TSHttpTxn txnp, TSHttpHookId hook, int idx)
   Get the priority of the :arg:`idx` th callback in :arg:`hook` in the transaction :arg:`txnp`. A
   negative return value indicates :arg:`idx` was out of bounds.

.. c:function:: TSReturnCode TSHttpTxnHookCallbackRemoveByIndex(TSHttpTxn txnp, TSHttpHookId hook, int idx)
   Remove the :arg:`idx` th callback in :arg:`hook` in the transaction :arg:`txnp`. An error return indicates that either :arg:`idx` was out of bounds or the callback had a priority larger than the maximum priority of the plugin that called this function.

.. rubric:: Footnotes

.. [#fn1] Useful for control of plugins based on the IP address of a user agent. The threshold can be set once and then all transactions for that session will use that threshold for hooks.