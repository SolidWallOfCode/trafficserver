/** @file

  Example plugin for plugin priority.

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

/* priority-plugin.c: an example plugin to demonstrate the lifecycle hooks.
 *                    of response body content
 */

#include <stdio.h>
#include <unistd.h>
#include <ts/ts.h>

#define PNAME "priority-plugin"

int
CheckVersion()
{
  int major_ts_version = TSTrafficServerVersionGetMajor();
  /* Need at least TS 6.1.0 */
  return major_ts_version > 6 ||  (major_ts_version == 6 && (TSTrafficServerVersionGetMinor() >= 1));
}

void
TSPluginInit(int argc, const char *argv[])
{
  TSPluginRegistrationInfo info;
  //  TSCont cb;

  (void)argc;
  (void)argv;

  info.plugin_name = PNAME;
  info.vendor_name = "Apache Software Foundation";
  info.support_email = "dev@trafficserver.apache.org";

  if (TSPluginRegister(TS_SDK_VERSION_3_0, &info) != TS_SUCCESS) {
    TSError("[" PNAME "] Plugin registration failed.");
  } else if  (!CheckVersion()) {
    TSError("[" PNAME "] Plugin requires Traffic Server 6.1.0 or later");
  } else {
    //  cb = TSContCreate(CallbackHandler, NULL);
    TSDebug(PNAME, "online");
    return;
  }
  TSError("[" PNAME "] Unable to initialize plugin (disabled).");
}
