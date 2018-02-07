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
  char *end;
  ptrdiff_t nbytes;

  end    = reader->memcpy(buf, sizeof(buf), 0 /* offset */);
  nbytes = end - buf;

  // Client must send at least 15 bytes to get a reasonable match.
  if (nbytes < (long)PROXY_V1_CONNECTION_HEADER_LEN_MIN) {
    return false;
  }

  if (0 != memcmp(PROXY_V1_CONNECTION_PREFACE, buf, PROXY_V1_CONNECTION_PREFACE_LEN)) {
    return false;
  }

  int64_t nl;
  nl = reader->memchr('\n', strlen(buf), 0);

  char local_buf[PROXY_V1_CONNECTION_HEADER_LEN_MAX + 1];
  reader->read(local_buf, nl + 1);

  // Now that we know we have a valid PROXY V1 preface, let's parse the
  // remainder of the header

  return (proxy_protov1_parse(netvc, local_buf));
}

bool
proxy_protov1_parse(NetVConnection *netvc, char *buf)
{
  std::string src_addr_port;
  std::string dst_addr_port;

  char *pch;
  int cnt = 0;
  pch     = strtok(buf, " \n\r");
  while (pch != NULL) {
    switch (cnt) {
    // The header should begin with the PROXY preface
    case 0:
      Debug("proxyprotocol_v1", "proto_has_proxy_v1: token[%d]:[%s] = PREFACE", cnt, pch);
      break;

    // After the PROXY preface, exactly one space followed by the INET protocol family
    // - TCP4, TCP6 or UNKNOWN
    case 1:
      Debug("proxyprotocol_v1", "proto_has_proxy_v1: token[%d]:[%s] = INET Protocol", cnt, pch);
      break;

    // Next up is exactly one space and the layer 3 source address
    // - 255.255.255.255 or ffff:f...f:ffff ffff:f...f:fff
    case 2:
      Debug("proxyprotocol_v1", "proto_has_proxy_v1: token[%d]:[%s] = Source Address", cnt, pch);
      src_addr_port.assign(pch);
      break;

    // Next is exactly one space followed by the layer3 destination address
    // - 255.255.255.255 or ffff:f...f:ffff ffff:f...f:fff
    case 3:
      Debug("proxyprotocol_v1", "proto_has_proxy_v1: token[%d]:[%s] = Destination Address", cnt, pch);
      dst_addr_port.assign(pch);
      break;

    // Next is exactly one space followed by TCP source port represented as a
    //   decimal number in the range of [0..65535] inclusive.
    case 4:
      Debug("proxyprotocol_v1", "proto_has_proxy_v1: token[%d]:[%s] = Source Port", cnt, pch);
      src_addr_port = src_addr_port + ":" + pch;
      netvc->set_proxy_protocol_src_addr(ts::string_view(src_addr_port));
      break;

    // Next is exactly one space followed by TCP destination port represented as a
    //   decimal number in the range of [0..65535] inclusive.
    case 5:
      Debug("proxyprotocol_v1", "proto_has_proxy_v1: token[%d]:[%s] = Destination Port", cnt, pch);
      dst_addr_port = dst_addr_port + ":" + pch;
      netvc->set_proxy_protocol_dst_addr(ts::string_view(dst_addr_port));
      break;
    }
    // if we have our all of our fields, set version as a flag, we are done here
    //  otherwise increment our field counter and tok another field
    //  cnt shoud never get greater than 5, but just in case...
    if (cnt >= 5) {
      netvc->set_proxy_protocol_version(NetVConnection::PROXY_V1);
      return true;
    } else {
      ++cnt;
      pch = strtok(NULL, " \n\r");
    }
  }
  // If we failed to parse all of the fields, we have incomplete data.  The
  // spec does not allow for incomplete data, so don't se the flag so the data
  // is deemed not valid.
  return false;
}
