'''
Add outbound address round-robin functionality to ATS
'''
#  Licensed to the Apache Software Foundation (ASF) under one
#  or more contributor license agreements.  See the NOTICE file
#  distributed with this work for additional information
#  regarding copyright ownership.  The ASF licenses this file
#  to you under the Apache License, Version 2.0 (the
#  "License"); you may not use this file except in compliance
#  with the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

import os
Test.Summary = '''
Verify outbound addresses are used in round robin order when
connecting to the upstream server
'''

# Needs Curl
Test.SkipUnless(
    Condition.HasProgram("curl", "curl needs to be installed on system for this test to work"),
)
Test.ContinueOnFail = True

# Note: We run two ATS processes and make the first ATS go the server via
# the second ATS. Then we can read the logs of the upstream ATS and verify
# the outgoing IP address of the first ATS
# Client ---> ATS1 ---> ATS2 ---> Server

# Define default ATS
ts = Test.MakeATSProcess("ts")
ts2 = Test.MakeATSProcess("ts2")
server = Test.MakeOriginServer("server")

#**testname is required**
testName = ""
request_header1 = {"headers": "GET / HTTP/1.1\r\nHost: www.example.com\r\n\r\n", "timestamp": "1469733493.993", "body": ""}
response_header1 = {"headers": "HTTP/1.1 200 OK\r\nConnection: close\r\n", "timestamp": "1469733493.993", "body": "xxx"}
server.addResponse("sessionlog.json", request_header1, response_header1)

# ATS Configuration for the upstream ATS
ts2.Disk.records_config.update({
    'proxy.config.diags.debug.enabled': 1,
    'proxy.config.diags.debug.tags': 'http',
})

ts2.Disk.remap_config.AddLine(
    'map / http://127.0.0.1:{0}'.format(server.Variables.Port)
)
ts2.Disk.logging_config.AddLines(
     '''custom = format {
  Format = "client-ip=%<chi> %<cqtx>"
}
 
log.ascii {
  Format = custom,
  Filename = 'ts2_logs'
}
'''.split("\n")
)

# ATS Configuration for the first ATS
ts.Disk.records_config.update({
    'proxy.config.diags.debug.enabled': 1,
    'proxy.config.diags.debug.tags': 'http',
    'proxy.local.outgoing_ip_to_bind': '127.0.0.10 127.0.0.11 127.0.0.12',
    # Do not reuse connections to the upstream ATS so that we can verify
    # that outbound addresses are used in round robin
    'proxy.config.http.server_session_sharing.match': 'none',
})

ts.Disk.remap_config.AddLine(
    'map / http://127.0.0.1:{0}'.format(ts2.Variables.port)
)
ts.Disk.logging_config.AddLines(
     '''custom = format {
  Format = " %<cqah> %<pssc> %<psah> %<ssah> %<pqah> %<cssah> "
}

log.ascii {
  Format = custom,
  Filename = 'ts1_logs'
}
'''.split("\n")
)

# Make a few requests to ATS.
# ATS1 should in turn use different outbound addresses
# Make 6 requests to test the wraparound
for x in range(6):
  tr = Test.AddTestRun()
  if x == 0:
    tr.Processes.Default.StartBefore(server, ready=When.PortOpen(server.Variables.Port))
    tr.Processes.Default.StartBefore(ts2, ready=When.PortOpen(ts2.Variables.port))
    tr.Processes.Default.StartBefore(Test.Processes.ts, ready=When.PortOpen(ts.Variables.port))

  tr.Processes.Default.Command = 'curl -s -D - -v --ipv4 -H "Host: www.example.com" http://localhost:{port}/'.\
    format(port=ts.Variables.port)
  tr.Processes.Default.ReturnCode = 0
  tr.StillRunningAfter = ts
  tr.StillRunningAfter = ts2
  tr.StillRunningAfter = server

# Delay to allow TS to flush report to disk
# Parse the upstream ATS (ts2) log files to verify client IP
# used by downstream TS
tr = Test.AddTestRun()
tr.DelayStart = 10
tr.Processes.Default.Command = 'cat {0}'.format(
    os.path.join(ts2.Variables.LOGDIR, 'ts2_logs.log'))
tr.Processes.Default.ReturnCode = 0
tr.Processes.Default.Streams.stdout = "gold/client-ips.gold"
