//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_IMPL_EXPECTED_HPP
#define BOOST_BEAST_CORE_IMPL_EXPECTED_HPP

namespace boost {
namespace beast {

template<class T>
void
expected<T>::
destroy()
{
    if(v_)
        t_.~T();
    else
        ec_.~error_code();
}

template<class T>
void
expected<T>::
move(expected&& other)
{
    v_ = other.v_;
    if(v_)
        ::new(std::addressof(t_)) T(
            std::move(other.t_));
    else
        ::new(&ec_)
            error_code(other.ec_);
}

//
// special members
//

template<class T>
expected<T>::
~expected()
{
    destroy();
}

template<class T>
expected<T>::
expected(expected&& other) noexcept
{
    move(std::move(other));
}

template<class T>
expected<T>::
expected(expected const& other)
    : v_(other.v_)
{
    if(v_)
        ::new(std::addressof(t_)) T(other.t_);
    else
        ::new(&ec_) 
            error_code(other.ec_);
}

template<class T>
expected<T>&
expected<T>::
operator=(expected&& other) noexcept
{
    destroy();
    move(std::move(other));
    return *this;
}

template<class T>
expected<T>&
expected<T>::
operator=(expected const& other)
{
    struct cleanup
    {
        expected* self;

        ~cleanup()
        {
            if(self)
            {
                self->v_ = false;
                ::new(&self->ec_)
                    error_code(
                        make_error_code(
                            errc::invalid_argument));
            }
        }
    };

    destroy();
    if(other.v_)
    {
        cleanup c{this};
        ::new(std::addressof(
            t_)) T(other.t_);
        c.self = nullptr;
    }
    else
    {
        ::new(&ec_)
            error_code(other.ec_);
    }
    v_ = other.v_;
    return *this;
}

//
// construction
//

template<class T>
expected<T>::
expected(T&& t) noexcept
    : v_(true)
{
    ::new(std::addressof(t_)) T(
        std::move(t));
}

template<class T>
expected<T>::
expected(T const& t)
    : v_(true)
{
    ::new(std::addressof(t_)) T(t);
}

template<class T>
template<class... Args>
expected<T>::
expected(
    boost::in_place_init_t,
    Args&&... args)
    : t_(std::forward<Args>(args)...)
    , v_(true)
{
}

template<class T>
expected<T>::
expected(error_code ec) noexcept
    : v_(false)
{
    ::new(&ec_) error_code(ec);
}

#if 0
template<class T>
template<class U, class>
expected<T>::
expected(U&& u) noexcept
    : expected(error_code(u))
{
}
#endif

template<class T>
template<class U, class>
expected<T>&
expected<T>::
operator=(U&& u) noexcept
{
    destroy();
    ::new(std::addressof(t_)) T(
        std::move(u));
    v_ = true;
    return *this;
}

template<class T>
expected<T>&
expected<T>::
operator=(T const& t) noexcept
{
    struct cleanup
    {
        expected* self;

        ~cleanup()
        {
            if(self)
            {
                self->v_ = false;
                ::new(&self->ec_)
                    error_code(
                        make_error_code(
                            errc::invalid_argument));
            }
        }
    };

    cleanup c{this};
    ::new(std::addressof(
        t_)) T(t);
    c.self = nullptr;
    v_ = true;
    return *this;
}

template<class T>
expected<T>&
expected<T>::
operator=(error_code ec) noexcept
{
    destroy();
    ::new(&ec_) error_code(ec);
    v_ = false;
    return *this;
}

//------------------------------------------------------------------------------

template<class T>
bool
operator==(
    expected<T> const& lhs,
    expected<T> const& rhs) noexcept
{
    if(lhs.has_value())
    {
        if(! rhs.has_value())
            return false;
        return *lhs == *rhs;
    }
    if(rhs.has_value())
        return false;
    return
        lhs.error() ==
        rhs.error();
}

} // beast
} // boost

#endif
