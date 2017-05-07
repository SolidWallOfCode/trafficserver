/** @file

  A brief file description

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  ditributed with this work for additional information
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

#ifndef __CACHE_ARRAY_H__
#define __CACHE_ARRAY_H__

/** A vector class that pre-allocates elements.

    This class works very similarily to std::vector with 2 key differences:

    - The memory is not guaranteed to be contiguous.
    - A fixed number of elements are pre-allocated as part of the class.

    This is useful if, in most cases, the fixed size is sufficient in which case no additional heap
    allocations are done. If the size exceeds the fixed size then additional space is allocated as needed.
    In general @a N should be small - if it is large then the utility of this class is dubious.
*/
template <typename T, int N = 4> class SplitVector
{
protected:
  typedef SplitVector self; ///< Self reference type.
  size_t _n = 0;            ///< # of valid elements.
  std::array<T,N> _data;                  ///< Fixed storage.
  std::vector<T> _ext;      ///< Extra storage.
public:
  class const_iterator
  {
 protected:
    T *_ptr             = nullptr; ///< Current item.
    SplitVector *_owner = nullptr; ///< Needed to handle split transitions.

    const_iterator(SplitVector *owner, T *current);

  public:
    const_iterator();
    bool operator==(const_iterator const& that) const;
    bool operator!=(const_iterator const& that) const;
    T const &operator*() const;
    T const *operator->() const;
    const_iterator operator++(int);
    const_iterator &operator++();
    const_iterator operator--(int);
    const_iterator &operator--();
  };

  class iterator : public const_iterator
  {
    using const_iterator::_ptr;
    using const_iterator::_owner;

    iterator(SplitVector *owner, T *current);

  public:
    iterator();
    T &operator*() const;
    T *operator->() const;
    iterator operator++(int);
    iterator &operator++();
    iterator operator--(int);
    iterator &operator--();
  };

  SplitVector();

  T &operator[](int idx);
  T const &operator[](int idx) const;

  self &resize(int n);
  self &clear();
  int size() const;

  const_iterator begin() const;
  const_iterator end() const;
  iterator begin();
  iterator end();

private:
  // iterator support methods.
  bool is_fixed(T const*current) const; ///< Is @a current in the fixed data?
  T const*next(T const*current) const;             ///< Compute a pointer to the next element after @a current.
  T const*prev(T const*current) const;             ///< Computer a pointer to the previous element before @a current.
  friend class const_iterator;
  friend class iterator;
};

template <typename T, int N> SplitVector<T, N>::SplitVector()
{
  static_assert(N > 0, "A SplitVector must have a fixed portion of non-zero size");
}
template <typename T, int N> T &SplitVector<T, N>::operator[](int idx)
{
  return idx < N ? _data[idx] : _ext[idx - N];
}
template <typename T, int N> T const &SplitVector<T, N>::operator[](int idx) const
{
  return idx < N ? _data[idx] : _ext[idx - N];
}
template <typename T, int N>
int
SplitVector<T, N>::size() const
{
  return _ext.size() + N;
}
template <typename T, int N>
auto
SplitVector<T, N>::resize(int n) -> self &
{
  if (n <= N) {
    _ext.clear();
  } else {
    _ext.resize(n - N);
  }
  _n = n;
  return *this;
}

template <typename T, int N>
auto
SplitVector<T, N>::clear() -> self &
{
  _n = 0;
  _ext.clear();
  return *this;
}

template <typename T, int N>
bool
SplitVector<T, N>::is_fixed(T const*current) const
{
  return _data <= current && current <= _data + N;
}

template <typename T, int N>
T const*
SplitVector<T, N>::next(T const*current) const
{
  return (current == nullptr) ? nullptr : (current == &_data[N - 1]) ? &_ext.front() : current + 1;
}

template <typename T, int N>
T const*
SplitVector<T, N>::prev(T const*current) const
{
  return (current == nullptr) ? nullptr :
                             this->is_fixed(current) ? current - 1 : current == &_ext.front() ? _data + (N - 1) : current - 1;
}

// Iterators
template <typename T, int N> SplitVector<T, N>::const_iterator::const_iterator(SplitVector<T,N>* owner, T* current) : _ptr(current), _owner(owner)
{
}

template <typename T, int N> bool SplitVector<T, N>::const_iterator::operator == (const_iterator const& that) const
{
  return _ptr == that._ptr;
}

template <typename T, int N> bool SplitVector<T, N>::const_iterator::operator != (const_iterator const& that) const
{
  return _ptr != that._ptr;
}

template <typename T, int N> auto SplitVector<T, N>::const_iterator::operator++() -> const_iterator &
{
  _ptr = _owner->next(_ptr);
  return *this;
}

template <typename T, int N> auto SplitVector<T, N>::const_iterator::operator++(int) -> const_iterator
{
  const_iterator zret(*this);
  _ptr = _owner->next(_ptr);
  return zret;
}

template <typename T, int N> SplitVector<T, N>::iterator::iterator(SplitVector<T,N>* owner, T* current) : const_iterator(owner, current)
{
}

template <typename T, int N> auto SplitVector<T, N>::iterator::operator++() -> iterator &
{
  _ptr = const_cast<T*>(_owner->next(_ptr));
  return *this;
}

template <typename T, int N> auto SplitVector<T, N>::iterator::operator++(int) -> iterator
{
  iterator zret(*this);
  _ptr = const_cast<T*>(_owner->next(_ptr));
  return zret;
}

// Hopefully this can be replace with SplitVector.
template <class T> struct CacheArray {
  static const size_t FIXED_COUNT = 4; ///< Number of intrisinic elements.

  CacheArray(const T *val, int initial_size = 0);
  ~CacheArray();

  operator const T *() const;
  operator T *();
  T &operator[](int idx);
  T &operator()(int idx);
  T *detach();
  int length();
  void clear();
  void
  set_length(int i)
  {
    pos = i - 1;
  }

  void resize(int new_size);

  T *data;
  T fast_data[FIXED_COUNT];
  const T *default_val;
  int size;
  int pos;
};

template <class T>
TS_INLINE
CacheArray<T>::CacheArray(const T *val, int initial_size)
  : data(nullptr), default_val(val), size(0), pos(-1)
{
  if (initial_size > 0) {
    int i = 1;

    while (i < initial_size)
      i <<= 1;

    resize(i);
  }
}

template <class T> TS_INLINE CacheArray<T>::~CacheArray()
{
  if (data) {
    if (data != fast_data) {
      delete[] data;
    }
  }
}

template <class T> TS_INLINE CacheArray<T>::operator const T *() const
{
  return data;
}

template <class T> TS_INLINE CacheArray<T>::operator T *()
{
  return data;
}

template <class T> TS_INLINE T &CacheArray<T>::operator[](int idx)
{
  return data[idx];
}

template <class T>
TS_INLINE T &
CacheArray<T>::operator()(int idx)
{
  if (idx >= size) {
    int new_size;

    if (size == 0) {
      new_size = FIXED_COUNT;
    } else {
      new_size = size * 2;
    }

    if (idx >= new_size) {
      new_size = idx + 1;
    }

    resize(new_size);
  }

  if (idx > pos) {
    pos = idx;
  }

  return data[idx];
}

template <class T>
TS_INLINE T *
CacheArray<T>::detach()
{
  T *d;

  d    = data;
  data = nullptr;

  return d;
}

template <class T>
TS_INLINE int
CacheArray<T>::length()
{
  return pos + 1;
}

template <class T>
TS_INLINE void
CacheArray<T>::clear()
{
  if (data) {
    if (data != fast_data) {
      delete[] data;
    }
    data = nullptr;
  }

  size = 0;
  pos  = -1;
}

template <class T>
TS_INLINE void
CacheArray<T>::resize(int new_size)
{
  if (new_size > size) {
    T *new_data;
    int i;

    if (new_size > FIXED_COUNT) {
      new_data = new T[new_size];
    } else {
      new_data = fast_data;
    }

    for (i = 0; i < size; i++) {
      new_data[i] = data[i];
    }

    for (; i < new_size; i++) {
      new_data[i] = *default_val;
    }

    if (data) {
      if (data != fast_data) {
        delete[] data;
      }
    }
    data = new_data;
    size = new_size;
  }
}

#endif /* __CACHE_ARRAY_H__ */
