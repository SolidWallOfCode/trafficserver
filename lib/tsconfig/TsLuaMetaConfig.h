/** @file

  TsLuaConfig Meta schema declarations.

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

#if !defined(META_TS_LUA_CONFIG_H)
#define META_TS_LUA_CONFIG_H

#include <tsconfig/TsLuaConfigValueData.h>
#include <tsconfig/forward_TsLuaConfigValueDescriptor.h>

// Representation of the schema.
class TsLuaMetaConfig : public TsLuaConfig
{
  using self_type  = TsLuaMetaConfig;
  using super_type = TsLuaConfig;

public:
  // Types from 'defines' property.
  enum ValueType { NIL = 0, BOOLEAN = 1, STRING = 2, INTEGER = 3, NUMBER = 4, OBJECT = 5, ARRAY = 6, ENUM = 7 };

  struct EnumType {
    std::string _typeName;
    std::string _global;
  };

  class SchemaValue;
  class SchemaDescriptor;
  class SchemaData;
  using SchemaProperties = TsLuaConfigObjectType<SchemaData, SchemaDescriptor>;

  class SchemaData : public TsLuaConfigValueData
  {
    using super_type = TsLuaConfigValueData;
    using self_type = SchemaData;
  public:
    using value_type = std::unique_ptr<SchemaValue>;

    SchemaData(SchemaValue &v, SchemaDescriptor const &d);
    SchemaData(value_type &v, SchemaDescriptor const &d);

    ts::Errata load(lua_State *L) override;

  private:
    SchemaValue &_value;
  };

  // The basic schema type.
  class SchemaValue
  {
    using self_type = SchemaValue;
  public:
    SchemaValue();

    std::string _description;
    TsLuaConfigStringData DESCRIPTION_DATA;
    static const TsLuaConfigValueDescriptor DESCRIPTION_DESCRIPTOR;

    std::string _class;
    TsLuaConfigStringData CLASS_DATA;
    static const TsLuaConfigValueDescriptor CLASS_DESCRIPTOR;

    std::string _type;
    TsLuaConfigStringData TYPE_DATA;
    static const TsLuaConfigValueDescriptor TYPE_DESCRIPTOR;

    SchemaProperties _properties;
    TsLuaConfigObjectData PROPERTIES_DATA;
    using PropertyDescriptor = TsLuaConfigObjectDescriptor<self_type>;
    static const PropertyDescriptor PROPERTIES_DESCRIPTOR;
  };

  class MetaSchemaDescriptor;

    // Metaschema class, a specialized schema.
  class MetaSchemaValue : public SchemaValue
  {
  public:
    MetaSchemaValue();

    std::string p_schema;
    TsLuaConfigStringData P_SCHEMA_DATA;
    static const TsLuaConfigValueDescriptor P_SCHEMA_DESCRIPTOR;

  } meta_schema;

  class MetaSchemaData : public TsLuaConfigValueData
  {
    using super_type = TsLuaConfigValueData;

  public:
    MetaSchemaData(MetaSchemaValue &v, MetaSchemaDescriptor const &d);

    ts::Errata load(lua_State *L) override;

  private:
    MetaSchemaValue &_value;
  } METASCHEMA_DATA;

  static const SchemaDescriptor SCHEMA_DESCRIPTOR;
  static const MetaSchemaDescriptor METASCHEMA_DESCRIPTOR;

  TsLuaMetaConfig();

  /// Internal load, called from super class.
  ts::Errata load(lua_State *L);
  using super_type::load;
};

// Save to bring back when we start playing with variants.
#if 0
    class GlobalsProperty {
    public:
      using nil_type = TsLuaConfigTypeVariant<TsLuaConfigNil, TsLuaConfigNilDescriptor>;
      using T0 = TsLuaConfigTypeVariant<TsLuaConfigObject<GlobalsType>, GlobalsDescriptor>;
      using T1 = TsLuaConfigTypeVariant<std::string, TsLuaConfigStringDescriptor>;

    private:
      char _store[ts::max(sizeof(nil_type), sizeof(T0), sizeof(T1))];
    } _globals;
    using GlobalsValue = TsLuaConfigObject<GlobalsProperty>;
    using GlobalsDescriptor = TsLuaConfigObjectDescriptor<GlobalsValue>;
    GlobalsData : public TsLuaConfigValueData {
      using super_type = TsLuaConfigValueData;
    public:
      GlobalsData(GlobalsValue& v, GlobalsDescriptor const& d);

      ts::Errata load(lua_State* L) override;
    private:
      GlobalsValue& _value;
    } GLOBALS_DATA;
#endif

#endif // include guard
