/** @file

    Assistant class for translating strings to and from enumeration values.

    @section license License

    Licensed to the Apache Software Foundation (ASF) under one or more contributor license
    agreements.  See the NOTICE file distributed with this work for additional information regarding
    copyright ownership.  The ASF licenses this file to you under the Apache License, Version 2.0
    (the "License"); you may not use this file except in compliance with the License.  You may
    obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software distributed under the
    License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
    express or implied. See the License for the specific language governing permissions and
    limitations under the License.
 */

#pragma once

#include <string_view>
#include <initializer_list>
#include <variant>
#include <tuple>
#include <functional>
#include <ts/IntrusiveHashMap.h>
#include <ts/MemArena.h>
#include <ts/BufferWriter.h>
#include <ts/HashFNV.h>

namespace ts
{
/** A bidirectional mapping between names and enumeration values.

    This is intended to be a support class to make interacting with enumerations easier for
    configuration and logging. Names and enumerations can then be easily and reliably interchanged.
    The names are case insensitive but preserving.

    Each enumeration has a @a primary name and an arbitrary number of @a secondary names. When
    converting from an enumeration, the primary name is used. However, any of the names will be
    converted to the enumeration. For instance, a @c Lexicon for a boolean might have the primary
    name of @c TRUE be "true" with the secondary names "1", "yes", "enable". In that case converting
    @c TRUE would always be "true", while converting any of "true", "1", "yes", or "enable" would
    yield @c TRUE. This is convenient for parsing configurations to be more tolerant of input.

    @note All names and value must be unique across the Lexicon. All name comparisons are case
    insensitive.
 */
template <typename E> class Lexicon
{
  using self_type = Lexicon; ///< Self reference type.
public:
  /// Used for initializer lists that have just a primary value.
  using Pair = std::tuple<E, std::string_view>;
  /// A function to be called if a value is not found.
  using UnknownValueHandler = std::function<std::string_view(E)>;
  /// A function to be called if a name is not found.
  using UnknownNameHandler = std::function<E(std::string_view)>;

  /// Element of an initializer list that contains secondary names.
  struct Definition {
    const E &value;                                       ///< Value for definition.
    const std::initializer_list<std::string_view> &names; ///< Primary then secondary names.
  };

  /// Construct with secondary names.
  Lexicon(const std::initializer_list<Definition> &items);
  /// Construct with primary names only.
  Lexicon(const std::initializer_list<Pair> &items);

  /// Convert a value to a name
  std::string_view operator[](E value);

  /// Convert a name to a value
  E operator[](std::string_view name);

  /// Define the @a names for a @a value.
  /// The first name is the primary name. All @a names must be convertible to @c std::string_view.
  /// <tt>lexicon.define(Value, primary, [secondary, ... ]);</tt>
  template <typename... Args> self_type &define(E value, Args &&... names);
  // These are really for consistency with constructors, they're not expected to be commonly used.
  /// Define a value and names.
  /// <tt>lexicon.define(Value, { primary, [secondary, ...] });</tt>
  self_type &define(E value, const std::initializer_list<std::string_view> &names);
  self_type &define(const Pair &pair);
  self_type &define(const Definition &init);

  /** Set a default @a value.
   *
   * @param value Value to return if a name is not found.
   * @return @c *this
   */
  self_type &set_default(E value);

  /** Set a default @a name.
   *
   * @param name Name to return if a value is not found.
   * @return @c *this
   *
   * @note The @a name is copied to local storage.
   */
  self_type &set_default(std::string_view name);

  /** Set a default @a handler for names that are not found.
   *
   * @param handler Function to call with a name that was not found.
   * @return @c this
   *
   * @a handler is passed the name that was not found as a @c std::string_view and must return a
   * value which is then returned to the caller.
   */
  self_type &set_default(const UnknownNameHandler &handler);

  /** Set a default @a handler for values that are not found.
   *
   * @param handler Function to call with a value that was not found.
   * @return @c *this
   *
   * @a handler is passed the value that was not found and must return a name as a @c std::string_view.
   * Caution must be used because the returned name must not leak and must be thread safe. The most
   * common use would be for logging bad values.
   */
  self_type &set_default(const UnknownValueHandler &handler);

protected:
  using NameDefault  = std::variant<std::monostate, std::string_view, UnknownValueHandler>;
  using ValueDefault = std::variant<std::monostate, E, UnknownNameHandler>;

  /// Each unique pair of value and name is stored as an instance of this class.
  /// The primary is stored first and is therefore found by normal lookup.
  struct Item {
    Item(E, std::string_view);

    E _value;               ///< Definition value.
    std::string_view _name; ///< Definition name

    /// Intrusive linkage for name lookup.
    struct NameLinkage {
      Item *_next{nullptr};
      Item *_prev{nullptr};

      static Item *&next_ptr(Item *);

      static Item *&prev_ptr(Item *);

      static std::string_view key_of(Item *);

      static uint32_t hash_of(std::string_view s);

      static bool equal(std::string_view const &lhs, std::string_view const &rhs);
    } _name_link;

    /// Intrusive linkage for value lookup.
    struct ValueLinkage {
      Item *_next{nullptr};
      Item *_prev{nullptr};

      static Item *&next_ptr(Item *);

      static Item *&prev_ptr(Item *);

      static E key_of(Item *);

      static uintmax_t hash_of(E);

      static bool equal(E lhs, E rhs);
    } _value_link;
  };

  /// Copy @a name in to local storage.
  std::string_view localize(std::string_view name);

  /// Storage for names.
  MemArena _arena{1024};
  /// Access by name.
  IntrusiveHashMap<typename Item::NameLinkage> _by_name;
  /// Access by value.
  IntrusiveHashMap<typename Item::ValueLinkage> _by_value;
  NameDefault _name_default;
  ValueDefault _value_default;
};

// ==============
// Implementation

// ----
// Item

template <typename E> Lexicon<E>::Item::Item(E value, std::string_view name) : _value(value), _name(name) {}

template <typename E>
auto
Lexicon<E>::Item::NameLinkage::next_ptr(Item *item) -> Item *&
{
  return item->_name_link._next;
}

template <typename E>
auto
Lexicon<E>::Item::NameLinkage::prev_ptr(Item *item) -> Item *&
{
  return item->_name_link._prev;
}

template <typename E>
auto
Lexicon<E>::Item::ValueLinkage::next_ptr(Item *item) -> Item *&
{
  return item->_value_link._next;
}

template <typename E>
auto
Lexicon<E>::Item::ValueLinkage::prev_ptr(Item *item) -> Item *&
{
  return item->_value_link._prev;
}

template <typename E>
std::string_view
Lexicon<E>::Item::NameLinkage::key_of(Item *item)
{
  return item->_name;
}

template <typename E>
E
Lexicon<E>::Item::ValueLinkage::key_of(Item *item)
{
  return item->_value;
}

template <typename E>
uint32_t
Lexicon<E>::Item::NameLinkage::hash_of(std::string_view s)
{
  return ATSHash32FNV1a().hash_immediate(s.data(), s.size(), ATSHash::nocase());
}

template <typename E>
uintmax_t
Lexicon<E>::Item::ValueLinkage::hash_of(E value)
{
  // In almost all cases, the values will be (roughly) sequential, so an identity hash works well.
  return static_cast<uintmax_t>(value);
}

template <typename E>
bool
Lexicon<E>::Item::NameLinkage::equal(std::string_view const &lhs, std::string_view const &rhs)
{
  return 0 == strcasecmp(lhs, rhs);
}

template <typename E>
bool
Lexicon<E>::Item::ValueLinkage::equal(E lhs, E rhs)
{
  return lhs == rhs;
}

// -------
// Lexicon
template <typename E>
std::string_view
Lexicon<E>::localize(std::string_view name)
{
  auto span = _arena.alloc(name.size());
  memcpy(span.data(), name.data(), name.size());
  return span.view();
}

template <typename E> Lexicon<E>::Lexicon(const std::initializer_list<Definition> &items)
{
  for (auto item : items) {
    this->define(item.value, item.names);
  }
}

template <typename E> Lexicon<E>::Lexicon(const std::initializer_list<Pair> &items)
{
  for (auto item : items) {
    this->define(item);
  }
}

template <typename E> std::string_view Lexicon<E>::Lexicon::operator[](E value)
{
  auto spot = _by_value.find(value);
  if (spot != _by_value.end()) {
    return spot->_name;
  } else if (_name_default.index() == 1) {
    return std::get<1>(_name_default);
  } else if (_value_default.index() == 2) {
    return std::get<2>(_name_default)(value);
  }
  throw std::domain_error(
    ts::LocalBufferWriter<128>().print("Lexicon: unknown enumeration '{}'\0", static_cast<uintmax_t>(value)).data());
}

template <typename E> E Lexicon<E>::Lexicon::operator[](std::string_view name)
{
  auto spot = _by_name.find(name);
  if (spot != _by_name.end()) {
    return spot->_value;
  } else if (_value_default.index() == 1) {
    return std::get<1>(_value_default);
  } else if (_value_default.index() == 2) {
    return std::get<2>(_value_default)(name);
  }
  throw std::domain_error(ts::LocalBufferWriter<128>().print("Lexicon: unknown name '{}'\0", name).data());
}

template <typename E>
template <typename... Args>
auto
Lexicon<E>::Lexicon::define(E value, Args &&... names) -> self_type &
{
  static_assert(sizeof...(Args) > 0, "A defined value must have at least a priamry name");
  return this->define(value, {std::forward<Args>(names)...});
}

template <typename E>
auto
Lexicon<E>::Lexicon::define(E value, const std::initializer_list<std::string_view> &names) -> self_type &
{
  if (names.size() < 1) {
    throw std::invalid_argument("A defined value must have at least a primary name");
  }
  for (auto name : names) {
    auto i = new Item(value, this->localize(name));
    _by_name.insert(i);
    _by_value.insert(i);
  }
  return *this;
}

template <typename E>
auto
Lexicon<E>::Lexicon::define(const Pair &pair) -> self_type &
{
  return this->define(std::get<0>(pair), {std::get<1>(pair)});
}

template <typename E>
auto
Lexicon<E>::Lexicon::define(const Definition &init) -> self_type &
{
  return this->define(init.value, init.names);
}

template <typename E>
auto
Lexicon<E>::Lexicon::set_default(std::string_view name) -> self_type &
{
  _name_default = this->localize(name);
  return *this;
}

template <typename E>
auto
Lexicon<E>::Lexicon::set_default(E value) -> self_type &
{
  _value_default = value;
  return *this;
}

template <typename E>
auto
Lexicon<E>::Lexicon::set_default(const UnknownValueHandler &handler) -> self_type &
{
  _name_default = handler;
  return *this;
}

template <typename E>
auto
Lexicon<E>::Lexicon::set_default(const UnknownNameHandler &handler) -> self_type &
{
  _value_default = handler;
  return *this;
}

} // namespace ts
