/** @file

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

#include "LuaSNIConfig.h"
#include <cstring>
#include "ts/Diags.h"
#include "P_SNIActionPerformer.h"
#include "tsconfig/Errata.h"

TsConfigDescriptor LuaSNIConfig::desc = {TsConfigDescriptor::Type::ARRAY, "Array", "Item vector", "Vector"};
TsConfigArrayDescriptor LuaSNIConfig::DESCRIPTOR(LuaSNIConfig::desc);
TsConfigDescriptor LuaSNIConfig::Item::FQDN_DESCRIPTOR = {TsConfigDescriptor::Type::STRING, "String", TS_fqdn,
                                                          "Fully Qualified Domain Name"};
TsConfigDescriptor LuaSNIConfig::Item::DISABLE_h2_DESCRIPTOR = {TsConfigDescriptor::Type::BOOL, "Boolean", TS_disable_H2,
                                                                "Disable H2"};
TsConfigEnumDescriptor LuaSNIConfig::Item::LEVEL_DESCRIPTOR = {TsConfigDescriptor::Type::ENUM,
                                                               "enum",
                                                               "Level",
                                                               "Level for client verification",
                                                               {{"NONE", 0}, {"MODERATE", 1}, {"STRICT", 2}}};
TsConfigDescriptor LuaSNIConfig::Item::TUNNEL_DEST_DESCRIPTOR = {TsConfigDescriptor::Type::STRING, "String", TS_tunnel_route,
                                                                 "tunnel route destination"};
TsConfigDescriptor LuaSNIConfig::Item::FORWARD_DEST_DESCRIPTOR = {TsConfigDescriptor::Type::STRING, "String", TS_forward_route,
                                                                 "tunnel route destination"};
TsConfigDescriptor LuaSNIConfig::Item::IP_ALLOW_DESCRIPTOR = {TsConfigDescriptor::Type::STRING, "String", TS_ip_allow,
                                                              "Client IP allowed for this communication"};
TsConfigDescriptor LuaSNIConfig::Item::CLIENT_CERT_DESCRIPTOR = {TsConfigDescriptor::Type::STRING, "String", TS_client_cert,
                                                                 "Client certificate to present to the next hop server"};
TsConfigDescriptor LuaSNIConfig::Item::CLIENT_KEY_DESCRIPTOR = {TsConfigDescriptor::Type::STRING, "String", TS_client_key,
                                                                 "Client key corresponding to certificate to present to the next hop server"};
TsConfigEnumDescriptor LuaSNIConfig::Item::VERIFY_NEXT_SERVER_DESCRIPTOR = {TsConfigDescriptor::Type::ENUM,
                                                                            "enum",
                                                                            "Level",
                                                                            "Level for server certificate verification",
                                                                            {{"NONE", 0}, {"MODERATE", 1}, {"STRICT", 2}}};

TsConfigEnumDescriptor LuaSNIConfig::Item::VERIFY_SERVER_POLICY_DESCRIPTOR = { TsConfigDescriptor::Type::ENUM,
                                                                    "enum",
                                                                    "Policy",
                                                                    "How the verification should be enforced",
                                                                    {{"DISABLED", 0}, {"PERMISSIVE", 1}, {"ENFORCED", 2}}};
TsConfigEnumDescriptor LuaSNIConfig::Item::VERIFY_SERVER_PROPERTIES_DESCRIPTOR = { TsConfigDescriptor::Type::ENUM,
                                                                        "enum",
                                                                        "Property",
                                                                        "Properties to be verified",
                                                                        {{"NONE", 0}, {"SIGNATURE", 0x1}, {"NAME", 0x2}, {"ALL", 0x3}}};
                                                                       

TsConfigEnumDescriptor LuaSNIConfig::Item::TLS_PROTOCOLS_DESCRIPTOR = {TsConfigDescriptor::Type::ENUM, "enum", "Protocols", "Enabled TLS protocols", {{"TLSv1", 0}, {"TLSv1_1", 1}, {"TLSv1_2", 2}, {"TLSv1_3", 3}}};

ts::Errata
LuaSNIConfig::loader(lua_State *L)
{
  ts::Errata zret;
  //  char buff[256];
  //  int error;

  lua_getfield(L, LUA_GLOBALSINDEX, "server_config");
  int l_type = lua_type(L, -1);

  switch (l_type) {
  case LUA_TTABLE: // this has to be a multidimensional table
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
      l_type = lua_type(L, -1);
      if (l_type == LUA_TTABLE) { // the item should be table
        // new Item
        LuaSNIConfig::Item item;
        zret = item.loader(L);
        items.push_back(item);
      } else {
        zret.push(ts::Errata::Message(0, 0, "Invalid Entry at SNI config"));
      }
      lua_pop(L, 1);
    }
    break;
  case LUA_TSTRING:
    Debug("ssl", "string value %s", lua_tostring(L, -1));
    break;
  default:
    zret.push(ts::Errata::Message(0, 0, "Invalid Lua SNI Config"));
    Debug("ssl", "Please check the format of your server_name_config");
    break;
  }

  return zret;
}

ts::Errata
LuaSNIConfig::Item::loader(lua_State *L)
{
  ts::Errata zret;
  //-1 will contain the subarray now (since it is a value in the main table))
  lua_pushnil(L);
  while (lua_next(L, -2)) {
    if (lua_type(L, -2) != LUA_TSTRING) {
      Debug("ssl", "string keys expected for entries in %s", SSL_SERVER_NAME_CONFIG);
    }
    const char *name = lua_tostring(L, -2);
    if (!strncmp(name, TS_fqdn, strlen(TS_fqdn))) {
      FQDN_CONFIG.loader(L);
    } else if (!strncmp(name, TS_disable_H2, strlen(TS_disable_H2))) {
      DISABLEH2_CONFIG.loader(L);
    } else if (!strncmp(name, TS_verify_client, strlen(TS_verify_client))) {
      VERIFYCLIENT_CONFIG.loader(L);
    } else if (!strncmp(name, TS_verify_origin_server, strlen(TS_verify_origin_server))) {
      VERIFY_NEXT_SERVER_CONFIG.loader(L);
    } else if (!strncmp(name, TS_verify_server_policy, strlen(TS_verify_server_policy))) {
      VERIFY_SERVER_POLICY_CONFIG.loader(L);
    } else if (!strncmp(name, TS_verify_server_properties, strlen(TS_verify_server_properties))) {
      VERIFY_SERVER_PROPERTIES_CONFIG.loader(L);
    } else if (!strncmp(name, TS_client_cert, strlen(TS_client_cert))) {
      CLIENT_CERT_CONFIG.loader(L);
    } else if (!strncmp(name, TS_client_key, strlen(TS_client_key))) {
      CLIENT_KEY_CONFIG.loader(L);
    } else if (!strncmp(name, TS_tunnel_route, strlen(TS_tunnel_route))) {
      TUNNEL_DEST_CONFIG.loader(L);
    } else if (!strncmp(name, TS_forward_route, strlen(TS_forward_route))) {
      FORWARD_DEST_CONFIG.loader(L);
      this->tunnel_decrypt = true;
    } else if (!strncmp(name, TS_ip_allow, strlen(TS_ip_allow))) {
      IP_ALLOW_CONFIG.loader(L);
    } else if (0 == strncmp(name, TS_tls_protocols.data(), TS_tls_protocols.size())) {
      TLS_PROTOCOL_SET_CONFIG.loader(L);
      InitializeNegativeMask(tls_valid_protocols_in);
    } else {
      zret.push(ts::Errata::Message(0, 0, "Invalid Entry at SNI config"));
    }
    lua_pop(L, 1);
  }
  return zret;
}

ts::Errata
LuaSNIConfig::registerEnum(lua_State *L)
{
  ts::Errata zret;
  lua_newtable(L);
  lua_setglobal(L, "LevelTable");
  int i = start;
  LUA_ENUM(L, "NONE", i++);
  LUA_ENUM(L, "MODERATE", i++);
  LUA_ENUM(L, "STRICT", i++);

  lua_newtable(L);
  lua_setglobal(L, "PolicyTable");
  LUA_ENUM(L, "DISABLED", 0);
  LUA_ENUM(L, "PERMISSIVE", 1);
  LUA_ENUM(L, "ENFORCED", 2);

  lua_newtable(L);
  lua_setglobal(L, "PropertyTable");
  LUA_ENUM(L, "NONE", 0);
  LUA_ENUM(L, "SIGNATURE", 1);
  LUA_ENUM(L, "NAME", 2);
  LUA_ENUM(L, "ALL", 3);

  lua_newtable(L);
  lua_setglobal(L, "TLSVersionTable");
  LUA_ENUM(L, "TLSv1", 0);
  LUA_ENUM(L, "TLSv1_1", 1);
  LUA_ENUM(L, "TLSv1_2", 2);
  LUA_ENUM(L, "TLSV1_3", 3);

  return zret;
}

void
LuaSNIConfig::Item::InitializeNegativeMask(uint8_t valid_protocols)
{
  if (valid_protocols > 0) {
    protocol_unset = false;
    protocol_mask = TLSValidProtocols::max_mask;
    uint8_t i = 0;
    for (; static_cast<LuaSNIConfig::TLS_Protocols>(i) <= LuaSNIConfig::TLS_Protocols::TLSv1_3; i++) {
      if ((1<<i) & valid_protocols) {
        switch (static_cast<LuaSNIConfig::TLS_Protocols>(i)) {
        case LuaSNIConfig::TLS_Protocols::TLSv1:
          protocol_mask &= ~SSL_OP_NO_TLSv1;
          break;
        case LuaSNIConfig::TLS_Protocols::TLSv1_1:
          protocol_mask &= ~SSL_OP_NO_TLSv1_1;
          break;
        case LuaSNIConfig::TLS_Protocols::TLSv1_2:
          protocol_mask &= ~SSL_OP_NO_TLSv1_2;
          break;
        case LuaSNIConfig::TLS_Protocols::TLSv1_3:
#ifdef SSL_OP_NO_TLSv1_3
          protocol_mask &= ~SSL_OP_NO_TLSv1_3;
#endif
          break;
        }
      }
    }
  }
}

