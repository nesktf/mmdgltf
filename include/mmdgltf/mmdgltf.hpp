#ifndef MMDGLTF_HPP
#define MMDGLTF_HPP

#include <mmdgltf/mmdgltf.h>

#include <cassert>
#include <cstddef>
#include <exception>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace mmdgltf {

using Status = mmdgltf_Status;
using TextEncoding = mmdgltf_TextEncoding;
using WeightType = mmdgltf_WeightType;
using SphereMode = mmdgltf_SphereMode;
using RigidShapeType = mmdgltf_RigidShapeType;
using RigidPhysicsMode = mmdgltf_RigidPhysicsMode;
using MorphCategory = mmdgltf_MorphCategory;
using MorphType = mmdgltf_MorphType;
using Vec2f = mmdgltf_Vec2f;
using Vec3f = mmdgltf_Vec3f;
using Vec4f = mmdgltf_Vec4f;
using Color3f = mmdgltf_Color3f;
using Color4f = mmdgltf_Color4f;
using Quatf = mmdgltf_Quatf;
using PmxHeader = mmdgltf_PmxHeader;
using PmxModelInfo = mmdgltf_PmxModelInfo;
using PmxVertex = mmdgltf_PmxVertex;
using PmxTexture = mmdgltf_PmxTexture;
using PmxMaterial = mmdgltf_PmxMaterial;
using PmxBone = mmdgltf_PmxBone;
using PmxIkLink = mmdgltf_PmxIkLink;
using PmxMorph = mmdgltf_PmxMorph;
using PmxDisplayItem = mmdgltf_PmxDisplayItem;
using PmxDisplayElement = mmdgltf_PmxDisplayElement;
using PmxRigidBody = mmdgltf_PmxRigidBody;
using PmxJoint = mmdgltf_PmxJoint;
using Image = mmdgltf_Image;
using ExportConfig = mmdgltf_ExportConfig;
using CoordinateSystem = mmdgltf_CoordinateSystem;
using ModelConfig = mmdgltf_ModelConfig;
using VertexBuffers = mmdgltf_VertexBuffers;
using StreamFunc = mmdgltf_StreamFunc;
using Alloc = mmdgltf_Alloc;

template<typename T>
class ArrayView {
public:
  using value_type = T;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using iterator = T*;
  using const_iterator = const T*;

  ArrayView() noexcept : _data(nullptr), _size(0) {}

  ArrayView(T* data, size_t size) noexcept :
      _data(data && size ? data : nullptr), _size(data && size ? size : 0) {}

public:
  bool empty() const noexcept { return _size == 0; }

  T* data() const noexcept { return _data; }

  size_t size() const noexcept { return _size; }

  T& operator[](size_t index) const noexcept {
    assert(index < _size && "Index out of range in ArrayView");
    return _data[index];
  }

  T& at(size_t index) const {
#ifdef __cpp_exceptions
    if (index >= _size) {
      throw std::out_of_range("ArrayView::at index out of range");
    }
#else
    assert(index < _size && "Index out of range in ArrayView");
#endif
    return _data[index];
  }

  T& front() const noexcept {
    assert(_size > 0 && "ArrayView::front called on empty view");
    return _data[0];
  }

  T& back() const noexcept {
    assert(_size > 0 && "ArrayView::back called on empty view");
    return _data[_size - 1];
  }

  T* begin() const noexcept { return _data; }

  T* end() const noexcept { return _data + _size; }

  const T* cbegin() const noexcept { return _data; }

  const T* cend() const noexcept { return _data + _size; }

private:
  T* _data;
  size_t _size;
};

using std::in_place_t;
constexpr inline in_place_t in_place;

struct unexpect_t {};

constexpr inline unexpect_t unexpect;

class Error final : public std::exception {
public:
  Error() noexcept : _status(MMDGLTF_OK) {}

  explicit Error(mmdgltf_Status status) noexcept : _status(status) {}

public:
  const char* what() const noexcept override {
    switch (_status) {
      case MMDGLTF_OK:
        return "OK";
      case MMDGLTF_ERROR_INVALID_ARGUMENT:
        return "Invalid argument";
      case MMDGLTF_ERROR_OUT_OF_MEMORY:
        return "Out of memory";
      case MMDGLTF_ERROR_IO:
        return "I/O error";
      case MMDGLTF_ERROR_INVALID_SIGNATURE:
        return "Invalid signature";
      case MMDGLTF_ERROR_UNSUPPORTED_VERSION:
        return "Unsupported version";
      case MMDGLTF_ERROR_BUFFER_TOO_SMALL:
        return "Buffer too small";
      case MMDGLTF_ERROR_CORRUPT_DATA:
        return "Corrupt data";
      default:
        return "Unknown error";
    }
  }

  mmdgltf_Status code() const noexcept { return _status; }

private:
  mmdgltf_Status _status;
};

template<typename T>
class [[nodiscard]] Expected final {
private:
  static_assert(!std::is_reference_v<T> && !std::is_void_v<T>, "T must be an object type");
  static_assert(!std::is_const_v<T> && !std::is_volatile_v<T>, "T must not be CV qualified");

public:
  template<typename = std::enable_if_t<std::is_default_constructible_v<T>>>
  Expected() noexcept(std::is_nothrow_default_constructible_v<T>) : _obj(), _status(MMDGLTF_OK) {}

  template<typename... Args>
  Expected(in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) :
      _obj(std::forward<Args>(args)...), _status(MMDGLTF_OK) {}

  Expected(const T& obj) noexcept(std::is_nothrow_copy_constructible_v<T>) :
      _obj(obj), _status(MMDGLTF_OK) {}

  Expected(T&& obj) noexcept(std::is_nothrow_move_constructible_v<T>) :
      _obj(std::move(obj)), _status(MMDGLTF_OK) {}

  Expected(unexpect_t, mmdgltf_Status status) : _dummy(), _status(status) {
    assert(_status != MMDGLTF_OK);
  }

  Expected(const Error& err) : _dummy(), _status(err.code()) { assert(_status != MMDGLTF_OK); }

  Expected(Expected&& other) noexcept(std::is_nothrow_move_constructible_v<T>) :
      _status(other.code()) {
    if (other.has_value()) {
      new (&_obj) T(std::move(*other));
    }
  }

  Expected(const Expected& other) noexcept(std::is_nothrow_copy_constructible_v<T>) :
      _status(other.code()) {
    if (other.has_value()) {
      new (&_obj) T(*other);
    }
  }

  ~Expected() noexcept {
    if (has_value()) {
      _obj.~T();
    }
  }

  Expected& operator=(const Expected& other) noexcept(std::is_nothrow_copy_constructible_v<T> &&
                                                      std::is_nothrow_copy_assignable_v<T>) {
    if (this == &other) {
      return *this;
    }
    if (has_value() && other.has_value()) {
      _obj = other._obj;
    } else if (has_value() && !other.has_value()) {
      _obj.~T();
      _status = other._status;
    } else if (!has_value() && other.has_value()) {
      new (&_obj) T(other._obj);
      _status = MMDGLTF_OK;
    } else {
      _status = other._status;
    }
    return *this;
  }

  Expected& operator=(Expected&& other) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                                 std::is_nothrow_move_assignable_v<T>) {
    if (this == &other) {
      return *this;
    }
    if (has_value() && other.has_value()) {
      _obj = std::move(other._obj);
    } else if (has_value() && !other.has_value()) {
      _obj.~T();
      _status = other._status;
    } else if (!has_value() && other.has_value()) {
      new (&_obj) T(std::move(other._obj));
      _status = MMDGLTF_OK;
    } else {
      _status = other._status;
    }
    return *this;
  }

  Expected& operator=(const T& obj) noexcept(std::is_nothrow_copy_constructible_v<T> &&
                                             std::is_nothrow_copy_assignable_v<T>) {
    if (has_value()) {
      _obj = obj;
    } else {
      new (&_obj) T(obj);
      _status = MMDGLTF_OK;
    }
    return *this;
  }

  Expected& operator=(T&& obj) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                        std::is_nothrow_move_assignable_v<T>) {
    if (has_value()) {
      _obj = std::move(obj);
    } else {
      new (&_obj) T(std::move(obj));
      _status = MMDGLTF_OK;
    }
    return *this;
  }

  Expected& operator=(const Error& err) noexcept {
    if (has_value()) {
      _obj.~T();
    }
    _status = err.code();
    assert(_status != MMDGLTF_OK);
    return *this;
  }

public:
  mmdgltf_Status code() const noexcept { return _status; }

  bool has_value() const noexcept { return _status == MMDGLTF_OK; }

  bool has_error() const noexcept { return !has_value(); }

  explicit operator bool() const noexcept { return has_value(); }

  Error error() const noexcept { return Error(_status); }

public:
  T& operator*() & noexcept {
    assert(has_value() && "Valueless Expected");
    return _obj;
  }

  const T& operator*() const& noexcept {
    assert(has_value() && "Valueless Expected");
    return _obj;
  }

  T&& operator*() && noexcept {
    assert(has_value() && "Valueless Expected");
    return std::move(_obj);
  }

  const T&& operator*() const&& noexcept {
    assert(has_value() && "Valueless Expected");
    return std::move(_obj);
  }

  T* operator->() noexcept {
    assert(has_value() && "Valueless Expected");
    return &_obj;
  }

  const T* operator->() const noexcept {
    assert(has_value() && "Valueless Expected");
    return &_obj;
  }

  T& value() & {
#ifdef __cpp_exceptions
    if (!has_value()) {
      throw Error(_status);
    }
#else
    assert(has_value() && "Valueless Expected");
#endif
    return _obj;
  }

  const T& value() const& {
#ifdef __cpp_exceptions
    if (!has_value()) {
      throw Error(_status);
    }
#else
    assert(has_value() && "Valueless Expected");
#endif
    return _obj;
  }

  T&& value() && {
#ifdef __cpp_exceptions
    if (!has_value()) {
      throw Error(_status);
    }
#else
    assert(has_value() && "Valueless Expected");
#endif
    return std::move(_obj);
  }

  const T&& value() const&& {
#ifdef __cpp_exceptions
    if (!has_value()) {
      throw Error(_status);
    }
#else
    assert(has_value() && "Valueless Expected");
#endif
    return std::move(_obj);
  }

  template<typename U>
  T value_or(U&& def) const& {
    return has_value() ? **this : static_cast<T>(std::forward<U>(def));
  }

  template<typename U>
  T value_or(U&& def) && {
    return has_value() ? std::move(**this) : static_cast<T>(std::forward<U>(def));
  }

public:
  template<typename F>
  auto and_then(F&& f) & -> decltype(std::forward<F>(f)(std::declval<T&>())) {
    using Ret = decltype(std::forward<F>(f)(std::declval<T&>()));
    if (has_value()) {
      return std::forward<F>(f)(**this);
    } else {
      return Ret(unexpect, _status);
    }
  }

  template<typename F>
  auto and_then(F&& f) && -> decltype(std::forward<F>(f)(std::declval<T&&>())) {
    using Ret = decltype(std::forward<F>(f)(std::declval<T&&>()));
    if (has_value()) {
      return std::forward<F>(f)(std::move(**this));
    } else {
      return Ret(unexpect, _status);
    }
  }

  template<typename F>
  auto and_then(F&& f) const& -> decltype(std::forward<F>(f)(std::declval<const T&>())) {
    using Ret = decltype(std::forward<F>(f)(std::declval<const T&>()));
    if (has_value()) {
      return std::forward<F>(f)(**this);
    } else {
      return Ret(unexpect, _status);
    }
  }

  template<typename F>
  auto and_then(F&& f) const&& -> decltype(std::forward<F>(f)(std::declval<const T&&>())) {
    using Ret = decltype(std::forward<F>(f)(std::declval<const T&&>()));
    if (has_value()) {
      return std::forward<F>(f)(std::move(**this));
    } else {
      return Ret(unexpect, _status);
    }
  }

  template<typename F>
  auto transform(
    F&& f) & -> Expected<std::remove_cv_t<decltype(std::forward<F>(f)(std::declval<T&>()))>> {
    using RetVal = std::remove_cv_t<decltype(std::forward<F>(f)(std::declval<T&>()))>;
    using Ret = Expected<RetVal>;
    if (has_value()) {
      return Ret(in_place, std::forward<F>(f)(**this));
    } else {
      return Ret(unexpect, _status);
    }
  }

  template<typename F>
  auto transform(
    F&& f) && -> Expected<std::remove_cv_t<decltype(std::forward<F>(f)(std::declval<T&&>()))>> {
    using RetVal = std::remove_cv_t<decltype(std::forward<F>(f)(std::declval<T&&>()))>;
    using Ret = Expected<RetVal>;
    if (has_value()) {
      return Ret(in_place, std::forward<F>(f)(std::move(**this)));
    } else {
      return Ret(unexpect, _status);
    }
  }

  template<typename F>
  auto transform(F&& f)
    const& -> Expected<std::remove_cv_t<decltype(std::forward<F>(f)(std::declval<const T&>()))>> {
    using RetVal = std::remove_cv_t<decltype(std::forward<F>(f)(std::declval<const T&>()))>;
    using Ret = Expected<RetVal>;
    if (has_value()) {
      return Ret(in_place, std::forward<F>(f)(**this));
    } else {
      return Ret(unexpect, _status);
    }
  }

  template<typename F>
  auto transform(F&& f) const&& -> Expected<
    std::remove_cv_t<decltype(std::forward<F>(f)(std::declval<const T&&>()))>> {
    using RetVal = std::remove_cv_t<decltype(std::forward<F>(f)(std::declval<const T&&>()))>;
    using Ret = Expected<RetVal>;
    if (has_value()) {
      return Ret(in_place, std::forward<F>(f)(std::move(**this)));
    } else {
      return Ret(unexpect, _status);
    }
  }

  template<typename F>
  auto or_else(F&& f) & {
    using Ret = decltype(_invoke_error_fn(std::forward<F>(f), _status));
    if constexpr (std::is_void_v<Ret>) {
      if (!has_value()) {
        _invoke_error_fn(std::forward<F>(f), _status);
      }
      return *this;
    } else if constexpr (std::is_same_v<std::decay_t<Ret>, Expected<T>>) {
      if (has_value()) {
        return *this;
      } else {
        return _invoke_error_fn(std::forward<F>(f), _status);
      }
    } else {
      if (has_value()) {
        return Expected<T>(in_place, **this);
      } else {
        return Expected<T>(in_place, _invoke_error_fn(std::forward<F>(f), _status));
      }
    }
  }

  template<typename F>
  auto or_else(F&& f) && {
    using Ret = decltype(_invoke_error_fn(std::forward<F>(f), _status));
    if constexpr (std::is_void_v<Ret>) {
      if (!has_value()) {
        _invoke_error_fn(std::forward<F>(f), _status);
      }
      return std::move(*this);
    } else if constexpr (std::is_same_v<std::decay_t<Ret>, Expected<T>>) {
      if (has_value()) {
        return std::move(*this);
      } else {
        return _invoke_error_fn(std::forward<F>(f), _status);
      }
    } else {
      if (has_value()) {
        return Expected<T>(in_place, std::move(**this));
      } else {
        return Expected<T>(in_place, _invoke_error_fn(std::forward<F>(f), _status));
      }
    }
  }

  template<typename F>
  auto or_else(F&& f) const& {
    using Ret = decltype(_invoke_error_fn(std::forward<F>(f), _status));
    if constexpr (std::is_void_v<Ret>) {
      if (!has_value()) {
        _invoke_error_fn(std::forward<F>(f), _status);
      }
      return *this;
    } else if constexpr (std::is_same_v<std::decay_t<Ret>, Expected<T>>) {
      if (has_value()) {
        return *this;
      } else {
        return _invoke_error_fn(std::forward<F>(f), _status);
      }
    } else {
      if (has_value()) {
        return Expected<T>(in_place, **this);
      } else {
        return Expected<T>(in_place, _invoke_error_fn(std::forward<F>(f), _status));
      }
    }
  }

  template<typename F>
  auto or_else(F&& f) const&& {
    using Ret = decltype(_invoke_error_fn(std::forward<F>(f), _status));
    if constexpr (std::is_void_v<Ret>) {
      if (!has_value()) {
        _invoke_error_fn(std::forward<F>(f), _status);
      }
      return std::move(*this);
    } else if constexpr (std::is_same_v<std::decay_t<Ret>, Expected<T>>) {
      if (has_value()) {
        return std::move(*this);
      } else {
        return _invoke_error_fn(std::forward<F>(f), _status);
      }
    } else {
      if (has_value()) {
        return Expected<T>(in_place, std::move(**this));
      } else {
        return Expected<T>(in_place, _invoke_error_fn(std::forward<F>(f), _status));
      }
    }
  }

private:
  template<typename F>
  static decltype(auto) _invoke_error_fn(F&& f, mmdgltf_Status status) {
    if constexpr (std::is_invocable_v<F, mmdgltf_Status>) {
      return std::forward<F>(f)(status);
    } else if constexpr (std::is_invocable_v<F, Error>) {
      return std::forward<F>(f)(Error(status));
    } else if constexpr (std::is_invocable_v<F, const Error&>) {
      const Error err(status);
      return std::forward<F>(f)(err);
    } else {
      return std::forward<F>(f)();
    }
  }

private:
  union {
    char _dummy;
    T _obj;
  };

  mmdgltf_Status _status;
};

class PmxModel {
public:
  static Expected<PmxModel> load_file(const char* path) noexcept {
    auto pmx = mmdgltf_pmx_load_file(path);
    if (auto err = MMDGLTF_CHECK(pmx); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, pmx};
  }

  static Expected<PmxModel> load_file(const char* path, size_t path_len) noexcept {
    auto pmx = mmdgltf_pmx_load_filen(path, path_len);
    if (auto err = MMDGLTF_CHECK(pmx); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, pmx};
  }

  static Expected<PmxModel> load_filen(const char* path, size_t path_len) noexcept {
    return load_file(path, path_len);
  }

  static Expected<PmxModel> load_memory(const void* data, size_t size) noexcept {
    auto pmx = mmdgltf_pmx_load_memory(data, size);
    if (auto err = MMDGLTF_CHECK(pmx); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, pmx};
  }

  static Expected<PmxModel> load_stream(const mmdgltf_StreamFunc* func, void* src) noexcept {
    auto pmx = mmdgltf_pmx_load_stream(func, src);
    if (auto err = MMDGLTF_CHECK(pmx); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, pmx};
  }

  static Expected<PmxModel> load_stream(const mmdgltf_StreamFunc& func, void* src) noexcept {
    return load_stream(&func, src);
  }

public:
  PmxModel() noexcept : _pmx(nullptr) {}

  explicit PmxModel(mmdgltf_Pmx pmx) noexcept : _pmx(pmx) {}

  explicit PmxModel(const char* path) {
    _pmx = mmdgltf_pmx_load_file(path);
    if (auto err = MMDGLTF_CHECK(_pmx); err != MMDGLTF_OK) {
#ifdef __cpp_exceptions
      throw Error(err);
#else
      assert(0 && "PmxModel error");
#endif
    }
  }

  explicit PmxModel(const char* path, size_t path_len) {
    _pmx = mmdgltf_pmx_load_filen(path, path_len);
    if (auto err = MMDGLTF_CHECK(_pmx); err != MMDGLTF_OK) {
#ifdef __cpp_exceptions
      throw Error(err);
#else
      assert(0 && "PmxModel error");
#endif
    }
  }

  explicit PmxModel(const void* data, size_t size) {
    _pmx = mmdgltf_pmx_load_memory(data, size);
    if (auto err = MMDGLTF_CHECK(_pmx); err != MMDGLTF_OK) {
#ifdef __cpp_exceptions
      throw Error(err);
#else
      assert(0 && "PmxModel error");
#endif
    }
  }

  explicit PmxModel(const mmdgltf_StreamFunc* func, void* src) {
    _pmx = mmdgltf_pmx_load_stream(func, src);
    if (auto err = MMDGLTF_CHECK(_pmx); err != MMDGLTF_OK) {
#ifdef __cpp_exceptions
      throw Error(err);
#else
      assert(0 && "PmxModel error");
#endif
    }
  }

  explicit PmxModel(const mmdgltf_StreamFunc& func, void* src) : PmxModel(&func, src) {}

  ~PmxModel() noexcept { mmdgltf_pmx_destroy(_pmx); }

  PmxModel(PmxModel&& other) noexcept : _pmx(other._pmx) { other._pmx = nullptr; }

  PmxModel& operator=(PmxModel&& other) noexcept {
    if (this != &other) {
      mmdgltf_pmx_destroy(_pmx);
      _pmx = other._pmx;
      other._pmx = nullptr;
    }
    return *this;
  }

  PmxModel(const PmxModel&) = delete;
  PmxModel& operator=(const PmxModel&) = delete;

public:
  bool empty() const noexcept { return _pmx == nullptr; }

  explicit operator bool() const noexcept { return _pmx != nullptr; }

  mmdgltf_Pmx raw() const noexcept { return _pmx; }

  mmdgltf_Pmx get() const noexcept { return _pmx; }

  mmdgltf_Pmx release() noexcept {
    auto p = _pmx;
    _pmx = nullptr;
    return p;
  }

  void reset(mmdgltf_Pmx pmx = nullptr) noexcept {
    mmdgltf_pmx_destroy(_pmx);
    _pmx = pmx;
  }

public:
  Expected<PmxHeader> header() const noexcept {
    PmxHeader pmx_header;
    if (auto err = mmdgltf_pmx_get_header(_pmx, &pmx_header); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, pmx_header};
  }

  Expected<PmxModelInfo> model_info() const noexcept {
    PmxModelInfo pmx_info;
    if (auto err = mmdgltf_pmx_get_model_info(_pmx, &pmx_info); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, pmx_info};
  }

  Expected<ArrayView<const PmxVertex>> vertices() const noexcept {
    const PmxVertex* ptr = nullptr;
    size_t count = 0;
    if (auto err = mmdgltf_pmx_vertices(_pmx, &ptr, &count); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, ptr, count};
  }

  Expected<ArrayView<const uint32_t>> indices() const noexcept {
    const uint32_t* ptr = nullptr;
    size_t count = 0;
    if (auto err = mmdgltf_pmx_indices(_pmx, &ptr, &count); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, ptr, count};
  }

  Expected<ArrayView<const PmxTexture>> textures() const noexcept {
    const PmxTexture* ptr = nullptr;
    size_t count = 0;
    if (auto err = mmdgltf_pmx_textures(_pmx, &ptr, &count); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, ptr, count};
  }

  Expected<ArrayView<const PmxMaterial>> materials() const noexcept {
    const PmxMaterial* ptr = nullptr;
    size_t count = 0;
    if (auto err = mmdgltf_pmx_materials(_pmx, &ptr, &count); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, ptr, count};
  }

  Expected<ArrayView<const PmxBone>> bones() const noexcept {
    const PmxBone* ptr = nullptr;
    size_t count = 0;
    if (auto err = mmdgltf_pmx_bones(_pmx, &ptr, &count); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, ptr, count};
  }

  Expected<ArrayView<const PmxMorph>> morphs() const noexcept {
    const PmxMorph* ptr = nullptr;
    size_t count = 0;
    if (auto err = mmdgltf_pmx_morphs(_pmx, &ptr, &count); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, ptr, count};
  }

  Expected<ArrayView<const PmxDisplayItem>> display_items() const noexcept {
    const PmxDisplayItem* ptr = nullptr;
    size_t count = 0;
    if (auto err = mmdgltf_pmx_display_items(_pmx, &ptr, &count); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, ptr, count};
  }

  Expected<ArrayView<const PmxRigidBody>> rigid_bodies() const noexcept {
    const PmxRigidBody* ptr = nullptr;
    size_t count = 0;
    if (auto err = mmdgltf_pmx_rigid_bodies(_pmx, &ptr, &count); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, ptr, count};
  }

  Expected<ArrayView<const PmxJoint>> joints() const noexcept {
    const PmxJoint* ptr = nullptr;
    size_t count = 0;
    if (auto err = mmdgltf_pmx_joints(_pmx, &ptr, &count); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, ptr, count};
  }

private:
  mmdgltf_Pmx _pmx;
};

class Model {
public:
  static Expected<Model> create(mmdgltf_Pmx pmx, const ModelConfig* config = nullptr) noexcept {
    auto model = mmdgltf_model_create(pmx, config);
    if (auto err = MMDGLTF_CHECK(model); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, model};
  }

  static Expected<Model> create(const PmxModel& pmx_model,
                                const ModelConfig* config = nullptr) noexcept {
    return create(pmx_model.raw(), config);
  }

public:
  Model() noexcept : _model(nullptr) {}

  explicit Model(mmdgltf_Model model) noexcept : _model(model) {}

  explicit Model(mmdgltf_Pmx pmx, const ModelConfig* config = nullptr) {
    _model = mmdgltf_model_create(pmx, config);
    if (auto err = MMDGLTF_CHECK(_model); err != MMDGLTF_OK) {
#ifdef __cpp_exceptions
      throw Error(err);
#else
      assert(0 && "Model error");
#endif
    }
  }

  explicit Model(const PmxModel& pmx_model, const ModelConfig* config = nullptr) :
      Model(pmx_model.raw(), config) {}

  ~Model() noexcept { mmdgltf_model_destroy(_model); }

  Model(Model&& other) noexcept : _model(other._model) { other._model = nullptr; }

  Model& operator=(Model&& other) noexcept {
    if (this != &other) {
      mmdgltf_model_destroy(_model);
      _model = other._model;
      other._model = nullptr;
    }
    return *this;
  }

  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

public:
  bool empty() const noexcept { return _model == nullptr; }

  explicit operator bool() const noexcept { return _model != nullptr; }

  mmdgltf_Model raw() const noexcept { return _model; }

  mmdgltf_Model get() const noexcept { return _model; }

  mmdgltf_Model release() noexcept {
    auto m = _model;
    _model = nullptr;
    return m;
  }

  void reset(mmdgltf_Model model = nullptr) noexcept {
    mmdgltf_model_destroy(_model);
    _model = model;
  }

public:
  Expected<VertexBuffers> vertex_buffers() const noexcept {
    VertexBuffers bufs{};
    if (auto err = mmdgltf_model_vertex_buffers(_model, &bufs); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, bufs};
  }

  Expected<ArrayView<const uint32_t>> indices() const noexcept {
    const uint32_t* ptr = nullptr;
    size_t count = 0;
    if (auto err = mmdgltf_model_indices(_model, &ptr, &count); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, ptr, count};
  }

  Expected<ArrayView<const PmxMaterial>> materials() const noexcept {
    const PmxMaterial* ptr = nullptr;
    size_t count = 0;
    if (auto err = mmdgltf_model_materials(_model, &ptr, &count); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, ptr, count};
  }

  Expected<ArrayView<const Image>> images() const noexcept {
    const Image* ptr = nullptr;
    size_t count = 0;
    if (auto err = mmdgltf_model_images(_model, &ptr, &count); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, ptr, count};
  }

  Expected<ArrayView<const PmxBone>> bones() const noexcept {
    const PmxBone* ptr = nullptr;
    size_t count = 0;
    if (auto err = mmdgltf_model_bones(_model, &ptr, &count); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, ptr, count};
  }

  Expected<ArrayView<const PmxMorph>> morphs() const noexcept {
    const PmxMorph* ptr = nullptr;
    size_t count = 0;
    if (auto err = mmdgltf_model_morphs(_model, &ptr, &count); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, ptr, count};
  }

  Expected<ArrayView<const PmxRigidBody>> rigid_bodies() const noexcept {
    const PmxRigidBody* ptr = nullptr;
    size_t count = 0;
    if (auto err = mmdgltf_model_rigid_bodies(_model, &ptr, &count); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, ptr, count};
  }

  Expected<ArrayView<const PmxJoint>> joints() const noexcept {
    const PmxJoint* ptr = nullptr;
    size_t count = 0;
    if (auto err = mmdgltf_model_joints(_model, &ptr, &count); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    return {in_place, ptr, count};
  }

  mmdgltf_Status export_glb_file(const char* filename,
                                 const ExportConfig* config = nullptr) const noexcept {
    return mmdgltf_export_glb_file(_model, filename, config);
  }

  Expected<std::vector<uint8_t>>
  export_glb_memory(const ExportConfig* config = nullptr) const noexcept {
    uint8_t* data = nullptr;
    size_t size = 0;
    if (auto err = mmdgltf_export_glb_memory(_model, &data, &size, config); err != MMDGLTF_OK) {
      return {unexpect, err};
    }
    std::vector<uint8_t> result(data, data + size);
    mmdgltf_export_free(data, size);
    return {in_place, std::move(result)};
  }

private:
  mmdgltf_Model _model;
};

inline Expected<Model> model_create(mmdgltf_Pmx pmx,
                                    const ModelConfig* config = nullptr) noexcept {
  return Model::create(pmx, config);
}

inline Expected<Model> model_create(const PmxModel& pmx_model,
                                    const ModelConfig* config = nullptr) noexcept {
  return Model::create(pmx_model, config);
}

inline Expected<PmxModel> pmx_load_file(const char* path) noexcept {
  return PmxModel::load_file(path);
}

inline Expected<PmxModel> pmx_load_file(const char* path, size_t path_len) noexcept {
  return PmxModel::load_file(path, path_len);
}

inline Expected<PmxModel> pmx_load_filen(const char* path, size_t path_len) noexcept {
  return PmxModel::load_filen(path, path_len);
}

inline Expected<PmxModel> pmx_load_memory(const void* data, size_t size) noexcept {
  return PmxModel::load_memory(data, size);
}

inline Expected<PmxModel> pmx_load_stream(const mmdgltf_StreamFunc* func, void* src) noexcept {
  return PmxModel::load_stream(func, src);
}

inline Expected<PmxModel> pmx_load_stream(const mmdgltf_StreamFunc& func, void* src) noexcept {
  return PmxModel::load_stream(func, src);
}

inline void set_alloc(const mmdgltf_Alloc* alloc) noexcept {
  mmdgltf_set_alloc(alloc);
}

inline void set_alloc(const mmdgltf_Alloc& alloc) noexcept {
  mmdgltf_set_alloc(&alloc);
}

} // namespace mmdgltf

#endif // MMDGLTF_HPP
