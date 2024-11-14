//
// Copyright (c) 2024 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_FRAME_STREAM_HPP
#define BOOST_BEAST_WEBSOCKET_FRAME_STREAM_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/websocket/detail/frame.hpp>
#include <boost/beast/websocket/detail/mask.hpp>
#include <boost/beast/websocket/detail/utf8_checker.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/core/role.hpp>
#include <boost/assert.hpp>
#include <boost/beast/core/error.hpp>
#include <memory>

#include <boost/core/span.hpp>
#include <boost/core/detail/string_view.hpp>

namespace boost {

namespace core {
template<class T, std::size_t E = boost::dynamic_extent>
using span = boost::span<T, E>;
} // core

namespace beast {
namespace websocket {

/** The type of a frame.
*/
enum class kind : unsigned char
{
    text    = 1,
    binary  = 2,
    close   = 8,
    ping    = 9,
    pong    = 10
};

/** A view to a frame
*/
struct frame_view
{
    core::string_view data;
    detail::opcode op;

    bool fin : 1;

};

struct flat_message_view
{
    bool is_text;
    core::string_view data;
};

struct message_view
{
    bool is_text;
    bool is_complete;
    core::span<core::string_view> payload;
};

struct ping_view
{
    bool is_pong;
    core::string_view payload; 
};

/** Parses incoming frame data.
*/
class frame_stream
{
public:
    struct full_results
    {
        core::span<
    };

    template<unsigned Size>
    struct results
    {
        //close_code close_code = close_code::none;
        //core::string_view reason;

        core::span<ping_view> pings;
        core::span<message_view> messages;

    private:
        friend class frame_stream;
    };

    std::size_t msg_max = 65536;                    // maximum allowed message size
    boost::span<frame_view> frames;

    ~frame_stream();

    explicit
    frame_stream(
        std::size_t size);

    void
    reset(
        role_type role) noexcept;

    /** Return a mutable buffer representing the input area

        This function may only be called exactly once after
        calling @ref reset or @ref read.
    */
    span<unsigned char>
    prepare();

    /** Commit bytes to the input area.
    */
    void
    commit(std::size_t size);

    void
    read(error_code& ec);

private:
    enum class state
    {
          fh0
        , fh1
        , fh2
        , payload
        , partial
    };

    // partial frame state
    struct partial
    {

    };

    // largest possible frame header
    static constexpr
    std::size_t
    max_frame_header_ = 14;

    void append_frame(void*, std::size_t);

private:
    std::size_t const rd_buf_size_;             // size of read buffer
    unsigned char* rd_buf_;                     // read buffer

    std::size_t rd_have_;                       // new data in buffer
    std::size_t rd_pos_;                        // position of leftover if any

    detail::utf8_checker utf8_;                 // to validate utf8
    
    beast::role_type role_;
    state st_;

    unsigned char fh_need_;                     // header bytes we need

    bool can_deflate_ = false;                  // if pmd is negotiated

    detail::prepared_key key_;                  // current stateful mask key

    detail::frame_header fh_;
    frame_view fv_[100];
    bool expect_cont_ = false;
};

//------------------------------------------------

inline
frame_stream::
~frame_stream()
{
    delete[] rd_buf_;
}

inline
frame_stream::
frame_stream(
    std::size_t size)
    : rd_buf_size_(
        [&]()
        {
            // if(size < 1500)
            // VFALCO THROW
            return size;
        }())
    , rd_buf_(new unsigned char[size])
{
}

inline
void
frame_stream::
reset(
    role_type role) noexcept
{
    rd_have_ = 0;
    rd_pos_ = 0;

    role_ = role;
    st_ = state::fh0;
}

inline
span<unsigned char>
frame_stream::
prepare()
{
    if( rd_have_ > 0 &&
        rd_pos_ > 0)
    {
        // move leftover to the front
        std::memmove(
            rd_buf_,
            rd_buf_ + rd_pos_,
            rd_have_);
        rd_pos_ = 0;
    }
    return {
        rd_buf_ + rd_have_,
        rd_buf_size_ - rd_have_ };
}

inline
void
frame_stream::
commit(
    std::size_t size)
{
    auto const lim = rd_buf_size_ - rd_have_;
    if(size > lim)
    {
        // overflow
        // VFALCO THROW
    }
    rd_have_ += size;
}

inline
void
frame_stream::
read(
    error_code& ec)
{
    BOOST_ASSERT(rd_pos_ == 0);

    ec = {}; // VFALCO ?
    frames = {};

    unsigned char* in = static_cast<
        unsigned char*>(rd_buf_);
    auto rd_have = rd_have_;
    while(rd_have > 0)
    {
        switch(st_)
        {

        //----------------------------------------
        //
        // decode header first byte
        //
        case state::fh0:
        {
            auto const v = *in;
            fh_.op   = static_cast<
                detail::opcode>(v & 0x0f);
            fh_.fin  = (v & 0x80) != 0;
            fh_.rsv1 = (v & 0x40) != 0;
            fh_.rsv2 = (v & 0x20) != 0;
            fh_.rsv3 = (v & 0x10) != 0;
            switch(fh_.op)
            {
            case detail::opcode::binary:
            case detail::opcode::text:
                if(expect_cont_)
                {
                    // new data frame when continuation expected
                    BOOST_BEAST_ASSIGN_EC(ec, error::bad_data_frame);
                    return;
                }
                if( fh_.rsv2 ||
                    fh_.rsv3 || (
                        fh_.rsv1 &&
                        ! can_deflate_))
                {
                    // reserved bits not cleared
                    BOOST_BEAST_ASSIGN_EC(ec, error::bad_reserved_bits);
                    return;
                }
                break;

            case detail::opcode::cont:
                if(! expect_cont_)
                {
                    // continuation without an active message
                    BOOST_BEAST_ASSIGN_EC(ec, error::bad_continuation);
                    return;
                }
                if( fh_.rsv1 ||
                    fh_.rsv2 ||
                    fh_.rsv3)
                {
                    // reserved bits not cleared
                    BOOST_BEAST_ASSIGN_EC(ec, error::bad_reserved_bits);
                    return;
                }
                break;

            default:
                if(detail::is_reserved(fh_.op))
                {
                    // reserved opcode
                    BOOST_BEAST_ASSIGN_EC(ec, error::bad_opcode);
                    return;
                }
                // its a control message
                if(! fh_.fin)
                {
                    // fragmented control message
                    BOOST_BEAST_ASSIGN_EC(ec, error::bad_control_fragment);
                    return;
                }
                if( fh_.rsv1 ||
                    fh_.rsv2 ||
                    fh_.rsv3)
                {
                    // reserved bits not cleared
                    BOOST_BEAST_ASSIGN_EC(ec, error::bad_reserved_bits);
                    return;
                }
                break;
            }
            ++in;
            st_ = state::fh1;
            if(--rd_have == 0)
                goto do_return;
            BOOST_FALLTHROUGH;
        }

        //----------------------------------------
        //
        // decode header second byte
        //
        case state::fh1:
        {
            unsigned char need;
            auto const v = *in;
            fh_.len = v & 0x7f;
            fh_.mask = (v & 0x80) != 0;
            switch(fh_.len)
            {
                case 126:
                    need = 2;
                    break;
                case 127:
                    need = 8;
                    break;
                default:
                    need = 0;
            }
            if(fh_.mask)
                need += 4;
            if(role_ == role_type::server)
            {
                if(! fh_.mask)
                {
                    // unmasked frame from client
                    BOOST_BEAST_ASSIGN_EC(ec, error::bad_unmasked_frame);
                    return;
                }
            }
            else if(role_ == role_type::client)
            {
                if(fh_.mask)
                {
                    // masked frame from server
                    BOOST_BEAST_ASSIGN_EC(ec, error::bad_masked_frame);
                    return;
                }
            }
            ++in;
            --rd_have;
            if(need == 0)
                goto do_fh_done;
            if(rd_have >= need)
                goto do_fh_ext;
            // need more
            fh_need_ = need;
            rd_pos_ = in - rd_buf_;
            st_ = state::fh2;
            goto do_return;
        }

        //----------------------------------------
        //
        // accumulate extended header
        //
        case state::fh2:
            if(rd_have < fh_need_)
            {
                // need more
                BOOST_ASSERT(rd_have == rd_have_);
                goto do_return;
            }

            // decode extended header
        do_fh_ext:
            switch(fh_.len)
            {
            case 126:
            {
                std::uint16_t len_be;
                std::memcpy(&len_be, in, sizeof(len_be));
                fh_.len = endian::big_to_native(len_be);
                if(fh_.len < 126)
                {
                    // length not canonical
                    BOOST_BEAST_ASSIGN_EC(ec, error::bad_size);
                    return;
                }
                in += sizeof(len_be);
                rd_have -= sizeof(len_be);
                break;
            }
            case 127:
            {
                std::uint64_t len_be;
                std::memcpy(&len_be, in, sizeof(len_be));
                fh_.len = endian::big_to_native(len_be);
                if(fh_.len < 65536)
                {
                    // length not canonical
                    BOOST_BEAST_ASSIGN_EC(ec, error::bad_size);
                    return;
                }
                in += sizeof(len_be);
                rd_have -= sizeof(len_be);
                break;
            }
            default:
                break;
            }
            if(fh_.mask)
            {
                std::uint32_t key_le;
                std::memcpy(&key_le, in, sizeof(key_le));
                fh_.key = endian::little_to_native(key_le);
                detail::prepare_key(key_, fh_.key);
                in += sizeof(key_le);
                rd_have -= sizeof(key_le);
            }
            else
            {
                // initialize this otherwise operator== breaks
                fh_.key = 0;
            }

        // process complete header
        do_fh_done:
            if(detail::is_control(fh_.op))
            {
                if(rd_have < fh_.len)
                {
                    // need more
                    rd_pos_ = in - rd_buf_;
                    rd_have_ = rd_have;
                    st_ = state::payload;
                    return;
                }
            }
            else
            {
            }
            // fall through

        //----------------------------------------
        //
        // process payload
        //
        case state::payload:
            if(detail::is_control(fh_.op))
            {
                if(rd_have < fh_.len)
                {
                    // need more
                    rd_pos_ = in - rd_buf_;
                    rd_have_ = rd_have;
                    return;
                }
                if(fh_.mask)
                    detail::mask_inplace(
                        { in, fh_.len }, key_);
                append_frame(in, fh_.len);
                in += fh_.len;
                rd_have -= fh_.len;
                st_ = state::fh0;
                continue;
            }
            BOOST_FALLTHROUGH;
       case state::partial:
            if(rd_have < fh_.len)
            {
                // deliver partial frame
                if(fh_.mask)
                    detail::mask_inplace(
                        { in, rd_have }, key_);
                append_frame(in, rd_have);
                fh_.len -= rd_have;
                rd_have = 0;
                st_ = state::partial;
                goto do_return;
            }
            if(fh_.mask)
                detail::mask_inplace(
                    { in, fh_.len }, key_);
            append_frame(in, fh_.len);
            in += fh_.len;
            rd_have -= fh_.len;
            st_ = state::fh0;
            continue;
        }
    }
do_return:
    rd_have_ = rd_have;
}

inline
void
frame_stream::
append_frame(
    void* data,
    std::size_t len)
{
    if(fh_.mask)
        detail::mask_inplace({ data, len }, key_);
    frame_view& fv(fv_[frames.size()]);
    fv.data = { reinterpret_cast<
        char const*>(data), len };
    fv.op = fh_.op;
    fv.fin = fh_.fin;
    frames = { fv_, frames.size() + 1 };
}

} // websocket
} // beast
} // boost

#endif
