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

#if defined(MAGIC)
// Who the beep is defining this? That's a bad choice of name to hard code with a define.
#undef MAGIC
#endif

struct PluginInfo {
  enum { MAGIC = 0xabacab56 };
  PluginInfo();
  // Subclasses will get stored in lists as this base class so make sure their
  // destructors are accessible.
  virtual ~PluginInfo();

  /// Path to the implmentation (library, so, dll) file.
  ats_scoped_str _file_path;
  /// Name of the plugin.
  ats_scoped_str _name;

  uint64_t _magic; ///< Standard magic value for validity checks.

  int _max_priority; ///< Maximum priority.
  int _eff_priority; ///< Effective priority.

  /// Library handle.
  void *dlh;

  /// Status flags.
  union {
    unsigned int _all : 32;
    struct {
      unsigned int _registered : 1;
      unsigned int _disabled : 1;
    } _flag;
  } _flags;

  /// For the overall registration list.
  LINK(PluginInfo, link);
  /// Disabled list.
  LINK(PluginInfo, disabled_link);
};

struct GlobalPluginInfo : public PluginInfo {
  /// Plugin vendor name.
  ats_scoped_str _vendor;
  /// email for vendor / author.
  ats_scoped_str _email;
};

/** Manage the set of plugins.
 */
class PluginManager
{
public:
  /// Iterator for disabled plugins.
  struct disabled_iterator {
  private:
    typedef disabled_iterator self; ///< Self reference type.

  public:
    disabled_iterator(); ///< Default constructor.
    self &operator++(); ///< Move to next disabled plugin.
    bool operator==(self const &that) const;
    bool operator!=(self const &that) const;
    PluginInfo const &operator*() const;
    PluginInfo const *operator->() const;

  private:
    /// Internal constructor for use by container.
    disabled_iterator(PluginInfo const *pi);

    PluginInfo const *_pi; ///< Current element (with embedded next pointer).
    friend class PluginManager;
  };

  PluginManager();

  /// Initialize all the plugins.
  bool init(bool continueOnError = false);
  /// Expand argument to plugin.
  char *expand(char *);

  /// Get the current configured default (maximum) priority.
  int get_default_priority() const;
  /// Get the current configured default effective prirority.
  int get_default_effective_priority() const;
  /// Get the configured default different between maximum and effective priorities.
  int get_effective_priority_gap() const;

  /// Locate a plugin by @a name.
  PluginInfo const *find(char const *name);

  /// Enable/disable a plugin globally.
  /// If @a enable_p is @c true then plugin is marked enabled, otherwise it is marked disabled.
  /// This can be overridden per session or transaction.
  void enable(PluginInfo const *pi, bool enable_p);

  /// Used for plugin type continuations created and used by TS itself.
  static GlobalPluginInfo *Internal_Plugin_Info;
  /// Used primarily for remap plugins which are not required to register.
  static GlobalPluginInfo *Default_Plugin_Info;

  /// Initialize thread local storage needed for plugin management.
  void initForThread();

  /// Iterator for the first disabled plugin.
  disabled_iterator disabled_begin() const;
  /// Iterator for one past the last disabled plugin.
  disabled_iterator disabled_end() const;

protected:
  /// Load a single plugin.
  bool load(int arg, char *argv[], bool continueOnError);

  int32_t _default_priority;
  int32_t _effective_priority_gap;
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
  PluginContext(PluginInfo const *plugin)
  {
    _save = ink_thread_getspecific(THREAD_KEY);
    // Unfortunately thread local storage won't preserve const
    ink_thread_setspecific(THREAD_KEY, const_cast<PluginInfo *>(plugin));
  }
  ~PluginContext() { ink_thread_setspecific(THREAD_KEY, _save); }

  /// Get the current plugin in context.
  /// @return The plugin info or @c NULL if there is no plugin context.
  static PluginInfo const *
  get()
  {
    return static_cast<PluginInfo const *>(ink_thread_getspecific(THREAD_KEY));
  }

private:
  /// Set a default plugin context if none in play.
  /// @internal This is used to set the internal plugin info as the default so that it is used when
  /// hook calls are made from core code. This needs to be called on each thread once when the thread starts.
  static void
  setDefaultPluginInfo(PluginInfo const *p)
  {
    ink_thread_setspecific(THREAD_KEY, const_cast<PluginInfo *>(p));
  }

  void *_save; ///< Value to restore when context goes out of scope.

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

inline int
PluginManager::get_default_priority() const
{
  return _default_priority;
}

inline int
PluginManager::get_default_effective_priority() const
{
  return _default_priority - _effective_priority_gap;
}

inline int
PluginManager::get_effective_priority_gap() const
{
  return _effective_priority_gap;
}

inline PluginManager::disabled_iterator::disabled_iterator() : _pi(NULL)
{
}

inline PluginManager::disabled_iterator &PluginManager::disabled_iterator::operator++()
{
  if (NULL != _pi)
    _pi = _pi->disabled_link.next;
  return *this;
}

inline bool PluginManager::disabled_iterator::operator==(self const &that) const
{
  return _pi == that._pi;
}

inline bool PluginManager::disabled_iterator::operator!=(self const &that) const
{
  return _pi != that._pi;
}

inline PluginInfo const &PluginManager::disabled_iterator::operator*() const
{
  return *_pi;
}

inline PluginInfo const *PluginManager::disabled_iterator::operator->() const
{
  return _pi;
}

inline PluginManager::disabled_iterator::disabled_iterator(PluginInfo const *pi) : _pi(pi)
{
}

#endif /* __PLUGIN_H__ */
