/** @file

  Plugin init

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

#include <stdio.h>
#include <limits>
#include "ts/ink_platform.h"
#include "ts/ink_file.h"
#include "ts/ParseRules.h"
#include "I_RecCore.h"
#include "I_Layout.h"
#include "InkAPIInternal.h"
#include "Main.h"
#include "Plugin.h"
#include "ink_cap.h"

static const char *plugin_dir = ".";

static const char OPT_PRIORITY[] = "priority";

typedef void (*init_func_t)(int argc, char *argv[]);

ink_thread_key PluginContext::THREAD_KEY;
GlobalPluginInfo* PluginManager::Internal_Plugin_Info;
GlobalPluginInfo* PluginManager::Default_Plugin_Info;

PluginManager pluginManager;

// Plugin registration vars
//
//    plugin_reg_list has an entry for each plugin
//      we've successfully been able to load
//    plugin_reg_current is used to associate the
//      plugin we're in the process of loading with
//      it struct.  We need this global pointer since
//      the API doesn't have any plugin context.  Init
//      is single threaded so we can get away with the
//      global pointer
//
DLL<PluginInfo> plugin_reg_list;
DLL<PluginInfo, PluginInfo::Link_disabled_link> plugin_disabled_list;

PluginInfo::PluginInfo()
  : _magic(MAGIC), dlh(NULL)
{
  _flags._all = 0; // clear all flags.
}

PluginInfo::~PluginInfo()
{
  // We don't support unloading plugins once they are successfully loaded, so assert
  // that we don't accidentally attempt this.
  ink_release_assert(!this->_flags._flag._registered == false);
  ink_release_assert(this->link.prev == NULL);
  if (dlh)
    dlclose(dlh);
}

PluginManager::PluginManager()
{
  ink_thread_key_create(&PluginContext::THREAD_KEY, NULL);
  ink_thread_setspecific(PluginContext::THREAD_KEY, NULL);
  // TS uses plugin mechanisms in various places and so needs a valid plugin info block
  // for them. This is it. This needs to be very early because threads get started before
  // PluginManager::init is called. This data is all effectively static so it can be done
  // earlier than configuration for actual plugins.
  Internal_Plugin_Info = new GlobalPluginInfo;
  Internal_Plugin_Info->_name = ats_strdup("TrafficServer Internal");
  Internal_Plugin_Info->_vendor = ats_strdup("Apache Software Foundation");
  Internal_Plugin_Info->_file_path = ats_strdup(".");
  Internal_Plugin_Info->_email = ats_strdup("dev@trafficserver.apache.org");
  Internal_Plugin_Info->_max_priority = std::numeric_limits<int>::max();
  Internal_Plugin_Info->_eff_priority = std::numeric_limits<int>::max();

  // For instances where real plugin info isn't available for various reasons.
  Default_Plugin_Info = new GlobalPluginInfo;
  Default_Plugin_Info->_name = ats_strdup("TrafficServer Default");
  Default_Plugin_Info->_vendor = ats_strdup("Apache Software Foundation");
  Default_Plugin_Info->_file_path = ats_strdup(".");
  Default_Plugin_Info->_email = ats_strdup("dev@trafficserver.apache.org");
  // These values are initially hardwired but should be updated in @c PluginManager::init
  // This constructor is called too early in startup to reliably access the configuration
  // values. Until then the primary purpose of this is to cover regression tests.
  // By the time the remap plugins are loaded the values should be updated.
  // Nevertheless, it would be prudent to keep these values in sync with the config defaults.
  Default_Plugin_Info->_max_priority = 1000;
  Default_Plugin_Info->_eff_priority = 800;

}

bool
PluginManager::load(int argc, char *argv[], bool continueOnError)
{
  char path[PATH_NAME_MAX + 1];
  void *handle;
  init_func_t init;
  PluginInfo* info = NULL;

  if (argc < 1) {
    return true;
  }
  ink_filepath_make(path, sizeof(path), plugin_dir, argv[0]);

  Note("loading plugin '%s'", path);

  for (PluginInfo *plugin_reg_temp = plugin_reg_list.head; plugin_reg_temp != NULL;
       plugin_reg_temp = (plugin_reg_temp->link).next) {
    if (strcmp(plugin_reg_temp->_file_path, path) == 0) {
      Warning("multiple loading of plugin %s", path);
      break;
    }
  }
  // elevate the access to read files as root if compiled with capabilities, if not
  // change the effective user to root
  {
    uint32_t elevate_access = 0;
    REC_ReadConfigInteger(elevate_access, "proxy.config.plugin.load_elevated");
    ElevateAccess access(elevate_access ? ElevateAccess::FILE_PRIVILEGE : 0);

    handle = dlopen(path, RTLD_NOW);
    if (!handle) {
      if (!continueOnError)
	Fatal("unable to load '%s': %s", path, dlerror());
      return false;
    }

    // Allocate a new registration structure for the
    //    plugin we're starting up
    info = new GlobalPluginInfo;
    info->_file_path = ats_strdup(path);
    info->dlh = handle;
    info->_max_priority = _default_priority;
    info->_eff_priority = _default_priority - _effective_priority_gap;

    init = reinterpret_cast<init_func_t>(dlsym(info->dlh, "TSPluginInit"));

    if (!init) {
      delete info;
      if (!continueOnError)
	Fatal("unable to find TSPluginInit function in '%s': %s", path, dlerror());
      return false; // this line won't get called since Fatal brings down ATS
    }

    // Process internal arguments
    for ( int i = 1, j = 1, n = argc ; i < n ; ++i) {
      if (argv[i][0] == '@') {
        if (0 == strncasecmp(argv[i]+1, OPT_PRIORITY, sizeof(OPT_PRIORITY)-1)) {
          int a = -1, b = -1, x;
          char const* parm = argv[i]+sizeof(OPT_PRIORITY);
          if ('=' == *parm) ++parm;
          x = sscanf(parm, "%d/%d", &a, &b);
          if (x == 1) {
            info->_max_priority = a;
            info->_eff_priority = std::max(0, a - _effective_priority_gap);
          } else if (x == 2) {
            info->_max_priority = std::max(a,b);
            info->_eff_priority = std::min(a,b);
          }
        }
        --argc;
      } else {
        if (i != j) argv[j] = argv[i];
        ++j;
      }
    }

    // Call the plugin to intialize.
    {
      PluginContext pc(info);
      init(argc, argv);
    }

  } // done elevating access

  if (info->_flags._flag._registered) {
    plugin_reg_list.push(info);
  } else {
    Fatal("plugin not registered by calling TSPluginRegister");
    return false; // this line won't get called since Fatal brings down ATS
  }

  return true;
}

char *
PluginManager::expand(char *arg)
{
  RecDataT data_type;
  char *str = NULL;

  if (*arg != '$')
    return NULL;

  // skip the $ character
  arg += 1;

  if (RecGetRecordDataType(arg, &data_type) == REC_ERR_OKAY) {
    switch (data_type) {
    case RECD_STRING: {
      RecString str_val;
      if (RecGetRecordString_Xmalloc(arg, &str_val) == REC_ERR_OKAY) {
	return static_cast<char*>(str_val);
      }
      break;
    }
    case RECD_FLOAT: {
      RecFloat float_val;
      if (RecGetRecordFloat(arg, &float_val) == REC_ERR_OKAY) {
	str = static_cast<char*>(ats_malloc(128));
	snprintf(str, 128, "%f", (float)float_val);
	return str;
      }
      break;
    }
    case RECD_INT: {
      RecInt int_val;
      if (RecGetRecordInt(arg, &int_val) == REC_ERR_OKAY) {
	str = static_cast<char*>(ats_malloc(128));
	snprintf(str, 128, "%ld", (long int)int_val);
	return str;
      }
      break;
    }
    case RECD_COUNTER: {
      RecCounter count_val;
      if (RecGetRecordCounter(arg, &count_val) == REC_ERR_OKAY) {
	str = static_cast<char*>(ats_malloc(128));
	snprintf(str, 128, "%ld", (long int)count_val);
	return str;
      }
      break;
    }
    default:
      break;
    }
  }

  Warning("plugin.config: unable to find parameter %s", arg);
  return NULL;
}

void
PluginManager::initForThread()
{
  PluginContext::setDefaultPluginInfo(Default_Plugin_Info);
  Debug("plugin", "Plugin Context %p [%d/%d] for thread %p [%" PRIx64 "]\n", Default_Plugin_Info, Default_Plugin_Info->_eff_priority, Default_Plugin_Info->_max_priority, this_ethread(), this_ethread()->tid);
}

bool
PluginManager::init(bool continueOnError)
{
  ats_scoped_str path;
  char line[1024], *p;
  char *argv[64];
  char *vars[64];
  int argc;
  int fd;
  int i;
  bool retVal = true;
  static bool INIT_ONCE = true;

  if (INIT_ONCE) {
    api_init();
    TSConfigDirGet();
    plugin_dir = TSPluginDirGet();
    INIT_ONCE = false;
  }

  REC_EstablishStaticConfigInt32(_default_priority, "proxy.config.plugin.priority.default");
  REC_EstablishStaticConfigInt32(_effective_priority_gap, "proxy.config.plugin.priority.effective_gap");

  // Get these updated now that the configured values are available.
  Default_Plugin_Info->_max_priority = _default_priority;
  Default_Plugin_Info->_eff_priority = _default_priority - _effective_priority_gap;

  path = RecConfigReadConfigPath(NULL, "plugin.config");
  fd = open(path, O_RDONLY);
  if (fd < 0) {
    Warning("unable to open plugin config file '%s': %d, %s", (const char *)path, errno, strerror(errno));
    return false;
  }

  while (ink_file_fd_readline(fd, sizeof(line) - 1, line) > 0) {
    argc = 0;
    p = line;

    // strip leading white space and test for comment or blank line
    while (*p && ParseRules::is_wslfcr(*p))
      ++p;
    if ((*p == '\0') || (*p == '#'))
      continue;

    // not comment or blank, so rip line into tokens
    while (1) {
      while (*p && ParseRules::is_wslfcr(*p))
        ++p;
      if ((*p == '\0') || (*p == '#'))
        break; // EOL

      if (*p == '\"') {
        p += 1;

        argv[argc++] = p;

        while (*p && (*p != '\"')) {
          p += 1;
        }
        if (*p == '\0') {
          break;
        }
        *p++ = '\0';
      } else {
        argv[argc++] = p;

        while (*p && !ParseRules::is_wslfcr(*p) && (*p != '#')) {
          p += 1;
        }
        if ((*p == '\0') || (*p == '#')) {
          break;
        }
        *p++ = '\0';
      }
    }

    for (i = 0; i < argc; i++) {
      vars[i] = this->expand(argv[i]);
      if (vars[i]) {
        argv[i] = vars[i];
      }
    }

    retVal = this->load(argc, argv, continueOnError);

    for (i = 0; i < argc; i++)
      ats_free(vars[i]);
  }

  close(fd);

  // Notification that plugin loading has finished.
  APIHook *hook = lifecycle_hooks->get(TS_LIFECYCLE_PLUGINS_LOADED_HOOK);
  while (hook) {
    hook->invoke(TS_EVENT_LIFECYCLE_PLUGINS_LOADED, NULL);
    hook = hook->next();
  }

  return retVal;
}

PluginInfo const*
PluginManager::find(char const* name)
{
  for ( PluginInfo* pi = plugin_reg_list.head ; NULL != pi ; pi = pi->link.next ) {
    if (0 == strcasecmp(name, pi->_name)) return pi;
  }
  return NULL;
}

void
PluginManager::enable(PluginInfo const* tag, bool enable_p)
{
  PluginInfo* pi = const_cast<PluginInfo*>(tag);
  pi->_flags._flag._disabled = enable_p ? 0 : 1;
  if (enable_p) {
    if (plugin_disabled_list.in(pi)) plugin_disabled_list.remove(pi);
  } else {
    if (!plugin_disabled_list.in(pi)) plugin_disabled_list.push(pi);
  }
}

PluginManager::disabled_iterator
PluginManager::disabled_begin() const
{
  return plugin_disabled_list.head;
}

PluginManager::disabled_iterator
PluginManager::disabled_end() const
{
  return NULL;
}
