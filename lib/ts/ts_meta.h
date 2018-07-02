/** @file

  Meta programming support utilities.

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

namespace ts
{
namespace meta
{
  /** This creates an order series of meta template cases that can be used to select one of a set of functions in a
   * priority ordering. A set of templated overloads take an (extra) argument of the case structures, each a
   * different one. Calling the function invokes the highest case that is valid. Because of SFINAE the templates
   * can have errors, as long as at least one doesn't. The root technique is to use @c decltype to check an expression for
   * the overload to be valid. Because the compiler will evaluate everything it can while parsing the template this
   * expression must be delayed until the template is instantiated. This is done by making the return type @c auto
   * and making the @c decltype dependent on the template parameter. In addition, the comma operator can be used to force
   * a specific return type while also checking the expression for validity. E.g.
   *
   * @code
   * template <typename T> auto func(T& t, CaseArg_0 const&) -> decltype(T::item, int()) { }
   * @endcode
   *
   * The comma operator discards the type and value of the left operand therefore the return type of the function is @c int
   * but this overload will not be available if @c T::item does not compile (e.g., there is no such member). The presence
   * of @c T::item also prevents this compilation check from happening until overload selection is needed. Therefore if
   * the goal was a function that would return the value of the @c T::count member if present and 0 if not, the code
   * would be
   *
   * @code
   * template <typename T> int func(T& t, CaseArg_0 const&) { return 0; }
   * template <typename T> auto func(T& t, CaseArg_1 const&) -> decltype(T::count, int()) { return t.count; }
   * int Get_Count(T& t) { return func(t, CaseArg); }
   * @endcode
   *
   * Note the overloads will be checked from the highest case to the lowest. This would not work if the @c CaseArg_0
   * and @c CaseArg_1 arguments were interchanged - the "return 0" overload would always be selected.
   *
   * Note @c decltype does not accept explicit types - to have the type of "int" an @c int must be constructed. This
   * is easy for builtin types except @c void. @c CaseVoidFunc is provided for that situation, e.g.
   * <tt>decltype(CaseVoidFunc())</tt> provides @c void via @c decltype.
   */

  // Base case
  struct CaseArg_0 {
  };
  // Next case.
  struct CaseArg_1 : public CaseArg_0 {
  };
  // Additional cases can be added here as necessary.

  // This is the final subclass so that callers can always use this, even if more cases are added.
  // This must be a subclass of the last specific case type.
  struct CaseArg : public CaseArg_1 {
    constexpr CaseArg() {}
  };

  // A single static instance suffices for all uses.
  static constexpr CaseArg Case_Arg;
  // A function to provide a @c void type for use in cases.
  void
  CaseVoidFunc()
  {
  }

} // namespace meta
} // namespace ts
