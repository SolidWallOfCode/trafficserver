/** @file

  Numeric utility classes.

  This contains helper / template classes to provide common numeric capabilities for other classes.

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

#include <cstdint>
#include <ratio>
#include <limits>
#include <type_traits>

namespace ts
{
/** An interval in a completely ordered discrete set.
 *
 * This represents an inclusive interval from a minmimum to a maximum. The metric, @a I, is required
 * to be discrete and completely ordered. This will generally be some integral type. The metric must
 * provide correct values via @c std::numeric_limits.
 *
 * For more efficient parameters the metric can be specified as a reference - this will be removed
 * for storage purposes but kept for parameter types. The internal type @c Metric is use for
 * storage.
 *
 * @internal Inclusive intervals because the interval can contain all valid values for the @c Metric.
 *
 * @tparam I The metric.
 */
template <typename I> class DiscreteInterval
{
  using self_type = DiscreteInterval; ///< Self reference type.

public:
  using Metric          = typename std::remove_reference<I>::type; ///< Export for client access.
  static const auto MIN = std::numeric_limits<I>::min();           ///< Minimum metric value.
  static const auto MAX = std::numeric_limits<I>::max();           ///< Maxium metric value.

  /// Default constructor - invalid (empty) range.
  DiscreteInterval();

  /// Construct a range of @a min to @a max.
  /// @note The values are not checked for @a min < @a max
  DiscreteInterval(const I min, const I max);

  /// Set the interval bounds explicitly.
  self_type &assign(const I min, const I max);

  /// Check if there the interval is empty (contains no values).
  bool empty() const;

  /// Check if @a v is a member of the interval.
  bool contains(const I v) const;

protected:
  Metric _min{MAX}; ///< Minimum value in range.
  Metric _max{MIN}; ///< Maximum value in range.
};

template <typename I> DiscreteInterval<I>::DiscreteInterval() {}

template <typename I> DiscreteInterval<I>::DiscreteInterval(const I min, const I max) : _min(min), _max(max) {}

template <typename I>
DiscreteInterval<I> &
DiscreteInterval<I>::assign(const I min, const I max)
{
  _min = min;
  _max = max;
  return *this;
}

template <typename I>
bool
DiscreteInterval<I>::empty() const
{
  return _min > _max;
}

template <typename I>
bool
DiscreteInterval<I>::contains(const I v) const
{
  return _min <= v && v <= _max;
}
} // namespace ts
