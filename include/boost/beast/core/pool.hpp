//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_POOL_HPP
#define BOOST_BEAST_CORE_POOL_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/allocator.hpp>
#include <boost/align/aligned_alloc.hpp>
#include <boost/asio/bind_allocator.hpp>
#include <boost/assert.hpp>
#include <unordered_map>

#include <atomic>

namespace boost {
namespace beast {

class pool
{
    enum
    {
        block_count = 10
    };

    struct block
    {
        void* p = nullptr;
        std::size_t n;  // bytes
        std::size_t a;  // alignment
    };

    block b_[block_count];
    std::atomic<std::size_t> count_ = 0;

public:
    ~pool()
    {
        for(int i = 0; i < block_count; ++i)
        {
            if(b_[i].p)
            {
                --count_;
                alignment::aligned_free(b_[i].p);
            }
        }
        BOOST_ASSERT(count_ == 0);
    }

    void* allocate(std::size_t n, std::size_t a)
    {
        for(int i = 0; i < block_count; ++i)
        {
            auto p = b_[i].p;
            if( p != nullptr &&
                b_[i].n == n &&
                b_[i].a == a)
            {
                b_[i].p = nullptr;
                return p;
            }
        }
        ++count_;
        return alignment::aligned_alloc(a, n);
    }

    void deallocate(void* p, std::size_t n, std::size_t a)
    {
        BOOST_ASSERT(p != nullptr);
        for(int i = 0; i < block_count; ++i)
        {
            if(b_[i].p == nullptr)
            {
                b_[i].p = p;
                b_[i].n = n;
                b_[i].a = a;
                return;
            }
        }
        --count_;
        alignment::aligned_free(p);
    }
};

template<class T>
class pool_allocator
{
    pool* p_ = nullptr;

    template<class U>
    friend class pool_allocator;

public:
    using pointer = T*;
    using const_pointer = T const*;
    using void_pointer = void*;
    using const_void_pointer = void const*;
    using value_type = T;
    using size_type = std::size_t;
    using difference_type =
        typename std::make_signed<size_type>;

    template<class U>
    struct rebind
    {
        using other = pool_allocator<U>;
    };

    pool_allocator() = default;

    explicit
    pool_allocator(
        pool& p) noexcept
        : p_(&p)
    {
    }

    template<class U>
    pool_allocator(
        pool_allocator<U> other) noexcept
        : p_(other.p_)
    {
    }

    pool_allocator& operator=(pool_allocator const&) = default;

    bool operator==(pool_allocator const& other) const noexcept
    {
        return p_ == other.p_;
    }

    bool operator!=(pool_allocator const& other) const noexcept
    {
        return p_ != other.p_;
    }

    pointer allocate(size_type n)
    {
        return reinterpret_cast<pointer>(p_->allocate(
            n * sizeof(value_type), alignof(value_type)));
    }

    void deallocate(pointer p, size_type n)
    {
        p_->deallocate(p,
            n * sizeof(value_type), alignof(value_type));
    }
};

template<class T>
using pool_handler_type = asio::allocator_binder<
    typename std::decay<T>::type,
    pool_allocator<char> >;

template<class T>
pool_handler_type<T>
pool_handler(pool& p, T&& t)
{
    return asio::bind_allocator(
        pool_allocator<char>(p),
        std::forward<T>(t));
}

} // beast
} // boost

#endif
