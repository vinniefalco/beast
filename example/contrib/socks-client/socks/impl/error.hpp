//
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_EXAMPLE_CONTRIB_SOCKS_IMPL_ERROR_HPP
#define BOOST_BEAST_EXAMPLE_CONTRIB_SOCKS_IMPL_ERROR_HPP

#include <type_traits>

namespace boost {
namespace system {
template<>
struct is_error_code_enum<::socks::error>
{
    static bool const value = true;
};
} // system
} // boost

namespace socks {

BOOST_BEAST_DECL
error_code
make_error_code(error e);

} // socks

#endif
