.. Licensed to the Apache Software Foundation (ASF) under one
   or more contributor license agreements.  See the NOTICE file
   distributed with this work for additional information
   regarding copyright ownership.  The ASF licenses this file
   to you under the Apache License, Version 2.0 (the
   "License"); you may not use this file except in compliance
   with the License.  You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing,
   software distributed under the License is distributed on an
   "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
   KIND, either express or implied.  See the License for the
   specific language governing permissions and limitations
   under the License.

.. include:: ../common.defs

.. _upgrading:

Upgrading to ATS v9.x
=====================

.. toctree::
   :maxdepth: 1

Remapping
---------

One of the biggest changes in ATS v9.0.0 is that URL rewrites, as specified in a :file:`remap.config` rule, now always happens
**before** all plugins are executed. This can have significant impact on behavior, since plugins might now see a different URL than
they did in prior versions. In particular, plugins modifying the cache key could have serious problems (see the section below for
details). This can also require changes in the configuration for the :ref:`header_rewrite <admin-plugins-header-rewrite>` plugin.

Relevant pull requests:

*  `PR 4964 <https://github.com/apache/trafficserver/pull/4964>`__

YAML
----

We are moving configurations over to YAML, and thus far, the following configurations are now fully migrated over to YAML:

* :file:`logging.yaml` (*was* `logging.config` or `logging.lua`)
* :file:`ip_allow.yaml` (*was* `ip_allow.config`)

In addition, a new file for TLS handhsake negotiation configuration is added:

* :file:`sni.yaml` (this was for a while named ssl_server_name.config in Github)

New records.config settings
----------------------------

These are the changes that are most likely to cause problems during an upgrade. Take special care making sure you have updated your
configurations accordingly.

Connection management
~~~~~~~~~~~~~~~~~~~~~

The old settings for origin connection management included the following settings:

* `proxy.config.http.origin_max_connections`
* `proxy.config.http.origin_max_connections_queue`
* `proxy.config.http.origin_min_keep_alive_connections`

These are all gone, and replaced with the following set of configurations:

* :ts:cv:`proxy.config.http.per_server.connection.max` (overridable)
* :ts:cv:`proxy.config.http.per_server.connection.match` (overridable)
* :ts:cv:`proxy.config.http.per_server.connection.alert_delay`
* :ts:cv:`proxy.config.http.per_server.connection.queue_size`
* :ts:cv:`proxy.config.http.per_server.connection.queue_delay`
* :ts:cv:`proxy.config.http.per_server.connection.min`

Removed records.config settings
-------------------------------

The following settings are simply gone, and have no purpose:

* `proxy.config.config_dir` (see PROXY_CONFIG_CONFIG_DIR environment variable)
* `proxy.config.cache.storage_filename` (see next section as well)

Deprecated records.config settings
----------------------------------

The following configurations still exist, and functions, but are considered deprecated and will be removed in a future release. We
**strongly** encourage you to avoid using any of these:

  * :ts:cv:`proxy.config.socks.socks_config_file`
  * :ts:cv:`proxy.config.log.config.filename`
  * :ts:cv:`proxy.config.url_remap.filename`
  * :ts:cv:`proxy.config.ssl.server.multicert.filename`
  * :ts:cv:`proxy.config.ssl.servername.filename`
  * ``proxy.config.http.parent_proxy.file``
  * ``proxy.config.cache.control.filename``
  * ``proxy.config.cache.ip_allow.filename``
  * ``proxy.config.cache.hosting_filename``
  * ``proxy.config.cache.volume_filename``
  * ``proxy.config.dns.splitdns.filename``

Deprecated or Removed Features
------------------------------

The following features, configurations and plugins are either removed or deprecated in this version of ATS. Deprecated features
should be avoided, with the expectation that they will be removed in the next major release of ATS.


API Changes
-----------

Our APIs are guaranteed to be compatible within major versions, but we do make changes for each new major release.

Removed APIs
~~~~~~~~~~~~

* ``TSHttpTxnRedirectRequest()``

Renamed or modified APIs
~~~~~~~~~~~~~~~~~~~~~~~~

* ``TSVConnSSLConnectionGet()`` is renamed to be :c:func:`TSVConnSslConnectionGet`

* ``TSHttpTxnServerPush()`` now returns a :c:type:`TSReturnCode`

The "transaction arguments" support has been generalized and the previous plugin API calls deprecated. The new API is

*  :c:func:`TSUserArgGet`
*  :c:func:`TSUserArgSet`
*  :c:func:`TSUserArgIndexReserve`
*  :c:func:`TSuserArgIndexLookup`
*  :c:func:`TSUserArgIndexNameLookup`
*  :c:type:`TSUserArgType`

These replace

*  :code:`TSHttpTxnArgGet`
*  :code:`TSHttpTxnArgSet`
*  :code:`TSHttpTxnArgIndexReserve`
*  :code:`TSHttpTxnArgIndexLookup`
*  :code:`TSHttpTxnArgIndexNameLookup`
*  :code:`TSHttpSsnArgGet`
*  :code:`TSHttpSsnArgSet`
*  :code:`TSHttpSsnArgIndexReserve`
*  :code:`TSHttpSsnArgIndexLookup`
*  :code:`TSHttpSsnArgIndexNameLookup`
*  :code:`TSVConnArgGet`
*  :code:`TSVConnArgSet`
*  :code:`TSVConnArgIndexReserve`
*  :code:`TSVConnArgIndexLookup`
*  :code:`TSVConnArgIndexNameLookup`

Upgrading the code is straight forward, the new API is almost a drop in replacement for the old one. The various specialized calls are replaced by the parallel new API. For :c:func:`TSUserArgGet` and :c:func:`TSUserArgSet` this is all that needs to be done. For the other functions, an additional argument needs to be added as the first argument. This argumnt will be one of the enumerations of :c:type:`TSUserArgType`. The transaction based calls use :c:macro:`TS_USER_ARGS_TXN` and the session based calls use :c:macro:`TS_USER_ARGS_SSN`. For virtual connections use :c:macro:`TS_USER_ARGS_VCONN`.

The new feature available are "user args", which are proxy level arguents. These can be used to pass data between plugins that is not dependent on a session or transaction.

The argument indices are independent per type, therefore reserving an index for one type does not reserve it for any other type. This had already been done in ATS 8 for transaction and session arguments. This change generalizes that for all argument types, while also adding the "user" arguments.

Relevant pull requests:

*  `PR 6468 <https://github.com/apache/trafficserver/pull/6468>`__

Cache
-----

The cache in this releases of ATS is compatible with previous versions of ATS. You would not expect to lose your cache, or have to
reinitialize the cache when upgrading.

However, due to changes in how remap plugins are processed, your cache key *might* change. In versions to v9.0.0, the first plugin
in a remap rule would get the pristine URL, and subsequent plugins would get the remapped URL. As of v9.0.0, **all** plugins now
receive the remapped URL. If you are using a plugin that modifies the cache key, e.g. :ref:`admin-plugins-cachekey`, if it was
evaluated first in a remap rule, the behavior (input) changes, and therefore, cache keys can change!

The old ``v23`` cache is no longer supported, which means caches created with ATS v2.x will no longer be possible to load with ATS
v9.0.0 or later. We feel that this is an unlikely scenario, but if you do run into this, clearing the cache is required.

Plugins
-------

The following plugins have changes that might require you to change
configurations.

header_rewrite
~~~~~~~~~~~~~~

* The `%{PATH}` directive is now removed, and instead you want to use `%{CLIENT-URL:PATH}`. This was done to unify the behavior of
  these operators, rather than having this one-off directive.

Dynamic remap plugin reloading
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Plugins used in :file:`remap.config` can be reloaded. This means the actual DSO and code, not the configuration. Due to limitations in the underlying support for loading DSOs, it is not possible to truly reload a DSO that has already been loaded into a process. For this reason the reloading logic makes a copy of the DSO and loads that. If the DSO needs to be reloaded, another copy is made andthat loaded.

This has implications primarily for *mixed mode* plugins, that is plugins that are loaded as both remap plugins and global plugins. The key effect is that static variables are **not** shared between the global code and the remap code because from the point of view of the operating system these are different DSOs. This can have some very subtle and hard to diagnose effects. Even if this could be overcome, after a reload there would be two DSOs sharing slightly different code while assuming it was identical. This is unlikely to end well.

This behavior can be globally disabled by setting :ts:cv:`proxy.config.remap.dynamic_reload_mode` to 0.

Going forward, if this feature is not disabled, mixed mode plugins will likely need to be split into two DSOs, one for the global "side" and another for the remap side. The remap DSO would then depend on the global DSO, which would make the static data in the global DSO reliably available to the remap DSO.

Relevant pull requests:

*  `PR 5282 <https://github.com/apache/trafficserver/pull/5282>`__
*  `PR 5970 <https://github.com/apache/trafficserver/pull/5970>`__
*  `PR 6071 <https://github.com/apache/trafficserver/pull/6071>`__
*  `PR 6421 <https://github.com/apache/trafficserver/pull/6421>`__

Platform specific
-----------------

Solaris is no longer a supported platform, but the code is still there. However, it's unlikely to work, and unless someone takes on
ownership of this Platform, it will be removed from the source in ATS v10.0.0. For more details, see issue #5553.

The cache in this releases of ATS is compatible with previous versions of ATS. You would not expect
to lose your cache, or have to reinitialize the cache when upgrading.
