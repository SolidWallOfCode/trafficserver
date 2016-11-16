#if !defined TS_BUFFER_VIEW
#define TS_BUFFER_VIEW

/** @file

    Class for handling "views" of a buffer. Views presume the memory for the
   buffer is managed
    elsewhere and allow efficient access to segments of the buffer without
   copies. Views are
    read only as the view doesn't own the memory. Along with generic buffer
   methods are specialized
    methods to support better string parsing, particularly token based parsing.

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

#include <bitset>
#include <functional>
#include <iosfwd>
#include <memory.h>
#include <ts/ParseRules.h>

/// Apache Traffic Server commons.
namespace ts
{
class BufferView;
int compare(BufferView const &lhs, BufferView const &rhs);
int compare_nocase(BufferView lhs, BufferView rhs);

/** A read only view of contiguous piece of memory.

    A @c BufferView does not own the memory to which it refers, it is simply a view of part of some
    (presumably) larger memory object. The purpose is to allow working in a read only way a specific
    part of the memory. A classic example for ATS is working with HTTP header fields and values
    which need to be accessed independently but preferably without copying. A @c BufferView supports this style.

    BufferView is based on an earlier class ConstBuffer and influenced by Boost.string_ref. Neither
    of these were adequate for how use of @c ConstBuffer evolved and so @c BufferView is @c
    ConstBuffer with some additional stylistic changes based on Boost.string_ref.

    In particular @c BufferView is designed both to support passing via API (to replace the need to
    pass two parameters for one real argument) and to aid in parsing input without copying.

 */
class BufferView
{
  typedef BufferView self; ///< Self reference type.

protected:
  const char *_ptr = nullptr; ///< Pointer to base of memory chunk.
  size_t _size     = 0;       ///< Size of memory chunk.
public:
  /// Default constructor (empty buffer).
  constexpr BufferView();

  /** Construct explicitly with a pointer and size.
   */
  constexpr BufferView(const char *ptr, ///< Pointer to buffer.
                       size_t n         ///< Size of buffer.
                       );

  /** Construct from a half open range of two pointers.
      @note The byte at @start is in the view but the byte at @a end is not.
  */
  constexpr BufferView(const char *start, ///< First byte in the view.
                       const char *end    ///< First byte not in the view.
                       );

  /** Construct from null terminated string.
      @note The terminating null is not included. @c strlen is used to determine the length.
  */
  explicit constexpr BufferView(const char *s);

  /** Equality.

      This is effectively a pointer comparison, buffer contents are not compared.

      @return @c true if @a that refers to the same view as @a this,
      @c false otherwise.
   */
  bool operator==(self const &that) const;

  /** Inequality.
      @return @c true if @a that does not refer to the same view as @a this,
      @c false otherwise.
   */
  bool operator!=(self const &that) const;

  /// Assignment - the view is copied, not the content.
  self &operator=(self const &that);

  /// @return The first byte in the view.
  char operator*() const;

  /** Shift the view to discard the first byte.
      @return @a this.
  */
  self &operator++();

  /// Check for empty view.
  /// @return @c true if the view has a zero pointer @b or size.
  bool operator!() const;

  /// Check for non-empty view.
  /// @return @c true if the view refers to a non-empty range of bytes.
  explicit operator bool() const;

  /// Check for empty view (no content).
  /// @see operator bool
  bool is_empty() const;

  /// @name Accessors.
  //@{
  /// Pointer to the first byte in the view.
  const char *begin() const;
  /// Pointer to first byte not in the view.
  const char *end() const;
  /// Number of bytes in the view.
  size_t size() const;
  /// Memory pointer.
  /// @note This is equivalent to @c begin currently but it's probably good to have separation.
  const char *data() const;
  //@}

  /// Set the view.
  /// This is faster but equivalent to constructing a new view with the same
  /// arguments and assigning it.
  /// @return @c this.
  self &setView(const char *ptr, ///< Buffer address.
                size_t n = 0     ///< Buffer size.
                );

  /// Set the view.
  /// This is faster but equivalent to constructing a new view with the same
  /// arguments and assigning it.
  /// @return @c this.
  self &setView(const char *start, ///< First valid character.
                char const *end    ///< First invalid character.
                );

  /// Clear the view (become an empty view).
  self &clear();

  /// @return @c true if the byte at @a *p is in the view.
  bool contains(const char *p) const;

  /// @return the byte at offset @a n.
  char operator[](int n) const;

  /** Find a byte.
      @return A pointer to the first occurrence of @a c in @a this
      or @c nullptr if @a c is not found.
  */
  char const *find(char c) const;

  /** Find a byte.
      @return A pointer to the first occurence of any of @a delimiters in @a
      this or @c nullptr if not found.
  */
  char const *find(BufferView delimiters) const;

  /** Find a byte.
      @return A pointer to the first byte for which @a pred is @c true otherwise
     @c nullptr.
  */
  const char *find(std::function<bool(char)> const &pred);

  /** Get the initial segment of the view before @a p.

      The byte at @a p is not included. If @a p is not in the view an empty view
     is returned.

      @return A buffer that contains all data before @a p.
  */
  self prefix(const char *p) const;

  /** Split the view on the character at @a p.

      The view is split in to two parts and the byte at @a p is discarded. @a this retains all data
      @b after @a p (equivalent to <tt>BufferView(p+1, this->end()</tt>). A new view containing the
      initial bytes up to but not including @a p is returned, (equivalent to
      <tt>BufferView(this->begin(), p)</tt>).

      This is convenient when tokenizing and @a p points at a delimiter.

      @note If @a *p refers toa byte that is not in @a this then @a this is not changed and an empty
      buffer is returned. Therefore this method can be safely called with the return value of
      calling @c find.

      @code
        void f(BufferView& text) {
          BufferView token = text.splitPrefix(text.find(delimiter));
          if (token) { // ... process token }
      @endcode

      @return A buffer containing data up to but not including @a p.

      @see extractPrefix
  */
  self splitPrefix(const char *p);

  /** Extract a prefix delimited by @a p.

      A prefix of @a this is removed from the view and returned. If @a p is not in the view then the
      entire view is extracted and returned.

      If @a p points at a byte in the view this is identical to @c splitPrefix.  If not then the
      entire view in @a this will be returned and @a this will become an empty view. This is easier
      to use when repeated extracting tokens. The source view will become empty after extracting the
      last token.

      @code
      BufferView text;
      while (text) {
        BufferView token = text.extractPrefix(text.find(delimiter));
        // .. process token which will always be non-empty because text was not empty.
      }
      @endcode

      @return The prefix bounded at @a p or the entire view if @a p is not a byte in the view.

      @see splitPrefix
  */
  self extractPrefix(const char *p);

  /** Get the trailing segment of the view after @a p.

      The byte at @a p is not included. If @a p is not in the view an empty view is returned.

      @return A buffer that contains all data after @a p.
  */
  self suffix(const char *p) const;

  /** Split the view on the character at @a p.

      The view is split in to two parts and the byte at @a p is discarded. @a this retains all data
      @b before @a p (equivalent to <tt>BufferView(this->begin(), p)</tt>). A new view containing
      the trailing bytes after @a p is returned, (equivalent to <tt>BufferView(p+1,
      this->end())</tt>).

      @note If @a p does not refer to a byte in the view, an empty view is returned and @a this is
      unchanged.

      @return @a this.
  */
  self splitSuffix(const char *p);

  // Functors for using this class in STL containers.
  /// Ordering functor, lexicographic comparison.
  struct LessThan {
    bool
    operator()(BufferView const &lhs, BufferView const &rhs)
    {
      return -1 == compare(lhs, rhs);
    }
  };
  /// Ordering functor, lexicographic case insensitive comparison.
  struct LessThanNoCase {
    bool
    operator()(BufferView const &lhs, BufferView const &rhs)
    {
      return -1 == compare_nocase(lhs, rhs);
    }
  };
};

// ----------------------------------------------------------
// Inline implementations.

inline constexpr BufferView::BufferView()
{
}
inline constexpr BufferView::BufferView(char const *ptr, size_t n) : _ptr(ptr), _size(n)
{
}
inline constexpr BufferView::BufferView(char const *start, char const *end) : _ptr(start), _size(end - start)
{
}
inline constexpr BufferView::BufferView(char const *s) : _ptr(s), _size(strlen(s))
{
}

inline BufferView &
BufferView::setView(const char *ptr, size_t n)
{
  _ptr  = ptr;
  _size = n;
  return *this;
}

inline BufferView &
BufferView::setView(const char *ptr, const char *limit)
{
  _ptr  = ptr;
  _size = limit - ptr;
  return *this;
}

inline BufferView &
BufferView::clear()
{
  _ptr  = 0;
  _size = 0;
  return *this;
}

inline bool
BufferView::operator==(self const &that) const
{
  return _size == that._size && _ptr == that._ptr;
}

inline bool
BufferView::operator!=(self const &that) const
{
  return !(*this == that);
}

inline bool BufferView::operator!() const
{
  return !(_ptr && _size);
}

inline BufferView::operator bool() const
{
  return _ptr && _size;
}

inline bool
BufferView::is_empty() const
{
  return !(_ptr && _size);
}

inline char BufferView::operator*() const
{
  return *_ptr;
}

inline BufferView &BufferView::operator++()
{
  ++_ptr;
  --_size;
  return *this;
}

inline const char *
BufferView::begin() const
{
  return _ptr;
}
inline const char *
BufferView::data() const
{
  return _ptr;
}

inline const char *
BufferView::end() const
{
  return _ptr + _size;
}

inline size_t
BufferView::size() const
{
  return _size;
}

inline BufferView &
BufferView::operator=(BufferView const &that)
{
  _ptr  = that._ptr;
  _size = that._size;
  return *this;
}

inline char BufferView::operator[](int n) const
{
  return _ptr[n];
}

inline bool
BufferView::contains(char const *p) const
{
  return _ptr <= p && p < _ptr + _size;
}

inline BufferView
BufferView::prefix(char const *p) const
{
  self zret;
  if (this->contains(p))
    zret.setView(_ptr, p);
  return zret;
}

inline BufferView
BufferView::splitPrefix(char const *p)
{
  self zret; // default to empty return.
  if (this->contains(p)) {
    zret.setView(_ptr, p);
    this->setView(p + 1, this->end());
  }
  return zret;
}

inline BufferView
BufferView::extractPrefix(char const *p)
{
  self zret{this->splitPrefix(p)};

  // For extraction if zret is empty, use up all of @a this
  if (!zret) {
    zret = *this;
    this->clear();
  }

  return zret;
}

inline BufferView
BufferView::suffix(char const *p) const
{
  self zret;
  if (this->contains(p))
    zret.setView(p + 1, _ptr + _size);
  return zret;
}

inline BufferView
BufferView::splitSuffix(const char *p)
{
  self zret;
  if (this->contains(p)) {
    zret.setView(p + 1, this->end());
    this->setView(_ptr, p);
  }
  return zret;
}

inline char const *
BufferView::find(char c) const
{
  return static_cast<char const *>(memchr(_ptr, c, _size));
}

inline char const *
BufferView::find(self delimiters) const
{
  std::bitset<256> valid;

  // Load the bits in the array. This should be faster because this iterates
  // over the
  // delimiters exactly once instead of for each byte in @a this.
  for (char c : delimiters)
    valid[static_cast<uint8_t>(c)] = true;

  for (const char *p = this->begin(), *limit = this->end(); p < limit; ++p)
    if (valid[static_cast<uint8_t>(*p)])
      return p;

  return nullptr;
}

inline const char *
BufferView::find(std::function<bool(char)> const &pred)
{
  for (const char *p = this->begin(), *limit = this->end(); p < limit; ++p)
    if (pred(*p))
      return p;
  return nullptr;
}

namespace detail
{
  // These are templated in order to not require including std::ostream but only
  // std::iosfwd.
  // Templates let the use of specific stream mechanisms be delayed until use at
  // which point
  // the caller will have included the required headers if needed but callers who
  // don't won't need to.

  template <typename Stream>
  void
  stream_padding(Stream &os, std::size_t n)
  {
    static constexpr size_t pad_size = 8;
    char padding[pad_size];

    std::fill_n(padding, pad_size, os.fill());
    for (; n >= pad_size && os.good(); n -= pad_size)
      os.write(padding, pad_size);
    if (n > 0 && os.good())
      os.write(padding, n);
  }

  template <typename Stream>
  void
  aligned_stream_write(Stream &os, const BufferView &b)
  {
    const std::size_t size           = b.size();
    const std::size_t alignment_size = static_cast<std::size_t>(os.width()) - size;
    const bool align_left            = (os.flags() & Stream::adjustfield) == Stream::left;
    if (!align_left) {
      detail::stream_padding(os, alignment_size);
      if (os.good())
        os.write(b.begin(), size);
    } else {
      os.write(b.begin(), size);
      if (os.good())
        detail::stream_padding(os, alignment_size);
    }
  }

  extern template void stream_padding(std::ostream &, std::size_t);
  extern template void aligned_stream_write(std::ostream &, const BufferView &);

} // detail

// Inserter
template <typename Stream>
Stream &
operator<<(Stream &os, const BufferView &b)
{
  if (os.good()) {
    const std::size_t size = b.size();
    const std::size_t w    = static_cast<std::size_t>(os.width());
    if (w <= size)
      os.write(b.begin(), size);
    else
      detail::aligned_stream_write<Stream>(os, b);
    os.width(0);
  }
  return os;
}

extern template std::ostream &operator<<(std::ostream &, const BufferView &);

} // end namespace

#endif // TS_BUFFER_HEADER
