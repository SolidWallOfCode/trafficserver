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

#include "P_Cache.h"

#ifdef HTTP_CACHE
#include "HttpCacheSM.h" //Added to get the scope of HttpCacheSM object.
#endif

extern int cache_config_compatibility_4_2_0_fixup;

Action *
Cache::open_read(Continuation *cont, const CacheKey *key, CacheFragType type, const char *hostname, int host_len)
{
  if (!CacheProcessor::IsCacheReady(type)) {
    cont->handleEvent(CACHE_EVENT_OPEN_READ_FAILED, (void *)-ECACHE_NOT_READY);
    return ACTION_RESULT_DONE;
  }
  ink_assert(caches[type] == this);

  Vol *vol = key_to_vol(key, hostname, host_len);
  Dir result, *last_collision = nullptr;
  ProxyMutex *mutex = cont->mutex.get();
  OpenDirEntry *od  = nullptr;
  CacheVC *c        = nullptr;
  {
    CACHE_TRY_LOCK(lock, vol->mutex, mutex->thread_holding);
    if (!lock.is_locked() || (od = vol->open_read(key)) || dir_probe(key, vol, &result, &last_collision)) {
      c            = new_CacheVC(cont);
      c->vol       = vol;
      c->first_key = c->key = c->earliest_key = *key;
      c->vio.op                               = VIO::READ;
      c->base_stat                            = cache_read_active_stat;
      c->od                                   = od;
      c->frag_type                            = type;
      CACHE_INCREMENT_DYN_STAT(c->base_stat + CACHE_STAT_ACTIVE);
      SET_CONTINUATION_HANDLER(c, &CacheVC::openReadStartHead);
    }
    if (!c) // got the lock but didn't find it in the open dir entries or the directory
      goto Lmiss;
    if (!lock.is_locked()) {
      CONT_SCHED_LOCK_RETRY(c);
      return &c->_action;
    }
    if (c->od) // if an ODE was found, then there is (or recently was) a writer
      goto Lwriter;
    // Otherwise start a local read of the first doc.
    c->dir            = result;
    c->last_collision = last_collision;
    switch (c->do_read_call(&c->key)) {
    case EVENT_DONE:
      return ACTION_RESULT_DONE;
    case EVENT_RETURN:
      goto Lcallreturn;
    default:
      return &c->_action;
    }
  }
Lmiss:
  CACHE_INCREMENT_DYN_STAT(cache_read_failure_stat);
  cont->handleEvent(CACHE_EVENT_OPEN_READ_FAILED, (void *)-ECACHE_NO_DOC);
  return ACTION_RESULT_DONE;
Lwriter:
  SET_CONTINUATION_HANDLER(c, &CacheVC::openReadFromWriter);
  if (c->handleEvent(EVENT_IMMEDIATE, nullptr) == EVENT_DONE)
    return ACTION_RESULT_DONE;
  return &c->_action;
Lcallreturn:
  if (c->handleEvent(AIO_EVENT_DONE, nullptr) == EVENT_DONE)
    return ACTION_RESULT_DONE;
  return &c->_action;
}

/** Open reader from writer @a vc
    This is used from a write VC to open a corresponding read VC to serve the content.
*/
Action *
Cache::open_read(Continuation *cont, CacheVConnection *vc, HTTPHdr *client_request_hdr)
{
  Action *zret = ACTION_RESULT_DONE;

  CacheVC *write_vc = dynamic_cast<CacheVC *>(vc);
  if (write_vc) {
    Vol *vol          = write_vc->vol;
    ProxyMutex *mutex = cont->mutex.get(); // needed for stat macros
    CacheVC *c        = new_CacheVC(cont);

    c->vol       = write_vc->vol;
    c->first_key = write_vc->first_key;
    // [amc] Need to fix this as it's pointless. In general @a earliest_key in the write VC
    // won't be the correct value - it's randomly generated and for a partial fill won't be
    // set to the actual alternate value until later (in @c set_http_info).
    c->earliest_key = c->key = write_vc->earliest_key;
    c->vio.op                = VIO::READ;
    c->base_stat             = cache_read_active_stat;
    c->od                    = write_vc->od;
    ++(c->od->num_active);
    c->frag_type = write_vc->frag_type;
    CACHE_INCREMENT_DYN_STAT(c->base_stat + CACHE_STAT_ACTIVE);
    //    write_vc->alternate.request_get(&c->request);
    //    client_request_hdr->copy_shallow(&c->request);
    c->request.copy_shallow(client_request_hdr);
    c->params = write_vc->params; // seems to be a no-op, always NULL.
    c->dir = c->first_dir = write_vc->first_dir;
    c->write_vc           = write_vc;
    c->first_buf          = write_vc->first_buf; // I don't think this is effective either.
    SET_CONTINUATION_HANDLER(c, &CacheVC::openReadFromWriter);
    zret = &c->_action; // default, override if needed.
    CACHE_TRY_LOCK(lock, vol->mutex, mutex->thread_holding);
    if (lock.is_locked() && c->handleEvent(EVENT_IMMEDIATE, 0) == EVENT_DONE) {
      zret = ACTION_RESULT_DONE;
    }
  }
  return zret;
}

/** Base open read for HTTP objects.
 */
Action *
Cache::open_read(Continuation *cont, const CacheKey *key, CacheHTTPHdr *request, CacheLookupHttpConfig *params, CacheFragType type,
                 const char *hostname, int host_len)
{
  if (!CacheProcessor::IsCacheReady(type)) {
    cont->handleEvent(CACHE_EVENT_OPEN_READ_FAILED, (void *)-ECACHE_NOT_READY);
    return ACTION_RESULT_DONE;
  }
  ink_assert(caches[type] == this);

  Vol *vol = key_to_vol(key, hostname, host_len);
  Dir result, *last_collision = nullptr;
  ProxyMutex *mutex = cont->mutex.get();
  OpenDirEntry *od  = nullptr;
  CacheVC *c        = nullptr;

  {
    CACHE_TRY_LOCK(lock, vol->mutex, mutex->thread_holding);
    // if not locked or found, create a vc to read or retry locks.
    if (!lock.is_locked() || (od = vol->open_read(key)) || dir_probe(key, vol, &result, &last_collision)) {
      c            = new_CacheVC(cont);
      c->vol       = vol;
      c->first_key = c->key = c->earliest_key = *key;
      c->vio.op                               = VIO::READ;
      c->base_stat                            = cache_read_active_stat;
      c->od                                   = od;
      c->frag_type                            = CACHE_FRAG_TYPE_HTTP;
      CACHE_INCREMENT_DYN_STAT(c->base_stat + CACHE_STAT_ACTIVE);
      c->request.copy_shallow(request);
      c->params = params;
    }
    if (!lock.is_locked()) {
      SET_CONTINUATION_HANDLER(c, &CacheVC::openReadStartHead);
      CONT_SCHED_LOCK_RETRY(c);
      return &c->_action;
    }
    if (!c) // got the lock but key was not found.
      goto Lmiss;
    if (c->od) //
      goto Lwriter;
    // hit
    c->dir = c->first_dir = result;
    c->last_collision     = last_collision;
    SET_CONTINUATION_HANDLER(c, &CacheVC::openReadStartHead);
    switch (c->do_read_call(&c->key)) {
    case EVENT_DONE:
      return ACTION_RESULT_DONE;
    case EVENT_RETURN:
      goto Lcallreturn;
    default:
      return &c->_action;
    }
  }
Lmiss:
  CACHE_INCREMENT_DYN_STAT(cache_read_failure_stat);
  cont->handleEvent(CACHE_EVENT_OPEN_READ_FAILED, (void *)-ECACHE_NO_DOC);
  return ACTION_RESULT_DONE;
Lwriter:
  SET_CONTINUATION_HANDLER(c, &CacheVC::openReadFromWriter);
  if (c->handleEvent(EVENT_IMMEDIATE, nullptr) == EVENT_DONE)
    return ACTION_RESULT_DONE;
  return &c->_action;
Lcallreturn:
  if (c->handleEvent(AIO_EVENT_DONE, nullptr) == EVENT_DONE)
    return ACTION_RESULT_DONE;
  return &c->_action;
}

uint32_t
CacheVC::load_http_info(CacheHTTPInfoVector *info, Doc *doc, RefCountObj *block_ptr)
{
  uint32_t zret = info->get_handles(doc->hdr(), doc->hlen, block_ptr);
  if (zret != static_cast<uint32_t>(-1) &&      // Make sure we haven't already failed
      cache_config_compatibility_4_2_0_fixup && // manual override not engaged
      !this->f.doc_from_ram_cache &&            // it's already been done for ram cache fragments
      vol->header->version.ink_major == 23 && vol->header->version.ink_minor == 0) {
    info->template for_each_slice([](CacheHTTPInfoVector::Slice& slice) {
      slice._alternate.m_alt->m_response_hdr.m_mime->recompute_accelerators_and_presence_bits();
      slice._alternate.m_alt->m_request_hdr.m_mime->recompute_accelerators_and_presence_bits();
      });
  }
  return zret;
}

char const *
CacheVC::get_http_range_boundary_string(int *len) const
{
  return resp_range.getBoundaryStr(len);
}

int64_t
CacheVC::get_effective_content_size()
{
  return resp_range.hasRanges() ? resp_range.calcContentLength() : alternate.object_size_get();
}

int
CacheVC::closeReadAndFree(int /* event ATS_UNUSED */, Event * /* e ATS_UNUSED */)
{
  //  cancel_trigger(); // ??
  if (od) {
    CACHE_TRY_LOCK(lock, vol->mutex, mutex->thread_holding);
    if (!lock.is_locked()) {
      SET_HANDLER(&CacheVC::closeReadAndFree);
      VC_SCHED_LOCK_RETRY();
    }
    vol->close_read(this);
  }
  return free_CacheVC(this);
}

int
CacheVC::openReadFromWriterFailure(int event, Event *e)
{
  // od = NULL;
  vol->close_read(this);
  vector.clear(false);
  CACHE_INCREMENT_DYN_STAT(cache_read_failure_stat);
  CACHE_INCREMENT_DYN_STAT(cache_read_busy_failure_stat);
  _action.continuation->handleEvent(event, e);
  free_CacheVC(this);
  return EVENT_DONE;
}

int
CacheVC::openReadFromWriter(int event, Event *e)
{
  if (!f.read_from_writer_called) {
    // The assignment to last_collision as nullptr was
    // made conditional after INKqa08411
    last_collision = nullptr;
    // Let's restart the clock from here - the first time this a reader
    // gets in this state. Its possible that the open_read was called
    // before the open_write, but the reader could not get the volume
    // lock. If we don't reset the clock here, we won't choose any writer
    // and hence fail the read request.
    start_time                = Thread::get_hrtime();
    f.read_from_writer_called = 1;
  }
  cancel_trigger();
  DDebug("cache_open_read", "%p: key: %X In openReadFromWriter", this, first_key.slice32(1));

  if (_action.cancelled) {
    return this->closeReadAndFree(0, NULL);
    //    od = NULL; // only open for read so no need to close
    //    return free_CacheVC(this);
  }
  CACHE_TRY_LOCK(lock, vol->mutex, mutex->thread_holding);
  if (!lock.is_locked())
    VC_SCHED_LOCK_RETRY();
  if (!od && NULL == (od = vol->open_read(&first_key))) {
    MUTEX_RELEASE(lock);
    write_vc = nullptr;
    SET_HANDLER(&CacheVC::openReadStartHead);
    return openReadStartHead(event, e);
  }

  CACHE_TRY_LOCK(lock_od, od->mutex, mutex->thread_holding);
  if (!lock_od.is_locked())
    VC_SCHED_LOCK_RETRY();

  if (od->open_writer) {
    // Alternates are in flux, wait for origin server response to update them.
    if (!od->open_waiting.in(this)) {
      // If the writer that's updating the alt table is the paired write VC for this reader
      // then we need to go with the alt selected by that specific writer rather than do
      // independent alt selection.
      if (od->open_writer == write_vc)
        SET_HANDLER(&CacheVC::waitForAltUpdate);
      wake_up_thread = mutex->thread_holding;
      od->open_waiting.push(this);
    }
    Debug("amc", "[CacheVC::openReadFromWriter] waiting for %p", od->open_writer);
    return EVENT_CONT; // wait for the writer to wake us up.
  }

  // For now the vol lock must be held to deal with clean up of potential failures. Need to fix
  // that at some point.

  if (write_vc && (slice_ref = od->vector.slice_ref_for(write_vc->earliest_key)).is_valid()) {
    MUTEX_RELEASE(lock);
    // Found the alternate for our write VC. Really, though, if we have a write_vc we should never fail to get
    // the alternate - we should probably check for that.
    alternate.copy_shallow(&slice_ref._slice->_alternate);
    MUTEX_RELEASE(lock_od);
    key = earliest_key = alternate.object_key_get();
    doc_len            = alternate.object_size_get();
    Debug("amc", "[openReadFromWriter] - setting alternate from write_vc %p to #%d : %p", write_vc, slice_ref._idx,
          alternate.m_alt);
    SET_HANDLER(&CacheVC::openReadStartEarliest);
    return openReadStartEarliest(event, e);
  } else {
    if (cache_config_select_alternate) {
      slice_ref._idx = HttpTransactCache::SelectFromAlternates(&od->vector, &request, params);
      if (slice_ref._idx < 0) {
        MUTEX_RELEASE(lock_od);
        SET_HANDLER(&CacheVC::openReadFromWriterFailure);
        return openReadFromWriterFailure(CACHE_EVENT_OPEN_READ_FAILED, reinterpret_cast<Event *>(-ECACHE_ALT_MISS));
      }
      Debug("amc", "[openReadFromWriter] select alt: %d %p (current %p)", slice_ref._idx, od->vector.get(slice_ref._idx)->m_alt,
            alternate.m_alt);
    } else {
      slice_ref._idx = 0;
    }
    MUTEX_RELEASE(lock);
    MUTEX_RELEASE(lock_od);
    SET_HANDLER(&CacheVC::openReadStartHead);
    return openReadStartHead(event, e);
  }
  ink_assert(false);
  return EVENT_DONE; // should not get here.
}

int
CacheVC::waitForAltUpdate(int event, Event *e)
{
  DDebug("cache_open_read", "[waitForAltUpdate] %p", this);
  void *tag = e->cookie; // Was the address of an alt.
  int i     = -1;
  cancel_trigger();

  if (_action.cancelled) {
    DDebug("cache_open_read", "[waitForAltUpdate] %p - canceled", this);
    return this->closeReadAndFree(0, NULL);
  }

  if (CACHE_EVENT_WRITER_UPDATED_ALT_TABLE == event) {
    CACHE_TRY_LOCK(lock_od, od->mutex, mutex->thread_holding);
    if (!lock_od.is_locked())
      VC_SCHED_LOCK_RETRY();

    // @a e carries a cookie which is computed from the earliest key of the alt selected by the writerVC.
    for (i = od->vector.count() - 1; i >= 0; --i) {
      CacheHTTPInfoVector::Slice &slice = *(od->vector.data[i]._slices.head);
      if (reinterpret_cast<void *>(slice._alternate.m_alt->m_earliest.m_key.fold()) == tag) {
        alternate.copy_shallow(&slice._alternate);
        earliest_key = alternate.m_alt->m_earliest.m_key;
        doc_len      = alternate.object_size_get();
        break;
      }
    }
  } else {
    Debug("amc", "[waitForAltUpdate] - unexpected event %d", event);
    // fall through and fail.
  }

  if (i < 0) { // alt not found, which is a serious error in this case (paired with writeVC).
    SET_HANDLER(&CacheVC::openReadFromWriterFailure);
    return openReadFromWriterFailure(CACHE_EVENT_OPEN_READ_FAILED, reinterpret_cast<Event *>(-ECACHE_ALT_MISS));
  }

  // The writer has already dealt with the earliest fragment, no need to read it again from disk.
  // Go straight to content service.
  SET_HANDLER(&CacheVC::openReadMain);
  return callcont(CACHE_EVENT_OPEN_READ);
}

int
CacheVC::openReadFromWriterMain(int /* event ATS_UNUSED */, Event * /* e ATS_UNUSED */)
{
  cancel_trigger();
  if (seek_to) {
    vio.ndone = seek_to;
    seek_to   = 0;
  }
  IOBufferBlock *b = nullptr;
  int64_t ntodo    = vio.ntodo();
  if (ntodo <= 0)
    return EVENT_CONT;
  if (length < ((int64_t)doc_len) - vio.ndone) {
    DDebug("cache_read_agg", "truncation %X", first_key.slice32(1));
    if (is_action_tag_set("cache")) {
      ink_release_assert(false);
    }
    Warning("Document %X truncated at %d of %d, reading from writer", first_key.slice32(1), (int)vio.ndone, (int)doc_len);
    return calluser(VC_EVENT_ERROR);
  }
  /* its possible that the user did a do_io_close before
     openWriteWriteDone was called. */
  if (length > ((int64_t)doc_len) - vio.ndone) {
    int64_t skip_bytes = length - (doc_len - vio.ndone);
    iobufferblock_skip(writer_buf.get(), &writer_offset, &length, skip_bytes);
  }
  int64_t bytes = length;
  if (bytes > vio.ntodo())
    bytes = vio.ntodo();
  if (vio.ndone >= (int64_t)doc_len) {
    ink_assert(bytes <= 0);
    // reached the end of the document and the user still wants more
    return calluser(VC_EVENT_EOS);
  }
  b          = iobufferblock_clone(writer_buf.get(), writer_offset, bytes);
  writer_buf = iobufferblock_skip(writer_buf.get(), &writer_offset, &length, bytes);
  vio.buffer.writer()->append_block(b);
  vio.ndone += bytes;
  if (vio.ntodo() <= 0)
    return calluser(VC_EVENT_READ_COMPLETE);
  else
    return calluser(VC_EVENT_READ_READY);
}

int
CacheVC::openReadClose(int event, Event * /* e ATS_UNUSED */)
{
  cancel_trigger();
  if (is_io_in_progress()) {
    if (event != AIO_EVENT_DONE)
      return EVENT_CONT;
    set_io_not_in_progress();
  }
  CACHE_TRY_LOCK(lock, vol->mutex, mutex->thread_holding);
  if (!lock.is_locked())
    VC_SCHED_LOCK_RETRY();
  if (f.hit_evacuate && dir_valid(vol, &first_dir) && closed > 0) {
    if (f.single_fragment)
      vol->force_evacuate_head(&first_dir, dir_pinned(&first_dir));
    else if (dir_valid(vol, &earliest_dir)) {
      vol->force_evacuate_head(&first_dir, dir_pinned(&first_dir));
      vol->force_evacuate_head(&earliest_dir, dir_pinned(&earliest_dir));
    }
  }
  vol->close_read(this);
  return free_CacheVC(this);
}

int
CacheVC::openReadReadDone(int event, Event *e)
{
  Doc *doc = nullptr;

  cancel_trigger();
  if (event == EVENT_IMMEDIATE)
    return EVENT_CONT;
  set_io_not_in_progress();
  {
    CACHE_TRY_LOCK(lock, vol->mutex, mutex->thread_holding);
    if (!lock.is_locked())
      VC_SCHED_LOCK_RETRY();
    if (event == AIO_EVENT_DONE && !io.ok()) {
      dir_delete(&earliest_key, vol, &earliest_dir);
      goto Lerror;
    }
    if (last_collision &&     // no missed lock
        dir_valid(vol, &dir)) // object still valid
    {
      doc = reinterpret_cast<Doc *>(buf->data());
      if (doc->magic != DOC_MAGIC) {
        char tmpstring[100];
        if (doc->magic == DOC_CORRUPT)
          Warning("Middle: Doc checksum does not match for %s", key.toHexStr(tmpstring));
        else
          Warning("Middle: Doc magic does not match for %s", key.toHexStr(tmpstring));
        goto Lerror;
      }
      if (doc->key == key)
        goto LreadMain;
    }
    if (last_collision && dir_offset(&dir) != dir_offset(last_collision))
      last_collision = nullptr; // object has been/is being overwritten
    if (dir_probe(&key, vol, &dir, &last_collision)) {
      int ret = do_read_call(&key);
      if (ret == EVENT_RETURN)
        goto Lcallreturn;
      return EVENT_CONT;
    } else if (write_vc) {
      ink_release_assert(!"[amc] Handle this");
    }
    // fall through for truncated documents
  }
Lerror:
  char tmpstring[100];
  Warning("Document %s truncated", earliest_key.toHexStr(tmpstring));
  return calluser(VC_EVENT_ERROR);
  // Ldone:
  return calluser(VC_EVENT_EOS);
Lcallreturn:
  return handleEvent(AIO_EVENT_DONE, nullptr);
LreadMain:
  wait_buffer.write(buf.get(), doc->data_len(), doc->prefix_len()); // just the content part.
  wait_position = alternate.get_frag_offset(fragment);
  // I think these are all useless now.
  doc_pos = doc->prefix_len();
  doc_pos += resp_range.getOffset() - frag_upper_bound; // used before update!
  frag_upper_bound += doc->data_len();
  SET_HANDLER(&CacheVC::openReadMain);
  return openReadMain(event, e);
}

// Content is ready to go out to the user agent. Ship it.
//
// The content is presumed to be either left or consumed in toto. If the output VIO is too full nothing
// is done. Otherwise as much of the content as possible is shipped. Content is discarded if there is too
// much to fit in the current range or the VIO write operation is finished (although it's wrong if the VIO
// finishes but not the range).
int64_t
CacheVC::shipContent()
{
  MIOBuffer *writer = vio.buffer.writer();
  Ptr<IOBufferBlock> block;
  int64_t bytes;

  // If some data has been written, don't write more than the high water mark. This prevents
  // internal IO buffers from filling when a slow user agent requests a large object.
  if (vio.ndone > 0 && writer->water_mark < writer->max_read_avail())
    return -1;

  bytes = std::min(wait_buffer.length(), vio.ntodo());                        // clip content length by VIO limit.
  bytes = std::min(bytes, static_cast<int64_t>(resp_range.getRemnantSize())); // and then by range

  // Ship it.
  if (bytes > 0) {
    int64_t offset = 0;
    int64_t r_pos  = resp_range.getOffset();

    // If there is a pending range shift then the last range was filled and the range spec advanced to
    // the next range. We have data for that range now so it's appropriate to write out the range
    // header.
    if (resp_range.hasPendingRangeShift()) {
      int b_len;
      char const *b_str = resp_range.getBoundaryStr(&b_len);
      size_t r_idx      = resp_range.getIdx();

      vio.ndone +=
        HTTPRangeSpec::writePartBoundary(vio.buffer.writer(), b_str, b_len, doc_len, resp_range[r_idx]._min, resp_range[r_idx]._max,
                                         resp_range.getContentTypeField(), r_idx >= (resp_range.count() - 1));
      resp_range.consumeRangeShift();
      Debug("amc", "Range boundary for range %" PRIu64, r_idx);
    }

    // The available content can be potentially shared. A new buffer block is therefore required.
    // Direct append to avoid allocating and copying to new buffer data blocks.
    if (wait_position < r_pos)
      offset = r_pos - wait_position;
    if (offset >= wait_buffer.length()) {
      // Not making progress, something has gone wrong.
      Debug("amc", "No content shipped (% " PRId64 " bytes) because content buffer length %" PRId64 " was less than content buffer offset %" PRId64
                   " [data @ %" PRId64 ", output @ %" PRId64 "].",
            bytes, wait_buffer.length(), offset, wait_position, r_pos);
      ink_release_assert(false); // core out for now, remove this for real production.
    } else {
      bytes = writer->write(wait_buffer.head(), bytes, offset);
      resp_range.consume(bytes);
      vio.ndone += bytes;
    }
    wait_buffer.clear();
    wait_position = -1;
    Debug("amc", "shipped %" PRId64 " bytes at range offset %" PRIu64, bytes, r_pos);
  } else {
    // @a wait position was set but no data was available, which is broken.
    Debug("amc", "No content at %" PRId64 " to ship!", wait_position);
    wait_position = -1;
  }

  // shipped, set up to start work on next piece of content.
  SET_HANDLER(&CacheVC::openReadMain);

  if (vio.ntodo() <= 0)
    return calluser(VC_EVENT_READ_COMPLETE);
  else if (calluser(VC_EVENT_READ_READY) == EVENT_DONE)
    return EVENT_DONE;
  return this->openReadMain(EVENT_IMMEDIATE, NULL);
}

// Ship content if available or set up to get content to ship.
int
CacheVC::openReadMain(int /* event ATS_UNUSED */, Event * /* e ATS_UNUSED */)
{
  int64_t target_position = resp_range.getOffset();
  int64_t target_size     = resp_range.getRemnantSize();

  cancel_trigger();

  if (wait_position >= 0) { // Data has arrived, ship it.
    ink_assert(wait_buffer.length());
    return this->shipContent();
  } else if (target_size) {
    int64_t fragment_length = alternate.clip_to_frag_boundary(target_position, target_size);
    fragment                = alternate.get_frag_index_of(target_position);
    if (alternate.is_frag_cached(fragment)) {
      key = alternate.get_frag_key(fragment);
      Debug("amc", "Frag %d cached, no waiting", fragment);
      return this->fetchFromCache(EVENT_IMMEDIATE, NULL);
    } else if (NULL == od) {
      // If it's not in cache and there is no OD then there are no writers, fail.
      Debug("amc", "[CacheVC::openReadMain] Uncached fragment %d at offset %" PRId64 " and no ODE", fragment, target_position);
      return calluser(VC_EVENT_ERROR);
    } else if (od->vector.getSideBufferContent(earliest_key, wait_buffer, target_position, fragment_length)) {
      wait_position = target_position;
      return this->shipContent();
    } else if (!od->wait_for(earliest_key, this, target_position)) {
      DDebug("cache_read_main", "%p: key: %X ReadMain writer aborted: %d", this, first_key.slice32(1), (int)vio.ndone);
      return calluser(VC_EVENT_ERROR);
    } else {
      // VC should be on the wait list in the OD. Should that be verified?
      DDebug("cache_read_main", "%p: key: %X ReadMain waiting: ndone=%d", this, first_key.slice32(1), (int)vio.ndone);
      SET_HANDLER(&CacheVC::openReadMain);
      return EVENT_CONT;
    }
  } else if (vio.ntodo() > 0) {
    return calluser(VC_EVENT_EOS);
  }
  return calluser(VC_EVENT_DONE);
}

int
CacheVC::fetchFromCache(int, Event *)
{
  cancel_trigger();

  Debug("amc", "[CacheVC::fetchFromCache] Fragment %d at offset %" PRIu64, fragment, resp_range.getOffset());

  last_collision    = 0;
  writer_lock_retry = 0;
  // if the state machine calls reenable on the callback from the cache,
  // we set up a schedule_imm event. The openReadReadDone discards
  // EVENT_IMMEDIATE events. So, we have to cancel that trigger and set
  // a new EVENT_INTERVAL event.
  CACHE_TRY_LOCK(lock, vol->mutex, mutex->thread_holding);
  if (!lock.is_locked()) {
    SET_HANDLER(&CacheVC::fetchFromCache);
    VC_SCHED_LOCK_RETRY();
  }
  if (dir_probe(&key, vol, &dir, &last_collision)) {
    SET_HANDLER(&CacheVC::openReadReadDone);
    int ret = do_read_call(&key);
    if (ret == EVENT_RETURN) {
      lock.release();
      return handleEvent(AIO_EVENT_DONE, 0);
    }
    return EVENT_CONT;
  }
  if (is_action_tag_set("cache"))
    ink_release_assert(false);
  Warning("Document %X truncated at %d of %d, missing fragment %X", first_key.slice32(1), (int)vio.ndone, (int)doc_len,
          key.slice32(1));
  // remove the directory entry
  dir_delete(&earliest_key, vol, &earliest_dir);
  lock.release();
  // Lerror:
  return calluser(VC_EVENT_ERROR);
  // Leos:
  return calluser(VC_EVENT_EOS);
}

int
CacheVC::openReadWaitEarliest(int evid, Event *)
{
  int zret = EVENT_CONT;
  cancel_trigger();

  CACHE_TRY_LOCK(lock, vol->mutex, mutex->thread_holding);
  if (!lock.is_locked())
    VC_SCHED_LOCK_RETRY();
  Debug("amc", "[CacheVC::openReadWaitEarliest] [%d]", evid);
  if (NULL == vol->open_read(&first_key)) {
    // Writer is gone, so no more data for which to wait.
    // Best option is to just start over from the first frag.
    // Most likely scenario - object turned out to be a resident alternate so
    // there's no explicit earliest frag.
    lock.release();
    SET_HANDLER(&self::openReadStartHead);
    //    od = NULL;
    key = first_key;
    return handleEvent(EVENT_IMMEDIATE, 0);
  } else if (dir_probe(&key, vol, &earliest_dir, &last_collision) || dir_lookaside_probe(&key, vol, &earliest_dir, NULL)) {
    dir = earliest_dir;
    SET_HANDLER(&self::openReadStartEarliest);
    if ((zret = do_read_call(&key)) == EVENT_RETURN) {
      lock.release();
      return handleEvent(AIO_EVENT_DONE, 0);
    }
  }
  return zret;
}

/*
  This code follows CacheVC::openReadStartHead closely,
  if you change this you might have to change that.
*/
int
CacheVC::openReadStartEarliest(int /* event ATS_UNUSED */, Event * /* e ATS_UNUSED */)
{
  int ret  = 0;
  Doc *doc = nullptr;
  cancel_trigger();
  set_io_not_in_progress();
  if (_action.cancelled)
    return free_CacheVC(this);
  {
    CACHE_TRY_LOCK(lock, vol->mutex, mutex->thread_holding);
    if (!lock.is_locked())
      VC_SCHED_LOCK_RETRY();
    if (!buf)
      goto Lread;
    if (!io.ok())
      goto Ldone;
    // an object needs to be outside the aggregation window in order to be
    // be evacuated as it is read
    if (!dir_agg_valid(vol, &dir)) {
      // a directory entry which is nolonger valid may have been overwritten
      if (!dir_valid(vol, &dir))
        last_collision = nullptr;
      goto Lread;
    }
    doc = (Doc *)buf->data();
    if (doc->magic != DOC_MAGIC) {
      char tmpstring[100];
      if (is_action_tag_set("cache")) {
        ink_release_assert(false);
      }
      if (doc->magic == DOC_CORRUPT)
        Warning("Earliest: Doc checksum does not match for %s", key.toHexStr(tmpstring));
      else
        Warning("Earliest : Doc magic does not match for %s", key.toHexStr(tmpstring));
      // remove the dir entry
      dir_delete(&key, vol, &dir);
      // try going through the directory entries again
      // in case the dir entry we deleted doesnt correspond
      // to the key we are looking for. This is possible
      // because of directory collisions
      last_collision = nullptr;
      goto Lread;
    }
    if (!(doc->key == key)) // collisiion
      goto Lread;
    // success
    earliest_key = key;
    doc_pos      = doc->prefix_len();
    next_CacheKey(&key, &doc->key);
    fragment         = 1;
    frag_upper_bound = doc->data_len();
    vol->begin_read(this);
    if (vol->within_hit_evacuate_window(&earliest_dir) &&
        (!cache_config_hit_evacuate_size_limit || doc_len <= (uint64_t)cache_config_hit_evacuate_size_limit)) {
      DDebug("cache_hit_evac", "dir: %" PRId64 ", write: %" PRId64 ", phase: %d", dir_offset(&earliest_dir),
             offset_to_vol_offset(vol, vol->header->write_pos), vol->header->phase);
      f.hit_evacuate = 1;
    }
    goto Lsuccess;
  Lread:
    if (dir_probe(&key, vol, &earliest_dir, &last_collision) || dir_lookaside_probe(&key, vol, &earliest_dir, nullptr)) {
      dir = earliest_dir;
      if ((ret = do_read_call(&key)) == EVENT_RETURN)
        goto Lcallreturn;
      return ret;
    }
// read has detected that alternate does not exist in the cache.
// rewrite the vector.
#ifdef HTTP_CACHE
    // It's OK if there's a writer for this alternate, we can wait on it.
    if (od && od->has_writer(earliest_key)) {
      wake_up_thread = mutex->thread_holding;
      od->wait_for(earliest_key, this, 0);
      lock.release();
      // The SM must be signaled that the cache read is open even if we haven't got the earliest frag
      // yet because otherwise it won't set up the read side of the tunnel before the write side finishes
      // and terminates the SM (in the case of a resident alternate). But the VC can't be left with this
      // handler or it will confuse itself when it wakes up from the earliest frag read. So we put it
      // in a special wait state / handler and then signal the SM.
      SET_HANDLER(&self::openReadWaitEarliest);
      return callcont(CACHE_EVENT_OPEN_READ); // must signal read is open
    } else if (frag_type == CACHE_FRAG_TYPE_HTTP) {
      // don't want any writers while we are evacuating the vector
      ink_release_assert(!"[amc] Not handling multiple writers with vector evacuate");
      if (!vol->open_write(this)) {
        Doc *doc1    = (Doc *)first_buf->data();
        uint32_t len = this->load_http_info(&(od->vector), doc1);
        ink_assert(len == doc1->hlen && od->vector.count() > 0);
        od->vector.remove(slice_ref._idx, true);
        // if the vector had one alternate, delete it's directory entry
        if (len != doc1->hlen || !od->vector.count()) {
          // sometimes the delete fails when there is a race and another read
          // finds that the directory entry has been overwritten
          // (cannot assert on the return value)
          dir_delete(&first_key, vol, &first_dir);
        } else {
          buf             = nullptr;
          last_collision  = nullptr;
          write_len       = 0;
          header_len      = od->vector.marshal_length();
          f.evac_vector   = 1;
          f.use_first_key = 1;
          key             = first_key;
          // always use od->first_dir to overwrite a directory.
          // If an evacuation happens while a vector is being updated
          // the evacuator changes the od->first_dir to the new directory
          // that it inserted
          od->first_dir   = first_dir;
          od->writing_vec = 1;
          earliest_key    = zero_key;

          // set up this VC as a alternate delete write_vc
          vio.op          = VIO::WRITE;
          total_len       = 0;
          f.update        = 1;
          slice_ref._idx = CACHE_ALT_REMOVED;
          /////////////////////////////////////////////////////////////////
          // change to create a directory entry for a resident alternate //
          // when another alternate does not exist.                      //
          /////////////////////////////////////////////////////////////////
          if (doc1->total_len > 0) {
            od->move_resident_alt = 1;
            od->single_doc_key    = doc1->key;
            dir_assign(&od->single_doc_dir, &dir);
            dir_set_tag(&od->single_doc_dir, od->single_doc_key.slice32(2));
          }
          SET_HANDLER(&CacheVC::openReadVecWrite);
          if ((ret = do_write_call()) == EVENT_RETURN)
            goto Lcallreturn;
          return ret;
        }
      }
    }
#endif
  // open write failure - another writer, so don't modify the vector
  Ldone:
    if (od)
      vol->close_write(this);
  }
  CACHE_INCREMENT_DYN_STAT(cache_read_failure_stat);
  _action.continuation->handleEvent(CACHE_EVENT_OPEN_READ_FAILED, (void *)-ECACHE_NO_DOC);
  return free_CacheVC(this);
Lcallreturn:
  return handleEvent(AIO_EVENT_DONE, nullptr); // hopefully a tail call
Lsuccess:
  if (write_vc)
    CACHE_INCREMENT_DYN_STAT(cache_read_busy_success_stat);
  SET_HANDLER(&CacheVC::openReadMain);
  return callcont(CACHE_EVENT_OPEN_READ);
}

// create the directory entry after the vector has been evacuated
// the volume lock has been taken when this function is called
#ifdef HTTP_CACHE
int
CacheVC::openReadVecWrite(int /* event ATS_UNUSED */, Event * /* e ATS_UNUSED */)
{
  cancel_trigger();
  set_io_not_in_progress();
  ink_assert(od);
  od->writing_vec = 0;
  if (_action.cancelled)
    return openWriteCloseDir(EVENT_IMMEDIATE, nullptr);
  {
    CACHE_TRY_LOCK(lock, vol->mutex, mutex->thread_holding);
    if (!lock.is_locked())
      VC_SCHED_LOCK_RETRY();
    if (io.ok()) {
      ink_assert(f.evac_vector);
      ink_assert(frag_type == CACHE_FRAG_TYPE_HTTP);
      ink_assert(!buf);
      f.evac_vector   = false;
      last_collision  = nullptr;
      f.update        = 0;
      slice_ref.clear();
      f.use_first_key = 0;
      vio.op          = VIO::READ;
      dir_overwrite(&first_key, vol, &dir, &od->first_dir);
      if (od->move_resident_alt)
        dir_insert(&od->single_doc_key, vol, &od->single_doc_dir);
      int alt_ndx = HttpTransactCache::SelectFromAlternates(&(od->vector), &request, params);
      Debug("amc", "[openReadVecWrite] select alt: %d %p (current %p)", alt_ndx, od->vector.get(alt_ndx)->m_alt, alternate.m_alt);
      vol->close_write(this);
      if (alt_ndx >= 0) {
        vector.clear();
        // we don't need to start all over again, since we already
        // have the vector in memory. But this is simpler and this
        // case is rare.
        goto Lrestart;
      }
    } else {
      vol->close_write(this);
    }
  }

  CACHE_INCREMENT_DYN_STAT(cache_read_failure_stat);
  _action.continuation->handleEvent(CACHE_EVENT_OPEN_READ_FAILED, (void *)-ECACHE_ALT_MISS);
  return free_CacheVC(this);
Lrestart:
  SET_HANDLER(&CacheVC::openReadStartHead);
  return openReadStartHead(EVENT_IMMEDIATE, nullptr);
}
#endif

/*
  This code follows CacheVC::openReadStartEarliest closely,
  if you change this you might have to change that.

  This handles the I/O completion of reading the first doc of the object.
  If there are alternates, we chain to openreadStartEarliest to read the
  earliest doc.
*/
int
CacheVC::openReadStartHead(int event, Event *e)
{
  intptr_t err = ECACHE_NO_DOC;
  Doc *doc     = nullptr;
  cancel_trigger();
  set_io_not_in_progress();
  if (_action.cancelled)
    return free_CacheVC(this);
  {
    CACHE_TRY_LOCK(lock, vol->mutex, mutex->thread_holding);
    if (!lock.is_locked())
      VC_SCHED_LOCK_RETRY();
    if (!buf)
      goto Lread;
    if (!io.ok())
      goto Ldone;
    // an object needs to be outside the aggregation window in order to be
    // be evacuated as it is read
    if (!dir_agg_valid(vol, &dir)) {
      // a directory entry which is no longer valid may have been overwritten
      if (!dir_valid(vol, &dir))
        last_collision = nullptr;
      goto Lread;
    }
    doc = (Doc *)buf->data();
    if (doc->magic != DOC_MAGIC) {
      char tmpstring[100];
      if (is_action_tag_set("cache")) {
        ink_release_assert(false);
      }
      if (doc->magic == DOC_CORRUPT)
        Warning("Head: Doc checksum does not match for %s", key.toHexStr(tmpstring));
      else
        Warning("Head : Doc magic does not match for %s", key.toHexStr(tmpstring));
      // remove the dir entry
      dir_delete(&key, vol, &dir);
      // try going through the directory entries again
      // in case the dir entry we deleted doesnt correspond
      // to the key we are looking for. This is possible
      // because of directory collisions
      last_collision = nullptr;
      goto Lread;
    }
    if (!(doc->first_key == key))
      goto Lread;
    if (f.lookup)
      goto Lookup;
    earliest_dir = dir;
#ifdef HTTP_CACHE
    CacheHTTPInfo *alternate_tmp;
    if (frag_type == CACHE_FRAG_TYPE_HTTP) {
      uint32_t uml;
      ink_assert(doc->hlen);
      if (!doc->hlen)
        goto Ldone;
      if ((uml = this->load_http_info(&vector, doc)) != doc->hlen) {
        if (buf) {
          HTTPCacheAlt *alt  = reinterpret_cast<HTTPCacheAlt *>(doc->hdr());
          int32_t alt_length = 0;
          // count should be reasonable, as vector is initialized and unlikly to be too corrupted
          // by bad disk data - count should be the number of successfully unmarshalled alts.
          for (int32_t i = 0; i < vector.count(); ++i) {
            CacheHTTPInfo *info = vector.get(i);
            if (info && info->m_alt)
              alt_length += info->m_alt->m_unmarshal_len;
          }
          Note("OpenReadHead failed for cachekey %X : vector inconsistency - "
               "unmarshalled %d expecting %d in %d (base=%d, ver=%d:%d) "
               "- vector n=%d size=%d"
               "first alt=%d[%s]",
               key.slice32(0), uml, doc->hlen, doc->len, sizeofDoc, doc->v_major, doc->v_minor, vector.count(), alt_length,
               alt->m_magic,
               (CACHE_ALT_MAGIC_ALIVE == alt->m_magic ?
                  "alive" :
                  CACHE_ALT_MAGIC_MARSHALED == alt->m_magic ? "serial" : CACHE_ALT_MAGIC_DEAD == alt->m_magic ? "dead" : "bogus"));
          dir_delete(&key, vol, &dir);
        }
        err = ECACHE_BAD_META_DATA;
        goto Ldone;
      }
      // If @a params is @c NULL then we're a retry from a range request pair so don't do alt select.
      // Instead try the @a earliest_key - if that's a match then that's the correct alt, written
      // by the paired write VC.
      if (cache_config_select_alternate && params) {
        slice_ref._idx = HttpTransactCache::SelectFromAlternates(&vector, &request, params);
        if (slice_ref._idx < 0) {
          err = ECACHE_ALT_MISS;
          goto Ldone;
        }
        Debug("amc", "[openReadStartHead] select alt: %d %p (current %p, od %p)", slice_ref._idx,
              vector.get(slice_ref._idx)->m_alt, alternate.m_alt, od);
      } else if (CACHE_ALT_INDEX_DEFAULT == (slice_ref._idx = get_alternate_index(&vector, earliest_key)))
        slice_ref._idx = 0;
      alternate_tmp     = vector.get(slice_ref._idx);
      if (!alternate_tmp->valid()) {
        if (buf) {
          Note("OpenReadHead failed for cachekey %X : alternate inconsistency", key.slice32(0));
          dir_delete(&key, vol, &dir);
        }
        goto Ldone;
      }

      alternate.copy_shallow(alternate_tmp);
      alternate.object_key_get(&key);
      doc_len = alternate.object_size_get();

      // If the object length is known we can check the range.
      // Otherwise we have to leave it vague and talk to the origin to get full length info.
      if (alternate.m_alt->m_flag.content_length_p && !resp_range.resolve(doc_len)) {
        err = ECACHE_UNSATISFIABLE_RANGE;
        goto Ldone;
      }
      if (resp_range.isMulti())
        resp_range.setContentTypeFromResponse(alternate.response_get()).generateBoundaryStr(earliest_key);

      if (key == doc->key) { // is this my data?
        f.single_fragment = doc->single_fragment();
        ink_assert(f.single_fragment); // otherwise need to read earliest
        ink_assert(doc->hlen);
        doc_pos = doc->prefix_len();
        next_CacheKey(&key, &doc->key);
        fragment         = 1;
        frag_upper_bound = doc->data_len();
      } else {
        f.single_fragment = false;
      }
    } else
#endif
    {
      next_CacheKey(&key, &doc->key);
      fragment          = 1;
      frag_upper_bound  = doc->data_len();
      f.single_fragment = doc->single_fragment();
      doc_pos           = doc->prefix_len();
      doc_len           = doc->total_len;
    }

    if (is_debug_tag_set("cache_read")) { // amc debug
      char xt[33], yt[33];
      Debug("cache_read", "CacheReadStartHead - read %s target %s - %s %d of %" PRId64 " bytes, %d fragments",
            doc->key.toHexStr(xt), key.toHexStr(yt), f.single_fragment ? "single" : "multi", doc->len, doc->total_len,
#ifdef HTTP_CACHE
            alternate.get_frag_count()
#else
            0
#endif
              );
    }
    // the first fragment might have been gc'ed. Make sure the first
    // fragment is there before returning CACHE_EVENT_OPEN_READ
    if (!f.single_fragment)
      goto Learliest;

    if (vol->within_hit_evacuate_window(&dir) &&
        (!cache_config_hit_evacuate_size_limit || doc_len <= (uint64_t)cache_config_hit_evacuate_size_limit)) {
      DDebug("cache_hit_evac", "dir: %" PRId64 ", write: %" PRId64 ", phase: %d", dir_offset(&dir),
             offset_to_vol_offset(vol, vol->header->write_pos), vol->header->phase);
      f.hit_evacuate = 1;
    }

    first_buf = buf;
    vol->begin_read(this);

    goto Lsuccess;

  Lread:
    // check for collision
    // INKqa07684 - Cache::lookup returns CACHE_EVENT_OPEN_READ_FAILED.
    // don't want to go through this BS of reading from a writer if
    // its a lookup. In this case lookup will fail while the document is
    // being written to the cache.
    OpenDirEntry *cod = vol->open_read(&key);
    if (cod && !f.read_from_writer_called) {
      if (f.lookup) {
        err = ECACHE_DOC_BUSY;
        goto Ldone;
      }
      od = cod;
      MUTEX_RELEASE(lock);
      SET_HANDLER(&CacheVC::openReadFromWriter);
      return handleEvent(EVENT_IMMEDIATE, nullptr);
    }
    if (dir_probe(&key, vol, &dir, &last_collision)) {
      first_dir = dir;
      int ret   = do_read_call(&key);
      if (ret == EVENT_RETURN)
        goto Lcallreturn;
      return ret;
    }
  }
Ldone:
  if (!f.lookup) {
    CACHE_INCREMENT_DYN_STAT(cache_read_failure_stat);
    _action.continuation->handleEvent(CACHE_EVENT_OPEN_READ_FAILED, (void *)-err);
  } else {
    CACHE_INCREMENT_DYN_STAT(cache_lookup_failure_stat);
    _action.continuation->handleEvent(CACHE_EVENT_LOOKUP_FAILED, (void *)-err);
  }
  return free_CacheVC(this);
Lcallreturn:
  return handleEvent(AIO_EVENT_DONE, nullptr); // hopefully a tail call
Lsuccess:
  SET_HANDLER(&CacheVC::openReadMain);
  return callcont(CACHE_EVENT_OPEN_READ);
Lookup:
  CACHE_INCREMENT_DYN_STAT(cache_lookup_success_stat);
  _action.continuation->handleEvent(CACHE_EVENT_LOOKUP, nullptr);
  return free_CacheVC(this);
Learliest:
  first_buf      = buf;
  buf            = nullptr;
  earliest_key   = key;
  last_collision = nullptr;
  SET_HANDLER(&CacheVC::openReadStartEarliest);
  return openReadStartEarliest(event, e);
}
