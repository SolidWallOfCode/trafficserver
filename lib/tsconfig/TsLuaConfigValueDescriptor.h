/** @file

  TS Lua Config base classes for static value descriptors.

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

/*
 * File:   TSConfigLua.h
 * Author: persia
 *
 * Created on September 21, 2017, 4:04 PM
 */

#if !defined(TS_LUA_CONFIG_VALUE_DESCRIPTOR_H)
#define TS_LUA_CONFIG_VALUE_DESCRIPTOR_H

#include <unordered_map>
#include <tsconfig/Errata.h>
#include <ts/string_view.h>
#include <tsconfig/TsLuaMetaConfig.h>

/** Static schema data for a configuration value.

    This is a base class for data about a configuration value. This is intended to be a singleton
    static instance that contains schema data that is the same for all instances of the
    configuration value.
*/
struct TsLuaConfigValueDescriptor {
  using Type = TsLuaMetaConfig::ValueType;
  Type type;                   ///< Value type.
  ts::string_view type_name;   ///< Literal type name used in the schema.
  ts::string_view name;        ///< Name of the configuration value.
  ts::string_view description; ///< Description of the  value.
};

template < typename V >
class TsLuaConfigObjectDescriptor : public TsLuaConfigValueDescriptor {
  using super_type = TsLuaConfigValueDescriptor;
  using self_type = TsLuaConfigObjectDescriptor;

public:
  using PropertyPtr = TsLuaConfigValueData V::*;
  using PropertyMap = std::unordered_map<ts::string_view, PropertyPtr>;
  using Property = typename PropertyMap::value_type;

 TsLuaConfigObjectDescriptor(ts::string_view name, ts::string_view description, std::initializer_list<Property> properties)
   : super_type{TsLuaMetaConfig::ValueType::OBJECT, "object", name, description},
    _properties(properties)
      {}

  PropertyMap _properties;
};

class TsLuaConfigEnumDescriptor : public TsLuaConfigValueDescriptor
{
  using self_type  = TsLuaConfigEnumDescriptor;
  using super_type = TsLuaConfigValueDescriptor;

public:
  struct Pair {
    ts::string_view key;
    int value;
  };
  TsLuaConfigEnumDescriptor(Type t, ts::string_view t_name, ts::string_view n, ts::string_view d, std::initializer_list<Pair> pairs)
    : super_type{t, t_name, n, d}, values{pairs.size()}, keys{pairs.size()}
  {
    for (auto &p : pairs) {
      values[p.key] = p.value;
      keys[p.value] = p.key;
    }
  }
  std::unordered_map<ts::string_view, int> values;
  std::unordered_map<int, ts::string_view> keys;
  int
  get(ts::string_view key)
  {
    return values[key];
  }
};

#if 0
template < typename E >
class TsLuaConfigEnumData : public TsLuaConfigValueData {
  using self_type = TsLuaConfigEnum;
  using super_type = TsLuaConfigValueData;
public:
   TsLuaConfigEnumData(TsLuaConfigEnumDescriptor const& d, int& i) : super_type(d),edescriptor(d), ref(i) {}
   TsLuaConfigEnumDescriptor edescriptor;
   int& ref;
   ts::Errata load(lua_State* L) override
   {
    ts::Errata zret;
    std::string key(lua_tostring(L,-1));
    ref = edescriptor.get(ts::string_view(key));
    return zret;
    }
};
#endif

#endif /* TSCONFIGLUA_H */
