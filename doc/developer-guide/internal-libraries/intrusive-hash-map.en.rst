.. Licensed to the Apache Software Foundation (ASF) under one
   or more contributor license agreements. See the NOTICE file distributed with this work for
   additional information regarding copyright ownership. The ASF licenses this file to you under the
   Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with
   the License. You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software distributed under the License
   is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
   or implied. See the License for the specific language governing permissions and limitations under
   the License.

.. include:: ../../common.defs

.. _lib-intrusive-hash-map:
.. highlight:: cpp
.. default-domain:: cpp

IntrusiveHashMap
****************

:class:`IntrusiveHashMap` provides a hash map, or unordered map, using intrusive links.

Definition
**********

.. class:: template < typename H > IntrusiveHashMap

   :tparam H: Hash types and operations.

   An unordered map using a hash function. The properties of the map are determined by types and
   operations provided by the descriptor type :arg:`H`.

   .. type:: value_type

      The type of elements in the container, deduced from the return types of the link accessor methods
      in :arg:`L`.

   .. type:: key_type

      The type of the key used for hash computations. Deduced from the return type of the key accessor. An instance
      of this type is never default constructed nor modified, therefore it can be a reference if the key type is
      expensive to copy.

   .. type:: hash_id

      The type of the hash of a :type:`key_type`. Deduced from the return type of the hash function. This must be a
      numeric type.

   :arg:`H`
      This describes the hash map, primarily via the operations required for the map. The related types are deduced
      from the function return types. This is designed to be compatible with :class:`IntrusiveDList`.

      .. function:: static key_type key_of(value_type *elt)

         Key accessor - return the key of the element :arg:`elt`.

      .. function:: static hash_id hash_of(key_type key)

         Hash function - compute the hash value of the :arg:`key`.

      .. function:: static bool equal(key_type lhs, key_type rhs)

         Key comparison - two keys are equal if this function returns :code:`true`.

      .. function:: static IntrusiveHashMap::value_type*& next_ptr(IntrusiveHashMap::value_type* elt)

         Return a reference to the next element pointer embedded in the element :arg:`elt`.

      .. function:: static IntrusiveHashMap::value_type*& prev_ptr(IntrusiveHashMap::value_type* elt)

         Return a reference to the previous element pointer embedded in the element :arg:`elt`.


Usage
*****


Examples
========

Design Notes
************

This is a refresh of an previously existing class, :code:`TSHahTable`. The switch to C++ 11 and then C++ 17 made it
possible to do much better in terms of the internal implementation and API. The overall functionality is the roughly
the same but with an easier API, compatiblity with :class:`IntrusiveDList`, and better internal implementation.

The biggest change is that elements are stored in a single global list rather than per hash bucket. The buckets
server only as entry points in to the global list and to count the number of elements per bucket. This simplifies the
implementation of iteration, so that the old :code:`Location` nested class can be removed. Elements with equal keys
can be handled in the same way as with STL containers, via iterator ranges, instead of a custom psuedo-iterator class.
