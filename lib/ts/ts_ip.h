/** @file

  IP address and network related classes.

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

//#include <sys/socket.h>
#include <netinet/in.h>
//#include <netdb.h>
#include <string_view>
#include <ts/Numeric.h>

#include <ts/BufferWriterForward.h>

namespace ts
{
struct IpAddr; // forward declare.

/** A union to hold the standard IP address structures.
    By standard we mean @c sockaddr compliant.

    We use the term "endpoint" because these contain more than just the
    raw address, all of the data for an IP endpoint is present.

    @internal This might be useful to promote to avoid strict aliasing
    problems.  Experiment with it here to see how it works in the
    field.

    @internal @c sockaddr_storage is not present because it is so
    large and the benefits of including it are small. Use of this
    structure will make it easy to add if that becomes necessary.

 */
union IpEndpoint {
  using self_type = IpEndpoint; ///< Self reference type.

  struct sockaddr sa;      ///< Generic address.
  struct sockaddr_in sa4;  ///< IPv4
  struct sockaddr_in6 sa6; ///< IPv6

  /// Invalidate a @c sockaddr.
  static void invalidate(sockaddr *addr);
  /// Invalidate this endpoint.
  self_type &invalidate();

  /** Copy (assign) the contents of @a src to @a dst.
   *
   * The caller must ensure @a dst is large enough to hold the contents of @a src, the size of which
   * can vary depending on the type of address in @a dst.
   *
   * @param dst Destination.
   * @param src Source.
   * @return @c true if @a dst is a valid IP address, @c false otherwise.
   */
  static bool assign(sockaddr *dst, sockaddr const *src);

  /** Assign from a socket address.
      The entire address (all parts) are copied if the @a ip is valid.
  */
  self_type &assign(sockaddr const *addr);

  /// Assign from an @a addr and @a port.
  self_type &assign(IpAddr const &addr, in_port_t port = 0);

  /// Test for valid IP address.
  bool is_valid() const;
  /// Test for IPv4.
  bool is_4() const;
  /// Test for IPv6.
  bool is_6() const;

  uint16_t family() const;

  /// Set to be any address for family @a family.
  /// @a family must be @c AF_INET or @c AF_INET6.
  /// @return This object.
  self_type &setToAnyAddr(int family);

  /// Set to be loopback for family @a family.
  /// @a family must be @c AF_INET or @c AF_INET6.
  /// @return This object.
  self_type &setToLoopback(int family);

  /// Port in network order.
  in_port_t &port();
  /// Port in network order.
  in_port_t port() const;
  /// Port in host horder.
  in_port_t host_order_port() const;
  /// Port in network order from @a sockaddr.
  static in_port_t &port(sockaddr *sa);
  /// Port in network order from @a sockaddr.
  static in_port_t port(sockaddr const *sa);
  /// Port in host order directly from a @c sockaddr
  static in_port_t host_order_port(sockaddr const *sa);

  operator sockaddr *() { return &sa; }
  operator sockaddr const *() const { return &sa; }

  static std::string_view family_name(uint16_t family);
};

/** Storage for an IP address.
    This should be presumed to be in network order.
 */
class IpAddr
{
  using self_type = IpAddr; ///< Self reference type.
public:
  static constexpr size_t IP4_SIZE  = sizeof(in_addr_t); ///< Size of IPv4 address in bytes.
  static constexpr size_t IP6_SIZE  = sizeof(in6_addr);  ///< Size of IPv6 address in bytes.
  static constexpr size_t IP6_QUADS = IP6_SIZE / 2;      ///< # of quads in an IPv6 address.

  IpAddr(); ///< Default constructor - invalid result.

  /// Construct using IPv4 @a addr.
  explicit constexpr IpAddr(in_addr_t addr);
  /// Construct using IPv6 @a addr.
  explicit constexpr IpAddr(in6_addr const &addr);
  /// Construct from @c sockaddr.
  explicit IpAddr(sockaddr const *addr);
  /// Construct from @c IpEndpoint.
  explicit IpAddr(IpEndpoint const &addr);
  /// Construct from text representation.
  /// If the @a text is invalid the result is an invalid instance.
  explicit IpAddr(std::string_view text);

  /// Set to the address in @a addr.
  self_type &assign(sockaddr const *addr);
  /// Set to the address in @a addr.
  self_type &assign(sockaddr_in const *addr);
  /// Set to the address in @a addr.
  self_type &assign(sockaddr_in6 const *addr);
  /// Set to the address in @a addr.
  self_type &assign(in_addr_t addr);
  /// Set to address in @a addr.
  self_type &assign(in6_addr const &addr);

  /// Assign from end point.
  self_type &operator=(IpEndpoint const &ip);
  /// Assign from IPv4 raw address.
  self_type &operator=(in_addr_t ip);
  /// Assign from IPv6 raw address.
  self_type &operator=(in6_addr const &ip);
  /// Assign from @c sockaddr
  self_type &operator=(sockaddr const *addr);

  /** Parse a string for an IP address.

      The address resuling from the parse is copied to this object if the conversion is successful,
      otherwise this object is invalidated.

      @return @c true on success, @c false otherwise.
  */
  bool parse(std::string_view const &str);

  /** Output to a string.
      @return The string @a dest.
  */
  char *toString(char *dest, ///< [out] Destination string buffer.
                 size_t len  ///< [in] Size of buffer.
                 ) const;

  /// Generic compare.
  int cmp(self_type const &that) const;

  /** Return a normalized hash value.
      - Ipv4: the address in host order.
      - Ipv6: folded 32 bit of the address.
      - Else: 0.
  */
  uint32_t hash() const;

  /** The hashing function embedded in a functor.
      @see hash
  */
  struct Hasher {
    uint32_t
    operator()(self_type const &ip) const
    {
      return ip.hash();
    }
  };

  /// Test for same address family.
  /// @c return @c true if @a that is the same address family as @a this.
  bool isCompatibleWith(self_type const &that);

  /// Get the address family.
  /// @return The address family.
  uint16_t family() const;
  /// Test for IPv4.
  bool is_4() const;
  /// Test for IPv6.
  bool is_6() const;

  in_addr_t raw_ip4() const;
  in6_addr const &raw_ip6() const;
  uint8_t const *raw_octet() const;
  uint64_t const *raw_64() const;

  /// Test for validity.
  bool is_valid() const;

  /// Make invalid.
  self_type &invalidate();

  /// Test for multicast
  bool is_multicast() const;

  /// Test for loopback
  bool is_loopback() const;

  ///< Pre-constructed invalid instance.
  static self_type const INVALID;

protected:
  friend bool operator==(self_type const &, self_type const &);

  uint16_t _family{AF_UNSPEC}; ///< Protocol family.

  /// Address data.
  union raw_addr_type {
    in_addr_t _ip4;                              ///< IPv4 address storage.
    in6_addr _ip6;                               ///< IPv6 address storage.
    uint8_t _octet[IP6_SIZE];                    ///< IPv4 octets.
    uint16_t _quad[IP6_SIZE / sizeof(uint16_t)]; ///< IPv6 quads.
    uint32_t _u32[IP6_SIZE / sizeof(uint32_t)];  ///< As 32 bit chunks.
    uint64_t _u64[IP6_SIZE / sizeof(uint64_t)];  ///< As 64 bit chunks.

    // Constructors needed so @c IpAddr can have @c constexpr constructors.
    constexpr raw_addr_type();
    constexpr raw_addr_type(in_addr_t addr);
    constexpr raw_addr_type(in6_addr const &addr);
  } _addr;
};

// @c constexpr constructor is required to initialize _something_, it can't be completely uninitializing.
inline constexpr IpAddr::raw_addr_type::raw_addr_type() : _ip4(INADDR_ANY) {}
inline constexpr IpAddr::raw_addr_type::raw_addr_type(in_addr_t addr) : _ip4(addr) {}
inline constexpr IpAddr::raw_addr_type::raw_addr_type(in6_addr const &addr) : _ip6(addr) {}

inline constexpr IpAddr::IpAddr(in_addr_t addr) : _family(AF_INET), _addr(addr) {}

inline constexpr IpAddr::IpAddr(in6_addr const &addr) : _family(AF_INET6), _addr(addr) {}

inline IpAddr::IpAddr(sockaddr const *addr)
{
  this->assign(addr);
}

inline IpAddr::IpAddr(IpEndpoint const &addr)
{
  this->assign(&addr.sa);
}

inline IpAddr &
IpAddr::operator=(in_addr_t addr)
{
  _family    = AF_INET;
  _addr._ip4 = addr;
  return *this;
}

inline IpAddr &
IpAddr::operator=(in6_addr const &addr)
{
  _family    = AF_INET6;
  _addr._ip6 = addr;
  return *this;
}

inline IpAddr &
IpAddr::operator=(IpEndpoint const &addr)
{
  return this->assign(&addr.sa);
}

inline IpAddr &
IpAddr::operator=(sockaddr const *addr)
{
  return this->assign(addr);
}

inline uint16_t
IpAddr::family() const
{
  return _family;
}

inline bool
IpAddr::is_4() const
{
  return AF_INET == _family;
}

inline bool
IpAddr::is_6() const
{
  return AF_INET6 == _family;
}

inline bool
IpAddr::isCompatibleWith(self_type const &that)
{
  return this->is_valid() && _family == that._family;
}

inline bool
IpAddr::is_loopback() const
{
  return (AF_INET == _family && 0x7F == _addr._octet[0]) || (AF_INET6 == _family && IN6_IS_ADDR_LOOPBACK(&_addr._ip6));
}

inline bool
operator==(IpAddr const &lhs, IpAddr const &rhs)
{
  if (lhs._family != rhs._family)
    return false;
  switch (lhs._family) {
  case AF_INET:
    return lhs._addr._ip4 == rhs._addr._ip4;
  case AF_INET6:
    return 0 == memcmp(&lhs._addr._ip6, &rhs._addr._ip6, IpAddr::IP6_SIZE);
  case AF_UNSPEC:
    return true;
  default:
    break;
  }
  return false;
}

inline bool
operator!=(IpAddr const &lhs, IpAddr const &rhs)
{
  return !(lhs == rhs);
}

inline IpAddr &
IpAddr::assign(in_addr_t addr)
{
  _family    = AF_INET;
  _addr._ip4 = addr;
  return *this;
}

inline IpAddr &
IpAddr::assign(in6_addr const &addr)
{
  _family    = AF_INET6;
  _addr._ip6 = addr;
  return *this;
}

inline IpAddr &
IpAddr::assign(sockaddr_in const *addr)
{
  if (addr) {
    _family    = AF_INET;
    _addr._ip4 = addr->sin_addr.s_addr;
  } else {
    _family = AF_UNSPEC;
  }
  return *this;
}

inline IpAddr &
IpAddr::assign(sockaddr_in6 const *addr)
{
  if (addr) {
    _family    = AF_INET6;
    _addr._ip6 = addr->sin6_addr;
  } else {
    _family = AF_UNSPEC;
  }
  return *this;
}

/// Assign from basic @c sockaddr.
inline IpAddr &
IpAddr::assign(sockaddr const *addr)
{
  if (addr) {
    switch (addr->sa_family) {
    case AF_INET:
      return this->assign(reinterpret_cast<sockaddr_in const *>(addr)->sin_addr.s_addr);
    case AF_INET6:
      return this->assign(reinterpret_cast<sockaddr_in6 const *>(addr)->sin6_addr);
    default:
      break;
    }
  }
  _family = AF_UNSPEC;
  return *this;
}

inline bool
IpAddr::is_valid() const
{
  return _family == AF_INET || _family == AF_INET6;
}

inline IpAddr &
IpAddr::invalidate()
{
  _family = AF_UNSPEC;
  return *this;
}

// Associated operators.
bool operator==(IpAddr const &lhs, sockaddr const *rhs);
inline bool
operator==(sockaddr const *lhs, IpAddr const &rhs)
{
  return rhs == lhs;
}
inline bool
operator!=(IpAddr const &lhs, sockaddr const *rhs)
{
  return !(lhs == rhs);
}
inline bool
operator!=(sockaddr const *lhs, IpAddr const &rhs)
{
  return !(rhs == lhs);
}
inline bool
operator==(IpAddr const &lhs, IpEndpoint const &rhs)
{
  return lhs == &rhs.sa;
}
inline bool
operator==(IpEndpoint const &lhs, IpAddr const &rhs)
{
  return &lhs.sa == rhs;
}
inline bool
operator!=(IpAddr const &lhs, IpEndpoint const &rhs)
{
  return !(lhs == &rhs.sa);
}
inline bool
operator!=(IpEndpoint const &lhs, IpAddr const &rhs)
{
  return !(rhs == &lhs.sa);
}

inline bool
operator<(IpAddr const &lhs, IpAddr const &rhs)
{
  return -1 == lhs.cmp(rhs);
}

inline bool
operator>=(IpAddr const &lhs, IpAddr const &rhs)
{
  return lhs.cmp(rhs) >= 0;
}

inline bool
operator>(IpAddr const &lhs, IpAddr const &rhs)
{
  return 1 == lhs.cmp(rhs);
}

inline bool
operator<=(IpAddr const &lhs, IpAddr const &rhs)
{
  return lhs.cmp(rhs) <= 0;
}

inline in_addr_t
IpAddr::raw_ip4() const
{
  return _addr._ip4;
}

inline in6_addr const &
IpAddr::raw_ip6() const
{
  return _addr._ip6;
}

inline uint8_t const *
IpAddr::raw_octet() const
{
  return _addr._octet;
}

inline uint64_t const *
IpAddr::raw_64() const
{
  return _addr._u64;
}

inline uint32_t
IpAddr::hash() const
{
  uint32_t zret = 0;
  if (this->is_4()) {
    zret = ntohl(_addr._ip4);
  } else if (this->is_6()) {
    zret = _addr._u32[0] ^ _addr._u32[1] ^ _addr._u32[2] ^ _addr._u32[3];
  }
  return zret;
}

inline void
IpEndpoint::invalidate(sockaddr *sa)
{
  sa->sa_family = AF_UNSPEC;
}

inline IpEndpoint &
IpEndpoint::invalidate()
{
  sa.sa_family = AF_UNSPEC;
  return *this;
}

inline bool
IpEndpoint::is_valid() const
{
  return sa.sa_family == AF_INET || sa.sa_family == AF_INET6;
}

inline IpEndpoint &
IpEndpoint::assign(sockaddr const *src)
{
  self_type::assign(&sa, src);
  return *this;
}

inline in_port_t &
IpEndpoint::port()
{
  return self_type::port(&sa);
}

inline in_port_t
IpEndpoint::port() const
{
  return self_type::port(&sa);
}

inline in_port_t
IpEndpoint::host_order_port() const
{
  return ntohs(this->port());
}

inline in_port_t &
IpEndpoint::port(sockaddr *sa)
{
  switch (sa->sa_family) {
  case AF_INET:
    return reinterpret_cast<sockaddr_in *>(sa)->sin_port;
  case AF_INET6:
    return reinterpret_cast<sockaddr_in6 *>(sa)->sin6_port;
  }
  // Force a failure upstream by returning a null reference.
  return *static_cast<in_port_t *>(nullptr);
}

inline in_port_t
IpEndpoint::port(sockaddr const *addr)
{
  return self_type::port(const_cast<sockaddr *>(addr));
}

inline in_port_t
IpEndpoint::host_order_port(sockaddr const *addr)
{
  return ntohs(self_type::port(addr));
}

#if 0
inline IpEndpoint &
IpEndpoint::assign(IpAddr const &addr, in_port_t port)
{
  ats_ip_set(&sa, addr, port);
  return *this;
}

inline bool
IpEndpoint::isValid() const
{
  return ats_is_ip(this);
}

inline bool
IpEndpoint::isIp4() const
{
  return AF_INET == sa.sa_family;
}
inline bool
IpEndpoint::isIp6() const
{
  return AF_INET6 == sa.sa_family;
}
inline uint16_t
IpEndpoint::family() const
{
  return sa.sa_family;
}

inline IpEndpoint &
IpEndpoint::setToAnyAddr(int family)
{
  ink_zero(*this);
  sa.sa_family = family;
  if (AF_INET == family) {
    sin.sin_addr.s_addr = INADDR_ANY;
#if HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
    sin.sin_len = sizeof(sockaddr_in);
#endif
  } else if (AF_INET6 == family) {
    sin6.sin6_addr = in6addr_any;
#if HAVE_STRUCT_SOCKADDR_IN6_SIN6_LEN
    sin6.sin6_len = sizeof(sockaddr_in6);
#endif
  }
  return *this;
}

inline IpEndpoint &
IpEndpoint::setToLoopback(int family)
{
  ink_zero(*this);
  sa.sa_family = family;
  if (AF_INET == family) {
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#if HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
    sin.sin_len = sizeof(sockaddr_in);
#endif
  } else if (AF_INET6 == family) {
    static const struct in6_addr init = IN6ADDR_LOOPBACK_INIT;
    sin6.sin6_addr                    = init;
#if HAVE_STRUCT_SOCKADDR_IN6_SIN6_LEN
    sin6.sin6_len = sizeof(sockaddr_in6);
#endif
  }
  return *this;
}
#endif

// BufferWriter formatting support.
BufferWriter &bwformat(BufferWriter &w, BWFSpec const &spec, IpAddr const &addr);
BufferWriter &bwformat(BufferWriter &w, BWFSpec const &spec, sockaddr const *addr);
inline BufferWriter &
bwformat(BufferWriter &w, BWFSpec const &spec, IpEndpoint const &addr)
{
  return bwformat(w, spec, &addr.sa);
}
} // namespace ts
