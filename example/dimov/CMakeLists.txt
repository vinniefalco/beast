#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(include/boost/beast beast)
GroupSources(example/dimov "/")

add_executable (dimov
    ${BOOST_BEAST_FILES}
    Jamfile
    dimov.cpp
)

target_link_libraries(dimov
    lib-asio
    lib-beast)

set_property(TARGET dimov PROPERTY FOLDER "example")
