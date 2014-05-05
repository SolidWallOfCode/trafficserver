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

    /// A block size for storage.
    size_t const STORE_BLOCK_SIZE = 8192;

    /// Descriptor for storage for a stripe.
    /// @note This is the storage for a stripe.
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
        uint32_t _num_volumes; ///< Number distinct volumes.
        uint32_t _num_free; ///< Number of free stripe spans.
        uint32_t _num_used; ///< Number of stripe spans in use.
        uint64_t _num_blocks; ///< Number of blocks of storage.
        StripeSpan _spans[1]; ///< Variable sized array of span descriptors.
    };
}

# endif // guard.
