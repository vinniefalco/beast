//
// Copyright (c) 2024 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_FRAME_WRITER_HPP
#define BOOST_BEAST_WEBSOCKET_FRAME_WRITER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/role.hpp>
#include <boost/core/span.hpp>

#if 0
#include <boost/beast/websocket/detail/frame.hpp>
#include <boost/beast/websocket/detail/mask.hpp>
#include <boost/beast/websocket/detail/utf8_checker.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/assert.hpp>
#include <boost/beast/core/error.hpp>
#include <memory>

#include <boost/core/detail/string_view.hpp>
#endif

namespace boost {

namespace core {
template<class T, std::size_t E = boost::dynamic_extent>
using span = boost::span<T, E>;
} // core

namespace beast {
namespace websocket {

/** Serializes websocket messages
*/
class frame_writer
{
public:
    ~frame_writer();

    explicit
    frame_writer(
        std::size_t size);

    void
    reset(
        role_type role) noexcept;

    /** Return a constant buffer representing the output area.
    */
    span<unsigned char const>
    data() const noexcept;

    /** Consume bytes from the output area.
    */
    void
    consume(std::size_t size);

private:
    std::size_t const buf_size_;                // size of write buffer
    unsigned char* buf_;                        // write buffer
    beast::role_type role_;
};

//------------------------------------------------

inline
frame_writer::
~frame_writer()
{
    delete[] buf_;
}

inline
frame_writer::
frame_writer(
    std::size_t size)
    : buf_size_(
        [&]()
        {
            // if(size < 1500)
            // VFALCO THROW
            return size;
        }())
    , buf_(new unsigned char[size])
{
}

inline
void
frame_writer::
reset(
    role_type role) noexcept
{
    role_ = role;
}

inline
span<unsigned char const>
frame_writer::
data() const noexcept
{
    return {};
}

} // websocket
} // beast
} // boost

#endif
