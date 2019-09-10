//
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_EXAMPLE_CONTRIB_SOCKS_CLIENT_IMPL_QUERY_IPP
#define BOOST_BEAST_EXAMPLE_CONTRIB_SOCKS_CLIENT_IMPL_QUERY_IPP

#include <boost/assert.hpp>
#include <string>
#include <stdexcept>
#include <iterator>

namespace socks {

void
qs_iterator::
qs_parse() noexcept
{
    auto b = qs_.data();
    auto e = qs_.data() + qs_.size();
    auto start = b;

    enum {
        qs_start,
        qs_key,
        qs_value,
    };

    int state = qs_start;

    while (b != e)
    {
        char c = *b++;
        bool eol = b == e;

        switch (state)
        {
        case qs_start:
            if (c == '&')
                continue;
            state = qs_key;
            start = b - 1;
            continue;
        case qs_key:
            if (c == '=')
            {
                value_.first = string_view(start, b - start - 1); // skip '='
                value_.second = string_view(b, 0);
                start = b;
                state = qs_value;
                continue;
            }
            if (c == '&')
            {
                value_.first = string_view(start, b - start - 1); // skip '&' symbol
                value_.second = string_view(b, 0);
                return;
            }
            if (eol)
            {
                value_.first = string_view(start, b - start);
                return;
            }
            continue;
        case qs_value:
            if (c == '&')
            {
                value_.second = string_view(start, b - start - 1); // skip '&' symbol
                return;
            }
            if (eol)
            {
                value_.second = string_view(start, b - start);
                return;
            }
            continue;
        default:
            BOOST_ASSERT(false);
            break;
        }
    }
}

void
qs_iterator::
increment()
{
    auto start_ptr = value_.second.data();
    auto offset = value_.second.size();

    value_ = {};

    if (!start_ptr)
        return;

    auto next_part = start_ptr + offset + 1;
    auto end = qs_.data() + qs_.size();

    if (end <= next_part)
        return;

    qs_ = string_view(next_part, end - next_part);
    qs_parse();
}

} // socks

#endif
