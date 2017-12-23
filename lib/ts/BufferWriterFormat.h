#if !defined TS_BUFFERWRITER_FORMAT_H_
#define TS_BUFFERWRITER_FORMAT_H_

/** @file

    Formatting of basic types for @c BufferWriter

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

#include <ts/BufferWriter.h>

namespace ts
{
inline BufferWriter &
operator<<(BufferWriter &w, uintmax_t x)
{
  char txt[std::numeric_limits<uintmax_t>::digits10 + 1];
  int n = sizeof(txt);
  while (x) {
    txt[--n] = '0' + (x % 10);
    x /= 10;
  }
  if (n == sizeof(txt))
    txt[--n] = '0';
  return w.write(txt + n, sizeof(txt) - n);
}

inline BufferWriter &
operator<<(BufferWriter &w, unsigned int x)
{
  return w << static_cast<uintmax_t>(x);
}

inline BufferWriter &
operator<<(BufferWriter &w, intmax_t x)
{
  if (x < 0) {
    w << '-';
    x = -x;
  }
  return w << static_cast<uintmax_t>(x);
}

inline BufferWriter &
operator<<(BufferWriter &w, int x)
{
  return w << static_cast<intmax_t>(x);
}
}

#endif
