'''
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

import subprocess

Test.Summary = '''
Test proxy.config.cache.max_doc_size config variable.
'''

Test.SkipUnless(
    Condition.HasProgram(
        "curl", "Curl need to be installed on system for this test to work"),
    Condition.HasProgram("netstat", "netstat need to be installed on system for this test to work"),
    Condition.PluginExists("conf_remap.so")
)

# Ask the OS if the port is ready for connect()
#
def CheckPort(Port):
    return lambda: 0 == subprocess.call('netstat --listen --tcp -n | grep -q :{}'.format(Port), shell=True)

server = Test.MakeOriginServer("server")

# Define ATSes.
#
# ts = Test.MakeATSProcess("ts", command="traffic_manager") use traffic manager so traffic_ctl will work
ts = Test.MakeATSProcess("ts")   # max doc size configured to 40, no read while writer.
ts2 = Test.MakeATSProcess("ts2") # max doc size configured to 40, but forced to 0 because read while writer enabled
ts3 = Test.MakeATSProcess("ts3") # max doc size defaults to 0 (disabled), no read while writer.

tr = Test.AddTestRun()
tr.Processes.Default.StartBefore(server, ready=CheckPort(server.Variables.Port))
tr.Processes.Default.ReturnCode = 0
tr.Processes.Default.Command = "echo 'micro-server ready'"

# Make 'body' with 41 bytes.
#
body = '0123456789'
body = body + body + body + body + '\n'

# expected response from the origin server
response_header = {
    "headers": "HTTP/1.1 200 OK\r\nConnection: close\r\n" +
    'Etag: "359670651"\r\n' +
    "Cache-Control: public, max-age=31536000\r\n" +
    "Accept-Ranges: bytes\r\n" +
    "\r\n",
    "timestamp": "1469733493.993",
    "body": body
}
for i in range(3):
    # add request/response to the server dictionary
    request_header = {
        "headers": "GET /obj{} HTTP/1.1\r\nHost: just.any.thing\r\n\r\n".format(i), "timestamp": "1469733493.993", "body": ""
    }
    server.addResponse("sessionfile.log", request_header, response_header)

def curl(ts, idx):
    for i in range(2):
        tr = Test.AddTestRun()
        tr.Processes.Default.Command = (
            "curl --verbose --proxy http://127.0.0.1:{}".format(ts.Variables.port) +
            " 'http://host{0}/obj{0}'".format(idx)
        )
        tr.Processes.Default.ReturnCode = 0

def tsCommon(ts):

    ts.Disk.records_config.update({
        # Do not accept connections from clients until cache subsystem is operational.
        'proxy.config.http.wait_for_cache': 1,

        'proxy.config.diags.debug.enabled': 0
    })

    ts.Disk.remap_config.AddLine(
        'map http://host0/ http://127.0.0.1:{}/'.format(server.Variables.Port)
    )
    ts.Disk.remap_config.AddLine(
        'map http://host1/ http://127.0.0.1:{}/'.format(server.Variables.Port) +
        ' @plugin=conf_remap.so @pparam=proxy.config.cache.max_doc_size=41'
    )
    ts.Disk.remap_config.AddLine(
        'map http://host2/ http://127.0.0.1:{}/'.format(server.Variables.Port) +
        ' @plugin=conf_remap.so @pparam=proxy.config.cache.max_doc_size=42'
    )

    ts.Disk.logging_config.AddLines(
        '''custom = format {
      Format = "%<cluc> %<crc>"
    }

    log.ascii {
      Format = custom,
      Filename = 'cheese'
    }'''.split("\n")
    )

    tr = Test.AddTestRun()
    tr.Processes.Default.StartBefore(server, ready=CheckPort(server.Variables.Port))
    tr.Processes.Default.StartBefore(ts, ready=CheckPort(ts.Variables.port))
    tr.Processes.Default.ReturnCode = 0
    tr.Processes.Default.Command = "echo 'trafficsever ready'"

    for idx in range(3):
        curl(ts, idx)

ts.Disk.records_config.update({
    'proxy.config.cache.enable_read_while_writer': 0,
    'proxy.config.cache.max_doc_size': 40,
})

tsCommon(ts)

ts2.Disk.records_config.update({
    'proxy.config.cache.enable_read_while_writer': 1,
    'proxy.config.cache.max_doc_size': 40,
})

tsCommon(ts2)

ts3.Disk.records_config.update({
    'proxy.config.cache.enable_read_while_writer': 0,
})

tsCommon(ts3)

# traffic_ctl does not play nice with autest.
'''
tr = Test.AddTestRun()
tr.Processes.Default.Command = "traffic_ctl --debug config set proxy.config.cache.max_doc_size 40"
tr.Processes.Default.Env = ts.Env
tr.Processes.Default.ReturnCode = 0

# Delay to allow traffic_ctl to take effect.
#
tr = Test.AddTestRun()
tr.DelayStart = 15
tr.Processes.Default.Command = 'echo'

for idx in range(3):
    curl(ts, idx)
'''

# Delay to allow TSes to flush report to disk.  Since the server port number may vary, use sed to change it to a fixed string to
# ensure a consistent match to the gold file.
#
tr = Test.AddTestRun()
tr.DelayStart = 10
tr.Processes.Default.Command = (
    "( cat {}/cheese.log ; echo ; cat {}/cheese.log ; echo ; cat {}/cheese.log ) | sed 's/{}/SERVER_PORT/' >| {}/cheese.log".format(
        ts.Variables.LOGDIR, ts2.Variables.LOGDIR, ts3.Variables.LOGDIR, server.Variables.Port, Test.RunDirectory)
)
f = tr.Disk.File("cheese.log")
f.Content = "gold.log"
tr.Processes.Default.ReturnCode = 0
