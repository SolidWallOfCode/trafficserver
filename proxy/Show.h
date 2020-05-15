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

/****************************************************************************

  Show.h


 ****************************************************************************/

#pragma once

#include "eventsystem/I_MIOBufferWriter.h"
#include "StatPages.h"

struct ShowCont;
using ShowContEventHandler = int (ShowCont::*)(int event, Event *data);
struct ShowCont : public Continuation {
private:
  IOBufferChain buf;

public:
  IOChainWriter mbw{buf};
  Action action;
  std::string sarg;

  int
  finishConn(int event, Event *e)
  {
    if (!action.cancelled) {
      action.continuation->handleEvent(STAT_PAGE_SUCCESS, &buf);
    }
    buf.clear();
    return done(VIO::CLOSE, event, e);
  }

  int
  complete(int event, Event *e)
  {
    mbw.print("</BODY>\n</HTML>\n");
    return finishConn(event, e);
  }

  int
  completeJson(int event, Event *e)
  {
    return finishConn(event, e);
  }

  int
  complete_error(int event, Event *e)
  {
    if (!action.cancelled) {
      action.continuation->handleEvent(STAT_PAGE_FAILURE, nullptr);
    }
    buf.clear();
    return done(VIO::ABORT, event, e);
  }

  void
  begin(std::string_view const &name)
  {
    mbw.print("<HTML>\n<HEAD><TITLE>{0}</TITLE>\n"
              "<BODY BGCOLOR=\"#ffffff\" FGCOLOR=\"#00ff00\">\n"
              "<H1>{0}</H1>\n",
              name);
  }

  int
  showError(int event, Event *e)
  {
    return complete_error(event, e);
  }

  virtual int
  done(int /* e ATS_UNUSED */, int /* event ATS_UNUSED */, void * /* data ATS_UNUSED */)
  {
    delete this;
    return EVENT_DONE;
  }

  ShowCont(Continuation *c, HTTPHdr * /* h ATS_UNUSED */) : Continuation(nullptr), sarg(nullptr)
  {
    mutex  = c->mutex;
    action = c;
    buf.clear(); // make sure it's empty.
  }

  ~ShowCont() override { buf.clear(); }
};
