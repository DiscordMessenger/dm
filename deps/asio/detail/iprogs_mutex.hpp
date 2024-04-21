//
// detail/iprogs_mutex.hpp
// ~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2023 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_IPROGS_MUTEX_HPP
#define ASIO_DETAIL_IPROGS_MUTEX_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"

#include "iprog/mutex.hpp"
#include "asio/detail/noncopyable.hpp"
#include "asio/detail/scoped_lock.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {
namespace detail {

class std_event;

class iprogs_mutex
  : private noncopyable
{
public:
  typedef asio::detail::scoped_lock<iprogs_mutex> scoped_lock;

  // Constructor.
  iprogs_mutex()
  {
  }

  // Destructor.
  ~iprogs_mutex()
  {
  }

  // Lock the mutex.
  void lock()
  {
    mutex_.lock();
  }

  // Unlock the mutex.
  void unlock()
  {
    mutex_.unlock();
  }

private:
  friend class std_event;
  iprog::mutex mutex_;
};

} // namespace detail
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_DETAIL_IPROGS_MUTEX_HPP
