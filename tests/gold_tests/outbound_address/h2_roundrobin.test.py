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
connecting to the upstream server for H2 clients
'''

# Needs Curl
Test.SkipUnless(
    Condition.HasProgram("curl", "Curl need to be installed on system for this test to work"),
    Condition.HasCurlFeature('http2'),
)
Test.ContinueOnFail = True

# Note: We run two ATS processes and make the first ATS go the server via
# the second ATS. Then we can read the logs of the upstream ATS and verify
# the outgoing IP address of the first ATS
# Client ---> ATS1 ---> ATS2 ---> Server

# Define two ATS processes
# ts2 is the upstream ATS which will talk to the origin server
ts2 = Test.MakeATSProcess("ts2", select_ports=True, enable_tls=True)
ts2.addSSLfile("ssl/server.pem")
ts2.addSSLfile("ssl/server.key")

server = Test.MakeOriginServer("server", ssl=True)

#**testname is required**
testName = ""
request_header1 = {"headers": "GET / HTTP/1.1\r\nHost: www.example.com\r\n\r\n", "timestamp": "1469733493.993", "body": ""}
response_header1 = {"headers": "HTTP/1.1 200 OK\r\nConnection: close\r\n", "timestamp": "1469733493.993", "body": "xxx"}
server.addResponse("sessionlog.json", request_header1, response_header1)

# ATS Configuration for the upstream ATS
ts2.Disk.records_config.update({
    'proxy.config.http2.enabled': 1,
    'proxy.config.diags.debug.enabled': 1,
    'proxy.config.diags.debug.tags': 'http.*|http2.*',
    'proxy.local.outgoing_ip_to_bind': '127.0.0.10 127.0.0.11 127.0.0.12',
    # Do not reuse connections to the upstream ATS so that we can verify
    # that outbound addresses are used in round robin
    'proxy.config.http.server_session_sharing.match': 'none',
    'proxy.config.ssl.server.cert.path': '{0}'.format(ts2.Variables.SSLDir),
    'proxy.config.ssl.server.private_key.path': '{0}'.format(ts2.Variables.SSLDir),
    'proxy.config.ssl.client.verify.server':  0,
    'proxy.config.ssl.server.cipher_suite': 'ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-SHA256:ECDHE-RSA-AES256-SHA384:AES128-GCM-SHA256:AES256-GCM-SHA384:ECDHE-RSA-RC4-SHA:ECDHE-RSA-AES128-SHA:ECDHE-RSA-AES256-SHA:RC4-SHA:RC4-MD5:AES128-SHA:AES256-SHA:DES-CBC3-SHA!SRP:!DSS:!PSK:!aNULL:!eNULL:!SSLv2',
})

ts2.Disk.ssl_multicert_config.AddLine(
    'dest_ip=* ssl_cert_name=server.pem ssl_key_name=server.key'
)

ts2.Disk.remap_config.AddLine(
    'map / https://127.0.0.1:{0}'.format(server.Variables.SSL_Port)
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

# ts is the downstream ATS process which will talk to the client
ts = Test.MakeATSProcess("ts", select_ports=True, enable_tls=True)
ts.addSSLfile("ssl/server.pem")
ts.addSSLfile("ssl/server.key")

# ATS Configuration for the first ATS
ts.Disk.records_config.update({
    'proxy.config.http2.enabled': 1,
    'proxy.config.diags.debug.enabled': 1,
    'proxy.config.diags.debug.tags': 'http.*|http2.*',
    'proxy.local.outgoing_ip_to_bind': '127.0.0.10 127.0.0.11 127.0.0.12',
    # Do not reuse connections to the upstream ATS so that we can verify
    # that outbound addresses are used in round robin
    'proxy.config.http.server_session_sharing.match': 'none',
    'proxy.config.ssl.server.cert.path': '{0}'.format(ts.Variables.SSLDir),
    'proxy.config.ssl.server.private_key.path': '{0}'.format(ts.Variables.SSLDir),
    'proxy.config.ssl.client.verify.server':  0,
    'proxy.config.ssl.server.cipher_suite': 'ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-SHA256:ECDHE-RSA-AES256-SHA384:AES128-GCM-SHA256:AES256-GCM-SHA384:ECDHE-RSA-RC4-SHA:ECDHE-RSA-AES128-SHA:ECDHE-RSA-AES256-SHA:RC4-SHA:RC4-MD5:AES128-SHA:AES256-SHA:DES-CBC3-SHA!SRP:!DSS:!PSK:!aNULL:!eNULL:!SSLv2',
})

ts.Disk.ssl_multicert_config.AddLine(
    'dest_ip=* ssl_cert_name=server.pem ssl_key_name=server.key'
)

ts.Disk.remap_config.AddLine(
    'map / https://127.0.0.1:{0}'.format(ts2.Variables.ssl_port)
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
    tr.Processes.Default.StartBefore(server, ready=When.PortOpen(server.Variables.SSL_Port))
    tr.Processes.Default.StartBefore(Test.Processes.ts2)
    tr.Processes.Default.StartBefore(Test.Processes.ts)

  tr.Processes.Default.Command = 'curl -k -s -D - -v --ipv4 --http2 -H "Host: www.example.com" https://localhost:{0}/'.\
    format(ts.Variables.ssl_port)
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
