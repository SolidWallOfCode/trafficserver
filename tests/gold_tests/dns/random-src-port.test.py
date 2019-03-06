'''
Test cached responses and requests with bodies
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
Test DNS source port randomization. I.e. each DNS request uses a new
ephemeral port
We make ATS do many DNS lookups and check whether it is using different
ports on each or most of the DNS requests.
'''

Setup.Copy("baddnsserver.py")  # copy the file to _sandbox

# Needs Curl
Test.SkipUnless(
    Condition.HasProgram("curl", "curl needs to be installed on system for this test to work"),
    Condition.HasProgram("nc", "nc needs to be installed on system for this test to work")
)
Test.ContinueOnFail = False

# Define default ATS
ts = Test.MakeATSProcess("ts")
server = Test.MakeOriginServer("server")

#**testname is required**
testName = ""
request_header1 = {"headers": "GET / HTTP/1.1\r\nHost: www.example.com\r\n\r\n", "timestamp": "1469733493.993", "body": ""}
response_header1 = {"headers": "HTTP/1.1 200 OK\r\nConnection: close\r\nCache-Control: max-age=300\r\n\r\n", "timestamp": "1469733493.993", "body": "xxx"}
server.addResponse("sessionlog.json", request_header1, response_header1)

# ATS Configuration
ts.Disk.records_config.update({
    'proxy.config.diags.debug.enabled': 1,
    'proxy.config.diags.debug.tags': 'http|dns.*',
    'proxy.config.http.response_via_str': 3,
    'proxy.config.http.cache.http': 1,
    'proxy.config.http.wait_for_cache': 1,
    # Send DNS request to our DNS server and set round robin = 0, so
    # that all requests go to our fake DNS server rather than
    # the actual nameservers in /etc/resolv.conf too
    'proxy.config.dns.nameservers': '127.0.0.1:5300',
    'proxy.config.dns.round_robin_nameservers': 0,
    # timeout aggressively so that we can generate more requests
    'proxy.config.dns.retries': 10,
    'proxy.config.dns.lookup_timeout': 2,
})

# TS will try to lookup garbagedomain.com in DNS
ts.Disk.remap_config.AddLine(
    'map http://www.example.com/ http://garbagedomain.com/'
)

# We don't care about the response to the client
tr = Test.AddTestRun()
# Start our DNS server that doesn't respond but just checks the source ports
# for randomness/spread
baddnsserver = tr.Processes.Process("baddnsserver","python baddnsserver.py -p 5300",returncode=None)
tr.Processes.Default.StartBefore(baddnsserver)
tr.Processes.Default.StartBefore(server)
tr.Processes.Default.StartBefore(Test.Processes.ts, ready=1)
tr.Processes.Default.Command = 'curl --max-time 28 -s -D - -v --ipv4 -H "Host: www.example.com" http://localhost:{port}/xxx'.format(port=ts.Variables.port)
tr.Processes.Default.ReturnCode = 28
tr.Processes.baddnsserver.Streams.stdout = Testers.ContainsExpression("DNS requests had randomness in their source ports", "randomness check")
tr.StillRunningAfter = ts
