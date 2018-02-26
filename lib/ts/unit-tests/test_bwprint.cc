/** @file

    Unit tests for BufferFormat and bwprint.

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

#include "catch.hpp"
#include <ts/bwprint.h>

TEST_CASE("bwprint basics", "[bwprint]")
{
  ts::LocalBufferWriter<256> bw;
  auto fmt1{"Some text"_sv};

  bwprint(bw, fmt1);
  REQUIRE(bw.view() == fmt1);
  bw.reduce(0);
  bwprint(bw, "Arg {}", 1);
  REQUIRE(bw.view() == "Arg 1");
  bw.reduce(0);
  bwprint(bw, "arg 1 {1} and 2 {2} and 0 {0}", "zero", "one", "two");
  REQUIRE(bw.view() == "arg 1 one and 2 two and 0 zero");
  bw.reduce(0);
  bwprint(bw, "args {2}{0}{1}", "zero", "one", "two");
  REQUIRE(bw.view() == "args twozeroone");
  bw.reduce(0);
  bwprint(bw, "left |{:<10}|", "text");
  REQUIRE(bw.view() == "left |text      |");
  bw.reduce(0);
  bwprint(bw, "right |{:>10}|", "text");
  REQUIRE(bw.view() == "right |      text|");
  bw.reduce(0);
  bwprint(bw, "right |{:.>10}|", "text");
  REQUIRE(bw.view() == "right |......text|");
  bw.reduce(0);
  bwprint(bw, "center |{:.=10}|", "text");
  REQUIRE(bw.view() == "center |...text...|");
  bw.reduce(0);
  bwprint(bw, "center |{:.=11}|", "text");
  REQUIRE(bw.view() == "center |...text....|");
  bw.reduce(0);
  bwprint(bw, "center |{:==10}|", "text");
  REQUIRE(bw.view() == "center |===text===|");
  bw.reduce(0);
  bwprint(bw, "center |{:%3A=10}|", "text");
  REQUIRE(bw.view() == "center |:::text:::|");
  bw.reduce(0);
  bwprint(bw, "left >{0:<9}< right >{0:>9}< center >{0:=9}<", 956);
  REQUIRE(bw.view() == "left >956      < right >      956< center >   956   <");
  bw.reduce(0);
  bwprint(bw, "Time is {now}");
  REQUIRE(bw.view() == "Time is");
}
