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

#include "tscore/ink_platform.h"
#include "P_Net.h"
#include "Show.h"
#include "I_Tasks.h"

struct ShowNet;
using ShowNetEventHandler = int (ShowNet::*)(int, Event *);
struct ShowNet : public ShowCont {
  int ithread;
  IpEndpoint addr;

  int
  showMain(int event, Event *e)
  {
    this->begin("Net");
    mbw.print("<H3>Show <A HREF=\"./connections\">Connections</A></H3>\n"
              "<form method = GET action = \"./ips\">\n"
              "Show Connections to/from IP (e.g. 127.0.0.1):<br>\n"
              "<input type=text name=ip size=64 maxlength=256>\n"
              "</form>\n"
              "<form method = GET action = \"./ports\">\n"
              "Show Connections to/from Port (e.g. 80):<br>\n"
              "<input type=text name=name size=64 maxlength=256>\n"
              "</form>\n");
    return complete(event, e);
  }

  int
  showConnectionsOnThread(int event, Event *e)
  {
    EThread *ethread = e->ethread;
    NetHandler *nh   = get_NetHandler(ethread);
    MUTEX_TRY_LOCK(lock, nh->mutex, ethread);
    if (!lock.is_locked()) {
      ethread->schedule_in(this, HRTIME_MSECONDS(net_retry_delay));
      return EVENT_DONE;
    }

    ink_hrtime now = Thread::get_hrtime();
    forl_LL(NetEvent, ne, nh->open_list)
    {
      auto vc = dynamic_cast<UnixNetVConnection *>(ne);
      //      uint16_t port = ats_ip_port_host_order(&addr.sa);
      if (vc == nullptr || (ats_is_ip(&addr) && !ats_ip_addr_port_eq(&addr.sa, vc->get_remote_addr()))) {
        continue;
      }
      mbw.print("<tr>"
                "<td>{}</td>"          // ID
                "<td>{::a}</td>"       // ipbuf
                "<td>{1::p}</td>"      // port
                "<td>{}</td>"          // fd
                "<td>[{}] {::ap}</td>" // interbuf
                                       //                      "<td>{}</td>"     // accept port
                "<td>{} secs ago</td>" // start time
                "<td>{}</td>"          // thread id
                "<td>{}</td>"          // read enabled
                "<td>{}</td>"          // read NBytes
                "<td>{}</td>"          // read NDone
                "<td>{}</td>"          // write enabled
                "<td>{}</td>"          // write nbytes
                "<td>{}</td>"          // write ndone
                "<td>{} secs</td>"     // Inactivity timeout at
                "<td>{} secs</td>"     // Activity timeout at
                "<td>{}</td>"          // shutdown
                "<td>-{}</td>"         // comments
                "</tr>\n",
                vc->id, vc->get_remote_addr(), vc->con.fd, vc->options.addr_binding, vc->options.local_ip,
                ((now - vc->submit_time) / HRTIME_SECOND), ethread->id, vc->read.enabled, vc->read.vio.nbytes, vc->read.vio.ndone,
                vc->write.enabled, vc->write.vio.nbytes, vc->write.vio.ndone, (vc->inactivity_timeout_in / HRTIME_SECOND),
                (vc->active_timeout_in / HRTIME_SECOND), vc->f.shutdown ? "shutdown" : "", vc->closed ? "closed " : "");
    }
    ithread++;
    if (ithread < eventProcessor.thread_group[ET_NET]._count) {
      eventProcessor.thread_group[ET_NET]._thread[ithread]->schedule_imm(this);
    } else {
      mbw.write("</table>\n");
      return complete(event, e);
    }
    return EVENT_CONT;
  }

  int
  showConnections(int event, Event *e)
  {
    this->begin("Net Connections");
    mbw.write("<H3>Connections</H3>\n"
              "<table border=1><tr>"
              "<th>ID</th>"
              "<th>IP</th>"
              "<th>Port</th>"
              "<th>FD</th>"
              "<th>Interface</th>"
              "<th>Accept Port</th>"
              "<th>Time Started</th>"
              "<th>Thread</th>"
              "<th>Read Enabled</th>"
              "<th>Read NBytes</th>"
              "<th>Read NDone</th>"
              "<th>Write Enabled</th>"
              "<th>Write NBytes</th>"
              "<th>Write NDone</th>"
              "<th>Inactive Timeout</th>"
              "<th>Active   Timeout</th>"
              "<th>Shutdown</th>"
              "<th>Comments</th>"
              "</tr>\n");
    SET_HANDLER(&ShowNet::showConnectionsOnThread);
    eventProcessor.thread_group[ET_NET]._thread[0]->schedule_imm(this); // This can not use ET_TASK.
    return EVENT_CONT;
  }

  int
  showSingleThread(int event, Event *e)
  {
    EThread *ethread               = e->ethread;
    NetHandler *nh                 = get_NetHandler(ethread);
    PollDescriptor *pollDescriptor = get_PollDescriptor(ethread);
    MUTEX_TRY_LOCK(lock, nh->mutex, ethread);
    if (!lock.is_locked()) {
      ethread->schedule_in(this, HRTIME_MSECONDS(net_retry_delay));
      return EVENT_DONE;
    }

    mbw.write("<H3>Thread: %d</H3>\n", ithread);
    mbw.write("<table border=1>\n");
    int connections = 0;
    forl_LL(NetEvent, ne, nh->open_list)
    {
      if (dynamic_cast<UnixNetVConnection *>(ne) != nullptr) {
        ++connections;
      }
    }
    mbw.print("<tr><td>Connections</td><td>{}</td></tr>\n", connections);
    mbw.print("<tr><td>Last Pool Ready</td><td>%d</td></tr>\n", pollDescriptor->result);
    mbw.write("</table>\n");
    mbw.write("<table border=1>\n");
    mbw.write("<tr><th>#</th><th>Read Priority</th><th>Read Bucket</th><th>Write Priority</th><th>Write Bucket</th></tr>\n");
    mbw.write("</table>\n");
    ++ithread;
    if (ithread < eventProcessor.thread_group[ET_NET]._count) {
      eventProcessor.thread_group[ET_NET]._thread[ithread]->schedule_imm(this);
    } else {
      return complete(event, e);
    }
    return EVENT_CONT;
  }

  int
  showThreads(int event, Event *e)
  {
    this->begin("Net Threads");
    SET_HANDLER(&ShowNet::showSingleThread);
    eventProcessor.thread_group[ET_NET]._thread[0]->schedule_imm(this); // This can not use ET_TASK
    return EVENT_CONT;
  }
  int
  showHostnames(int event, Event *e)
  {
    this->begin("Net Connections to/from Host");
    return complete(event, e);
  }

  ShowNet(Continuation *c, HTTPHdr *h) : ShowCont(c, h), ithread(0)
  {
    memset(&addr, 0, sizeof(addr));
    SET_HANDLER(&ShowNet::showMain);
  }
};

Action *
register_ShowNet(Continuation *c, HTTPHdr *h)
{
  ShowNet *s = new ShowNet(c, h);
  int str_len;
  const char *str = h->url_get()->path_get(&str_len);
  ts::TextView path{str, str_len};

  SET_CONTINUATION_HANDLER(s, &ShowNet::showMain);
  if ("connections"_tv.isNoCasePrefixOf(path)) {
    SET_CONTINUATION_HANDLER(s, &ShowNet::showConnections);
  } else if ("threads"_tv.isNoCasePrefixOf(path)) {
    SET_CONTINUATION_HANDLER(s, &ShowNet::showThreads);
  } else if ("ips"_tv.isNoCasePrefixOf(path)) {
    int query_len;
    const char *query = h->url_get()->query_get(&query_len);
    s->sarg.assign(query, query_len);
    ts::TextView gn = ts::TextView{s->sarg}.split_suffix_at('=');
    if (gn) {
      ats_ip_pton(gn, &s->addr);
    }
    SET_CONTINUATION_HANDLER(s, &ShowNet::showConnections);
  } else if ("ports"_tv.isNoCasePrefixOf(path)) {
    int query_len;
    const char *query = h->url_get()->query_get(&query_len);
    s->sarg.assign(query, query_len);
    ts::TextView gn = ts::TextView{s->sarg}.split_suffix_at('=');
    if (gn) {
      ats_ip_port_cast(&s->addr.sa) = htons(svtoi(gn));
    }
    SET_CONTINUATION_HANDLER(s, &ShowNet::showConnections);
  }
  eventProcessor.schedule_imm(s, ET_TASK);
  return &s->action;
}
