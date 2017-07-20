.. Licensed to the Apache Software Foundation (ASF) under one
   or more contributor license agreements.  See the NOTICE file
   distributed with this work for additional information
   regarding copyright ownership.  The ASF licenses this file
   to you under the Apache License, Version 2.0 (the
   "License"); you may not use this file except in compliance
   with the License.  You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing,
   software distributed under the License is distributed on an
   "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
   KIND, either express or implied.  See the License for the
   specific language governing permissions and limitations
   under the License.

.. include:: ../../common.defs

.. default-domain:: cpp

.. _partial-cache-operations:

Structures
***************

.. class:: OpenDirEntry

   Represents an active cache object, i.e. one that is being used by a transaction.

.. class:: CacheHTTPInfoVector

   Defined in :ts:git:`iocore/cache/P_CacheHttp.h`. This represents an array or set of alternates for an object.
   Each slot contains a time ordered list of alternates, stored in a :class:`SlicedAlt`. These are
   referred to as "slices" of the alternate.

.. class:: SlicedAlt

   A set of alternates representing different temporal slices of that alternate. Each instance in a :class:`CacheHTTPInfoVector` has an
   identifier local to that vector which is used to identify the alternate.

.. class:: Slice

   A wrapper for an alternate.

.. class:: SlicedRef

   A reference to a particular slice of an alternate.

   .. member:: Slice* _slice

      A pointer to the slice for which this is a reference.

   .. member:: int _idx

      The index in the containing :class:`CacheHTTPInfoVector`.

   .. member:: int _alt_id

      The vector local identifier of the :class:`SlicedAlt` for this slice.

   .. member:: int _gen

      A generation number indicating which generation of the alternate.

   This is designed to be a robust reference in the presence of vector changes. If alternates are removed or added to the vector this reference
   should remain as long as the target alternate is not removed. If the vector slot index is needed,
   :member:`_idx` is used but verified. The :member:`_alt_id` can be checked against the alternate in that slot to check if the vector has been
   updated. In the common case where it has not, this is fast. If the vector has been updated it almost always be the case that an alternate was
   removed and this alternate shifted down the alternate index can be searched for toward the front
   of the vector. Otherwise it is searched for from the end toward the original index.

