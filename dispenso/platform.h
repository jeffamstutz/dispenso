// Copyright (c) Facebook, Inc. and its affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE.md file in the root directory of this source tree.

/**
 * @file platform constants and common utilities.
 **/

#pragma once
#include <algorithm>
#include <atomic>
#include <thread>
#include <type_traits>

namespace dispenso {

#define DISPENSO_MAJOR_VERSION 1
#define DISPENSO_MINOR_VERSION 0

#if defined(DISPENSO_SHARED_LIB)
#if defined _WIN32

#if defined(DISPENSO_LIB_EXPORT)
#define DISPENSO_DLL_ACCESS __declspec(dllexport)
#else
#define DISPENSO_DLL_ACCESS __declspec(dllimport)
#endif // DISPENSO_LIB_EXPORT

#elif defined(__clang__) || defined(__GNUC__)
#define DISPENSO_DLL_ACCESS __attribute__((visibility("default")))
#endif // PLATFORM
#endif // DISPENSO_SHARED_LIB

#if !defined(DISPENSO_DLL_ACCESS)
#define DISPENSO_DLL_ACCESS
#endif // DISPENSO_DLL_ACCESS

#if defined(_MSC_VER)
using ssize_t = std::make_signed<std::size_t>::type;
#endif

#if defined(__clang__) || defined(__GNUC__)
#define DISPENSO_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)
#define DISPENSO_INLINE __forceinline
#else
#define DISPENSO_INLINE inline
#endif // PLATFORM

/**
 * @var constexpr size_t kCacheLineSize
 * @brief A constant that defines a safe number of bytes+alignment to avoid false sharing.
 **/
constexpr size_t kCacheLineSize = 64;

/**
 * @def DISPENSO_THREAD_LOCAL
 * @brief A macro that can be used when declaring a lightweight thread-local variable.
 **/

// TODO(bbudge): Non-gcc/clang/msvc platforms.
#if defined(_MSC_VER)
#define DISPENSO_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define DISPENSO_THREAD_LOCAL __thread
#else
#error Supply lightweight thread-locals for this compiler.  Can define to thread_local if lightweight not available
#endif

#if (defined(__GNUC__) || defined(__clang__))
#define DISPENSO_EXPECT(a, b) __builtin_expect(a, b)
#else
#define DISPENSO_EXPECT(a, b) a
#endif

// clang-format off
#if (defined(__GNUC__) || defined(__clang__))
#define DO_PRAGMA(X) _Pragma(#X)
#define DISPENSO_DISABLE_WARNING_PUSH DO_PRAGMA(GCC diagnostic push)
#define DISPENSO_DISABLE_WARNING_POP DO_PRAGMA(GCC diagnostic pop)
#define DISPENSO_DISABLE_WARNING(warningName) DO_PRAGMA(GCC diagnostic ignored #warningName)
#if !defined(__clang__)
#define DISPENSO_DISABLE_WARNING_ZERO_VARIADIC_MACRO_ARGUMENTS
#else
#define DISPENSO_DISABLE_WARNING_ZERO_VARIADIC_MACRO_ARGUMENTS \
  DISPENSO_DISABLE_WARNING(-Wgnu-zero-variadic-macro-arguments)
#endif
#elif defined(_MSC_VER)
#define DISPENSO_DISABLE_WARNING_PUSH __pragma(warning(push))
#define DISPENSO_DISABLE_WARNING_POP __pragma(warning(pop))
#define DISPENSO_DISABLE_WARNING(warningNumber) __pragma(warning(disable : warningNumber))
#define DISPENSO_DISABLE_WARNING_ZERO_VARIADIC_MACRO_ARGUMENTS
#else
#define DISPENSO_DISABLE_WARNING_PUSH
#define DISPENSO_DISABLE_WARNING_POP
#define DISPENSO_DISABLE_WARNING_ZERO_VARIADIC_MACRO_ARGUMENTS
#endif
// clang-format on

template <typename T>
class CacheAligned {
 public:
  CacheAligned() = default;
  CacheAligned(T t) : t_(t) {}
  operator T&() {
    return t_;
  }

 private:
  alignas(kCacheLineSize) T t_;
};

namespace detail {

template <typename T>
struct AlignedBuffer {
  alignas(alignof(T)) char b[sizeof(T)];
};

template <typename T>
struct alignas(kCacheLineSize) AlignedAtomic : public std::atomic<T*> {};

inline void* alignedMalloc(size_t bytes, size_t alignment) {
  alignment = std::max(alignment, sizeof(uintptr_t));
  char* ptr = reinterpret_cast<char*>(::malloc(bytes + alignment));
  uintptr_t base = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t oldBase = base;
  uintptr_t mask = alignment - 1;
  base += alignment;
  base &= ~mask;

  uintptr_t* recovery = reinterpret_cast<uintptr_t*>(base - sizeof(uintptr_t));
  *recovery = oldBase;
  return reinterpret_cast<void*>(base);
}

inline void* alignedMalloc(size_t bytes) {
  return alignedMalloc(bytes, kCacheLineSize);
}

inline void alignedFree(void* ptr) {
  if (!ptr) {
    return;
  }
  char* p = reinterpret_cast<char*>(ptr);
  uintptr_t recovered = *reinterpret_cast<uintptr_t*>(p - sizeof(uintptr_t));
  ::free(reinterpret_cast<void*>(recovered));
}

inline constexpr uintptr_t alignToCacheLine(uintptr_t val) {
  constexpr uintptr_t kMask = kCacheLineSize - 1;
  val += kMask;
  val &= ~kMask;
  return val;
}

#if defined __x86_64__ || defined __i386__
inline void cpuRelax() {
  asm volatile("pause" ::: "memory");
}
#else
// TODO: provide reasonable relax on non-x86
inline void cpuRelax() {}
#endif // x86-arch

// When statically chunking a range, it is generally not possible to use a single chunk size plus
// remainder and get a good load distribution.  By estimating too high, we can have idle threads. By
// estimating too low, the remainder can be several times as large as the chunk for other threads.
// Instead, we compute the chunk size that is the ceil of the fractional chunk size.  That can be
// used for the first transitionIndex values, while the remaining (chunks - transitionTaskIndex)
// values will be ceilChunkSize - 1.
struct StaticChunking {
  ssize_t transitionTaskIndex;
  ssize_t ceilChunkSize;
};

inline StaticChunking staticChunkSize(ssize_t items, ssize_t chunks) {
  StaticChunking chunking;
  chunking.ceilChunkSize = (items + chunks - 1) / chunks;
  ssize_t numLeft = chunking.ceilChunkSize * chunks - items;
  chunking.transitionTaskIndex = chunks - numLeft;
  return chunking;
}

} // namespace detail
} // namespace dispenso
