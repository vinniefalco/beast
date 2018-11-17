//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TASKS_HPP
#define BOOST_BEAST_TASKS_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/serializer.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <experimental/resumable>
#include <memory>
#include <utility>

#include "async_task.hpp"

namespace boost {
namespace beast {

//------------------------------------------------------------------------------

namespace http {

namespace detail {

template<
    class Stream,
    class Handler,
    bool isRequest, class Body, class Fields>
class op_write_msg
{
    struct data
    {
        Stream& s;
        boost::asio::executor_work_guard<decltype(
            std::declval<Stream&>().get_executor())> wg;
        message<isRequest, Body, Fields> m;
        serializer<isRequest, Body, Fields> sr;

        data(
            Handler const&,
            Stream& s_,
            message<isRequest, Body, Fields>&& m_)
            : s(s_)
            , wg(s.get_executor())
            , m(std::move(m_))
            , sr(m)
        {
        }
    };

    handler_ptr<data, Handler> d_;

public:
    op_write_msg(op_write_msg&&) = default;
    op_write_msg(op_write_msg const&) = delete;

    template<class DeducedHandler, class... Args>
    op_write_msg(DeducedHandler&& h,
        Stream& s, Args&&... args)
        : d_(std::forward<DeducedHandler>(h),
            s, std::forward<Args>(args)...)
    {
    }

    using allocator_type =
        boost::asio::associated_allocator_t<Handler>;

    allocator_type
    get_allocator() const noexcept
    {
        return (boost::asio::get_associated_allocator)(d_.handler());
    }

    using executor_type = boost::asio::associated_executor_t<
        Handler, decltype(std::declval<Stream&>().get_executor())>;

    executor_type
    get_executor() const noexcept
    {
        return (boost::asio::get_associated_executor)(
            d_.handler(), d_->s.get_executor());
    }

    void
    operator()();

    void
    operator()(
        error_code ec, std::size_t bytes_transferred);

    friend
    bool asio_handler_is_continuation(op_write_msg* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->d_.handler()));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, op_write_msg* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(op->d_.handler()));
    }
};

template<class Stream, class Handler,
    bool isRequest, class Body, class Fields>
void
op_write_msg<
    Stream, Handler, isRequest, Body, Fields>::
operator()()
{
    auto& d = *d_;
    return async_write(d.s, d.sr, std::move(*this));
}

template<class Stream, class Handler,
    bool isRequest, class Body, class Fields>
void
op_write_msg<
    Stream, Handler, isRequest, Body, Fields>::
operator()(error_code ec, std::size_t bytes_transferred)
{
    auto wg = std::move(d_->wg);
    d_.invoke(ec, bytes_transferred);
}

template<
    class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
    class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code, std::size_t))
async_write_msg(
    AsyncWriteStream& stream,
    message<isRequest, Body, Fields>&& msg,
    WriteHandler&& handler)
{
    static_assert(
        is_async_write_stream<AsyncWriteStream>::value,
        "AsyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        WriteHandler, void(error_code, std::size_t));
    op_write_msg<
        AsyncWriteStream,
        BOOST_ASIO_HANDLER_TYPE(WriteHandler,
            void(error_code, std::size_t)),
        isRequest, Body, Fields>{
            std::move(init.completion_handler), stream,
                std::move(msg)}();
    return init.result.get();
}

} // detail

//------------------------------------------------------------------------------

/** Task-based version of async_read
*/
template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
auto
async_read(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser)
{
    static_assert(is_async_read_stream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(! parser.is_done());
    return make_async_task<
        void(error_code, std::size_t)>(
        [&](auto&& token)
        {
            using CompletionToken =
                std::decay_t<decltype(token)>;
            return async_read(stream, buffer, parser,
                std::forward<CompletionToken>(token));
        });
}

/** Task-based version of async_write
*/
template<
    class AsyncWriteStream,
    bool isRequest, class Body, class Fields>
auto
async_write(
    AsyncWriteStream& stream,
    message<isRequest, Body, Fields>&& msg)
{
    static_assert(is_async_write_stream<AsyncWriteStream>::value,
        "AsyncWriteStream requirements not met");
    return make_async_task<
        void(error_code, std::size_t)>(
        [&](auto&& token)
        {
            using CompletionToken =
                std::decay_t<decltype(token)>;
            return detail::async_write_msg(stream,
                std::move(msg), std::forward<
                    CompletionToken>(token));
        });
}

} // http

//------------------------------------------------------------------------------



//------------------------------------------------------------------------------

} // beast
} // boost

#endif
