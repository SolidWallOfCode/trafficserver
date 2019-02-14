/** @file

  HTTP configuration support.

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

#include <records/I_RecCore.h>
#include <records/I_RecHttp.h>
#include "tscore/ink_defs.h"
#include "tscore/TextBuffer.h"
#include <strings.h>
#include "tscore/ink_inet.h"
#include <string_view>
#include <unordered_set>
#include <tscore/IpMapConf.h>
#include <tscpp/util/TextView.h>
#include <tscore/BufferWriter.h>

using ts::TextView;
using namespace std::literals;

SessionProtocolNameRegistry globalSessionProtocolNameRegistry;

/* Protocol session well-known protocol names.
   These are also used for NPN setup.
*/

const char *const TS_ALPN_PROTOCOL_HTTP_0_9 = IP_PROTO_TAG_HTTP_0_9.data();
const char *const TS_ALPN_PROTOCOL_HTTP_1_0 = IP_PROTO_TAG_HTTP_1_0.data();
const char *const TS_ALPN_PROTOCOL_HTTP_1_1 = IP_PROTO_TAG_HTTP_1_1.data();
const char *const TS_ALPN_PROTOCOL_HTTP_2_0 = IP_PROTO_TAG_HTTP_2_0.data();

const char *const TS_ALPN_PROTOCOL_GROUP_HTTP  = "http";
const char *const TS_ALPN_PROTOCOL_GROUP_HTTP2 = "http2";

const char *const TS_PROTO_TAG_HTTP_1_0 = TS_ALPN_PROTOCOL_HTTP_1_0;
const char *const TS_PROTO_TAG_HTTP_1_1 = TS_ALPN_PROTOCOL_HTTP_1_1;
const char *const TS_PROTO_TAG_HTTP_2_0 = TS_ALPN_PROTOCOL_HTTP_2_0;
const char *const TS_PROTO_TAG_TLS_1_3  = IP_PROTO_TAG_TLS_1_3.data();
const char *const TS_PROTO_TAG_TLS_1_2  = IP_PROTO_TAG_TLS_1_2.data();
const char *const TS_PROTO_TAG_TLS_1_1  = IP_PROTO_TAG_TLS_1_1.data();
const char *const TS_PROTO_TAG_TLS_1_0  = IP_PROTO_TAG_TLS_1_0.data();
const char *const TS_PROTO_TAG_TCP      = IP_PROTO_TAG_TCP.data();
const char *const TS_PROTO_TAG_UDP      = IP_PROTO_TAG_UDP.data();
const char *const TS_PROTO_TAG_IPV4     = IP_PROTO_TAG_IPV4.data();
const char *const TS_PROTO_TAG_IPV6     = IP_PROTO_TAG_IPV6.data();

std::unordered_set<std::string_view> TSProtoTags;

// Precomputed indices for ease of use.
int TS_ALPN_PROTOCOL_INDEX_HTTP_0_9 = SessionProtocolNameRegistry::INVALID;
int TS_ALPN_PROTOCOL_INDEX_HTTP_1_0 = SessionProtocolNameRegistry::INVALID;
int TS_ALPN_PROTOCOL_INDEX_HTTP_1_1 = SessionProtocolNameRegistry::INVALID;
int TS_ALPN_PROTOCOL_INDEX_HTTP_2_0 = SessionProtocolNameRegistry::INVALID;

// Predefined protocol sets for ease of use.
SessionProtocolSet HTTP_PROTOCOL_SET;
SessionProtocolSet HTTP2_PROTOCOL_SET;
SessionProtocolSet DEFAULT_NON_TLS_SESSION_PROTOCOL_SET;
SessionProtocolSet DEFAULT_TLS_SESSION_PROTOCOL_SET;

namespace
{
template <typename... Args>
void
BW_Warning(TextView format, Args &&... args)
{
  std::string text;
  bwprint(text, format, args...);
  Warning("%s", text.c_str());
}

bool
Validate_Prefix(TextView &token, TextView const &prefix)
{
  if (prefix.isNoCasePrefixOf(token)) {
    token.remove_prefix(prefix.size());
    if ('-' == *token || '=' == *token) {
      ++token;
    }
    return true;
  }
  return false;
}

} // namespace

static bool
mptcp_supported()
{
  ats_scoped_fd fd(::open("/proc/sys/net/mptcp/mptcp_enabled", O_RDONLY));
  int value = 0;

  if (fd) {
    TextBuffer buffer(16);

    buffer.slurp(fd.get());
    value = atoi(buffer.bufPtr());
  }

  return value != 0;
}

void
RecHttpLoadIp(const char *value_name, IpAddr &ip4, IpAddr &ip6)
{
  std::string value;
  ip4.invalidate();
  ip6.invalidate();
  if (REC_ERR_OKAY == RecGetRecordString(std::string_view{value_name}, value)) {
    TextView text{value};
    while (text) {
      auto host = text.take_prefix_at(", "sv).trim_if(&isspace);
      if (host.empty()) {
        continue;
      }

      IpEndpoint tmp4, tmp6;
      // For backwards compatibility we need to support the use of host names
      // for the address to bind.
      if (0 == ats_ip_getbestaddrinfo(host, &tmp4, &tmp6)) {
        if (ats_is_ip4(&tmp4)) {
          if (!ip4.isValid()) {
            ip4 = tmp4;
          } else {
            BW_Warning("'{}' specifies more than one IPv4 address, ignoring '{}'.", value_name, host);
          }
        }
        if (ats_is_ip6(&tmp6)) {
          if (!ip6.isValid()) {
            ip6 = tmp6;
          } else {
            BW_Warning("'{}' specifies more than one IPv6 address, ignoring '{}'", value_name, host);
          }
        }
      } else {
        BW_Warning("'{}' has an value '{}' that is not recognized as an IP address, ignored.", value_name, host);
      }
    }
  }
}

void
RecHttpLoadIpMap(const char *value_name, IpMap &ipmap)
{
  std::string value;
  IpAddr laddr;
  IpAddr raddr;
  void *payload = nullptr;

  if (REC_ERR_OKAY == RecGetRecordString(std::string_view{value_name}, value)) {
    Debug("config", "RecHttpLoadIpMap: parsing the name [%s] and value [%s] to an IpMap", value_name, value.c_str());
    TextView text{value};
    while (text) {
      auto token{text.take_prefix_at(", "sv)};
      if (token.trim_if(&isspace).empty()) {
        continue;
      }
      Debug("config", "RecHttpLoadIpMap: marking the value [%.*s] to an IpMap entry", static_cast<int>(token.size()), token.data());
      if (0 == ats_ip_range_parse(token, laddr, raddr)) {
        ipmap.fill(laddr, raddr, payload);
      }
    }
  }
  Debug("config", "RecHttpLoadIpMap: parsed %zu IpMap entries", ipmap.count());
}

// "_PREFIX" means the option contains additional data.
// Each has a corresponding _LEN value that is the length of the option text.
// Options without _PREFIX are just flags with no additional data.

const char *const HttpProxyPort::OPT_IPV6                    = "ipv6";
const char *const HttpProxyPort::OPT_IPV4                    = "ipv4";
const char *const HttpProxyPort::OPT_TRANSPARENT_INBOUND     = "tr-in";
const char *const HttpProxyPort::OPT_TRANSPARENT_OUTBOUND    = "tr-out";
const char *const HttpProxyPort::OPT_TRANSPARENT_FULL        = "tr-full";
const char *const HttpProxyPort::OPT_TRANSPARENT_PASSTHROUGH = "tr-pass";
const char *const HttpProxyPort::OPT_SSL                     = "ssl";
const char *const HttpProxyPort::OPT_PROXY_PROTO             = "pp";
const char *const HttpProxyPort::OPT_PLUGIN                  = "plugin";
const char *const HttpProxyPort::OPT_BLIND_TUNNEL            = "blind";
const char *const HttpProxyPort::OPT_MPTCP                   = "mptcp";

namespace
{
// Solaris work around. On that OS the compiler will not let me use an
// instantiated instance of Vec<self> inside the class, even if
// static. So we have to declare it elsewhere and then import via
// reference. Might be a problem with Vec<> creating a fixed array
// rather than allocating on first use (compared to std::vector<>).
HttpProxyPort::Group GLOBAL_DATA;
} // namespace
HttpProxyPort::Group &HttpProxyPort::m_global = GLOBAL_DATA;

HttpProxyPort::HttpProxyPort()
  : m_fd(ts::NO_FD),
    m_type(TRANSPORT_DEFAULT),
    m_port(0),
    m_family(AF_INET),
    m_proxy_protocol(false),
    m_inbound_transparent_p(false),
    m_outbound_transparent_p(false),
    m_transparent_passthrough(false),
    m_mptcp(false)
{
  memcpy(m_host_res_preference, host_res_default_preference_order, sizeof(m_host_res_preference));
}

bool
HttpProxyPort::hasSSL(Group const &ports)
{
  return std::any_of(ports.begin(), ports.end(), [](HttpProxyPort const &port) { return port.isSSL(); });
}

const HttpProxyPort *
HttpProxyPort::findHttp(Group const &ports, uint16_t family)
{
  bool check_family_p   = ats_is_ip(family);
  const self_type *zret = nullptr;
  for (int i = 0, n = ports.size(); i < n && !zret; ++i) {
    const self_type &p = ports[i];
    if (p.m_port &&                               // has a valid port
        TRANSPORT_DEFAULT == p.m_type &&          // is normal HTTP
        (!check_family_p || p.m_family == family) // right address family
    ) {
      zret = &p;
    };
  }
  return zret;
}

const char *
HttpProxyPort::checkPrefix(const char *src, char const *prefix, size_t prefix_len)
{
  const char *zret = nullptr;
  if (0 == strncasecmp(prefix, src, prefix_len)) {
    src += prefix_len;
    if ('-' == *src || '=' == *src) {
      ++src; // permit optional '-' or '='
    }
    zret = src;
  }
  return zret;
}

bool
HttpProxyPort::loadConfig(std::vector<self_type> &entries)
{
  std::string text;

  if (REC_ERR_OKAY == RecGetRecordString(PORTS_CONFIG_NAME, text)) {
    self_type::loadValue(entries, text);
  }

  return 0 < entries.size();
}

bool
HttpProxyPort::loadDefaultIfEmpty(Group &ports)
{
  if (0 == ports.size()) {
    self_type::loadValue(ports, DEFAULT_VALUE);
  }

  return 0 < ports.size();
}

bool
HttpProxyPort::loadValue(std::vector<self_type> &ports, TextView text)
{
  unsigned old_port_length = ports.size(); // remember this.
  while (text) {
    auto token = text.take_prefix_at(", ");
    HttpProxyPort entry;
    if (entry.processOptions(token)) {
      ports.push_back(entry);
    } else {
      Warning("No valid definition was found in proxy port configuration element '%.*s'", static_cast<int>(text.size()),
              text.data());
    }
  }
  return ports.size() > old_port_length; // we added at least one port.
}

bool
HttpProxyPort::processOptions(TextView opts)
{
  bool zret           = false; // found a port?
  bool af_set_p       = false; // AF explicitly specified?
  bool host_res_set_p = false; // Host resolution order set explicitly?
  bool sp_set_p       = false; // Session protocol set explicitly?
  IpAddr ip;                   // temp for loading IP addresses.

  auto text{opts};
  while (text) {
    TextView::size_type offset = 0;
    bool bracket_p             = false;
    while (offset < text.size()) {
      offset = text.find_first_of("[:]"sv, offset); // next character of interest.
      if (TextView::npos == offset) {
        if (bracket_p) {
          BW_Warning("Invalid port descriptor '{}' - left bracket without closing right bracket", opts);
          return zret;
        }
        break;
      } else if ('[' == text[offset]) {
        if (bracket_p) {
          BW_Warning("Invalid port descriptor '{}' - left bracket after left bracket without right bracket", opts);
          return zret;
        } else {
          bracket_p = true;
          ++offset;
        }
      } else if (']' == text[offset]) {
        if (bracket_p) {
          bracket_p = false;
          ++offset;
        } else {
          BW_Warning("Invalid port descriptor {}' - right bracket without opent left bracket", opts);
          return zret;
        }
      } else if (':' == text[offset]) {
        if (bracket_p) {
          ++offset;
        } else {
          break;
        }
      }
    }
    TextView token{text.take_prefix_at(offset)};
    if (token.empty()) {
      continue;
    }

    TextView value{token}; // updated by option prefix check, @a token remains the parsed token.

    if (isdigit(token[0])) { // leading digit -> port value
      TextView port_text{token};
      auto port = ts::svto_radix<10>(port_text);
      if (!port_text.empty()) {
        // really, this shouldn't happen, since we checked for a leading digit.
        BW_Warning("Mangled port value '{}' in port configuration '{}'", token, opts);
      } else if (port <= 0 || 65536 <= port) {
        BW_Warning("Port value '{}' out of range (1..65535) in port configuration '{}'", token, opts);
      } else {
        m_port = static_cast<in_port_t>(port);
        zret   = true;
      }
    } else if (Validate_Prefix(value, OPT_FD_PREFIX)) {
      int fd = ts::svto_radix<10>(value);
      if (!value.empty()) {
        BW_Warning("Mangled file descriptor value '{}' in port descriptor '{}'", token, opts);
      } else {
        m_fd = fd;
        zret = true;
      }
    } else if (Validate_Prefix(value, OPT_INBOUND_IP_PREFIX)) {
      if (0 == ip.load(value)) {
        m_inbound_ip = ip;
      } else {
        BW_Warning("Invalid IP address value '{}' in port descriptor '{}'", token, opts);
      }
    } else if (Validate_Prefix(value, OPT_OUTBOUND_IP_PREFIX)) {
      if (0 == ip.load(value)) {
        this->outboundIp(ip.family()) = ip;
      } else {
        BW_Warning("Invalid IP address value '{}' in port descriptor '{}'", token, opts);
      }
    } else if (0 == strcasecmp(OPT_COMPRESSED, value)) {
      m_type = TRANSPORT_COMPRESSED;
    } else if (0 == strcasecmp(OPT_BLIND_TUNNEL, value)) {
      m_type = TRANSPORT_BLIND_TUNNEL;
    } else if (0 == strcasecmp(OPT_IPV6, value)) {
      m_family = AF_INET6;
      af_set_p = true;
    } else if (0 == strcasecmp(OPT_IPV4, token)) {
      m_family = AF_INET;
      af_set_p = true;
    } else if (0 == strcasecmp(OPT_SSL, token)) {
      m_type = TRANSPORT_SSL;
    } else if (0 == strcasecmp(OPT_PLUGIN, token)) {
      m_type = TRANSPORT_PLUGIN;
    } else if (0 == strcasecmp(OPT_PROXY_PROTO, token)) {
      m_proxy_protocol = true;
    } else if (0 == strcasecmp(OPT_TRANSPARENT_INBOUND, token)) {
#if TS_USE_TPROXY
      m_inbound_transparent_p = true;
#else
      BW_Warning("Transparency requested [{}] in port descriptor '{}' but TPROXY was not configured.", token, opts);
#endif
    } else if (0 == strcasecmp(OPT_TRANSPARENT_OUTBOUND, token)) {
#if TS_USE_TPROXY
      m_outbound_transparent_p = true;
#else
      BW_Warning("Transparency requested [{}] in port descriptor '{}' but TPROXY was not configured.", token, opts);
#endif
    } else if (0 == strcasecmp(OPT_TRANSPARENT_FULL, token)) {
#if TS_USE_TPROXY
      m_inbound_transparent_p  = true;
      m_outbound_transparent_p = true;
#else
      BW_Warning("Transparency requested [{}] in port descriptor '{}' but TPROXY was not configured.", token, opts);
#endif
    } else if (0 == strcasecmp(OPT_TRANSPARENT_PASSTHROUGH, token)) {
#if TS_USE_TPROXY
      m_transparent_passthrough = true;
#else
      BW_Warning("Transparent pass-through requested [{}] in port descriptor '{}' but TPROXY was not configured.", token, opts);
#endif
    } else if (0 == strcasecmp(OPT_MPTCP, token)) {
      if (mptcp_supported()) {
        m_mptcp = true;
      } else {
        BW_Warning("Multipath TCP requested [{}] in port descriptor '{}' but it is not supported by this host.", token, opts);
      }
    } else if (Validate_Prefix(value, OPT_HOST_RES_PREFIX)) {
      this->processFamilyPreference(value);
      host_res_set_p = true;
    } else if (Validate_Prefix(value, OPT_PROTO_PREFIX)) {
      this->processSessionProtocolPreference(value);
      sp_set_p = true;
    } else {
      BW_Warning("Invalid option '{}' in proxy port descriptor '{}'", token, opts);
    }
  }

  bool in_ip_set_p = m_inbound_ip.isValid();

  if (af_set_p) {
    if (in_ip_set_p && m_family != m_inbound_ip.family()) {
      BW_Warning(
        "Invalid port descriptor '{}' - the inbound address family [{:s:f}] is not the same type as the explicit family value "
        "[{}]",
        opts, m_inbound_ip, ats_ip_family_name(m_family));
      zret = false;
    }
  } else if (in_ip_set_p) {
    m_family = m_inbound_ip.family(); // set according to address.
  }

  // If the port is outbound transparent only CLIENT host resolution is possible.
  if (m_outbound_transparent_p) {
    if (host_res_set_p &&
        (m_host_res_preference[0] != HOST_RES_PREFER_CLIENT || m_host_res_preference[1] != HOST_RES_PREFER_NONE)) {
      BW_Warning("Outbound transparent port '{}' requires the IP address resolution ordering '{},{}'. "
                 "This is set automatically and does not need to be set explicitly.",
                 opts, HOST_RES_PREFERENCE_STRING[HOST_RES_PREFER_CLIENT], HOST_RES_PREFERENCE_STRING[HOST_RES_PREFER_NONE]);
    }
    m_host_res_preference[0] = HOST_RES_PREFER_CLIENT;
    m_host_res_preference[1] = HOST_RES_PREFER_NONE;
  }

  // Transparent pass-through requires tr-in
  if (m_transparent_passthrough && !m_inbound_transparent_p) {
    BW_Warning("Port descriptor '{}' has transparent pass-through enabled without inbound transparency, this will be ignored.",
               opts);
    m_transparent_passthrough = false;
  }

  // Set the default session protocols.
  if (!sp_set_p) {
    m_session_protocol_preference = this->isSSL() ? DEFAULT_TLS_SESSION_PROTOCOL_SET : DEFAULT_NON_TLS_SESSION_PROTOCOL_SET;
  }

  return zret;
}

void
HttpProxyPort::processFamilyPreference(TextView const &value)
{
  parse_host_res_preference(value, m_host_res_preference);
}

void
HttpProxyPort::processSessionProtocolPreference(TextView const &value)
{
  m_session_protocol_preference.markAllOut();
  globalSessionProtocolNameRegistry.markIn(value, m_session_protocol_preference);
}

void
SessionProtocolNameRegistry::markIn(TextView value, SessionProtocolSet &sp_set)
{
  while (value) {
    auto token = value.take_prefix_at(" ;|,:"sv);
    /// Check special cases
    if (0 == strcasecmp(token, TS_ALPN_PROTOCOL_GROUP_HTTP)) {
      sp_set.markIn(HTTP_PROTOCOL_SET);
    } else if (0 == strcasecmp(token, TS_ALPN_PROTOCOL_GROUP_HTTP2)) {
      sp_set.markIn(HTTP2_PROTOCOL_SET);
    } else { // user defined - register and mark.
      int idx = globalSessionProtocolNameRegistry.toIndex(token);
      sp_set.markIn(idx);
    }
  }
}

ts::BufferWriter &
HttpProxyPort::print(ts::BufferWriter &w) const
{
  bool need_colon_p = false;

  if (m_inbound_ip.isValid()) {
    w.print("{}=[{}]", OPT_INBOUND_IP_PREFIX, m_inbound_ip);
    need_colon_p = true;
  }

  if (m_outbound_ip4.isValid()) {
    if (need_colon_p) {
      w.write(':');
    }
    w.print("{}={}", OPT_OUTBOUND_IP_PREFIX, m_outbound_ip4);
    need_colon_p = true;
  }

  if (m_outbound_ip6.isValid()) {
    if (need_colon_p) {
      w.write(':');
    }
    w.print("{}=[{}]", OPT_OUTBOUND_IP_PREFIX, m_outbound_ip6);
    need_colon_p = true;
  }

  if (0 != m_port) {
    if (need_colon_p) {
      w.write(':');
    }
    w.print("{}", m_port);
    need_colon_p = true;
  }

  if (ts::NO_FD != m_fd) {
    if (need_colon_p) {
      w.write(':');
    }
    w.print("{}={}", OPT_FD_PREFIX, m_fd);
  }

  // After this point, all of these options require other options which we've already
  // generated so all of them need a leading colon and we can stop checking for that.

  if (AF_INET6 == m_family) {
    w.write(':').write(OPT_IPV6);
  }

  if (TRANSPORT_BLIND_TUNNEL == m_type) {
    w.write(':').write(OPT_BLIND_TUNNEL);
  } else if (TRANSPORT_SSL == m_type) {
    w.write(':').write(OPT_SSL);
  } else if (TRANSPORT_PLUGIN == m_type) {
    w.write(':').write(OPT_PLUGIN);
  } else if (TRANSPORT_COMPRESSED == m_type) {
    w.write(':').write(OPT_COMPRESSED);
  }

  if (m_proxy_protocol) {
    w.write(':').write(OPT_PROXY_PROTO);
  }

  if (m_outbound_transparent_p && m_inbound_transparent_p) {
    w.write(':').write(OPT_TRANSPARENT_FULL);
  } else if (m_inbound_transparent_p) {
    w.write(':').write(OPT_TRANSPARENT_INBOUND);
  } else if (m_outbound_transparent_p) {
    w.write(':').write(OPT_TRANSPARENT_OUTBOUND);
  }

  if (m_mptcp) {
    w.write(':').write(OPT_MPTCP);
  }

  if (m_transparent_passthrough) {
    w.write(':').write(OPT_TRANSPARENT_PASSTHROUGH);
  }

  /* Don't print the IP resolution preferences if the port is outbound
   * transparent (which means the preference order is forced) or if
   * the order is the same as the default.
   */
  if (!m_outbound_transparent_p &&
      0 != memcmp(m_host_res_preference, host_res_default_preference_order, sizeof(m_host_res_preference))) {
    w.print(":{}={}", OPT_HOST_RES_PREFIX, m_host_res_preference);
  }

  // session protocol options - look for condensed options first
  // first two cases are the defaults so if those match, print nothing.
  SessionProtocolSet sp_set = m_session_protocol_preference; // need to modify so copy.
  need_colon_p              = true;                          // for listing case, turned off if we do a special case.
  if (sp_set == DEFAULT_NON_TLS_SESSION_PROTOCOL_SET && !this->isSSL()) {
    sp_set.markOut(DEFAULT_NON_TLS_SESSION_PROTOCOL_SET);
  } else if (sp_set == DEFAULT_TLS_SESSION_PROTOCOL_SET && this->isSSL()) {
    sp_set.markOut(DEFAULT_TLS_SESSION_PROTOCOL_SET);
  }

  // pull out groups.
  if (sp_set.contains(HTTP_PROTOCOL_SET)) {
    w.print(":{}={}", OPT_PROTO_PREFIX, TS_ALPN_PROTOCOL_GROUP_HTTP);
    sp_set.markOut(HTTP_PROTOCOL_SET);
    need_colon_p = false;
  }
  if (sp_set.contains(HTTP2_PROTOCOL_SET)) {
    if (need_colon_p) {
      w.print(":{}=", OPT_PROTO_PREFIX);
    } else {
      w.write(';');
    }
    w.write(TS_ALPN_PROTOCOL_GROUP_HTTP2);
    sp_set.markOut(HTTP2_PROTOCOL_SET);
    need_colon_p = false;
  }
  // now enumerate what's left.
  if (!sp_set.isEmpty()) {
    if (need_colon_p) {
      w.print(":{}=", OPT_PROTO_PREFIX);
    }
    bool sep_p = !need_colon_p;
    for (unsigned k = 0; k < SessionProtocolSet::MAX; ++k) {
      if (sp_set.contains(k)) {
        if (sep_p) {
          w.write(';');
        }
        sep_p = true;
        w.print("{}", globalSessionProtocolNameRegistry.nameFor(k));
      }
    }
  }

  return w;
}

void
ts_host_res_global_init()
{
  std::string value;
  // Global configuration values.
  memcpy(host_res_default_preference_order, HOST_RES_DEFAULT_PREFERENCE_ORDER, sizeof(host_res_default_preference_order));

  if (REC_ERR_OKAY == RecGetRecordString("proxy.config.hostdb.ip_resolve", value)) {
    parse_host_res_preference(value, host_res_default_preference_order);
  }
}

// Whatever executable uses librecords must call this.
void
ts_session_protocol_well_known_name_indices_init()
{
  // register all the well known protocols and get the indices set.
  TS_ALPN_PROTOCOL_INDEX_HTTP_0_9 = globalSessionProtocolNameRegistry.toIndexConst(std::string_view{TS_ALPN_PROTOCOL_HTTP_0_9});
  TS_ALPN_PROTOCOL_INDEX_HTTP_1_0 = globalSessionProtocolNameRegistry.toIndexConst(std::string_view{TS_ALPN_PROTOCOL_HTTP_1_0});
  TS_ALPN_PROTOCOL_INDEX_HTTP_1_1 = globalSessionProtocolNameRegistry.toIndexConst(std::string_view{TS_ALPN_PROTOCOL_HTTP_1_1});
  TS_ALPN_PROTOCOL_INDEX_HTTP_2_0 = globalSessionProtocolNameRegistry.toIndexConst(std::string_view{TS_ALPN_PROTOCOL_HTTP_2_0});

  // Now do the predefined protocol sets.
  HTTP_PROTOCOL_SET.markIn(TS_ALPN_PROTOCOL_INDEX_HTTP_0_9);
  HTTP_PROTOCOL_SET.markIn(TS_ALPN_PROTOCOL_INDEX_HTTP_1_0);
  HTTP_PROTOCOL_SET.markIn(TS_ALPN_PROTOCOL_INDEX_HTTP_1_1);
  HTTP2_PROTOCOL_SET.markIn(TS_ALPN_PROTOCOL_INDEX_HTTP_2_0);

  DEFAULT_TLS_SESSION_PROTOCOL_SET.markAllIn();

  DEFAULT_NON_TLS_SESSION_PROTOCOL_SET = HTTP_PROTOCOL_SET;

  TSProtoTags.insert(TS_PROTO_TAG_HTTP_1_0);
  TSProtoTags.insert(TS_PROTO_TAG_HTTP_1_1);
  TSProtoTags.insert(TS_PROTO_TAG_HTTP_2_0);
  TSProtoTags.insert(TS_PROTO_TAG_TLS_1_3);
  TSProtoTags.insert(TS_PROTO_TAG_TLS_1_2);
  TSProtoTags.insert(TS_PROTO_TAG_TLS_1_1);
  TSProtoTags.insert(TS_PROTO_TAG_TLS_1_0);
  TSProtoTags.insert(TS_PROTO_TAG_TCP);
  TSProtoTags.insert(TS_PROTO_TAG_UDP);
  TSProtoTags.insert(TS_PROTO_TAG_IPV4);
  TSProtoTags.insert(TS_PROTO_TAG_IPV6);
}

const char *
RecNormalizeProtoTag(const char *tag)
{
  auto findResult = TSProtoTags.find(tag);
  return findResult == TSProtoTags.end() ? nullptr : findResult->data();
}

int
SessionProtocolNameRegistry::toIndex(ts::TextView name)
{
  int zret = this->indexFor(name);
  if (INVALID == zret) {
    if (m_n < MAX) {
      // Localize the name by copying it in to the arena.
      auto text = m_arena.alloc(name.size() + 1);
      memcpy(text.data(), name.data(), name.size());
      text.end()[-1] = '\0';
      m_names[m_n]   = text.view();
      zret           = m_n++;
    } else {
      ink_release_assert(!"Session protocol name registry overflow");
    }
  }
  return zret;
}

int
SessionProtocolNameRegistry::toIndexConst(TextView name)
{
  int zret = this->indexFor(name);
  if (INVALID == zret) {
    if (m_n < MAX) {
      m_names[m_n] = name;
      zret         = m_n++;
    } else {
      ink_release_assert(!"Session protocol name registry overflow");
    }
  }
  return zret;
}

int
SessionProtocolNameRegistry::indexFor(TextView name) const
{
  auto spot = std::find(m_names.begin(), m_names.begin() + m_n, name);
  if (spot != m_names.end()) {
    return static_cast<int>(spot - m_names.begin());
  }
  return INVALID;
}

ts::TextView
SessionProtocolNameRegistry::nameFor(int idx) const
{
  return 0 <= idx && idx < m_n ? m_names[idx] : TextView{};
}
