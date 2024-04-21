//
// detail/iprogs_thread.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2023 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_IPROGS_THREAD_HPP
#define ASIO_DETAIL_IPROGS_THREAD_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"

#if defined(ASIO_IPROGS_THREADS)

#include <thread>
#include "asio/detail/noncopyable.hpp"

#include "asio/detail/push_options.hpp"

#include "iprog/thread.hpp" // <-- how does asio have access to standard C++11 threads then??

namespace asio {
namespace detail {

class iprogs_thread
  : private noncopyable
{
public:
  // Constructor.
  template <typename Function>
  iprogs_thread(Function f, unsigned int = 0)
    : thread_(f)
  {
  }

  // Destructor.
  ~iprogs_thread()
  {
    join();
  }

  // Wait for the thread to exit.
  void join()
  {
    if (thread_.joinable())
      thread_.join();
  }

  // Get number of CPUs.
  static std::size_t hardware_concurrency()
  {
    return iprog::thread::hardware_concurrency();
  }

private:
  iprog::thread thread_;
};

} // namespace detail
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // defined(ASIO_IPROGS_THREADS)

#endif // ASIO_DETAIL_IPROGS_THREAD_HPP
