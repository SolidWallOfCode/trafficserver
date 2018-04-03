/** @file

  Outbound connection tracking support.

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

#include "HttpConnectionCount.h"

OutboundConnTracker::Imp OutboundConnTracker::_imp;

std::string
OutboundConnTracker::to_json_string()
{
  std::string text;
  size_t extent = 0;
  static const ts::BWFormat header_fmt{"{{\"connectionCountSize\": {}, \"connectionCountList\": ["};
  static const ts::BWFormat item_fmt{"{{\"ip\": \"{}\", \"port\": {}, \"hostname_hash\": \"{}\", \"type\": {}, \"count\": {}}},"};
  static const ts::string_view trailer{"]}}"};
  std::vector<Group const *> groups;
  {
    ink_scoped_mutex_lock lock(_imp._mutex);
    auto n = _imp._table.count();
    groups.reserve(n);
    for (Group const &g : _imp._table) {
      groups.push_back(&g);
    }
  }

  extent += trailer.size();
  extent += ts::LocalBufferWriter<0>().print(header_fmt, groups.size()).extent();
  for (auto g : groups) {
    extent += ts::LocalBufferWriter<0>()
                .print(item_fmt, g->_addr, g->_addr.host_order_port(), g->_fqdn_hash, g->_match_type, g->_count.load())
                .extent();
  }
  text.resize(extent);
  ts::FixedBufferWriter w(const_cast<char *>(text.data()), text.size());
  w.print(header_fmt, groups.size());
  for (auto g : groups) {
    w.print(item_fmt, g->_addr, g->_addr.host_order_port(), g->_fqdn_hash, g->_match_type, g->_count.load());
  }
  if (groups.size() > 0 && w.remaining()) {
    w.auxBuffer()[-1] = ' '; // convert trailing comma to space.
  }
  w.write(trailer);
  return text;
}

struct ShowConnectionCount : public ShowCont {
  ShowConnectionCount(Continuation *c, HTTPHdr *h) : ShowCont(c, h) { SET_HANDLER(&ShowConnectionCount::showHandler); }
  int
  showHandler(int event, Event *e)
  {
    CHECK_SHOW(show(OutboundConnTracker::to_json_string().c_str()));
    return completeJson(event, e);
  }
};

Action *
register_ShowConnectionCount(Continuation *c, HTTPHdr *h)
{
  ShowConnectionCount *s = new ShowConnectionCount(c, h);
  this_ethread()->schedule_imm(s);
  return &s->action;
}
