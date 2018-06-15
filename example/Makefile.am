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

include $(top_srcdir)/build/plugins.mk
include $(top_srcdir)/build/tidy.mk

AM_CXXFLAGS += -Wno-unused-variable
AM_LDFLAGS = $(TS_PLUGIN_LD_FLAGS)

libtscpp = $(top_builddir)/lib/tscpp/libtscpp.la
libtsconfig = $(top_builddir)/lib/tsconfig/libtsconfig.la

if BUILD_EXAMPLE_PLUGINS

example_Plugins = \
	add_header.la \
	append_transform.la \
	basic_auth.la \
	blacklist_0.la \
	blacklist_1.la \
	bnull_transform.la \
	request_buffer.la \
	cache_scan.la \
	file_1.la \
	hello.la \
	intercept.la \
	lifecycle_plugin.la \
	null_transform.la \
	output_header.la \
	passthru.la \
	protocol_stack.la \
	protocol.la \
	psi.la \
	query_remap.la \
	redirect_1.la \
	remap.la \
	remap_header_add.la \
	replace_header.la \
	response_header_1.la \
	secure_link.la \
	server_push.la \
	server_transform.la \
	session_hooks.la \
	ssl_preaccept.la \
	ssl_sni_whitelist.la \
	ssl_sni.la \
	statistic.la \
	thread_1.la \
	txn_data_sink.la \
	version.la \
	disable_http2.la \
	verify_cert.la \
	vconn_args.la

example_Plugins += \
	tscpp/AsyncHttpFetch.la \
	tscpp/AsyncHttpFetchStreaming.la \
	tscpp/AsyncTimer.la \
	tscpp/ClientRedirect.la \
	tscpp/ClientRequest.la \
	tscpp/CustomErrorRemapPlugin.la \
	tscpp/CustomResponse.la \
	tscpp/DelayTransformationPlugin.la \
	tscpp/GlobalHookPlugin.la \
	tscpp/GzipTransformationPlugin.la \
	tscpp/HelloWorldPlugin.la \
	tscpp/InternalTransactionHandling.la \
	tscpp/LoggerExample.la \
	tscpp/MultipleTransactionHookPlugins.la \
	tscpp/NullTransformationPlugin.la \
	tscpp/PostBuffer.la \
	tscpp/RemapPlugin.la \
	tscpp/ServerResponse.la \
	tscpp/StatExample.la \
	tscpp/TimeoutExamplePlugin.la \
	tscpp/TransactionHookPlugin.la \
	tscpp/WebSocket.la \
	tscpp/boom.la \
	tscpp/intercept.la

pkglib_LTLIBRARIES = $(example_Plugins)

endif

add_header_la_SOURCES = add_header/add_header.c
append_transform_la_SOURCES = append_transform/append_transform.c
basic_auth_la_SOURCES = basic_auth/basic_auth.c
blacklist_0_la_SOURCES = blacklist_0/blacklist_0.c
blacklist_1_la_SOURCES = blacklist_1/blacklist_1.c
bnull_transform_la_SOURCES = bnull_transform/bnull_transform.c
request_buffer_la_SOURCES = request_buffer/request_buffer.c
cache_scan_la_SOURCES = cache_scan/cache_scan.cc
file_1_la_SOURCES = file_1/file_1.c
hello_la_SOURCES = hello/hello.c
intercept_la_SOURCES = intercept/intercept.cc
lifecycle_plugin_la_SOURCES = lifecycle_plugin/lifecycle_plugin.c
null_transform_la_SOURCES = null_transform/null_transform.c
output_header_la_SOURCES = output_header/output_header.c
passthru_la_SOURCES = passthru/passthru.cc
protocol_la_SOURCES = protocol/Protocol.c protocol/TxnSM.c
protocol_stack_la_SOURCES = protocol_stack/protocol_stack.cc
psi_la_SOURCES = thread_pool/psi.c thread_pool/thread.c
query_remap_la_SOURCES = query_remap/query_remap.c
remap_header_add_la_SOURCES = remap_header_add/remap_header_add.cc
remap_la_SOURCES = remap/remap.cc
replace_header_la_SOURCES = replace_header/replace_header.c
response_header_1_la_SOURCES = response_header_1/response_header_1.c
secure_link_la_SOURCES = secure_link/secure_link.c
server_push_la_SOURCES = server_push/server_push.c
server_transform_la_SOURCES = server_transform/server_transform.c
ssl_preaccept_la_SOURCES = ssl_preaccept/ssl_preaccept.cc
ssl_sni_la_SOURCES = ssl_sni/ssl_sni.cc
ssl_sni_la_LIBADD = $(libtsconfig)
ssl_sni_whitelist_la_SOURCES = ssl_sni_whitelist/ssl_sni_whitelist.cc
ssl_sni_whitelist_la_LIBADD = $(libtsconfig)
disable_http2_la_SOURCES = disable_http2/disable_http2.cc
verify_cert_la_SOURCES = verify_cert/verify_cert.cc
statistic_la_SOURCES = statistic/statistic.cc
thread_1_la_SOURCES = thread_1/thread_1.c
txn_data_sink_la_SOURCES = txn_data_sink/txn_data_sink.c
version_la_SOURCES = version/version.c
redirect_1_la_SOURCES = redirect_1/redirect_1.c
session_hooks_la_SOURCES = session_hooks/session_hooks.c
vconn_args_la_SOURCES = vconn_args/vconn_args.cc

tscpp_AsyncHttpFetchStreaming_la_SOURCES = tscpp/async_http_fetch_streaming/AsyncHttpFetchStreaming.cc
tscpp_AsyncHttpFetch_la_SOURCES = tscpp/async_http_fetch/AsyncHttpFetch.cc
tscpp_AsyncTimer_la_SOURCES = tscpp/async_timer/AsyncTimer.cc
tscpp_ClientRedirect_la_SOURCES = tscpp/clientredirect/ClientRedirect.cc
tscpp_ClientRequest_la_SOURCES = tscpp/clientrequest/ClientRequest.cc
tscpp_CustomErrorRemapPlugin_la_SOURCES = tscpp/custom_error_remap_plugin/CustomErrorRemapPlugin.cc
tscpp_CustomResponse_la_SOURCES = tscpp/customresponse/CustomResponse.cc
tscpp_DelayTransformationPlugin_la_SOURCES = tscpp/delay_transformation_plugin/DelayTransformationPlugin.cc
tscpp_GlobalHookPlugin_la_SOURCES = tscpp/globalhook/GlobalHookPlugin.cc
tscpp_GzipTransformationPlugin_la_SOURCES = tscpp/gzip_transformation/GzipTransformationPlugin.cc
tscpp_HelloWorldPlugin_la_SOURCES = tscpp/helloworld/HelloWorldPlugin.cc
tscpp_InternalTransactionHandling_la_SOURCES = tscpp/internal_transaction_handling/InternalTransactionHandling.cc
tscpp_LoggerExample_la_SOURCES = tscpp/logger_example/LoggerExample.cc
tscpp_MultipleTransactionHookPlugins_la_SOURCES = tscpp/multiple_transaction_hooks/MultipleTransactionHookPlugins.cc
tscpp_NullTransformationPlugin_la_SOURCES = tscpp/null_transformation_plugin/NullTransformationPlugin.cc
tscpp_PostBuffer_la_SOURCES = tscpp/post_buffer/PostBuffer.cc
tscpp_RemapPlugin_la_SOURCES = tscpp/remap_plugin/RemapPlugin.cc
tscpp_ServerResponse_la_SOURCES = tscpp/serverresponse/ServerResponse.cc
tscpp_StatExample_la_SOURCES = tscpp/stat_example/StatExample.cc
tscpp_TimeoutExamplePlugin_la_SOURCES = tscpp/timeout_example/TimeoutExamplePlugin.cc
tscpp_TransactionHookPlugin_la_SOURCES = tscpp/transactionhook/TransactionHookPlugin.cc
tscpp_WebSocket_la_SOURCES = tscpp/websocket/WebSocket.cc tscpp/websocket/WSBuffer.cc
tscpp_boom_la_SOURCES = tscpp/boom/boom.cc
tscpp_intercept_la_SOURCES = tscpp/intercept/intercept.cc

tscpp_AsyncHttpFetchStreaming_la_LIBADD = $(libtscpp)
tscpp_AsyncHttpFetch_la_LIBADD = $(libtscpp)
tscpp_AsyncTimer_la_LIBADD = $(libtscpp)
tscpp_ClientRedirect_la_LIBADD = $(libtscpp)
tscpp_ClientRequest_la_LIBADD = $(libtscpp)
tscpp_CustomErrorRemapPlugin_la_LIBADD = $(libtscpp)
tscpp_CustomResponse_la_LIBADD = $(libtscpp)
tscpp_DelayTransformationPlugin_la_LIBADD = $(libtscpp)
tscpp_GlobalHookPlugin_la_LIBADD = $(libtscpp)
tscpp_GzipTransformationPlugin_la_LIBADD = $(libtscpp)
tscpp_HelloWorldPlugin_la_LIBADD = $(libtscpp)
tscpp_InternalTransactionHandling_la_LIBADD = $(libtscpp)
tscpp_LoggerExample_la_LIBADD = $(libtscpp)
tscpp_MultipleTransactionHookPlugins_la_LIBADD = $(libtscpp)
tscpp_NullTransformationPlugin_la_LIBADD = $(libtscpp)
tscpp_PostBuffer_la_LIBADD = $(libtscpp)
tscpp_RemapPlugin_la_LIBADD = $(libtscpp)
tscpp_ServerResponse_la_LIBADD = $(libtscpp)
tscpp_StatExample_la_LIBADD = $(libtscpp)
tscpp_TimeoutExamplePlugin_la_LIBADD = $(libtscpp)
tscpp_TransactionHookPlugin_la_LIBADD = $(libtscpp)
tscpp_WebSocket_la_LIBADD = $(libtscpp)
tscpp_boom_la_LIBADD = $(libtscpp)
tscpp_intercept_la_LIBADD = $(libtscpp)

clang-tidy-local: $(DIST_SOURCES)
	$(CXX_Clang_Tidy)
	$(CC_Clang_Tidy)
