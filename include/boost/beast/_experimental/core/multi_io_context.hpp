//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_MULTI_IO_CONTEXT_HPP
#define BOOST_BEAST_MULTI_IO_CONTEXT_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <atomic>
#include <utility>

namespace boost {
namespace beast {

class multi_io_context
{
    static constexpr int concurrency_hint_ = 1;

    struct element;

    class table
    {
        std::size_t n_;
        element* v_;

    public:
        ~table();
        explicit table(std::size_t n);
        element* begin() noexcept;
        element* end() noexcept;
        std::size_t size() const noexcept;
    };

    table tab_;
    element* idle_;
    boost::shared_mutex m_;

public:
    class executor_type
    {
        element* e_;

        friend class multi_io_context;

        explicit
        executor_type(element& e);

    public:
        ~executor_type();

        executor_type(
            executor_type const& ex) noexcept;

        net::io_context&
        context() const noexcept;

        executor_type&
        operator=(
            executor_type const& ex) noexcept;

        void on_work_started() const noexcept;
        void on_work_finished() const noexcept;

        template<class F, class Alloc>
        void dispatch(F&& f, Alloc const& a);

        template<class F, class Alloc>
        void post(F&& f, Alloc const& a);

        template<class F, class Alloc>
        void defer(F&& f, Alloc const& a);

        friend bool operator==(
            executor_type const& ex1,
            executor_type const& ex2) noexcept
        {
            return ex1.e_ == ex2.e_;
        }

        friend bool operator!=(
            executor_type const& ex1,
            executor_type const& ex2) noexcept
        {
            return ex1.e_ != ex2.e_;
        }
    };

    explicit
    multi_io_context(
        std::size_t number_of_threads);

    executor_type
    make_executor() noexcept;

    void run();

    void stop();
};

} // beast
} // boost

#include <boost/beast/_experimental/core/impl/multi_io_context.hpp>

#endif
