//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_SPAWN_HPP
#define BOOST_BEAST_CORE_SPAWN_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/post.hpp>
#include <experimental/resumable>
#include <exception>
#include <memory>
#include <type_traits>

namespace boost {
namespace beast {

namespace detail {

template<class Executor, class Awaitable>
void
co_spawn_entry_point(
    boost::asio::executor_work_guard<
        Executor> wg,
    Awaitable a)
{
    std::exception_ptr e = nullptr;
    try
    {
        co_await a;
    }
    catch(...)
    {
        e = std::current_exception();
    }

    // propagate e to where?
}

} // detail

/** A coroutine wrapper for spawning new coroutines.

    This wrapper is used when launching top-level coroutines
    on an executor. It cannot be awaited, and returns no result.
*/
class [[maybe_unused]] detached_task
{
public:
#if ! BOOST_BEAST_DOXYGEN
    struct promise_type
    {
        promise_type() = default;

        detached_task
        get_return_object() noexcept
        {
            return {std::experimental::coroutine_handle<
                promise_type>::from_promise(*this)};
        }

        constexpr auto initial_suspend() const noexcept
        {
            return std::experimental::suspend_always{};
        }

        constexpr auto final_suspend() const noexcept
        {
            return std::experimental::suspend_never{};
        }

        constexpr void return_void() noexcept
        {
        }

        void unhandled_exception() const noexcept
        {
        }
    };
#endif

    /// Constructor
    detached_task() = default;

    /// Constructor
    detached_task(detached_task&& other)
        : co_(other.co_)
    {
        other.co_ = nullptr;
    }

    /// Destructor
    ~detached_task()
    {
        if(co_)
            co_.destroy();
    }

    /// Assignment
    detached_task&
    operator=(detached_task&& other)
    {
        if(co_)
            co_.destroy();
        co_ = other.co_;
        other.co_ = nullptr;
    }

    /// Resume the contained coroutine.
    void operator()()
    {
        if(co_)
        {
            auto co = co_;
            co_ = nullptr;
            co.resume();
        }
    }

private:
    std::experimental::coroutine_handle<
        promise_type> co_ = nullptr;

    /// Constructor
    detached_task(
        std::experimental::coroutine_handle<
            promise_type> co)
        : co_(co)
    {
    }
};

//------------------------------------------------------------------------------

/** Spawn a coroutine on the specified executor.
*/
template<class Executor>
std::enable_if_t<
    boost::asio::is_executor<Executor>::value>
co_spawn(Executor const& ex, detached_task task)
{
    using result_type = std::invoke_result_t<decltype(task)>; // void for now

    boost::asio::post(
        boost::asio::bind_executor(
            ex, std::move(task)));
}

/** Spawn a coroutine on the specified execution context
*/
template<class ExecutionContext>
std::enable_if_t<std::is_convertible<
    ExecutionContext&, boost::asio::execution_context&>::value>
co_spawn(ExecutionContext& ctx, detached_task task)
{
    co_spawn(ctx.get_executor(), std::move(task));
}

} // beast
} // boost

#endif
