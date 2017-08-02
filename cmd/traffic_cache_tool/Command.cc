/** @file

    Nest commands (for command line processing).

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

#include "Command.h"
#include <new>
#include <algorithm>
#include <string.h>
#include <sstream>

namespace ts
{
int CommandTable::_opt_idx = 0;

static const std::string LEADING(":-  ");

// Error message functions.
ts::Errata
ERR_COMMAND_TAG_NOT_FOUND(char const *tag)
{
  std::ostringstream s;
  s << "Command tag " << tag << " not found";
  return ts::Errata(s.str());
}

CommandTable::Command::Command(std::string const &name, std::string const &help) : _name(name), _help(help)
{
}

CommandTable::Command::Command(std::string const &name, std::string const &help, Action const &f)
  : _name(name), _help(help), _style(LEAF)
{
  _action._a = f;
}

CommandTable::Command::Command(std::string const &name, std::string const &help, NullaryAction const &f)
  : _name(name), _help(help), _style(NO_ARGS)
{
  _action._na = f;
}

CommandTable::Command::Command(Command && that) : _name(std::move(that._name)), _help(std::move(that._help)), _style(that._style) {
  switch (_style) {
  case SUPER: break;
  case NO_ARGS: _action._na = std::move(that._action._na); break;
  case LEAF: _action._a = std::move(that._action._a); break;
  }
}

CommandTable::Command::~Command() {
  switch (_style) {
  case SUPER: break;
  case NO_ARGS: _action._na.~NullaryAction(); break;
  case LEAF: _action._a.~Action(); break;
  }
}

# if 0
auto
CommandTable::Command::set(CommandFunction const &f) -> self &
{
  _func = f;
  return *this;
}
#endif

CommandTable::Command &
CommandTable::Command::subCommand(std::string const &name, std::string const &help, Action const &f)
{
  _group.push_back(Command(name, help, f));
  return _group.back();
}

CommandTable::Command &
CommandTable::Command::subCommand(std::string const &name, std::string const &help, NullaryAction const &f)
{
  _group.push_back(Command(name, help, f));
  return _group.back();
}

auto
CommandTable::Command::subCommand(std::string const &name, std::string const &help) -> self &
{
  _group.push_back(Command(name, help));
  return _group.back();
}

ts::Errata
CommandTable::Command::invoke(int argc, char *argv[])
{
  ts::Errata zret;

  if (LEAF == _style) {
    zret = _action._a(argc - CommandTable::_opt_idx, argv + CommandTable::_opt_idx);
  } else if (CommandTable::_opt_idx >= argc || argv[CommandTable::_opt_idx][0] == '-') {
    if (NO_ARGS == _style) {
      zret = _action._na();
    } else {
      std::ostringstream s;
      s << "Incomplete command, additional keyword required";
      s << std::endl;
      this->helpMessage(0, nullptr, s, LEADING);
      zret.push(s.str());
    }
  } else {
    char const *tag = argv[CommandTable::_opt_idx];
    auto spot       = std::find_if(_group.begin(), _group.end(),
                             [tag](CommandGroup::value_type const &elt) { return 0 == strcasecmp(tag, elt._name.c_str()); });
    if (spot != _group.end()) {
      ++CommandTable::_opt_idx;
      zret = spot->invoke(argc, argv);
    } else {
      zret = ERR_COMMAND_TAG_NOT_FOUND(tag);
    }
  }
  return zret;
}

void
CommandTable::Command::helpMessage(int argc, char *argv[], std::ostream &out, std::string const &prefix) const
{
  if (CommandTable::_opt_idx >= argc || argv[CommandTable::_opt_idx][0] == '-') {
    // Tail of command keywords, start listing
    if (_name.empty()) { // root command group, don't print for that.
      for (Command const &c : _group)
        c.helpMessage(argc, argv, out, prefix);
    } else {
      out << prefix << _name << ": " << _help << std::endl;
      for (Command const &c : _group)
        c.helpMessage(argc, argv, out, "  " + prefix);
    }
  } else {
    char const *tag = argv[CommandTable::_opt_idx];
    auto spot       = std::find_if(_group.begin(), _group.end(),
                             [tag](CommandGroup::value_type const &elt) { return 0 == strcasecmp(tag, elt._name.c_str()); });
    if (spot != _group.end()) {
      ++CommandTable::_opt_idx;
      spot->helpMessage(argc, argv, out, prefix);
    } else {
      out << ERR_COMMAND_TAG_NOT_FOUND(tag) << std::endl;
    }
  }
}

CommandTable::CommandTable()
{
}

auto
CommandTable::add(std::string const &name, std::string const &help) -> Command &
{
  return _top.subCommand(name, help);
}

auto
CommandTable::add(std::string const &name, std::string const &help, Action const &f) -> Command &
{
  return _top.subCommand(name, help, f);
}

auto
CommandTable::add(std::string const &name, std::string const &help, NullaryAction const &f) -> Command &
{
  return _top.subCommand(name, help, f);
}

ts::Errata
CommandTable::invoke(int argc, char *argv[])
{
  return _top.invoke(argc, argv);
}

// This is basically cloned from invoke(), need to find how to do some unification.
void
CommandTable::helpMessage(int argc, char *argv[]) const
{
  _opt_idx = 0;
  std::cerr << "Command tree" << std::endl;
  _top.helpMessage(argc, argv, std::cerr, LEADING);
}
}
