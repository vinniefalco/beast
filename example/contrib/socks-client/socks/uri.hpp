//
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_EXAMPLE_CONTRIB_SOCKS_CLIENT_URI_HPP
#define BOOST_BEAST_EXAMPLE_CONTRIB_SOCKS_CLIENT_URI_HPP

#include <socks/config.hpp>
#include <socks/query.hpp>
#include <string>
#include <stdexcept>
#include <iterator>

namespace socks {

class uri
{
public:
    uri() = default;
    ~uri() = default;

    BOOST_BEAST_DECL
    uri(const char* s);

    BOOST_BEAST_DECL
    uri(const std::string& s);

    BOOST_BEAST_DECL
    uri(string_view s);

    string_view
    scheme() noexcept
    {
        return scheme_;
    }

    string_view
    host() noexcept
    {
        return host_;
    }

    string_view
    port() noexcept
    {
        if (!port_.empty())
            return port_;
        return known_port();
    }

    string_view
    username() noexcept
    {
        return username_;
    }

    string_view
    password() noexcept
    {
        return password_;
    }

    string_view
    path() noexcept
    {
        return path_;
    }

    string_view
    query() noexcept
    {
        return query_;
    }

    string_view
    fragment() noexcept
    {
        return fragment_;
    }

    BOOST_BEAST_DECL
    bool
    parse(string_view url) noexcept;

    BOOST_BEAST_DECL
    static
    std::string
    encodeURI(string_view str) noexcept;

    BOOST_BEAST_DECL
    static
    std::string
    decodeURI(string_view str);

    BOOST_BEAST_DECL
    static
    std::string
    encodeURIComponent(string_view str) noexcept;

    BOOST_BEAST_DECL
    static
    std::string
    decodeURIComponent(string_view str);

    qs_iterator
    qs_begin() const noexcept
    {
        return qs_iterator(query_);
    }

    qs_iterator
    qs_end() const noexcept
    {
        return qs_iterator();
    }

    struct qs_range
    {
        qs_iterator
        begin() const noexcept
        {
            return begin_;
        }

        qs_iterator
        end() const noexcept
        {
            return end_;
        }

        qs_iterator begin_;
        qs_iterator end_;
    };

    qs_range
    qs()
    {
        return {qs_begin(), qs_end()};
    }

private:
    BOOST_BEAST_DECL
    string_view
    known_port() noexcept;

private:
    string_view scheme_;
    string_view username_;
    string_view password_;
    string_view host_;
    string_view port_;
    string_view path_;
    string_view query_;
    string_view fragment_;
};

} // socks

#include <socks/impl/uri.ipp>

#endif
