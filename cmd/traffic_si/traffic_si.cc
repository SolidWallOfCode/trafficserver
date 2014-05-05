/** @file

    Main file for the traffic_si application.

    Traffic Stripe Inspector (SI) is a tool for inspecting stripe data in the cache.

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

#include "ink_config.h"
#include "libts.h"
#include "CacheBase.h"
#include <getopt.h>

//----------------------------------------------------------------------------
static void usage()
{
  fprintf(stderr, "Usage: traffic_si <path/to/cache-storage>\n");
  exit(1);
}
//----------------------------------------------------------------------------
int main(int argc, char** argv)
{
  char const* path;


# if 0
  int opt;
  while ((opt = getopt(argc, argv, "")) != -1) {
    switch(opt) {
    default:
      usage();
    }
  }
# endif

  if (optind >= argc) {
    fprintf(stderr, "Error: a path to a storage span is required and was not found.\n");
    usage();
  }

  path = argv[optind];
  printf("Opening %s\n", path);

  return 0;
}
