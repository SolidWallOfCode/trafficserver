/** @file

  Internal SDK stuff

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

#ifndef __INK_API_INTERNAL_H__
#define __INK_API_INTERNAL_H__

#include "P_EventSystem.h"
#include "URL.h"
#include "P_Net.h"
#include "HTTP.h"
#include "ts/List.h"
#include "ProxyConfig.h"
#include "P_Cache.h"
#include "I_Tasks.h"
#include "Plugin.h"

class ProxyClientSession;
class HttpSM;

#include "api/ts/InkAPIPrivateIOCore.h"
#include "api/ts/experimental.h"

#include <typeinfo>

/* Some defines that might be candidates for configurable settings later.
 */
#define HTTP_SSN_TXN_MAX_USER_ARG 16 /* max number of user arguments for Transactions and Sessions */

typedef int8_t TSMgmtByte; // Not for external use

/* ****** Cache Structure ********* */

// For memory corruption detection
enum CacheInfoMagic {
  CACHE_INFO_MAGIC_ALIVE = 0xfeedbabe,
  CACHE_INFO_MAGIC_DEAD  = 0xdeadbeef,
};

struct CacheInfo {
  INK_MD5 cache_key;
  CacheFragType frag_type;
  char *hostname;
  int len;
  time_t pin_in_cache;
  CacheInfoMagic magic;

  CacheInfo()
  {
    frag_type    = CACHE_FRAG_TYPE_NONE;
    hostname     = NULL;
    len          = 0;
    pin_in_cache = 0;
    magic        = CACHE_INFO_MAGIC_ALIVE;
  }
};

class FileImpl
{
  enum {
    CLOSED = 0,
    READ   = 1,
    WRITE  = 2,
  };

public:
  FileImpl();
  ~FileImpl();

  int fopen(const char *filename, const char *mode);
  void fclose();
  int fread(void *buf, int length);
  int fwrite(const void *buf, int length);
  int fflush();
  char *fgets(char *buf, int length);

public:
  int m_fd;
  int m_mode;
  char *m_buf;
  int m_bufsize;
  int m_bufpos;
};

struct INKConfigImpl : public ConfigInfo {
  void *mdata;
  TSConfigDestroyFunc m_destroy_func;

  virtual ~INKConfigImpl() { m_destroy_func(mdata); }
};

struct HttpAltInfo {
  HTTPHdr m_client_req;
  HTTPHdr m_cached_req;
  HTTPHdr m_cached_resp;
  float m_qvalue;
};

enum APIHookScope {
  API_HOOK_SCOPE_NONE,
  API_HOOK_SCOPE_GLOBAL,
  API_HOOK_SCOPE_LOCAL,
};

/// Value used to mark no threshold set.
static int const API_HOOK_THRESHOLD_UNSET = -2;

/// A single API hook that can be invoked.
class APIHook
{
public:
  INKContInternal *m_cont;
  int m_priority; ///< Priority of this callback.

  int invoke(int event, void *edata) const;

  APIHook *next();
  APIHook const *next() const;
  APIHook *prev();
  APIHook const *prev() const;

  LINK(APIHook, m_link);
};

inline APIHook *
APIHook::next()
{
  return m_link.next;
}

inline APIHook const *
APIHook::next() const
{
  return m_link.next;
}

inline APIHook *
APIHook::prev()
{
  return m_link.prev;
}

inline APIHook const *
APIHook::prev() const
{
  return m_link.prev;
}

/// A collection of API hooks.
class APIHooks
{
public:
  APIHooks() : m_threshold(API_HOOK_THRESHOLD_UNSET) {}
  /// Add a hook at @a priority.
  void add(INKContInternal *cont, int priority);
  /// Get the first hook.
  APIHook *head();
  /// Get the first hook.
  APIHook const *head() const;
  /// Remove all hooks.
  void clear();
  /// Check if there are no hooks.
  bool is_empty() const;
  void invoke(int event, void *data);

  int m_threshold; ///< Priority threshold for invocation.

private:
  Que(APIHook, m_link) m_hooks;
};

inline APIHook *
APIHooks::head()
{
  return m_hooks.head;
}

inline APIHook const *
APIHooks::head() const
{
  return m_hooks.head;
}

inline bool
APIHooks::is_empty() const
{
  return NULL == m_hooks.head;
}

inline void
APIHooks::invoke(int event, void *data)
{
  for (APIHook *hook = m_hooks.head; NULL != hook; hook = hook->next())
    hook->invoke(event, data);
}

/** Container for API hooks for a specific feature.

    This is an array of hook lists, each identified by a numeric identifier (id). Each array element is a list of all
    hooks for that ID. Adding a hook means adding to the list in the corresponding array element. There is no provision
    for removing a hook.

    @note The minimum value for a hook ID is zero. Therefore the template parameter @a N_ID should be one more than the
    maximum hook ID so the valid ids are 0..(N-1) in the standard C array style.
 */
template <typename ID, ///< Type of hook ID
          ID N         ///< Number of hooks
          >
class FeatureAPIHooks
{
public:
  FeatureAPIHooks();  ///< Constructor (empty container).
  ~FeatureAPIHooks(); ///< Destructor.

  /// Remove all hooks.
  void clear();
  /// Add the callback @a cont to the hook @a id with @a priority.
  void add(ID id, INKContInternal *cont, int priority);
  /// Get the list of hooks for @a id.
  APIHook *get(ID id);
  APIHook const *get(ID id) const;
  /// Get the priority threshold.
  int threshold() const;
  /// Set the priority threshold.
  void set_threshold(int priority);
  /// Set the priority threshold for a specific hook.
  void set_threshold(TSHttpHookID id, int priority);
  /// @return @c true if @a id is a valid id, @c false otherwise.
  static bool is_valid(ID id);

  /// Invoke the callbacks for the hook @a id.
  void invoke(ID id, int event, void *data);

  /// Fast check for any hooks in this container.
  ///
  /// @return @c true if any list has at least one hook, @c false if
  /// all lists have no hooks.
  bool has_hooks() const;

  /// Check for existence of hooks of a specific @a id.
  /// @return @c true if any hooks of type @a id are present.
  bool has_hooks_for(ID id) const;

  /// Get a pointer to the set of hooks for a specific hook @ id
  APIHooks *operator[](ID id);
  APIHooks const *operator[](ID id) const;

private:
  bool m_hooks_p;  ///< Flag for (not) empty container.
  int m_threshold; ///< Invocation threshold.
  /// The array of hooks lists.
  APIHooks m_hooks[N];
};

template <typename ID, ID N> FeatureAPIHooks<ID, N>::FeatureAPIHooks() : m_hooks_p(false), m_threshold(API_HOOK_THRESHOLD_UNSET)
{
}

template <typename ID, ID N> FeatureAPIHooks<ID, N>::~FeatureAPIHooks()
{
  this->clear();
}

template <typename ID, ID N>
void
FeatureAPIHooks<ID, N>::clear()
{
  for (int i = 0; i < N; ++i) {
    m_hooks[i].clear();
  }
  m_hooks_p = false;
}

template <typename ID, ID N>
void
FeatureAPIHooks<ID, N>::add(ID id, INKContInternal *cont, int priority)
{
  if (likely(is_valid(id))) {
    m_hooks_p = true;
    m_hooks[id].add(cont, priority);
  }
}

template <typename ID, ID N>
APIHook *
FeatureAPIHooks<ID, N>::get(ID id)
{
  return likely(is_valid(id)) ? m_hooks[id].head() : NULL;
}

template <typename ID, ID N>
APIHook const *
FeatureAPIHooks<ID, N>::get(ID id) const
{
  return likely(is_valid(id)) ? m_hooks[id].head() : NULL;
}

template <typename ID, ID N> APIHooks *FeatureAPIHooks<ID, N>::operator[](ID id)
{
  return likely(is_valid(id)) ? &(m_hooks[id]) : NULL;
}

template <typename ID, ID N> APIHooks const *FeatureAPIHooks<ID, N>::operator[](ID id) const
{
  return likely(is_valid(id)) ? &(m_hooks[id]) : NULL;
}

template <typename ID, ID N>
void
FeatureAPIHooks<ID, N>::invoke(ID id, int event, void *data)
{
  if (likely(is_valid(id))) {
    m_hooks[id].invoke(event, data);
  }
}

template <typename ID, ID N>
bool
FeatureAPIHooks<ID, N>::has_hooks() const
{
  return m_hooks_p;
}

template <typename ID, ID N>
int
FeatureAPIHooks<ID, N>::threshold() const
{
  return m_threshold;
}

template <typename ID, ID N>
void
FeatureAPIHooks<ID, N>::set_threshold(int priority)
{
  m_threshold = priority;
}

template <typename ID, ID N>
void
FeatureAPIHooks<ID, N>::set_threshold(TSHttpHookID id, int priority)
{
  if (is_valid(id))
    m_hooks[id].m_threshold = priority;
}

template <typename ID, ID N>
bool
FeatureAPIHooks<ID, N>::is_valid(ID id)
{
  return 0 <= id && id < N;
}

class HttpAPIHooks : public FeatureAPIHooks<TSHttpHookID, TS_HTTP_LAST_HOOK>
{
};

typedef enum {
  TS_SSL_INTERNAL_FIRST_HOOK,
  TS_VCONN_PRE_ACCEPT_INTERNAL_HOOK = TS_SSL_INTERNAL_FIRST_HOOK,
  TS_SSL_CERT_INTERNAL_HOOK,
  TS_SSL_SERVERNAME_INTERNAL_HOOK,
  TS_SSL_INTERNAL_LAST_HOOK
} TSSslHookInternalID;

class SslAPIHooks : public FeatureAPIHooks<TSSslHookInternalID, TS_SSL_INTERNAL_LAST_HOOK>
{
};

class LifecycleAPIHooks : public FeatureAPIHooks<TSLifecycleHookID, TS_LIFECYCLE_LAST_HOOK>
{
};

class ConfigUpdateCallback : public Continuation
{
public:
  ConfigUpdateCallback(INKContInternal *contp) : Continuation(contp->mutex.get()), m_cont(contp)
  {
    SET_HANDLER(&ConfigUpdateCallback::event_handler);
  }

  int
  event_handler(int, void *)
  {
    if (m_cont->mutex) {
      MUTEX_TRY_LOCK(trylock, m_cont->mutex, this_ethread());
      if (!trylock.is_locked()) {
        eventProcessor.schedule_in(this, HRTIME_MSECONDS(10), ET_TASK);
      } else {
        m_cont->handleEvent(TS_EVENT_MGMT_UPDATE, NULL);
        delete this;
      }
    } else {
      m_cont->handleEvent(TS_EVENT_MGMT_UPDATE, NULL);
      delete this;
    }

    return 0;
  }

private:
  INKContInternal *m_cont;
};

class ConfigUpdateCbTable
{
public:
  ConfigUpdateCbTable();
  ~ConfigUpdateCbTable();

  void insert(INKContInternal *contp, const char *name);
  void invoke(const char *name);
  void invoke(INKContInternal *contp);

private:
  InkHashTable *cb_table;
};

/// Container for the state of HTTP API hook invocation.
class HttpHookState
{
private:
  typedef Vec<PluginInfo const *> PIVec;

public:
  /// Scope tags for interacting with a live instance.
  enum ScopeTag { GLOBAL, SESSION, TRANSACTION };

  struct disabled_iterator {
  private:
    typedef disabled_iterator self;

  public:
    disabled_iterator();
    self &operator++();
    bool operator==(self const &that) const;
    bool operator!=(self const &that) const;
    PluginInfo const &operator*() const;
    PluginInfo const *operator->() const;

  private:
    disabled_iterator(PIVec const*v, unsigned int idx);

    PIVec const*_v;
    unsigned int _idx;

    friend class HttpHookState;
  };

  /// Default constructor.
  HttpHookState();

  /// Initialize the hook state.
  /// This tracks up to 3 sources of hooks. The argument order to this method is used
  /// to break priority ties (callbacks from earlier args are invoked earlier).
  /// The order in terms of @a ScopeTag is GLOBAL, SESSION, TRANSACTION.
  void init(TSHttpHookID id, HttpAPIHooks const *global, HttpAPIHooks const *ua = NULL, HttpAPIHooks const *sm = NULL);
  /// Set the hook invocation threshold for a specific scope.
  void setThreshold(int t, ScopeTag scope);
  /// Set the hook invocation threshold for a specific scope and hook.
  void setThreshold(TSHttpHookID id, int t, ScopeTag scope);
  /// Select a hook for invocation and advance the state to the next valid hook.
  /// @return @c NULL if no current hook.
  APIHook const *getNext();
  /// Get the hook ID
  TSHttpHookID id() const;
  /// Override global plugin enable state.
  /// The plugin is forced enabled if @a
  void enable(PluginInfo const *pi, bool enable_p);
  /// Check if a plugin is enabled, based on local overrides and global state.
  bool is_enabled(PluginInfo const *pi);

  disabled_iterator disabled_begin() const;
  disabled_iterator disabled_end() const;

protected:
  /// Track the state of one scope of hooks.
  struct Scope {
    APIHook const *_c;    ///< Current hook (candidate for invocation).
    APIHook const *_p;    ///< Previous hook (already invoked).
    int _scope_threshold; ///< Threshold from the scope.
    int _hook_threshold;  ///< Threshold for this set of hooks.

    /// Initialize the scope.
    void init(HttpAPIHooks const *scope, TSHttpHookID id);
    /// Clear the scope.
    void clear();
    /// Return the current candidate for threshold @a t
    /// Can update state to account for hooks added since last candidate.
    APIHook const *candidate(int t, int prev_t);
    /// Advance state to the next hook.
    void operator++();
    /// Get the effective threshold.
    int get_effective_threshold() const;
  };

  /// Set @a _threshold based on the thresholds for the scopes.
  /// @return @a _threhold after updating.
  int update_effective_threshold();

private:
  TSHttpHookID _id;
  Scope _global;      ///< Chain from global hooks.
  Scope _ssn;         ///< Chain from session hooks.
  Scope _txn;         ///< Chain from transaction hooks.
  int _threshold;     ///< Effective threshold, gathered from the various sources.
  int _last_priority; ///< Priority of the most recently invoked callback.
                      /** Plugin enablement list.
                          @internal A bit ugly but after much thought I think this is the minimal amount.
                          The data is optimized on the assumption the set of plugins in play is small and
                          usually zero. Therefore the pointers are stored munged, using the bottom bit to
                          indicate whether the override is to enable (1) or disable (0).
                      */
  PIVec m_pi_list;
};

inline TSHttpHookID
HttpHookState::id() const
{
  return _id;
}

inline HttpHookState::disabled_iterator
HttpHookState::disabled_begin() const
{
  return disabled_iterator(&m_pi_list, 0);
}

inline HttpHookState::disabled_iterator
HttpHookState::disabled_end() const
{
  return disabled_iterator(&m_pi_list, m_pi_list.n);
}

inline int
HttpHookState::Scope::get_effective_threshold() const
{
  return _hook_threshold >= 0 ? _hook_threshold : _scope_threshold;
}

inline HttpHookState::disabled_iterator::disabled_iterator() : _v(NULL), _idx(-1)
{
}

inline HttpHookState::disabled_iterator &HttpHookState::disabled_iterator::operator++()
{
  if (NULL != _v && _idx < _v->n)
    ++_idx;
  return *this;
}

inline bool HttpHookState::disabled_iterator::operator==(self const &that) const
{
  return _v == that._v && _idx == that._idx;
}

inline bool HttpHookState::disabled_iterator::operator!=(self const &that) const
{
  return !(*this == that);
}

inline PluginInfo const &HttpHookState::disabled_iterator::operator*() const
{
  return *((*_v)[_idx]);
}

inline PluginInfo const *HttpHookState::disabled_iterator::operator->() const
{
  return (*_v)[_idx];
}

inline HttpHookState::disabled_iterator::disabled_iterator(PIVec const*v, unsigned int idx) : _v(v), _idx(idx)
{
}

void api_init();

extern HttpAPIHooks *http_global_hooks;
extern LifecycleAPIHooks *lifecycle_hooks;
extern SslAPIHooks *ssl_hooks;
extern ConfigUpdateCbTable *global_config_cbs;

#endif /* __INK_API_INTERNAL_H__ */
