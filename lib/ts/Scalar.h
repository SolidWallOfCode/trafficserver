/** @file

  Scaled integral values.

  In many situations it is desirable to define scaling factors or base units (a "metric"). This template
  enables this to be done in a type and scaling safe manner where the defined factors carry their scaling
  information as part of the type.

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

#if !defined(TS_SCALAR_H)
#define TS_SCALAR_H

#include <cstdint>
#include <ratio>
#include <ostream>
#include <type_traits>

namespace tag
{
struct generic;
}

namespace ApacheTrafficServer
{
template <intmax_t N, typename C, typename T> class Scalar;

namespace detail
{
  // @internal - althought these conversion methods look bulky, in practice they compile down to
  // very small amounts of code due to dead code elimination and that all of the conditions are
  // compile time constants.

  // The general case where neither N nor S are a multiple of the other seems a bit long but this
  // minimizes the risk of integer overflow.  I need to validate that under -O2 the compiler will
  // only do 1 division to get both the quotient and remainder for (n/N) and (n%N). In cases where
  // N,S are powers of 2 I have verified recent GNU compilers will optimize to bit operations.

  /// Convert a count @a c that is scale @s S to scale @c N
  template <intmax_t N, intmax_t S>
  intmax_t
  scale_conversion_round_up(intmax_t c)
  {
    typedef std::ratio<N, S> R;
    if (N == S) {
      return c;
    } else if (R::den == 1) {
      return c / R::num + (0 != c % R::num); // N is a multiple of S.
    } else if (R::num == 1) {
      return c * R::den; // S is a multiple of N.
    } else {
      return (c / R::num) * R::den + ((c % R::num) * R::den) / R::num + (0 != (c % R::num));
    }
  }

  /// Convert a count @a c that is scale @s S to scale @c N
  template <intmax_t N, intmax_t S>
  intmax_t
  scale_conversion_round_down(intmax_t c)
  {
    typedef std::ratio<N, S> R;
    if (N == S) {
      return c;
    } else if (R::den == 1) {
      return c / R::num; // N = k S
    } else if (R::num == 1) {
      return c * R::den; // S = k N
    } else {
      return (c / R::num) * R::den + ((c % R::num) * R::den) / R::num;
    }
  }

  /* Helper classes for @c Scalar

     These wrap values to capture extra information for @c Scalar methods. This includes whether to
     round up or down when converting and, when the wrapped data is also a @c Scalar, the scale.

     These are not intended for direct use but by the @c round_up and @c round_down free functions
     which capture the information about the argument and construct an instance of one of these
     classes to pass it on to a @c Scalar method.

     Scale conversions between @c Scalar instances are handled in these classes via the templated
     methods @c scale_conversion_round_up and @c scale_conversion_round_down.

     Conversions between scales and types for the scalar helpers is done inside the helper classes
     and a user type conversion operator exists so the helper can be converted by the compiler to
     the correct type. For the untis bases conversion this is done in @c Scalar because the
     generality of the needed conversion is too broad to be easily used. It can be done but there is
     some ugliness due to the fact that in some cases two user conversions which is difficult to
     deal with. I have tried it both ways and overall this seems a cleaner implementation.

     Much of this is driven by the fact that the assignment operator, in some case, can not be
     templated and therefore to have a nice interace for assignment this split is needed.
   */

  // Unit value, to be rounded up.
  template <typename C> struct scalar_unit_round_up_t {
    C _n;
    //    template <typename I> constexpr operator scalar_unit_round_up_t<I>() { return {static_cast<I>(_n)}; }
    template <intmax_t N, typename I>
    constexpr I
    scale() const
    {
      return static_cast<I>(_n / N + (0 != (_n % N)));
    }
  };
  // Unit value, to be rounded down.
  template <typename C> struct scalar_unit_round_down_t {
    C _n;
    //    template <typename I> operator scalar_unit_round_down_t<I>() { return {static_cast<I>(_n)}; }
    template <intmax_t N, typename I>
    constexpr I
    scale() const
    {
      return static_cast<I>(_n / N);
    }
  };
  // Scalar value, to be rounded up.
  template <intmax_t N, typename C, typename T> struct scalar_round_up_t {
    C _n;
    template <intmax_t S, typename I> constexpr operator Scalar<S, I, T>() const
    {
      return Scalar<S, I, T>(scale_conversion_round_up<S, N>(_n));
    }
  };
  // Scalar value, to be rounded down.
  template <intmax_t N, typename C, typename T> struct scalar_round_down_t {
    C _n;
    template <intmax_t S, typename I> constexpr operator Scalar<S, I, T>() const
    {
      return Scalar<S, I, T>(scale_conversion_round_down<S, N>(_n));
    }
  };
}

/// Mark a unit value to be scaled, rounding down.
template <typename C>
constexpr detail::scalar_unit_round_up_t<C>
round_up(C n)
{
  return {n};
}
/// Mark a @c Scalar value to be scaled, rounding up.
template <intmax_t N, typename C, typename T>
constexpr detail::scalar_round_up_t<N, C, T>
round_up(Scalar<N, C, T> v)
{
  return {v.count()};
}
/// Mark a unit value to be scaled, rounding down.
template <typename C>
constexpr detail::scalar_unit_round_down_t<C>
round_down(C n)
{
  return {n};
}
/// Mark a @c Scalar value, to be rounded down.
template <intmax_t N, typename C, typename T>
constexpr detail::scalar_round_down_t<N, C, T>
round_down(Scalar<N, C, T> v)
{
  return {v.count()};
}

/** A class to hold scaled values.

    Instances of this class have a @a count and a @a scale. The "value" of the instance is @a
    count * @a scale.  The scale is stored in the compiler in the class symbol table and so only
    the count is a run time value. An instance with a large scale can be assign to an instance
    with a smaller scale and the conversion is done automatically. Conversions from a smaller to
    larger scale must be explicit using @c round_up and @c round_down. This prevents
    inadvertent changes in value. Because the scales are not the same these conversions can be
    lossy and the two conversions determine whether, in such a case, the result should be rounded
    up or down to the nearest scale value.

    @a N sets the scale. @a C is the type used to hold the count, which is in units of @a N.

    @a T is a "tag" type which is used only to distinguish the base metric for the scale. Scalar
    types that have different tags are not interoperable although they can be converted manually by
    converting to units and then explicitly constructing a new Scalar instance. This is by
    design. This can be ignored - if not specified then it defaults to a "generic" tag. The type can
    be (and usually is) defined in name only).

    @note This is modeled somewhat on @c std::chrono and serves a similar function for different
    and simpler cases (where the ratio is always an integer, never a fraction).

    @see round_up
    @see round_down
 */
template <intmax_t N, typename C = int, typename T = tag::generic> class Scalar
{
  typedef Scalar self; ///< Self reference type.

public:
  /// Scaling factor - make it external accessible.
  constexpr static intmax_t SCALE = N;
  typedef C Count; ///< Type used to hold the count.
  typedef T Tag;   ///< Make tag accessible.

  static_assert(N > 0, "The scaling factor (1st template argument) must be a positive integer");
  static_assert(std::is_integral<C>::value, "The counter type (2nd template argument) must be an integral type");

  constexpr Scalar(); ///< Default contructor.
  ///< Construct to have @a n scaled units.
  explicit constexpr Scalar(Count n);
  /// Copy constructor.
  constexpr Scalar(self const &that); /// Copy constructor.
  /// Copy constructor for same scale.
  template <typename I> constexpr Scalar(Scalar<N, I, T> const &that);
  /// Direct conversion constructor.
  /// @note Requires that @c S be an integer multiple of @c SCALE.
  template <intmax_t S, typename I> constexpr Scalar(Scalar<S, I, T> const &that);
  /// Conversion constructor.
  constexpr Scalar(detail::scalar_round_up_t<N, C, T> const &that);
  /// Conversion constructor.
  constexpr Scalar(detail::scalar_round_down_t<N, C, T> const &that);
  /// Conversion constructor.
  template <typename I> constexpr Scalar(detail::scalar_unit_round_up_t<I> v);
  /// Conversion constructor.
  template <typename I> constexpr Scalar(detail::scalar_unit_round_down_t<I> v);

  /// Assignment operator.
  /// The value @a that is scaled appropriately.
  /// @note Requires the scale of @a that be an integer multiple of the scale of @a this. If this isn't the case then
  /// the @c round_up or @c round_down must be used to indicate the rounding direction.
  template <intmax_t S, typename I> self &operator=(Scalar<S, I, T> const &that);
  /// Assignment from same scale.
  self &operator=(self const &that);
  // Conversion assignments.
  template <typename I> self &operator=(detail::scalar_unit_round_up_t<I> n);
  template <typename I> self &operator=(detail::scalar_unit_round_down_t<I> n);
  self &operator                      =(detail::scalar_round_up_t<N, C, T> v);
  self &operator                      =(detail::scalar_round_down_t<N, C, T> v);

  /// Direct assignment.
  /// The count is set to @a n.
  self &assign(Count n);
  /// The value @a that is scaled appropriately.
  /// @note Requires the scale of @a that be an integer multiple of the scale of @a this. If this isn't the case then
  /// the @c round_up or @c round_down must be used to indicate the rounding direction.
  template <intmax_t S, typename I> self &assign(Scalar<S, I, T> const &that);
  // Conversion assignments.
  template <typename I> self &assign(detail::scalar_unit_round_up_t<I> n);
  template <typename I> self &assign(detail::scalar_unit_round_down_t<I> n);
  self &assign(detail::scalar_round_up_t<N, C, T> v);
  self &assign(detail::scalar_round_down_t<N, C, T> v);

  /// The number of scale units.
  constexpr Count count() const;
  /// The scaled value.
  constexpr Count units() const;
  /// User conversion to scaled value.
  constexpr operator Count() const;

  /// Addition operator.
  /// The value is scaled from @a that to @a this.
  /// @note Requires the scale of @a that be an integer multiple of the scale of @a this. If this isn't the case then
  /// the @c scale_up or @c scale_down casts must be used to indicate the rounding direction.
  template <intmax_t S, typename I> self &operator+=(Scalar<S, I, T> const &that);
  /// Addition - add @a n as a number of scaled units.
  self &operator+=(C n);
  /// Addition - add @a n as a number of scaled units.
  self &operator+=(self const &that);
  template <typename I> self &operator+=(detail::scalar_unit_round_up_t<I> n);
  template <typename I> self &operator+=(detail::scalar_unit_round_down_t<I> n);
  self &operator+=(detail::scalar_round_up_t<N, C, T> v);
  self &operator+=(detail::scalar_round_down_t<N, C, T> v);

  /// Increment - increase count by 1.
  self &operator++();
  /// Increment - increase count by 1.
  self operator++(int);
  /// Decrement - decrease count by 1.
  self &operator--();
  /// Decrement - decrease count by 1.
  self operator--(int);

  /// Subtraction operator.
  /// The value is scaled from @a that to @a this.
  /// @note Requires the scale of @a that be an integer multiple of the scale of @a this. If this isn't the case then
  /// the @c scale_up or @c scale_down casts must be used to indicate the rounding direction.
  template <intmax_t S, typename I> self &operator-=(Scalar<S, I, T> const &that);
  /// Subtraction - subtract @a n as a number of scaled units.
  self &operator-=(C n);
  /// Subtraction - subtract @a n as a number of scaled units.
  self &operator-=(self const &that);

  template <typename I> self &operator-=(detail::scalar_unit_round_up_t<I> n);
  template <typename I> self &operator-=(detail::scalar_unit_round_down_t<I> n);
  self &operator-=(detail::scalar_round_up_t<N, C, T> v);
  self &operator-=(detail::scalar_round_down_t<N, C, T> v);

  /// Multiplication - multiple the count by @a n.
  self &operator*=(C n);

  /// Division - divide (rounding down) the count by @a n.
  self &operator/=(C n);

  /// Run time access to the scale (template arg @a N).
  static constexpr intmax_t scale();

protected:
  Count _n; ///< Number of scale units.
};

template <intmax_t N, typename C, typename T> constexpr Scalar<N, C, T>::Scalar() : _n()
{
}
template <intmax_t N, typename C, typename T> constexpr Scalar<N, C, T>::Scalar(Count n) : _n(n)
{
}
template <intmax_t N, typename C, typename T> constexpr Scalar<N, C, T>::Scalar(self const &that) : _n(that._n)
{
}
template <intmax_t N, typename C, typename T>
template <typename I>
constexpr Scalar<N, C, T>::Scalar(Scalar<N, I, T> const &that) : _n(static_cast<C>(that.count()))
{
}
template <intmax_t N, typename C, typename T>
template <intmax_t S, typename I>
constexpr Scalar<N, C, T>::Scalar(Scalar<S, I, T> const &that) : _n(std::ratio<S, N>::num * that.count())
{
  static_assert(std::ratio<S, N>::den == 1,
                "Construction not permitted - target scale is not an integral multiple of source scale.");
}
template <intmax_t N, typename C, typename T>
constexpr Scalar<N, C, T>::Scalar(detail::scalar_round_up_t<N, C, T> const &v) : _n(v._n)
{
}
template <intmax_t N, typename C, typename T>
constexpr Scalar<N, C, T>::Scalar(detail::scalar_round_down_t<N, C, T> const &v) : _n(v._n)
{
}
template <intmax_t N, typename C, typename T>
template <typename I>
constexpr Scalar<N, C, T>::Scalar(detail::scalar_unit_round_up_t<I> v) : _n(v.template scale<N, C>())
{
}
template <intmax_t N, typename C, typename T>
template <typename I>
constexpr Scalar<N, C, T>::Scalar(detail::scalar_unit_round_down_t<I> v) : _n(v.template scale<N, C>())
{
}

template <intmax_t N, typename C, typename T>
constexpr auto
Scalar<N, C, T>::count() const -> Count
{
  return _n;
}
template <intmax_t N, typename C, typename T>
constexpr auto
Scalar<N, C, T>::units() const -> Count
{
  return _n * SCALE;
}
template <intmax_t N, typename C, typename T> constexpr Scalar<N, C, T>::operator C() const
{
  return _n * SCALE;
}

template <intmax_t N, typename C, typename T>
inline auto
Scalar<N, C, T>::assign(Count n) -> self &
{
  _n = n;
  return *this;
}
template <intmax_t N, typename C, typename T>
inline auto
Scalar<N, C, T>::operator=(self const &that) -> self &
{
  _n = that._n;
  return *this;
}
template <intmax_t N, typename C, typename T>
inline auto
Scalar<N, C, T>::operator=(detail::scalar_round_up_t<N, C, T> v) -> self &
{
  _n = v._n;
  return *this;
}
template <intmax_t N, typename C, typename T>
inline auto
Scalar<N, C, T>::assign(detail::scalar_round_up_t<N, C, T> v) -> self &
{
  _n = v._n;
  return *this;
}
template <intmax_t N, typename C, typename T>
inline auto
Scalar<N, C, T>::operator=(detail::scalar_round_down_t<N, C, T> v) -> self &
{
  _n = v._n;
  return *this;
}
template <intmax_t N, typename C, typename T>
inline auto
Scalar<N, C, T>::assign(detail::scalar_round_down_t<N, C, T> v) -> self &
{
  _n = v._n;
  return *this;
}
template <intmax_t N, typename C, typename T>
template <typename I>
inline auto
Scalar<N, C, T>::operator=(detail::scalar_unit_round_up_t<I> v) -> self &
{
  _n = v.template scale<N, C>();
  return *this;
}
template <intmax_t N, typename C, typename T>
template <typename I>
inline auto
Scalar<N, C, T>::assign(detail::scalar_unit_round_up_t<I> v) -> self &
{
  _n = v.template scale<N, C>();
  return *this;
}
template <intmax_t N, typename C, typename T>
template <typename I>
inline auto
Scalar<N, C, T>::operator=(detail::scalar_unit_round_down_t<I> v) -> self &
{
  _n = v.template scale<N, C>();
  return *this;
}
template <intmax_t N, typename C, typename T>
template <typename I>
inline auto
Scalar<N, C, T>::assign(detail::scalar_unit_round_down_t<I> v) -> self &
{
  _n = v.template scale<N, C>();
  return *this;
}
template <intmax_t N, typename C, typename T>
template <intmax_t S, typename I>
auto
Scalar<N, C, T>::operator=(Scalar<S, I, T> const &that) -> self &
{
  typedef std::ratio<S, N> R;
  static_assert(R::den == 1, "Assignment not permitted - target scale is not an integral multiple of source scale.");
  _n = that.count() * R::num;
  return *this;
}
template <intmax_t N, typename C, typename T>
template <intmax_t S, typename I>
auto
Scalar<N, C, T>::assign(Scalar<S, I, T> const &that) -> self &
{
  typedef std::ratio<S, N> R;
  static_assert(R::den == 1, "Assignment not permitted - target scale is not an integral multiple of source scale.");
  _n = that.count() * R::num;
  return *this;
}

template <intmax_t N, typename C, typename T>
constexpr inline intmax_t
Scalar<N, C, T>::scale()
{
  return SCALE;
}

// --- Compare operators
// These optimize nicely due to dead code elimination.

template <intmax_t N, typename C1, intmax_t S, typename I, typename T>
bool
operator<(Scalar<N, C1, T> const &lhs, Scalar<S, I, T> const &rhs)
{
  typedef std::ratio<N, S> R;
  if (N == S)
    return lhs.count() < rhs.count();
  else if (R::den == 1)
    return lhs.count() * R::num < rhs.count();
  else if (R::num == 1)
    return lhs.count() < rhs.count() * R::den;
  else
    return lhs.units() < rhs.units();
}

template <intmax_t N, typename C1, intmax_t S, typename I, typename T>
bool
operator==(Scalar<N, C1, T> const &lhs, Scalar<S, I, T> const &rhs)
{
  typedef std::ratio<N, S> R;
  if (N == S)
    return lhs.count() == rhs.count();
  else if (R::den == 1)
    return lhs.count() * R::num == rhs.count();
  else if (R::num == 1)
    return lhs.count() == rhs.count() * R::den;
  else
    return lhs.units() == rhs.units();
}

template <intmax_t N, typename C1, intmax_t S, typename I, typename T>
bool
operator<=(Scalar<N, C1, T> const &lhs, Scalar<S, I, T> const &rhs)
{
  typedef std::ratio<N, S> R;
  if (N == S)
    return lhs.count() <= rhs.count();
  else if (R::den == 1)
    return lhs.count() * R::num <= rhs.count();
  else if (R::num == 1)
    return lhs.count() <= rhs.count() * R::den;
  else
    return lhs.units() <= rhs.units();
}

// Derived compares.

template <intmax_t N, typename C1, intmax_t S, typename I, typename T>
bool
operator>(Scalar<N, C1, T> const &lhs, Scalar<S, I, T> const &rhs)
{
  return rhs < lhs;
}

template <intmax_t N, typename C1, intmax_t S, typename I, typename T>
bool
operator>=(Scalar<N, C1, T> const &lhs, Scalar<S, I, T> const &rhs)
{
  return rhs <= lhs;
}

# if 0
// Do the integer compares.
// A bit ugly to handle the issue that integers without explicit type are <int>. Therefore suppport
// must be provided for comparison not just to the counter type C but also explicitly <int>, otherwise
// function template argument deduction may fail (because it can't figure out what to use for <C>).

template <intmax_t N, typename C, typename T>
bool
operator<(Scalar<N, C, T> const &lhs, C n)
{
  return lhs.units() < n;
}
template <intmax_t N, typename C, typename T>
bool
operator<(C n, Scalar<N, C, T> const &rhs)
{
  return n < rhs.units();
}
template <intmax_t N, typename C, typename T>
bool
operator<(Scalar<N, C, T> const &lhs, int n)
{
  return lhs.units() < static_cast<C>(n);
}
template <intmax_t N, typename C, typename T>
bool
operator<(int n, Scalar<N, C, T> const &rhs)
{
  return static_cast<C>(n) < rhs.units();
}
template <intmax_t N>
bool
operator<(Scalar<N, int> const &lhs, int n)
{
  return lhs.units() < n;
}
template <intmax_t N>
bool
operator<(int n, Scalar<N, int> const &rhs)
{
  return n < rhs.units();
}

template <intmax_t N, typename C, typename T>
bool
operator==(Scalar<N, C, T> const &lhs, C n)
{
  return lhs.units() == n;
}
template <intmax_t N, typename C, typename T>
bool
operator==(C n, Scalar<N, C, T> const &rhs)
{
  return n == rhs.units();
}
template <intmax_t N, typename C, typename T>
bool
operator==(Scalar<N, C, T> const &lhs, int n)
{
  return lhs.units() == static_cast<C>(n);
}
template <intmax_t N, typename C, typename T>
bool
operator==(int n, Scalar<N, C, T> const &rhs)
{
  return static_cast<C>(n) == rhs.units();
}
template <intmax_t N>
bool
operator==(Scalar<N, int> const &lhs, int n)
{
  return lhs.units() == n;
}
template <intmax_t N>
bool
operator==(int n, Scalar<N, int> const &rhs)
{
  return n == rhs.units();
}

template <intmax_t N, typename C, typename T>
bool
operator>(Scalar<N, C, T> const &lhs, C n)
{
  return lhs.units() > n;
}
template <intmax_t N, typename C, typename T>
bool
operator>(C n, Scalar<N, C, T> const &rhs)
{
  return n > rhs.units();
}
template <intmax_t N, typename C, typename T>
bool
operator>(Scalar<N, C, T> const &lhs, int n)
{
  return lhs.units() > static_cast<C>(n);
}
template <intmax_t N, typename C, typename T>
bool
operator>(int n, Scalar<N, C, T> const &rhs)
{
  return static_cast<C>(n) > rhs.units();
}
template <intmax_t N>
bool
operator>(Scalar<N, int> const &lhs, int n)
{
  return lhs.units() > n;
}
template <intmax_t N>
bool
operator>(int n, Scalar<N, int> const &rhs)
{
  return n > rhs.units();
}

template <intmax_t N, typename C, typename T>
bool
operator<=(Scalar<N, C, T> const &lhs, C n)
{
  return lhs.units() <= n;
}
template <intmax_t N, typename C, typename T>
bool
operator<=(C n, Scalar<N, C, T> const &rhs)
{
  return n <= rhs.units();
}
template <intmax_t N, typename C, typename T>
bool
operator<=(Scalar<N, C, T> const &lhs, int n)
{
  return lhs.units() <= static_cast<C>(n);
}
template <intmax_t N, typename C, typename T>
bool
operator<=(int n, Scalar<N, C, T> const &rhs)
{
  return static_cast<C>(n) <= rhs.units();
}
template <intmax_t N>
bool
operator<=(Scalar<N, int> const &lhs, int n)
{
  return lhs.units() <= n;
}
template <intmax_t N>
bool
operator<=(int n, Scalar<N, int> const &rhs)
{
  return n <= rhs.units();
}

template <intmax_t N, typename C, typename T>
bool
operator>=(Scalar<N, C, T> const &lhs, C n)
{
  return lhs.units() >= n;
}
template <intmax_t N, typename C, typename T>
bool
operator>=(C n, Scalar<N, C, T> const &rhs)
{
  return n >= rhs.units();
}
template <intmax_t N, typename C, typename T>
bool
operator>=(Scalar<N, C, T> const &lhs, int n)
{
  return lhs.units() >= static_cast<C>(n);
}
template <intmax_t N, typename C, typename T>
bool
operator>=(int n, Scalar<N, C, T> const &rhs)
{
  return static_cast<C>(n) >= rhs.units();
}
template <intmax_t N>
bool
operator>=(Scalar<N, int> const &lhs, int n)
{
  return lhs.units() >= n;
}
template <intmax_t N>
bool
operator>=(int n, Scalar<N, int> const &rhs)
{
  return n >= rhs.units();
}
# endif
// Arithmetic operators
template <intmax_t N, typename C, typename T>
template <intmax_t S, typename I>
auto
Scalar<N, C, T>::operator+=(Scalar<S, I, T> const &that) -> self &
{
  typedef std::ratio<S, N> R;
  static_assert(R::den == 1, "Addition not permitted - target scale is not an integral multiple of source scale.");
  _n += that.count() * R::num;
  return *this;
}
template <intmax_t N, typename C, typename T>
auto
Scalar<N, C, T>::operator+=(self const &that) -> self &
{
  _n += that._n;
  return *this;
}
template <intmax_t N, typename C, typename T>
auto
Scalar<N, C, T>::operator+=(C n) -> self &
{
  _n += n;
  return *this;
}
template <intmax_t N, typename C, typename T>
template <typename I>
auto
Scalar<N, C, T>::operator+=(detail::scalar_unit_round_up_t<I> v) -> self &
{
  _n += v.template scale<N, C>();
  return *this;
}
template <intmax_t N, typename C, typename T>
template <typename I>
auto
Scalar<N, C, T>::operator+=(detail::scalar_unit_round_down_t<I> v) -> self &
{
  _n += v.template scale<N, C>();
  return *this;
}
template <intmax_t N, typename C, typename T>
auto
Scalar<N, C, T>::operator+=(detail::scalar_round_up_t<N, C, T> v) -> self &
{
  _n += v._n;
  return *this;
}
template <intmax_t N, typename C, typename T>
auto
Scalar<N, C, T>::operator+=(detail::scalar_round_down_t<N, C, T> v) -> self &
{
  _n += v._n;
  return *this;
}

template <intmax_t N, typename C, intmax_t S, typename I, typename T>
auto
operator+(Scalar<N, C, T> lhs, Scalar<S, I, T> const &rhs) -> typename std::common_type<Scalar<N, C, T>, Scalar<S, I, T>>::type
{
  return typename std::common_type<Scalar<N, C, T>, Scalar<S, I, T>>::type(lhs) += rhs;
}

template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator+(Scalar<N, C, T> const &lhs, Scalar<N, C, T> const &rhs)
{
  return Scalar<N, C, T>(lhs) += rhs;
}
template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator+(Scalar<N, C, T> const &lhs, C n)
{
  return Scalar<N, C, T>(lhs) += n;
}
template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator+(C n, Scalar<N, C, T> const &rhs)
{
  return Scalar<N, C, T>(rhs) += n;
}
template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator+(Scalar<N, C, T> const &lhs, int n)
{
  return Scalar<N, C, T>(lhs) += n;
}
template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator+(int n, Scalar<N, C, T> const &rhs)
{
  return Scalar<N, C, T>(rhs) += n;
}
template <intmax_t N, typename T>
Scalar<N, int>
operator+(Scalar<N, int, T> const &lhs, int n)
{
  return Scalar<N, int, T>(lhs) += n;
}
template <intmax_t N>
Scalar<N, int>
operator+(int n, Scalar<N, int> const &rhs)
{
  return Scalar<N, int>(rhs) += n;
}
template <intmax_t N, typename C, typename T, typename I>
Scalar<N, C, T>
operator+(detail::scalar_unit_round_up_t<I> lhs, Scalar<N, C, T> const &rhs)
{
  return Scalar<N, C, T>(rhs) += lhs.template scale<N, C>();
}
template <intmax_t N, typename C, typename T, typename I>
Scalar<N, C, T>
operator+(Scalar<N, C, T> const &lhs, detail::scalar_unit_round_up_t<I> rhs)
{
  return Scalar<N, C, T>(lhs) += rhs.template scale<N, C>();
}
template <intmax_t N, typename C, typename T, typename I>
Scalar<N, C, T>
operator+(detail::scalar_unit_round_down_t<I> lhs, Scalar<N, C, T> const &rhs)
{
  return Scalar<N, C, T>(rhs) += lhs.template scale<N, C>();
}
template <intmax_t N, typename C, typename T, typename I>
Scalar<N, C, T>
operator+(Scalar<N, C, T> const &lhs, detail::scalar_unit_round_down_t<I> rhs)
{
  return Scalar<N, C, T>(lhs) += rhs.template scale<N, C>();
}
template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator+(detail::scalar_round_up_t<N, C, T> lhs, Scalar<N, C, T> const &rhs)
{
  return Scalar<N, C, T>(rhs) += lhs._n;
}
template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator+(Scalar<N, C, T> const &lhs, detail::scalar_round_up_t<N, C, T> rhs)
{
  return Scalar<N, C, T>(lhs) += rhs._n;
}
template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator+(detail::scalar_round_down_t<N, C, T> lhs, Scalar<N, C, T> const &rhs)
{
  return Scalar<N, C, T>(rhs) += lhs._n;
}
template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator+(Scalar<N, C, T> const &lhs, detail::scalar_round_down_t<N, C, T> rhs)
{
  return Scalar<N, C, T>(lhs) += rhs._n;
}

template <intmax_t N, typename C, typename T>
template <intmax_t S, typename I>
auto
Scalar<N, C, T>::operator-=(Scalar<S, I, T> const &that) -> self &
{
  typedef std::ratio<S, N> R;
  static_assert(R::den == 1, "Subtraction not permitted - target scale is not an integral multiple of source scale.");
  _n -= that.count() * R::num;
  return *this;
}
template <intmax_t N, typename C, typename T>
auto
Scalar<N, C, T>::operator-=(self const &that) -> self &
{
  _n -= that._n;
  return *this;
}
template <intmax_t N, typename C, typename T>
auto
Scalar<N, C, T>::operator-=(C n) -> self &
{
  _n -= n;
  return *this;
}
template <intmax_t N, typename C, typename T>
template <typename I>
auto
Scalar<N, C, T>::operator-=(detail::scalar_unit_round_up_t<I> v) -> self &
{
  _n -= v.template scale<N, C>();
  return *this;
}
template <intmax_t N, typename C, typename T>
template <typename I>
auto
Scalar<N, C, T>::operator-=(detail::scalar_unit_round_down_t<I> v) -> self &
{
  _n -= v.template scale<N, C>();
  return *this;
}
template <intmax_t N, typename C, typename T>
auto
Scalar<N, C, T>::operator-=(detail::scalar_round_up_t<N, C, T> v) -> self &
{
  _n -= v._n;
  return *this;
}
template <intmax_t N, typename C, typename T>
auto
Scalar<N, C, T>::operator-=(detail::scalar_round_down_t<N, C, T> v) -> self &
{
  _n -= v._n;
  return *this;
}

template <intmax_t N, typename C, intmax_t S, typename I, typename T>
auto
operator-(Scalar<N, C, T> lhs, Scalar<S, I, T> const &rhs) -> typename std::common_type<Scalar<N, C, T>, Scalar<S, I, T>>::type
{
  return typename std::common_type<Scalar<N, C, T>, Scalar<S, I, T>>::type(lhs) -= rhs;
}

template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator-(Scalar<N, C, T> const &lhs, Scalar<N, C, T> const &rhs)
{
  return Scalar<N, C, T>(lhs) -= rhs;
}
template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator-(Scalar<N, C, T> const &lhs, C n)
{
  return Scalar<N, C, T>(lhs) -= n;
}
template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator-(C n, Scalar<N, C, T> const &rhs)
{
  return Scalar<N, C, T>(n) -= rhs;
}
template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator-(Scalar<N, C, T> const &lhs, int n)
{
  return Scalar<N, C, T>(lhs) -= n;
}
template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator-(int n, Scalar<N, C, T> const &rhs)
{
  return Scalar<N, C, T>(n) -= rhs;
}
template <intmax_t N>
Scalar<N, int>
operator-(Scalar<N, int> const &lhs, int n)
{
  return Scalar<N, int>(lhs) -= n;
}
template <intmax_t N>
Scalar<N, int>
operator-(int n, Scalar<N, int> const &rhs)
{
  return Scalar<N, int>(n) -= rhs;
}
template <intmax_t N, typename C, typename T, typename I>
Scalar<N, C, T>
operator-(detail::scalar_unit_round_up_t<I> lhs, Scalar<N, C, T> const &rhs)
{
  return Scalar<N, C, T>(lhs.template scale<N, C>()) -= rhs;
}
template <intmax_t N, typename C, typename T, typename I>
Scalar<N, C, T>
operator-(Scalar<N, C, T> const &lhs, detail::scalar_unit_round_up_t<I> rhs)
{
  return Scalar<N, C, T>(lhs) -= rhs.template scale<N, C>();
}
template <intmax_t N, typename C, typename T, typename I>
Scalar<N, C, T>
operator-(detail::scalar_unit_round_down_t<I> lhs, Scalar<N, C, T> const &rhs)
{
  return Scalar<N, C, T>(lhs.template scale<N, C>()) -= rhs;
}
template <intmax_t N, typename C, typename T, typename I>
Scalar<N, C, T>
operator-(Scalar<N, C, T> const &lhs, detail::scalar_unit_round_down_t<I> rhs)
{
  return Scalar<N, C, T>(lhs) -= rhs.template scale<N, C>();
}
template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator-(detail::scalar_round_up_t<N, C, T> lhs, Scalar<N, C, T> const &rhs)
{
  return Scalar<N, C, T>(lhs._n) -= rhs;
}
template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator-(Scalar<N, C, T> const &lhs, detail::scalar_round_up_t<N, C, T> rhs)
{
  return Scalar<N, C, T>(lhs) -= rhs._n;
}
template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator-(detail::scalar_round_down_t<N, C, T> lhs, Scalar<N, C, T> const &rhs)
{
  return Scalar<N, C, T>(lhs._n) -= rhs;
}
template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator-(Scalar<N, C, T> const &lhs, detail::scalar_round_down_t<N, C, T> rhs)
{
  return Scalar<N, C, T>(lhs) -= rhs._n;
}

template <intmax_t N, typename C, typename T> auto Scalar<N, C, T>::operator++() -> self &
{
  ++_n;
  return *this;
}

template <intmax_t N, typename C, typename T> auto Scalar<N, C, T>::operator++(int) -> self
{
  self zret(*this);
  ++_n;
  return zret;
}

template <intmax_t N, typename C, typename T> auto Scalar<N, C, T>::operator--() -> self &
{
  --_n;
  return *this;
}

template <intmax_t N, typename C, typename T> auto Scalar<N, C, T>::operator--(int) -> self
{
  self zret(*this);
  --_n;
  return zret;
}

template <intmax_t N, typename C, typename T>
auto
Scalar<N, C, T>::operator*=(C n) -> self &
{
  _n *= n;
  return *this;
}

template <intmax_t N, typename C, typename T> Scalar<N, C, T> operator*(Scalar<N, C, T> const &lhs, C n)
{
  return Scalar<N, C, T>(lhs) *= n;
}
template <intmax_t N, typename C, typename T> Scalar<N, C, T> operator*(C n, Scalar<N, C, T> const &rhs)
{
  return Scalar<N, C, T>(rhs) *= n;
}
template <intmax_t N, typename C, typename T> Scalar<N, C, T> operator*(Scalar<N, C, T> const &lhs, int n)
{
  return Scalar<N, C, T>(lhs) *= n;
}
template <intmax_t N, typename C, typename T> Scalar<N, C, T> operator*(int n, Scalar<N, C, T> const &rhs)
{
  return Scalar<N, C, T>(rhs) *= n;
}
template <intmax_t N> Scalar<N, int> operator*(Scalar<N, int> const &lhs, int n)
{
  return Scalar<N, int>(lhs) *= n;
}
template <intmax_t N> Scalar<N, int> operator*(int n, Scalar<N, int> const &rhs)
{
  return Scalar<N, int>(rhs) *= n;
}

template <intmax_t N, typename C, typename T>
auto
Scalar<N, C, T>::operator/=(C n) -> self &
{
  _n /= n;
  return *this;
}

# if 0
template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator/(Scalar<N, C, T> const &lhs, C n)
{
  return Scalar<N, C, T>(lhs) /= n;
}
template <intmax_t N, typename C, typename T>
Scalar<N, C, T>
operator/(Scalar<N, C, T> const &lhs, int n)
{
  return Scalar<N, C, T>(lhs) /= n;
}
template <intmax_t N>
Scalar<N, int>
operator/(Scalar<N, int> const &lhs, int n)
{
  return Scalar<N, int>(lhs) /= n;
}
# endif

namespace detail
{
  // These classes exist only to create distinguishable overloads.
  struct tag_label_A {
  };
  struct tag_label_B : public tag_label_A {
  };
  // The purpose is to print a label for a tagged type only if the tag class defines a member that
  // is the label.  This creates a base function that always works and does nothing. The second
  // function creates an overload if the tag class has a member named 'label' that has an stream IO
  // output operator. When invoked with a second argument of B then the second overload exists and
  // is used, otherwise only the first exists and that is used. The critical technology is the use
  // of 'auto' and 'decltype' which effectively checks if the code inside 'decltype' compiles.
  template <typename T>
  inline std::ostream &
  tag_label(std::ostream &s, tag_label_A const &)
  {
    return s;
  }
  template <typename T>
  inline auto
  tag_label(std::ostream &s, tag_label_B const &) -> decltype(s << T::label, s)
  {
    return s << T::label;
  }
} // detail

} // namespace

namespace std
{
template <intmax_t N, typename C, typename T>
ostream &
operator<<(ostream &s, ApacheTrafficServer::Scalar<N, C, T> const &x)
{
  static ApacheTrafficServer::detail::tag_label_B b; // Can't be const or the compiler gets upset.
  s << x.units();
  return ApacheTrafficServer::detail::tag_label<T>(s, b);
}

/// Compute common type of two scalars.
/// In `std` to overload the base definition. This yields a type that has the common type of the
/// counter type and a scale that is the GCF of the input scales.
template <intmax_t N, typename C, intmax_t S, typename I, typename T>
struct common_type<ApacheTrafficServer::Scalar<N, C, T>, ApacheTrafficServer::Scalar<S, I, T>> {
  typedef std::ratio<N, S> R;
  typedef ApacheTrafficServer::Scalar<N / R::num, typename common_type<C, I>::type, T> type;
};
}
#endif // TS_SCALAR_H
