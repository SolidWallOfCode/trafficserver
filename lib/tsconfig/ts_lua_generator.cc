/** @file

    TS Lua Config Generator.

    This reads TS Lua configuration description files and generates the infrastructure to process
    the corresponding run time configuration files.

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

#include <getopt.h>
#include <ts/BufferWriter.h>
#include <ts/BufferWriterFormat.h>
#include <ts/TextView.h>
#include <tsconfig/TsErrataUtil.h>

#include <iostream>
#include <fstream>
#include <map>
#include <chrono>
#include <ctype.h>

#include <tsconfig/TsLuaConfig.h>

// Temporary - this should be promoted to BufferWriter.h
namespace std
{
std::ostream &
operator<<(std::ostream &s, ts::FixedBufferWriter const &w)
{
  s.write(w.data(), w.size());
  return s;
}
}

namespace
{
// getopt return values. Selected to avoid collisions with short arguments.
static constexpr int OPT_HELP = 257; ///< Help message.
static constexpr int OPT_OUT  = 258; ///< Out source file.
static constexpr int OPT_HDR  = 259; ///< Name of the output header file.

const std::map<int, ts::string_view> OPT_DESCRIPTIONS = {{OPT_HELP, "Print the usage message"},
                                                         {OPT_OUT, "Generated source file."},
                                                         {OPT_HDR, "Generated header file."}};
}

using ts::Severity;
using ts::bwprint;

int
main(int argc, char **argv)
{
  static constexpr option OPTIONS[] = {
    {"header", required_argument, 0, OPT_HDR},
    {"out", required_argument, 0, OPT_OUT},
    {"help", 0, 0, OPT_HELP},
    {0, 0, 0, 0} // required terminator.
  };

  ts::Errata zret;
  int idx;
  ts::LocalBufferWriter<1024> msg;
  TsLuaMetaConfig config;
  std::string text_out;

  std::string schema_file_path;
  std::string hdr_file_path;
  std::string out_file_path;

  while (-1 != (idx = getopt_long_only(argc, argv, "", OPTIONS, &idx))) {
    switch (idx) {
    case '?':
    case OPT_HELP: {
      auto cmd = ts::TextView(argv[0], strlen(argv[0])).take_suffix_at('/');
      msg << cmd << " [options] schema-file";
      for (auto const &item : OPTIONS) {
        if (!item.name)
          break;
        ts::string_view txt{"*no description available*"};
        auto spot = OPT_DESCRIPTIONS.find(item.val);
        if (spot != OPT_DESCRIPTIONS.end()) {
          txt = spot->second;
        }
        msg << "\n    --" << item.name << ": " << txt;
      }
      zret.msg(Severity::INFO, msg.view());
      break;
    }
    case OPT_OUT:
      out_file_path = optarg;
      break;
    case OPT_HDR:
      hdr_file_path = optarg;
      break;
    }
  }

  if (zret.size()) {
    std::cout << bwprint(text_out, "{}", zret);
    return 0;
  }

  if (argc != optind + 1) {
    zret.msg(Severity ::FATAL, "Needed {} arguments, only {} provided", optind, argc-1);
    std::cout << bwprint(text_out, "{}", zret);
    return 1;
  }

  schema_file_path = argv[optind];

  if (out_file_path.empty()) {
    ts::TextView p{schema_file_path};
    p.split_suffix_at('.');
    out_file_path.reserve(p.size() + 4);
    out_file_path.assign(p.data(), p.size());
    out_file_path += ".cc";
  }
  if (hdr_file_path.empty()) {
    ts::TextView p{out_file_path};
    p.split_suffix_at('.');
    hdr_file_path.reserve(p.size() + 3);
    hdr_file_path.assign(p.data(), p.size());
    hdr_file_path += ".h";
  }

  std::ofstream out_file(out_file_path, std::ios::out | std::ios::trunc);
  std::ofstream hdr_file(hdr_file_path, std::ios::out | std::ios::trunc);

  std::cout << "Loading config " << schema_file_path << " generating " << out_file_path << " and " << hdr_file_path << std::endl;

  zret = config.load("lua-config-meta-schema.lua");

  bwprint(text_out, "#pragma once\n// File generated from {} at {timestamp}\n\n", schema_file_path);
  hdr_file << text_out;

  // Start generating.
  bwprint(text_out, "class {}\n{{\n}};\n", config.meta_schema._class);
  out_file << text_out;
  return 0;
}
