/** @file

   MemView testing.

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

#include <ts/MemView.h>
#include <algorithm>
#include <iomanip>
#include <ios>
#include <iostream>
#include <string>
#include <vector>

using namespace ts;

template <typename T, typename S>
bool
CheckEqual(T const &lhs, S const &rhs, std::string const &prefix)
{
  bool zret = lhs == rhs;
  if (!zret) {
    std::cout << "FAIL: " << prefix << ": Expected " << lhs << " to be " << rhs << std::endl;
  }
  return zret;
}

bool
Test_1()
{
  std::string text = "01234567";
  StringView a(text);

  std::cout << "Text = |" << a << '|' << std::endl;
  std::cout << "     = |" << std::setw(5) << a << '|' << std::endl;
  std::cout << "     = |" << std::setw(12) << a << '|' << std::endl;
  std::cout << "     = |" << std::setw(12) << std::right << a << '|' << std::endl;
  std::cout << "     = |" << std::setw(12) << std::left << a << '|' << std::endl;
  std::cout << "     = |" << std::setw(12) << std::right << std::setfill('_') << a << '|' << std::endl;
  std::cout << "     = |" << std::setw(12) << std::left << std::setfill('_') << a << '|' << std::endl;
  return true;
}

bool
Test_2()
{
  bool zret = true;
  StringView sva("litt\0ral");
  StringView svb("litt\0ral", StringView::literal);
  StringView svc("litt\0ral", StringView::array);

  zret = zret && CheckEqual(sva.size(), 4U, "strlen constructor");
  zret = zret && CheckEqual(svb.size(), 8U, "literal constructor");
  zret = zret && CheckEqual(svc.size(), 9U, "array constructor");

  return zret;
}

// These tests are purely compile time.
void
Test_Compile()
{
  int i[12];
  char c[29];
  void *x = i, *y = i + 12;
  MemView mvi(i, i + 12);
  MemView mci(c, c + 29);
  MemView mcv(x, y);
}

// Example parser
// Parse a nest option string of the form "tag|tag=opt|tag=opt,opt,opt" where ':' can be used instead of '|'.
// For test, put the results in bits for comparison.

struct Token {
  StringView _name;
  int _idx; ///< Bit index for result.

  template < intmax_t N >  Token(const char (&s)[N], int n) : _name(s, StringView::literal), _idx(n) {}
};

uint64_t
Example_Parser(StringView input)
{
  static constexpr StringView OUTER_DELIMITERS{"|:", StringView::literal};
  static constexpr char INNER_DELIMITERS { ',' };
  struct Tag {
    Token _tag;
    std::vector<Token> _opts;

    Tag(Token const& token) : _tag(token) {}
    Tag(std::initializer_list<Token> const& tokens) : _tag(*tokens.begin()), _opts(tokens.begin()+1, tokens.end()) {}
  };

  static std::array<Tag, 5> tags { Tag{{"by", 0}, {"intf", 5}, {"hidden", 6}},
  Tag{{"for", 1}},
    Tag{{"host", 2}, {"pristine", 7}, {"remap", 8}, {"addr", 9}},
  Tag{{"proto", 3}},
  Tag{{"connection", 4}}
  };

  int zret = 0;
  while (input) {
    StringView opts = input.extractPrefix(OUTER_DELIMITERS);
    StringView tag = opts.extractPrefix('=');
    tag.trim(&isspace);
    for ( Tag const& t : tags ) {
      if (0 == strcasecmp(tag, t._tag._name)) {
        zret |= (1 << t._tag._idx);
        while (opts) {
          StringView opt = opts.extractPrefix(INNER_DELIMITERS);
          opt.trim(&isspace);
          for ( Token const& o : t._opts ) {
            if (0 == strcasecmp(opt, o._name))
              zret |= (1 << o._idx);
          }
        }
      }
    }
  }
  return zret;
}

int
main(int, char *argv[])
{
  bool zret = true;

  zret = zret && Test_1();
  zret = zret && Test_2();

  uint64_t p;

  p = Example_Parser(StringView("by|for|proto", StringView::literal));
  if (p != 0xb)
    std::cout << "FAIL Parse test - got " << std::hex << p << " expected " << 0xb << std::endl;

  p = Example_Parser(StringView("by=hidden|for|proto", StringView::literal));
  if (p != 0x4b)
    std::cout << "FAIL Parse test - got " << std::hex << p << " expected " << 0x4b << std::endl;

  p = Example_Parser(StringView("by=intf|for|proto|host=pristine,addr", StringView::literal));
  if (p != 0x2af)
    std::cout << "FAIL Parse test - got " << std::hex << p << " expected " << 0x2af << std::endl;

  return zret ? 0 : 1;
}
