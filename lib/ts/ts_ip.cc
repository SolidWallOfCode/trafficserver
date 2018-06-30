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

#include <fstream>
#include <ts/ts_ip.h>
#include <BufferWriter.h>

namespace
{
// These classes exist only to create distinguishable overloads.
struct MetaCase_0 {
};
struct MetaCase_1 : public MetaCase_0 {
};
// This is the final subclass so that callers can always use this, even if more cases are added.
struct MetaCaseArg : public MetaCase_1 {
  constexpr MetaCaseArg() {}
};
// A single static instance suffices for all uses.
static constexpr MetaCaseArg Meta_Case_Arg;
// Used to get a void type for auto + decltype
void
Meta_Case_Void_Func()
{
}

// sockaddr::sin_len - call this with a sockaddr type and Meta_Case_arg to set the sin_len
// member if it exists.
template <typename T>
auto
Set_Sockaddr_Len(T *, MetaCase_0 const &) -> decltype(Meta_Case_Void_Func())
{
}

template <typename T>
auto
Set_Sockaddr_Len(T *addr, MetaCase_1 const &) -> decltype(T::sin_len, Meta_Case_Void_Func())
{
  addr->sin_len = sizeof(T);
}

} // namespace

namespace ts
{
IpAddr const IpAddr::INVALID;

bool
IpEndpoint::assign(sockaddr *dst, sockaddr const *src)
{
  size_t n = 0;
  if (dst != src) {
    self_type::invalidate(dst);
    switch (src->sa_family) {
    case AF_INET:
      n = sizeof(sockaddr_in);
      break;
    case AF_INET6:
      n = sizeof(sockaddr_in6);
      break;
    }
    if (n) {
      memcpy(dst, src, n);
    }
  }
  return n != 0;
}

IpEndpoint &
IpEndpoint::assign(IpAddr const &src, in_port_t port)
{
  switch (src.family()) {
  case AF_INET: {
    memset(&sa4, 0, sizeof sa4);
    sa4.sin_family      = AF_INET;
    sa4.sin_addr.s_addr = src.raw_ip4();
    sa4.sin_port        = port;
    Set_Sockaddr_Len(&sa4, Meta_Case_Arg);
  } break;
  case AF_INET6: {
    memset(&sa6, 0, sizeof sa6);
    sa6.sin6_family = AF_INET6;
    sa6.sin6_addr   = src.raw_ip6();
    sa6.sin6_port   = port;
    Set_Sockaddr_Len(&sa6, Meta_Case_Arg);
  } break;
  }
  return *this;
}

std::string_view
IpEndpoint::family_name(uint16_t family)
{
  switch (family) {
  case AF_INET:
    return "ipv4"sv;
  case AF_INET6:
    return "ipv6"sv;
  case AF_UNIX:
    return "unix"sv;
  case AF_UNSPEC:
    return "unspec"sv;
  }
  return "unknown"sv;
}

#if 0
bool
IpAddr::parse(std::string_view const &text)
{
  IpEndpoint ip;
  int zret = ats_ip_pton(text, &ip.sa);
  this->assign(&ip.sa);
  return zret;
}

char *
IpAddr::toString(char *dest, size_t len) const
{
  IpEndpoint ip;
  ip.assign(*this);
  ats_ip_ntop(&ip, dest, len);
  return dest;
}

bool
operator==(IpAddr const &lhs, sockaddr const *rhs)
{
  bool zret = false;
  if (lhs._family == rhs->sa_family) {
    if (AF_INET == lhs._family) {
      zret = lhs._addr._ip4 == ats_ip4_addr_cast(rhs);
    } else if (AF_INET6 == lhs._family) {
      zret = 0 == memcmp(&lhs._addr._ip6, &ats_ip6_addr_cast(rhs), sizeof(in6_addr));
    } else { // map all non-IP to the same thing.
      zret = true;
    }
  } // else different families, not equal.
  return zret;
}

/** Compare two IP addresses.
    This is useful for IPv4, IPv6, and the unspecified address type.
    If the addresses are of different types they are ordered

    Non-IP < IPv4 < IPv6

     - all non-IP addresses are the same ( including @c AF_UNSPEC )
     - IPv4 addresses are compared numerically (host order)
     - IPv6 addresses are compared byte wise in network order (MSB to LSB)

    @return
      - -1 if @a lhs is less than @a rhs.
      - 0 if @a lhs is identical to @a rhs.
      - 1 if @a lhs is greater than @a rhs.
*/
int
IpAddr::cmp(self_type const &that) const
{
  int zret       = 0;
  uint16_t rtype = that._family;
  uint16_t ltype = _family;

  // We lump all non-IP addresses into a single equivalence class
  // that is less than an IP address. This includes AF_UNSPEC.
  if (AF_INET == ltype) {
    if (AF_INET == rtype) {
      in_addr_t la = ntohl(_addr._ip4);
      in_addr_t ra = ntohl(that._addr._ip4);
      if (la < ra) {
        zret = -1;
      } else if (la > ra) {
        zret = 1;
      } else {
        zret = 0;
      }
    } else if (AF_INET6 == rtype) { // IPv4 < IPv6
      zret = -1;
    } else { // IP > not IP
      zret = 1;
    }
  } else if (AF_INET6 == ltype) {
    if (AF_INET6 == rtype) {
      zret = memcmp(&_addr._ip6, &that._addr._ip6, TS_IP6_SIZE);
    } else {
      zret = 1; // IPv6 greater than any other type.
    }
  } else if (AF_INET == rtype || AF_INET6 == rtype) {
    // ltype is non-IP so it's less than either IP type.
    zret = -1;
  } else { // Both types are non-IP so they're equal.
    zret = 0;
  }

  return zret;
}

#endif

bool
IpAddr::parse(const std::string_view &str)
{
  bool bracket_p  = false;
  uint16_t family = AF_UNSPEC;
  ts::TextView src(str);
  _family = AF_UNSPEC; // invalidate until/unless success.
  src.trim_if(&isspace);
  if (*src == '[') {
    bracket_p = true;
    family    = AF_INET6;
    ++src;
  } else { // strip leading (hex) digits and see what the delimiter is.
    auto tmp = src;
    tmp.ltrim_if(&isxdigit);
    if (':' == *tmp) {
      family = AF_INET6;
    } else if ('.' == *tmp) {
      family = AF_INET;
    }
  }
  // Do the real parse now
  switch (family) {
  case AF_INET: {
    int n = 0; /// # of octets
    while (n < IP4_SIZE && !src.empty()) {
      ts::TextView token{src.take_prefix_at('.')};
      ts::TextView r;
      auto x = svtoi(token, &r, 10);
      if (r.size() == token.size()) {
        _addr._octet[n++] = x;
      } else {
        break;
      }
    }
    if (n == IP4_SIZE && src.empty()) {
      _family = AF_INET;
    }
  }
  case AF_INET6: {
    int n         = 0;
    int empty_idx = -1; // index of empty quad, -1 if not found yet.
    while (n < IP6_QUADS && !src.empty()) {
      ts::TextView token{src.take_prefix_at(':')};
      if (token.empty()) {
        if (empty_idx > 0 || (empty_idx == 0 && n > 1)) {
          // two empty slots OK iff it's the first two (e.g. "::1"), otherwise invalid.
          break;
        }
        empty_idx = n;
      } else {
        ts::TextView r;
        auto x = svtoi(token, &r, 16);
        if (r.size() == token.size()) {
          _addr._quad[n++] = htons(x);
        } else {
          break;
        }
      }
    }
    if (bracket_p) {
      src.ltrim_if(&isspace);
      if (']' != *src) {
        break;
      } else {
        ++src;
      }
    }
    // Handle empty quads - invalid if empty and still had a full set of quads
    if (empty_idx >= 0 && n < IP6_QUADS) {
      if (n <= empty_idx) {
        while (empty_idx < IP6_QUADS) {
          _addr._quad[empty_idx++] = 0;
        }
      } else {
        int k = 1;
        for (; n - k >= empty_idx; ++k) {
          _addr._quad[IP6_QUADS - k] = _addr._quad[n - k];
        }
        for (; IP6_QUADS - k >= empty_idx; ++k) {
          _addr._quad[IP6_QUADS - k] = 0;
          ++n; // track this so the validity check does the right thing.
        }
      }
    }
    if (n == IP6_QUADS && src.empty()) {
      _family = AF_INET6;
    }
  }
  }
  return this->is_valid();
}

} // namespace ts

bool
IpAddr::is_multicast() const
{
  return (AF_INET == _family && 0xe == (_addr._byte[0] >> 4)) || (AF_INET6 == _family && IN6_IS_ADDR_MULTICAST(&_addr._ip6));
}

BufferWriter &
bwformat(BufferWriter &w, BWFSpec const &spec, in_addr_t addr)
{
  uint8_t *ptr = reinterpret_cast<uint8_t *>(&addr);
  BWFSpec local_spec{spec}; // Format for address elements.
  bool align_p = false;

  if (spec._ext.size()) {
    if (spec._ext.front() == '=') {
      align_p          = true;
      local_spec._fill = '0';
    } else if (spec._ext.size() > 1 && spec._ext[1] == '=') {
      align_p          = true;
      local_spec._fill = spec._ext[0];
    }
  }

  if (align_p) {
    local_spec._min   = 3;
    local_spec._align = BWFSpec::Align::RIGHT;
  } else {
    local_spec._min = 0;
  }

  bwformat(w, local_spec, ptr[0]);
  w.write('.');
  bwformat(w, local_spec, ptr[1]);
  w.write('.');
  bwformat(w, local_spec, ptr[2]);
  w.write('.');
  bwformat(w, local_spec, ptr[3]);
  return w;
}

BufferWriter &
bwformat(BufferWriter &w, BWFSpec const &spec, in6_addr const &addr)
{
  using QUAD = uint16_t const;
  BWFSpec local_spec{spec}; // Format for address elements.
  uint8_t const *ptr   = addr.s6_addr;
  uint8_t const *limit = ptr + sizeof(addr.s6_addr);
  QUAD *lower          = nullptr; // the best zero range
  QUAD *upper          = nullptr;
  bool align_p         = false;

  if (spec._ext.size()) {
    if (spec._ext.front() == '=') {
      align_p          = true;
      local_spec._fill = '0';
    } else if (spec._ext.size() > 1 && spec._ext[1] == '=') {
      align_p          = true;
      local_spec._fill = spec._ext[0];
    }
  }

  if (align_p) {
    local_spec._min   = 4;
    local_spec._align = BWFSpec::Align::RIGHT;
  } else {
    local_spec._min = 0;
    // do 0 compression if there's no internal fill.
    for (QUAD *spot = reinterpret_cast<QUAD *>(ptr), *last = reinterpret_cast<QUAD *>(limit), *current = nullptr; spot < last;
         ++spot) {
      if (0 == *spot) {
        if (current) {
          // If there's no best, or this is better, remember it.
          if (!lower || (upper - lower < spot - current)) {
            lower = current;
            upper = spot;
          }
        } else {
          current = spot;
        }
      } else {
        current = nullptr;
      }
    }
  }

  if (!local_spec.has_numeric_type()) {
    local_spec._type = 'x';
  }

  for (; ptr < limit; ptr += 2) {
    if (reinterpret_cast<uint8_t const *>(lower) <= ptr && ptr <= reinterpret_cast<uint8_t const *>(upper)) {
      if (ptr == addr.s6_addr) {
        w.write(':'); // only if this is the first quad.
      }
      if (ptr == reinterpret_cast<uint8_t const *>(upper)) {
        w.write(':');
      }
    } else {
      uint16_t f = (ptr[0] << 8) + ptr[1];
      bwformat(w, local_spec, f);
      if (ptr != limit - 2) {
        w.write(':');
      }
    }
  }
  return w;
}

BufferWriter &
bwformat(BufferWriter &w, BWFSpec const &spec, IpAddr const &addr)
{
  BWFSpec local_spec{spec}; // Format for address elements and port.
  bool addr_p{true};
  bool family_p{false};

  if (spec._ext.size()) {
    if (spec._ext.front() == '=') {
      local_spec._ext.remove_prefix(1);
    } else if (spec._ext.size() > 1 && spec._ext[1] == '=') {
      local_spec._ext.remove_prefix(2);
    }
  }
  if (local_spec._ext.size()) {
    addr_p = false;
    for (char c : local_spec._ext) {
      switch (c) {
      case 'a':
      case 'A':
        addr_p = true;
        break;
      case 'f':
      case 'F':
        family_p = true;
        break;
      }
    }
  }

  if (addr_p) {
    if (addr.is_4()) {
      bwformat(w, spec, addr.raw_4());
    } else if (addr.is_6()) {
      bwformat(w, spec, addr.raw_6());
    } else {
      w.print("*Not IP address [{}]*", addr.family());
    }
  }

  if (family_p) {
    local_spec._min = 0;
    if (addr_p) {
      w.write(' ');
    }
    if (spec.has_numeric_type()) {
      bwformat(w, local_spec, static_cast<uintmax_t>(addr.family()));
    } else {
      bwformat(w, local_spec, addr.family());
    }
  }
  return w;
}

BufferWriter &
bwformat(BufferWriter &w, BWFSpec const &spec, sockaddr const *addr)
{
  BWFSpec local_spec{spec}; // Format for address elements and port.
  bool port_p{true};
  bool addr_p{true};
  bool family_p{false};
  bool local_numeric_fill_p{false};
  char local_numeric_fill_char{'0'};

  if (spec._type == 'p' || spec._type == 'P') {
    bwformat(w, spec, static_cast<void const *>(addr));
    return w;
  }

  if (spec._ext.size()) {
    if (spec._ext.front() == '=') {
      local_numeric_fill_p = true;
      local_spec._ext.remove_prefix(1);
    } else if (spec._ext.size() > 1 && spec._ext[1] == '=') {
      local_numeric_fill_p    = true;
      local_numeric_fill_char = spec._ext.front();
      local_spec._ext.remove_prefix(2);
    }
  }
  if (local_spec._ext.size()) {
    addr_p = port_p = false;
    for (char c : local_spec._ext) {
      switch (c) {
      case 'a':
      case 'A':
        addr_p = true;
        break;
      case 'p':
      case 'P':
        port_p = true;
        break;
      case 'f':
      case 'F':
        family_p = true;
        break;
      }
    }
  }

  if (addr_p) {
    bool bracket_p = false;
    switch (addr->sa_family) {
    case AF_INET:
      bwformat(w, spec, reinterpret_cast<sockaddr_in const *>(addr));
      break;
    case AF_INET6:
      if (port_p) {
        w.write('[');
        bracket_p = true; // take a note - put in the trailing bracket.
      }
      bwformat(w, spec, reinterpret_cast<sockaddr_in6 const *>(addr));
      break;
    default:
      w.print("*Not IP address [{}]*", addr->sa_family);
      break;
    }
    if (bracket_p)
      w.write(']');
    if (port_p)
      w.write(':');
  }
  if (port_p) {
    if (local_numeric_fill_p) {
      local_spec._min   = 5;
      local_spec._fill  = local_numeric_fill_char;
      local_spec._align = BWFSpec::Align::RIGHT;
    } else {
      local_spec._min = 0;
    }
    bwformat(w, local_spec, static_cast<uintmax_t>(IpEndpoint::port(addr)));
  }
  if (family_p) {
    local_spec._min = 0;
    if (addr_p || port_p)
      w.write(' ');
    if (spec.has_numeric_type()) {
      bwformat(w, local_spec, static_cast<uintmax_t>(addr->sa_family));
    } else {
      bwformat(w, local_spec, IpEndpoint::family_name(addr->sa_family));
    }
  }
  return w;
}

} // namespace ts
