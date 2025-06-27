// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// a simple lockless thread-safe,
// single producer, single consumer queue

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <mutex>

#include "Common/TypeUtils.h"

namespace Common
{

namespace detail
{
// Compatibility layer for C++20 atomic wait/notify
template <typename T>
class AtomicWaitCompat
{
public:
  AtomicWaitCompat() = default;
  
  void wait(T old_value, std::memory_order order = std::memory_order_seq_cst) const
  {
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L && \
    (!defined(__APPLE__) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 110000)
    // Use C++20 atomic wait if available and on macOS 11.0+
    m_value.wait(old_value, order);
#else
    // Fallback for older systems
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this, old_value] { return m_value.load(std::memory_order_acquire) != old_value; });
#endif
  }
  
  void notify_one() const
  {
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L && \
    (!defined(__APPLE__) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 110000)
    // Use C++20 atomic notify if available and on macOS 11.0+
    m_value.notify_one();
#else
    // Fallback for older systems
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cv.notify_one();
#endif
  }
  
  void notify_all() const
  {
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L && \
    (!defined(__APPLE__) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 110000)
    // Use C++20 atomic notify if available and on macOS 11.0+
    m_value.notify_all();
#else
    // Fallback for older systems
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cv.notify_all();
#endif
  }
  
  // Proxy methods to std::atomic
  T load(std::memory_order order = std::memory_order_seq_cst) const
  {
    return m_value.load(order);
  }
  
  void store(T desired, std::memory_order order = std::memory_order_seq_cst)
  {
    m_value.store(desired, order);
#if !defined(__cpp_lib_atomic_wait) || __cpp_lib_atomic_wait < 201907L || \
    (defined(__APPLE__) && __MAC_OS_X_VERSION_MIN_REQUIRED < 110000)
    // Notify on store for fallback implementation
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cv.notify_all();
#endif
  }
  
  T fetch_add(T arg, std::memory_order order = std::memory_order_seq_cst)
  {
    T result = m_value.fetch_add(arg, order);
#if !defined(__cpp_lib_atomic_wait) || __cpp_lib_atomic_wait < 201907L || \
    (defined(__APPLE__) && __MAC_OS_X_VERSION_MIN_REQUIRED < 110000)
    // Notify on fetch_add for fallback implementation
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cv.notify_all();
#endif
    return result;
  }

private:
  std::atomic<T> m_value{0};
#if !defined(__cpp_lib_atomic_wait) || __cpp_lib_atomic_wait < 201907L || \
    (defined(__APPLE__) && __MAC_OS_X_VERSION_MIN_REQUIRED < 110000)
  mutable std::mutex m_mutex;
  mutable std::condition_variable m_cv;
#endif
};

template <typename T, bool IncludeWaitFunctionality>
class SPSCQueueBase final
{
public:
  SPSCQueueBase() = default;
  ~SPSCQueueBase()
  {
    Clear();
    delete m_read_ptr;
  }

  SPSCQueueBase(const SPSCQueueBase&) = delete;
  SPSCQueueBase& operator=(const SPSCQueueBase&) = delete;

  std::size_t Size() const { return m_size.load(std::memory_order_acquire); }
  bool Empty() const { return Size() == 0; }

  // The following are only safe from the "producer thread":
  void Push(const T& arg) { Emplace(arg); }
  void Push(T&& arg) { Emplace(std::move(arg)); }
  template <typename... Args>
  void Emplace(Args&&... args)
  {
    m_write_ptr->value.Construct(std::forward<Args>(args)...);

    Node* const new_ptr = new Node;
    m_write_ptr->next = new_ptr;
    m_write_ptr = new_ptr;

    AdjustSize(1);
  }

  void WaitForEmpty() requires(IncludeWaitFunctionality)
  {
    while (const std::size_t old_size = Size())
      m_size.wait(old_size, std::memory_order_acquire);
  }

  // The following are only safe from the "consumer thread":
  T& Front() { return m_read_ptr->value.Ref(); }
  const T& Front() const { return m_read_ptr->value.Ref(); }

  void Pop()
  {
    assert(!Empty());

    m_read_ptr->value.Destroy();

    Node* const old_node = m_read_ptr;
    m_read_ptr = old_node->next;
    delete old_node;

    AdjustSize(-1);
  }

  bool Pop(T& result)
  {
    if (Empty())
      return false;

    result = std::move(Front());
    Pop();
    return true;
  }

  void WaitForData() requires(IncludeWaitFunctionality)
  {
    m_size.wait(0, std::memory_order_acquire);
  }

  void Clear()
  {
    while (!Empty())
      Pop();
  }

private:
  struct Node
  {
    ManuallyConstructedValue<T> value;
    Node* next;
  };

  Node* m_write_ptr = new Node;
  Node* m_read_ptr = m_write_ptr;

  void AdjustSize(std::size_t value)
  {
    m_size.fetch_add(value, std::memory_order_release);
    if constexpr (IncludeWaitFunctionality)
      m_size.notify_one();
  }

  AtomicWaitCompat<std::size_t> m_size;
};
}  // namespace detail

template <typename T>
using SPSCQueue = detail::SPSCQueueBase<T, false>;

template <typename T>
using WaitableSPSCQueue = detail::SPSCQueueBase<T, true>;

}  // namespace Common
