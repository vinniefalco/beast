#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(include/boost/beast beast)
GroupSources(example/http/server/cppcoro "/")
GroupSources(cppcoro/include/ "cppcoro")
GroupSources(cppcoro/lib/ "cppcoro-src")

file (GLOB_RECURSE CPPCORO_HEADERS
    ${PROJECT_SOURCE_DIR}/cppcoro/include/*.hpp
)

file (GLOB_RECURSE CPPCORO_SOURCES
    ${PROJECT_SOURCE_DIR}/cppcoro/lib/*.hpp
    ${PROJECT_SOURCE_DIR}/cppcoro/lib/*.cpp
)

include_directories (${PROJECT_SOURCE_DIR}/cppcoro/include)
include_directories (${PROJECT_SOURCE_DIR}/cppcoro/lib/include)

add_compile_options(
    /await      # enable co_await
    #/await:heapelide
    /std:c++latest
)

add_definitions (
  -D_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING
  -D_SILENCE_CXX17_ADAPTOR_TYPEDEFS_DEPRECATION_WARNING
)

add_executable (http-server-cppcoro
    ${BOOST_BEAST_FILES}
    ${CPPCORO_HEADERS}
    ${CPPCORO_SOURCES}
    http_server_cppcoro.cpp
)

set_property(TARGET http-server-cppcoro PROPERTY FOLDER "example-http-server")
