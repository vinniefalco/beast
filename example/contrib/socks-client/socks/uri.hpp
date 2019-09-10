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

    class qs_iterator
    {
    public:
        using value_type = std::pair<string_view, string_view>;
        using difference_type = std::ptrdiff_t;
        using reference = value_type&;
        using pointer = const value_type*;
        using iterator_category = std::forward_iterator_tag;

        qs_iterator() = default;

        explicit
        qs_iterator(const string_view& s) : qs_(s)
        {
            qs_parse();
        }

        ~qs_iterator() = default;

        reference
        operator*() const noexcept
        {
            return const_cast<reference>(value_);
        }

        pointer
        operator->() const noexcept
        {
            return &value_;
        }

        qs_iterator&
        operator++() noexcept
        {
            increment();
            return *this;
        }

        qs_iterator
        operator++(int) noexcept
        {
            qs_iterator tmp = *this;
            increment();
            return tmp;
        }

        bool
        operator==(const qs_iterator &other) const noexcept
        {
            if ((value_.first.data() == other.value_.first.data() &&
                value_.first.size() == other.value_.first.size()) &&
                (value_.second.data() == other.value_.second.data() &&
                value_.second.size() == other.value_.second.size()))
                return true;
            return false;
        }

        bool
        operator!=(const qs_iterator &other) const noexcept
        {
            return !(*this == other);
        }

        string_view
        key() const noexcept
        {
            return value_.first;
        }

        string_view
        value() const noexcept
        {
            return value_.second;
        }

    protected:
        BOOST_BEAST_DECL
        void
        qs_parse() noexcept;

        BOOST_BEAST_DECL
        void
        increment();

    protected:
        string_view qs_;
        value_type value_;
    };

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
