/** @file
    Errata implementation.

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

#include "Errata.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <memory.h>

namespace ts
{
/** List of sinks for abandoned erratum.
 */
namespace
{
  std::vector<Errata::Sink::Handle> Sink_List;
}

string_view const Errata::DEFAULT_GLUE{"\n", 1};
Errata::Message const Errata::NIL_MESSAGE;

Errata::~Errata()
{
  if (_data && _data->_log_on_delete) {
    for (auto &f : Sink_List) {
      (*f)(*this);
    }
  }
}

/*  We want to allow iteration on empty / nil containers because that's very
    convenient for clients. We need only return the same value for begin()
    and end() and everything works as expected.

    However we need to be a bit more clever for VC 8.  It checks for
    iterator compatibility, i.e. that the iterators are not
    invalidated and that they are for the same container.  It appears
    that default iterators are not compatible with anything.  So we
    use static container for the nil data case.
 */
static Errata::Container NIL_CONTAINER;

Errata::iterator
Errata::begin()
{
  return _data ? _data->_items.rbegin() : NIL_CONTAINER.rbegin();
}

Errata::const_iterator
Errata::begin() const
{
  return _data ? static_cast<Data const &>(*_data)._items.rbegin() : static_cast<Container const &>(NIL_CONTAINER).rbegin();
}

Errata::iterator
Errata::end()
{
  return (_data ? _data->_items : NIL_CONTAINER).rend();
}

Errata::const_iterator
Errata::end() const
{
  Errata::Container::const_reverse_iterator zret{(_data ? _data->_items : NIL_CONTAINER).rend()};
  return zret;
}

void
Errata::registerSink(Sink::Handle const &s)
{
  Sink_List.push_back(s);
}

std::ostream &
Errata::write(std::ostream &out) const
{
  string_view lead;
  for (auto &m : *this) {
    out << lead << " [" << static_cast<int>(m._level) << "]: " << m._text << std::endl;
    if (0 == lead.size()) {
      lead = "  "_sv;
    }
  }
  return out;
}

BufferWriter&
bwformat(BufferWriter& bw, BWFSpec const& spec, Errata::Severity level)
{
  static constexpr string_view name[] = {
          "DIAG", "DEBUG", "INFO", "NOTE", "WARNING", "ERROR", "FATAL", "ALERT", "EMERGENCY"
  };
  return bwformat(bw, spec, name[static_cast<int>(level)]);
}

BufferWriter&
bwformat(BufferWriter& bw, BWFSpec const& spec, Errata const& errata)
{
  string_view lead;
  for (auto &m : errata) {
    bw.print("{}[{}] {}\n", lead, m._level, m._text);
    if (0 == lead.size()) {
      lead = "  "_sv;
    }
  }
  return bw;
}

std::ostream &
operator<<(std::ostream &os, Errata const &err)
{
  return err.write(os);
}

} // namespace ts
