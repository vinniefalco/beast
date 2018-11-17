//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_ASYNC_TASK_HPP
#define BOOST_BEAST_CORE_ASYNC_TASK_HPP

#include <boost/beast/core/detail/config.hpp>
#include <experimental/resumable>
#include <optional>
#include <tuple>

namespace boost {
namespace beast {

namespace detail {

template<class Signature>
struct args_tuple_t;

template<class... Ts>
struct args_tuple_t<void(Ts...)>
{
    using type = std::tuple<Ts...>;
};

template<class... Ts>
using args_tuple = typename args_tuple_t<Ts...>::type;

template<class Signature>
struct abort_args;

template<>
struct abort_args<void(error_code)>
{
    static
    std::tuple<error_code>
    value()
    {
        return {boost::asio::error::operation_aborted};
    }
};

template<>
struct abort_args<void(error_code, std::size_t)>
{
    static
    std::tuple<error_code, std::size_t>
    value()
    {
        return {boost::asio::error::operation_aborted, 0};
    }
};

} // detail

//------------------------------------------------------------------------------

/** Return type of Awaitable/Sender compatible initiating functions.

    @tparam Signature The signature of the handler. For example
    `void(error_code, size_t)`. The value of the co_await
    expression will be the argument list expressed as a tuple.

    @tparam The type of the initiating function. This must be
    invocable as a unary function template which accepts a
    CompletionToken.
*/
template<
    class Signature,
    class InitiatingFunction>
class async_task
{
    detail::args_tuple<Signature> params_;
    InitiatingFunction f_;

    class awaiter_handler
    {
        using args_type =
            std::optional<detail::args_tuple<Signature>>;

        args_type& args_;
        std::experimental::coroutine_handle<> co_;
        bool abort_ = true;

    public:
        awaiter_handler(
            args_type& args,
            std::experimental::coroutine_handle<> co)
            : args_(args)
            , co_(co)
        {
        }

        awaiter_handler(awaiter_handler&& other)
            : args_(other.args_)
            , co_(boost::exchange(other.co_, nullptr))
            , abort_(boost::exchange(other.abort_, false))
        {
        }
        
        ~awaiter_handler()
        {
            if(abort_)
            {
                args_.emplace(detail::abort_args<Signature>::value());
                co_.resume();
            }
        }

        template<class... Args>
        void
        operator()(Args&&... args)
        {
            args_.emplace(std::forward<Args>(args)...);
            abort_ = false;
            co_.resume();
        }
    };

    class awaiter
    {
        InitiatingFunction f_;
        std::optional<detail::args_tuple<Signature>> results_;

    public:
        explicit
        awaiter(async_task& task)
            : f_(std::move(task.f_))
        {
        }

        constexpr
        bool
        await_ready() const noexcept
        {
            return false;
        }

        void
        await_suspend(
            std::experimental::coroutine_handle<> co) noexcept
        {
            f_(awaiter_handler(results_, co));
        }

        beast::detail::args_tuple<Signature>&&
        await_resume()
        {
            return std::move(*results_);
        }
    };

public:
    async_task(async_task&&) = default;

    template<class Deduced>
    explicit
    async_task(Deduced&& f)
        : f_(std::forward<Deduced>(f))
    {
    }

    // CompletionToken interface
    template<class CompletionToken>
    auto
    with_token(CompletionToken&& token) const &&
    {
        return f_(std::forward<CompletionToken>(token));
    }

    // Awaitable interface
    auto
    operator co_await() && noexcept
    {
        return awaiter(*this);
    }
};

/** Returns an async_task.

    This helps perform partial class template argument deduction.
*/
template<
    class Signature,
    class InitiatingFunction>
async_task<Signature, InitiatingFunction>
make_async_task(InitiatingFunction&& f)
{
    return async_task<Signature, InitiatingFunction>(
        std::forward<InitiatingFunction>(f));
}

} // beast
} // boost

#endif
