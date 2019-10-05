/** @file

  A brief file description

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

#pragma once

#include <chrono>
#include <atomic>

#include "tscore/HashFNV.h"
#include "tscore/ink_time.h"
#include "tscore/CryptoHash.h"
#include "tscore/ink_align.h"
#include "tscore/ink_resolver.h"
#include "I_EventSystem.h"
#include "SRV.h"
#include "P_RefCountCache.h"

// Event returned on a lookup
#define EVENT_HOST_DB_LOOKUP (HOSTDB_EVENT_EVENTS_START + 0)
#define EVENT_HOST_DB_IP_REMOVED (HOSTDB_EVENT_EVENTS_START + 1)
#define EVENT_HOST_DB_GET_RESPONSE (HOSTDB_EVENT_EVENTS_START + 2)

#define EVENT_SRV_LOOKUP (SRV_EVENT_EVENTS_START + 0)
#define EVENT_SRV_IP_REMOVED (SRV_EVENT_EVENTS_START + 1)
#define EVENT_SRV_GET_RESPONSE (SRV_EVENT_EVENTS_START + 2)

//
// Data
//
struct HostDBContinuation;
struct ResolveInfo;

//
// The host database stores host information, most notably the
// IP address.
//
// Since host information is relatively small, we can afford to have
// a reasonable size memory cache, and use a (relatively) sparse
// disk representation to decrease # of seeks.
//
extern int hostdb_enable;
extern ts_time hostdb_current_interval;
extern unsigned int hostdb_ip_stale_interval;
extern unsigned int hostdb_ip_timeout_interval;
extern unsigned int hostdb_ip_fail_timeout_interval;
extern unsigned int hostdb_serve_stale_but_revalidate;
extern unsigned int hostdb_round_robin_max_count;

extern int hostdb_max_iobuf_index;

static inline unsigned int
makeHostHash(const char *string)
{
  ink_assert(string && *string);

  if (string && *string) {
    ATSHash32FNV1a fnv;
    fnv.update(string, strlen(string), ATSHash::nocase());
    fnv.final();
    return fnv.get();
  }

  return 0;
}

//
// Types
//

struct HostDBRecord;

struct SRVInfo {
  unsigned int srv_offset : 16; ///< Memory offset from @c HostDBInfo to name.
  unsigned int srv_weight : 16;
  unsigned int srv_priority : 16;
  unsigned int srv_port : 16;
  unsigned int key;
};

/** Information about a single target.
 *
 * - *valid* means data was retrieved successfully. Invalid info had a retrieval error.
 * - *failed* means the target has failed. The opposite of failed is *alive*.
 * - *alive* means not failed.
 * - *dead* means failed more recently than the failure window.
 * - *zombie* means failed longer ago than the failure window.
 */
struct HostDBInfo {
  using self_type = HostDBInfo; ///< Self reference type.

  /// Expected HTTP version.
  enum HttpVersion : uint8_t {
    HTTP_VERSION_UNDEFINED = 0,
    HTTP_VERSION_09        = 1,
    HTTP_VERSION_10        = 2,
    HTTP_VERSION_11        = 3,
  };

  /// Default constructor.
  HostDBInfo() = default;

  /// Absolute time of when this target failed.
  /// A value of zero (@c TS_TIME_ZERO ) indicates no failure.
  ts_time last_fail_time() const;

  /// Network address of target.
  sockaddr *addr();

  /// Network address of target.
  sockaddr const *addr() const;

  /// Target is alive - no known failure.
  bool is_alive();

  /// Target is failed but a connection attempt is active.
  bool is_zombie(ts_time now, ts_seconds fail_window);

  /// Target has failed and is still in the blocked time window.
  bool is_dead(ts_time now, ts_seconds fail_window);

  /** Mark as selected for connection.
   *
   * @param now Current time.
   * @param fail_window Failure window.
   * @return @c true if this target can be used for a connection, @c false otherwise.
   *
   * If a zombie is selected the failure time is updated to make it look dead to other threads in a thread safe
   * manner.
   */
  bool select(ts_time now, ts_seconds fail_window);

  /// Check if this info is valid.
  bool
  is_valid() const
  {
    return flags.f.is_valid;
  }

  /// Mark this info as invalid.
  void
  invalidate()
  {
    flags.f.is_valid = false;
  }

  /** Mark the entry as down.
   *
   * @param now Time of the failure.
   * @return @c true if @a this was marked down, @c false if not.
   *
   * This can return @c false if the entry is already marked down, in which case the failure time is not updated.
   */
  bool mark_down(ts_time now);

  /** Mark the target as up / alive.
   *
   * @return Previous alive state of the target.
   */
  bool mark_up();

  char const *srvname() const;

  HostDBInfo &
  operator=(HostDBInfo const &src)
  {
    if (this != &src) {
      memcpy(static_cast<void *>(this), static_cast<const void *>(&src), sizeof(*this));
    }
    return *this;
  }

  /// A target is either an IP address or an SRV record.
  /// The type should be indicated by @c flags.f.is_srv;
  union {
    IpEndpoint ip; ///< IP address / port data.
    SRVInfo srv;   ///< SRV record.
  } data;

  /// Migrating data.
  /// @{
  /// Last time a failure was recorded.
  std::atomic<ts_time> last_failure{TS_TIME_ZERO};
  /// Expected HTTP version of the target based on earlier transactions.
  HttpVersion http_version = HttpVersion::HTTP_VERSION_UNDEFINED;
  /// @}

  void
  migrate_from(self_type const &that)
  {
    this->last_failure = that.last_failure.load();
    this->http_version = that.http_version;
  }

protected:
  union {
    uint8_t all = 0;
    struct {
      /// Valid data.
      bool is_valid;
    } f;
  } flags;
};

inline ts_time
HostDBInfo::last_fail_time() const
{
  return last_failure;
}

inline sockaddr *
HostDBInfo::addr()
{
  return &data.ip.sa;
}

inline sockaddr const *
HostDBInfo::addr() const
{
  return &data.ip.sa;
}

inline bool
HostDBInfo::is_alive()
{
  return this->last_fail_time() != TS_TIME_ZERO;
}

inline bool
HostDBInfo::is_zombie(ts_time now, ts_seconds fail_window)
{
  auto last_fail = this->last_fail_time();
  return last_fail != TS_TIME_ZERO && last_fail + fail_window >= now;
}

inline bool
HostDBInfo::is_dead(ts_time now, ts_seconds fail_window)
{
  auto last_fail = this->last_fail_time();
  return (last_fail != TS_TIME_ZERO) && (last_fail + fail_window < now);
}

inline bool
HostDBInfo::mark_up()
{
  bool zret    = this->is_alive();
  last_failure = TS_TIME_ZERO;
  return zret;
}

inline bool
HostDBInfo::mark_down(ts_time now)
{
  auto t0{TS_TIME_ZERO};
  return last_failure.compare_exchange_strong(t0, now);
}

inline bool
HostDBInfo::select(ts_time now, ts_seconds fail_window)
{
  auto t0 = this->last_fail_time();
  if (t0 != TS_TIME_ZERO) {
    // Success means this is a zombie and this thread updated the failure time.
    return (t0 + fail_window >= now) && last_failure.compare_exchange_strong(t0, now);
  }
  return true; // Ready to go!
}

// ----

#if 0
struct HostDBRoundRobin {
  HostDBRoundRobin() = default;

  // This is the equivalent of a variable length array, we can't use a VLA because
  // HostDBInfo is a non-POD type -- this is the best we can do.
  HostDBInfo &operator[](unsigned n);

  ts::MemSpan<HostDBInfo> rr_info();

  // Return the allocation size of a HostDBRoundRobin struct suitable for storing
  // "count" HostDBInfo records.
  static unsigned
  size(unsigned count, unsigned srv_len = 0)
  {
    ink_assert(count > 0);
    return INK_ALIGN((sizeof(HostDBRoundRobin) + (count * sizeof(HostDBInfo)) + srv_len), 8);
  }

  /** Find the index of @a addr in member @a info.
      @return The index if found, -1 if not found.
  */
  int index_of(sockaddr const *addr);
  HostDBInfo *find_ip(sockaddr const *addr);
  // Find the srv target
  HostDBInfo *find_target(const char *target);
  /** Select the next entry after @a addr.
      @note If @a addr isn't an address in the round robin nothing is updated.
      @return The selected entry or @c nullptr if @a addr wasn't present.
   */
  HostDBInfo *select_next(sockaddr const *addr);

  /** Select the best upstream address from the round robin record.
   *
   * @param client_ip Remote inbound IP address.
   * @param now Current time.
   * @param fail_window Down server window.
   * @return The selected host record, or @c nullptr if there are no host records.
   *
   * @a client_ip is used to do hashing on the upstream addresses such that different clients
   * get different permutations of the hosts in the record.
   *
   * The returned host record will be down iff all hosts in the record are down.
   */
  HostDBInfo *select_best_http(ResolveInfo *resolve_info, ts_time now);
  HostDBInfo *select_best_srv(char *target, InkRand *rand, ts_time now, ts_seconds fail_window);

protected:
  HostDBInfo *
  data()
  {
    return reinterpret_cast<HostDBInfo *>(reinterpret_cast<char *>(this) + offset);
  }
};

// ----

inline HostDBInfo &
HostDBRoundRobin::operator[](unsigned int n)
{
  ink_assert(n < count);
  return this->data()[n];
}

inline ts::MemSpan<HostDBInfo>
HostDBRoundRobin::rr_info()
{
  return {this->data(), size_t(count)};
}
#endif

/// Type of data stored in a @c HostDBRecord.
enum class HostDBType : uint8_t {
  UNSPEC, ///< No valid data.
  ADDR,   ///< IP address.
  SRV,    ///< SRV record.
  HOST    ///< Hostname (reverse DNS)
};

char const *name_of(HostDBType t);
// ----
/** Root item for HostDB.
 * This is the container for HostDB data. It is always an array of @c HostDBInfo instances plus metadata.
 * All strings are C-strings and therefore don't need a distinct size.
 *
 */
class HostDBRecord : public RefCountObj
{
  friend class HostDBContinuation;
  using self_type = HostDBRecord;

  /// Size of the IO buffer block owned by @a this.
  /// If negative @a this is in not allocated memory.
  int _iobuffer_index{-1};

public:
  HostDBRecord()                      = default;
  HostDBRecord(self_type const &that) = delete;

  /// Type of data stored in this record.
  HostDBType record_type = HostDBType::UNSPEC;

  /// IP family of this record.
  sa_family_t af_family = AF_UNSPEC;

  /// Offset from @a this to the VLA.
  unsigned short rr_offset = 0;

  /** Total number (to compute space used). */
  unsigned short rr_count = 0;

  /** Number which have not failed a connect. */
  unsigned short rr_good = 0;

  /// Current active info.
  std::atomic<unsigned short> rr_idx = 0;

  /// Timing data for switch records in the RR.
  std::atomic<ts_time> rr_ctime;

  bool
  is_srv() const
  {
    return HostDBType::SRV == record_type;
  }

  char const *
  name() const
  {
    return static_cast<char const *>(this->apply_offset(name_offset));
  }

  void *
  apply_offset(unsigned offset)
  {
    return reinterpret_cast<char *>(this) + offset;
  }

  void const *
  apply_offset(unsigned offset) const
  {
    return reinterpret_cast<char const *>(this) + offset;
  }

  /// Get the array of info instances.
  ts::MemSpan<HostDBInfo>
  rr_info()
  {
    return {static_cast<HostDBInfo *>(this->apply_offset(rr_offset)), rr_count};
  }

  /// Find a host record by IP address.
  HostDBInfo *
  find(sockaddr const *addr)
  {
    for (auto &item : this->rr_info()) {
      if (ats_ip_addr_eq(addr, item.addr())) {
        return &item;
      }
    }
    return nullptr;
  }

  HostDBInfo *select_best_http(ResolveInfo *resolve_info, ts_time now);
  HostDBInfo *select_best_srv(char *target, InkRand *rand, ts_time now, ts_seconds fail_window);
  HostDBInfo *select_next(sockaddr const *addr);

  /// Hash key.
  uint64_t key;

  /// When the data was received.
  ts_time ip_timestamp;

  /// Valid duration of the data.
  ts_seconds ip_timeout_interval;

  bool
  is_failed() const
  {
    return flags.f.failed_p;
  }

  void
  set_failed()
  {
    flags.f.failed_p = true;
  }

  /// @return The time point when the item expires.
  ts_time expiry_time() const;

  ts_seconds ip_interval() const;

  ts_seconds ip_time_remaining() const;

  bool is_ip_stale() const;

  bool is_ip_timeout() const;

  bool is_ip_fail_timeout() const;

  void refresh_ip();

  bool serve_stale_but_revalidate() const;

  /// Deallocate @a this.
  void free() override;

  /** Allocate an instance.
   *
   * @param extra Additional space required for the record.
   * @return An initialized instance with a zero reference count.
   */
  static self_type *alloc(size_t extra = 0);

  /** Allocation and initialize an instance from a serialized buffer.
   *
   * @param buff Serialization data.
   * @param size Size of @a buff.
   * @return An instance initialized from @a buff.
   */
  static self_type *unmarshall(char *buff, unsigned size);

  /// Database version.
  static constexpr ts::VersionNumber Version{3, 0};

protected:
  /// Location of name relative to @a this.
  /// If this is a @c HOST record, this is the resolved named and the query was based on the IP address.
  /// Otherwise this is the name used in the query.
  unsigned name_offset;

  union {
    uint16_t all;
    struct {
      unsigned failed_p : 1; ///< DNS error.
    } f;
  } flags{0};

  char *
  name_ptr()
  {
    return static_cast<char *>(this->apply_offset(name_offset));
  }
};

struct HostDBCache;
struct HostDBHash;

// Prototype for inline completion function or
//  getbyname_imm()
using cb_process_result_pfn = void (Continuation::*)(HostDBRecord *r);

Action *iterate(Continuation *cont);

/** Information for doing host resolution for a requst.
 *
 * This is effectively a state object for a request attempting to connect upstream. Information about its attempt
 * that are local to the request are kept here, while shared data is accessed via the @c HostDBInfo pointers.
 *
 * A primitive version of the IP address generator concept.
 */
struct ResolveInfo {
  using self_type = ResolveInfo; ///< Self reference type.

  /// Not quite sure what this is for.
  enum UpstreamResolveStyle { UNDEFINED_LOOKUP, ORIGIN_SERVER, PARENT_PROXY, HOST_NONE };

  /** Origin server address source selection.

      If config says to use CTA (client target addr) state is TRY_CLIENT, otherwise it
      remains the default. If the connect fails then we switch to a USE. We go to USE_HOSTDB if (1)
      the HostDB lookup is successful and (2) some address other than the CTA is available to try.
      Otherwise we keep retrying on the CTA (USE_CLIENT) up to the max retry value.  In essence we
      try to treat the CTA as if it were another RR value in the HostDB record.
   */
  enum class OS_Addr {
    TRY_DEFAULT, ///< Initial state, use what config says.
    TRY_HOSTDB,  ///< Try HostDB data.
    TRY_CLIENT,  ///< Try client target addr.
    USE_HOSTDB,  ///< Force use of HostDB target address.
    USE_CLIENT   ///< Use client target addr, no fallback.
  };

  /// Keep a reference to the base HostDB object, so it doesn't get GC'd.
  Ptr<HostDBRecord> record;
  HostDBInfo *active = nullptr; ///< Active host record.

  /// Working address. The meaning / source of the value depends on other elements.
  IpEndpoint addr;

  int attempts = 0;            ///< Number of connection attempts.
  ts_seconds fail_window{300}; ///< Down server blackout time (txn overridable)

  char *lookup_name                   = nullptr;
  char srv_hostname[MAXDNAME]         = {0};
  const sockaddr *inbound_remote_addr = nullptr; ///< Remote address of inbound client - used for hashing.
  const sockaddr *client_target_addr  = nullptr; ///< Set if trying a transparent connection.
  in_port_t srv_port                  = 0;       ///< Port from SRV lookup.

  OS_Addr os_addr_style           = OS_Addr::TRY_DEFAULT;
  HostResStyle host_res_style     = HOST_RES_IPV4;
  UpstreamResolveStyle looking_up = UNDEFINED_LOOKUP;

  HostDBInfo::HttpVersion http_version = HostDBInfo::HTTP_VERSION_UNDEFINED;

  bool resolved_p = false; ///< If there is a valid, resolved address in @a addr.

  /// Flag for @a addr being set externally.
  bool api_addr_set_p = false;

  /*** Set to true by default.  If use_client_target_address is set
   * to 1, this value will be set to false if the client address is
   * not in the DNS pool */
  bool cta_validated_p = true;

  /// If upstream was in zombie mode.
  bool is_zombie = false;

  ResolveInfo() = default;

  /** Force the address to resolve.
   *
   * @param sa Address to use for the upstream.
   * @return @c true if successful, @c false if error.
   *
   * This fails if @a sa isn't a valid IP address.
   */
  bool
  set_upstream_address(const sockaddr *sa)
  {
    if (ats_ip_copy(&addr, sa)) {
      api_addr_set_p = true;
      return true;
    }
    return false;
  }

  /** Mark the current active record as dead.
   *
   * @param now Time of failure.
   * @return @c true if the server was marked as dead, @c false if not.
   *
   *
   */
  bool
  mark_active_server_dead(ts_time now)
  {
    return active ? active->mark_down(now) : false;
  }

  bool
  mark_active_server_alive()
  {
    if (is_zombie && active) {
      active->mark_up();
    }
    is_zombie = false;
    return false;
  }
};

/** The Host Database access interface. */
struct HostDBProcessor : public Processor {
  friend struct HostDBSync;
  // Public Interface

  // Lookup Hostinfo by name
  //    cont->handleEvent( EVENT_HOST_DB_LOOKUP, HostDBInfo * ); on success
  //    cont->handleEVent( EVENT_HOST_DB_LOOKUP, 0); on failure
  // Failure occurs when the host cannot be DNS-ed
  // NOTE: Will call the continuation back before returning if data is in the
  //       cache.  The HostDBInfo * becomes invalid when the callback returns.
  //       The HostDBInfo may be changed during the callback.

  enum {
    HOSTDB_DO_NOT_FORCE_DNS   = 0,
    HOSTDB_ROUND_ROBIN        = 0,
    HOSTDB_FORCE_DNS_RELOAD   = 1,
    HOSTDB_FORCE_DNS_ALWAYS   = 2,
    HOSTDB_DO_NOT_ROUND_ROBIN = 4
  };

  /// Optional parameters for getby...
  struct Options {
    typedef Options self;                                  ///< Self reference type.
    int port                    = 0;                       ///< Target service port (default 0 -> don't care)
    int flags                   = HOSTDB_DO_NOT_FORCE_DNS; ///< Processing flags (default HOSTDB_DO_NOT_FORCE_DNS)
    int timeout                 = 0;                       ///< Timeout value (default 0 -> default timeout)
    HostResStyle host_res_style = HOST_RES_IPV4;           ///< How to query host (default HOST_RES_IPV4)

    Options() {}
    /// Set the flags.
    self &
    setFlags(int f)
    {
      flags = f;
      return *this;
    }
  };

  /// Default options.
  static Options const DEFAULT_OPTIONS;

  HostDBProcessor() {}
  inkcoreapi Action *getbyname_re(Continuation *cont, const char *hostname, int len, Options const &opt = DEFAULT_OPTIONS);

  Action *getbynameport_re(Continuation *cont, const char *hostname, int len, Options const &opt = DEFAULT_OPTIONS);

  Action *getSRVbyname_imm(Continuation *cont, cb_process_result_pfn process_srv_info, const char *hostname, int len,
                           Options const &opt = DEFAULT_OPTIONS);

  Action *getbyname_imm(Continuation *cont, cb_process_result_pfn process_hostdb_info, const char *hostname, int len,
                        Options const &opt = DEFAULT_OPTIONS);

  Action *iterate(Continuation *cont);

  /** Lookup Hostinfo by addr */
  Action *getbyaddr_re(Continuation *cont, sockaddr const *aip);

  /** Configuration. */
  static int hostdb_strict_round_robin;
  static int hostdb_timed_round_robin;

  // Processor Interface
  /* hostdb does not use any dedicated event threads
   * currently. Dont pass any value to start
   */
  int start(int no_of_additional_event_threads = 0, size_t stacksize = DEFAULT_STACKSIZE) override;

  // Private
  HostDBCache *cache();

private:
  Action *getby(Continuation *cont, cb_process_result_pfn cb_process_result, HostDBHash &hash, Options const &opt);

public:
  /** Update / assign to a HostDB record.
   * @param name Hostname.
   * @param aip Address and/or port.
   *
   * @a aip can carry address and / or port information. If setting just by a port value, the address should be set to INADDR_ANY
   * which is of type IPv4.
   *
   * This can update asynchronously if the lock for the record cannot be obtained immediately.
   */
  void setby(ts::TextView name, sockaddr const *aip);

  void setby_srv(const char *hostname, int len, const char *target);
};

void run_HostDBTest();

extern inkcoreapi HostDBProcessor hostDBProcessor;

void ink_hostdb_init(ts::ModuleVersion version);

inline ts_time
HostDBRecord::expiry_time() const
{
  return ip_timestamp + ip_timeout_interval + ts_seconds(hostdb_serve_stale_but_revalidate);
}

inline ts_seconds
HostDBRecord::ip_interval() const
{
  static constexpr ts_seconds ZERO{0};
  static constexpr ts_seconds MAX{0x7FFFFFFF};
  return std::clamp(std::chrono::duration_cast<ts_seconds>((hostdb_current_interval - ip_timestamp)), ZERO, MAX);
}

inline ts_seconds
HostDBRecord::ip_time_remaining() const
{
  return ip_timeout_interval - this->ip_interval();
}

inline bool
HostDBRecord::is_ip_stale() const
{
  return ip_timeout_interval >= ts_seconds(2 * hostdb_ip_stale_interval) && ip_interval() >= ts_seconds(hostdb_ip_stale_interval);
}

inline bool
HostDBRecord::is_ip_timeout() const
{
  return ip_interval() >= ip_timeout_interval;
}

inline bool
HostDBRecord::is_ip_fail_timeout() const
{
  return ip_interval() >= ts_seconds(hostdb_ip_fail_timeout_interval);
}

inline void
HostDBRecord::refresh_ip()
{
  ip_timestamp = hostdb_current_interval;
}
