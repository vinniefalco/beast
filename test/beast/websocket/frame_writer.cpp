//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/frame_writer.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace websocket {

namespace {

class stream
{
public:

    template<class ConstBufferSequence>
    auto
    async_write(
        ConstBufferSequence&& m);
};

} // (anon)

struct frame_writer_test
    : beast::unit_test::suite
{
    void run() override
    {
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,frame_writer);

} // websocket
} // beast
} // boost
