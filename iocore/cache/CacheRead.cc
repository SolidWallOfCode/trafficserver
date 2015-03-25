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
#include "HttpCacheSM.h"      //Added to get the scope of HttpCacheSM object.
#endif

#define READ_WHILE_WRITER 1
extern int cache_config_compatibility_4_2_0_fixup;

Action *
Cache::open_read(Continuation * cont, CacheKey * key, CacheFragType type, char *hostname, int host_len)
{
  if (!CacheProcessor::IsCacheReady(type)) {
    cont->handleEvent(CACHE_EVENT_OPEN_READ_FAILED, (void *) -ECACHE_NOT_READY);
    return ACTION_RESULT_DONE;
  }
  ink_assert(caches[type] == this);

  Vol *vol = key_to_vol(key, hostname, host_len);
  Dir result, *last_collision = NULL;
  ProxyMutex *mutex = cont->mutex;
  OpenDirEntry *od = NULL;
  CacheVC *c = NULL;
  {
    CACHE_TRY_LOCK(lock, vol->mutex, mutex->thread_holding);
    if (!lock.is_locked() || (od = vol->open_read(key)) || dir_probe(key, vol, &result, &last_collision)) {
      c = new_CacheVC(cont);
      c->vol = vol;
      c->first_key = c->key = c->earliest_key = *key;
      c->vio.op = VIO::READ;
      c->base_stat = cache_read_active_stat;
      c->od = od;
      c->frag_type = type;
      CACHE_INCREMENT_DYN_STAT(c->base_stat + CACHE_STAT_ACTIVE);
      SET_CONTINUATION_HANDLER(c, &CacheVC::openReadStartHead);
    }
    if (!c)
      goto Lmiss;
    if (!lock.is_locked()) {
      CONT_SCHED_LOCK_RETRY(c);
      return &c->_action;
    }
    if (c->od)
      goto Lwriter;
    c->dir = result;
    c->last_collision = last_collision;
    switch(c->do_read_call(&c->key)) {
      case EVENT_DONE: return ACTION_RESULT_DONE;
      case EVENT_RETURN: goto Lcallreturn;
      default: return &c->_action;
    }
  }
Lmiss:
  CACHE_INCREMENT_DYN_STAT(cache_read_failure_stat);
  cont->handleEvent(CACHE_EVENT_OPEN_READ_FAILED, (void *) -ECACHE_NO_DOC);
  return ACTION_RESULT_DONE;
Lwriter:
  SET_CONTINUATION_HANDLER(c, &CacheVC::openReadFromWriter);
  if (c->handleEvent(EVENT_IMMEDIATE, 0) == EVENT_DONE)
    return ACTION_RESULT_DONE;
  return &c->_action;
Lcallreturn:
  if (c->handleEvent(AIO_EVENT_DONE, 0) == EVENT_DONE)
    return ACTION_RESULT_DONE;
  return &c->_action;
}

#ifdef HTTP_CACHE
Action*
Cache::open_read(Continuation* cont, CacheVConnection* vc, HTTPHdr* client_request_hdr)
{
  Action* zret = ACTION_RESULT_DONE;

  CacheVC* write_vc = dynamic_cast<CacheVC*>(vc);
  if (write_vc) {
    Vol *vol = write_vc->vol;
    ProxyMutex *mutex = cont->mutex; // needed for stat macros
    CacheVC *c = new_CacheVC(cont);

    c->vol = write_vc->vol;
    c->first_key = write_vc->first_key;
    c->earliest_key = c->key = write_vc->earliest_key;
    c->vio.op = VIO::READ;
    c->base_stat = cache_read_active_stat;
    c->od = write_vc->od;
    c->frag_type = write_vc->frag_type;
    CACHE_INCREMENT_DYN_STAT(c->base_stat + CACHE_STAT_ACTIVE);
//    write_vc->alternate.request_get(&c->request);
//    client_request_hdr->copy_shallow(&c->request);
    c->request.copy_shallow(client_request_hdr);
    c->params = write_vc->params; // seems to be a no-op, always NULL.
    c->dir = c->first_dir = write_vc->first_dir;
    c->write_vc = write_vc;
    c->first_buf = write_vc->first_buf; // I don't think this is effective either.
    SET_CONTINUATION_HANDLER(c, &CacheVC::openReadFromWriter);
    zret = &c->_action; // default, override if needed.
    CACHE_TRY_LOCK(lock, vol->mutex, mutex->thread_holding);
    if (lock.is_locked() && c->handleEvent(EVENT_IMMEDIATE, 0) == EVENT_DONE) {
      zret = ACTION_RESULT_DONE;
    }
  }
  return zret;
}

Action *
Cache::open_read(Continuation * cont, CacheKey * key, CacheHTTPHdr * request,
                 CacheLookupHttpConfig * params, CacheFragType type, char *hostname, int host_len)
{

  if (!CacheProcessor::IsCacheReady(type)) {
    cont->handleEvent(CACHE_EVENT_OPEN_READ_FAILED, (void *) -ECACHE_NOT_READY);
    return ACTION_RESULT_DONE;
  }
  ink_assert(caches[type] == this);

  Vol *vol = key_to_vol(key, hostname, host_len);
  Dir result, *last_collision = NULL;
  ProxyMutex *mutex = cont->mutex;
  OpenDirEntry *od = NULL;
  CacheVC *c = NULL;

  {
    CACHE_TRY_LOCK(lock, vol->mutex, mutex->thread_holding);
    if (!lock.is_locked() || (od = vol->open_read(key)) || dir_probe(key, vol, &result, &last_collision)) {
      c = new_CacheVC(cont);
      c->vol = vol;
      c->first_key = c->key = c->earliest_key = *key;
      c->vio.op = VIO::READ;
      c->base_stat = cache_read_active_stat;
      c->od = od;
      c->frag_type = CACHE_FRAG_TYPE_HTTP;
      CACHE_INCREMENT_DYN_STAT(c->base_stat + CACHE_STAT_ACTIVE);
      c->request.copy_shallow(request);
      c->params = params;
    }
    if (!lock.is_locked()) {
      SET_CONTINUATION_HANDLER(c, &CacheVC::openReadStartHead);
      CONT_SCHED_LOCK_RETRY(c);
      return &c->_action;
    }
    if (!c)
      goto Lmiss;
    if (c->od)
      goto Lwriter;
    // hit
    c->dir = c->first_dir = result;
    c->last_collision = last_collision;
    SET_CONTINUATION_HANDLER(c, &CacheVC::openReadStartHead);
    switch(c->do_read_call(&c->key)) {
      case EVENT_DONE: return ACTION_RESULT_DONE;
      case EVENT_RETURN: goto Lcallreturn;
      default: return &c->_action;
    }
  }
Lmiss:
  CACHE_INCREMENT_DYN_STAT(cache_read_failure_stat);
  cont->handleEvent(CACHE_EVENT_OPEN_READ_FAILED, (void *) -ECACHE_NO_DOC);
  return ACTION_RESULT_DONE;
Lwriter:
  // this is a horrible violation of the interface and should be fixed (FIXME)
  ((HttpCacheSM *)cont)->set_readwhilewrite_inprogress(true);
  SET_CONTINUATION_HANDLER(c, &CacheVC::openReadFromWriter);
  if (c->handleEvent(EVENT_IMMEDIATE, 0) == EVENT_DONE)
    return ACTION_RESULT_DONE;
  return &c->_action;
Lcallreturn:
  if (c->handleEvent(AIO_EVENT_DONE, 0) == EVENT_DONE)
    return ACTION_RESULT_DONE;
  return &c->_action;
}
#endif

uint32_t
CacheVC::load_http_info(CacheHTTPInfoVector* info, Doc* doc, RefCountObj * block_ptr)
{
  uint32_t zret = info->get_handles(doc->hdr(), doc->hlen, block_ptr);
  if (zret != static_cast<uint32_t>(-1) && // Make sure we haven't already failed
      cache_config_compatibility_4_2_0_fixup && // manual override not engaged
      ! this->f.doc_from_ram_cache && // it's already been done for ram cache fragments
      vol->header->version.ink_major == 23 && vol->header->version.ink_minor == 0
    ) {
    for ( int i = info->xcount - 1 ; i >= 0 ; --i ) {
      info->data(i)._alternate.m_alt->m_response_hdr.m_mime->recompute_accelerators_and_presence_bits();
      info->data(i)._alternate.m_alt->m_request_hdr.m_mime->recompute_accelerators_and_presence_bits();
    }
  }
  return zret;
}

char const*
CacheVC::get_http_range_boundary_string(int* len) const
{
  return resp_range.getBoundaryStr(len);
}

int64_t
CacheVC::get_effective_content_size()
{
  return this->is_http_partial_content() ? resp_range.calcContentLength() : alternate.object_size_get();
}

HTTPRangeSpec&
CacheVC::get_http_range_spec()
{
  return resp_range.getRangeSpec();
}

bool
CacheVC::is_http_partial_content()
{
  return resp_range.hasRanges();
}

int
CacheVC::openReadFromWriterFailure(int event, Event * e)
{

  od = NULL;
  vector.clear(false);
  CACHE_INCREMENT_DYN_STAT(cache_read_failure_stat);
  CACHE_INCREMENT_DYN_STAT(cache_read_busy_failure_stat);
  _action.continuation->handleEvent(event, e);
  free_CacheVC(this);
  return EVENT_DONE;
}

int
CacheVC::openReadChooseWriter(int /* event ATS_UNUSED */, Event * /* e ATS_UNUSED */)
{
//  intptr_t err = ECACHE_DOC_BUSY;
//  CacheVC *w = NULL;

  ink_assert(vol->mutex->thread_holding == mutex->thread_holding && write_vc == NULL);

  if (frag_type != CACHE_FRAG_TYPE_HTTP) {
    ink_release_assert(! "[amc] Fix reader from writer for non-HTTP");
  }
#ifdef HTTP_CACHE
  else {
    write_vector = &od->vector;
    int write_vec_cnt = write_vector->count();
    for (int c = 0; c < write_vec_cnt; c++)
      vector.insert(write_vector->get(c));
    if (!vector.count()) {
      // [amc] Not sure of the utility of this now
      if (od->reading_vec) {
       // the writer(s) are reading the vector, so there is probably
        // an old vector. Since this reader came before any of the
        // current writers, we should return the old data
        od = NULL;
        return EVENT_RETURN;
      }
      return -ECACHE_NO_DOC;
    }
    if (cache_config_select_alternate) {
      alternate_index = HttpTransactCache::SelectFromAlternates(&vector, &request, params);
      if (alternate_index < 0)
        return -ECACHE_ALT_MISS;
    } else
      alternate_index = 0;
    vector.clear(false);
    DDebug("cache_read_agg",
          "%p: key: %X eKey: %d # alts: %d, ndx: %d, # writers: %d writer: %p",
          this, first_key.slice32(1), write_vc->earliest_key.slice32(1),
          vector.count(), alternate_index, od->num_writers, write_vc);
  }
#endif //HTTP_CACHE
  return EVENT_NONE;
}

int
CacheVC::openReadFromWriter(int event, Event * e)
{
  if (!f.read_from_writer_called) {
    // The assignment to last_collision as NULL was
    // made conditional after INKqa08411
    last_collision = NULL;
    // Let's restart the clock from here - the first time this a reader
    // gets in this state. Its possible that the open_read was called
    // before the open_write, but the reader could not get the volume
    // lock. If we don't reset the clock here, we won't choose any writer
    // and hence fail the read request.
    start_time = ink_get_hrtime();
    f.read_from_writer_called = 1;
  }
  cancel_trigger();
  intptr_t err = ECACHE_DOC_BUSY;
  DDebug("cache_read_agg", "%p: key: %X In openReadFromWriter", this, first_key.slice32(1));
#ifndef READ_WHILE_WRITER
  return openReadFromWriterFailure(CACHE_EVENT_OPEN_READ_FAILED, (Event *) -err);
#else
  if (_action.cancelled) {
    od = NULL; // only open for read so no need to close
    return free_CacheVC(this);
  }
  CACHE_TRY_LOCK(lock, vol->mutex, mutex->thread_holding);
  if (!lock.is_locked())
    VC_SCHED_LOCK_RETRY();
  od = vol->open_read(&first_key); // recheck in case the lock failed
  if (!od) {
    MUTEX_RELEASE(lock);
    write_vc = NULL;
    SET_HANDLER(&CacheVC::openReadStartHead);
    return openReadStartHead(event, e);
  } else
    ink_assert(od == vol->open_read(&first_key));

  CACHE_TRY_LOCK(lock_od, od->mutex, mutex->thread_holding);
  if (!lock_od.is_locked())
    VC_SCHED_LOCK_RETRY();

  if (od->open_writer) {
    // Can't pick a writer yet, so wait for it.
    if (!od->open_waiting.in(this)) {
      wake_up_thread = mutex->thread_holding;
      od->open_waiting.push(this);
    }
    Debug("amc", "[CacheVC::openReadFromWriter] waiting for %p", od->open_writer);
    return EVENT_CONT; // wait for the writer to wake us up.
  }

  MUTEX_RELEASE(lock); // we have the OD lock now, don't need the vol lock.

  if (write_vc && CACHE_ALT_INDEX_DEFAULT != (alternate_index = get_alternate_index(&(od->vector), earliest_key))) {
    alternate.copy_shallow(od->vector.get(alternate_index));
    MUTEX_RELEASE(lock_od);
    SET_HANDLER(&CacheVC::openReadStartEarliest);
    return openReadStartEarliest(event, e);
  } else {
    int ret = openReadChooseWriter(event, e);
    if (ret < 0) {
      MUTEX_RELEASE(lock_od);
      SET_HANDLER(&CacheVC::openReadFromWriterFailure);
      return openReadFromWriterFailure(CACHE_EVENT_OPEN_READ_FAILED, reinterpret_cast<Event *> (ret));
    } else if (ret == EVENT_RETURN) {
      MUTEX_RELEASE(lock_od);
      SET_HANDLER(&CacheVC::openReadStartHead);
      return openReadStartHead(event, e);
    } else if (ret == EVENT_CONT) {
      ink_assert(!write_vc);
      VC_SCHED_WRITER_RETRY();
    } else
      ink_assert(write_vc);
  }
#ifdef HTTP_CACHE
  OpenDirEntry *cod = od;
#endif
  od = NULL;
  // someone is currently writing the document
  if (write_vc->closed < 0) {
    MUTEX_RELEASE(lock_od);
    write_vc = NULL;
    //writer aborted, continue as if there is no writer
    SET_HANDLER(&CacheVC::openReadStartHead);
    return openReadStartHead(EVENT_IMMEDIATE, 0);
  }
  // allow reading from unclosed writer for http requests only.
  ink_assert(frag_type == CACHE_FRAG_TYPE_HTTP || write_vc->closed);
  if (!write_vc->closed && !write_vc->fragment) {
    if (!cache_config_read_while_writer || frag_type != CACHE_FRAG_TYPE_HTTP) {
      MUTEX_RELEASE(lock_od);
      return openReadFromWriterFailure(CACHE_EVENT_OPEN_READ_FAILED, (Event *) - err);
    }
    DDebug("cache_read_agg",
          "%p: key: %X writer: closed:%d, fragment:%d, retry: %d",
          this, first_key.slice32(1), write_vc->closed, write_vc->fragment, writer_lock_retry);
    VC_SCHED_WRITER_RETRY();
  }

  CACHE_TRY_LOCK(writer_lock, write_vc->mutex, mutex->thread_holding);
  if (!writer_lock.is_locked()) {
    DDebug("cache_read_agg", "%p: key: %X lock miss", this, first_key.slice32(1));
    VC_SCHED_LOCK_RETRY();
  }
  MUTEX_RELEASE(lock);

  if (!write_vc->io.ok())
    return openReadFromWriterFailure(CACHE_EVENT_OPEN_READ_FAILED, (Event *) - err);
#ifdef HTTP_CACHE
  if (frag_type == CACHE_FRAG_TYPE_HTTP) {
    DDebug("cache_read_agg",
          "%p: key: %X http passed stage 1, closed: %d, frag: %d",
          this, first_key.slice32(1), write_vc->closed, write_vc->fragment);
    if (!write_vc->alternate.valid())
      return openReadFromWriterFailure(CACHE_EVENT_OPEN_READ_FAILED, (Event *) - err);
    alternate.copy(&write_vc->alternate);
    vector.insert(&alternate);
    alternate.object_key_get(&key);
    write_vc->f.readers = 1;
    if (!(write_vc->f.update && write_vc->total_len == 0)) {
      key = write_vc->earliest_key;
      if (!write_vc->closed)
        alternate.object_size_set(write_vc->vio.nbytes);
      else
        alternate.object_size_set(write_vc->total_len);
    } else {
      key = write_vc->update_key;
      ink_assert(write_vc->closed);
      DDebug("cache_read_agg", "%p: key: %X writer header update", this, first_key.slice32(1));
      // Update case (b) : grab doc_len from the writer's alternate
      doc_len = alternate.object_size_get();
      if (write_vc->update_key == cod->single_doc_key &&
          (cod->move_resident_alt || write_vc->f.rewrite_resident_alt) && write_vc->first_buf._ptr()) {
        // the resident alternate is being updated and its a
        // header only update. The first_buf of the writer has the
        // document body.
        Doc *doc = (Doc *) write_vc->first_buf->data();
        writer_buf = new_IOBufferBlock(write_vc->first_buf, doc->data_len(), doc->prefix_len());
        MUTEX_RELEASE(writer_lock);
        ink_assert(doc_len == doc->data_len());
        length = doc_len;
        f.single_fragment = 1;
        doc_pos = 0;
        earliest_key = key;
        dir_clean(&first_dir);
        dir_clean(&earliest_dir);
        SET_HANDLER(&CacheVC::openReadFromWriterMain);
        CACHE_INCREMENT_DYN_STAT(cache_read_busy_success_stat);
        return callcont(CACHE_EVENT_OPEN_READ);
      }
      // want to snarf the new headers from the writer
      // and then continue as if nothing happened
      last_collision = NULL;
      MUTEX_RELEASE(writer_lock);
      SET_HANDLER(&CacheVC::openReadStartEarliest);
      return openReadStartEarliest(event, e);
    }
  } else {
#endif //HTTP_CACHE
    DDebug("cache_read_agg", "%p: key: %X non-http passed stage 1", this, first_key.slice32(1));
    key = write_vc->earliest_key;
#ifdef HTTP_CACHE
  }
#endif
  if (write_vc->fragment) {
    doc_len = write_vc->vio.nbytes;
    last_collision = NULL;
    DDebug("cache_read_agg",
          "%p: key: %X closed: %d, fragment: %d, len: %d starting first fragment",
          this, first_key.slice32(1), write_vc->closed, write_vc->fragment, (int)doc_len);
    MUTEX_RELEASE(writer_lock);
    // either a header + body update or a new document
    SET_HANDLER(&CacheVC::openReadStartEarliest);
    return openReadStartEarliest(event, e);
  }
  writer_buf = write_vc->blocks;
  writer_offset = write_vc->offset;
  length = write_vc->length;
  // copy the vector
  f.single_fragment = !write_vc->fragment;        // single fragment doc
  doc_pos = 0;
  earliest_key = write_vc->earliest_key;
  ink_assert(earliest_key == key);
  doc_len = write_vc->total_len;
  dir_clean(&first_dir);
  dir_clean(&earliest_dir);
  DDebug("cache_read_agg", "%p: key: %X %X: single fragment read", this, first_key.slice32(1), key.slice32(0));
  MUTEX_RELEASE(writer_lock);
  SET_HANDLER(&CacheVC::openReadFromWriterMain);
  CACHE_INCREMENT_DYN_STAT(cache_read_busy_success_stat);
  return callcont(CACHE_EVENT_OPEN_READ);
#endif //READ_WHILE_WRITER
}

int
CacheVC::openReadFromWriterMain(int /* event ATS_UNUSED */, Event * /* e ATS_UNUSED */)
{
  cancel_trigger();
  if (seek_to) {
    vio.ndone = seek_to;
    seek_to = 0;
  }
  IOBufferBlock *b = NULL;
  int64_t ntodo = vio.ntodo();
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
    iobufferblock_skip(writer_buf, &writer_offset, &length, skip_bytes);
  }
  int64_t bytes = length;
  if (bytes > vio.ntodo())
    bytes = vio.ntodo();
  if (vio.ndone >= (int64_t)doc_len) {
    ink_assert(bytes <= 0);
    // reached the end of the document and the user still wants more
    return calluser(VC_EVENT_EOS);
  }
  b = iobufferblock_clone(writer_buf, writer_offset, bytes);
  writer_buf = iobufferblock_skip(writer_buf, &writer_offset, &length, bytes);
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
CacheVC::openReadReadDone(int event, Event * e)
{
  Doc *doc = NULL;

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
    if (last_collision &&         // no missed lock
        dir_valid(vol, &dir))    // object still valid
    {
      doc = (Doc *) buf->data();
      if (doc->magic != DOC_MAGIC) {
        char tmpstring[100];
        if (doc->magic == DOC_CORRUPT)
          Warning("Middle: Doc checksum does not match for %s", key.toHexStr(tmpstring));
        else
          Warning("Middle: Doc magic does not match for %s", key.toHexStr(tmpstring));
#if TS_USE_INTERIM_CACHE == 1
        if (dir_ininterim(&dir)) {
          dir_delete(&key, vol, &dir);
          goto Lread;
        }
#endif
        goto Lerror;
      }
      if (doc->key == key)
        goto LreadMain;
#if TS_USE_INTERIM_CACHE == 1
      else if (dir_ininterim(&dir)) {
          dir_delete(&key, vol, &dir);
          last_collision = NULL;
        }
#endif
    }
#if TS_USE_INTERIM_CACHE == 1
    if (last_collision && dir_get_offset(&dir) != dir_get_offset(last_collision))
      last_collision = 0;
Lread:
#else
    if (last_collision && dir_offset(&dir) != dir_offset(last_collision))
      last_collision = 0;       // object has been/is being overwritten
#endif
    if (dir_probe(&key, vol, &dir, &last_collision)) {
      int ret = do_read_call(&key);
      if (ret == EVENT_RETURN)
        goto Lcallreturn;
      return EVENT_CONT;
    } else if (write_vc) {
      ink_release_assert(! "[amc] Handle this");
# if 0
      if (writer_done()) {
        last_collision = NULL;
        while (dir_probe(&earliest_key, vol, &dir, &last_collision)) {
#if TS_USE_INTERIM_CACHE == 1
          if (dir_get_offset(&dir) == dir_get_offset(&earliest_dir)) {
#else
          if (dir_offset(&dir) == dir_offset(&earliest_dir)) {
#endif
            DDebug("cache_read_agg", "%p: key: %X ReadRead complete: %d",
                  this, first_key.slice32(1), (int)vio.ndone);
            doc_len = vio.ndone;
            goto Ldone;
          }
        }
        DDebug("cache_read_agg", "%p: key: %X ReadRead writer aborted: %d",
              this, first_key.slice32(1), (int)vio.ndone);
        goto Lerror;
      }
      DDebug("cache_read_agg", "%p: key: %X ReadRead retrying: %d", this, first_key.slice32(1), (int)vio.ndone);
      VC_SCHED_WRITER_RETRY(); // wait for writer
# endif
    }
    // fall through for truncated documents
  }
Lerror:
  char tmpstring[100];
  Warning("Document %s truncated", earliest_key.toHexStr(tmpstring));
  return calluser(VC_EVENT_ERROR);
//Ldone:
  return calluser(VC_EVENT_EOS);
Lcallreturn:
  return handleEvent(AIO_EVENT_DONE, 0);
LreadMain:
  ++fragment;
  doc_pos = doc->prefix_len();
  doc_pos += resp_range.getOffset() - frag_upper_bound; // used before update!
  frag_upper_bound += doc->data_len();
  next_CacheKey(&key, &key);
  SET_HANDLER(&CacheVC::openReadMain);
  return openReadMain(event, e);
}

void
CacheVC::update_key_to_frag_idx(int target)
{
  FragmentDescriptorTable& frags = (*alternate.get_frag_table());

  if (0 == target) {
    fragment = 0;
    key = earliest_key;
  } else if (! frags[target].m_key.is_zero()) {
    key = frags[target].m_key;
  } else { // step through the hard way
    if (target < fragment) { // going backwards
      if (target < (fragment - target)) { // faster to go from start
        fragment = 0;
        key = earliest_key;
      } else { // quicker to go from here to there
        while (target < fragment) {
          prev_CacheKey(&key, &key);
          --fragment;
          if (frags[fragment].m_key.is_zero())
            frags[fragment].m_key = key; // might as well store it as we go.
          else ink_assert(frags[fragment].m_key == key);
        }
      }
    }
    // advance to target if we're not already there.
    while (target > fragment) {
      next_CacheKey(&key, &key);
      ++fragment;
      if (frags[fragment].m_key.is_zero())
        frags[fragment].m_key = key; // might as well store it as we go.
      else ink_assert(frags[fragment].m_key == key);
    }
  }
}

int
CacheVC::frag_idx_for_offset(uint64_t offset)
{
  FragmentDescriptorTable* frags = alternate.get_frag_table();
  int count = alternate.get_frag_count();
  uint32_t ffs = alternate.get_frag_fixed_size();
  int idx = count / 2;

  ink_assert(offset < doc_len);

  if (ffs) idx = offset / ffs; // good guess as to the right offset.

  if (count > 1 && 0 == (*frags)[1].m_offset) ++idx;

  do {
    uint64_t upper = idx >= count ? doc_len : (*frags)[idx+1].m_offset;
    uint64_t lower = idx <= 0 ? 0 : (*frags)[idx].m_offset;
    if (offset < lower) idx = idx / 2;
    else if (offset >= upper) idx = (count + idx + 1)/2;
    else break;
  } while (true);
  return idx;
}

/* There is a fragment available, decide what do to next.
 */
int
CacheVC::openReadMain(int /* event ATS_UNUSED */, Event * /* e ATS_UNUSED */)
{
  cancel_trigger();
  Doc *doc = (Doc *) buf->data();
  int64_t bytes = vio.ntodo();
  IOBufferBlock *b = NULL;
  uint64_t target_offset = resp_range.getOffset();
  uint64_t lower_bound = frag_upper_bound - doc->data_len();

  if (bytes <= 0)
    return EVENT_CONT;

  // Start shipping
  while (bytes > 0 && lower_bound <= target_offset && target_offset < frag_upper_bound) {
    if (vio.buffer.writer()->max_read_avail() > vio.buffer.writer()->water_mark && vio.ndone) // wait for reader
      return EVENT_CONT;

    if (resp_range.hasPendingRangeShift()) { // in new range, shift to start location.
      int b_len;
      char const* b_str = resp_range.getBoundaryStr(&b_len);
      size_t r_idx = resp_range.getIdx();

      doc_pos = doc->prefix_len() + (target_offset - lower_bound);
      
      vio.ndone += HTTPRangeSpec::writePartBoundary(vio.buffer.writer(), b_str, b_len
                                                    , doc_len, resp_range[r_idx]._min, resp_range[r_idx]._max
                                                    , resp_range.getContentTypeField(), r_idx >= (resp_range.count() - 1)
        );
      resp_range.consumeRangeShift();
      Debug("amc", "Range boundary for range %" PRIu64, r_idx);
    }

    bytes = std::min(doc->len - doc_pos, static_cast<int64_t>(resp_range.getRemnantSize()));
    bytes = std::min(bytes, vio.ntodo());
    if (bytes > 0) {
      b = new_IOBufferBlock(buf, bytes, doc_pos);
      b->_buf_end = b->_end;
      vio.buffer.writer()->append_block(b);
      vio.ndone += bytes;
      doc_pos += bytes;
      resp_range.consume(bytes);
      Debug("amc", "shipped %" PRId64 " bytes at target offset %" PRIu64, bytes, target_offset);
      target_offset = resp_range.getOffset();
    }

    if (vio.ntodo() <= 0)
      return calluser(VC_EVENT_READ_COMPLETE);
    else if (calluser(VC_EVENT_READ_READY) == EVENT_DONE)
      return EVENT_DONE;
  }


#ifdef HTTP_CACHE
  if (resp_range.getRemnantSize()) {
    FragmentDescriptorTable* frags = alternate.get_frag_table();
    int n_frags = alternate.get_frag_count();

    // Quick check for offset in next fragment - very common
    if (target_offset >= frag_upper_bound && (!frags || fragment > n_frags || target_offset < (*frags)[fragment].m_offset)) {
      Debug("amc", "Non-seeking continuation to next fragment");
    } else {
      int target = -1; // target fragment index.

      if (is_debug_tag_set("amc")) {
        char b[33], c[33];
        Debug("amc", "Seek @ %" PRIu64 " [r#=%d] in %s from #%d @ %" PRIu64 "/%d/%" PRId64 ":%s%s",
              target_offset, resp_range.getIdx(), first_key.toHexStr(b), fragment, frag_upper_bound, doc->len, doc->total_len, doc->key.toHexStr(c)
              , (frags ? "" : "no frag table")
          );
      }

      target = this->frag_idx_for_offset(target_offset);
      this->update_key_to_frag_idx(target);
      /// one frag short, because it gets bumped when the fragment is actually read.
      frag_upper_bound = target > 0 ? (*frags)[target].m_offset : 0;
      Debug("amc", "Fragment seek from %d to %d target offset %" PRIu64, fragment - 1, target, target_offset);
    }
  }
#endif

  if (vio.ntodo() > 0 && 0 == resp_range.getRemnantSize())
    // reached the end of the document and the user still wants more
    return calluser(VC_EVENT_EOS);
  last_collision = 0;
  writer_lock_retry = 0;
  // if the state machine calls reenable on the callback from the cache,
  // we set up a schedule_imm event. The openReadReadDone discards
  // EVENT_IMMEDIATE events. So, we have to cancel that trigger and set
  // a new EVENT_INTERVAL event.
  cancel_trigger();
  CACHE_TRY_LOCK(lock, vol->mutex, mutex->thread_holding);
  if (!lock.is_locked()) {
    SET_HANDLER(&CacheVC::openReadMain);
    VC_SCHED_LOCK_RETRY();
  }
  if (dir_probe(&key, vol, &dir, &last_collision)) {
    SET_HANDLER(&CacheVC::openReadReadDone);
    int ret = do_read_call(&key);
    if (ret == EVENT_RETURN)
      goto Lcallreturn;
    return EVENT_CONT;
  } else if (write_vc) {
    ink_release_assert(! "[amc] Handle case where fragment is not in the directory during read");
    /* Check for an OpenDirEntry - if none, no hope. If so, see if any of the writers in it are still
       planning to write this fragment (might check against all fragments - if any aren't going to
       be written, might just give up at that point rather than hope some other user agent will happen
       to ask for the missing fragments. Although, if there's a herd, that might not be so far fetched).
    */
# if 0
    if (writer_done()) {
      last_collision = NULL;
      while (dir_probe(&earliest_key, vol, &dir, &last_collision)) {
        if (dir_offset(&dir) == dir_offset(&earliest_dir)) {
          DDebug("cache_read_agg", "%p: key: %X ReadMain complete: %d",
                 this, first_key.slice32(1), (int)vio.ndone);
          doc_len = vio.ndone;
          goto Leos;
        }
      }
      DDebug("cache_read_agg", "%p: key: %X ReadMain writer aborted: %d",
             this, first_key.slice32(1), (int)vio.ndone);
      goto Lerror;
    }
    DDebug("cache_read_agg", "%p: key: %X ReadMain retrying: %d", this, first_key.slice32(1), (int)vio.ndone);
    SET_HANDLER(&CacheVC::openReadMain);
    VC_SCHED_WRITER_RETRY();
# endif
  }
  if (is_action_tag_set("cache"))
    ink_release_assert(false);
  Warning("Document %X truncated at %d of %d, missing fragment %X", first_key.slice32(1), (int)vio.ndone, (int)doc_len, key.slice32(1));
  // remove the directory entry
  dir_delete(&earliest_key, vol, &earliest_dir);
//Lerror:
  return calluser(VC_EVENT_ERROR);
//Leos:
  return calluser(VC_EVENT_EOS);
Lcallreturn:
  return handleEvent(AIO_EVENT_DONE, 0);
}

int
CacheVC::openReadWaitEarliest(int evid, Event*)
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
    od = NULL;
    key = first_key;
    return handleEvent(EVENT_IMMEDIATE, 0);
  } else if (dir_probe(&key, vol, &earliest_dir, &last_collision) ||
      dir_lookaside_probe(&key, vol, &earliest_dir, NULL))
  {
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
  int ret = 0;
  Doc *doc = NULL;
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
        last_collision = NULL;
      goto Lread;
    }
    doc = (Doc *) buf->data();
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
      last_collision = NULL;
      goto Lread;
    }
    if (!(doc->key == key)) // collisiion
      goto Lread;
    // success
    earliest_key = key;
    doc_pos = doc->prefix_len();
    next_CacheKey(&key, &doc->key);
    fragment = 1;
    frag_upper_bound = doc->data_len();
    vol->begin_read(this);
    if (vol->within_hit_evacuate_window(&earliest_dir) &&
        (!cache_config_hit_evacuate_size_limit || doc_len <= (uint64_t)cache_config_hit_evacuate_size_limit)
#if TS_USE_INTERIM_CACHE == 1
        && !dir_ininterim(&dir)
#endif
        ) {
      DDebug("cache_hit_evac", "dir: %" PRId64", write: %" PRId64", phase: %d",
            dir_offset(&earliest_dir), offset_to_vol_offset(vol, vol->header->write_pos), vol->header->phase);
      f.hit_evacuate = 1;
    }
    goto Lsuccess;
Lread:
    if (dir_probe(&key, vol, &earliest_dir, &last_collision) ||
        dir_lookaside_probe(&key, vol, &earliest_dir, NULL))
    {
      dir = earliest_dir;
#if TS_USE_INTERIM_CACHE == 1
      if (dir_ininterim(&dir) && alternate.get_frag_count() > 1) {
        dir_delete(&key, vol, &dir);
        last_collision = NULL;
        goto Lread;
      }
#endif
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
      od->waiting_for(earliest_key, this, 0);
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
      if (!vol->open_write(this, false, 1)) {
        Doc *doc1 = (Doc *) first_buf->data();
        uint32_t len = this->load_http_info(write_vector, doc1);
        ink_assert(len == doc1->hlen && write_vector->count() > 0);
        write_vector->remove(alternate_index, true);
        // if the vector had one alternate, delete it's directory entry
        if (len != doc1->hlen || !write_vector->count()) {
          // sometimes the delete fails when there is a race and another read
          // finds that the directory entry has been overwritten
          // (cannot assert on the return value)
          dir_delete(&first_key, vol, &first_dir);
        }
#if TS_USE_INTERIM_CACHE == 1
        else if (dir_ininterim(&first_dir))
          dir_delete(&first_key, vol, &first_dir);
#endif
        else {
          buf = NULL;
          last_collision = NULL;
          write_len = 0;
          header_len = write_vector->marshal_length();
          f.evac_vector = 1;
          f.use_first_key = 1;
          key = first_key;
          // always use od->first_dir to overwrite a directory.
          // If an evacuation happens while a vector is being updated
          // the evacuator changes the od->first_dir to the new directory
          // that it inserted
          od->first_dir = first_dir;
          od->writing_vec = 1;
          earliest_key = zero_key;

          // set up this VC as a alternate delete write_vc
          vio.op = VIO::WRITE;
          total_len = 0;
          f.update = 1;
          alternate_index = CACHE_ALT_REMOVED;
          /////////////////////////////////////////////////////////////////
          // change to create a directory entry for a resident alternate //
          // when another alternate does not exist.                      //
          /////////////////////////////////////////////////////////////////
          if (doc1->total_len > 0) {
            od->move_resident_alt = 1;
            od->single_doc_key = doc1->key;
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
  _action.continuation->handleEvent(CACHE_EVENT_OPEN_READ_FAILED, (void *) -ECACHE_NO_DOC);
  return free_CacheVC(this);
Lcallreturn:
  return handleEvent(AIO_EVENT_DONE, 0); // hopefully a tail call
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
    return openWriteCloseDir(EVENT_IMMEDIATE, 0);
  {
    CACHE_TRY_LOCK(lock, vol->mutex, mutex->thread_holding);
    if (!lock.is_locked())
      VC_SCHED_LOCK_RETRY();
    if (io.ok()) {
      ink_assert(f.evac_vector);
      ink_assert(frag_type == CACHE_FRAG_TYPE_HTTP);
      ink_assert(!buf.m_ptr);
      f.evac_vector = false;
      last_collision = NULL;
      f.update = 0;
      alternate_index = CACHE_ALT_INDEX_DEFAULT;
      f.use_first_key = 0;
      vio.op = VIO::READ;
      dir_overwrite(&first_key, vol, &dir, &od->first_dir);
      if (od->move_resident_alt)
        dir_insert(&od->single_doc_key, vol, &od->single_doc_dir);
      int alt_ndx = HttpTransactCache::SelectFromAlternates(write_vector, &request, params);
      vol->close_write(this);
      if (alt_ndx >= 0) {
        vector.clear();
        // we don't need to start all over again, since we already
        // have the vector in memory. But this is simpler and this
        // case is rare.
        goto Lrestart;
      }
    } else
      vol->close_write(this);
  }

  CACHE_INCREMENT_DYN_STAT(cache_read_failure_stat);
  _action.continuation->handleEvent(CACHE_EVENT_OPEN_READ_FAILED, (void *) -ECACHE_ALT_MISS);
  return free_CacheVC(this);
Lrestart:
  SET_HANDLER(&CacheVC::openReadStartHead);
  return openReadStartHead(EVENT_IMMEDIATE, 0);
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
CacheVC::openReadStartHead(int event, Event * e)
{
  intptr_t err = ECACHE_NO_DOC;
  Doc *doc = NULL;
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
        last_collision = NULL;
#if TS_USE_INTERIM_CACHE == 1
      if (dir_ininterim(&dir)) {
        dir_delete(&key, vol, &dir);
        last_collision = NULL;
      }
#endif
      goto Lread;
    }
    doc = (Doc *) buf->data();
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
      last_collision = NULL;
      goto Lread;
    }
    if (!(doc->first_key == key))
#if TS_USE_INTERIM_CACHE == 1
    {
      if (dir_ininterim(&dir)) {
        dir_delete(&key, vol, &dir);
        last_collision = NULL;
      }
      goto Lread;
    }
#else
      goto Lread;
#endif
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
          HTTPCacheAlt* alt = reinterpret_cast<HTTPCacheAlt*>(doc->hdr());
          int32_t alt_length = 0;
          // count should be reasonable, as vector is initialized and unlikly to be too corrupted
          // by bad disk data - count should be the number of successfully unmarshalled alts.
          for ( int32_t i = 0 ; i < vector.count() ; ++i ) {
            CacheHTTPInfo* info = vector.get(i);
            if (info && info->m_alt) alt_length += info->m_alt->m_unmarshal_len;
          }
          Note("OpenReadHead failed for cachekey %X : vector inconsistency - "
               "unmarshalled %d expecting %d in %d (base=%d, ver=%d:%d) "
               "- vector n=%d size=%d"
               "first alt=%d[%s]"
               , key.slice32(0)
               , uml, doc->hlen, doc->len, sizeofDoc
               , doc->v_major, doc->v_minor
               , vector.count(), alt_length
               , alt->m_magic
               , (CACHE_ALT_MAGIC_ALIVE == alt->m_magic ? "alive"
                  : CACHE_ALT_MAGIC_MARSHALED == alt->m_magic ? "serial"
                  : CACHE_ALT_MAGIC_DEAD == alt->m_magic ? "dead"
                  : "bogus")
            );
          dir_delete(&key, vol, &dir);
        }
        err = ECACHE_BAD_META_DATA;
        goto Ldone;
      }
      // If @a params is @c NULL then we're a retry from a range request pair so don't do alt select.
      // Instead try the @a earliest_key - if that's a match then that's the correct alt, written
      // by the paired write VC.
      if (cache_config_select_alternate && params) {
        alternate_index = HttpTransactCache::SelectFromAlternates(&vector, &request, params);
        if (alternate_index < 0) {
          err = ECACHE_ALT_MISS;
          goto Ldone;
        }
      } else if (CACHE_ALT_INDEX_DEFAULT == (alternate_index = get_alternate_index(&vector, earliest_key)))
        alternate_index = 0;
      alternate_tmp = vector.get(alternate_index);
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

      // Handle any range related setup.
      if (!resp_range.init(&request)) { // shouldn't this have been checked earlier?
        err = ECACHE_INVALID_RANGE;
        goto Ldone;
      }
      // If the object length is known we can check the range.
      // Otherwise we have to leave it vague and talk to the origin to get full length info.
      if (alternate.m_alt->m_flag.content_length_p && !resp_range.apply(doc_len)) {
        err = ECACHE_UNSATISFIABLE_RANGE;
        goto Ldone;
      }
      if (resp_range.isMulti())
        resp_range.setContentTypeFromResponse(alternate.response_get()).generateBoundaryStr(earliest_key);
//      alternate.get_uncached(resp_range.getRangeSpec(), uncached_range.getRangeSpec());

      if (key == doc->key) {      // is this my data?
        f.single_fragment = doc->single_fragment();
        ink_assert(f.single_fragment);     // otherwise need to read earliest
        ink_assert(doc->hlen);
        doc_pos = doc->prefix_len();
        next_CacheKey(&key, &doc->key);
        fragment = 1;
        frag_upper_bound = doc->data_len();
      } else {
        f.single_fragment = false;
      }
    } else
#endif
    {
      next_CacheKey(&key, &doc->key);
      fragment = 1;
      frag_upper_bound = doc->data_len();
      f.single_fragment = doc->single_fragment();
      doc_pos = doc->prefix_len();
      doc_len = doc->total_len;
    }

    if (is_debug_tag_set("cache_read")) { // amc debug
      char xt[33],yt[33];
      Debug("cache_read", "CacheReadStartHead - read %s target %s - %s %d of %" PRId64" bytes, %d fragments",
            doc->key.toHexStr(xt), key.toHexStr(yt),
            f.single_fragment ? "single" : "multi",
            doc->len, doc->total_len,
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
        (!cache_config_hit_evacuate_size_limit || doc_len <= (uint64_t)cache_config_hit_evacuate_size_limit)
#if TS_USE_INTERIM_CACHE == 1
        && !f.read_from_interim
#endif
        ) {
      DDebug("cache_hit_evac", "dir: %" PRId64", write: %" PRId64", phase: %d",
            dir_offset(&dir), offset_to_vol_offset(vol, vol->header->write_pos), vol->header->phase);
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
      return handleEvent(EVENT_IMMEDIATE, 0);
    }
    if (dir_probe(&key, vol, &dir, &last_collision)) {
      first_dir = dir;
      int ret = do_read_call(&key);
      if (ret == EVENT_RETURN)
        goto Lcallreturn;
      return ret;
    }
  }
Ldone:
  if (!f.lookup) {
    CACHE_INCREMENT_DYN_STAT(cache_read_failure_stat);
    _action.continuation->handleEvent(CACHE_EVENT_OPEN_READ_FAILED, (void *) -err);
  } else {
    CACHE_INCREMENT_DYN_STAT(cache_lookup_failure_stat);
    _action.continuation->handleEvent(CACHE_EVENT_LOOKUP_FAILED, (void *) -err);
  }
  return free_CacheVC(this);
Lcallreturn:
  return handleEvent(AIO_EVENT_DONE, 0); // hopefully a tail call
Lsuccess:
  SET_HANDLER(&CacheVC::openReadMain);
  return callcont(CACHE_EVENT_OPEN_READ);
Lookup:
  CACHE_INCREMENT_DYN_STAT(cache_lookup_success_stat);
  _action.continuation->handleEvent(CACHE_EVENT_LOOKUP, 0);
  return free_CacheVC(this);
Learliest:
  first_buf = buf;
  buf = NULL;
  earliest_key = key;
  last_collision = NULL;
  SET_HANDLER(&CacheVC::openReadStartEarliest);
  return openReadStartEarliest(event, e);
}
