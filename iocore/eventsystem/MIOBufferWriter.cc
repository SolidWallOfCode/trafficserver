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
  MIOBufferWriter.cc

**************************************************************************/

#include "I_MIOBufferWriter.h"

auto
IOBlockWriter::write(const void *data, size_t length) -> self_type &
{
  while (length > 0) {
    auto span = this->writable();
    auto n    = std::min(span.size(), length);
    memcpy(span.data(), data, n);
    length -= n;
    this->commit(n);
  }
  return *this;
}

ts::MemSpan<char>
IOChainWriter::writable()
{
  auto span = _chain.writable();
  if (span.empty()) {
    // Get bigger blocks if more space is needed, up to the max block size.
    _block_size_idx = std::min<int64_t>(_block_size_idx + 1, MAX_BUFFER_SIZE_INDEX);
    _chain.add_block(_block_size_idx);
    span = _chain.writable();
  }
  return span;
}

#if defined(UNIT_TEST_BUFFER_WRITER)

// Dummys just for linkage (never called).

std::ostream &
MIOBufferWriter::operator>>(std::ostream &stream) const
{
  return stream;
}

ssize_t
MIOBufferWriter::operator>>(int fd) const
{
  return 0;
}

#else

std::ostream &
MIOBufferWriter::operator>>(std::ostream &stream) const
{
  IOBufferReader *r = _miob->alloc_reader();
  if (r) {
    IOBufferBlock *b;
    while (nullptr != (b = r->get_current_block())) {
      auto n = b->read_avail();
      stream.write(b->start(), n);
      r->consume(n);
    }
    _miob->dealloc_reader(r);
  }
  return stream;
}

ssize_t
MIOBufferWriter::operator>>(int fd) const
{
  ssize_t zret           = 0;
  IOBufferReader *reader = _miob->alloc_reader();
  if (reader) {
    IOBufferBlock *b;
    while (nullptr != (b = reader->get_current_block())) {
      auto n = b->read_avail();
      auto r = ::write(fd, b->start(), n);
      if (r <= 0) {
        break;
      } else {
        reader->consume(r);
        zret += r;
      }
    }
    _miob->dealloc_reader(reader);
  }
  return zret;
}

#endif // defined(UNIT_TEST_BUFFER_WRITER)
