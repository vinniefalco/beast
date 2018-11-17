//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_TASK_HPP
#define BOOST_BEAST_CORE_TASK_HPP

#include <boost/beast/core/detail/config.hpp>
#include <experimental/resumable>
#include <atomic>
#include <exception>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace boost {
namespace beast {

template<class T>
class task;

//------------------------------------------------------------------------------

namespace detail {

class task_promise_base
{
    struct final_awaitable
    {
        bool
        await_ready() const noexcept
        {
            return false;
        }

        template<class Promise>
        void
        await_suspend(
            std::experimental::coroutine_handle<Promise> co)
        {
            task_promise_base& promise = co.promise();
            if(promise.b_.exchange(true, std::memory_order_acq_rel))
                promise.co_.resume();
        }

        void
        await_resume() noexcept
        {
        }
    };

public:
    task_promise_base() noexcept
        : b_(false)
    {
    }

    constexpr auto
    initial_suspend() noexcept
    {
        return std::experimental::suspend_always{};
    }

    auto
    final_suspend() noexcept
    {
        return final_awaitable{};
    }

    bool
    try_attach(std::experimental::coroutine_handle<> co)
    {
        co_ = co;
        return ! b_.exchange(true, std::memory_order_acq_rel);
    }

private:
    std::atomic<bool> b_;
    std::experimental::coroutine_handle<> co_ = nullptr;
};

//------------------------------------------------------------------------------

template<class T>
class task_promise final
    : public detail::task_promise_base
{
    enum class kind
    {
        empty,
        value,
        exception
    };

    kind k_ = kind::empty;
    union
    {
        T v_;
        std::exception_ptr e_;
    };

public:
    task_promise() noexcept
    {
    }

    ~task_promise()
    {
        switch(k_)
        {
        case kind::value:
            v_.~T();
            break;
        case kind::exception:
            e_.~exception_ptr();
            break;
        default:
            break;
        }
    }

    task<T>
    get_return_object() noexcept;

    void
    unhandled_exception() noexcept
    {
        ::new(static_cast<void*>(
            std::addressof(e_))) std::exception_ptr(
                std::current_exception());
        k_ = kind::exception;
    }

    template<
        class U,
        class = std::enable_if_t<
            std::is_convertible_v<U&&, T>>>
    void
    return_value(U&& u)
        noexcept(std::is_nothrow_constructible_v<T, U&&>)
    {
        ::new (static_cast<void*>(
            std::addressof(v_))) T(
                std::forward<U>(u));
        k_ = kind::value;
    }

    T&
    result() &
    {
        if(k_ == kind::exception)
            std::rethrow_exception(e_);
        return v_;
    }

    // See https://github.com/lewissbaker/cppcoro/issues/40#issuecomment-326864107
    using rvalue_type = std::conditional_t<
        std::is_arithmetic_v<T> || std::is_pointer_v<T>,
            T, T&&>;

    rvalue_type
    result() &&
    {
        if(k_ == kind::exception)
            std::rethrow_exception(e_);
        return std::move(v_);
    }
};

//------------------------------------------------------------------------------

template<>
class task_promise<void> final
    : public detail::task_promise_base
{
    std::exception_ptr e_;

public:
    task_promise() noexcept = default;

    task<void>
    get_return_object() noexcept;

    void
    unhandled_exception() noexcept
    {
        e_ = std::current_exception();
    };

    void
    return_void() noexcept
    {
    }

    void
    result()
    {
        if(e_)
            std::rethrow_exception(e_);
    }
};

//------------------------------------------------------------------------------

template<class T>
class task_promise<T&> final
    : public detail::task_promise_base
{
    T* v_ = nullptr;
    std::exception_ptr e_;

public:
    task_promise() noexcept = default;

    task<T&>
    get_return_object() noexcept;

    void
    unhandled_exception() noexcept
    {
        e_ = std::current_exception();
    };

    void
    return_value(T& v) noexcept
    {
        v_ = std::addressof(v);
    }

    T&
    result()
    {
        if(e_)
            std::rethrow_exception(e_);
        return *v_;
    }
};

} // detail

//------------------------------------------------------------------------------

template<
    class T>
    //class Executor = boost::asio::strand<boost::asio::executor>>
class task
{
public:
    using value_type = T;
    //using executor_type = Executor;
    using promise_type = detail::task_promise<T>;

private:
    struct awaitable_base
    {
        std::experimental::coroutine_handle<promise_type> co_;

        awaitable_base(
            std::experimental::coroutine_handle<promise_type> co)
            : co_(co)
        {
        }

        bool
        await_ready() const noexcept
        {
            return ! co_ || co_.done();
        }

        bool
        await_suspend(
            std::experimental::coroutine_handle<void> co) const noexcept
        {
            // VFALCO Has to use executor...
            co_.resume();
            return co_.promise().try_attach(co);
        }
    };

public:
    task(task const&) = delete;
    task& operator=(task const&) = delete;

    task() noexcept
        : co_(nullptr)
    {
    }

    explicit
    task(std::experimental::coroutine_handle<promise_type> co)
        : co_(co)
    {
    }

    task(task&& other)
        : co_(boost::exchange(other.co_, nullptr))
    {
    }

    task&
    operator=(task&& other)
    {
        if(std::addressof(other) != this)
        {
            if(co_)
                co_.destroy();
            co_ = boost::exchange(other.co_, nullptr);
        }
        return *this;
    }

    ~task()
    {
        if(co_)
            co_.destroy();
    }

    bool
    is_ready() const noexcept
    {
        return ! co_ || co_.done();
    }

    auto
    operator co_await() const & noexcept
    {
        struct awaitable : awaitable_base
        {
            using awaitable_base::awaitable_base;

            decltype(auto)
            await_resume()
            {
                if(! co_)
                    throw std::logic_error("broken promise");
                return co_.promise().result();
            }
        };
        return awaitable{co_};
    }

    auto
    operator co_await() const && noexcept
    {
        struct awaitable : awaitable_base
        {
            using awaitable_base::awaitable_base;

            decltype(auto)
            await_resume()
            {
                if(! co_)
                    throw std::logic_error("broken promise");
                return std::move(co_.promise()).result();
            }
        };
        return awaitable{co_};
    }

    auto
    when_ready() const noexcept
    {
        struct awaitable : awaitable_base
        {
            using awaitable_base::awaitable_base;

            void
            await_resume() const noexcept
            {
            }
        };
        return awaitable{co_};
    }

private:
    std::experimental::coroutine_handle<promise_type> co_;
};

//------------------------------------------------------------------------------

namespace detail {

inline
task<void>
task_promise<void>::
get_return_object() noexcept
{
    return task<void>{
        std::experimental::coroutine_handle<
            task_promise>::from_promise(*this)};
}

template<class T>
task<T&>
task_promise<T&>::
get_return_object() noexcept
{
    return task<T&>{
        std::experimental::coroutine_handle<
            task_promise>::from_promise(*this)};
}

template<class T>
task<T>
task_promise<T>::
get_return_object() noexcept
{
    return task<T>{
        std::experimental::coroutine_handle<
            task_promise>::from_promise(*this)};
}

} // detail

} // beast
} // boost

#endif
