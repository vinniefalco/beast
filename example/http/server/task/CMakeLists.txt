#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(include/boost/beast beast)
GroupSources(example/http/server/task "/")

add_compile_options(
    /await      # enable co_await
    #/await:heapelide
    /std:c++latest
)

add_definitions (
  -D_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING
  -D_SILENCE_CXX17_ADAPTOR_TYPEDEFS_DEPRECATION_WARNING
)

add_executable (http-server-task
    ${BOOST_BEAST_FILES}
    http_server_task.cpp
    task.hpp
    async_task.hpp
    spawn.hpp
    tasks.hpp
)

set_property(TARGET http-server-task PROPERTY FOLDER "example-http-server")
