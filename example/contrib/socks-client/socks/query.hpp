//
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_EXAMPLE_CONTRIB_SOCKS_QUERY_HPP
#define BOOST_BEAST_EXAMPLE_CONTRIB_SOCKS_QUERY_HPP

#include <socks/config.hpp>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace socks {

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

} // socks

#include <socks/impl/query.ipp>

#endif
