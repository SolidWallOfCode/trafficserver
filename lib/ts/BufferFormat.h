/** @file

    Support for printf like output to @c BufferWriter.

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

#pragma once

#include <ts/BufferWriter.h>
#include <ts/TextView.h>
#include <ts/c14_utility.h>
#include <vector>
#include <unordered_map>

namespace ts
{
/** A parsed version of a format specifier.
 */
struct BW_Spec {
  /// Construct by parsing @a fmt.
  BW_Spec(TextView fmt);

  char _fill = ' '; ///< Fill character.
  char _sign = ' '; ///< Numeric sign style, space + -
  enum class Align : char {
    NONE,                   ///< No alignment.
    LEFT,                   ///< Left alignment.
    RIGHT,                  ///< Right alignment.
    CENTER,                 ///< Center alignment.
    SIGN                    ///< Align plus/minus sign before numeric fill.
  } _align   = Align::NONE; ///< Output field alignment.
  char _base = 0;           ///< Print leading base indication.
  int _min   = -1;          ///< Minimum width.
  int _prec  = -1;          ///< Precision
  int _max   = -1;          ///< Maxium width
  int _idx   = -1;          ///< Positional "name" of the specification.
  string_view _name;        ///< Name of the specification.
  string_view _ext;         ///< Extension if provided.

protected:
  Align
  align_of(char c)
  {
    return '<' == c ? Align::LEFT : '>' == c ? Align::RIGHT : '=' == c ? Align::CENTER : '^' == c ? Align::SIGN : Align::NONE;
  }
  bool
  is_sign(char c)
  {
    return '+' == c || '-' == c || ' ' == c;
  }
};

/** Overridable formatting for type @a V.

    If formatting for a type wants to have additional support for the format specifiers, this template should
    be specialized for that type. Otherwise the base stream operator is invoked.

    The most common use will be to use the extension field of the format specified, which is embedded in
    the @c BW_Spec. This can provide additional formatting options for more complex types.
 */
template <typename V>
BufferWriter &
bw_formatter(BufferWriter &w, BW_Spec const &, V const &v)
{
  return w << v;
}

template <typename TUPLE>
using FormatterSignature = BufferWriter &(*)(BufferWriter &w, BW_Spec const &, TUPLE const &args);

namespace detail
{
  // MSVC will expand the parameter pack inside a lambda but not gcc, so this indirection is required.

  /// This selects the @a I th argument in the @a TUPLE arg pack and calls the formatter on it. This
  /// (or the equivalent lambda) is needed because the array of formatters must have a homogenous
  /// signature, not vary per argument. Effectively this indirection erases the type of the specific
  /// argument being formatter.
  template <typename TUPLE, size_t I>
  BufferWriter &
  bw_formatter_selecter(BufferWriter &w, BW_Spec const &spec, TUPLE const &args)
  {
    return bw_formatter(w, spec, std::get<I>(args));
  }

  /// This exists only to expand the index sequence into an array of formatters for the tuple type
  /// @a TUPLE.  Due to langauge limitations it cannot be done directly. The formatters can be
  /// access via standard array access in constrast to templated tuple access. The actual array is
  /// static and therefor at run time the only operation is loading and return the address of the
  /// array.
  template <typename TUPLE, size_t... N>
  FormatterSignature<TUPLE> *
  bw_formatter_array(index_sequence<N...>)
  {
    static FormatterSignature<TUPLE> fa[sizeof...(N)] = {&detail::bw_formatter_selecter<TUPLE, N>...};
    return fa;
  }

  /// Perform alignment adjustments / fill on @a w of the content in @a lw.
  void bw_aligner(BW_Spec const &spec, BufferWriter &w, BufferWriter &lw);

  /// Global named argument table.
  using BW_GlobalSignature = void(*)(BufferWriter&, BW_Spec const&);
  using BW_GlobalTable = std::unordered_map<string_view, BW_GlobalSignature>;
  BW_GlobalSignature BUFFER_FORMAT_GLOBAL_TABLE;
} // detail

/** BufferWriter print.

    This prints its arguments to the @c BufferWriter @a w according to the format @a fmt. The format
    string is based on Python style formating, each argument substitution marked by braces, {}. Each
    specification has three parts, a @a name, a @a specified, and a @a extention. These are
    separated by colons. The name should be either omitted or a number, the index of the argument to
    use. If omitted the place in the format string is used as the argument index. E.g. "{} {} {}",
    "{} {1} {}", and "{0} {1} {2}" are equivalent. Using an explicit index does not reset the
    position of subsequent substiations, therefore "{} {0} {}" is equivalent to "{0} {0} {2}".
 */
template <typename... Rest>
int
bwprint(BufferWriter &w, TextView fmt, Rest const &... rest)
{
  static constexpr int N = sizeof...(Rest);
  auto args(std::forward_as_tuple(rest...));
  auto fa     = detail::bw_formatter_array<decltype(args)>(index_sequence_for<Rest...>{});
  int arg_idx = 0;

  while (fmt) {
    ts::TextView lit = fmt.take_prefix_at('{');
    if (lit) {
      w.write(lit);
    }
    if (fmt) {
      // Need to be careful, because an empty format is OK and it's hard to tell if
      // take_prefix_at failed to find the delimiter or found it as the first byte.
      TextView::size_type off = fmt.find('}');
      if (off == TextView::npos) {
        throw std::invalid_argument("Unclosed {");
      }
      BW_Spec spec{fmt.take_prefix_at(off)};
      if (spec._name.size() == 0)
        spec._idx = arg_idx;
      if (0 <= spec._idx && spec._idx < N) {
        size_t width = w.remaining();
        if (spec._max >= 0)
          width = std::min(width, static_cast<size_t>(spec._max));
        FixedBufferWriter lw{w.auxBuffer(), width};
        fa[spec._idx](lw, spec, args);
        detail::bw_aligner(spec, w, lw);
      }
      ++arg_idx;
    }
  }
  return 0;
}
}

class BW_
{
public:
  BW_(ts::TextView fmt);
  ~BW_();

protected:
  enum spec_type { LITERAL, SPEC };
  struct Item {
    spec_type type = LITERAL;
    union Payload {
      ts::string_view text;
      ts::BW_Spec spec;

      Payload() : text{} {}
    } payload;
    Item(ts::string_view t) : type(LITERAL) { payload.text = t; }
    Item(ts::BW_Spec const &s) : type(SPEC) { payload.spec = s; }
  };

  using Items = std::vector<Item>;
  Items items;
};
