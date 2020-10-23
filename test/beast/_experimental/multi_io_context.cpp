//
// Copyright (c) 2020 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/_experimental/core/multi_io_context.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {

class multi_io_context_test
    : public unit_test::suite
{
public:
    void
    testContext()
    {
        multi_io_context ioc(2);
    }

    void
    run() override
    {
        testContext();
        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,multi_io_context);

} // beast
} // boost
