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

/**************************************************************************

  P_DNSConnection.h
  Description:
  struct DNSRequest
  **************************************************************************/

#ifndef __P_DNSCONNECTION_H__
#define __P_DNSCONNECTION_H__

#include <unordered_set>
#include "I_EventSystem.h"

using std::unordered_set;

//
// Connection
//
struct DNSHandler;
struct DNSRequestMap;

struct DNSRequest {
  /// Options for connecting.
  struct Options {
    typedef Options self; ///< Self reference type.

    /// Connection is done non-blocking.
    /// Default: @c true.
    bool _non_blocking_connect;
    /// Set socket to have non-blocking I/O.
    /// Default: @c true.
    bool _non_blocking_io;
    /// Use TCP if @c true, use UDP if @c false.
    /// Default: @c false.
    bool _use_tcp;
    /// Bind to a random port.
    /// Default: @c true.
    bool _bind_random_port;
    /// Bind to this local address when using IPv6.
    /// Default: unset, bind to IN6ADDR_ANY.
    sockaddr const *_local_ipv6;
    /// Bind to this local address when using IPv4.
    /// Default: unset, bind to INADDRY_ANY.
    sockaddr const *_local_ipv4;

    Options();

    self &setUseTcp(bool p);
    self &setNonBlockingConnect(bool p);
    self &setNonBlockingIo(bool p);
    self &setBindRandomPort(bool p);
    self &setLocalIpv6(sockaddr const *addr);
    self &setLocalIpv4(sockaddr const *addr);
  };

  int fd;
  LINK(DNSRequest, link);
  EventIO eio;
  DNSHandler *handler;
  DNSRequestMap *_map;
  ink_hrtime start_time;
  bool for_healthcheck;

  void init(DNSHandler *a_handler, DNSRequestMap *cmap, bool healthcheck = false);
  int open(sockaddr const *addr, Options const &opt = DEFAULT_OPTIONS);
  /*
                bool non_blocking_connect = NON_BLOCKING_CONNECT,
                bool use_tcp = CONNECT_WITH_TCP, bool non_blocking = NON_BLOCKING, bool bind_random_port = BIND_ANY_PORT);
  */
  int close();
  void trigger();

  virtual ~DNSRequest();
  DNSRequest();

  static Options const DEFAULT_OPTIONS;
};

inline DNSRequest::Options::Options()
  : _non_blocking_connect(true), _non_blocking_io(true), _use_tcp(false), _bind_random_port(true), _local_ipv6(0), _local_ipv4(0)
{
}

inline DNSRequest::Options &
DNSRequest::Options::setNonBlockingIo(bool p)
{
  _non_blocking_io = p;
  return *this;
}
inline DNSRequest::Options &
DNSRequest::Options::setNonBlockingConnect(bool p)
{
  _non_blocking_connect = p;
  return *this;
}
inline DNSRequest::Options &
DNSRequest::Options::setUseTcp(bool p)
{
  _use_tcp = p;
  return *this;
}
inline DNSRequest::Options &
DNSRequest::Options::setBindRandomPort(bool p)
{
  _bind_random_port = p;
  return *this;
}
inline DNSRequest::Options &
DNSRequest::Options::setLocalIpv4(sockaddr const *ip)
{
  _local_ipv4 = ip;
  return *this;
}
inline DNSRequest::Options &
DNSRequest::Options::setLocalIpv6(sockaddr const *ip)
{
  _local_ipv6 = ip;
  return *this;
}

class DNSRequestMap
{
 public:
  void initialize(sockaddr const *target, DNSRequest::Options &opt);
  void close();
  int sendRequest(const int qtype,
      const char *qname,
      char *query,
      const int len,
      bool hc,
      DNSRequest *&request);
  DNSRequest *getRequest(bool health_check = false);
  bool releaseRequest(DNSRequest *conn);
  void pruneStaleHealthCheckConnections();

  DNSHandler *handler;
  IpEndpoint m_target;
  DNSRequest::Options m_opt;

  unordered_set<DNSRequest *> m_requests;
  unordered_set<DNSRequest *> m_healthCheckRequests;
  int num;

  DNSRequestMap();
  DNSRequestMap(const DNSRequestMap &) = delete;
  DNSRequestMap &operator=(const DNSRequestMap &) = delete;

};

inline
DNSRequestMap::DNSRequestMap()
:
handler(nullptr)
{
  ats_ip_invalidate(&m_target);
}

#endif /*_P_DNSConnection_h*/
