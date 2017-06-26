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

#include "ts/ink_config.h"
#include <string.h>
#include "P_Cache.h"

#include <ts/TestBox.h>

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

void
CacheHTTPInfoVector::Slice::addSideBuffer(IOBufferBlock *block, int64_t position, int64_t length)
{
  // Blend in to overlapping existing buffer or insert in order.
  CacheBuffer *cb = _content_buffers.head;
  int64_t last    = position + length;

  // Always create a new cache buffer. Existing intersecting buffers will be coalesced in to this.
  CacheBuffer *n = new CacheBuffer;
  n->_position   = position;

  while (cb && length) {
    int64_t cb_last = cb->_position + cb->_data.length();

    // No intersection, before all remaining buffers, write it all and finish.
    if (last < cb->_position) {
      n->_data.write(block, length);
      length = 0;
    } else if (position <= cb_last) {                // intersection - write something.
      CacheBuffer *next = _content_buffers.next(cb); // save this for end of scope update.
      if (cb->_position < position) {                // copy over leading part of existing data buffer.
        n->_data.write(cb->_data.head(), position - cb->_position);
        n->_position = cb->_position;
      }
      // Invariant - valid incoming data starts no later than existing valid data that's not in the new buffer.
      if (last < cb_last) { // incoming ends first. Write all of it, then non-intersecting tail of existing buffer.
        n->_data.write(block, length);
        n->_data.write(cb->_data.head(), cb_last - last, last - cb->_position);
        length = 0;
      } // else just drop existing buffer, it's covered. Incoming gets written later.
      // Existing buffer has been copied in to the new buffer, clean it up.
      _content_buffers.remove(cb);
      delete cb;
      cb = next;
    } else { // no intersection, check the next buffer.
      cb = _content_buffers.next(cb);
    }
  }

  // If the incoming data hasn't been written yet, take care of it.
  if (length)
    n->_data.write(block, length);
  if (cb) // there's an existing buffer that starts after the end of the new buffer.
    _content_buffers.insert(n, _content_buffers.prev(cb)); // insert after previous -> insert before.
  else                                                     // No buffers start after incoming buffer.
    _content_buffers.enqueue(n);
}

bool
CacheHTTPInfoVector::Slice::getSideBufferContent(IOBufferChain &data, int64_t position, int64_t length)
{
  CacheBuffer *cb = _content_buffers.head;

  while (NULL != cb) {
    if (cb->_position <= position && cb->_position + cb->_data.length() >= position + length) {
      data.write(cb->_data.head(), length, position - cb->_position);
      return true;
    }
    cb = _content_buffers.next(cb);
  }
  return false;
}

CacheHTTPInfoVector::Slice::~Slice()
{
  CacheBuffer *cb = _content_buffers.head;
  while (cb) {
    CacheBuffer *next = _content_buffers.next(cb);
    delete cb;
    cb = next;
  }
  _alternate.destroy();
}
/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

#ifdef HTTP_CACHE

CacheHTTPInfoVector::CacheHTTPInfoVector()
{
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

CacheHTTPInfoVector::~CacheHTTPInfoVector()
{
  for ( auto& group : data ) {
    for ( auto& slice : group )
      slice.~Slice();
  }
  data.clear();
  vector_buf.clear();
  magic = nullptr;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/
CacheHTTPInfoVector::Slice*
CacheHTTPInfoVector::alloc_slice(int& idx)
{
  Slice* slice;

  if (CACHE_ALT_INDEX_DEFAULT == idx) {
    idx = data.size();
    data.resize(idx+1);
  }
  // See if there is a pre-allocated available.
  for (s& : fixed_slices) {
    if (s._alternate.valid()) {
      slice = &s;
      break;
    }
  }

  if (!slice) { // no pre-allocated, do a real allocation.
    slice = new Slice;
  }

  data[idx].push_front(slice);
  return slice;
}
/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
CacheHTTPInfoVector::insert(CacheHTTPInfo *info, int index)
{
  Slice* slice = this->alloc_slice(index);
  slice->_alternate.copy_shallow(info);
  return index;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

void
CacheHTTPInfoVector::detach(int idx, CacheHTTPInfo *r)
{
  ink_assert(idx >= 0);
  ink_assert(idx < data.size());

  # if 0
  r->copy_shallow(&data[idx]._alternate);
  data[idx]._alternate.destroy();

  for (i = idx; i < (xcount - 1); i++) {
    data[i] = data[i + i];
  }
  # else
  ink_release_assert(false); // this needs to be implemented.
  # endif
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

void
CacheHTTPInfoVector::remove(int idx, bool destroy)
{
  # if 0
  if (destroy)
    data[idx]._alternate.destroy();

  for (; idx < (xcount - 1); idx++)
    data[idx] = data[idx + 1];

  xcount--;
  # else
  ink_release_assert(false);
  # endif
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

void
CacheHTTPInfoVector::clear(bool destroy)
{
  # if 0
  int i;

  if (destroy) {
    for (i = 0; i < xcount; i++) {
      data[i]._alternate.destroy();
    }
  }
  xcount = 0;
  data.clear();
  vector_buf.clear();
  # else
  ink_release_assert(false);
  # endif
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

void
CacheHTTPInfoVector::clean()
{
  # if 0
  int idx, dst, n = xcount;

  for (dst = idx = 0; idx < n; ++idx) {
    if (data[idx]._alternate.m_alt->m_earliest.m_flag.cached_p) {
      if (dst != idx)
        data[dst] = data[idx];
      ++dst;
    }
  }
  xcount = dst;
  # else
  ink_release_assert(false);
  # endif
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

void
CacheHTTPInfoVector::print(char *buffer, size_t buf_size, bool temps)
{
  char buf[33], *p;
  int purl;
  int tmp;

  p    = buffer;
  purl = 1;

  for ( auto& group : data ) {
    for ( auto& slice : group ) {
      if (slice._alternate.valid()) {
        if (purl) {
          Arena arena;
          char *url;

          purl = 0;
          URL u;
          slice._alternate.request_url_get(&u);
          url = u.string_get(&arena);
          if (url) {
            snprintf(p, buf_size, "[%s] ", url);
            tmp = strlen(p);
            p += tmp;
            buf_size -= tmp;
          }
        }

        if (temps || !(slice._alternate.object_key_get() == zero_key)) {
          snprintf(p, buf_size, "[%d %s]", slice._alternate.id_get(), CacheKey(slice._alternate.object_key_get()).toHexStr(buf));
          tmp = strlen(p);
          p += tmp;
          buf_size -= tmp;
        }
      }
    }
  }
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
CacheHTTPInfoVector::marshal_length()
{
  int length = 0;

  for ( auto& group : data )
    length += group.marshal_length();

  return length;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/
int
CacheHTTPInfoVector::marshal(char *buf, int length)
{
  char *start = buf;
  int count   = 0;

  ink_assert(!(((intptr_t)buf) & 3)); // buf must be aligned

  for ( auto& group : data ) {
    int tmp = group.marshal(buf, length);
    length -= tmp;
    buf += tmp;
    count++;
  }

  GLOBAL_CACHE_SUM_GLOBAL_DYN_STAT(cache_hdr_vector_marshal_stat, 1);
  GLOBAL_CACHE_SUM_GLOBAL_DYN_STAT(cache_hdr_marshal_stat, count);
  GLOBAL_CACHE_SUM_GLOBAL_DYN_STAT(cache_hdr_marshal_bytes_stat, buf - start);
  return buf - start;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/
uint32_t
CacheHTTPInfoVector::get_handles(const char *buf, int length, RefCountObj *block_ptr)
{
  ink_assert(!(((intptr_t)buf) & 3)); // buf must be aligned

  const char *start = buf;
  CacheHTTPInfo info;

  vector_buf = block_ptr;

  while (length - (buf - start) > (int)sizeof(HTTPCacheAlt)) {
    int idx = CACHE_ALT_INDEX_DEFAULT;
    int tmp = info.get_handle((char *)buf, length - (buf - start));
    if (tmp < 0) {
      ink_assert(!"CacheHTTPInfoVector::unmarshal get_handle() failed");
      return (uint32_t)-1;
    }
    buf += tmp;

    Slice* slice = this->alloc_slice(idx);
    slice->_alternate = info;
  }

  return ((caddr_t)buf - (caddr_t)start);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
CacheHTTPInfoVector::index_of(CacheKey const &alt_key)
{
  for (int idx = 0, n = data.size(); idx < n ; ++idx) {
    auto& group = data[idx];
    if (group._slices.head && alt_key == group._slices.head->_alternate.object_key_get())
      return idx;
  }
  return CACHE_ALT_INDEX_DEFAULT;
}

CacheHTTPInfoVector::SliceRef
CacheHTTPInfoVector::slice_ref_for(CacheKey const& alt_key)
{
  SliceRef zret;

  for (int idx = 0, n = data.size(); idx < n ; ++idx) {
    for (auto& slice : data[idx]) {
      if (alt_key == slice._alternate.object_key_get()) {
        zret._idx = idx;
        zret._slice = &slice;
        zret._gen = slice._gen;
        return zret;
      }
    }
  }
  return zret;
}
/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

CacheKey const &
CacheHTTPInfoVector::key_for(CacheKey const &alt_key, int64_t offset)
{
  SliceRef sr = this->slice_ref_for(alt_key);
  return sr._slice->_alternate.get_frag_key_of(offset);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

CacheHTTPInfoVector &
CacheHTTPInfoVector::write_active(CacheKey const &alt_key, CacheVC *vc, int64_t offset)
{
  SliceRef sr = this->slice_ref_for(alt_key);

  Debug("amc", "[CacheHTTPInfoVector::write_active] VC %p write %" PRId64, vc, offset);

  sr._slice->_active.push(vc);
  return *this;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

CacheHTTPInfoVector &
CacheHTTPInfoVector::write_complete(CacheKey const &alt_key, CacheVC *vc, CacheBuffer const &cb, bool success)
{
  SliceRef sr = this->slice_ref_for(alt_key);
  CacheVC *reader;
  DLL<CacheVC, Link_CacheVC_Active_Link> waiters;
  static void *cookie = reinterpret_cast<void *>(0x56); // tracking value, not used.

  Debug("amc", "[CacheHTTPInfoVector::write_complete] VC %p write of %" PRId64 " bytes at %" PRId64 "  %s", vc, cb._data.length(),
        cb._position, (success ? "succeeded" : "failed"));

  sr._slice->_active.remove(vc);
  if (success)
    sr._slice->_alternate.mark_frag_write(vc->fragment);

  // Kick all the waiters, success or fail.
  std::swap(waiters, sr._slice->_waiting); // moves all waiting VCs to local list.
  while (NULL != (reader = waiters.pop())) {
    if (reader->fragment == vc->fragment) {
      Debug("amc", "[write_complete] wake up %p", reader);
      reader->wait_buffer   = cb._data;
      reader->wait_position = cb._position;
      reader->wake_up(EVENT_IMMEDIATE, cookie);
      //      reader->wake_up_thread->schedule_imm(reader)->cookie = reinterpret_cast<void *>(0x56);
    } else {
      sr._slice->_waiting.push(reader); // not waiting for this, put it back.
    }
  }

  return *this;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

CacheHTTPInfoVector &
CacheHTTPInfoVector::addSideBuffer(CacheKey const &alt_key, IOBufferBlock *block, int64_t len, int64_t position)
{
  SliceRef sr = this->slice_ref_for(alt_key);
  sr._slice->addSideBuffer(block, position, len);
  return *this;
}

bool
CacheHTTPInfoVector::getSideBufferContent(CacheKey const &alt_key, IOBufferChain &chain, int64_t position, int64_t length)
{
  SliceRef sr = this->slice_ref_for(alt_key);
  return sr._slice->getSideBufferContent(chain, position, length);
}

bool
CacheHTTPInfoVector::has_writer(CacheKey const &alt_key)
{
  SliceRef sr = this->slice_ref_for(alt_key);
  return sr._idx >= 0 && sr._slice->_writers.head != nullptr;
}

bool
CacheHTTPInfoVector::is_write_active(CacheKey const &alt_key, int64_t offset)
{
  SliceRef sr = this->slice_ref_for(alt_key);
  int frag_idx = sr._slice->_alternate.get_frag_index_of(offset);
  for (CacheVC *vc = sr._slice->_active.head; vc; vc = sr._slice->_active.next(vc)) {
    if (vc->fragment == frag_idx)
      return true;
  }
  return false;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

bool
CacheHTTPInfoVector::wait_for(CacheKey const &alt_key, CacheVC *vc, int64_t offset)
{
  bool zret    = true;
  SliceRef sr = this->slice_ref_for(alt_key);
  Slice& item = *(sr._slice);
  int frag_idx = item._alternate.get_frag_index_of(offset);
  ink_assert(vc->fragment == frag_idx);
  //  vc->fragment = frag_idx; // really? Shouldn't this already be set?
  if (item.has_writers()) {
    if (!item._waiting.in(vc))
      item._waiting.push(vc);
  } else {
    zret = false;
  }
  return zret;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

CacheHTTPInfoVector &
CacheHTTPInfoVector::close_writer(CacheKey const &alt_key, CacheVC *vc)
{
  CacheVC *reader;
  SliceRef sr = this->slice_ref_for(alt_key);
  // If the writer aborts before before the transaction completes, it won't have an ALT assigned.
  if (sr._idx != CACHE_ALT_INDEX_DEFAULT) {
    Slice& slice = *(sr._slice);
    slice._writers.remove(vc);
    if (slice._writers.empty()) {
      // if there are no more writers, none of these will ever wake up normally so kick them all now.
      while (NULL != (reader = slice._waiting.pop())) {
        Debug("amc", "[close_writer] no writers left wake up %p", reader);
        reader->wake_up_thread->schedule_imm(reader)->cookie = reinterpret_cast<void *>(0x112);
      }
    }
  }
  return *this;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

HTTPRangeSpec::Range
CacheHTTPInfoVector::get_uncached_hull(CacheKey const &alt_key, HTTPRangeSpec const &req, int64_t initial)
{
  SliceRef sr = this->slice_ref_for(alt_key);
  Slice& slice = *(sr._slice);
  Queue<CacheVC, Link_CacheVC_OpenDir_Link> writers;
  CacheVC *vc;
  CacheVC *cycle_vc = NULL;
  // Yeah, this need to be tunable.
  int64_t DELTA = slice._alternate.get_frag_fixed_size() * 16;
  HTTPRangeSpec::Range r(slice._alternate.get_uncached_hull(req, initial));

  if (r.isValid()) {
    /* Now clip against the writers.
       We move all the writers to a local list and move them back as we are done using them to clip.
       This is so we don't skip a potentially valid writer because they are not in start order.
    */
    writers.append(slice._writers);
    slice._writers.clear();
    while (r._min < r._max && NULL != (vc = writers.pop())) {
      int64_t base  = static_cast<int64_t>(writers.head->resp_range.getOffset());
      int64_t delta = static_cast<int64_t>(writers.head->resp_range.getRemnantSize());

      if (base + delta < r._min || base > r._max) {
        slice._writers.push(vc); // of no use to us, just put it back.
      } else if (base < r._min + DELTA) {
        r._min = base + delta;     // we can wait, so depend on this writer and clip.
        slice._writers.push(vc);    // we're done with it, put it back.
        cycle_vc = NULL;           // we did something so clear cycle indicator
      } else if (vc == cycle_vc) { // we're looping.
        // put everyone back and drop out of the loop.
        slice._writers.push(vc);
        while (NULL != (vc = writers.pop()))
          slice._writers.push(vc);
        break;
      } else {
        writers.enqueue(vc); // put it back to later checking.
        if (NULL == cycle_vc)
          cycle_vc = vc; // but keep an eye out for it coming around again.
      }
    }
  }
  return r;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

void
CacheRange::clear()
{
  _offset                = 0;
  _idx                   = -1;
  _pending_range_shift_p = false;
  _ct_field              = NULL; // need to do real cleanup at some point.
  _r.clear();
}

bool
CacheRange::init(HTTPHdr *req)
{
  bool zret     = true;
  MIMEField *rf = req->field_find(MIME_FIELD_RANGE, MIME_LEN_RANGE);
  if (rf) {
    int len;
    char const *val = rf->value_get(&len);
    zret            = _r.parseRangeFieldValue(val, len);
  }
  return zret;
}

bool
CacheRange::start()
{
  bool zret = true;

  if (_r.hasRanges()) {
    _offset = _r[_idx = 0]._min;
    _pending_range_shift_p = _r.isMulti();
  } else if (_r.isEmpty()) {
    _offset = 0;
  } else {
    zret = false;
  }
  return zret;
}

bool
CacheRange::resolve(int64_t len)
{
  bool zret = false;
  if (len < 0) {
    if (!_r.hasOpenRange()) {
      zret        = true;
      _resolved_p = true;
    }
  } else {
    zret = _r.apply(len);
    if (zret) {
      _len        = len;
      _resolved_p = true;
      if (_r.hasRanges()) {
        _offset = _r[_idx = 0]._min;
        if (_r.isMulti())
          _pending_range_shift_p = true;
      }
    }
  }
  return zret;
}

uint64_t
CacheRange::consume(int64_t size)
{
  switch (_r._state) {
  case HTTPRangeSpec::EMPTY:
    _offset += size;
    break;
  case HTTPRangeSpec::SINGLE:
    _offset += std::min(size, (_r._single._max - _offset) + 1);
    break;
  case HTTPRangeSpec::MULTI:
    ink_assert(_idx < static_cast<int>(_r.count()));
    // Must not consume more than 1 range or the boundary strings won't get sent.
    ink_assert(!_pending_range_shift_p);
    ink_assert(size <= (_r[_idx]._max - _offset) + 1);
    _offset += size;
    if (_offset > _r[_idx]._max && ++_idx < static_cast<int>(_r.count())) {
      _offset                = _r[_idx]._min;
      _pending_range_shift_p = true;
    }
    break;
  default:
    break;
  }

  return _offset;
}

CacheRange &
CacheRange::generateBoundaryStr(CacheKey const &key)
{
  uint64_t rnd = this_ethread()->generator.random();
  snprintf(_boundary, sizeof(_boundary), "%016" PRIx64 "%016" PRIx64 "..%016" PRIx64, key.slice64(0), key.slice64(1), rnd);
  // GAH! snprintf null terminates so we can't actually print the last nybble that way and all of
  // the internal hex converters do the same thing. This is crazy code I need to fix at some point.
  // It is critical to print every nybble or the content lengths won't add up.
  _boundary[HTTP_RANGE_BOUNDARY_LEN - 1] = "0123456789abcdef"[rnd & 0xf];
  return *this;
}

uint64_t
CacheRange::calcContentLength() const
{
  return _r.calcContentLength(_len, _ct_field ? _ct_field->m_len_value : 0);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

REGRESSION_TEST(CacheSideContent)(RegressionTest *t, int, int *pstatus)
{
  TestBox box(t, pstatus, REGRESSION_TEST_PASSED);
  CacheHTTPInfoVector::Slice item;
  IOBufferChain data;
  IOBufferBlock src;

  src.alloc(BUFFER_SIZE_INDEX_64K);

  memset(src.start(), 0xaa, 200);
  src.fill(200);
  item.addSideBuffer(&src, 100, 200); // [100,300)

  src.reset();
  memset(src.start(), 0xbb, 300);
  src.fill(300);
  item.addSideBuffer(&src, 1000, 300); // [100,300) [1000,1300)

  box.check(item.getSideBufferContent(data, 150, 50), "False negative at [150,200)");
  box.check(!item.getSideBufferContent(data, 600, 200), "False positive at [600,800)");
  box.check(!item.getSideBufferContent(data, 100, 300), "False positive at [100,400)");

  src.reset();
  memset(src.start(), 0xcc, 700);
  src.fill(700);
  item.addSideBuffer(&src, 300, 700); // [300,1000) -> [100,1300)
  box.check(item.getSideBufferContent(data, 100, 300), "False negative at [100,400)");
  box.check(item.getSideBufferContent(data, 400, 400), "False negative at [400,800)");
  box.check(!item.getSideBufferContent(data, 200, 1500), "False positive at [200,1700)");

  src.reset();
  memset(src.start(), 0xdd, 500);
  src.fill(800);
  item.addSideBuffer(&src, 2000, 400); // [100,1300) [2000, 2400)
  item.addSideBuffer(&src, 3000, 500); // [100,1300) [2000, 2400) [3000, 3500)
  box.check(item.getSideBufferContent(data, 2000, 300), "False negative at [2000,2300)");
  box.check(!item.getSideBufferContent(data, 2001, 400), "False positive at [2001,2501)");
  box.check(item.getSideBufferContent(data, 1200, 100), "False negative at [1200,1300)");
  box.check(!item.getSideBufferContent(data, 1200, 300), "False positive at [1200,1500)");
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

#else // HTTP_CACHE

CacheHTTPInfoVector::CacheHTTPInfoVector() : data(&default_vec_info, 4), xcount(0)
{
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

CacheHTTPInfoVector::~CacheHTTPInfoVector()
{
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
CacheHTTPInfoVector::insert(CacheHTTPInfo * /* info ATS_UNUSED */, int index)
{
  ink_assert(0);
  return index;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

void
CacheHTTPInfoVector::detach(int /* idx ATS_UNUSED */, CacheHTTPInfo * /* r ATS_UNUSED */)
{
  ink_assert(0);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

void
CacheHTTPInfoVector::remove(int /* idx ATS_UNUSED */, bool /* destroy ATS_UNUSED */)
{
  ink_assert(0);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

void
CacheHTTPInfoVector::clear(bool /* destroy ATS_UNUSED */)
{
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

void
CacheHTTPInfoVector::print(char * /* buffer ATS_UNUSED */, size_t /* buf_size ATS_UNUSED */, bool /* temps ATS_UNUSED */)
{
  ink_assert(0);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

int
CacheHTTPInfoVector::marshal_length()
{
  ink_assert(0);
  return 0;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/
int
CacheHTTPInfoVector::marshal(char * /* buf ATS_UNUSED */, int length)
{
  ink_assert(0);
  return length;
}

int
CacheHTTPInfoVector::unmarshal(const char * /* buf ATS_UNUSED */, int /* length ATS_UNUSED */,
                               RefCountObj * /* block_ptr ATS_UNUSED */)
{
  ink_assert(0);
  return 0;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/
uint32_t
CacheHTTPInfoVector::get_handles(const char * /* buf ATS_UNUSED */, int /* length ATS_UNUSED */,
                                 RefCountObj * /* block_ptr ATS_UNUSED */)
{
  ink_assert(0);
  return 0;
}
/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

#endif // HTTP_CACHE
