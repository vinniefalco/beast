//
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_EXAMPLE_CONTRIB_SOCKS_CLIENT_SOCKS_CLIENT_HPP
#define BOOST_BEAST_EXAMPLE_CONTRIB_SOCKS_CLIENT_SOCKS_CLIENT_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/async_base.hpp>
#include <boost/beast/core/detail/is_invocable.hpp>
#include "error.hpp"
#include <boost/config.hpp> // for BOOST_FALLTHROUGH
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/buffer.hpp>
#include <iostream>
#include <string>
#include <memory>
#include <utility>
#include <type_traits>

namespace socks {

namespace beast = boost::beast;
namespace net = beast::net;

using net::ip::tcp;
using beast::error_code;

namespace detail {

template<typename type, typename source>
type read(source& p)
{
    type ret = 0;
    for (std::size_t i = 0; i < sizeof(type); i++)
        ret = (ret << 8) | (static_cast<unsigned char>(*p++));
    return ret;
}

template<typename type, typename target>
void write(type v, target& p)
{
    for (auto i = (int)sizeof(type) - 1; i >= 0; i--, p++)
        *p = static_cast<unsigned char>((v >> (i * 8)) & 0xff);
}

} // detail

enum
{
    SOCKS_VERSION_4 = 4,
    SOCKS_VERSION_5 = 5
};

enum
{
    SOCKS5_AUTH_NONE = 0x00,
    SOCKS5_AUTH = 0x02,
    SOCKS5_AUTH_UNACCEPTABLE = 0xFF
};

enum
{
    SOCKS_CMD_CONNECT = 0x01,
    SOCKS_CMD_BIND = 0x02,
    SOCKS5_CMD_UDP = 0x03
};

enum
{
    SOCKS5_ATYP_IPV4 = 0x01,
    SOCKS5_ATYP_DOMAINNAME = 0x03,
    SOCKS5_ATYP_IPV6 = 0x04
};

enum
{
    SOCKS5_SUCCEEDED = 0x00,
    SOCKS5_GENERAL_SOCKS_SERVER_FAILURE,
    SOCKS5_CONNECTION_NOT_ALLOWED_BY_RULESET,
    SOCKS5_NETWORK_UNREACHABLE,
    SOCKS5_CONNECTION_REFUSED,
    SOCKS5_TTL_EXPIRED,
    SOCKS5_COMMAND_NOT_SUPPORTED,
    SOCKS5_ADDRESS_TYPE_NOT_SUPPORTED,
    SOCKS5_UNASSIGNED
};

enum
{
    SOCKS4_REQUEST_GRANTED = 90,
    SOCKS4_REQUEST_REJECTED_OR_FAILED,
    SOCKS4_CANNOT_CONNECT_TARGET_SERVER,
    SOCKS4_REQUEST_REJECTED_USER_NO_ALLOW,
};

template<
    class AsyncStream,
    class Handler,
    class Buffer,
    class base_type = beast::async_base<
        Handler, typename AsyncStream::executor_type>>
class socks4_op : public base_type
{
public:
    socks4_op(socks4_op&&) = default;
    socks4_op(socks4_op const&) = default;

    socks4_op(AsyncStream& stream, Handler& handler,
        const std::string& hostname, unsigned short port,
        const std::string& username)
        : base_type(std::move(handler), stream.get_executor())
        , stream_(stream)
        , hostname_(hostname)
        , port_(port)
        , username_(username)
    {
        (*this)({}, 0); // start the operation
    }

    void operator()(error_code ec, std::size_t /*bytes_transferred*/)
    {
        using detail::write;
        using detail::read;

        auto& response = *response_;
        auto& request = *request_;

        switch (ec ? 3 : step_)
        {
        case 0:
        {
            step_ = 1;

            std::size_t bytes_to_write = 9 + username_.size();
            auto req = static_cast<char*>(request.prepare(bytes_to_write).data());

            write<uint8_t>(SOCKS_VERSION_4, req); // SOCKS VERSION 4.
            write<uint8_t>(SOCKS_CMD_CONNECT, req); // CONNECT.

            write<uint16_t>(port_, req); // DST PORT.

            auto address = net::ip::make_address_v4(hostname_, ec);
            if (ec)
                break;

            write<uint32_t>(address.to_uint(), req); // DST I

            if (!username_.empty())
            {
                std::copy(username_.begin(), username_.end(), req);    // USERID
                req += username_.size();
            }
            write<uint8_t>(0, req); // NULL.

            request.commit(bytes_to_write);
            return net::async_write(stream_, request, std::move(*this));
        }
        case 1:
        {
            step_ = 2;
            return net::async_read(stream_, response,
                net::transfer_exactly(8), std::move(*this));
        }
        case 2:
        {
            auto resp = static_cast<const unsigned char*>(response.data().data());

            read<uint8_t>(resp); // VN is the version of the reply code and should be 0.
            auto cd = read<uint8_t>(resp);

            if (cd != SOCKS4_REQUEST_GRANTED)
            {
                switch (cd)
                {
                case SOCKS4_REQUEST_REJECTED_OR_FAILED:
                    ec = errc::socks_request_rejected_or_failed;
                    break;
                case SOCKS4_CANNOT_CONNECT_TARGET_SERVER:
                    ec = errc::socks_request_rejected_cannot_connect;
                    break;
                case SOCKS4_REQUEST_REJECTED_USER_NO_ALLOW:
                    ec = errc::socks_request_rejected_incorrect_userid;
                    break;
                default:
                    ec = errc::socks_unknown_error;
                    break;
                }
            }
            BOOST_FALLTHROUGH;
        }
        case 3:
            break;
        }

        this->complete_now(ec);
    }

private:
    AsyncStream& stream_;

    using BufferPtr = std::unique_ptr<Buffer>;
    BufferPtr request_{ new Buffer() }; // std::make_unique c++14 or later.
    BufferPtr response_{ new Buffer() };

    std::string hostname_;
    unsigned short port_;
    std::string username_;
    int step_ = 0;
};

template<class AsyncStream, class Handler, class Buffer,
    class base_type = beast::async_base<Handler, typename AsyncStream::executor_type>>
class socks5_op : public base_type
{
public:
    socks5_op(socks5_op&&) = default;
    socks5_op(socks5_op const&) = default;

    socks5_op(AsyncStream& stream, Handler& handler,
        const std::string& hostname, unsigned short port,
        const std::string& username, const std::string& password,
        bool use_hostname)
        : base_type(std::move(handler), stream.get_executor()),
        stream_(stream),
        hostname_(hostname), port_(port),
        username_(username), password_(password),
        use_hostname_(use_hostname)
    {
        (*this)({}, 0); // start the operation
    }

    void operator()(error_code ec, std::size_t/*bytes_transferred*/)
    {
        using detail::write;
        using detail::read;

        auto& response = *response_;
        auto& request = *request_;

        switch (ec ? 10 : step_)
        {
        case 0:
        {
            step_ = 1;

            std::size_t bytes_to_write = username_.empty() ? 3 : 4;
            auto req = static_cast<char*>(request.prepare(bytes_to_write).data());

            write<uint8_t>(SOCKS_VERSION_5, req); // SOCKS VERSION 5.
            if (username_.empty())
            {
                write<uint8_t>(1, req); // 1 method
                write<uint8_t>(SOCKS5_AUTH_NONE, req); // support no authentication
            }
            else
            {
                write<uint8_t>(2, req); // 2 methods
                write<uint8_t>(SOCKS5_AUTH_NONE, req); // support no authentication
                write<uint8_t>(SOCKS5_AUTH, req); // support username/password
            }

            request.commit(bytes_to_write);
            return boost::asio::async_write(stream_, request, std::move(*this));
        }
        case 1:
        {
            step_ = 2;

            return boost::asio::async_read(stream_, response, boost::asio::transfer_exactly(2), std::move(*this));
        }
        case 2:
        {
            step_ = 3;

            BOOST_ASSERT(response.size() == 2);
            auto resp = static_cast<const char*>(response.data().data());
            auto version = read<uint8_t>(resp);
            auto method = read<uint8_t>(resp);

            if (version != SOCKS_VERSION_5)
            {
                ec = socks::errc::socks_unsupported_version;
                break;
            }

            if (method == SOCKS5_AUTH) // need username&password auth...
            {
                if (username_.empty())
                {
                    ec = socks::errc::socks_username_required;
                    break;
                }

                request.consume(request.size());

                std::size_t bytes_to_write = username_.size() + password_.size() + 3;
                auto auth = static_cast<char*>(request.prepare(bytes_to_write).data());

                write<uint8_t>(0x01, auth); // auth version.
                write<uint8_t>(static_cast<uint8_t>(username_.size()), auth);
                std::copy(username_.begin(), username_.end(), auth);    // username.
                auth += username_.size();
                write<uint8_t>(static_cast<int8_t>(password_.size()), auth);
                std::copy(password_.begin(), password_.end(), auth);    // password.
                auth += password_.size();
                request.commit(bytes_to_write);

                // write username & password.
                return boost::asio::async_write(stream_, request, std::move(*this));
            }

            if (method == SOCKS5_AUTH_NONE) // no need auth...
            {
                step_ = 5;
                return (*this)(ec, 0);
            }

            ec = socks::errc::socks_unsupported_authentication_version;
            break;
        }
        case 3:
        {
            step_ = 4;
            response.consume(response.size());
            return boost::asio::async_read(stream_, response,
                boost::asio::transfer_exactly(2), std::move(*this));
        }
        case 4:
        {
            step_ = 5;
            auto resp = static_cast<const char*>(response.data().data());

            auto version = read<uint8_t>(resp);
            auto status = read<uint8_t>(resp);

            if (version != 0x01) // auth version.
            {
                ec = errc::socks_unsupported_authentication_version;
                break;
            }

            if (status != 0x00)
            {
                ec = errc::socks_authentication_error;
                break;
            }
            BOOST_FALLTHROUGH;
        }
        case 5:
        {
            step_ = 6;

            request.consume(request.size());
            std::size_t bytes_to_write = 7 + hostname_.size();
            auto req = static_cast<char*>(
                request.prepare(std::max<std::size_t>(bytes_to_write, 22)).data());

            write<uint8_t>(SOCKS_VERSION_5, req); // SOCKS VERSION 5.
            write<uint8_t>(SOCKS_CMD_CONNECT, req); // CONNECT command.
            write<uint8_t>(0, req); // reserved.

            if (use_hostname_)
            {
                write<uint8_t>(SOCKS5_ATYP_DOMAINNAME, req); // atyp, domain name.
                BOOST_ASSERT(hostname_.size() <= 255);
                write<uint8_t>(static_cast<int8_t>(hostname_.size()), req);    // domainname size.
                std::copy(hostname_.begin(), hostname_.end(), req);    // domainname.
                req += hostname_.size();
                write<uint16_t>(port_, req);    // port.
            }
            else
            {
                auto endp = boost::asio::ip::make_address(hostname_, ec);
                if (ec)
                    break;

                if (endp.is_v4())
                {
                    write<uint8_t>(SOCKS5_ATYP_IPV4, req); // ipv4.
                    write<uint32_t>(endp.to_v4().to_uint(), req);
                    write<uint16_t>(port_, req);
                    bytes_to_write = 10;
                }
                else
                {
                    write<uint8_t>(SOCKS5_ATYP_IPV6, req); // ipv6.
                    auto bytes = endp.to_v6().to_bytes();
                    std::copy(bytes.begin(), bytes.end(), req);
                    req += 16;
                    write<uint16_t>(port_, req);
                    bytes_to_write = 22;
                }
            }

            request.commit(bytes_to_write);
            return boost::asio::async_write(stream_, request, std::move(*this));
        }
        case 6:
        {
            step_ = 7;

            std::size_t bytes_to_read = 10;
            response.consume(response.size());
            return boost::asio::async_read(stream_, response,
                boost::asio::transfer_exactly(bytes_to_read), std::move(*this));
        }
        case 7:
        {
            step_ = 8;

            auto resp = static_cast<const char*>(response.data().data());
            auto ver = read<uint8_t>(resp);
            /*auto rep = */read<uint8_t>(resp);
            read<uint8_t>(resp);    // skip RSV.
            int atyp = read<uint8_t>(resp);

            if (ver != SOCKS_VERSION_5)
            {
                ec = errc::socks_unsupported_version;
                break;
            }

            if (atyp != SOCKS5_ATYP_IPV4 &&
                atyp != SOCKS5_ATYP_DOMAINNAME &&
                atyp != SOCKS5_ATYP_IPV6)
            {
                ec = errc::socks_general_failure;
                break;
            }

            if (atyp == SOCKS5_ATYP_DOMAINNAME)
            {
                auto domain_length = read<uint8_t>(resp);

                return boost::asio::async_read(stream_, response,
                    boost::asio::transfer_exactly(domain_length - 3), std::move(*this));
            }

            if (atyp == SOCKS5_ATYP_IPV6)
            {
                return boost::asio::async_read(stream_, response,
                    boost::asio::transfer_exactly(12), std::move(*this));
            }
            BOOST_FALLTHROUGH;
        }
        case 8:
        {
            auto resp = static_cast<const char*>(response.data().data());
            read<uint8_t>(resp);
            auto rep = read<uint8_t>(resp);
            read<uint8_t>(resp);    // skip RSV.
            int atyp = read<uint8_t>(resp);

            if (atyp == SOCKS5_ATYP_DOMAINNAME)
            {
                auto domain_length = read<uint8_t>(resp);

                std::string domain;
                for (int i = 0; i < domain_length; i++)
                    domain.push_back(read<uint8_t>(resp));
                auto port = read<uint16_t>(resp);

                std::cout << "* SOCKS remote host: " << domain << ":" << port << std::endl;
            }

            if (atyp == SOCKS5_ATYP_IPV4)
            {
                tcp::endpoint remote_endp(
                    boost::asio::ip::address_v4(read<uint32_t>(resp)),
                    read<uint16_t>(resp));

                std::cout << "* SOCKS remote host: " << remote_endp.address().to_string()
                    << ":" << remote_endp.port() << std::endl;
            }

            if (atyp == SOCKS5_ATYP_IPV6)
            {
                boost::asio::ip::address_v6::bytes_type bytes;
                for (auto i = 0; i < 16; i++)
                    bytes[i] = read<uint8_t>(resp);

                tcp::endpoint remote_endp(
                    boost::asio::ip::address_v6(bytes),
                    read<uint16_t>(resp));

                std::cout << "* SOCKS remote host: " << remote_endp.address().to_string()
                    << ":" << remote_endp.port() << std::endl;
            }

            // fail.
            if (rep != 0)
            {
                switch (rep)
                {
                case SOCKS5_GENERAL_SOCKS_SERVER_FAILURE:
                    ec = errc::socks_general_failure; break;
                case SOCKS5_CONNECTION_NOT_ALLOWED_BY_RULESET:
                    ec = errc::socks_connection_not_allowed_by_ruleset; break;
                case SOCKS5_NETWORK_UNREACHABLE:
                    ec = errc::socks_network_unreachable; break;
                case SOCKS5_CONNECTION_REFUSED:
                    ec = errc::socks_connection_refused; break;
                case SOCKS5_TTL_EXPIRED:
                    ec = errc::socks_ttl_expired; break;
                case SOCKS5_COMMAND_NOT_SUPPORTED:
                    ec = errc::socks_command_not_supported; break;
                case SOCKS5_ADDRESS_TYPE_NOT_SUPPORTED:
                    ec = errc::socks_address_type_not_supported; break;
                default:
                    ec = errc::socks_unassigned; break;
                }

                break;
            }
            BOOST_FALLTHROUGH;
        }
        case 10:
            break;
        }

        this->complete_now(ec);
    }

private:

    AsyncStream& stream_;

    using BufferPtr = std::unique_ptr<Buffer>;
    BufferPtr request_ { new Buffer() };
    BufferPtr response_ { new Buffer() };

    std::string hostname_;
    unsigned short port_;
    std::string username_;
    std::string password_;
    bool use_hostname_;
    int step_ = 0;
};


template<typename AsyncStream, typename Handler>
typename net::async_result<
    Handler, void(error_code)>::return_type
async_handshake(
    AsyncStream& stream,
    const std::string& hostname,
    unsigned short port,
    int version,
    std::string const& username,
    std::string const& password,
    bool use_hostname,
    Handler&& handler)
{
    BOOST_ASSERT("incorrect socks version" && (version == SOCKS_VERSION_5 || version == SOCKS_VERSION_4));

    net::async_completion<Handler, void(error_code)> init{ handler };
    using HandlerType = typename std::decay<decltype(init.completion_handler)>::type;

    static_assert(beast::detail::is_invocable<HandlerType,
        void(error_code)>::value, "Handler type requirements not met");

    using Buffer = net::basic_streambuf<typename std::allocator_traits<
        net::associated_allocator_t<HandlerType>>:: template rebind_alloc<char> >;

    if (version == SOCKS_VERSION_4)
        socks4_op<AsyncStream, HandlerType, Buffer>(stream, init.completion_handler,
            hostname, port, username);
    else
        socks5_op<AsyncStream, HandlerType, Buffer>(stream, init.completion_handler,
            hostname, port, username, password, use_hostname);

    return init.result.get();
}

} // socks

#endif
