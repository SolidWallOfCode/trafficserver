/** @file

    Buffer Writer for an MIOBuffer.

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

#pragma once

#include <cstring>
#include <iosfwd>

#include "tscore/ink_assert.h"

#if defined(UNIT_TEST_BUFFER_WRITER)
#undef ink_assert
#define ink_assert ink_release_assert
#endif

#include "tscore/BufferWriter.h"

#if !defined(UNIT_TEST_BUFFER_WRITER)
#include <I_IOBuffer.h>
#endif

/** BufferWriter on top of IO Buffer Blocks.
 *
 * This is intended for use as a base class for other writer that are based on IOBufferBlocks.
 */
class IOBlockWriter : public ts::BufferWriter
{
  using self_type  = IOBlockWriter; ///< Self reference type.
  using super_type = ts::BufferWriter;

public:
  /// Default constructor.
  IOBlockWriter() = default;

  /** Write a single character.
   *
   * @param c Character to write.
   * @return @a this
   */
  self_type &write(char c) override;

  using super_type::write;

  /** Write a block of @a data.
   *
   * @param data Content to write.
   * @param length Number of bytes to write in @a data.
   * @return @a this.
   */
  self_type &write(void const *data, size_t length) override;

  /// @return The maximum number of bytes available to write.
  /// @note Because this is block based, there is no upper limit.
  size_t capacity() const override;

  /// @return The start of the unwritten bytes.
  char *auxBuffer() override;

  /// @return The number of bytes that can be immediately written.
  size_t remaining();

  /** A span for the immediately writable data.
   *
   * @return A span of writable bytes.
   *
   * This is the existing writable buffer. If filled, another will be allocated. To mark as
   * used call @c fill.
   */
  ts::MemSpan<char> aux_span();

  /** Mark bytes as written.
   *
   * @param n Number of bytes to mark.
   * @return @a this
   */
  self_type &fill(size_t n) override;

  /// Reserve bytes at the end.
  /// @note No-op for this class.
  self_type &clip(size_t) override;

  /// Restore reerved bytes.
  /// @note No-op for this class.
  self_type &extend(size_t) override { return *this; }

  /// Get the start of the output buffer
  /// @note For IOBuffer based writers this is not useful.
  const char *data() const override;

  /// Get the overflow condition
  /// @note For IOBuffer based writers, this never happens.
  bool error() const override;

protected:
  /** Retrieve the currently writable output memory.
   *
   * @return A span covering the memory to which to write output.
   *const char * location = "IOBufferChain") : _location(location) {}
   * This must always return a non-empty span. If additional IOBlocks need to be allocated that
   * must be done in this method.
   */
  virtual ts::MemSpan<char> writable() = 0;

  /** Commit @a n bytes of writable memory.
   *
   * @param n Number of bytes to commit.
   *
   * This marks @a n bytes of the writable area as consumed / written.
   */
  virtual void commit(size_t n) = 0;
};

inline auto
IOBlockWriter::write(char c) -> self_type &
{
  return this->write(&c, 1);
}

inline ts::MemSpan<char>
IOBlockWriter::aux_span()
{
  return this->writable();
}

inline size_t
IOBlockWriter::remaining()
{
  return this->aux_span().size();
}

inline char *
IOBlockWriter::auxBuffer()
{
  return this->aux_span().data();
}

inline auto
IOBlockWriter::fill(size_t n) -> self_type &
{
  this->commit(n);
  return *this;
}

inline size_t
IOBlockWriter::capacity() const
{
  return std::numeric_limits<size_t>::max();
}

inline auto IOBlockWriter::clip(size_t) -> self_type &
{
  return *this;
}

inline const char *
IOBlockWriter::data() const
{
  return nullptr;
}

inline bool
IOBlockWriter::error() const
{
  return false;
}

/// BufferWriter @c for IOBufferChain.
/// @see IOBufferChain
class IOChainWriter : public IOBlockWriter
{
  using self_type  = IOChainWriter; ///< Self reference type.
  using super_type = IOBlockWriter; ///< Parent type.

public:
  /** Construct to write on @a chain.
   *
   * @param chain Output buffer chain.
   */
  IOChainWriter(IOBufferChain &chain);

  size_t
  extent() const override
  {
    return _chain.length();
  }

  std::ostream &operator>>(std::ostream &stream) const override;
  ssize_t operator>>(int fd) const override;

protected:
  /// BlockChain containing the output.
  IOBufferChain &_chain;
  /// Block size / index for the next block to allocate.
  int64_t _block_size_idx = iobuffer_size_to_index(default_small_iobuffer_size, MAX_BUFFER_SIZE_INDEX);

  /// Obtain the next block of writable memory.
  ts::MemSpan<char> writable() override;
  /// Mark @a n bytes of memory written.
  void commit(size_t n) override;
};

IOChainWriter::IOChainWriter(IOBufferChain &chain) : _chain(chain)
{
  if (auto tail = _chain.tail(); nullptr != tail) {
    // Get the block size of the last block in the chain
    _block_size_idx = iobuffer_size_to_index(tail->block_size(), MAX_BUFFER_SIZE_INDEX);
  }
}

inline void
IOChainWriter::commit(size_t n)
{
  _chain.fill(n);
}

/** BufferWriter interface on top of IOBuffer blocks.

    @internal This should be changed to IOBufferChain once I port that to open source.
 */
class MIOBufferWriter : public IOBlockWriter
{
  using self_type = MIOBufferWriter; ///< Self reference type.

public:
  explicit MIOBufferWriter(MIOBuffer *miob) : _miob(miob) {}

  bool
  error() const override
  {
    return false;
  }

  size_t
  extent() const override
  {
    return _numWritten;
  }

  // This must not be called for this derived class.
  //
  const char *
  data() const override
  {
    ink_assert(false);
    return nullptr;
  }

  /// Output the buffer contents to the @a stream.
  /// @return The destination stream.
  std::ostream &operator>>(std::ostream &stream) const override;
  /// Output the buffer contents to the file for file descriptor @a fd.
  /// @return The number of bytes written.
  ssize_t operator>>(int fd) const override;

protected:
  MIOBuffer *_miob;

private:
  size_t _numWritten = 0;

  ts::MemSpan<char>
  writable() override
  {
    IOBufferBlock *iobbPtr = _miob->first_write_block();

    if (!iobbPtr) {
      _miob->add_block();
      iobbPtr = _miob->first_write_block();
    }

    return {iobbPtr->end(), size_t(iobbPtr->write_avail())};
  }

  void
  commit(size_t n) override
  {
    if (n) {
      IOBufferBlock *iobbPtr = _miob->first_write_block();

      ink_assert(iobbPtr and (n <= size_t(iobbPtr->write_avail())));

      iobbPtr->fill(n);

      _numWritten += n;
    }
  }
};
