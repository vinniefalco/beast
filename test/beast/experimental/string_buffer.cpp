//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/_experimental/core/string_buffer.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/core/dynamic_buffer_ref.hpp>
#include <boost/beast/core/ostream.hpp>
#include <boost/asio/read_until.hpp>

#include "../core/test_buffer.hpp"

namespace boost {
namespace beast {

class string_buffer_test
    : public beast::unit_test::suite
{
public:
    void
    testMembers()
    {
        string_buffer b;
        ostream(b) << "Hello, world!";
        BEAST_EXPECT(b.get() == "Hello, world!");
        std::string s = b.release();
        BEAST_EXPECT(s == "Hello, world!");
        BEAST_EXPECT(b.size() == 0);
    }

    //--------------------------------------------------------------------------

    template <class SyncReadStream>
    std::string get_line (SyncReadStream& stream)
    {
        string_buffer buffer;
        net::read_until(stream, dynamic_buffer_ref(buffer), "\n");
        return buffer.release();
    }

    void
    testJavadoc()
    {
        BEAST_EXPECT(&string_buffer_test::get_line<test::stream>);
    }

    //--------------------------------------------------------------------------

    void
    run() override
    {
        {
            string_buffer b;
            b.max_size(30);
            test_dynamic_buffer(b);
        }

        testMembers();
        testJavadoc();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,string_buffer);

} // beast
} // boost
