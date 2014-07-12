/** @file

    Basic cache definitions.

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

# if ! defined(_TS_CACHE_BASE_H)
# define _TS_CACHE_BASE_H

namespace Cache {
  /// Device block size.
  /// @internal formerly CACHE_BLOCK_SIZE
  static size_t const DEV_BLOCK_SIZE = 512;

  /** A block size for storage.
      Metadata stored on disk is stored in units of this size.
   */
  static size_t const STORE_BLOCK_SIZE = 8192;

  /// Reserved space at the start of raw device storage, not accessed by ATS.
  /// @internal @c START_POS
  static size_t const DEV_RESERVE_SIZE = DEV_BLOCK_SIZE * 16;

  /// Descriptor for storage for a stripe.
  /// @note Can be directly serialized.
  /// @internal Equivalent to DiskVolBlock
  struct StripeSpan {
    uint64_t    _offset; ///< Offset from start of span. (bytes)
    uint64_t    _len; ///< Length of block. (STORE_BLOCK_SIZE)
    int32_t     _number; ///< Block index.
    unsigned int _type:3; ///< Block type.
    unsigned int _free_p:1; ///< Block not in use.
  };

  /** Header for span on disk.
      @note Can be directly serialized.
  */
  struct SpanHeader {
    static uint32_t const MAGIC_ALIVE = 0xABCD1237;
    uint32_t _magic;    ///< Magic value (validity checking).
    uint32_t _n_stripes; ///< Number distinct stripes.
    uint32_t _n_free; ///< Number of free stripe spans.
    uint32_t _n_used; ///< Number of stripe spans in use.
    uint32_t _n_stripe_spans; ///< Number of distinct storage areas.
    uint64_t _n_storage_blocks; ///< Total number of blocks of storage.
    StripeSpan _spans[1]; ///< Variable sized array of span descriptors.
  };

  /** Metadata (description) of a stripe.
      @note There are 4 copies of this per stripe. Two copies, A and B, each consisting of a header and a footer
      which are instances of this class. The @a _freelist is stored only in the header instances.
      @internal @c VolHeaderFooter
   */
  struct StripeDescriptor  {
    static uint32_t const MAGIC_ALIVE = 0xF1D0F00D;
    uint32_t _magic;
    VersionNumber _version;
    time_t _create_time;
    off_t _write_pos;
    off_t _last_write_pos;
    off_t _agg_pos;
    uint32_t _generation;            // token generation (vary), this cannot be 0
    uint32_t _phase;
    uint32_t _cycle;
    uint32_t _sync_serial;
    uint32_t _write_serial;
    uint32_t _dirty;
    uint32_t _sector_size;
    uint32_t _unused;                // pad out to 8 byte boundary
    uint16_t _freelist[1];
  };

} // namespace Cache

# endif // guard.
