/** @file
 *
 *  PROXY protocol definitions and parsers.
 *
 *  @section license License
 *
 *  Licensed to the Apache Software Foundation (ASF) under one
 *  or more contributor license agreements.  See the NOTICE file
 *  distributed with this work for additional information
 *  regarding copyright ownership.  The ASF licenses this file
 *  to you under the Apache License, Version 2.0 (the
 *  "License"); you may not use this file except in compliance
 *  with the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "ts/ink_assert.h"
#include "ProxyProtocol.h"
#include "I_NetVConnection.h"

bool
ssl_has_proxy_v1(NetVConnection *sslvc, char *buffer, int64_t *r)
{
  int64_t nl = 0;
  char *nlp  = nullptr;
  if (0 == memcmp(PROXY_V1_CONNECTION_PREFACE, buffer, PROXY_V1_CONNECTION_PREFACE_LEN)) {
    nlp = (char *)memchr(buffer, '\n', PROXY_V1_CONNECTION_HEADER_LEN_MAX);
    if (nlp) {
      nl = (int64_t)(nlp - buffer);
      Debug("ssl", "Consuming %" PRId64 " characters of the PROXY header from %p to %p", nl + 1, buffer, nlp);
      char local_buf[PROXY_V1_CONNECTION_HEADER_LEN_MAX + 1];
      memcpy(local_buf, buffer, nl + 1);
      memmove(buffer, buffer + nl + 1, nl + 1);
      *r -= nl + 1;
      if (*r <= 0) {
        *r = -EAGAIN;
      }
      return (proxy_protov1_parse(sslvc, local_buf));
    }
  }
  return false;
}

bool
http_has_proxy_v1(IOBufferReader *reader, NetVConnection *netvc)
{
  char buf[PROXY_V1_CONNECTION_HEADER_LEN_MAX + 1];
  ts::TextView tv;

  tv.set_view(buf, reader->memcpy(buf, sizeof(buf), 0));

  // Client must send at least 15 bytes to get a reasonable match.
  if (tv.size() < PROXY_V1_CONNECTION_HEADER_LEN_MIN) {
    return false;
  }

  if (0 != memcmp(PROXY_V1_CONNECTION_PREFACE, buf, PROXY_V1_CONNECTION_PREFACE_LEN)) {
    return false;
  }

  // Find the terminating LF, which should already be in the buffer.
  ts::TextView::size_type pos = tv.find('\n');
  if (pos == tv.npos) { // not found, it's not a proxy protocol header.
    return false;
  }
  reader->consume(pos+1); // clear out the header.

  // Now that we know we have a valid PROXY V1 preface, let's parse the
  // remainder of the header

  return proxy_protov1_parse(netvc, tv);
}

bool
proxy_protov1_parse(NetVConnection *netvc, ts::TextView hdr)
{
  static const ts::string_view PREFACE{PROXY_V1_CONNECTION_PREFACE, PROXY_V1_CONNECTION_PREFACE_LEN};
  IpEndpoint src_addr;
  IpEndpoint dst_addr;
  ts::TextView token, rest;
  in_port_t port;

  // All the cases are special and sequence, might as well unroll them.

  // The header should begin with the PROXY preface
  token = hdr.split_prefix_at(' ');
  if (0 == token.size() || token != PREFACE) {
    return false;
  }
  Debug("proxyprotocol_v1", "proto_has_proxy_v1: [%.*s] = PREFACE", static_cast<int>(token.size()), token.data());

  // The INET protocol family - TCP4, TCP6 or UNKNOWN
  token = hdr.split_prefix_at(' ');
  if (0 == token.size()) {
    return false;
  }
  Debug("proxyprotocol_v1", "proto_has_proxy_v1: [%.*s] = INET Family", static_cast<int>(token.size()), token.data());

  // Next up is the layer 3 source address
  // - 255.255.255.255 or ffff:f...f:ffff ffff:f...f:fff
  token = hdr.split_prefix_at(' ');
  if (0 == token.size()) {
    return false;
  }
  Debug("proxyprotocol_v1", "proto_has_proxy_v1: [%.*s] = Source Address", static_cast<int>(token.size()), token.data());
  if (0 != ats_ip_pton(token, &src_addr.sa)) {
    return false;
  }

  // Next is the layer3 destination address
  // - 255.255.255.255 or ffff:f...f:ffff ffff:f...f:fff
  token = hdr.split_prefix_at(' ');
  if (0 == token.size()) {
    return false;
  }
  Debug("proxyprotocol_v1", "proto_has_proxy_v1: [%.*s] = Destination Address", static_cast<int>(token.size()), token.data());
  if (0 != ats_ip_pton(token, &dst_addr.sa)) {
    return false;
  }

  // Next is the TCP source port represented as a decimal number in the range of [0..65535] inclusive.
  token = hdr.split_prefix_at(' ');
  if (0 == token.size()) {
    return false;
  }
  Debug("proxyprotocol_v1", "proto_has_proxy_v1: [%.*s] = Source Port", static_cast<int>(token.size()), token.data());
  if (0 == (port = ts::svtoi(token, &rest)) || port >= (1<<16) || rest.size()) {
    return false;
  }
  src_addr.port() = htons(port);
  
  // Next is the TCP destination port represented as a decimal number in the range of [0..65535] inclusive.
  // Final trailer is CR LF so split at CR.
  token = hdr.split_prefix_at('\r');
  if (0 == token.size()) {
    return false;
  }
  Debug("proxyprotocol_v1", "proto_has_proxy_v1: [%.*s] = Destination Port", static_cast<int>(token.size()), token.data());
  if (0 == (port = ts::svtoi(token, &rest)) || port >= (1<<16) || rest.size()) {
    return false;
  }
  dst_addr.port() = htons(port);
  // Is this check useful? This doesn't get called unless the LF was found.
  if (hdr.size() == 0 || '\n' != hdr.front()) {
    return false;
  }

  netvc->set_proxy_protocol_version(NetVConnection::PROXY_V1);

  return true;
}
