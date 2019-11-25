//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_EXPECTED_HPP
#define BOOST_BEAST_CORE_EXPECTED_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/assert.hpp>
#include <boost/optional.hpp> // for in_place_init
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {

struct bad_expected_access
    : std::exception
{
};

/** Simplified expected<T> which uses error_code
*/
template<class T>
class expected
{
    // To keep things simple, we don't allow
    // T that is convertible to error_code.
    static_assert(
        ! std::is_convertible<T,
            error_code>::value,
        "T requirements not met");

    static_assert(
        ! std::is_reference<T>::value,
        "T& is disallowed");

    union
    {
        T t_;
        error_code ec_;
    };
    bool v_;

    void destroy();
    void move(expected&& other);

public:
    using value_type = T;
    using error_type = error_code;

    //
    // special members
    //

    expected() = delete;

    ~expected();

    expected(expected&& other) noexcept;

    expected(expected const& other);

    expected&
    operator=(expected&& other) noexcept;

    expected&
    operator=(expected const& other);

    //
    // construction
    //

    explicit
    expected(T&& t) noexcept;

    expected(T const& t);

    template<class... Args>
    explicit
    expected(
        boost::in_place_init_t,
        Args&&... args);

    explicit
    expected(error_code ec) noexcept;

#if 0
    template<class U
#ifndef BOOST_BEAST_DOXYGEN
        , class = typename std::enable_if<
            std::is_constructible<error_code, U&&>::value>::type
#endif
    >
    expected(U&& u) noexcept;
#endif

    expected&
    operator=(T const& t) noexcept;

    template<class U
#ifndef BOOST_BEAST_DOXYGEN
        , class = typename std::enable_if<
            std::is_constructible<T, U&&>::value>::type
#endif
    >
    expected&
    operator=(U&& u) noexcept;

    expected&
    operator=(error_code ec) noexcept;

    //
    // observers
    //

    value_type*
    operator->()
    {
        return std::addressof(t_);
    }

    value_type const*
    operator->() const
    {
        return std::addressof(t_);
    }

    value_type const&
    operator*() const&
    {
        return t_;
    }

    value_type&&
    operator*()&&
    {
        return t_;
    }

    explicit
    operator bool() const noexcept
    {
        return v_;
    }

    bool
    has_value() const noexcept
    {
        return v_;
    }

    value_type const&
    value() const&
    {
        if(! v_)
            BOOST_THROW_EXCEPTION(
                bad_expected_access{});
        return t_;
    }

    value_type&
    value()&
    {
        if(! v_)
            BOOST_THROW_EXCEPTION(
                bad_expected_access{});
        return t_;
    }

    value_type&&
    value()&&
    {
        if(! v_)
            BOOST_THROW_EXCEPTION(
                bad_expected_access{});
        return t_;
    }

    error_type const&
    error() const
    {
        BOOST_ASSERT(! has_value());
        return ec_;
    }

    template<class U>
    value_type
    value_or(U&& u) const&
    {
        if(v_)
            return t_;
        return static_cast<T>(
            std::forward<U>(u));
    }

    template<class U>
    value_type
    value_or(U&& u)&&
    {
        if(v_)
            return std::move(t_);
        return static_cast<T>(
            std::forward<U>(u));
    }
};

template<class T>
bool
operator==(
    expected<T> const& lhs,
    expected<T> const& rhs) noexcept;

template<class T>
bool
operator!=(
    expected<T> const& lhs,
    expected<T> const& rhs) noexcept
{
    return !(lhs == rhs);
}

template<class T>
bool
operator==(
    expected<T> const& lhs,
    error_code rhs) noexcept
{
    if(lhs.has_value())
        return false;
    return lhs.error() == rhs;
}

template<class T>
bool
operator==(
    error_code lhs,
    expected<T> const& rhs) noexcept
{
    if(rhs.has_value())
        return false;
    return lhs == rhs.error();
}

template<class T>
bool
operator!=(
    expected<T> const& lhs,
    error_code rhs) noexcept
{
    if(lhs.has_value())
        return true;
    return lhs.error() != rhs;
}

template<class T>
bool
operator!=(
    error_code lhs,
    expected<T> const& rhs) noexcept
{
    if(rhs.has_value())
        return true;
    return lhs != rhs.error();
}

} // beast
} // boost

#include <boost/beast/_experimental/core/impl/expected.hpp>

#endif
