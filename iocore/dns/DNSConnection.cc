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
  Connections

  Commonality across all platforms -- move out as required.

**************************************************************************/
#include "P_DNS.h"
#include "P_DNSConnection.h"
#include "P_DNSProcessor.h"

#define SET_TCP_NO_DELAY
#define SET_NO_LINGER
// set in the OS
// #define RECV_BUF_SIZE            (1024*64)
// #define SEND_BUF_SIZE            (1024*64)
#define FIRST_RANDOM_PORT (16000)
#define LAST_RANDOM_PORT (60000)

#define ROUNDUP(x, y) ((((x) + ((y)-1)) / (y)) * (y))

ClassAllocator<DNSRequest> dnsRequestAllocator("dnsRequestAllocator");

DNSRequest::Options const DNSRequest::DEFAULT_OPTIONS;

//
// Functions
//

DNSRequest::DNSRequest()
  : fd(NO_FD),
    handler(nullptr),
    _map(nullptr),
    for_healthcheck(false)
{
}

DNSRequest::~DNSRequest()
{
  close();
}

void
DNSRequest::init(DNSHandler *a_handler, DNSRequestMap *cmap, bool healthcheck)
{
  handler = a_handler;
  _map = cmap;
  start_time = Thread::get_hrtime();
  for_healthcheck = healthcheck;
}

int
DNSRequest::close()
{
  eio.stop();
  handler = nullptr;
  _map = nullptr;
  // don't close any of the standards
  if (fd != NO_FD) {
    int fd_save = fd;
    fd          = NO_FD;
    return socketManager.close(fd_save);
  } else {
    fd = NO_FD;
    return -EBADF;
  }
}

void
DNSRequest::trigger()
{
  handler->triggered.enqueue(this);

  // Since the periodic check is removed, we need to call
  // this when it's triggered by EVENTIO_DNS_CONNECTION.
  // The handler should be pionting to DNSHandler::mainEvent.
  // We can schedule an immediate event or call the handler
  // directly, and since both arguments are not being used
  // passing in 0 and nullptr will do the job.
  handler->handleEvent(0, nullptr);
}

int
DNSRequest::open(sockaddr const *addr, const Options &opt)
{
  ink_assert(ats_is_ip(addr));
  PollDescriptor *pd = get_PollDescriptor(dnsProcessor.thread);

  int res = 0;
  short Proto;
  uint8_t af = addr->sa_family;
  IpEndpoint bind_addr;
  size_t bind_size = 0;

  if (opt._use_tcp) {
    Proto = IPPROTO_TCP;
    if ((res = socketManager.socket(af, SOCK_STREAM, 0)) < 0)
      goto Lerror;
  } else {
    Proto = IPPROTO_UDP;
    if ((res = socketManager.socket(af, SOCK_DGRAM, 0)) < 0)
      goto Lerror;
  }

  fd = res;

  memset(&bind_addr, 0, sizeof bind_addr);
  bind_addr.sa.sa_family = af;

  if (AF_INET6 == af) {
    if (ats_is_ip6(opt._local_ipv6)) {
      ats_ip_copy(&bind_addr.sa, opt._local_ipv6);
    } else {
      bind_addr.sin6.sin6_addr = in6addr_any;
    }
    bind_size = sizeof(sockaddr_in6);
  } else if (AF_INET == af) {
    if (ats_is_ip4(opt._local_ipv4))
      ats_ip_copy(&bind_addr.sa, opt._local_ipv4);
    else
      bind_addr.sin.sin_addr.s_addr = INADDR_ANY;
    bind_size                       = sizeof(sockaddr_in);
  } else {
    ink_assert(!"Target DNS address must be IP.");
  }

  ip_text_buffer b;
  res = socketManager.ink_bind(fd, &bind_addr.sa, bind_size, Proto);
  if (res < 0)
    Warning("Unable to bind local address to %s.", ats_ip_ntop(&bind_addr.sa, b, sizeof b));

  if (opt._non_blocking_connect)
    if ((res = safe_nonblocking(fd)) < 0)
      goto Lerror;

// cannot do this after connection on non-blocking connect
#ifdef SET_TCP_NO_DELAY
  if (opt._use_tcp)
    if ((res = safe_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, SOCKOPT_ON, sizeof(int))) < 0)
      goto Lerror;
#endif
#ifdef RECV_BUF_SIZE
  socketManager.set_rcvbuf_size(fd, RECV_BUF_SIZE);
#endif
#ifdef SET_SO_KEEPALIVE
  // enables 2 hour inactivity probes, also may fix IRIX FIN_WAIT_2 leak
  if ((res = safe_setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, SOCKOPT_ON, sizeof(int))) < 0)
    goto Lerror;
#endif

  if (opt._use_tcp)
    res = ::connect(fd, addr, ats_ip_size(addr));

  if (!res || ((res < 0) && (errno == EINPROGRESS || errno == EWOULDBLOCK))) {
    if (!opt._non_blocking_connect && opt._non_blocking_io)
      if ((res = safe_nonblocking(fd)) < 0)
        goto Lerror;
    // Shouldn't we turn off non-blocking when it's a non-blocking connect
    // and blocking IO?
  } else
    goto Lerror;

  if (eio.start(pd, this, EVENTIO_READ) < 0) {
    Error("[iocore_dns] DNSRequest::open: Failed to add %d fd to epoll list\n", fd);
    goto Lerror;
  }


  return 0;

Lerror:
  if (fd != NO_FD)
    close();
  return res;
}

void
DNSRequestMap::initialize(sockaddr const *target, DNSRequest::Options &opt)
{
  ats_ip_copy(&m_target, target);
  m_opt = opt;

  close();
}

int
DNSRequestMap::sendRequest(const int qtype,
    const char *qname,
    char *query,
    const int len,
    bool hc,
    DNSRequest *&request)
{
  request = getRequest();
  if (request == nullptr) {
    return -1;
  }

  int fd = request->fd;
  Debug("dns", "send query (qtype=%d) for %s to name_server %d fd %d hc=%d", qtype, qname, num, fd, hc);
  int s = socketManager.sendto(fd, query, len, 0, &m_target.sa, ats_ip_size(&m_target.sa));
  if (s != len) {
    releaseRequest(request);
    request = nullptr;
  }

  return s;
}

DNSRequest *
DNSRequestMap::getRequest(bool health_check)
{
  DNSRequest *req = dnsRequestAllocator.alloc();
  req->init(handler, this, health_check);
  if (req->open(&m_target.sa, m_opt) == 0)
  {
    if (health_check)
      m_healthCheckRequests.insert(req);
    else
      m_requests.insert(req);

    Debug("dns", "Creating new req %p fd = %d hc = %d to name server %d", req, req->fd, health_check, num);
    return req;
  }
  else
  {
    Error("[iocore_dns] Error creating new req %p to name server %d", req, num);
    delete req;
    return nullptr;
  }

}

bool
DNSRequestMap::releaseRequest(DNSRequest *req)
{
  if (req == nullptr)
  {
    Error("[iocore_dns] Error: Tried to release null request to name server %d", num);
    return false;
  }

  if (!req->for_healthcheck && m_requests.find(req) != m_requests.end())
  {
    Debug("dns", "Releasing req %p fd = %d to name server %d", req, req->fd, num);
    m_requests.erase(req);
    req->close();
    dnsRequestAllocator.free(req);
    return true;
  }
  else if (req->for_healthcheck && m_healthCheckRequests.find(req) != m_healthCheckRequests.end())
  {
    Debug("dns", "Releasing req %p fd = %d to name server %d", req, req->fd, num);
    m_healthCheckRequests.erase(req);
    req->close();
    dnsRequestAllocator.free(req);
    return true;
  }

  Error("[iocore_dns] Error releasing request %p fd = %d to name server %d", req, req->fd, num);
  return false;
}

void
DNSRequestMap::close()
{
  if (m_requests.empty() == false)
  {
    Debug("dns", "Releasing %zu currently open sockets to name server %d",
        m_requests.size() + m_healthCheckRequests.size(), num);
  }

  for(auto it = m_requests.begin(); it != m_requests.end(); ++it)
  {
    (*it)->close();
    dnsRequestAllocator.free(*it);
  }

  m_requests.clear();

  for(auto it = m_healthCheckRequests.begin(); it != m_healthCheckRequests.end(); ++it)
  {
    (*it)->close();
    dnsRequestAllocator.free(*it);
  }

  m_healthCheckRequests.clear();
}

void
DNSRequestMap::pruneStaleHealthCheckConnections()
{
  for(auto it = m_healthCheckRequests.begin(); it != m_healthCheckRequests.end(); )
  {
    DNSRequest *req= *it;
    if (Thread::get_hrtime() - req->start_time >= DNS_PRIMARY_RETRY_PERIOD)
    {
      Debug("dns", "Pruning health check request %p fd = %d to name server %d", req, req->fd, num);
      (*it)->close();
      dnsRequestAllocator.free(*it);
      it = m_healthCheckRequests.erase(it);
    }
    else
    {
      ++it;
    }
  }

}
