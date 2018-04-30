/** @file

  TsLuaConfig Meta schema definitions and statics.

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

#include "TsLuaConfig.h"
#include <tsconfig/TsErrataUtil.h>
#include "luajit/src/lua.hpp"

class TsLuaMetaConfig::SchemaDescriptor : public TsLuaConfigValueDescriptor
{
  using super_type = TsLuaConfigValueDescriptor;
  using self_type  = SchemaDescriptor;

public:
  using value_type  = SchemaValue;
  using MPtr        = TsLuaConfigValueData value_type::*;
  using PropertyMap = std::unordered_map<ts::string_view, MPtr>;

  SchemaDescriptor()
    : super_type{OBJECT, "object", "schema", "Schema for configuration"},
      _properties{{value_type::DESCRIPTION_DESCRIPTOR.name, reinterpret_cast<MPtr>(&value_type::DESCRIPTION_DATA)},
                  {value_type::CLASS_DESCRIPTOR.name, reinterpret_cast<MPtr>(&value_type::CLASS_DATA)},
                  {value_type::TYPE_DESCRIPTOR.name, reinterpret_cast<MPtr>(&value_type::TYPE_DATA)},
                  {value_type::PROPERTIES_DESCRIPTOR.name, reinterpret_cast<MPtr>(&value_type::PROPERTIES_DATA)}}
  {
  }

  PropertyMap _properties;
};

class TsLuaMetaConfig::MetaSchemaDescriptor : public SchemaDescriptor
{
  using super_type = SchemaDescriptor;
  using self_type  = MetaSchemaDescriptor;
  using value_type = MetaSchemaValue;

public:
  MetaSchemaDescriptor()
  {
    _properties.emplace(std::make_pair(value_type::P_SCHEMA_DESCRIPTOR.name, reinterpret_cast<MPtr>(&value_type::P_SCHEMA_DATA)));
  }
};

const TsLuaMetaConfig::SchemaDescriptor TsLuaMetaConfig::SCHEMA_DESCRIPTOR;
const TsLuaMetaConfig::MetaSchemaDescriptor TsLuaMetaConfig::METASCHEMA_DESCRIPTOR;

const TsLuaConfigStringDescriptor TsLuaMetaConfig::SchemaValue::DESCRIPTION_DESCRIPTOR{STRING, "string", "description",
                                                                                       "Schema / configuration description."};
const TsLuaConfigStringDescriptor TsLuaMetaConfig::SchemaValue::CLASS_DESCRIPTOR{STRING, "string", "class",
                                                                                 "Name of the C++ class to generate."};
const TsLuaConfigStringDescriptor TsLuaMetaConfig::SchemaValue::TYPE_DESCRIPTOR{STRING, "string", "type",
                                                                                "Type reference for value."};

const TsLuaConfigObjectDescriptor<TsLuaMetaConfig::SchemaValue> TsLuaMetaConfig::SchemaValue::PROPERTIES_DESCRIPTOR(
  "properties"_sv, "the schema properties"_sv,
  {{SchemaValue::DESCRIPTION_DESCRIPTOR.name, reinterpret_cast<PropertyDescriptor::PropertyPtr>(&SchemaValue::DESCRIPTION_DATA)},
   {SchemaValue::CLASS_DESCRIPTOR.name, reinterpret_cast<PropertyDescriptor::PropertyPtr>(&SchemaValue::CLASS_DATA)},
   {SchemaValue::TYPE_DESCRIPTOR.name, reinterpret_cast<PropertyDescriptor::PropertyPtr>(&SchemaValue::TYPE_DATA)},
   {SchemaValue::PROPERTIES_DESCRIPTOR.name, reinterpret_cast<PropertyDescriptor::PropertyPtr>(&SchemaValue::PROPERTIES_DATA)}});

template <>
TsLuaMetaConfig::SchemaDescriptor const &TsLuaMetaConfig::SchemaProperties::value_type::DESCRIPTOR{
  TsLuaMetaConfig::SCHEMA_DESCRIPTOR};

const TsLuaConfigStringDescriptor TsLuaMetaConfig::MetaSchemaValue::P_SCHEMA_DESCRIPTOR{STRING, "string", "$schema",
                                                                                        "Schema identifier"};

TsLuaMetaConfig::SchemaValue::SchemaValue()
  : DESCRIPTION_DATA(_description, DESCRIPTION_DESCRIPTOR),
    CLASS_DATA(_class, CLASS_DESCRIPTOR),
    TYPE_DATA(_type, TYPE_DESCRIPTOR),
    PROPERTIES_DATA(_properties, PROPERTIES_DESCRIPTOR)
{
}

TsLuaMetaConfig::MetaSchemaValue::MetaSchemaValue() : P_SCHEMA_DATA(p_schema, P_SCHEMA_DESCRIPTOR)
{
}

// schema.globals

#if 0
ts::Errata
TsLuaMetaConfig::SchemaValue::TsLua::load(lua_State *L)
{
  ts::Errata zret;

  lua_getfield(L, -1, _descriptor._name.data());
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    return {ts::msg::WARN, "schema.globals load failed - not an OBJECT [table]"};
  }
  // Walk the table.
  lua_pushnil(L);
  while (lua_next(L, -2)) {
    if (lua_isstring(L, -2)) {
      ts::string_view name = lua_tostring(L, -2);
    }
    lua_pop(L, 1); // drop value, keep name for iteration.
  }

  return zret;
}
#endif

TsLuaMetaConfig::SchemaData::SchemaData(SchemaValue &v, SchemaDescriptor const &d) : super_type(d), _value(v)
{
}
TsLuaMetaConfig::SchemaData::SchemaData(value_type &v, SchemaDescriptor const &d)
  : super_type(d), _value(((v.get() || (v.reset(new SchemaValue), true)), *v))
{
}

ts::Errata
TsLuaMetaConfig::SchemaData::load(lua_State *L)
{
  ts::Errata zret;
  lua_getfield(L, -1, descriptor.name.data());
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    zret.msg(ts::Severity::FATAL, "Schema load failed - not an OBJECT [table]");
  } else {
    // Walk the table.
    lua_pushnil(L);
    while (lua_next(L, -2)) {
      if (lua_isstring(L, -2)) {
        ts::string_view name = lua_tostring(L, -2);
        auto d = static_cast<SchemaDescriptor const &>(descriptor);
        auto spot = d._properties.find(name);
        if (spot != d._properties.end()) {
          // This is wrong - need to accumulate erratum, not replace.
          zret = (_value.*(spot->second)).load(L);
        } else {
        }
      }
      lua_pop(L, 1); // drop value, keep name for iteration.
    }
  }
  return zret;
}

TsLuaMetaConfig::MetaSchemaData::MetaSchemaData(MetaSchemaValue &v, MetaSchemaDescriptor const &d) : super_type(d), _value(v)
{
}

ts::Errata
TsLuaMetaConfig::MetaSchemaData::load(lua_State *L)
{
  ts::Errata zret;
  lua_getfield(L, -1, descriptor.name.data());
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    zret.msg(ts::Severity::FATAL, "MetaSchema load failed - not an OBJECT [table]");
  } else {
    // Walk the table.
    lua_pushnil(L);
    while (lua_next(L, -2)) {
      if (lua_isstring(L, -2)) {
        ts::string_view name = lua_tostring(L, -2);
        auto d = static_cast<MetaSchemaDescriptor const &>(descriptor);
        auto spot = d._properties.find(name);
        if (spot != d._properties.end()) {
          // This is wrong - need to accumulate erratum, not replace.
          zret = (_value.*(spot->second)).load(L);
        } else {
        }
      }
      lua_pop(L, 1); // drop value, keep name for iteration.
    }
  }
  return zret;
}

TsLuaMetaConfig::TsLuaMetaConfig() : METASCHEMA_DATA(meta_schema, METASCHEMA_DESCRIPTOR)
{
}

ts::Errata
TsLuaMetaConfig::load(lua_State *L)
{
  ts::Errata zret;
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  METASCHEMA_DATA.load(L);
  return zret;
}
