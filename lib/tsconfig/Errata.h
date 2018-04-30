/** @file
    Stacking error message handling.

    The problem addressed by this library is the ability to pass back
    detailed error messages from failures. It is hard to get good
    diagnostics because the specific failures and general context are
    located in very different stack frames. This library allows local
    functions to pass back local messages which can be easily
    augmented as the error travels up the stack frame.

    This aims to improve over exceptions by being lower cost and not requiring callers to handle the messages.
    On the other hand, the messages could be used just as easily with exceptions.

    Each message on a stack contains text and a numeric identifier.
    The identifier value zero is reserved for messages that are not
    errors so that information can be passed back even in the success
    case.

    The implementation takes the position that success must be fast and
    failure is expensive. Therefore Errata is optimized for the success
    path, imposing very little overhead in that case. On the other hand, if an
    error occurs and is handled, that is generally so expensive that
    optimizations are pointless (although, of course, code should not
    be gratuitiously expensive).

    The library provides the @c Rv ("return value") template to
    make returning values and status easier. This template allows a
    function to return a value and status pair with minimal changes.
    The pair acts like the value type in most situations, while
    providing access to the status.

    Each instance of an erratum is a wrapper class that emulates value
    semantics (copy on write). This means passing even large message
    stacks is inexpensive, involving only a pointer copy and reference
    counter increment and decrement. A success value is represented by
    an internal @c NULL so it is even cheaper to copy.

    To further ease use, the library has the ability to define @a
    sinks.  A sink is a function that acts on an erratum when it
    becomes unreferenced. The indended use is to send the messages to
    an output log. This makes reporting errors to a log from even
    deeply nested functions easy while preserving the ability of the
    top level logic to control such logging.

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

#include <memory>
#include <vector>
#include <ts/string_view.h>
#include <ts/MemArena.h>
#include <ts/BufferWriter.h>
#include <tsconfig/NumericType.h>
#include "IntrusivePtr.h"

namespace ts
{
/// Severity levels for Errata.
enum class Severity {
  DIAG, ///< Diagnostic only. DL_Diag
  DBG, ///< Debugging. DL_Debug ('DEBUG' is a macro)
  INFO, ///< Informative. DL_Status
  NOTE, ///< Note. DL_Note
  WARN, ///< Warning. DL_Warning
  ERROR, ///< Error. DL_Error
  FATAL, ///< Fatal. DL_Fatal
  ALERT, ///< Alert. DL_Alert
  EMERGENCY, ///< Emergency. DL_Emergency.
};

/** Class to hold a stack of error messages (the "errata").
    This is a smart handle class, which wraps the actual data
    and can therefore be treated a value type with cheap copy
    semantics. Default construction is very cheap.
 */
class Errata
{
protected:
  /// Implementation class.
  struct Data;

  using self_type   = Errata;          ///< Self reference type.
  using string_view = ts::string_view; // use this to adjust for C++11 vs. C++17.

public:
  using Severity = ts::Severity; ///< Import for associated classes.

  /// Severity used if not specified.
  static constexpr Severity DEFAULT_SEVERITY{Severity::DIAG};
  /// Severity level at which the instance is a failure of some sort.
  static constexpr Severity SERIOUS_SEVERITY{Severity::WARN};

  struct Message; // Forward declaration.

  /// Storage type for list of messages.
  /// Internally the vector is accessed backwards, in order to make it LIFO.
  using Container = std::vector<Message>;

  /// Default constructor - empty errata, very fast.
  Errata();
  Errata(self_type const &that) = delete;                          // no copying.
  Errata(self_type &&that) = default;                                        ///< Move constructor.
  self_type &operator=(self_type const &that) = delete;            // no assignemnt.
  self_type &operator                         =(self_type &&that) = default; // Move assignment.
  ~Errata();                                                       ///< Destructor.

  /** Add a new message to the top of stack with default severity and @a text.
   * @param level Severity of the message.
   * @param text Text of the message.
   * @return *this
   */
  self_type &msg(string_view text);

  /** Add a new message to the top of stack with severity @a level and @a text.
   * @param level Severity of the message.
   * @param text Text of the message.
   * @return *this
   */
  self_type &msg(Severity level, string_view text);

  /** Push a constructed @c Message.
      The @c Message is set to have the @a id and @a code. The other arguments are converted
      to strings and concatenated to form the messsage text.
      @return A reference to this object.
  */
  template <typename... Args> self_type &msg(Severity level, string_view fmt, Args &&... args);

  /** Push a constructed @c Message.
      The @c Message is set to have the @a id and @a code. The other arguments are converted
      to strings and concatenated to form the messsage text.
      @return A reference to this object.
  */
  template <typename... Args> self_type &msgv(Severity level, string_view fmt, std::tuple<Args...> const&args);

  /** Access last message.
      @return If the errata is empty, a default constructed message
      otherwise the most recent message.
   */
  Message const &front() const;

  /// Remove all messages.
  self_type &clear();

  /** Inhibit logging.
  */
  self_type &disable_logging();

  friend std::ostream &operator<<(std::ostream &, self_type const &);

  /// Default glue value (a newline) for text rendering.
  static string_view const DEFAULT_GLUE;

  /** Test status.

      Equivalent to @c success but more convenient for use in
      control statements.

      @return @c true if no messages or last message has a zero
      message ID, @c false otherwise.
   */
  explicit operator bool() const;

  /** Test errata for no failure condition.

      Equivalent to @c operator @c bool but easier to invoke.

      @return @c true if no messages or last message has a zero
      message ID, @c false otherwise.
   */
  bool is_ok() const;

  /** Get the maximum severity of the messages in the erratum.
   *
   * @return Max severity for all messages.
   */
  Severity severity() const;

  /// Number of messages in the errata.
  size_t size() const;

  /** Copy messages from @a that to @a this.
   *
   * @param that Source object from which to copy.
   * @return @a *this
   */
  self_type & copy_from(self_type const& that);

  /*  Forward declares.
      We have to make our own iterators as the least bad option. The problem
      is that we have recursive structures so declaration order is difficult.
      We can't use the container iterators here because the element type is
      not yet defined. If we define the element type here, it can't contain
      an Errata and we have to do funky things to get around that. So we
      have our own iterators, which are just shadowing sublclasses of the
      container iterators.
   */
  class iterator;
  class const_iterator;

  /// Reference to top item on the stack.
  iterator begin();
  /// Reference to top item on the stack.
  const_iterator begin() const;
  //! Reference one past bottom item on the stack.
  iterator end();
  //! Reference one past bottom item on the stack.
  const_iterator end() const;

  // Logging support.

  /** Base class for erratum sink.
      When an errata is abandoned, this will be called on it to perform
      any client specific logging. It is passed around by handle so that
      it doesn't have to support copy semantics (and is not destructed
      until application shutdown). Clients can subclass this class in order
      to preserve arbitrary data for the sink or retain a handle to the
      sink for runtime modifications.
   */
  class Sink
  {
    using self_type = Sink;

  public:
    using Handle = std::shared_ptr<self_type>; ///< Handle type.

    /// Handle an abandoned errata.
    virtual void operator()(Errata const &) const = 0;
    /// Force virtual destructor.
    virtual ~Sink() {}
  };

  //! Register a sink for discarded erratum.
  static void registerSink(Sink::Handle const &s);

  /// Register a function as a sink.
  using SinkHandlerFunction = void (*)(Errata const &);

    // Wrapper class to support registering functions as sinks.
    struct SinkFunctionWrapper : public Sink {
        /// Constructor.
        SinkFunctionWrapper(SinkHandlerFunction f) : m_f(f) { }
        /// Operator to invoke the function.
        void operator() (Errata const& e) const override { m_f(e); }
        SinkHandlerFunction m_f; ///< Client supplied handler.
    };

  /// Register a sink function for abandonded erratum.
  static void
  registerSink(SinkHandlerFunction f)
  {
    registerSink(Sink::Handle(new SinkFunctionWrapper(f)));
  }

  /** Simple formatted output.
   */
  std::ostream &write(std::ostream &out) const;

protected:
  /// Implementation instance.
  std::unique_ptr<Data> _data;
  /// Force data existence.
  Data *data();

  /// Used for returns when no data is present.
  static Message const NIL_MESSAGE;

  friend struct Data;
  friend class Item;
};

extern std::ostream &operator<<(std::ostream &os, Errata const &stat);

/// Storage for a single message.
struct Errata::Message {
  using self_type   = Message;          ///< Self reference type.
  using Severity    = Errata::Severity; ///< Message severity level.
  using string_view = ts::string_view;

  /// Default constructor.
  /// The message has Id = 0, default code,  and empty text.
  Message();

  /** Construct with severity @a level and @a text.
   *
   * @param level Severity level.
   * @param text Message content.
   */
  Message(Severity level, string_view text);

  /// Reset to the message to default state.
  self_type &clear();

  /// Get the severity.
  Severity severity() const;

  /// Get the text of the message.
  string_view text() const;

  /// Set the text of the message.
  self_type &assign(string_view text);

  /// Set the severity @a level
  self_type &assign(Severity level);

  Severity _level{Errata::DEFAULT_SEVERITY}; ///< Message code.
  string_view _text;                         ///< Final text.
};

/** This is the implementation class for Errata.

    It holds the actual messages and is treated as a passive data
    object with nice constructors.
*/
struct Errata::Data {
  using self_type = Data; ///< Self reference type.
  using Severity  = Errata::Severity;

  //! Default constructor.
  Data();

  /** Duplicate @a src in this arena.
   *
   * @param src Source data.
   * @return View of copy in this arean.
   */
  string_view dup(string_view src);

  /// The message stack.
  Container _items;
  /// Message text storage.
  MemArena _arena{512}; // start with 512 bytes of string storage.
  /// The effective severity of the message stack.
  Severity _level{Errata::DEFAULT_SEVERITY};
  /// Log this when it is deleted.
  mutable bool _log_on_delete{true};
};

/// Forward iterator for @c Messages in an @c Errata.
class Errata::iterator : public Errata::Container::reverse_iterator
{
  using self_type  = iterator; ///< Self-reference type.
  using super_type = Errata::Container::reverse_iterator;

public:
  iterator(); ///< Default constructor.
  /// Copy constructor.
  iterator(self_type const &that ///< Source instance.
           );
  /// Construct from super class.
  iterator(super_type const &that ///< Source instance.
           );
  /// Assignment.
  self_type &operator=(self_type const &that);
  /// Assignment from super class.
  self_type &operator=(super_type const &that);
  /// Prefix increment.
  self_type &operator++();
  /// Prefix decrement.
  self_type &operator--();
};

/// Forward constant iterator for @c Messages in an @c Errata.
class Errata::const_iterator : public Errata::Container::const_reverse_iterator
{
  using self_type  = const_iterator;                            ///< Self reference type.
  using super_type = Errata::Container::const_reverse_iterator; ///< Parent type.
public:
  const_iterator(); ///< Default constructor.
  /// Copy constructor.
  const_iterator(self_type const &that ///< Source instance.
                 );
  const_iterator(super_type const &that ///< Source instance.
                 );
  /// Assignment.
  self_type &operator=(self_type const &that);
  /// Assignment from super class.
  self_type &operator=(super_type const &that);
  /// Prefix increment.
  self_type &operator++();
  /// Prefix decrement.
  self_type &operator--();
};

/** Helper class for @c Rv.
    This class enables us to move the implementation of non-templated methods
    and members out of the header file for a cleaner API.
 */
struct RvBase {
  Errata _errata; ///< The status from the function.

  /** Default constructor. */
  RvBase();

  /**
   * Construct with existing @c Errata.
   * @param s Errata for result.
   */
  RvBase(Errata &&s);

  //! Test the return value for success.
  bool is_ok() const;

  /** Clear any stacked errors.
      This is useful during shutdown, to silence irrelevant errors caused
      by the shutdown process.
  */
  void clear();

  /// Inhibit logging of the errata.
  void disable_logging();
};

/** Return type for returning a value and status (errata).  In
    general, a method wants to return both a result and a status so
    that errors are logged properly. This structure is used to do that
    in way that is more usable than just @c std::pair.  - Simpler and
    shorter typography - Force use of @c errata rather than having to
    remember it (and the order) each time - Enable assignment directly
    to @a R for ease of use and compatibility so clients can upgrade
    asynchronously.
 */
template <typename R> struct Rv : public RvBase {
  using self_type  = Rv;     ///< Standard self reference type.
  using super_type = RvBase; ///< Standard super class reference type.
  using Result     = R;      ///< Type of result value.

  Result _result{}; ///< The actual result of the function.

  /** Default constructor.
      The default constructor for @a R is used.
      The status is initialized to SUCCESS.
  */
  Rv();

  /** Construct with copy of @a result and empty Errata.
   *
   * Construct with a specified @a result and a default (successful) @c Errata.
   * @param result Return value / result.
   */
  Rv(Result const &result);

  /** Construct with copy of @a result and move @a errata.
   *
   * Construct with a specified @a result and a default (successful) @c Errata.
   * @param result Return value / result.
   */
  Rv(Result const &result, Errata && errata);

  /** Construct with move of @a result and empty Errata.
   *
   * @param result The return / result value.
    */
  Rv(Result &&result);

  /** Construct with result and move of @a errata.
   *
   * @param result The return / result value to assign.
   * @param errata Status to move.
    */
  Rv(Result &&result, Errata &&errata);

  /** Push a message in to the result.
   *
   * @param level Severity of the message.
   * @param text Text of the message.
   * @return @a *this
   */
  self_type &msg(Severity level, string_view text);

  /** Push a message in to the result.
   *
   * @param level Severity of the message.
   * @param text Text of the message.
   * @return @a *this
   */
  template <typename... Args> self_type &msg(Severity level, string_view fmt, Args &&... args);

  /** User conversion to the result type.

      This makes it easy to use the function normally or to pass the
      result only to other functions without having to extract it by
      hand.
  */
  operator Result const &() const;

  /** Assignment from result type.

      This allows the result to be assigned to a pre-declared return
      value structure.  The return value is a reference to the
      internal result so that this operator can be chained in other
      assignments to instances of result type. This is most commonly
      used when the result is computed in to a local variable to be
      both returned and stored in a member.

      @code
      Rv<int> zret;
      int value;
      // ... complex computations, result in value
      this->m_value = zret = value;
      // ...
      return zret;
      @endcode

      @return A reference to the copy of @a r stored in this object.
  */
  Result &
  operator=(Result const &r ///< Result to assign
            )
  {
    _result = r;
    return _result;
  }

  /** Set the result.

      This differs from assignment of the function result in that the
      return value is a reference to the @c Rv, not the internal
      result. This makes it useful for assigning a result local
      variable and then returning.

   * @param result Value to move.
   * @return @a this

      @code
      Rv<int> func(...) {
        Rv<int> zret;
        int value;
        // ... complex computation, result in value
        return zret.set(value);
      }
      @endcode
  */
  self_type &set(Result const &result);

  /** Move the @a result to @a this.
   *
   * @param result Value to move.
   * @return @a this,
   */
  self_type &set(Result &&result);

  /** Return the result.
      @return A reference to the result value in this object.
  */
  Result &result();

  /** Return the result.
      @return A reference to the result value in this object.
  */
  Result const &result() const;

  /** Return the status.
      @return A reference to the @c errata in this object.
  */
  Errata &errata();

  /** Get the internal @c Errata.
   *
   * @return Reference to internal @c Errata.
   */
  operator Errata& ();

  /** Return the status.
      @return A reference to the @c errata in this object.
  */
  Errata const &errata() const;

  /** Directly set the @c Errata with @a status.
   *
   * @param status Errata to move in to this instance.
   * @return *this
   */
  self_type &operator=(Errata &&status);
};

/** Combine a function result and status in to an @c Rv.
    This is useful for clients that want to declare the status object
    and result independently.
 */
template <typename R>
Rv<typename std::remove_reference<R>::type>
MakeRv(R && r,      ///< The function result
       Errata &&erratum ///< The pre-existing status object
       )
{
  return Rv<typename std::remove_reference<R>::type>(std::forward<R>(r), std::move(erratum));
}
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
// Inline methods.

// Errata::Message
inline Errata::Message::Message()
{
}

inline Errata::Message::Message(Severity level, string_view text) : _level(level), _text(text)
{
}

inline Errata::Message &
Errata::Message::clear()
{
  _level = Errata::DEFAULT_SEVERITY;
  _text  = string_view{};
  return *this;
}

inline ts::string_view
Errata::Message::text() const
{
  return _text;
}

inline Errata::Severity
Errata::Message::severity() const
{
  return _level;
}

inline Errata::Message &
Errata::Message::assign(string_view text)
{
  _text = text;
  return *this;
}

inline Errata::Message &
Errata::Message::assign(Severity level)
{
  _level = level;
  return *this;
}

// Errata

inline Errata::Errata()
{
}

inline Errata::Data *
Errata::data()
{
  if (!_data) {
    _data.reset(new Data);
  }
  return _data.get();
}

inline Errata::operator bool() const
{
  return this->is_ok();
}

inline Severity Errata::severity() const { return _data ? DEFAULT_SEVERITY : _data->_level; }

inline size_t
Errata::size() const
{
  return _data ? _data->_items.size() : 0;
}

inline bool
Errata::is_ok() const
{
  return 0 == _data || 0 == _data->_items.size() || _data->_level < SERIOUS_SEVERITY;
}

inline Errata &
Errata::msg(string_view text)
{
  this->msg(DEFAULT_SEVERITY, text);
  return *this;
}

inline Errata &
Errata::msg(Severity level, ts::string_view text)
{
  MemSpan span{this->data()->_arena.alloc(text.size())};
  memcpy(span.data(), text.data(), text.size());
  _data->_items.emplace_back(level, string_view(span.begin(), span.size()));
  _data->_level = std::max(_data->_level, level);
  return *this;
}

template <typename... Args>
Errata &
Errata::msg(Severity level, string_view fmt, Args &&... args) {
  return this->msgv(level, fmt, std::forward_as_tuple(args...));
}

template <typename... Args>
Errata &
Errata::msgv(Severity level, string_view fmt, std::tuple<Args...> const&args)
{
  MemSpan span{this->data()->_arena.remnant()};
  FixedBufferWriter bw{span};
  if (bw.printv(fmt, args).error()) {
    span = _data->_arena.alloc(bw.extent());
    FixedBufferWriter{span}.printv(fmt, args);
  }
  this->data()->_items.emplace_back(level, string_view(span.begin(), span.size()));
  _data->_level = std::max(_data->_level, level);
  return *this;
}

inline Errata::Message const &
Errata::front() const
{
  return _data && _data->_items.size() ? _data->_items.front() : NIL_MESSAGE;
}

inline Errata &
Errata::disable_logging()
{
  this->data()->_log_on_delete = false;
  return *this;
}

inline Errata&
Errata::clear()
{
  _data.reset(nullptr);
  return *this;
}

// Data

inline Errata::Data::Data()
{
}

inline string_view Errata::Data::dup(string_view src) {
  auto mem = this->_arena.alloc(src.size());
  memcpy(mem.data(), src.data(), mem.size());
  return {mem.begin(), static_cast<size_t >(mem.size())};
}

// Errata iterators

inline Errata::iterator::iterator()
{
}

inline Errata::iterator::iterator(self_type const &that) : super_type(that)
{
}

inline Errata::iterator::iterator(super_type const &that) : super_type(that)
{
}

inline Errata::iterator &
Errata::iterator::operator=(self_type const &that)
{
  this->super_type::operator=(that);
  return *this;
}

inline Errata::iterator &
Errata::iterator::operator=(super_type const &that)
{
  this->super_type::operator=(that);
  return *this;
}

inline Errata::iterator &Errata::iterator::operator++()
{
  this->super_type::operator++();
  return *this;
}

inline Errata::iterator &Errata::iterator::operator--()
{
  this->super_type::operator--();
  return *this;
}

inline Errata::const_iterator::const_iterator()
{
}

inline Errata::const_iterator::const_iterator(self_type const &that) : super_type(that)
{
}

inline Errata::const_iterator::const_iterator(super_type const &that) : super_type(that)
{
}

inline Errata::const_iterator &
Errata::const_iterator::operator=(self_type const &that)
{
  super_type::operator=(that);
  return *this;
}

inline Errata::const_iterator &
Errata::const_iterator::operator=(super_type const &that)
{
  super_type::operator=(that);
  return *this;
}

inline Errata::const_iterator &Errata::const_iterator::operator++()
{
  this->super_type::operator++();
  return *this;
}

inline Errata::const_iterator &Errata::const_iterator::operator--()
{
  this->super_type::operator--();
  return *this;
}

// RvBase

inline RvBase::RvBase()
{
}

inline RvBase::RvBase(Errata &&errata) : _errata(std::move(errata))
{
}

inline bool
RvBase::is_ok() const
{
  return _errata.is_ok();
}

inline void
RvBase::clear()
{
  _errata.clear();
}

inline void
RvBase::disable_logging()
{
  _errata.disable_logging();
}

// Rv<T>

template <typename T> Rv<T>::Rv()
{
}

template <typename T> Rv<T>::Rv(Result const &r) : _result(r)
{
}

template <typename T> Rv<T>::Rv(Result const &r, Errata && errata) : super_type(std::move(errata)), _result(r)
{
}

template <typename R> Rv<R>::Rv(R &&r) : _result(std::forward<R>(r))
{
}

template <typename R> Rv<R>::Rv(R &&r, Errata &&errata) : super_type(std::move(errata)), _result(std::forward<R>(r))
{
}

template <typename T> Rv<T>::operator Result const &() const
{
  return _result;
}

template <typename T>
T const &
Rv<T>::result() const
{
  return _result;
}

template <typename T>
T &
Rv<T>::result()
{
  return _result;
}

template <typename T>
Errata const &
Rv<T>::errata() const
{
  return _errata;
}

template <typename T>
Errata &
Rv<T>::errata()
{
  return _errata;
}

template < typename T > Rv<T>::operator Errata&() { return _errata; }

template <typename T>
Rv<T> &
Rv<T>::set(Result const &r)
{
  _result = r;
  return *this;
}

template <typename R>
Rv<R> &
Rv<R>::set(R &&r)
{
  _result = std::forward<R>(r);
  return *this;
}

template <typename T>
Rv<T> &
Rv<T>::operator=(Errata &&errata)
{
  _errata = std::move(errata);
  return *this;
}

template <typename T>
Rv<T> &
Rv<T>::msg(Severity level, string_view text)
{
  _errata.msg(level, text);
  return *this;
}

template <typename R>
template <typename... Args>
Rv<R> &
Rv<R>::msg(Severity level, string_view fmt, Args &&... args)
{
  _errata.msgv(level, fmt, std::forward_as_tuple(args...));
  return *this;
}

BufferWriter&
bwformat(BufferWriter& w, BWFSpec const& spec, Severity);

BufferWriter&
bwformat(BufferWriter& w, BWFSpec const& spec, Errata const&);

} // namespace ts
