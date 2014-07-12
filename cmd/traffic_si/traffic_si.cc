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
#include <fcntl.h>

//----------------------------------------------------------------------------
static void usage()
{
  fprintf(stderr, "Usage: traffic_si <path/to/cache-storage>\n");
  exit(1);
}
//----------------------------------------------------------------------------

// A hacked size for now, need to do better computation at some point.
static size_t const RAW_BUFF_SIZE = Cache::STORE_BLOCK_SIZE * 8;
char buff[RAW_BUFF_SIZE] __attribute__ ((aligned(512))) ;
// stripe meta data.
char meta_buff[4][Cache::STORE_BLOCK_SIZE] __attribute((aligned(512)));

static struct option cmd_options[] = {
  { "object-size", required_argument, 0, 0 },
  { 0, 0, 0, 0}
};

int main(int argc, char** argv)
{
  int fd;
  ssize_t n;
  int opts = O_RDONLY; // don't need write access.
  Cache::SpanHeader* dev_header;
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

#if defined(O_DIRECT)
    opts |= O_DIRECT;
#endif
#if defined(O_DSYNC)
    opts |= O_DSYNC;
#endif
  fd = open(path, opts, 0644);

  if (fd < 0) {
    fprintf(stderr, "Failed to open storage %s - [%d:%s]", path, errno, strerror(errno));
    exit(1);
  }

  n = pread(fd, buff, sizeof(buff), Cache::DEV_RESERVE_SIZE);
  printf("read %" PRId64 " byte [%d:%s]\n", n, errno,strerror(errno));

  dev_header = reinterpret_cast<Cache::SpanHeader*>(buff);

  if (Cache::SpanHeader::MAGIC_ALIVE != dev_header->_magic) {
    fprintf(stderr, "Unable to locate valid span header on device %s\n", path);
    exit(2);
  }

  printf("Span %s with %d stripes.\nStripe spans -  %d used, %d free, %d total\nTotal storage blocks = %" PRIu64 "\n"
         , path
         , dev_header->_n_stripes
         , dev_header->_n_used, dev_header->_n_free, dev_header->_n_stripe_spans
         , dev_header->_n_storage_blocks
    );

  for (unsigned i = 0 ; i < dev_header->_n_stripe_spans ; ++i ) {
    Cache::StripeSpan* sspan = dev_header->_spans + i;
    Cache::StripeDescriptor* smeta;
    printf("Stripe span %d - Start = %" PRIu64 " Length = %" PRIu64" blocks Index = %d Type = %d (%s)\n"
           , i, sspan->_offset, sspan->_len, sspan->_number, sspan->_type, sspan->_free_p ? "free" : "in-use"
      );
    // Attempt to read the stripe header
    if (!sspan->_free_p) {
      n = pread(fd, meta_buff[0], sizeof(meta_buff[0]), sspan->_offset);
      if (n > 0) {
        smeta = reinterpret_cast<Cache::StripeDescriptor*>(meta_buff[0]);
        if (Cache::StripeDescriptor::MAGIC_ALIVE == smeta->_magic) {
          printf("\tVersion %d:%d\n", smeta->_version.ink_major, smeta->_version.ink_minor);
        } else {
          printf("\tCopy A header invalid\n");
        }
      } else {
        printf("\tRead failed - %d (%s)\n", errno, strerror(errno));
      }
    }
  }
  
  close(fd);
  return 0;
}
