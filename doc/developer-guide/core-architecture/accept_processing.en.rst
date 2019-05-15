.. Licensed to the Apache Software Foundation (ASF) under one or more contributor license
   agreements.  See the NOTICE file distributed with this work for additional information regarding
   copyright ownership.  The ASF licenses this file to you under the Apache License, Version 2.0
   (the "License"); you may not use this file except in compliance with the License.  You may obtain
   a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software distributed under the License
   is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
   or implied.  See the License for the specific language governing permissions and limitations
   under the License.

.. include:: ../../common.defs

.. default-domain:: cpp

.. highlight:: cpp

.. _accept_processing:

Accept Processing
*****************

Accept processing is the mechanism that accepts inbound connections. This starts with the proxy
ports as defined by :ts:cv:`proxy.config.http.server_ports`. Each descriptor creates a proxy port to
accept inbound connections. :ts:cv: `proxy.config.accept_threads` sets the number of threads to use
for each proxy port.

If the number of accept threads is positive, then that many instance of :code:`NetAccept` are
created for each proxy port, each :code:`NetAccept` running in a distinct thread.

If the number of accept threads is 0 or negative the accept logic is
scheduled on the ``ET_NET`` threads. For each proxy port, a :code:`NetAccept` is created for each
``ET_NET`` thread, and is scheduled to run periodically. The timing is determined by
:ts:cv:`proxy.config.net.accept_period`.

For each :code:`NetAccept` an action is created. This action handles the initial connection
setup for the proxy.

.. figure:: /uml/images/accept_classes.svg
   :align: center
   :caption: Accept Class Hierarchy.
