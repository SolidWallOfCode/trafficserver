/** @file

  A brief file description

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

#ifndef __CACHE_HTTP_H__
#define __CACHE_HTTP_H__

#include "P_CacheArray.h"
#ifdef HTTP_CACHE
#include "HTTP.h"
#include "URL.h"


typedef URL CacheURL;
typedef HTTPHdr CacheHTTPHdr;
typedef HTTPInfo CacheHTTPInfo;

#define OFFSET_BITS 24
enum
{
  OWNER_NONE = 0,
  OWNER_CACHE = 1,
  OWNER_HTTP = 2
};

#else
struct CacheHTTPInfo
{
};

#endif //HTTP_CACHE

LINK_FORWARD_DECLARATION(CacheVC, OpenDir_Link) // forward declaration
LINK_FORWARD_DECLARATION(CacheVC, Active_Link) // forward declaration

struct CacheHTTPInfoVector
{
  typedef CacheHTTPInfoVector self; ///< Self reference type.

  struct Item
  {
    /// Descriptor for an alternate for this object.
    CacheHTTPInfo _alternate;
    /// CacheVCs which are writing data to this alternate.
    DLL<CacheVC, Link_CacheVC_OpenDir_Link> _writers;
    ///@{ Active I/O
    /** These two lists tracks active / outstanding I/O operations on The @a _active list is for writers
	and the CacheVC should be on this list iff it has initiated an I/O that has not yet
	completed. The @a _waiting list is for CacheVCs that are waiting for a fragment that is being written
	by a CacheVC on the @a _active list. That is, it is waiting on the same I/O operation as the @a _active
	CacheVC.

	@internal An alternative implementation would be to have an array with an element for each fragment. With
	this scheme we will have to linear search these lists to find the corresponding fragment I/O if any.
	However, these lists should be short (only very rarely more than 1 or 2) and an array, given the ever
	larger objects to be stored, would be large and require allocation. For these reasons I think this is the
	better choice.
    */
    /// CacheVCs with pending write I/O.
    DLL<CacheVC, Link_CacheVC_Active_Link> _active;
    /// CacheVCs waiting on fragments.
    DLL<CacheVC, Link_CacheVC_Active_Link> _waiting;
    // To minimize list walking, we track the convex hull of fragments for which readers are waiting.
    // We update the values whenever we must actually walk the list.
    // Otherwise we maintain the convex hull invariant so if a written fragment is outside the range,
    // we can assume no reader was waiting for it.
    /// lowest fragment index for which a reader is waiting.
    int _wait_idx_min;
    /// highest fragment inddex for which a reader is waiting.
    int _wait_idx_max;
    ///@}
  };

  typedef CacheArray<Item> InfoVector;

  void *magic;

  CacheHTTPInfoVector();
  ~CacheHTTPInfoVector();

  int count()
  {
    return xcount;
  }

  int insert(CacheHTTPInfo * info, int id = -1);
  CacheHTTPInfo *get(int idx);
  void detach(int idx, CacheHTTPInfo * r);
  void remove(int idx, bool destroy);
  void clear(bool destroy = true);
  void reset()
  {
    xcount = 0;
    data.clear();
  }
  void print(char *buffer, size_t buf_size, bool temps = true);

  int marshal_length();
  int marshal(char *buf, int length);
  uint32_t get_handles(const char *buf, int length, RefCountObj * block_ptr = NULL);

  /// Get the alternate index for the @a key.
  int index_of(CacheKey const& key);
  /// Mark a @c CacheVC as actively writing at @a offset on the alternate with @a alt_key.
  self& write_active(CacheKey const& alt_key, CacheVC* vc, int64_t offset);
  /// Mark an active write by @a vc as complete and indicate whether it had @a success.
  /// If the write is not @a success then the fragment is not marked as cached.
  self& write_complete(CacheKey const& alt_key, CacheVC* vc, bool success = true);
  /// Indicate if a VC is currently writing to the fragment with this @a offset.
  bool is_write_active(CacheKey const& alt_key, int64_t offset);
  /// Mark a CacheVC as waiting for the fragment containing the byte at @a offset.
  self& waiting_for(CacheKey const& alt_key, CacheVC* vc, int64_t offset);
  /// Get the fragment key for a specific @a offset.
  CacheKey const& key_for(CacheKey const& alt_key, int64_t offset);
  /** Compute the convex hull of the uncached parts of the @a request taking current writers in to account.

      @return @c true if there is uncached data that must be retrieved.
   */
  HTTPRangeSpec::Range get_uncached_hull(CacheKey const& alt_key, HTTPRangeSpec const& request);

  /** Sigh, yet another custom array class.
      @c Vec doesn't work because it really only works well with pointers, not objects.
  */
  InfoVector data;

  int xcount;
  Ptr<RefCountObj> vector_buf;
};

/** Range operation tracking.

    This holds a range specification. It also tracks the current object offset and the individual range.

    For simplification of the logic that uses this class it will pretend to be a single range of
    the object size if it is empty. To return the correct response we still need to distinuish
    those two cases.
*/
class CacheRange
{
 public:
  typedef CacheRange self; ///< Self reference type.

  /// Default constructor
 CacheRange() : _offset(0), _idx(-1), _ct_field(NULL), _pending_range_shift_p(false) { }

  /// Test if the range spec has actual ranges in it
  bool hasRanges() const;

  /// Test for multiple ranges.
  bool isMulti() const;

  /// Get the current object offset
  uint64_t getOffset() const;

  /// Get the current range index.
  int getIdx() const;

  /// Get the number of ranges.
  size_t count() const;

  /// Get the remaining contiguous bytes for the current range.
  uint64_t getRemnantSize() const;

  /** Advance @a size bytes in the range spec.

      @return The resulting offset in the object.
  */
  uint64_t consume(uint64_t size);

  /** Initialize from a request header.
   */
  bool init(HTTPHdr* req);

  /** Set the range to the start of the range set.
      @return @c true if there is a valid range, @c false otherwise.
  */
  bool start();

  /** Apply a content @a len to the ranges.

      @return @c true if successfully applied, @c false otherwise.
  */
  bool apply(uint64_t len);

  /** Get the range boundary string.
      @a len if not @c NULL receives the length of the string.
  */
  char const* getBoundaryStr(int* len) const;

  /** Generate the range boundary string */
  self& generateBoundaryStr(CacheKey const& key);

  /// Get the cached Content-Type field.
  MIMEField* getContentTypeField() const;

  /// Set the Content-Type field from a response header.
  self& setContentTypeFromResponse(HTTPHdr* resp);

  /** Calculate the effective HTTP content length value.
   */
  uint64_t calcContentLength() const;

  /// Raw access to internal range spec.
  HTTPRangeSpec& getRangeSpec();

  /// Test if a consume moved across a range boundary.
  bool hasPendingRangeShift() const;

  /// Clear the pending range shift flag.
  self& consumeRangeShift();

  /// Range access.
  HTTPRangeSpec::Range& operator [] (int n);

  /// Range access.
  HTTPRangeSpec::Range const& operator [] (int n) const;

  /// Reset to re-usable state.
  void clear();

 protected:
  uint64_t _len; ///< Total object length.
  uint64_t _offset; ///< Offset in content.
  int _idx; ///< Current range index. (< 0 means not in a range)
  HTTPRangeSpec _r; ///< The actual ranges.
  MIMEField* _ct_field; ///< Content-Type field.
  char _boundary[HTTP_RANGE_BOUNDARY_LEN];
  bool _pending_range_shift_p;
};

TS_INLINE CacheHTTPInfo *
CacheHTTPInfoVector::get(int idx)
{
  ink_assert(idx >= 0);
  ink_assert(idx < xcount);
  return &data[idx]._alternate;
}

inline bool
CacheRange::hasRanges() const
{
  return _r.isSingle() || _r.isMulti();
}

inline uint64_t
CacheRange::getOffset() const
{
  return _offset;
}

inline int
CacheRange::getIdx() const
{
  return _idx;
}

inline uint64_t
CacheRange::getRemnantSize() const
{
  uint64_t zret = 0;

  if (_r.isEmpty())
    zret = _len - _offset;
  else if (_r.isValid() && 0 <= _idx && _idx < static_cast<int>(_r.count()))
    zret = (_r[_idx]._max - _offset) + 1;

  return zret;
}

inline char const*
CacheRange::getBoundaryStr(int* len) const
{
  if (len) *len = sizeof(_boundary);
  return _boundary;
}

inline HTTPRangeSpec&
CacheRange::getRangeSpec()
{
  return _r;
}

inline bool
CacheRange::isMulti() const
{
  return _r.isMulti();
}

inline bool
CacheRange::hasPendingRangeShift() const
{
  return _pending_range_shift_p;
}

inline CacheRange&
CacheRange::consumeRangeShift()
{
  _pending_range_shift_p = false;
  return *this;
}

inline MIMEField*
CacheRange::getContentTypeField() const
{
  return _ct_field;
}

inline size_t
CacheRange::count() const
{
  return _r.count();
}

inline HTTPRangeSpec::Range&
CacheRange::operator [] (int n)
{
  return _r[n];
}

inline HTTPRangeSpec::Range const&
CacheRange::operator [] (int n) const
{
  return _r[n];
}

inline CacheRange&
CacheRange::setContentTypeFromResponse(HTTPHdr* resp)
{
  _ct_field = resp->field_find(MIME_FIELD_CONTENT_TYPE, MIME_LEN_CONTENT_TYPE);
  return *this;
}

#endif /* __CACHE_HTTP_H__ */
