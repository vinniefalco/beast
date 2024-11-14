//
// Copyright (c) 2024 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/asio.hpp>

namespace net = boost::asio;

//------------------------------------------------

// submits 3 requests, receives 3 responses, then destroys
class client
{
    net::execution_context& exc_;

public:

};

//------------------------------------------------

int main(int argc, char** argv)
{
    return EXIT_SUCCESS;
}
