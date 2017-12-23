/** @file

    Base code for TsLuaConfig library.

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

#include <tsconfig/TsLuaConfig.h>
#include <sys/stat.h>
#include <tsconfig/TsErrataUtil.h>
#include "luajit/src/lua.hpp"

TsLuaConfig::~TsLuaConfig()
{
}

ts::Errata
TsLuaConfigStringData::load(lua_State *L)
{
  ts::Errata zret;

  return zret;
}

ts::Errata
TsLuaConfig::load(const char *path)
{
  struct stat info;
  if (stat(path, &info) == -1 && errno == ENOENT) {
    return {};
  }

  lua_State *L = lua_open();
  luaL_openlibs(L);
  if (luaL_loadfile(L, path)) {
    lua_pop(L, 1);
    return {};
  }

  if (lua_pcall(L, 0, 0, 0)) {
    lua_pop(L, 1);
    return {};
  }

  return this->load(L);
}

ts::Errata
TsLuaConfigObjectData::load(lua_State* L) {
  ts::Errata zret;
  lua_getfield(L, -1, descriptor.name.data());
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    return ts::Errata::Message{ts::msg::WARN, "Schema load failed - not an OBJECT [table]"};
  }
  // Walk the table.
  lua_pushnil(L);
  while (lua_next(L, -2)) {
    if (lua_isstring(L, -2)) {
      ts::string_view name = lua_tostring(L, -2);
      _value._names.emplace_back(std::string{name.data(), name.size()});
      name = _value._names.back(); // update to memory that will stay around.
      _value.make(name)->load(L);
    }
    lua_pop(L, 1); // drop value, keep name for iteration.
  }
  return zret;
}
