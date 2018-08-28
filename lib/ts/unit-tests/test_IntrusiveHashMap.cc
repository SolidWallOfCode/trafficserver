/** @file

    IntrusiveHashMap unit tests.

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

#include <iostream>
#include <string_view>
#include <string>
#include <bitset>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <random>
#include <ts/IntrusiveHashMap.h>
#include <ts/BufferWriter.h>
#include <catch.hpp>
#include "../../../tests/include/catch.hpp"

// -------------
// --- TESTS ---
// -------------

using namespace std::literals;

namespace
{
struct Thing {
  std::string _payload;
  int _n{0};

  Thing(std::string_view text) : _payload(text) {}
  Thing(std::string_view text, int x) : _payload(text), _n(x) {}

  Thing *_next{nullptr};
  Thing *_prev{nullptr};
};

struct ThingMapDescriptor {
  static Thing *&
  next_ptr(Thing *thing)
  {
    return thing->_next;
  }
  static Thing *&
  prev_ptr(Thing *thing)
  {
    return thing->_prev;
  }
  static std::string_view
  key_of(Thing *thing)
  {
    return thing->_payload;
  }
  static constexpr std::hash<std::string_view> hasher{};
  static auto
  hash_of(std::string_view s) -> decltype(hasher(s))
  {
    return hasher(s);
  }
  static bool
  equal(std::string_view const &lhs, std::string_view const &rhs)
  {
    return lhs == rhs;
  }
};

using Map = IntrusiveHashMap<ThingMapDescriptor>;

} // namespace

TEST_CASE("IntrusiveHashMap", "[libts][IntrusiveHashMap]")
{
  Map map;
  map.insert(new Thing("bob"));
  REQUIRE(map.count() == 1);
  map.insert(new Thing("dave"));
  map.insert(new Thing("persia"));
  REQUIRE(map.count() == 3);
  for (auto &thing : map) {
    delete &thing;
  }
  map.clear();
  REQUIRE(map.count() == 0);

  size_t nb = map.bucket_count();
  std::bitset<64> marks;
  for (int i = 1; i <= 63; ++i) {
    std::string name;
    ts::bwprint(name, "{} squared is {}", i, i * i);
    Thing *thing = new Thing(name);
    thing->_n    = i;
    map.insert(thing);
    REQUIRE(map.count() == i);
    REQUIRE(map.find(name) != map.end());
  }
  REQUIRE(map.count() == 63);
  REQUIRE(map.bucket_count() > nb);
  for (auto &thing : map) {
    REQUIRE(0 == marks[thing._n]);
    marks[thing._n] = 1;
  }
  marks[0] = 1;
  REQUIRE(marks.all());
  map.insert(new Thing("dup"sv, 79));
  map.insert(new Thing("dup"sv, 80));
  map.insert(new Thing("dup"sv, 81));

  auto r = map.equal_range("dup"sv);
  REQUIRE(r.first != r.second);
  REQUIRE(r.first->_payload == "dup"sv);

  Map::iterator idx;

  // Erase all the non-"dup" and see if the range is still correct.
  map.apply([&map](Thing &thing) {
    if (thing._payload != "dup"sv)
      map.erase(map.iterator_for(&thing));
  });
  r = map.equal_range("dup"sv);
  REQUIRE(r.first != r.second);
  idx = r.first;
  REQUIRE(idx->_payload == "dup"sv);
  REQUIRE((++idx)->_payload == "dup"sv);
  REQUIRE(idx->_n != r.first->_n);
  REQUIRE((++idx)->_payload == "dup"sv);
  REQUIRE(idx->_n != r.first->_n);
  REQUIRE(++idx == map.end());
  // Verify only the "dup" items are left.
  for (auto &&elt : map) {
    REQUIRE(elt._payload == "dup"sv);
  }
  // clean up the last bits.
  map.apply([](Thing *thing) { delete thing; });
};

// Normally there's no point in running the performance tests, but it's worth keeping the code
// for when additional testing needs to be done.
#if 0

struct Thing2 {
  std::string _payload;
  int _n{0};

  Thing2(std::string_view text) : _payload(text) {}
  Thing2(std::string_view text, int x) : _payload(text), _n(x) {}

  Thing2 * _next_name {nullptr};
  Thing2 * _prev_name {nullptr};

  struct LinkByName {
    static Thing2*&
    next_ptr(Thing2 *thing)
    {
      return thing->_next_name;
    }
    static Thing2 *&
    prev_ptr(Thing2 *thing)
    {
      return thing->_prev_name;
    }
    static std::string_view
    key_of(Thing2 *thing)
    {
      return thing->_payload;
    }
    static constexpr std::hash<std::string_view> hasher{};
    static auto
    hash_of(std::string_view s) -> decltype(hasher(s))
    {
      return hasher(s);
    }
    static bool
    equal(std::string_view const &lhs, std::string_view const &rhs)
    {
      return lhs == rhs;
    }
  };

  Thing2 *_next_n{nullptr};
  Thing2 *_prev_n{nullptr};

  struct LinkByN {
    static Thing2*&
    next_ptr(Thing2 *thing)
    {
      return thing->_next_n;
    }
    static Thing2 *&
    prev_ptr(Thing2 *thing)
    {
      return thing->_prev_n;
    }
    static int
    key_of(Thing2 *thing)
    {
      return thing->_n;
    }
    static int
    hash_of(int n)
    {
      return n;
    }
    static bool
    equal(int lhs, int rhs)
    {
      return lhs == rhs;
    }
  };
};

TEST_CASE("IntrusiveHashMapPerf", "[IntrusiveHashMap][performance]")
{
  // Force these so I can easily change the set of tests.
  auto start            = std::chrono::high_resolution_clock::now();
  auto delta = std::chrono::high_resolution_clock::now() - start;
  constexpr int N_LOOPS = 1000;
  constexpr int N = 1009; // prime > N_LOOPS

  std::vector<const char *> strings;

  std::uniform_int_distribution<short> char_gen {'a', 'z'};
  std::uniform_int_distribution<short> length_gen { 20, 40 };
  std::minstd_rand randu;

  Map ihm;
  std::unordered_map<std::string, Thing> um;

  strings.reserve(N);
  for ( int i = 0 ; i < N ; ++i ) {
    auto len = length_gen(randu);
    char *s = static_cast<char *>(malloc(len + 1));
    for (decltype(len) j = 0; j < len; ++j) {
      s[j] = char_gen(randu);
    }
    s[len] = 0;
    strings.push_back(s);
  }

  // Fill the IntrusiveHashMap
  start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < N ; ++i) {
    auto thing = new Thing{strings[i], i};
    ihm.insert(thing);
  }
  delta = std::chrono::high_resolution_clock::now() - start;
  std::cout << "IHM populate " << delta.count() << "ns or " << std::chrono::duration_cast<std::chrono::milliseconds>(delta).count()
            << "ms" << std::endl;

  // Do some lookups
  start = std::chrono::high_resolution_clock::now();
  for ( int i = 0 ; i < N_LOOPS ; ++i ) {
    for (int j = 0, idx = i; j < N; ++j, idx = (idx + i) % N) {
      ihm.find(strings[idx]);
    }
  }
  delta = std::chrono::high_resolution_clock::now() - start;
  std::cout << "IHM find " << delta.count() << "ns or " << std::chrono::duration_cast<std::chrono::milliseconds>(delta).count()
            << "ms" << std::endl;

  // Fill the std::unordered_map
  start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < N ; ++i) {
    um.emplace(strings[i], Thing{strings[i], i});
  }
  delta = std::chrono::high_resolution_clock::now() - start;
  std::cout << "UM populate " << delta.count() << "ns or " << std::chrono::duration_cast<std::chrono::milliseconds>(delta).count()
            << "ms" << std::endl;

  // Do some lookups
  start = std::chrono::high_resolution_clock::now();
  for ( int i = 0 ; i < N_LOOPS ; ++i ) {
    for (int j = 0, idx = i; j < N; ++j, idx = (idx + i) % N) {
      um.find(strings[idx]);
    }
  }
  delta = std::chrono::high_resolution_clock::now() - start;
  std::cout << "UM find " << delta.count() << "ns or " << std::chrono::duration_cast<std::chrono::milliseconds>(delta).count()
            << "ms" << std::endl;

  // Test double indexing

  std::vector<Thing2> things;
  things.reserve(N);
  for ( int i = 0 ; i < N ; ++i ) {
    things.emplace_back(strings[i], i);
  }

  std::unordered_map<std::string, Thing2*> um_by_name;
  std::unordered_map<int, Thing2*> um_by_n;

  IntrusiveHashMap<Thing2::LinkByName> ihm_by_name;
  IntrusiveHashMap<Thing2::LinkByN> ihm_by_n;

  // Fill the IntrusiveHashMap
  start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < N ; ++i) {
    ihm_by_name.insert(&things[i]);
    ihm_by_n.insert(&things[i]);
  }
  delta = std::chrono::high_resolution_clock::now() - start;
  std::cout << "IHM2 populate " << delta.count() << "ns or " << std::chrono::duration_cast<std::chrono::milliseconds>(delta).count()
            << "ms" << std::endl;

  // Do some lookups
  start = std::chrono::high_resolution_clock::now();
  for ( int i = 0 ; i < N_LOOPS ; ++i ) {
    for (int j = 0, idx = i; j < N; ++j, idx = (idx + i) % N) {
      Thing2 * thing = ihm_by_n.find(idx);
      ihm_by_name.iterator_for(thing);
    }
  }
  delta = std::chrono::high_resolution_clock::now() - start;
  std::cout << "IHM2 find " << delta.count() << "ns or " << std::chrono::duration_cast<std::chrono::milliseconds>(delta).count()
            << "ms" << std::endl;

  // Fill the std::unordered_map
  start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < N ; ++i) {
    um_by_n.emplace(things[i]._n, &things[i]);
    um_by_name.emplace(things[i]._payload, &things[i]);
  }
  delta = std::chrono::high_resolution_clock::now() - start;
  std::cout << "UM2 populate " << delta.count() << "ns or " << std::chrono::duration_cast<std::chrono::milliseconds>(delta).count()
            << "ms" << std::endl;

  // Do some lookups
  start = std::chrono::high_resolution_clock::now();
  for ( int i = 0 ; i < N_LOOPS ; ++i ) {
    for (int j = 0, idx = i; j < N; ++j, idx = (idx + i) % N) {
      um_by_name.find(strings[idx]);
      um_by_n.find(idx);
    }
  }
  delta = std::chrono::high_resolution_clock::now() - start;
  std::cout << "UM2 find " << delta.count() << "ns or " << std::chrono::duration_cast<std::chrono::milliseconds>(delta).count()
            << "ms" << std::endl;


};
#endif
