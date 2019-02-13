//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_STRING_BUFFER_HPP
#define BOOST_BEAST_STRING_BUFFER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/core/exchange.hpp>
#include <boost/assert.hpp>
#include <memory>
#include <stdexcept>
#include <string>

namespace boost {
namespace beast {

/** A dynamic buffer which uses a string for the internal storage.

    This dynamic buffer uses a standard string for the internal
    storage. It otherwise behaves, and meets the requirements for,
    the <em>DynamicBuffer</em> named requirements.

    The caller may view the readable bytes as a `string_view`, or
    optionally take ownership of the string itself by calling
    @ref release.

    In the current implementation of Networking, algorithms which
    operate on dynamic buffers take ownership of the buffer,
    presenting a problem when implementing composed operations.
    To work around this, the function @ref dynamic_buffer_ref
    returns a lightweight movable reference to a dynamic buffer,
    which may be passed to networking algorithms which expect
    to take ownership.

    @par Example
    This example demonstrates how to read a line of text into a
    @ref string_buffer and return it to the caller as a string,
    using the networking algorithm `net::read_until` which wants
    to take ownership of the buffer:
    @code
    template <class SyncReadStream>
    std::string get_line (SyncReadStream& stream)
    {
        string_buffer buffer;
        net::read_until(stream, dynamic_buffer_ref(buffer), "\n");
        return buffer.release();
    }
    @endcode

    @tparam CharT The type of character in the string.
    For @ref string_buffer this will be `char`.

    @tparam Traits The character traits to use.
    This will default to `std::char_traits<CharT>`.

    @tparam The allocator to use.
    This will default to `std::allocator<CharT>`.

    @see
    
    @li @ref string_buffer

    @li @ref wstring_buffer

    @li <a href="http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1100r0.html">[P1100R0] Efficient composition with DynamicBuffer</a>
*/
template<
    class CharT,
    class Traits = std::char_traits<CharT>,
    class Allocator = std::allocator<CharT>
>
class basic_string_buffer
{
public:
    /// The type of string used by the dynamic buffer
    using value_type =
        std::basic_string<CharT, Traits, Allocator>;

    /// The type used to represent integral sizes
    using size_type = typename value_type::size_type;

private:

/*  Layout:

        0        in_          out_      s_.size()    s_.capacity()
        |<------->|<---------->|<---------->|<----------->|
                  |  readable  |  writable  |
*/

    std::basic_string<CharT, Traits, Allocator> s_;
    size_type max_ =
        (std::numeric_limits<std::size_t>::max)();
    size_type in_ = 0;
    size_type out_ = 0;

public:
    /// The ConstBufferSequence used to represent the readable bytes.
    using const_buffers_type = net::const_buffer;

    /// The MutableBufferSequence used to represent the readable bytes.
    using mutable_data_type = net::mutable_buffer;

    /// The MutableBufferSequence used to represent the writable bytes.
    using mutable_buffers_type = net::mutable_buffer;

    /// Constructor
    basic_string_buffer() = default;

    /// Copy Constructor
    basic_string_buffer(
        basic_string_buffer const&) = default;

    /// Copy Assignment
    basic_string_buffer& operator=(
        basic_string_buffer const&) = default;

    /** Move Constructor

        After the move, the moved-from object will be as if default constructed.

        @par Exception Safety

        No-throw guarantee.
    */
    basic_string_buffer(
        basic_string_buffer&& other) noexcept
        : s_(std::move(other.s_))
        , max_(other.max_)
        , in_(boost::exchange(other.in_, 0))
        , out_(boost::exchange(other.out_, 0))
    {
    }

    /** Move Assignment

        After the move, the moved-from object will be as if default constructed.

        @par Exception Safety

        No-throw guarantee.
    */
    basic_string_buffer& operator=(
        basic_string_buffer&& other) noexcept
    {
        s_   = std::move(other.s_);
        max_ = other.max_;
        in_  = boost::exchange(other.in_, 0);
        out_ = boost::exchange(other.out_, 0);
        return *this;
    }

    /// Return the input area as a string view
    string_view
    get() const noexcept
    {
        return { s_.data() + in_, size() };
    }

    /** Returns the internal string by transferring ownership to the caller.

        After the call, the internal string will be empty.
        All buffer sequences previously obtained using @ref prepare are
        invalidated. Buffer sequences previously obtained using @ref data
        remain valid.
    */
    value_type
    release() noexcept
    {
        if(in_ > 0)
        {
            BOOST_ASSERT(out_ != in_);
            std::memmove(&s_[0], &s_[in_], size());
        }
        s_.resize(size());
        in_ = 0;
        out_ = 0;
        return std::move(s_);
    }

    /// Returns the number of readable bytes.
    size_type
    size() const noexcept
    {
        return out_ - in_;
    }

    /// Return the maximum number of bytes, both readable and writable, that can ever be held.
    size_type
    max_size() const noexcept
    {
        return max_;
    }

    /** Set the maximum allowed total of readable and writable bytes.

        This function changes the currently configured upper limit
        on capacity to the specified value.

        @param n The maximum number of characters ever allowed for capacity.

        @par Exception Safety

        No-throw guarantee.
    */
    void
    max_size(std::size_t n) noexcept
    {
        max_ = n;
    }

    /// Return the maximum number of bytes, both readable and writable, that can be held without requiring an allocation.
    size_type
    capacity() const noexcept
    {
        return s_.capacity();
    }

    /** Returns a constant buffer sequence representing the readable bytes

        @note The sequence may contain multiple contiguous memory regions.
    */
    const_buffers_type
    data() const noexcept
    {
        return { s_.data() + in_, size() };
    }

    /** Returns a constant buffer sequence representing the readable bytes

        @note The sequence may contain multiple contiguous memory regions.
    */
    const_buffers_type
    cdata() const noexcept
    {
        return { s_.data() + in_, size() };
    }

    /** Returns a mutable buffer sequence representing the readable bytes.

        @note The sequence may contain multiple contiguous memory regions.
    */
    mutable_data_type
    data() noexcept
    {
        return { &s_[0] + in_, size() };
    }

    /** Returns a mutable buffer sequence representing writable bytes.
    
        Returns a mutable buffer sequence representing the writable
        bytes containing exactly `n` bytes of storage. Memory may be
        reallocated as needed.

        All buffer sequences previously obtained using @ref prepare are
        invalidated. Buffer sequences previously obtained using @ref data
        remain valid.

        @param n The desired number of bytes in the returned buffer
        sequence.

        @throws std::length_error if `size() + n` exceeds `max_size()`.

        @par Exception Safety

        Strong guarantee.
    */
    mutable_buffers_type
    prepare(size_type n)
    {
        auto const len = size();
        if(len > max_ || n > (max_ - len))
            BOOST_THROW_EXCEPTION(std::length_error{
            "basic_string_buffer overflow"});
        if( out_ + n > s_.capacity() &&
            out_ + n - in_ <= s_.capacity())
        {
            std::memmove(&s_[0], &s_[in_], size());
            out_ -= in_;
            in_ = 0;
        }
        s_.resize(out_ + n);
        return { &s_[0] + out_, n };
    }

    /** Append writable bytes to the readable bytes.

        Appends n bytes from the start of the writable bytes to the
        end of the readable bytes. The remainder of the writable bytes
        are discarded. If n is greater than the number of writable
        bytes, all writable bytes are appended to the readable bytes.

        All buffer sequences previously obtained using @ref prepare are
        invalidated. Buffer sequences previously obtained using @ref data
        remain valid.

        @param n The number of bytes to append. If this number
        is greater than the number of writable bytes, all
        writable bytes are appended.

        @par Exception Safety

        No-throw guarantee.
    */
    void
    commit(size_type n) noexcept
    {
        if(n >= s_.size() - out_)
            out_ = s_.size();
        else
            out_ += n;
    }

    /** Remove bytes from beginning of the readable bytes.

        Removes n bytes from the beginning of the readable bytes.

        All buffers sequences previously obtained using
        @ref data or @ref prepare are invalidated.

        @param n The number of bytes to remove. If this number
        is greater than the number of readable bytes, all
        readable bytes are removed.

        @par Exception Safety

        No-throw guarantee.
    */
    void
    consume(size_type n) noexcept
    {
        if(in_ + n < out_)
        {
            in_ += n;
        }
        else
        {
            in_ = 0;
            out_ = 0;
        }
    }
};

/// A dynamic string buffer of char
using string_buffer = basic_string_buffer<char>;

/// A dynamic string buffer of wchar_t
using wstring_buffer = basic_string_buffer<wchar_t>;

} // beast
} // boost

#endif
