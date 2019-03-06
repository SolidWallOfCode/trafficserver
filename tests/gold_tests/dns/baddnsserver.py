# coding=utf-8

#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

import datetime
import sys
import time
import threading
import traceback
import socket
import argparse
import codecs
import json
import signal
import os

# Create a dictionary of all the client source ports that we recv from
clientports = {}
numClientRequests = 0

def checkSourcePortRandomness():

  rc = 1
  if numClientRequests < 5:
    print("Did not see enough client requests %d" % numClientRequests)
    return rc

  numUniquePorts = len(clientports)
  if numUniquePorts > 3:
    rc = 0
    print('DNS requests had randomness in their source ports: %d unique ports' % numUniquePorts)
  else:
    print('DNS requests did not have randomness in their source ports %d unique ports' % numUniquePorts)

  return rc

def handler(signum, frame):
    # Before we exit test that the source ports were not all the same and had some
    # distribution
    exitcode = checkSourcePortRandomness()
    print('Signal handler called with signal', signum)
    sys.exit(exitcode)

if __name__ == '__main__':

  parser = argparse.ArgumentParser(description='UDP Server that does not respond')
  parser.add_argument("-l", "--listen", type=str, default = '127.0.0.1', help='Server address to bind to')
  parser.add_argument("-p", "--port", type=int, default = 53, help='Server port to bind to')
  args = vars(parser.parse_args())

  listenAddr = args.get('listen')
  portNum    = args.get('port')

  signal.signal(signal.SIGINT, handler)

  # Create a UDP/IP socket
  sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

  # Bind the socket to the port
  server_address = (listenAddr, portNum)
  print('starting up on %s port %s' % server_address)
  sock.bind(server_address)

  # Recv datagrams from client and record their source ports
  while True:
    data, address = sock.recvfrom(4096)

    numClientRequests += 1
    print('received %s bytes from %s' % (len(data), address))
    clientport = address[1]
    if clientport in clientports:
      clientports[clientport] += 1
    else:
      clientports[clientport] = 1

