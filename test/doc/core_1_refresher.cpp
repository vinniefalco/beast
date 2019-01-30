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
        assert(string_view(reinterpret_cast<char const*>(cb.data()), cb.size()) == "Hello, world!");

        char storage[13];
        net::mutable_buffer mb(storage, sizeof(storage));
        std::memcpy(mb.data(), cb.data(), mb.size());
        assert(string_view(reinterpret_cast<char const*>(mb.data()), mb.size()) == "Hello, world!");
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
