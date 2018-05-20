/** @file

  Calculate some derivative metrics (for convenience).

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

#pragma once

#include <tuple>
#include <string_view>
#include "I_RecLocal.h"

// ToDo: It's a little bizarre that we include this here, but it's the only way to get to RecSetRecord(). We should
// move that elsewhere... But other places in our core does the same thing.
#include "P_RecCore.h"

using DerivativeSum = std::tuple<std::string_view, RecDataT, std::vector<std::string_view>>;

class DerivativeMetrics
{
public:
  DerivativeMetrics();
  void Update();

  // Don't allow copy and assign
  DerivativeMetrics(DerivativeMetrics &) = delete;
  DerivativeMetrics &operator=(DerivativeMetrics &) = delete;
};
