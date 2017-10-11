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

#ifndef _P_CACHE_DIR_H__
#define _P_CACHE_DIR_H__

#include "P_CacheHttp.h"

struct Vol;
struct InterimCacheVol;
struct CacheVC;

/*
  Directory layout
*/

// Constants

#define DIR_TAG_WIDTH 12
#define DIR_MASK_TAG(_t) ((_t) & ((1 << DIR_TAG_WIDTH) - 1))
#define SIZEOF_DIR 10
#define ESTIMATED_OBJECT_SIZE 8000

#define MAX_DIR_SEGMENTS (32 * (1 << 16))
#define DIR_DEPTH 4
#define MAX_ENTRIES_PER_SEGMENT (1 << 16)
#define MAX_BUCKETS_PER_SEGMENT (MAX_ENTRIES_PER_SEGMENT / DIR_DEPTH)
#define DIR_SIZE_WIDTH 6
#define DIR_BLOCK_SIZES 4
#define DIR_BLOCK_SHIFT(_i) (3 * (_i))
#define DIR_BLOCK_SIZE(_i) (CACHE_BLOCK_SIZE << DIR_BLOCK_SHIFT(_i))
#define DIR_SIZE_WITH_BLOCK(_i) ((1 << DIR_SIZE_WIDTH) * DIR_BLOCK_SIZE(_i))
#define DIR_OFFSET_BITS 40
#define DIR_OFFSET_MAX ((((off_t)1) << DIR_OFFSET_BITS) - 1)

#define SYNC_MAX_WRITE (2 * 1024 * 1024)
#define SYNC_DELAY HRTIME_MSECONDS(500)
#define DO_NOT_REMOVE_THIS 0

// Debugging Options

//#define DO_CHECK_DIR_FAST
//#define DO_CHECK_DIR

// Macros

#ifdef DO_CHECK_DIR
#define CHECK_DIR(_d) ink_assert(check_dir(_d))
#else
#define CHECK_DIR(_d) ((void)0)
#endif

#define dir_index(_e, _i) ((Dir *)((char *)(_e)->dir + (SIZEOF_DIR * (_i))))
#define dir_assign(_e, _x)   \
  do {                       \
    (_e)->w[0] = (_x)->w[0]; \
    (_e)->w[1] = (_x)->w[1]; \
    (_e)->w[2] = (_x)->w[2]; \
    (_e)->w[3] = (_x)->w[3]; \
    (_e)->w[4] = (_x)->w[4]; \
  } while (0)
#define dir_assign_data(_e, _x)         \
  do {                                  \
    unsigned short next = dir_next(_e); \
    dir_assign(_e, _x);                 \
    dir_set_next(_e, next);             \
  } while (0)
// entry is valid
#define dir_valid(_d, _e) (_d->header->phase == dir_phase(_e) ? vol_in_phase_valid(_d, _e) : vol_out_of_phase_valid(_d, _e))
// entry is valid and outside of write aggregation region
#define dir_agg_valid(_d, _e) (_d->header->phase == dir_phase(_e) ? vol_in_phase_valid(_d, _e) : vol_out_of_phase_agg_valid(_d, _e))
// entry may be valid or overwritten in the last aggregated write
#define dir_write_valid(_d, _e) \
  (_d->header->phase == dir_phase(_e) ? vol_in_phase_valid(_d, _e) : vol_out_of_phase_write_valid(_d, _e))
#define dir_agg_buf_valid(_d, _e) (_d->header->phase == dir_phase(_e) && vol_in_phase_agg_buf_valid(_d, _e))
#define dir_is_empty(_e) (!dir_offset(_e))
#define dir_clear(_e) \
  do {                \
    (_e)->w[0] = 0;   \
    (_e)->w[1] = 0;   \
    (_e)->w[2] = 0;   \
    (_e)->w[3] = 0;   \
    (_e)->w[4] = 0;   \
  } while (0)
#define dir_clean(_e) dir_set_offset(_e, 0)
#define dir_segment(_s, _d) vol_dir_segment(_d, _s)

// OpenDir

#define OPEN_DIR_BUCKETS 256

struct EvacuationBlock;
typedef uint32_t DirInfo;

// Cache Directory

// INTERNAL: do not access these members directly, use the
// accessors below (e.g. dir_offset, dir_set_offset).
// These structures are stored in memory 2 byte aligned.
// The accessors prevent unaligned memory access which
// is often either less efficient or unsupported depending
// on the processor.
struct Dir {
#if DO_NOT_REMOVE_THIS
  // THE BIT-FIELD INTERPRETATION OF THIS STRUCT WHICH HAS TO
  // USE MACROS TO PREVENT UNALIGNED LOADS
  // bits are numbered from lowest in u16 to highest
  // always index as u16 to avoid byte order issues
  unsigned int offset : 24; // (0,1:0-7) 16M * 512 = 8GB
  unsigned int big : 2;     // (1:8-9) 512 << (3 * big)
  unsigned int size : 6;    // (1:10-15) 6**2 = 64, 64*512 = 32768 .. 64*256=16MB
  unsigned int tag : 12;    // (2:0-11) 2048 / 8 entries/bucket = .4%
  unsigned int phase : 1;   // (2:12)
  unsigned int head : 1;    // (2:13) first segment in a document
  unsigned int pinned : 1;  // (2:14)
  unsigned int token : 1;   // (2:15)
  unsigned int next : 16;   // (3)
  inku16 offset_high;       // 8GB * 65k = 0.5PB (4)
#else
  uint16_t w[5];
  Dir() { dir_clear(this); }
#endif
};

// INTERNAL: do not access these members directly, use the
// accessors below (e.g. dir_offset, dir_set_offset)
struct FreeDir {
#if DO_NOT_REMOVE_THIS
  // THE BIT-FIELD INTERPRETATION OF THIS STRUCT WHICH HAS TO
  // USE MACROS TO PREVENT UNALIGNED LOADS
  unsigned int offset : 24; // 0: empty
  unsigned int reserved : 8;
  unsigned int prev : 16; // (2)
  unsigned int next : 16; // (3)
  inku16 offset_high;     // 0: empty
#else
  uint16_t w[5];
  FreeDir() { dir_clear(this); }
#endif
};

#define dir_offset(_e) \
  ((int64_t)(((uint64_t)(_e)->w[0]) | (((uint64_t)((_e)->w[1] & 0xFF)) << 16) | (((uint64_t)(_e)->w[4]) << 24)))
#define dir_set_offset(_e, _o)                                              \
  do {                                                                      \
    (_e)->w[0] = (uint16_t)_o;                                              \
    (_e)->w[1] = (uint16_t)((((_o) >> 16) & 0xFF) | ((_e)->w[1] & 0xFF00)); \
    (_e)->w[4] = (uint16_t)((_o) >> 24);                                    \
  } while (0)
#define dir_bit(_e, _w, _b) ((uint32_t)(((_e)->w[_w] >> (_b)) & 1))
#define dir_set_bit(_e, _w, _b, _v) (_e)->w[_w] = (uint16_t)(((_e)->w[_w] & ~(1 << (_b))) | (((_v) ? 1 : 0) << (_b)))
#define dir_big(_e) ((uint32_t)((((_e)->w[1]) >> 8) & 0x3))
#define dir_set_big(_e, _v) (_e)->w[1] = (uint16_t)(((_e)->w[1] & 0xFCFF) | (((uint16_t)(_v)) & 0x3) << 8)
#define dir_size(_e) ((uint32_t)(((_e)->w[1]) >> 10))
#define dir_set_size(_e, _v) (_e)->w[1] = (uint16_t)(((_e)->w[1] & ((1 << 10) - 1)) | ((_v) << 10))
#define dir_set_approx_size(_e, _s)                   \
  do {                                                \
    if ((_s) <= DIR_SIZE_WITH_BLOCK(0)) {             \
      dir_set_big(_e, 0);                             \
      dir_set_size(_e, ((_s)-1) / DIR_BLOCK_SIZE(0)); \
    } else if ((_s) <= DIR_SIZE_WITH_BLOCK(1)) {      \
      dir_set_big(_e, 1);                             \
      dir_set_size(_e, ((_s)-1) / DIR_BLOCK_SIZE(1)); \
    } else if ((_s) <= DIR_SIZE_WITH_BLOCK(2)) {      \
      dir_set_big(_e, 2);                             \
      dir_set_size(_e, ((_s)-1) / DIR_BLOCK_SIZE(2)); \
    } else {                                          \
      dir_set_big(_e, 3);                             \
      dir_set_size(_e, ((_s)-1) / DIR_BLOCK_SIZE(3)); \
    }                                                 \
  } while (0)
#define dir_approx_size(_e) ((dir_size(_e) + 1) * DIR_BLOCK_SIZE(dir_big(_e)))
#define round_to_approx_dir_size(_s)      \
  (_s <= DIR_SIZE_WITH_BLOCK(0) ?         \
     ROUND_TO(_s, DIR_BLOCK_SIZE(0)) :    \
     (_s <= DIR_SIZE_WITH_BLOCK(1) ?      \
        ROUND_TO(_s, DIR_BLOCK_SIZE(1)) : \
        (_s <= DIR_SIZE_WITH_BLOCK(2) ? ROUND_TO(_s, DIR_BLOCK_SIZE(2)) : ROUND_TO(_s, DIR_BLOCK_SIZE(3)))))
#define dir_tag(_e) ((uint32_t)((_e)->w[2] & ((1 << DIR_TAG_WIDTH) - 1)))
#define dir_set_tag(_e, _t) \
  (_e)->w[2] = (uint16_t)(((_e)->w[2] & ~((1 << DIR_TAG_WIDTH) - 1)) | ((_t) & ((1 << DIR_TAG_WIDTH) - 1)))
#define dir_phase(_e) dir_bit(_e, 2, 12)
#define dir_set_phase(_e, _v) dir_set_bit(_e, 2, 12, _v)
#define dir_head(_e) dir_bit(_e, 2, 13)
#define dir_set_head(_e, _v) dir_set_bit(_e, 2, 13, _v)
#define dir_pinned(_e) dir_bit(_e, 2, 14)
#define dir_set_pinned(_e, _v) dir_set_bit(_e, 2, 14, _v)
#define dir_token(_e) dir_bit(_e, 2, 15)
#define dir_set_token(_e, _v) dir_set_bit(_e, 2, 15, _v)
#define dir_next(_e) (_e)->w[3]
#define dir_set_next(_e, _o) (_e)->w[3] = (uint16_t)(_o)
#define dir_prev(_e) (_e)->w[2]
#define dir_set_prev(_e, _o) (_e)->w[2] = (uint16_t)(_o)

struct OpenDirEntry {
  typedef OpenDirEntry self; ///< Self reference type.

  Ptr<ProxyMutex> mutex;

  /// Vector for the http document. Each writer maintains a pointer to this vector and writes it down to disk.
  CacheHTTPInfoVector vector;
  CacheKey first_key;         ///< Key for first doc for this object.
  CacheKey single_doc_key;    ///< Key for the resident alternate.
  Dir single_doc_dir;         ///< Directory for the resident alternate
  /// Dir for the vector. If empty, a new dir is inserted, otherwise this dir is overwritten
  Dir first_dir;
  Ptr<IOBufferData> first_buf; ///< First doc (vector) fragment.
  uint16_t num_active;        ///< num of VCs working with this entry
  bool dont_update_directory; ///< if set, the first_dir is not updated.
  bool move_resident_alt;     ///< if set, single_doc_dir is inserted.

  /** Lock queue support.
      If a lock request is made and fails to acquire the lock, an event for the requesting CacheVC can
      be placed on the @a lock_trigger list safely without the lock. To guarantee a future dispatch
      there also needs to be a trigger event pending if the queue is not empty. To avoid event build
      up, such an event is stored in @a lock_trigger. This is made atomic so that a thread without
      the ODE lock can set it using CAS. The process is the event is scheduled on the local thread
      and then put in @a lock_trigger. If the CAS fails, the event is cancelled because there is
      already a trigger event that will run the lock queue. The return value @c push on the queue
      can be checked for a previously empty queue and the trigger event scheduled only in that case.
      Technically this would suffice but for now the CAS will be used to verify that.
   */
  std::atomic<Event*> lock_trigger; ///< Trigger event for lock queue.
  AtomicSLL<Event*> lock_waiting; ///< Queue of events for CacheVCs waiting on the lock for this instance.

  bool reading_vec;  ///< An I/O operation is currently active to read the vector (first_buf).
  bool writing_vec;  ///< An I/O operation is currently active to write the vector (first_buf).

  /** Set to a writer @c CacheVC that is waiting for response headers to update the altvec.

      If this is set then there is a write @c CacheVC that is active but has not yet been able to
      update the vector for its alternate. Any new reader should block on open if this is set and
      enter itself on the @a _waiting list, making this effectively a write lock on the object.
      This is necessary because we can't reliably do alternate selection in this state. The waiting
      reader @c CacheVC instances are released as soon as the vector is updated, they do not have to
      wait until the write @c CacheVC has finished its transaction. In practice this means until the
      server response headers have been received and processed.
  */
  CacheVC * open_writer;

  /** A list of @c CacheVC instances that are waiting for the @a open_writer.
   */
  DLL<CacheVC, Link_CacheVC_Active_Link> open_waiting;

  /// Link for stripe global hash table.
  LINK(OpenDirEntry, link);

  /// Get the alternate index for the @a key.
  int index_of(CacheKey const &key);
  /// Check if there are any writers for the alternate of @a alt_key.
  bool has_writer(CacheKey const &alt_key);
  /// Mark a @c CacheVC as actively writing at @a offset on the alternate with @a alt_key.
  self &write_active(CacheKey const &alt_key, CacheVC *vc, int64_t offset);
  /// Mark an active write by @a vc as complete and indicate whether it had @a success.
  /// If the write is not @a success then the fragment is not marked as cached.
  self &write_complete(CacheKey const &alt_key, CacheVC *vc, CacheBuffer const &data, bool success = true);
  /// Indicate if a VC is currently writing to the fragment with this @a offset.
  bool is_write_active(CacheKey const &alt_key, int64_t offset);
  /// Get the fragment key for a specific @a offset.
  CacheKey const &key_for(CacheKey const &alt_key, int64_t offset);
  /** Wait for a fragment to be written.

      @return @c false if there is no writer that is scheduled to write that fragment.
   */
  bool wait_for(CacheKey const &alt_key, CacheVC *vc, int64_t offset);
  /// Close out anything related to this writer
  self &close_writer(CacheKey const &alt_key, CacheVC *vc);
  /// Close out the open writer if it is @a writer
  self &close_open_writer(CacheVC *writer);

  /// Send @c CACHE_EVENT_ODE_LOCK_READY with @a this locked when lock is available.
  Action* wait_for_lock(CacheVC *cachevc);
};

struct OpenDir : public Continuation {
  typedef Queue<CacheVC, Link_CacheVC_OpenDir_Link> CacheVCQ;
  CacheVCQ delayed_readers;

  DLL<OpenDirEntry> bucket[OPEN_DIR_BUCKETS];

  /** Open a live directory entry for @a vc.

      @a force_p is set to @c true to force the entry if it's not already there.
  */
  OpenDirEntry *open_entry(Vol *vol, CryptoHash const &key, bool force_p = false);
  void close_entry(CacheVC *c);
  //  OpenDirEntry *open_read(CryptoHash *key);
  int signal_readers(int event, Event *e);

  OpenDir();
};

struct CacheSync : public Continuation {
  int vol_idx;
  char *buf;
  size_t buflen;
  bool buf_huge;
  off_t writepos;
  AIOCallbackInternal io;
  Event *trigger;
  ink_hrtime start_time;
  int mainEvent(int event, Event *e);
  void aio_write(int fd, char *b, int n, off_t o);

  CacheSync()
    : Continuation(new_ProxyMutex()), vol_idx(0), buf(0), buflen(0), buf_huge(false), writepos(0), trigger(0), start_time(0)
  {
    SET_HANDLER(&CacheSync::mainEvent);
  }
};

// Global Functions

void vol_init_dir(Vol *d);
int dir_token_probe(const CacheKey *, Vol *, Dir *);
int dir_probe(const CacheKey *, Vol *, Dir *, Dir **);
int dir_insert(const CacheKey *key, Vol *d, Dir *to_part);
int dir_overwrite(const CacheKey *key, Vol *d, Dir *to_part, Dir *overwrite, bool must_overwrite = true);
int dir_delete(const CacheKey *key, Vol *d, Dir *del);
int dir_lookaside_probe(const CacheKey *key, Vol *d, Dir *result, EvacuationBlock **eblock);
int dir_lookaside_insert(EvacuationBlock *b, Vol *d, Dir *to);
int dir_lookaside_fixup(const CacheKey *key, Vol *d);
void dir_lookaside_cleanup(Vol *d);
void dir_lookaside_remove(const CacheKey *key, Vol *d);
void dir_free_entry(Dir *e, int s, Vol *d);
void dir_sync_init();
int check_dir(Vol *d);
void dir_clean_vol(Vol *d);
void dir_clear_range(off_t start, off_t end, Vol *d);
int dir_segment_accounted(int s, Vol *d, int offby = 0, int *free = 0, int *used = 0, int *empty = 0, int *valid = 0,
                          int *agg_valid = 0, int *avg_size = 0);
uint64_t dir_entries_used(Vol *d);
void sync_cache_dir_on_shutdown();

// Global Data

extern Dir empty_dir;

// Inline Funtions

#define dir_in_seg(_s, _i) ((Dir *)(((char *)(_s)) + (SIZEOF_DIR * (_i))))

TS_INLINE bool
dir_compare_tag(const Dir *e, const CacheKey *key)
{
  return (dir_tag(e) == DIR_MASK_TAG(key->slice32(2)));
}

TS_INLINE Dir *
dir_from_offset(int64_t i, Dir *seg)
{
#if DIR_DEPTH < 5
  if (!i)
    return 0;
  return dir_in_seg(seg, i);
#else
  i = i + ((i - 1) / (DIR_DEPTH - 1));
  return dir_in_seg(seg, i);
#endif
}
TS_INLINE Dir *
next_dir(Dir *d, Dir *seg)
{
  int i = dir_next(d);
  return dir_from_offset(i, seg);
}
TS_INLINE int64_t
dir_to_offset(const Dir *d, const Dir *seg)
{
#if DIR_DEPTH < 5
  return (((char *)d) - ((char *)seg)) / SIZEOF_DIR;
#else
  int64_t i = (int64_t)((((char *)d) - ((char *)seg)) / SIZEOF_DIR);
  i         = i - (i / DIR_DEPTH);
  return i;
#endif
}
TS_INLINE Dir *
dir_bucket(int64_t b, Dir *seg)
{
  return dir_in_seg(seg, b * DIR_DEPTH);
}
TS_INLINE Dir *
dir_bucket_row(Dir *b, int64_t i)
{
  return dir_in_seg(b, i);
}

#endif /* _P_CACHE_DIR_H__ */
