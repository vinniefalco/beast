#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copytight (c) 2018 jackarain (jack dot wgm at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(include/boost/beast beast)
GroupSources(example/socks "/")

add_executable (socks-client-coro
    ${BOOST_BEAST_FILES}
    Jamfile
    socks_client_coro.cpp
    error.hpp
    socks_client.hpp
    uri.hpp
)

target_link_libraries(socks-client-coro
    lib-asio
    lib-beast)

set_property(TARGET socks-client-coro PROPERTY FOLDER "example")
