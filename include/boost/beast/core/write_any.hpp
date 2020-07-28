//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WRITE_ANY_HPP
#define BOOST_BEAST_WRITE_ANY_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/coroutine.hpp>

namespace boost {
namespace beast {

namespace detail {

template<
    class Buffers,
    class Handler>
class any_write_some_op_impl
{
    Buffers buffers_;
    Handler handler_;
    net::const_buffer buf_[64];

public:
    any_write_some_op_impl(
        Buffers const& buffers)
        : buffers_(buffers)
    {
        auto it =
            net::buffer_sequence_begin(
                buffers_);
        auto const last =
            net::buffer_sequence_end(
                buffers_);
        auto const n =
            std::distance(it, last);
        for(int i = 0; i < n; ++i)
            buf_[i] = *it++;
    }

    void
    invoke(
        error_code ec,
        std::size_t bytes_transferred) override
    {
        Handler handler(std::move(handler_));
        // deallocate
        handler_(ec, bytes_transferred);
    }
};

template<
    class AsyncWriteStream
>
struct any_write_some_op
    : boost::asio::coroutine
{
    AsyncWriteStream& stream_;

    template<
        class Buffers,
        class Handler>
    any_write_some_op(
        AsyncWriteStream& stream,
        Buffers const& buffers,
        Handler&& handler)
        : stream_(stream)
        , 
    {
    }

    void
    operator()(
        error_code ec = {},
        std::size_t n = 0)
    {
        BOOST_ASIO_CORO_REENTER(*this)
        {
            BOOST_ASIO_CORO_YIELD
        }
    }
};

} // detail

template<
    class AsyncWriteStream,
    class ConstBufferSequence,
    class CompletionToken
#if 1
    ,typename std::enable_if<
        net::is_const_buffer_sequence<
            ConstBufferSequence
                >::value>::type* = nullptr
#endif
>
auto
async_any_write_some(
    AsyncWriteStream& stream,
    ConstBufferSequence const& buffers,
    CompletionToken&& token) ->
        typename net::async_result<
            typename std::decay<CompletionToken>::type,
            void(error_code, std::size_t)>::return_type
{
    return asio::async_compose<
        CompletionToken,
        void(error_code, std::size_t)>(
        detail::any_write_some_op<
            AsyncWriteStream,
            BuffersGenerator>{
                stream,
                std::move(generator)},
            token,
            stream);
}

} // beast
} // boost

#endif
