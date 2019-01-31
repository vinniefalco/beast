//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include "snippets.hpp"

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/asio/write.hpp>
#include <assert.h>

namespace boost {
namespace beast {

namespace {

void
snippets()
{
    #include "snippets.ipp"
    {
    //[code_core_1_refresher_1
        net::const_buffer cb("Hello, world!", 13);
        assert(string_view(reinterpret_cast<char const*>(
            cb.data()), cb.size()) == "Hello, world!");

        char storage[13];
        net::mutable_buffer mb(storage, sizeof(storage));
        std::memcpy(mb.data(), cb.data(), mb.size());
        assert(string_view(reinterpret_cast<char const*>(
            mb.data()), mb.size()) == "Hello, world!");
    //]
    }
    {
    //[code_core_1_refresher_2
        net::const_buffer b1;
        net::mutable_buffer b2;
        std::array<net::const_buffer, 3> b3;
    //]
    }
    {
    //[code_core_1_refresher_5
        net::async_write(sock, net::const_buffer("Hello, world!", 13),
            [](error_code ec, std::size_t bytes_transferred)
            {
                if(! ec)
                    assert(bytes_transferred == 13);
                else
                    std::cerr << "Error: " << ec.message() << "\n";
            });
    //]
    }
    {
    //[code_core_1_refresher_7
        std::future<std::size_t> f = net::async_write(sock,
            net::const_buffer("Hello, world!", 13), net::use_future);
    //]
    }
    {
    //[code_core_1_refresher_8
        net::spawn(
            [&sock](net::yield_context yield)
            {
                std::size_t bytes_transferred = net::async_write(sock,
                    net::const_buffer("Hello, world!", 13), yield);
                (void)bytes_transferred;
            });
    //]
    }
}

//[code_core_1_refresher_3
template <class SyncWriteStream>
void hello (SyncWriteStream& stream)
{
    net::const_buffer cb(net::const_buffer("Hello, world!", 13));
    do
    {
        auto bytes_transferred = stream.write_some(cb); // may throw
        cb += bytes_transferred; // adjust the pointer and size
    }
    while (cb.size() > 0);
}
//]

//[code_core_1_refresher_4
template <class SyncWriteStream>
void hello (SyncWriteStream& stream, error_code& ec)
{
    net::const_buffer cb("Hello, world!", 13);
    do
    {
        auto bytes_transferred = stream.write_some(cb, ec);
        cb += bytes_transferred; // adjust the pointer and size
    }
    while (cb.size() > 0 && ! ec);
}
//]

//[code_core_1_refresher_6
template <class AsyncWriteStream, class WriteHandler>
void async_hello (AsyncWriteStream& stream, WriteHandler&& handler)
{
    net::async_write (stream,
        net::buffer("Hello, world!", 13),
        std::forward<WriteHandler>(handler));
}
//]

//[code_core_1_refresher_9
template<
    class AsyncWriteStream,
    class ConstBufferSequence,
    class WriteHandler>
auto
async_write(
    AsyncWriteStream& stream,
    ConstBufferSequence const& buffers,
    WriteHandler&& handler) ->
        typename net::async_result<                     // return-type customization point
            typename std::decay<WriteHandler>::type,    // type used to specialize async_result
            void(error_code, std::size_t)               // signature of the corresponding completion handler
                >::return_type
{
    net::async_completion<
        WriteHandler,                                   // completion handler customization point
        void(error_code, std::size_t)                   // signature of the corresponding completion handler
            > init(handler);                            // variable which holds the corresponding completion handler

    (void)init.completion_handler;                      // the underlying completion handler used for the operation

    // ...launch the operation (omitted for clarity)

    return init.result.get();
}
//]

} // (anon)

struct core_1_refresher_test
    : public beast::unit_test::suite
{
    struct handler
    {
        void operator()(error_code, std::size_t)
        {
        }
    };

    void
    run() override
    {
        BEAST_EXPECT(&snippets);

        BEAST_EXPECT(static_cast<
            void(*)(test::stream&)>(
            &hello<test::stream>));

        BEAST_EXPECT(static_cast<
            void(*)(test::stream&, error_code&)>(
            &hello<test::stream>));

        BEAST_EXPECT((&async_hello<test::stream, handler>));
    }
};

BEAST_DEFINE_TESTSUITE(beast,doc,core_1_refresher);

} // beast
} // boost
