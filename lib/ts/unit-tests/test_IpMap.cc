/** @file

    IpMap unit tests.

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

#include <ts/IpMap.h>
#include <iostream>
#include <catch.hpp>
#include <ts/BufferWriter.h>
#include "../../../tests/include/catch.hpp"

using ts::IpEndpoint;
using ts::IpAddr;

void
IpMapTestPrint(IpMap &map)
{
  std::cout << "IpMap Dump" << std::endl;

  for (auto &spot : map) {
    ts::LocalBufferWriter<256> w;

    std::cout << w.print("{::a} - {::a} : {:p}\n", spot.min(), spot.max(), spot.data());
  }
  std::cout << std::endl;
}

// --- Test helper classes ---
class MapMarkedAt : public Catch::MatcherBase<IpMap>
{
  IpEndpoint const &_addr;

public:
  MapMarkedAt(IpEndpoint const &addr) : _addr(addr) {}

  bool
  match(IpMap const &map) const override
  {
    return map.contains(&_addr);
  }

  std::string
  describe() const override
  {
    std::string zret;
    ts::bwprint(zret, "{} is marked", _addr);
    return zret;
  }
};

// The builder function
inline MapMarkedAt
IsMarkedAt(IpEndpoint const &_addr)
{
  return {_addr};
}

class MapMarkedWith : public Catch::MatcherBase<IpMap>
{
  IpEndpoint const &_addr;
  void *_mark;
  mutable bool _found_p = false;

public:
  MapMarkedWith(IpEndpoint const &addr, void *mark) : _addr(addr), _mark(mark) {}

  bool
  match(IpMap const &map) const override
  {
    void *mark = nullptr;
    return (_found_p = map.contains(&_addr, &mark)) && mark == _mark;
  }

  std::string
  describe() const override
  {
    std::string zret;
    if (_found_p) {
      bwprint(zret, "is marked at {} with {:x}", _addr, _mark);
    } else {
      bwprint(zret, "is not marked at {}", _addr);
    }
    return zret;
  }
};

inline MapMarkedWith
IsMarkedWith(IpEndpoint const &addr, void *mark)
{
  return {addr, mark};
}

// -------------
// --- TESTS ---
// -------------
TEST_CASE("IpMap Basic", "[libts][ipmap]")
{
  IpMap map;
  void *const markA = reinterpret_cast<void *>(1);
  void *const markB = reinterpret_cast<void *>(2);
  void *const markC = reinterpret_cast<void *>(3);
  void *mark; // for retrieval

  in_addr_t ip5 = htonl(5), ip9 = htonl(9);
  in_addr_t ip10 = htonl(10), ip15 = htonl(15), ip20 = htonl(20);
  in_addr_t ip50 = htonl(50), ip60 = htonl(60);
  in_addr_t ip100 = htonl(100), ip120 = htonl(120), ip140 = htonl(140);
  in_addr_t ip150 = htonl(150), ip160 = htonl(160);
  in_addr_t ip200 = htonl(200);
  in_addr_t ip0   = 0;
  in_addr_t ipmax = ~static_cast<in_addr_t>(0);

  map.mark(ip10, ip20, markA);
  map.mark(ip5, ip9, markA);
  {
    INFO("Coalesce failed");
    CHECK(map.getCount() == 1);
  }
  {
    INFO("Range max not found.");
    CHECK(map.contains(ip9));
  }
  {
    INFO("Span min not found");
    CHECK(map.contains(ip10, &mark));
  }
  {
    INFO("Mark not preserved.");
    CHECK(mark == markA);
  }

  map.fill(ip15, ip100, markB);
  {
    INFO("Fill failed.");
    CHECK(map.getCount() == 2);
  }
  {
    INFO("fill interior missing");
    CHECK(map.contains(ip50, &mark));
  }
  {
    INFO("Fill mark not preserved.");
    CHECK(mark == markB);
  }
  {
    INFO("Span min not found.");
    CHECK(!map.contains(ip200));
  }
  {
    INFO("Old span interior not found");
    CHECK(map.contains(ip15, &mark));
  }
  {
    INFO("Fill overwrote mark.");
    CHECK(mark == markA);
  }

  map.clear();
  {
    INFO("Clear failed.");
    CHECK(map.getCount() == 0);
  }

  map.mark(ip20, ip50, markA);
  map.mark(ip100, ip150, markB);
  map.fill(ip10, ip200, markC);
  CHECK(map.getCount() == 5);
  {
    INFO("Left span missing");
    CHECK(map.contains(ip15, &mark));
  }
  {
    INFO("Middle span missing");
    CHECK(map.contains(ip60, &mark));
  }
  {
    INFO("fill mark wrong.");
    CHECK(mark == markC);
  }
  {
    INFO("right span missing.");
    CHECK(map.contains(ip160));
  }
  {
    INFO("right span missing");
    CHECK(map.contains(ip120, &mark));
  }
  {
    INFO("wrong data on right mark span.");
    CHECK(mark == markB);
  }

  map.unmark(ip140, ip160);
  {
    INFO("unmark failed");
    CHECK(map.getCount() == 5);
  }
  {
    INFO("unmark left edge still there.");
    CHECK(!map.contains(ip140));
  }
  {
    INFO("unmark middle still there.");
    CHECK(!map.contains(ip150));
  }
  {
    INFO("unmark right edge still there.");
    CHECK(!map.contains(ip160));
  }

  map.clear();
  map.mark(ip20, ip20, markA);
  {
    INFO("Map failed on singleton insert");
    CHECK(map.contains(ip20));
  }
  map.mark(ip10, ip200, markB);
  mark = 0;
  map.contains(ip20, &mark);
  {
    INFO("Map held singleton against range.");
    CHECK(mark == markB);
  }
  map.mark(ip100, ip120, markA);
  map.mark(ip150, ip160, markB);
  map.mark(ip0, ipmax, markC);
  {
    INFO("IpMap: Full range fill left extra ranges.");
    CHECK(map.getCount() == 1);
  }
}

TEST_CASE("IpMap Unmark", "[libts][ipmap]")
{
  IpMap map;
  void *const markA = &map;

  IpEndpoint a_0{"0.0.0.0"sv};
  IpEndpoint a_0_0_0_16{"0.0.0.16"sv};
  IpEndpoint a_0_0_0_17{"0.0.0.17"sv};
  IpEndpoint a_max{"255.255.255.255"sv};
  IpEndpoint a_10_28_55_255{"10.28.55.255"sv};
  IpEndpoint a_10_28_56_0{"10.28.56.0"sv};
  IpEndpoint a_10_28_56_4{"10.28.56.4"sv};
  IpEndpoint a_10_28_56_255{"10.28.56.255"sv};
  IpEndpoint a_10_28_57_0{"10.28.57.0"sv};
  IpEndpoint a_63_128_1_12{"63.128.1.12"sv};
  IpEndpoint a6_0{"::"sv};
  IpEndpoint a6_max{"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"sv};
  IpEndpoint a6_fe80_9d90{"fe80::221:9bff:fe10:9d90"sv};
  IpEndpoint a6_fe80_9d9d{"fe80::221:9bff:fe10:9d9d"sv};
  IpEndpoint a6_fe80_9d95{"fe80::221:9bff:fe10:9d95"sv};
  IpEndpoint a_loopback{"127.0.0.1"sv};
  IpEndpoint a_loopback2{"127.0.0.255"sv};

  map.mark(&a_0, &a_max, markA);
  {
    INFO("IpMap Unmark: Full range not single.");
    CHECK(map.getCount() == 1);
  }
  map.unmark(&a_10_28_56_0, &a_10_28_56_255);
  {
    INFO("IpMap Unmark: Range unmark failed.");
    CHECK(map.getCount() == 2);
  }
  // Generic range check.
  {
    INFO("IpMap Unmark: Range unmark min address not removed.");
    CHECK(!map.contains(&a_10_28_56_0));
  }
  {
    INFO("IpMap Unmark: Range unmark max address not removed.");
    CHECK(!map.contains(&a_10_28_56_255));
  }
  {
    INFO("IpMap Unmark: Range unmark min-1 address removed.");
    CHECK(map.contains(&a_10_28_55_255));
  }
  {
    INFO("IpMap Unmark: Range unmark max+1 address removed.");
    CHECK(map.contains(&a_10_28_57_0));
  }
  // Test min bounded range.
  map.unmark(&a_0, &a_0_0_0_16);
  {
    INFO("IpMap Unmark: Range unmark zero address not removed.");
    CHECK(!map.contains(&a_0));
  }
  {
    INFO("IpMap Unmark: Range unmark zero bounded range max not removed.");
    CHECK(!map.contains(&a_0_0_0_16));
  }
  {
    INFO("IpMap Unmark: Range unmark zero bounded range max+1 removed.");
    CHECK(map.contains(&a_0_0_0_17));
  }
}

TEST_CASE("IpMap Fill", "[libts][ipmap]")
{
  IpMap map;
  void *const allow = reinterpret_cast<void *>(0);
  void *const deny  = reinterpret_cast<void *>(~0);
  void *const markA = reinterpret_cast<void *>(1);
  void *const markB = reinterpret_cast<void *>(2);
  void *const markC = reinterpret_cast<void *>(3);

  IpEndpoint a0{"0.0.0.0"sv};
  IpEndpoint a_max{"255.255.255.255"sv};

  IpEndpoint a_9_255_255_255{"9.255.255.255"sv};
  IpEndpoint a_10_0_0_0{"10.0.0.0"sv};
  IpEndpoint a_10_0_0_19{"10.0.0.19"sv};
  IpEndpoint a_10_0_0_255{"10.0.0.255"sv};
  IpEndpoint a_10_0_1_0{"10.0.1.0"sv};

  IpEndpoint a_10_28_55_255{"10.28.55.255"sv};
  IpEndpoint a_10_28_56_0{"10.28.56.0"sv};
  IpEndpoint a_10_28_56_4{"10.28.56.4"sv};
  IpEndpoint a_10_28_56_255{"10.28.56.255"sv};
  IpEndpoint a_10_28_57_0{"10.28.57.0"sv};

  IpEndpoint a3{"192.168.1.0"sv};
  IpEndpoint a4{"192.168.1.255"sv};

  IpEndpoint a_0000_0000{"::"sv};
  IpEndpoint a_0000_0001{"::1"sv};
  IpEndpoint a_ffff_ffff{"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"sv};
  IpEndpoint a_fe80_9d8f{"fe80::221:9bff:fe10:9d8f"sv};
  IpEndpoint a_fe80_9d90{"fe80::221:9bff:fe10:9d90"sv};
  IpEndpoint a_fe80_9d95{"fe80::221:9bff:fe10:9d95"sv};
  IpEndpoint a_fe80_9d9d{"fe80::221:9bff:fe10:9d9d"sv};
  IpEndpoint a_fe80_9d9e{"fe80::221:9bff:fe10:9d9e"sv};

  IpEndpoint a_loopback{"127.0.0.0"sv};
  IpEndpoint a_loopback2{"127.0.0.255"sv};
  IpEndpoint a_63_128_1_12{"63.128.1.12"sv};

  SECTION("subnet overfill")
  {
    map.fill(&a_10_28_56_0, &a_10_28_56_255, deny);
    map.fill(&a0, &a_max, allow);
    CHECK_THAT(map, IsMarkedWith(a_10_28_56_4, deny));
  }

  SECTION("singleton overfill")
  {
    map.fill(&a_loopback, &a_loopback, allow);
    {
      INFO("singleton not marked.");
      CHECK_THAT(map, IsMarkedAt(a_loopback));
    }
    map.fill(&a0, &a_max, deny);
    THEN("singleton mark")
    {
      CHECK_THAT(map, IsMarkedWith(a_loopback, allow));
      THEN("not empty")
      {
        REQUIRE(map.begin() != map.end());
        IpMap::iterator spot = map.begin();
        ++spot;
        THEN("more than one range")
        {
          REQUIRE(spot != map.end());
          THEN("ranges disjoint")
          {
            INFO(" " << map.begin()->max() << " < " << spot->min());
            REQUIRE(-1 == ats_ip_addr_cmp(map.begin()->max(), spot->min()));
          }
        }
      }
    }
  }

  SECTION("3")
  {
    map.fill(&a_loopback, &a_loopback2, markA);
    map.fill(&a_10_28_56_0, &a_10_28_56_255, markB);
    {
      INFO("over extended range");
      CHECK_THAT(map, !IsMarkedWith(a_63_128_1_12, markC));
    }
    map.fill(&a0, &a_max, markC);
    {
      INFO("IpMap[2]: Fill failed.");
      CHECK(map.getCount() == 5);
    }
    {
      INFO("invalid mark in range gap");
      CHECK_THAT(map, IsMarkedWith(a_63_128_1_12, markC));
    }
  }

  SECTION("4")
  {
    map.fill(&a_10_0_0_0, &a_10_0_0_255, allow);
    map.fill(&a_loopback, &a_loopback2, allow);
    {
      INFO("invalid mark between ranges");
      CHECK_THAT(map, !IsMarkedAt(a_63_128_1_12));
    }
    {
      INFO("invalid mark in lower range");
      CHECK_THAT(map, IsMarkedWith(a_10_0_0_19, allow));
    }
    map.fill(&a0, &a_max, deny);
    {
      INFO("range count incorrect");
      CHECK(map.getCount() == 5);
    }
    {
      INFO("mark between ranges");
      CHECK_THAT(map, IsMarkedWith(a_63_128_1_12, deny));
    }

    map.fill(&a_fe80_9d90, &a_fe80_9d9d, markA);
    map.fill(&a_0000_0001, &a_0000_0001, markA);
    map.fill(&a_0000_0000, &a_ffff_ffff, markB);

    {
      INFO("IpMap Fill[v6]: Zero address has bad mark.");
      CHECK_THAT(map, IsMarkedWith(a_0000_0000, markB));
    }
    {
      INFO("IpMap Fill[v6]: Max address has bad mark.");
      CHECK_THAT(map, IsMarkedWith(a_ffff_ffff, markB));
    }
    {
      INFO("IpMap Fill[v6]: 9d90 address has bad mark.");
      CHECK_THAT(map, IsMarkedWith(a_fe80_9d90, markA));
    }
    {
      INFO("IpMap Fill[v6]: 9d8f address has bad mark.");
      CHECK_THAT(map, IsMarkedWith(a_fe80_9d8f, markB));
    }
    {
      INFO("IpMap Fill[v6]: 9d9d address has bad mark.");
      CHECK_THAT(map, IsMarkedWith(a_fe80_9d9d, markA));
    }
    {
      INFO("IpMap Fill[v6]: 9d9b address has bad mark.");
      CHECK_THAT(map, IsMarkedWith(a_fe80_9d9e, markB));
    }
    {
      INFO("IpMap Fill[v6]: ::1 has bad mark.");
      CHECK_THAT(map, IsMarkedWith(a_0000_0001, markA));
    }

    {
      INFO("IpMap Fill[pre-refill]: Bad range count.");
      CHECK(map.getCount() == 10);
    }
    // These should be ignored by the map as it is completely covered for IPv6.
    map.fill(&a_fe80_9d90, &a_fe80_9d9d, markA);
    map.fill(&a_0000_0001, &a_0000_0001, markC);
    map.fill(&a_0000_0000, &a_ffff_ffff, markB);
    {
      INFO("IpMap Fill[post-refill]: Bad range count.");
      CHECK(map.getCount() == 10);
    }
  }

  SECTION("5")
  {
    map.fill(&a_fe80_9d90, &a_fe80_9d9d, markA);
    map.fill(&a_0000_0001, &a_0000_0001, markC);
    map.fill(&a_0000_0000, &a_ffff_ffff, markB);
    {
      INFO("IpMap Fill[v6-2]: Zero address has bad mark.");
      CHECK_THAT(map, IsMarkedWith(a_0000_0000, markB));
    }
    {
      INFO("IpMap Fill[v6-2]: Max address has bad mark.");
      CHECK_THAT(map, IsMarkedWith(a_ffff_ffff, markB));
    }
    {
      INFO("IpMap Fill[v6-2]: 9d90 address has bad mark.");
      CHECK_THAT(map, IsMarkedWith(a_fe80_9d90, markA));
    }
    {
      INFO("IpMap Fill[v6-2]: 9d8f address has bad mark.");
      CHECK_THAT(map, IsMarkedWith(a_fe80_9d8f, markB));
    }
    {
      INFO("IpMap Fill[v6-2]: 9d9d address has bad mark.");
      CHECK_THAT(map, IsMarkedWith(a_fe80_9d9d, markA));
    }
    {
      INFO("IpMap Fill[v6-2]: 9d9b address has bad mark.");
      CHECK_THAT(map, IsMarkedWith(a_fe80_9d9e, markB));
    }
    {
      INFO("IpMap Fill[v6-2]: ::1 has bad mark.");
      CHECK_THAT(map, IsMarkedWith(a_0000_0001, markC));
    }
  }
}

TEST_CASE("IpMap CloseIntersection", "[libts][ipmap]")
{
  IpMap map;
  void *const markA = reinterpret_cast<void *>(1);
  void *const markB = reinterpret_cast<void *>(2);
  void *const markC = reinterpret_cast<void *>(3);
  void *const markD = reinterpret_cast<void *>(4);
  // void *mark; // for retrieval

  IpEndpoint a_1_l{"123.88.172.0"sv};
  IpEndpoint a_1_m{"123.88.180.93"sv};
  IpEndpoint a_1_u{"123.88.191.255"sv};
  IpEndpoint a_2_l{"123.89.132.0"sv};
  IpEndpoint a_2_u{"123.89.135.255"sv};
  IpEndpoint a_3_l{"123.89.160.0"sv};
  IpEndpoint a_3_u{"123.89.167.255"sv};
  IpEndpoint a_4_l{"123.90.108.0"sv};
  IpEndpoint a_4_u{"123.90.111.255"sv};
  IpEndpoint a_5_l{"123.90.152.0"sv};
  IpEndpoint a_5_u{"123.90.159.255"sv};
  IpEndpoint a_6_l{"123.91.0.0"sv};
  IpEndpoint a_6_u{"123.91.35.255"sv};
  IpEndpoint a_7_l{"123.91.40.0"sv};
  IpEndpoint a_7_u{"123.91.47.255"sv};

  IpEndpoint b_1_l{"123.78.100.0"sv};
  IpEndpoint b_1_u{"123.78.115.255"sv};

  IpEndpoint c_1_l{"123.88.204.0"sv};
  IpEndpoint c_1_u{"123.88.219.255"sv};
  IpEndpoint c_2_l{"123.90.112.0"sv};
  IpEndpoint c_2_u{"123.90.119.255"sv};
  IpEndpoint c_3_l{"123.90.132.0"sv};
  IpEndpoint c_3_u{"123.90.135.255"sv};

  IpEndpoint d_1_l{"123.82.196.0"sv};
  IpEndpoint d_1_u{"123.82.199.255"sv};
  IpEndpoint d_2_l{"123.82.204.0"sv};
  IpEndpoint d_2_u{"123.82.219.255"sv};

  map.mark(a_1_l, a_1_u, markA);
  map.mark(a_2_l, a_2_u, markA);
  map.mark(a_3_l, a_3_u, markA);
  map.mark(a_4_l, a_4_u, markA);
  map.mark(a_5_l, a_5_u, markA);
  map.mark(a_6_l, a_6_u, markA);
  map.mark(a_7_l, a_7_u, markA);
  CHECK_THAT(map, IsMarkedAt(a_1_m));

  map.mark(b_1_l, b_1_u, markB);
  CHECK_THAT(map, IsMarkedAt(a_1_m));

  map.mark(c_1_l, c_1_u, markC);
  map.mark(c_2_l, c_2_u, markC);
  map.mark(c_3_l, c_3_u, markC);
  CHECK_THAT(map, IsMarkedAt(a_1_m));

  map.mark(d_1_l, d_1_u, markD);
  map.mark(d_2_l, d_2_u, markD);
  CHECK_THAT(map, IsMarkedAt(a_1_m));

  CHECK(map.getCount() == 13);
}
