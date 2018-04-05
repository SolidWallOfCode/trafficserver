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
#include <sstream>
#include <tuple>
#include <ts/ink_platform.h>
#include <ts/ink_config.h>
#include <ts/ink_mutex.h>
#include <ts/ink_inet.h>
#include <ts/Map.h>
#include <ts/Diags.h>
#include <ts/CryptoHash.h>
#include "HttpProxyAPIEnums.h"
#include "Show.h"

namespace ts
{
inline BufferWriter &
bwformat(BufferWriter &w, BWFSpec const &spec, TSServerSessionSharingMatchType type)
{
  static const string_view name[] = {"None"_sv, "Both"_sv, "IP Address"_sv, "Host Name"_sv};
  if (spec.has_numeric_type()) {
    bwformat(w, spec, static_cast<unsigned int>(type));
  } else {
    bwformat(w, spec, name[type]);
  }
  return w;
}
} // ts

/**
 * Singleton class to keep track of the number of outbound connnections.
 *
 * Outbound connections are divided in to equivalence classes (called "groups" here) based on the
 * session matching setting. A count is stored for each group.
 */
class OutboundConnTracker
{
  using self_type = OutboundConnTracker;

public:
  // Non-copyable.
  OutboundConnTracker(const self_type &) = delete;
  self_type &operator=(const self_type &) = delete;

  /// A record for the outbound connection count.
  /// These are stored per outbound session equivalence class, as determined by the session matching.
  struct Group {
    struct Key {
      IpEndpoint const &_addr;
      CryptoHash const &_fqdn_hash;
      TSServerSessionSharingMatchType _match_type;
    };

    using Time   = std::chrono::high_resolution_clock::time_point;
    using Ticker = Time::rep; ///< Raw type used to track HR ticks.
    static constexpr std::chrono::seconds ALERT_DELAY{60};

    IpEndpoint _addr;                            ///< Remote address & port.
    CryptoHash _fqdn_hash;                       ///< Hash of the host name.
    TSServerSessionSharingMatchType _match_type; ///< Outbound session matching type.
    std::atomic<int> _count;                     ///< Number of outbound connections.
    std::atomic<Ticker> _last_alert;             ///< Absolute time of the last alert.
    std::atomic<int> _blocked;                   ///< Number of outbound connections blocked since last alert.
    std::atomic<int> _queued;                    ///< # of connections queued, waiting for a connection.
    LINK(Group, _link);                          ///< Intrusive hash table support.
    Key _key = {_addr, _fqdn_hash, _match_type}; ///< Pre-constructed key for performance on lookup.

    /// Constructor.
    Group(Key const &key);
    /// Key equality checker.
    static bool equal(Key const &lhs, Key const &rhs);
    /// Hashing function.
    static uint64_t hash(Key const &);
    /// Generate alert message.
    /// This is a modifying call - internal state will be updated to prevent too frequent alerts.
    /// @param w A @c BufferWriter on which to write the output.
    /// @return @c true if an alert was generated, @c false otherwise.
    bool should_alert();
  };

  /**
   * Get the @c Group for the specified session properties.
   * @param ip IP address and port of the host.
   * @param hostname_hash Hash of the FQDN for the host.
   * @param match_type Session matching type.
   * @return Number of connections
   */
  static Group *get(const IpEndpoint &addr, const CryptoHash &hostname_hash, TSServerSessionSharingMatchType match_type);

  /**
   * dump to JSON for stat page.
   * @return string containing a JSON encoding of the table.
   */
  static std::string to_json_string();

protected:
  /// Types and methods for the hash table.
  struct HashDescriptor {
    using ID       = uint64_t;
    using Key      = Group::Key const &;
    using Value    = Group;
    using ListHead = DList(Value, _link);

    static ID
    hash(Key key)
    {
      return Group::hash(key);
    }
    static Key
    key(Value *v)
    {
      return v->_key;
    }
    static bool
    equal(Key lhs, Key rhs)
    {
      return Group::equal(lhs, rhs);
    }
  };

  /// Container type for the connection groups.
  using HashTable = TSHashTable<HashDescriptor>;

  /// Internal implementation class instance.
  struct Imp {
    Imp();

    HashTable _table; ///< Hash table of upstream groups.
    ink_mutex _mutex; ///< Lock for insert & find.
  };
  static Imp _imp;

  /// Get the implementation instance.
  /// @note This is done purely to provide subclasses to reuse methods in this class.
  Imp &instance();
};

inline OutboundConnTracker::Imp::Imp()
{
  ink_mutex_init(&_mutex);
};

inline OutboundConnTracker::Imp &
OutboundConnTracker::instance()
{
  return _imp;
}

inline OutboundConnTracker::Group *
OutboundConnTracker::get(IpEndpoint const &addr, CryptoHash const &fqdn_hash, TSServerSessionSharingMatchType match_type)
{
  if (TS_SERVER_SESSION_SHARING_MATCH_NONE == match_type) {
    return 0; // We can never match a node if match type is NONE
  }

  Group::Key key{addr, fqdn_hash, match_type};
  Group *g = nullptr;
  ink_scoped_mutex_lock lock(_imp._mutex);
  auto loc = _imp._table.find(key);
  if (loc.isValid()) {
    g = loc;
  } else {
    g = new Group(key);
    _imp._table.insert(g);
  }
  return g;
}

inline bool
OutboundConnTracker::Group::equal(const Key &lhs, const Key &rhs)
{
  TSServerSessionSharingMatchType mt = lhs._match_type;
  bool zret = mt == rhs._match_type && (mt == TS_SERVER_SESSION_SHARING_MATCH_IP || lhs._fqdn_hash == rhs._fqdn_hash) &&
              (mt == TS_SERVER_SESSION_SHARING_MATCH_HOST || ats_ip_addr_port_eq(&lhs._addr.sa, &rhs._addr.sa));

  if (is_debug_tag_set("conn_count")) {
    ts::LocalBufferWriter<2 * INET6_ADDRSTRLEN + 2 * CRYPTO_HEX_SIZE + 256> bw;
    bw.print("Comparing {}:{::p}:{} to {}:{::p}:{} -> {}", lhs._fqdn_hash, lhs._addr, lhs._match_type, rhs._fqdn_hash, rhs._addr,
             rhs._match_type, zret ? "match" : "fail");
    Debug("conn_count", "%.*s", static_cast<int>(bw.size()), bw.data());
  }

  return zret;
}

inline uint64_t
OutboundConnTracker::Group::hash(const Key &key)
{
  switch (key._match_type) {
  case TS_SERVER_SESSION_SHARING_MATCH_IP:
    return ats_ip_port_hash(&key._addr.sa);
  case TS_SERVER_SESSION_SHARING_MATCH_HOST:
    return key._fqdn_hash.fold();
  case TS_SERVER_SESSION_SHARING_MATCH_BOTH:
    return ats_ip_port_hash(&key._addr.sa) ^ key._fqdn_hash.fold();
  default:
    return 0;
  }
}

inline OutboundConnTracker::Group::Group(const Key &key) : _fqdn_hash(key._fqdn_hash), _match_type(key._match_type)
{
  ats_ip_copy(_addr, &key._addr);
  _key._match_type = _match_type; // make sure this gets updated.
}

Action *register_ShowConnectionCount(Continuation *, HTTPHdr *);

#if 0

  struct ConnAddr {
    IpEndpoint _addr;
    CryptoHash _fqdn_hash;
    TSServerSessionSharingMatchType _match_type;

    ConnAddr() : _match_type(TS_SERVER_SESSION_SHARING_MATCH_NONE)
    {
      ink_zero(_addr);
      ink_zero(_fqdn_hash);
    }

    ConnAddr(int x) : _match_type(TS_SERVER_SESSION_SHARING_MATCH_NONE)
    {
      ink_release_assert(x == 0);
      ink_zero(_addr);
      ink_zero(_fqdn_hash);
    }

    ConnAddr(const IpEndpoint &addr, const CryptoHash &hostname_hash, TSServerSessionSharingMatchType match_type)
      : _addr(addr), _fqdn_hash(hostname_hash), _match_type(match_type)
    {
    }

    ConnAddr(const IpEndpoint &addr, const char *hostname, TSServerSessionSharingMatchType match_type)
      : _addr(addr), _match_type(match_type)
    {
      CryptoContext().hash_immediate(_fqdn_hash, static_cast<const void *>(hostname), strlen(hostname));
    }

    operator bool() { return ats_is_ip(&_addr); }
    std::string
    getIpStr()
    {
      std::string str;
      if (*this) {
        ip_text_buffer buf;
        const char *ret = ats_ip_ntop(&_addr.sa, buf, sizeof(buf));
        if (ret) {
          str.assign(ret);
        }
      }
      return str;
    }

    std::string
    getHostnameHashStr()
    {
      char hashBuffer[CRYPTO_HEX_SIZE];
      return std::string(_fqdn_hash.toHexStr(hashBuffer));
    }
  };

  class ConnAddrHashFns
  {
  public:
    static uintptr_t
    hash(ConnAddr &addr)
    {
      if (addr._match_type == TS_SERVER_SESSION_SHARING_MATCH_IP) {
        return (uintptr_t)ats_ip_port_hash(&addr._addr.sa);
      } else if (addr._match_type == TS_SERVER_SESSION_SHARING_MATCH_HOST) {
        return (uintptr_t)addr._fqdn_hash.u64[0];
      } else if (addr._match_type == TS_SERVER_SESSION_SHARING_MATCH_BOTH) {
        return ((uintptr_t)ats_ip_port_hash(&addr._addr.sa) ^ (uintptr_t)addr._fqdn_hash.u64[0]);
      } else {
        return 0; // they will never be equal() because of it returns false for NONE matches.
      }
    }

    static int
    equal(ConnAddr &a, ConnAddr &b)
    {
      char addrbuf1[INET6_ADDRSTRLEN];
      char addrbuf2[INET6_ADDRSTRLEN];
      char crypto_hashbuf1[CRYPTO_HEX_SIZE];
      char crypto_hashbuf2[CRYPTO_HEX_SIZE];
      a._fqdn_hash.toHexStr(crypto_hashbuf1);
      b._fqdn_hash.toHexStr(crypto_hashbuf2);
      Debug("conn_count", "Comparing hostname hash %s dest %s match method %d to hostname hash %s dest %s match method %d",
            crypto_hashbuf1, ats_ip_nptop(&a._addr.sa, addrbuf1, sizeof(addrbuf1)), a._match_type, crypto_hashbuf2,
            ats_ip_nptop(&b._addr.sa, addrbuf2, sizeof(addrbuf2)), b._match_type);

      if (a._match_type != b._match_type || a._match_type == TS_SERVER_SESSION_SHARING_MATCH_NONE) {
        Debug("conn_count", "result = 0, a._match_type != b._match_type || a._match_type == TS_SERVER_SESSION_SHARING_MATCH_NONE");
        return 0;
      }

      if (a._match_type == TS_SERVER_SESSION_SHARING_MATCH_IP) {
        if (ats_ip_addr_port_eq(&a._addr.sa, &b._addr.sa)) {
          Debug("conn_count", "result = 1, a._match_type == TS_SERVER_SESSION_SHARING_MATCH_IP");
          return 1;
        } else {
          Debug("conn_count", "result = 0, a._match_type == TS_SERVER_SESSION_SHARING_MATCH_IP");
          return 0;
        }
      }

      if (a._match_type == TS_SERVER_SESSION_SHARING_MATCH_HOST) {
        if ((a._fqdn_hash.u64[0] == b._fqdn_hash.u64[0] && a._fqdn_hash.u64[1] == b._fqdn_hash.u64[1])) {
          Debug("conn_count", "result = 1, a._match_type == TS_SERVER_SESSION_SHARING_MATCH_HOST");
          return 1;
        } else {
          Debug("conn_count", "result = 0, a._match_type == TS_SERVER_SESSION_SHARING_MATCH_HOST");
          return 0;
        }
      }

      if (a._match_type == TS_SERVER_SESSION_SHARING_MATCH_BOTH) {
        if ((ats_ip_addr_port_eq(&a._addr.sa, &b._addr.sa)) &&
            (a._fqdn_hash.u64[0] == b._fqdn_hash.u64[0] && a._fqdn_hash.u64[1] == b._fqdn_hash.u64[1])) {
          Debug("conn_count", "result = 1, a._match_type == TS_SERVER_SESSION_SHARING_MATCH_BOTH");

          return 1;
        }
      }

      Debug("conn_count", "result = 0, a._match_type == TS_SERVER_SESSION_SHARING_MATCH_BOTH");
      return 0;
    }
  };

protected:
  // Hide the constructor and copy constructor
  OutboundConnTracker() { ink_mutex_init(&_mutex); }

  static HashMap<ConnAddr, ConnAddrHashFns, int> _hostCount;
  static ink_mutex _mutex;

private:
  void
  appendJSONPair(std::ostringstream &oss, const std::string &key, const int value)
  {
    oss << '\"' << key << "\": " << value;
  }

  void
  appendJSONPair(std::ostringstream &oss, const std::string &key, const std::string &value)
  {
    oss << '\"' << key << "\": \"" << value << '"';
  }
};

#endif
