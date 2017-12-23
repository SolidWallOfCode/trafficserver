/** @file

  Dynamic data for configuration values.

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

#if !defined(TS_LUA_CONFIG_VALUE_DATA_H)
#define TS_LUA_CONFIG_VALUE_DATA_H

#include <type_traits>
#include <memory>
#include <unordered_map>
#include <list>
#include <tsconfig/Errata.h>

namespace ts
{
// Local style to enable constexpr. This could be implemented better, although it's likely the compiler is smart enough
// to put max(rest...) in a temporary.
template <typename T>
constexpr T
max(T t)
{
  return t;
}
template <typename T0, typename... T>
constexpr auto
max(T0 n, T... rest) -> typename std::common_type<T0, T...>::type
{
  return n > max(rest...) ? n : max(rest...);
}
}

struct TsLuaConfigValueDescriptor;
struct lua_State;

/// Source of the value in the config struct.
enum class TsLuaConfigSource {
  NONE,   ///< No source, the value is default constructed.
  SCHEMA, ///< Value set in schema.
  CONFIG  ///< Value set in configuration file.
};

/** Runtime (dynamic) information about a configuration value.

    This is an abstract base class for data about an instance of the value in a configuration
    struct. Actual instances will be a subclass for a supported configuration item type. This holds
    data that is per instance and therefore must be dynamically constructed as part of the
    configuration struct construction. The related description classes in contrast are data that is
    schema based and therefore can be static and shared among instances of the configuration struct.
*/
class TsLuaConfigValueData
{
public:
  using Source = TsLuaConfigSource; ///< Import type.

  TsLuaConfigValueDescriptor const &descriptor; ///< Static schema data.
  Source source = Source::NONE;                 ///< Where the instance data came from.

  /// Constructor.
  TsLuaConfigValueData(TsLuaConfigValueDescriptor const &d) : descriptor(d) {}
  virtual ~TsLuaConfigValueData() {} // force virtual.
  /// Load the instance data from the Lua stack.
  virtual ts::Errata load(lua_State *s) = 0;
};

/** Runtime configuration data for a string.

 */
class TsLuaConfigStringData : public TsLuaConfigValueData
{
  using super_type = TsLuaConfigValueData;

public:
  TsLuaConfigStringData(std::string &v, TsLuaConfigValueDescriptor const &d) : super_type(d), _value(v) {}
  ts::Errata load(lua_State *s) override;

private:
  std::string &_value; ///< Member for config data storage.
};

/** Base class for object values.

    Actual objects will be represented by subclasses of this. This class provides an abstract API for interacting
    with the subclasses. Features common to all object classes are pushed down to this class.
 */
class TsLuaConfigObjectValue
{
 protected:
  friend class TsLuaConfigObjectData;

  /** Create a value instance  in the object table indexed by @a name.

      @return Pointer to a data instance suitable for interacting with the new value for the @a name.
  */
  virtual TsLuaConfigValueData* make(ts::string_view name) = 0;

  virtual ~TsLuaConfigObjectValue() {} /// force virtual.

  /** Storage for names in the map. @c string_view is used as the key.

      For better performance on lookup but the actual string data must be stored somewhere and the Lua
      state object is not sufficiently durable.
  */
  std::list<std::string> _names;
};

/** Object handling.

    This maps from strings to values (which are schema instances). @a T is expected to be a Data
    class that loads the schema objects. It must have public type @c value_type which is the type
    for the schema instance.

    @a D is the descriptor (static data) support class for the schema instance.
 */
template <typename T, typename D>
class TsLuaConfigObjectType : public TsLuaConfigObjectValue
{
  using self_type = TsLuaConfigObjectType;
  using super_type = TsLuaConfigObjectValue;
public:
  /// The underlying schema type to be store in the properties of this object.
  /// @note For C++ implementation reasons, this can be a @c unique_ptr to the actual value type.
  using V = typename T::value_type;

  /// The value type stored in the property table.
  struct value_type {
    /// Default Constructor.
    /// Initialize the data element to reference the value and descriptor elements.
    value_type() : DATA(_value, DESCRIPTOR) {}
    V _value; ///< The schema value.
    T DATA; ///< Dynamic data helper.
    static D const& DESCRIPTOR;
  };

  /// The container type.
  using Container = std::unordered_map<ts::string_view, value_type>;

  /// Default constructor - empty object.
  TsLuaConfigObjectType() {}

  // Iteration requires subclasses because we want to transform the apparent
  // type of the table values from @c value_type to @a V so the table appears
  // to be of the schema type. This is perferred to maintaining dual tables of
  // the schema values and the helper meta data.

  class iterator {
    using self_type = iterator;
  public:
    using value_type = V;

    iterator();
    value_type& operator*() { return _i->_value; }
    value_type* operator->() { return &_i->_value; }

    bool operator == (self_type const& that) { return *this == that; }
    bool operator != (self_type const& that) { return *this != that; }
  protected:
    iterator(typename Container::iterator &&i) : _i(std::move(i)) {}

    typename Container::iterator _i;
  };

  iterator begin() { return { _map.begin() }; }
  iterator end() { return { _map.end() }; }
  iterator find(ts::string_view name) { return {_map.find(name)}; }

protected:
  TsLuaConfigValueData* make(ts::string_view name) override {
    return &((_map.emplace(std::make_pair(name, value_type())).first)->second.DATA);
  }

  /// Storage for the properties in the object.
  Container _map;
};

class TsLuaConfigObjectData : public TsLuaConfigValueData {
  using super_type = TsLuaConfigValueData;
  using self_type = TsLuaConfigObjectData;
public:
  TsLuaConfigObjectData(TsLuaConfigObjectValue &v, TsLuaConfigValueDescriptor const &d) : super_type(d), _value(v) {}

  ts::Errata load(lua_State *s) override;

private:
  TsLuaConfigObjectValue &_value; ///< Member for config data storage.
};

// Data classes for objects are dependent on the actual members and therefore are always generated and
// subclassed from @c TsLuaConfigValueData.

/// Nil type to mark uninitialized type variants.
struct TsLuaConfigNil {
};

struct TsLuaConfigNilDescriptor {
};

/** Wrapper class for variants.

    This effectively replaces the data (dynamic) structure for a value @a T. It still requires the static
    data to be passed via the descriptor type (@a D)  which needs to have the static singleton.
 */
template <typename T, typename D> class TsLuaConfigTypeVariant
{
};

/// Specialize for @c nil
template <> class TsLuaConfigTypeVariant<TsLuaConfigNil, TsLuaConfigNilDescriptor>
{
};

/** Base class for configuration classes generated from a Lua schema.
 */
class TsLuaConfig
{
public:
  virtual ~TsLuaConfig();
  ts::Errata load(const char *path);
  virtual ts::Errata load(lua_State *) = 0;
};

#endif // include
