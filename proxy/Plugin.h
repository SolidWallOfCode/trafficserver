/** @file

  Plugin init declarations

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

#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#include <ts/List.h>
#include <ts/ink_thread.h>

struct PluginInfo {
  PluginInfo();
  ~PluginInfo();

  bool _registered_p;

  /// Path to the implmentation (library, so, dll) file.
  ats_scoped_str _file_path;
  /// Name of the plugin.
  ats_scoped_str _name;
  /// Plugin vendor name.
  ats_scoped_str _vendor;
  /// email for vendor / author.
  ats_scoped_str _email;

  int _max_priority; ///< Maximum priority.
  int _default_priority; ///< Default priority.

  /// Library handle.
  void *dlh;

  LINK(PluginInfo, link);
};

/** Manage the set of plugins.
 */
class PluginManager
{
 public:
  PluginManager();

  /// Initialize all the plugins.
  bool init(bool continueOnError = false);
  /// Expand argument to plugin.
  char* expand(char*);
 protected:
  /// Load a single plugin.
  bool load(int arg, char * argv[], bool continueOnError);
};

/// Globally accessible singleton.
extern PluginManager pluginManager;

/** Control and access a per thread plugin context.

      This should be used to set the context when a plugin callback is invoked.
      Static methods can then be used to get the current plugin.
*/
class PluginContext
{
 public:
  /// Set the plugin context in a scoped fashion.
  /// This is re-entrant.
  PluginContext(PluginInfo* plugin)
    {
      _save = ink_thread_getspecific(THREAD_KEY);
      ink_thread_setspecific(THREAD_KEY, plugin);
    }
  ~PluginContext()
    {
      ink_thread_setspecific(THREAD_KEY, _save);
    }

  /// Get the current plugin in context.
  /// @return The plugin info or @c NULL if there is no plugin context.
  static PluginInfo* get()
  {
    return static_cast<PluginInfo*>(ink_thread_getspecific(THREAD_KEY));
  }
  
 private:
  void* _save; ///< Value to restore when context goes out of scope.
  
  /// The key for the per thread context. This is initialized by the @c PluginManager singleton.
  static ink_thread_key THREAD_KEY;

 friend class PluginManager;
};

/** Abstract interface class for plugin based continuations.

    The primary intended use of this is for logging so that continuations
    that generate logging messages can generate plugin local data in a
    generic way.

    The core will at appropriate times dynamically cast the continuation
    to this class and if successful access the plugin data via these
    methods.

    Plugins should mix this in to continuations for which it is useful.
    The default implementations return empty / invalid responses and should
    be overridden by the plugin.
 */
class PluginIdentity
{
public:
  /// Make sure destructor is virtual.
  virtual ~PluginIdentity() {}

  /** Get the plugin tag.
      The returned string must have a lifetime at least as long as the plugin.
      @return A string identifying the plugin or @c NULL.
  */
  virtual char const *
  getPluginTag() const
  {
    return NULL;
  }
  /** Get the plugin instance ID.
      A plugin can create multiple subsidiary instances. This is used as the
      identifier for those to distinguish the instances.
  */
  virtual int64_t
  getPluginId() const
  {
    return 0;
  }
};

#endif /* __PLUGIN_H__ */
