#include "lnav_ffi.hh"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace rust {
inline namespace cxxbridge1 {
// #include "rust/cxx.h"

#ifndef CXXBRIDGE1_PANIC
#define CXXBRIDGE1_PANIC
template <typename Exception>
void panic [[noreturn]] (const char *msg);
#endif // CXXBRIDGE1_PANIC

struct unsafe_bitcopy_t;

namespace {
template <typename T>
class impl;
} // namespace

class Opaque;

template <typename T>
::std::size_t size_of();
template <typename T>
::std::size_t align_of();

#ifndef CXXBRIDGE1_RUST_STRING
#define CXXBRIDGE1_RUST_STRING
class String final {
public:
  String() noexcept;
  String(const String &) noexcept;
  String(String &&) noexcept;
  ~String() noexcept;

  String(const std::string &);
  String(const char *);
  String(const char *, std::size_t);
  String(const char16_t *);
  String(const char16_t *, std::size_t);

  static String lossy(const std::string &) noexcept;
  static String lossy(const char *) noexcept;
  static String lossy(const char *, std::size_t) noexcept;
  static String lossy(const char16_t *) noexcept;
  static String lossy(const char16_t *, std::size_t) noexcept;

  String &operator=(const String &) &noexcept;
  String &operator=(String &&) &noexcept;

  explicit operator std::string() const;

  const char *data() const noexcept;
  std::size_t size() const noexcept;
  std::size_t length() const noexcept;
  bool empty() const noexcept;

  const char *c_str() noexcept;

  std::size_t capacity() const noexcept;
  void reserve(size_t new_cap) noexcept;

  using iterator = char *;
  iterator begin() noexcept;
  iterator end() noexcept;

  using const_iterator = const char *;
  const_iterator begin() const noexcept;
  const_iterator end() const noexcept;
  const_iterator cbegin() const noexcept;
  const_iterator cend() const noexcept;

  bool operator==(const String &) const noexcept;
  bool operator!=(const String &) const noexcept;
  bool operator<(const String &) const noexcept;
  bool operator<=(const String &) const noexcept;
  bool operator>(const String &) const noexcept;
  bool operator>=(const String &) const noexcept;

  void swap(String &) noexcept;

  String(unsafe_bitcopy_t, const String &) noexcept;

private:
  struct lossy_t;
  String(lossy_t, const char *, std::size_t) noexcept;
  String(lossy_t, const char16_t *, std::size_t) noexcept;
  friend void swap(String &lhs, String &rhs) noexcept { lhs.swap(rhs); }

  std::array<std::uintptr_t, 3> repr;
};
#endif // CXXBRIDGE1_RUST_STRING

#ifndef CXXBRIDGE1_RUST_STR
#define CXXBRIDGE1_RUST_STR
class Str final {
public:
  Str() noexcept;
  Str(const String &) noexcept;
  Str(const std::string &);
  Str(const char *);
  Str(const char *, std::size_t);

  Str &operator=(const Str &) &noexcept = default;

  explicit operator std::string() const;

  const char *data() const noexcept;
  std::size_t size() const noexcept;
  std::size_t length() const noexcept;
  bool empty() const noexcept;

  Str(const Str &) noexcept = default;
  ~Str() noexcept = default;

  using iterator = const char *;
  using const_iterator = const char *;
  const_iterator begin() const noexcept;
  const_iterator end() const noexcept;
  const_iterator cbegin() const noexcept;
  const_iterator cend() const noexcept;

  bool operator==(const Str &) const noexcept;
  bool operator!=(const Str &) const noexcept;
  bool operator<(const Str &) const noexcept;
  bool operator<=(const Str &) const noexcept;
  bool operator>(const Str &) const noexcept;
  bool operator>=(const Str &) const noexcept;

  void swap(Str &) noexcept;

private:
  class uninit;
  Str(uninit) noexcept;
  friend impl<Str>;

  std::array<std::uintptr_t, 2> repr;
};
#endif // CXXBRIDGE1_RUST_STR

#ifndef CXXBRIDGE1_RUST_SLICE
#define CXXBRIDGE1_RUST_SLICE
namespace detail {
template <bool>
struct copy_assignable_if {};

template <>
struct copy_assignable_if<false> {
  copy_assignable_if() noexcept = default;
  copy_assignable_if(const copy_assignable_if &) noexcept = default;
  copy_assignable_if &operator=(const copy_assignable_if &) &noexcept = delete;
  copy_assignable_if &operator=(copy_assignable_if &&) &noexcept = default;
};
} // namespace detail

template <typename T>
class Slice final
    : private detail::copy_assignable_if<std::is_const<T>::value> {
public:
  using value_type = T;

  Slice() noexcept;
  Slice(T *, std::size_t count) noexcept;

  Slice &operator=(const Slice<T> &) &noexcept = default;
  Slice &operator=(Slice<T> &&) &noexcept = default;

  T *data() const noexcept;
  std::size_t size() const noexcept;
  std::size_t length() const noexcept;
  bool empty() const noexcept;

  T &operator[](std::size_t n) const noexcept;
  T &at(std::size_t n) const;
  T &front() const noexcept;
  T &back() const noexcept;

  Slice(const Slice<T> &) noexcept = default;
  ~Slice() noexcept = default;

  class iterator;
  iterator begin() const noexcept;
  iterator end() const noexcept;

  void swap(Slice &) noexcept;

private:
  class uninit;
  Slice(uninit) noexcept;
  friend impl<Slice>;
  friend void sliceInit(void *, const void *, std::size_t) noexcept;
  friend void *slicePtr(const void *) noexcept;
  friend std::size_t sliceLen(const void *) noexcept;

  std::array<std::uintptr_t, 2> repr;
};

template <typename T>
class Slice<T>::iterator final {
public:
  using iterator_category = std::random_access_iterator_tag;
  using value_type = T;
  using difference_type = std::ptrdiff_t;
  using pointer = typename std::add_pointer<T>::type;
  using reference = typename std::add_lvalue_reference<T>::type;

  reference operator*() const noexcept;
  pointer operator->() const noexcept;
  reference operator[](difference_type) const noexcept;

  iterator &operator++() noexcept;
  iterator operator++(int) noexcept;
  iterator &operator--() noexcept;
  iterator operator--(int) noexcept;

  iterator &operator+=(difference_type) noexcept;
  iterator &operator-=(difference_type) noexcept;
  iterator operator+(difference_type) const noexcept;
  iterator operator-(difference_type) const noexcept;
  difference_type operator-(const iterator &) const noexcept;

  bool operator==(const iterator &) const noexcept;
  bool operator!=(const iterator &) const noexcept;
  bool operator<(const iterator &) const noexcept;
  bool operator<=(const iterator &) const noexcept;
  bool operator>(const iterator &) const noexcept;
  bool operator>=(const iterator &) const noexcept;

private:
  friend class Slice;
  void *pos;
  std::size_t stride;
};

template <typename T>
Slice<T>::Slice() noexcept {
  sliceInit(this, reinterpret_cast<void *>(align_of<T>()), 0);
}

template <typename T>
Slice<T>::Slice(T *s, std::size_t count) noexcept {
  assert(s != nullptr || count == 0);
  sliceInit(this,
            s == nullptr && count == 0
                ? reinterpret_cast<void *>(align_of<T>())
                : const_cast<typename std::remove_const<T>::type *>(s),
            count);
}

template <typename T>
T *Slice<T>::data() const noexcept {
  return reinterpret_cast<T *>(slicePtr(this));
}

template <typename T>
std::size_t Slice<T>::size() const noexcept {
  return sliceLen(this);
}

template <typename T>
std::size_t Slice<T>::length() const noexcept {
  return this->size();
}

template <typename T>
bool Slice<T>::empty() const noexcept {
  return this->size() == 0;
}

template <typename T>
T &Slice<T>::operator[](std::size_t n) const noexcept {
  assert(n < this->size());
  auto ptr = static_cast<char *>(slicePtr(this)) + size_of<T>() * n;
  return *reinterpret_cast<T *>(ptr);
}

template <typename T>
T &Slice<T>::at(std::size_t n) const {
  if (n >= this->size()) {
    panic<std::out_of_range>("rust::Slice index out of range");
  }
  return (*this)[n];
}

template <typename T>
T &Slice<T>::front() const noexcept {
  assert(!this->empty());
  return (*this)[0];
}

template <typename T>
T &Slice<T>::back() const noexcept {
  assert(!this->empty());
  return (*this)[this->size() - 1];
}

template <typename T>
typename Slice<T>::iterator::reference
Slice<T>::iterator::operator*() const noexcept {
  return *static_cast<T *>(this->pos);
}

template <typename T>
typename Slice<T>::iterator::pointer
Slice<T>::iterator::operator->() const noexcept {
  return static_cast<T *>(this->pos);
}

template <typename T>
typename Slice<T>::iterator::reference Slice<T>::iterator::operator[](
    typename Slice<T>::iterator::difference_type n) const noexcept {
  auto ptr = static_cast<char *>(this->pos) + this->stride * n;
  return *reinterpret_cast<T *>(ptr);
}

template <typename T>
typename Slice<T>::iterator &Slice<T>::iterator::operator++() noexcept {
  this->pos = static_cast<char *>(this->pos) + this->stride;
  return *this;
}

template <typename T>
typename Slice<T>::iterator Slice<T>::iterator::operator++(int) noexcept {
  auto ret = iterator(*this);
  this->pos = static_cast<char *>(this->pos) + this->stride;
  return ret;
}

template <typename T>
typename Slice<T>::iterator &Slice<T>::iterator::operator--() noexcept {
  this->pos = static_cast<char *>(this->pos) - this->stride;
  return *this;
}

template <typename T>
typename Slice<T>::iterator Slice<T>::iterator::operator--(int) noexcept {
  auto ret = iterator(*this);
  this->pos = static_cast<char *>(this->pos) - this->stride;
  return ret;
}

template <typename T>
typename Slice<T>::iterator &Slice<T>::iterator::operator+=(
    typename Slice<T>::iterator::difference_type n) noexcept {
  this->pos = static_cast<char *>(this->pos) + this->stride * n;
  return *this;
}

template <typename T>
typename Slice<T>::iterator &Slice<T>::iterator::operator-=(
    typename Slice<T>::iterator::difference_type n) noexcept {
  this->pos = static_cast<char *>(this->pos) - this->stride * n;
  return *this;
}

template <typename T>
typename Slice<T>::iterator Slice<T>::iterator::operator+(
    typename Slice<T>::iterator::difference_type n) const noexcept {
  auto ret = iterator(*this);
  ret.pos = static_cast<char *>(this->pos) + this->stride * n;
  return ret;
}

template <typename T>
typename Slice<T>::iterator Slice<T>::iterator::operator-(
    typename Slice<T>::iterator::difference_type n) const noexcept {
  auto ret = iterator(*this);
  ret.pos = static_cast<char *>(this->pos) - this->stride * n;
  return ret;
}

template <typename T>
typename Slice<T>::iterator::difference_type
Slice<T>::iterator::operator-(const iterator &other) const noexcept {
  auto diff = std::distance(static_cast<char *>(other.pos),
                            static_cast<char *>(this->pos));
  return diff / static_cast<typename Slice<T>::iterator::difference_type>(
                    this->stride);
}

template <typename T>
bool Slice<T>::iterator::operator==(const iterator &other) const noexcept {
  return this->pos == other.pos;
}

template <typename T>
bool Slice<T>::iterator::operator!=(const iterator &other) const noexcept {
  return this->pos != other.pos;
}

template <typename T>
bool Slice<T>::iterator::operator<(const iterator &other) const noexcept {
  return this->pos < other.pos;
}

template <typename T>
bool Slice<T>::iterator::operator<=(const iterator &other) const noexcept {
  return this->pos <= other.pos;
}

template <typename T>
bool Slice<T>::iterator::operator>(const iterator &other) const noexcept {
  return this->pos > other.pos;
}

template <typename T>
bool Slice<T>::iterator::operator>=(const iterator &other) const noexcept {
  return this->pos >= other.pos;
}

template <typename T>
typename Slice<T>::iterator Slice<T>::begin() const noexcept {
  iterator it;
  it.pos = slicePtr(this);
  it.stride = size_of<T>();
  return it;
}

template <typename T>
typename Slice<T>::iterator Slice<T>::end() const noexcept {
  iterator it = this->begin();
  it.pos = static_cast<char *>(it.pos) + it.stride * this->size();
  return it;
}

template <typename T>
void Slice<T>::swap(Slice &rhs) noexcept {
  std::swap(*this, rhs);
}
#endif // CXXBRIDGE1_RUST_SLICE

#ifndef CXXBRIDGE1_RUST_BITCOPY_T
#define CXXBRIDGE1_RUST_BITCOPY_T
struct unsafe_bitcopy_t final {
  explicit unsafe_bitcopy_t() = default;
};
#endif // CXXBRIDGE1_RUST_BITCOPY_T

#ifndef CXXBRIDGE1_RUST_BITCOPY
#define CXXBRIDGE1_RUST_BITCOPY
constexpr unsafe_bitcopy_t unsafe_bitcopy{};
#endif // CXXBRIDGE1_RUST_BITCOPY

#ifndef CXXBRIDGE1_RUST_VEC
#define CXXBRIDGE1_RUST_VEC
template <typename T>
class Vec final {
public:
  using value_type = T;

  Vec() noexcept;
  Vec(std::initializer_list<T>);
  Vec(const Vec &);
  Vec(Vec &&) noexcept;
  ~Vec() noexcept;

  Vec &operator=(Vec &&) &noexcept;
  Vec &operator=(const Vec &) &;

  std::size_t size() const noexcept;
  bool empty() const noexcept;
  const T *data() const noexcept;
  T *data() noexcept;
  std::size_t capacity() const noexcept;

  const T &operator[](std::size_t n) const noexcept;
  const T &at(std::size_t n) const;
  const T &front() const noexcept;
  const T &back() const noexcept;

  T &operator[](std::size_t n) noexcept;
  T &at(std::size_t n);
  T &front() noexcept;
  T &back() noexcept;

  void reserve(std::size_t new_cap);
  void push_back(const T &value);
  void push_back(T &&value);
  template <typename... Args>
  void emplace_back(Args &&...args);
  void truncate(std::size_t len);
  void clear();

  using iterator = typename Slice<T>::iterator;
  iterator begin() noexcept;
  iterator end() noexcept;

  using const_iterator = typename Slice<const T>::iterator;
  const_iterator begin() const noexcept;
  const_iterator end() const noexcept;
  const_iterator cbegin() const noexcept;
  const_iterator cend() const noexcept;

  void swap(Vec &) noexcept;

  Vec(unsafe_bitcopy_t, const Vec &) noexcept;

private:
  void reserve_total(std::size_t new_cap) noexcept;
  void set_len(std::size_t len) noexcept;
  void drop() noexcept;

  friend void swap(Vec &lhs, Vec &rhs) noexcept { lhs.swap(rhs); }

  std::array<std::uintptr_t, 3> repr;
};

template <typename T>
Vec<T>::Vec(std::initializer_list<T> init) : Vec{} {
  this->reserve_total(init.size());
  std::move(init.begin(), init.end(), std::back_inserter(*this));
}

template <typename T>
Vec<T>::Vec(const Vec &other) : Vec() {
  this->reserve_total(other.size());
  std::copy(other.begin(), other.end(), std::back_inserter(*this));
}

template <typename T>
Vec<T>::Vec(Vec &&other) noexcept : repr(other.repr) {
  new (&other) Vec();
}

template <typename T>
Vec<T>::~Vec() noexcept {
  this->drop();
}

template <typename T>
Vec<T> &Vec<T>::operator=(Vec &&other) &noexcept {
  this->drop();
  this->repr = other.repr;
  new (&other) Vec();
  return *this;
}

template <typename T>
Vec<T> &Vec<T>::operator=(const Vec &other) & {
  if (this != &other) {
    this->drop();
    new (this) Vec(other);
  }
  return *this;
}

template <typename T>
bool Vec<T>::empty() const noexcept {
  return this->size() == 0;
}

template <typename T>
T *Vec<T>::data() noexcept {
  return const_cast<T *>(const_cast<const Vec<T> *>(this)->data());
}

template <typename T>
const T &Vec<T>::operator[](std::size_t n) const noexcept {
  assert(n < this->size());
  auto data = reinterpret_cast<const char *>(this->data());
  return *reinterpret_cast<const T *>(data + n * size_of<T>());
}

template <typename T>
const T &Vec<T>::at(std::size_t n) const {
  if (n >= this->size()) {
    panic<std::out_of_range>("rust::Vec index out of range");
  }
  return (*this)[n];
}

template <typename T>
const T &Vec<T>::front() const noexcept {
  assert(!this->empty());
  return (*this)[0];
}

template <typename T>
const T &Vec<T>::back() const noexcept {
  assert(!this->empty());
  return (*this)[this->size() - 1];
}

template <typename T>
T &Vec<T>::operator[](std::size_t n) noexcept {
  assert(n < this->size());
  auto data = reinterpret_cast<char *>(this->data());
  return *reinterpret_cast<T *>(data + n * size_of<T>());
}

template <typename T>
T &Vec<T>::at(std::size_t n) {
  if (n >= this->size()) {
    panic<std::out_of_range>("rust::Vec index out of range");
  }
  return (*this)[n];
}

template <typename T>
T &Vec<T>::front() noexcept {
  assert(!this->empty());
  return (*this)[0];
}

template <typename T>
T &Vec<T>::back() noexcept {
  assert(!this->empty());
  return (*this)[this->size() - 1];
}

template <typename T>
void Vec<T>::reserve(std::size_t new_cap) {
  this->reserve_total(new_cap);
}

template <typename T>
void Vec<T>::push_back(const T &value) {
  this->emplace_back(value);
}

template <typename T>
void Vec<T>::push_back(T &&value) {
  this->emplace_back(std::move(value));
}

template <typename T>
template <typename... Args>
void Vec<T>::emplace_back(Args &&...args) {
  auto size = this->size();
  this->reserve_total(size + 1);
  ::new (reinterpret_cast<T *>(reinterpret_cast<char *>(this->data()) +
                               size * size_of<T>()))
      T(std::forward<Args>(args)...);
  this->set_len(size + 1);
}

template <typename T>
void Vec<T>::clear() {
  this->truncate(0);
}

template <typename T>
typename Vec<T>::iterator Vec<T>::begin() noexcept {
  return Slice<T>(this->data(), this->size()).begin();
}

template <typename T>
typename Vec<T>::iterator Vec<T>::end() noexcept {
  return Slice<T>(this->data(), this->size()).end();
}

template <typename T>
typename Vec<T>::const_iterator Vec<T>::begin() const noexcept {
  return this->cbegin();
}

template <typename T>
typename Vec<T>::const_iterator Vec<T>::end() const noexcept {
  return this->cend();
}

template <typename T>
typename Vec<T>::const_iterator Vec<T>::cbegin() const noexcept {
  return Slice<const T>(this->data(), this->size()).begin();
}

template <typename T>
typename Vec<T>::const_iterator Vec<T>::cend() const noexcept {
  return Slice<const T>(this->data(), this->size()).end();
}

template <typename T>
void Vec<T>::swap(Vec &rhs) noexcept {
  using std::swap;
  swap(this->repr, rhs.repr);
}

template <typename T>
Vec<T>::Vec(unsafe_bitcopy_t, const Vec &bits) noexcept : repr(bits.repr) {}
#endif // CXXBRIDGE1_RUST_VEC

#ifndef CXXBRIDGE1_IS_COMPLETE
#define CXXBRIDGE1_IS_COMPLETE
namespace detail {
namespace {
template <typename T, typename = std::size_t>
struct is_complete : std::false_type {};
template <typename T>
struct is_complete<T, decltype(sizeof(T))> : std::true_type {};
} // namespace
} // namespace detail
#endif // CXXBRIDGE1_IS_COMPLETE

#ifndef CXXBRIDGE1_LAYOUT
#define CXXBRIDGE1_LAYOUT
class layout {
  template <typename T>
  friend std::size_t size_of();
  template <typename T>
  friend std::size_t align_of();
  template <typename T>
  static typename std::enable_if<std::is_base_of<Opaque, T>::value,
                                 std::size_t>::type
  do_size_of() {
    return T::layout::size();
  }
  template <typename T>
  static typename std::enable_if<!std::is_base_of<Opaque, T>::value,
                                 std::size_t>::type
  do_size_of() {
    return sizeof(T);
  }
  template <typename T>
  static
      typename std::enable_if<detail::is_complete<T>::value, std::size_t>::type
      size_of() {
    return do_size_of<T>();
  }
  template <typename T>
  static typename std::enable_if<std::is_base_of<Opaque, T>::value,
                                 std::size_t>::type
  do_align_of() {
    return T::layout::align();
  }
  template <typename T>
  static typename std::enable_if<!std::is_base_of<Opaque, T>::value,
                                 std::size_t>::type
  do_align_of() {
    return alignof(T);
  }
  template <typename T>
  static
      typename std::enable_if<detail::is_complete<T>::value, std::size_t>::type
      align_of() {
    return do_align_of<T>();
  }
};

template <typename T>
std::size_t size_of() {
  return layout::size_of<T>();
}

template <typename T>
std::size_t align_of() {
  return layout::align_of<T>();
}
#endif // CXXBRIDGE1_LAYOUT

namespace detail {
template <typename T, typename = void *>
struct operator_new {
  void *operator()(::std::size_t sz) { return ::operator new(sz); }
};

template <typename T>
struct operator_new<T, decltype(T::operator new(sizeof(T)))> {
  void *operator()(::std::size_t sz) { return T::operator new(sz); }
};
} // namespace detail

template <typename T>
union MaybeUninit {
  T value;
  void *operator new(::std::size_t sz) { return detail::operator_new<T>{}(sz); }
  MaybeUninit() {}
  ~MaybeUninit() {}
};
} // namespace cxxbridge1
} // namespace rust

namespace lnav_rs_ext {
  struct ExtError;
  enum class Status : ::std::uint8_t;
  struct ExtProgress;
  struct Options;
  struct SourceTreeElement;
  enum class MessageKind : ::std::uint8_t;
  struct Message;
  struct CompileResult2;
  struct FindLogResultJson;
  struct VarPair;
  struct SourceDetails;
  struct FindLogResult;
  struct ViewStates;
  struct PollInput;
  struct PollResult;
  struct ExecError;
  struct ExecResult;
  enum class LnavLogLevel : ::std::uint8_t;
  struct StartExtResult;
}

namespace lnav_rs_ext {
#ifndef CXXBRIDGE1_STRUCT_lnav_rs_ext$ExtError
#define CXXBRIDGE1_STRUCT_lnav_rs_ext$ExtError
struct ExtError final {
  ::rust::String error;
  ::rust::String source;
  ::rust::String help;

  using IsRelocatable = ::std::true_type;
};
#endif // CXXBRIDGE1_STRUCT_lnav_rs_ext$ExtError

#ifndef CXXBRIDGE1_ENUM_lnav_rs_ext$Status
#define CXXBRIDGE1_ENUM_lnav_rs_ext$Status
enum class Status : ::std::uint8_t {
  idle = 0,
  working = 1,
};
#endif // CXXBRIDGE1_ENUM_lnav_rs_ext$Status

#ifndef CXXBRIDGE1_STRUCT_lnav_rs_ext$ExtProgress
#define CXXBRIDGE1_STRUCT_lnav_rs_ext$ExtProgress
struct ExtProgress final {
  ::rust::String id;
  ::lnav_rs_ext::Status status;
  ::std::size_t version;
  ::rust::String current_step;
  ::std::uint64_t completed;
  ::std::uint64_t total;
  ::rust::Vec<::lnav_rs_ext::ExtError> messages;

  using IsRelocatable = ::std::true_type;
};
#endif // CXXBRIDGE1_STRUCT_lnav_rs_ext$ExtProgress

#ifndef CXXBRIDGE1_STRUCT_lnav_rs_ext$Options
#define CXXBRIDGE1_STRUCT_lnav_rs_ext$Options
struct Options final {
  bool format;
  ::rust::String target;
  bool signature_comment;

  using IsRelocatable = ::std::true_type;
};
#endif // CXXBRIDGE1_STRUCT_lnav_rs_ext$Options

#ifndef CXXBRIDGE1_STRUCT_lnav_rs_ext$SourceTreeElement
#define CXXBRIDGE1_STRUCT_lnav_rs_ext$SourceTreeElement
struct SourceTreeElement final {
  ::rust::String path;
  ::rust::String content;

  using IsRelocatable = ::std::true_type;
};
#endif // CXXBRIDGE1_STRUCT_lnav_rs_ext$SourceTreeElement

#ifndef CXXBRIDGE1_ENUM_lnav_rs_ext$MessageKind
#define CXXBRIDGE1_ENUM_lnav_rs_ext$MessageKind
enum class MessageKind : ::std::uint8_t {
  Error = 0,
  Warning = 1,
  Lint = 2,
};
#endif // CXXBRIDGE1_ENUM_lnav_rs_ext$MessageKind

#ifndef CXXBRIDGE1_STRUCT_lnav_rs_ext$Message
#define CXXBRIDGE1_STRUCT_lnav_rs_ext$Message
struct Message final {
  ::lnav_rs_ext::MessageKind kind;
  ::rust::String code;
  ::rust::String reason;
  ::rust::Vec<::rust::String> hints;
  ::rust::String display;

  using IsRelocatable = ::std::true_type;
};
#endif // CXXBRIDGE1_STRUCT_lnav_rs_ext$Message

#ifndef CXXBRIDGE1_STRUCT_lnav_rs_ext$CompileResult2
#define CXXBRIDGE1_STRUCT_lnav_rs_ext$CompileResult2
struct CompileResult2 final {
  ::rust::String output;
  ::rust::Vec<::lnav_rs_ext::Message> messages;

  using IsRelocatable = ::std::true_type;
};
#endif // CXXBRIDGE1_STRUCT_lnav_rs_ext$CompileResult2

#ifndef CXXBRIDGE1_STRUCT_lnav_rs_ext$FindLogResultJson
#define CXXBRIDGE1_STRUCT_lnav_rs_ext$FindLogResultJson
struct FindLogResultJson final {
  ::rust::String src;
  ::rust::String pattern;
  ::rust::String variables;
  ::rust::String stack_trace;

  using IsRelocatable = ::std::true_type;
};
#endif // CXXBRIDGE1_STRUCT_lnav_rs_ext$FindLogResultJson

#ifndef CXXBRIDGE1_STRUCT_lnav_rs_ext$VarPair
#define CXXBRIDGE1_STRUCT_lnav_rs_ext$VarPair
struct VarPair final {
  ::rust::String expr;
  ::rust::String value;

  using IsRelocatable = ::std::true_type;
};
#endif // CXXBRIDGE1_STRUCT_lnav_rs_ext$VarPair

#ifndef CXXBRIDGE1_STRUCT_lnav_rs_ext$SourceDetails
#define CXXBRIDGE1_STRUCT_lnav_rs_ext$SourceDetails
struct SourceDetails final {
  ::rust::String file;
  ::std::size_t begin_line;
  ::std::size_t end_line;
  ::rust::String name;
  ::rust::Str language;

  using IsRelocatable = ::std::true_type;
};
#endif // CXXBRIDGE1_STRUCT_lnav_rs_ext$SourceDetails

#ifndef CXXBRIDGE1_STRUCT_lnav_rs_ext$FindLogResult
#define CXXBRIDGE1_STRUCT_lnav_rs_ext$FindLogResult
struct FindLogResult final {
  ::lnav_rs_ext::SourceDetails src;
  ::rust::String pattern;
  ::rust::Vec<::lnav_rs_ext::VarPair> variables;

  using IsRelocatable = ::std::true_type;
};
#endif // CXXBRIDGE1_STRUCT_lnav_rs_ext$FindLogResult

#ifndef CXXBRIDGE1_STRUCT_lnav_rs_ext$ViewStates
#define CXXBRIDGE1_STRUCT_lnav_rs_ext$ViewStates
struct ViewStates final {
  ::rust::String log;
  ::rust::String log_selection;
  ::rust::String text;

  using IsRelocatable = ::std::true_type;
};
#endif // CXXBRIDGE1_STRUCT_lnav_rs_ext$ViewStates

#ifndef CXXBRIDGE1_STRUCT_lnav_rs_ext$PollInput
#define CXXBRIDGE1_STRUCT_lnav_rs_ext$PollInput
struct PollInput final {
  ::std::size_t last_event_id;
  ::lnav_rs_ext::ViewStates view_states;
  ::rust::Vec<::std::size_t> task_states;

  using IsRelocatable = ::std::true_type;
};
#endif // CXXBRIDGE1_STRUCT_lnav_rs_ext$PollInput

#ifndef CXXBRIDGE1_STRUCT_lnav_rs_ext$PollResult
#define CXXBRIDGE1_STRUCT_lnav_rs_ext$PollResult
struct PollResult final {
  ::lnav_rs_ext::PollInput next_input;
  ::rust::Vec<::lnav_rs_ext::ExtProgress> background_tasks;

  using IsRelocatable = ::std::true_type;
};
#endif // CXXBRIDGE1_STRUCT_lnav_rs_ext$PollResult

#ifndef CXXBRIDGE1_STRUCT_lnav_rs_ext$ExecError
#define CXXBRIDGE1_STRUCT_lnav_rs_ext$ExecError
struct ExecError final {
  ::rust::String msg;
  ::rust::String reason;
  ::rust::String help;

  using IsRelocatable = ::std::true_type;
};
#endif // CXXBRIDGE1_STRUCT_lnav_rs_ext$ExecError

#ifndef CXXBRIDGE1_STRUCT_lnav_rs_ext$ExecResult
#define CXXBRIDGE1_STRUCT_lnav_rs_ext$ExecResult
struct ExecResult final {
  ::rust::String status;
  ::rust::String content_type;
  ::std::int32_t content_fd;
  ::lnav_rs_ext::ExecError error;

  using IsRelocatable = ::std::true_type;
};
#endif // CXXBRIDGE1_STRUCT_lnav_rs_ext$ExecResult

#ifndef CXXBRIDGE1_ENUM_lnav_rs_ext$LnavLogLevel
#define CXXBRIDGE1_ENUM_lnav_rs_ext$LnavLogLevel
enum class LnavLogLevel : ::std::uint8_t {
  trace = 0,
  debug = 1,
  info = 2,
  warning = 3,
  error = 4,
};
#endif // CXXBRIDGE1_ENUM_lnav_rs_ext$LnavLogLevel

#ifndef CXXBRIDGE1_STRUCT_lnav_rs_ext$StartExtResult
#define CXXBRIDGE1_STRUCT_lnav_rs_ext$StartExtResult
struct StartExtResult final {
  ::std::uint16_t port;
  ::rust::String error;

  using IsRelocatable = ::std::true_type;
};
#endif // CXXBRIDGE1_STRUCT_lnav_rs_ext$StartExtResult

extern "C" {
void lnav_rs_ext$cxxbridge1$version_info(::rust::String *return$) noexcept {
  ::rust::String (*version_info$)() = ::lnav_rs_ext::version_info;
  new (return$) ::rust::String(version_info$());
}

void lnav_rs_ext$cxxbridge1$longpoll(::lnav_rs_ext::PollInput const &poll_inpout, ::lnav_rs_ext::PollResult *return$) noexcept {
  ::lnav_rs_ext::PollResult (*longpoll$)(::lnav_rs_ext::PollInput const &) = ::lnav_rs_ext::longpoll;
  new (return$) ::lnav_rs_ext::PollResult(longpoll$(poll_inpout));
}

void lnav_rs_ext$cxxbridge1$notify_pollers() noexcept {
  void (*notify_pollers$)() = ::lnav_rs_ext::notify_pollers;
  notify_pollers$();
}

void lnav_rs_ext$cxxbridge1$execute_external_command(::rust::String const *src, ::rust::String const *cmd, ::rust::String const *hdrs, ::lnav_rs_ext::ExecResult *return$) noexcept {
  ::lnav_rs_ext::ExecResult (*execute_external_command$)(::rust::String, ::rust::String, ::rust::String) = ::lnav_rs_ext::execute_external_command;
  new (return$) ::lnav_rs_ext::ExecResult(execute_external_command$(::rust::String(::rust::unsafe_bitcopy, *src), ::rust::String(::rust::unsafe_bitcopy, *cmd), ::rust::String(::rust::unsafe_bitcopy, *hdrs)));
}

::lnav_rs_ext::LnavLogLevel lnav_rs_ext$cxxbridge1$get_lnav_log_level() noexcept {
  ::lnav_rs_ext::LnavLogLevel (*get_lnav_log_level$)() = ::lnav_rs_ext::get_lnav_log_level;
  return get_lnav_log_level$();
}

void lnav_rs_ext$cxxbridge1$log_msg(::lnav_rs_ext::LnavLogLevel level, ::rust::Str file, ::std::uint32_t line, ::rust::Str msg) noexcept {
  void (*log_msg$)(::lnav_rs_ext::LnavLogLevel, ::rust::Str, ::std::uint32_t, ::rust::Str) = ::lnav_rs_ext::log_msg;
  log_msg$(level, file, line, msg);
}

void lnav_rs_ext$cxxbridge1$init_ext() noexcept;

void lnav_rs_ext$cxxbridge1$compile_tree(::rust::Vec<::lnav_rs_ext::SourceTreeElement> const &tree, ::lnav_rs_ext::Options const &options, ::lnav_rs_ext::CompileResult2 *return$) noexcept;

::lnav_rs_ext::ExtError *lnav_rs_ext$cxxbridge1$add_src_root(::rust::String *path) noexcept;

void lnav_rs_ext$cxxbridge1$discover_srcs() noexcept;

void lnav_rs_ext$cxxbridge1$get_status(::lnav_rs_ext::ExtProgress *return$) noexcept;

::lnav_rs_ext::FindLogResult *lnav_rs_ext$cxxbridge1$find_log_statement(::rust::Str file, ::std::size_t line, ::rust::Str body) noexcept;

::lnav_rs_ext::FindLogResultJson *lnav_rs_ext$cxxbridge1$find_log_statement_json(::rust::Str file, ::std::size_t line, ::rust::Str body) noexcept;

void lnav_rs_ext$cxxbridge1$get_log_statements_for(::rust::Str file, ::rust::Vec<::lnav_rs_ext::FindLogResult> *return$) noexcept;

void lnav_rs_ext$cxxbridge1$start_ext_access(::std::uint16_t port, ::rust::String *api_key, ::lnav_rs_ext::StartExtResult *return$) noexcept;

void lnav_rs_ext$cxxbridge1$stop_ext_access() noexcept;
} // extern "C"

void init_ext() noexcept {
  lnav_rs_ext$cxxbridge1$init_ext();
}

::lnav_rs_ext::CompileResult2 compile_tree(::rust::Vec<::lnav_rs_ext::SourceTreeElement> const &tree, ::lnav_rs_ext::Options const &options) noexcept {
  ::rust::MaybeUninit<::lnav_rs_ext::CompileResult2> return$;
  lnav_rs_ext$cxxbridge1$compile_tree(tree, options, &return$.value);
  return ::std::move(return$.value);
}

::std::unique_ptr<::lnav_rs_ext::ExtError> add_src_root(::rust::String path) noexcept {
  return ::std::unique_ptr<::lnav_rs_ext::ExtError>(lnav_rs_ext$cxxbridge1$add_src_root(&path));
}

void discover_srcs() noexcept {
  lnav_rs_ext$cxxbridge1$discover_srcs();
}

::lnav_rs_ext::ExtProgress get_status() noexcept {
  ::rust::MaybeUninit<::lnav_rs_ext::ExtProgress> return$;
  lnav_rs_ext$cxxbridge1$get_status(&return$.value);
  return ::std::move(return$.value);
}

::std::unique_ptr<::lnav_rs_ext::FindLogResult> find_log_statement(::rust::Str file, ::std::size_t line, ::rust::Str body) noexcept {
  return ::std::unique_ptr<::lnav_rs_ext::FindLogResult>(lnav_rs_ext$cxxbridge1$find_log_statement(file, line, body));
}

::std::unique_ptr<::lnav_rs_ext::FindLogResultJson> find_log_statement_json(::rust::Str file, ::std::size_t line, ::rust::Str body) noexcept {
  return ::std::unique_ptr<::lnav_rs_ext::FindLogResultJson>(lnav_rs_ext$cxxbridge1$find_log_statement_json(file, line, body));
}

::rust::Vec<::lnav_rs_ext::FindLogResult> get_log_statements_for(::rust::Str file) noexcept {
  ::rust::MaybeUninit<::rust::Vec<::lnav_rs_ext::FindLogResult>> return$;
  lnav_rs_ext$cxxbridge1$get_log_statements_for(file, &return$.value);
  return ::std::move(return$.value);
}

::lnav_rs_ext::StartExtResult start_ext_access(::std::uint16_t port, ::rust::String api_key) noexcept {
  ::rust::MaybeUninit<::lnav_rs_ext::StartExtResult> return$;
  lnav_rs_ext$cxxbridge1$start_ext_access(port, &api_key, &return$.value);
  return ::std::move(return$.value);
}

void stop_ext_access() noexcept {
  lnav_rs_ext$cxxbridge1$stop_ext_access();
}
} // namespace lnav_rs_ext

extern "C" {
void cxxbridge1$rust_vec$lnav_rs_ext$ExtError$new(::rust::Vec<::lnav_rs_ext::ExtError> const *ptr) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$ExtError$drop(::rust::Vec<::lnav_rs_ext::ExtError> *ptr) noexcept;
::std::size_t cxxbridge1$rust_vec$lnav_rs_ext$ExtError$len(::rust::Vec<::lnav_rs_ext::ExtError> const *ptr) noexcept;
::std::size_t cxxbridge1$rust_vec$lnav_rs_ext$ExtError$capacity(::rust::Vec<::lnav_rs_ext::ExtError> const *ptr) noexcept;
::lnav_rs_ext::ExtError const *cxxbridge1$rust_vec$lnav_rs_ext$ExtError$data(::rust::Vec<::lnav_rs_ext::ExtError> const *ptr) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$ExtError$reserve_total(::rust::Vec<::lnav_rs_ext::ExtError> *ptr, ::std::size_t new_cap) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$ExtError$set_len(::rust::Vec<::lnav_rs_ext::ExtError> *ptr, ::std::size_t len) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$ExtError$truncate(::rust::Vec<::lnav_rs_ext::ExtError> *ptr, ::std::size_t len) noexcept;

void cxxbridge1$rust_vec$lnav_rs_ext$Message$new(::rust::Vec<::lnav_rs_ext::Message> const *ptr) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$Message$drop(::rust::Vec<::lnav_rs_ext::Message> *ptr) noexcept;
::std::size_t cxxbridge1$rust_vec$lnav_rs_ext$Message$len(::rust::Vec<::lnav_rs_ext::Message> const *ptr) noexcept;
::std::size_t cxxbridge1$rust_vec$lnav_rs_ext$Message$capacity(::rust::Vec<::lnav_rs_ext::Message> const *ptr) noexcept;
::lnav_rs_ext::Message const *cxxbridge1$rust_vec$lnav_rs_ext$Message$data(::rust::Vec<::lnav_rs_ext::Message> const *ptr) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$Message$reserve_total(::rust::Vec<::lnav_rs_ext::Message> *ptr, ::std::size_t new_cap) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$Message$set_len(::rust::Vec<::lnav_rs_ext::Message> *ptr, ::std::size_t len) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$Message$truncate(::rust::Vec<::lnav_rs_ext::Message> *ptr, ::std::size_t len) noexcept;

void cxxbridge1$rust_vec$lnav_rs_ext$VarPair$new(::rust::Vec<::lnav_rs_ext::VarPair> const *ptr) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$VarPair$drop(::rust::Vec<::lnav_rs_ext::VarPair> *ptr) noexcept;
::std::size_t cxxbridge1$rust_vec$lnav_rs_ext$VarPair$len(::rust::Vec<::lnav_rs_ext::VarPair> const *ptr) noexcept;
::std::size_t cxxbridge1$rust_vec$lnav_rs_ext$VarPair$capacity(::rust::Vec<::lnav_rs_ext::VarPair> const *ptr) noexcept;
::lnav_rs_ext::VarPair const *cxxbridge1$rust_vec$lnav_rs_ext$VarPair$data(::rust::Vec<::lnav_rs_ext::VarPair> const *ptr) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$VarPair$reserve_total(::rust::Vec<::lnav_rs_ext::VarPair> *ptr, ::std::size_t new_cap) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$VarPair$set_len(::rust::Vec<::lnav_rs_ext::VarPair> *ptr, ::std::size_t len) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$VarPair$truncate(::rust::Vec<::lnav_rs_ext::VarPair> *ptr, ::std::size_t len) noexcept;

void cxxbridge1$rust_vec$lnav_rs_ext$ExtProgress$new(::rust::Vec<::lnav_rs_ext::ExtProgress> const *ptr) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$ExtProgress$drop(::rust::Vec<::lnav_rs_ext::ExtProgress> *ptr) noexcept;
::std::size_t cxxbridge1$rust_vec$lnav_rs_ext$ExtProgress$len(::rust::Vec<::lnav_rs_ext::ExtProgress> const *ptr) noexcept;
::std::size_t cxxbridge1$rust_vec$lnav_rs_ext$ExtProgress$capacity(::rust::Vec<::lnav_rs_ext::ExtProgress> const *ptr) noexcept;
::lnav_rs_ext::ExtProgress const *cxxbridge1$rust_vec$lnav_rs_ext$ExtProgress$data(::rust::Vec<::lnav_rs_ext::ExtProgress> const *ptr) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$ExtProgress$reserve_total(::rust::Vec<::lnav_rs_ext::ExtProgress> *ptr, ::std::size_t new_cap) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$ExtProgress$set_len(::rust::Vec<::lnav_rs_ext::ExtProgress> *ptr, ::std::size_t len) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$ExtProgress$truncate(::rust::Vec<::lnav_rs_ext::ExtProgress> *ptr, ::std::size_t len) noexcept;

void cxxbridge1$rust_vec$lnav_rs_ext$SourceTreeElement$new(::rust::Vec<::lnav_rs_ext::SourceTreeElement> const *ptr) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$SourceTreeElement$drop(::rust::Vec<::lnav_rs_ext::SourceTreeElement> *ptr) noexcept;
::std::size_t cxxbridge1$rust_vec$lnav_rs_ext$SourceTreeElement$len(::rust::Vec<::lnav_rs_ext::SourceTreeElement> const *ptr) noexcept;
::std::size_t cxxbridge1$rust_vec$lnav_rs_ext$SourceTreeElement$capacity(::rust::Vec<::lnav_rs_ext::SourceTreeElement> const *ptr) noexcept;
::lnav_rs_ext::SourceTreeElement const *cxxbridge1$rust_vec$lnav_rs_ext$SourceTreeElement$data(::rust::Vec<::lnav_rs_ext::SourceTreeElement> const *ptr) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$SourceTreeElement$reserve_total(::rust::Vec<::lnav_rs_ext::SourceTreeElement> *ptr, ::std::size_t new_cap) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$SourceTreeElement$set_len(::rust::Vec<::lnav_rs_ext::SourceTreeElement> *ptr, ::std::size_t len) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$SourceTreeElement$truncate(::rust::Vec<::lnav_rs_ext::SourceTreeElement> *ptr, ::std::size_t len) noexcept;

static_assert(sizeof(::std::unique_ptr<::lnav_rs_ext::ExtError>) == sizeof(void *), "");
static_assert(alignof(::std::unique_ptr<::lnav_rs_ext::ExtError>) == alignof(void *), "");
void cxxbridge1$unique_ptr$lnav_rs_ext$ExtError$null(::std::unique_ptr<::lnav_rs_ext::ExtError> *ptr) noexcept {
  ::new (ptr) ::std::unique_ptr<::lnav_rs_ext::ExtError>();
}
::lnav_rs_ext::ExtError *cxxbridge1$unique_ptr$lnav_rs_ext$ExtError$uninit(::std::unique_ptr<::lnav_rs_ext::ExtError> *ptr) noexcept {
  ::lnav_rs_ext::ExtError *uninit = reinterpret_cast<::lnav_rs_ext::ExtError *>(new ::rust::MaybeUninit<::lnav_rs_ext::ExtError>);
  ::new (ptr) ::std::unique_ptr<::lnav_rs_ext::ExtError>(uninit);
  return uninit;
}
void cxxbridge1$unique_ptr$lnav_rs_ext$ExtError$raw(::std::unique_ptr<::lnav_rs_ext::ExtError> *ptr, ::lnav_rs_ext::ExtError *raw) noexcept {
  ::new (ptr) ::std::unique_ptr<::lnav_rs_ext::ExtError>(raw);
}
::lnav_rs_ext::ExtError const *cxxbridge1$unique_ptr$lnav_rs_ext$ExtError$get(::std::unique_ptr<::lnav_rs_ext::ExtError> const &ptr) noexcept {
  return ptr.get();
}
::lnav_rs_ext::ExtError *cxxbridge1$unique_ptr$lnav_rs_ext$ExtError$release(::std::unique_ptr<::lnav_rs_ext::ExtError> &ptr) noexcept {
  return ptr.release();
}
void cxxbridge1$unique_ptr$lnav_rs_ext$ExtError$drop(::std::unique_ptr<::lnav_rs_ext::ExtError> *ptr) noexcept {
  ptr->~unique_ptr();
}

static_assert(sizeof(::std::unique_ptr<::lnav_rs_ext::FindLogResult>) == sizeof(void *), "");
static_assert(alignof(::std::unique_ptr<::lnav_rs_ext::FindLogResult>) == alignof(void *), "");
void cxxbridge1$unique_ptr$lnav_rs_ext$FindLogResult$null(::std::unique_ptr<::lnav_rs_ext::FindLogResult> *ptr) noexcept {
  ::new (ptr) ::std::unique_ptr<::lnav_rs_ext::FindLogResult>();
}
::lnav_rs_ext::FindLogResult *cxxbridge1$unique_ptr$lnav_rs_ext$FindLogResult$uninit(::std::unique_ptr<::lnav_rs_ext::FindLogResult> *ptr) noexcept {
  ::lnav_rs_ext::FindLogResult *uninit = reinterpret_cast<::lnav_rs_ext::FindLogResult *>(new ::rust::MaybeUninit<::lnav_rs_ext::FindLogResult>);
  ::new (ptr) ::std::unique_ptr<::lnav_rs_ext::FindLogResult>(uninit);
  return uninit;
}
void cxxbridge1$unique_ptr$lnav_rs_ext$FindLogResult$raw(::std::unique_ptr<::lnav_rs_ext::FindLogResult> *ptr, ::lnav_rs_ext::FindLogResult *raw) noexcept {
  ::new (ptr) ::std::unique_ptr<::lnav_rs_ext::FindLogResult>(raw);
}
::lnav_rs_ext::FindLogResult const *cxxbridge1$unique_ptr$lnav_rs_ext$FindLogResult$get(::std::unique_ptr<::lnav_rs_ext::FindLogResult> const &ptr) noexcept {
  return ptr.get();
}
::lnav_rs_ext::FindLogResult *cxxbridge1$unique_ptr$lnav_rs_ext$FindLogResult$release(::std::unique_ptr<::lnav_rs_ext::FindLogResult> &ptr) noexcept {
  return ptr.release();
}
void cxxbridge1$unique_ptr$lnav_rs_ext$FindLogResult$drop(::std::unique_ptr<::lnav_rs_ext::FindLogResult> *ptr) noexcept {
  ptr->~unique_ptr();
}

static_assert(sizeof(::std::unique_ptr<::lnav_rs_ext::FindLogResultJson>) == sizeof(void *), "");
static_assert(alignof(::std::unique_ptr<::lnav_rs_ext::FindLogResultJson>) == alignof(void *), "");
void cxxbridge1$unique_ptr$lnav_rs_ext$FindLogResultJson$null(::std::unique_ptr<::lnav_rs_ext::FindLogResultJson> *ptr) noexcept {
  ::new (ptr) ::std::unique_ptr<::lnav_rs_ext::FindLogResultJson>();
}
::lnav_rs_ext::FindLogResultJson *cxxbridge1$unique_ptr$lnav_rs_ext$FindLogResultJson$uninit(::std::unique_ptr<::lnav_rs_ext::FindLogResultJson> *ptr) noexcept {
  ::lnav_rs_ext::FindLogResultJson *uninit = reinterpret_cast<::lnav_rs_ext::FindLogResultJson *>(new ::rust::MaybeUninit<::lnav_rs_ext::FindLogResultJson>);
  ::new (ptr) ::std::unique_ptr<::lnav_rs_ext::FindLogResultJson>(uninit);
  return uninit;
}
void cxxbridge1$unique_ptr$lnav_rs_ext$FindLogResultJson$raw(::std::unique_ptr<::lnav_rs_ext::FindLogResultJson> *ptr, ::lnav_rs_ext::FindLogResultJson *raw) noexcept {
  ::new (ptr) ::std::unique_ptr<::lnav_rs_ext::FindLogResultJson>(raw);
}
::lnav_rs_ext::FindLogResultJson const *cxxbridge1$unique_ptr$lnav_rs_ext$FindLogResultJson$get(::std::unique_ptr<::lnav_rs_ext::FindLogResultJson> const &ptr) noexcept {
  return ptr.get();
}
::lnav_rs_ext::FindLogResultJson *cxxbridge1$unique_ptr$lnav_rs_ext$FindLogResultJson$release(::std::unique_ptr<::lnav_rs_ext::FindLogResultJson> &ptr) noexcept {
  return ptr.release();
}
void cxxbridge1$unique_ptr$lnav_rs_ext$FindLogResultJson$drop(::std::unique_ptr<::lnav_rs_ext::FindLogResultJson> *ptr) noexcept {
  ptr->~unique_ptr();
}

void cxxbridge1$rust_vec$lnav_rs_ext$FindLogResult$new(::rust::Vec<::lnav_rs_ext::FindLogResult> const *ptr) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$FindLogResult$drop(::rust::Vec<::lnav_rs_ext::FindLogResult> *ptr) noexcept;
::std::size_t cxxbridge1$rust_vec$lnav_rs_ext$FindLogResult$len(::rust::Vec<::lnav_rs_ext::FindLogResult> const *ptr) noexcept;
::std::size_t cxxbridge1$rust_vec$lnav_rs_ext$FindLogResult$capacity(::rust::Vec<::lnav_rs_ext::FindLogResult> const *ptr) noexcept;
::lnav_rs_ext::FindLogResult const *cxxbridge1$rust_vec$lnav_rs_ext$FindLogResult$data(::rust::Vec<::lnav_rs_ext::FindLogResult> const *ptr) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$FindLogResult$reserve_total(::rust::Vec<::lnav_rs_ext::FindLogResult> *ptr, ::std::size_t new_cap) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$FindLogResult$set_len(::rust::Vec<::lnav_rs_ext::FindLogResult> *ptr, ::std::size_t len) noexcept;
void cxxbridge1$rust_vec$lnav_rs_ext$FindLogResult$truncate(::rust::Vec<::lnav_rs_ext::FindLogResult> *ptr, ::std::size_t len) noexcept;
} // extern "C"

namespace rust {
inline namespace cxxbridge1 {
template <>
Vec<::lnav_rs_ext::ExtError>::Vec() noexcept {
  cxxbridge1$rust_vec$lnav_rs_ext$ExtError$new(this);
}
template <>
void Vec<::lnav_rs_ext::ExtError>::drop() noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$ExtError$drop(this);
}
template <>
::std::size_t Vec<::lnav_rs_ext::ExtError>::size() const noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$ExtError$len(this);
}
template <>
::std::size_t Vec<::lnav_rs_ext::ExtError>::capacity() const noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$ExtError$capacity(this);
}
template <>
::lnav_rs_ext::ExtError const *Vec<::lnav_rs_ext::ExtError>::data() const noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$ExtError$data(this);
}
template <>
void Vec<::lnav_rs_ext::ExtError>::reserve_total(::std::size_t new_cap) noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$ExtError$reserve_total(this, new_cap);
}
template <>
void Vec<::lnav_rs_ext::ExtError>::set_len(::std::size_t len) noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$ExtError$set_len(this, len);
}
template <>
void Vec<::lnav_rs_ext::ExtError>::truncate(::std::size_t len) {
  return cxxbridge1$rust_vec$lnav_rs_ext$ExtError$truncate(this, len);
}
template <>
Vec<::lnav_rs_ext::Message>::Vec() noexcept {
  cxxbridge1$rust_vec$lnav_rs_ext$Message$new(this);
}
template <>
void Vec<::lnav_rs_ext::Message>::drop() noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$Message$drop(this);
}
template <>
::std::size_t Vec<::lnav_rs_ext::Message>::size() const noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$Message$len(this);
}
template <>
::std::size_t Vec<::lnav_rs_ext::Message>::capacity() const noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$Message$capacity(this);
}
template <>
::lnav_rs_ext::Message const *Vec<::lnav_rs_ext::Message>::data() const noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$Message$data(this);
}
template <>
void Vec<::lnav_rs_ext::Message>::reserve_total(::std::size_t new_cap) noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$Message$reserve_total(this, new_cap);
}
template <>
void Vec<::lnav_rs_ext::Message>::set_len(::std::size_t len) noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$Message$set_len(this, len);
}
template <>
void Vec<::lnav_rs_ext::Message>::truncate(::std::size_t len) {
  return cxxbridge1$rust_vec$lnav_rs_ext$Message$truncate(this, len);
}
template <>
Vec<::lnav_rs_ext::VarPair>::Vec() noexcept {
  cxxbridge1$rust_vec$lnav_rs_ext$VarPair$new(this);
}
template <>
void Vec<::lnav_rs_ext::VarPair>::drop() noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$VarPair$drop(this);
}
template <>
::std::size_t Vec<::lnav_rs_ext::VarPair>::size() const noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$VarPair$len(this);
}
template <>
::std::size_t Vec<::lnav_rs_ext::VarPair>::capacity() const noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$VarPair$capacity(this);
}
template <>
::lnav_rs_ext::VarPair const *Vec<::lnav_rs_ext::VarPair>::data() const noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$VarPair$data(this);
}
template <>
void Vec<::lnav_rs_ext::VarPair>::reserve_total(::std::size_t new_cap) noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$VarPair$reserve_total(this, new_cap);
}
template <>
void Vec<::lnav_rs_ext::VarPair>::set_len(::std::size_t len) noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$VarPair$set_len(this, len);
}
template <>
void Vec<::lnav_rs_ext::VarPair>::truncate(::std::size_t len) {
  return cxxbridge1$rust_vec$lnav_rs_ext$VarPair$truncate(this, len);
}
template <>
Vec<::lnav_rs_ext::ExtProgress>::Vec() noexcept {
  cxxbridge1$rust_vec$lnav_rs_ext$ExtProgress$new(this);
}
template <>
void Vec<::lnav_rs_ext::ExtProgress>::drop() noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$ExtProgress$drop(this);
}
template <>
::std::size_t Vec<::lnav_rs_ext::ExtProgress>::size() const noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$ExtProgress$len(this);
}
template <>
::std::size_t Vec<::lnav_rs_ext::ExtProgress>::capacity() const noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$ExtProgress$capacity(this);
}
template <>
::lnav_rs_ext::ExtProgress const *Vec<::lnav_rs_ext::ExtProgress>::data() const noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$ExtProgress$data(this);
}
template <>
void Vec<::lnav_rs_ext::ExtProgress>::reserve_total(::std::size_t new_cap) noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$ExtProgress$reserve_total(this, new_cap);
}
template <>
void Vec<::lnav_rs_ext::ExtProgress>::set_len(::std::size_t len) noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$ExtProgress$set_len(this, len);
}
template <>
void Vec<::lnav_rs_ext::ExtProgress>::truncate(::std::size_t len) {
  return cxxbridge1$rust_vec$lnav_rs_ext$ExtProgress$truncate(this, len);
}
template <>
Vec<::lnav_rs_ext::SourceTreeElement>::Vec() noexcept {
  cxxbridge1$rust_vec$lnav_rs_ext$SourceTreeElement$new(this);
}
template <>
void Vec<::lnav_rs_ext::SourceTreeElement>::drop() noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$SourceTreeElement$drop(this);
}
template <>
::std::size_t Vec<::lnav_rs_ext::SourceTreeElement>::size() const noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$SourceTreeElement$len(this);
}
template <>
::std::size_t Vec<::lnav_rs_ext::SourceTreeElement>::capacity() const noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$SourceTreeElement$capacity(this);
}
template <>
::lnav_rs_ext::SourceTreeElement const *Vec<::lnav_rs_ext::SourceTreeElement>::data() const noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$SourceTreeElement$data(this);
}
template <>
void Vec<::lnav_rs_ext::SourceTreeElement>::reserve_total(::std::size_t new_cap) noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$SourceTreeElement$reserve_total(this, new_cap);
}
template <>
void Vec<::lnav_rs_ext::SourceTreeElement>::set_len(::std::size_t len) noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$SourceTreeElement$set_len(this, len);
}
template <>
void Vec<::lnav_rs_ext::SourceTreeElement>::truncate(::std::size_t len) {
  return cxxbridge1$rust_vec$lnav_rs_ext$SourceTreeElement$truncate(this, len);
}
template <>
Vec<::lnav_rs_ext::FindLogResult>::Vec() noexcept {
  cxxbridge1$rust_vec$lnav_rs_ext$FindLogResult$new(this);
}
template <>
void Vec<::lnav_rs_ext::FindLogResult>::drop() noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$FindLogResult$drop(this);
}
template <>
::std::size_t Vec<::lnav_rs_ext::FindLogResult>::size() const noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$FindLogResult$len(this);
}
template <>
::std::size_t Vec<::lnav_rs_ext::FindLogResult>::capacity() const noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$FindLogResult$capacity(this);
}
template <>
::lnav_rs_ext::FindLogResult const *Vec<::lnav_rs_ext::FindLogResult>::data() const noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$FindLogResult$data(this);
}
template <>
void Vec<::lnav_rs_ext::FindLogResult>::reserve_total(::std::size_t new_cap) noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$FindLogResult$reserve_total(this, new_cap);
}
template <>
void Vec<::lnav_rs_ext::FindLogResult>::set_len(::std::size_t len) noexcept {
  return cxxbridge1$rust_vec$lnav_rs_ext$FindLogResult$set_len(this, len);
}
template <>
void Vec<::lnav_rs_ext::FindLogResult>::truncate(::std::size_t len) {
  return cxxbridge1$rust_vec$lnav_rs_ext$FindLogResult$truncate(this, len);
}
} // namespace cxxbridge1
} // namespace rust
