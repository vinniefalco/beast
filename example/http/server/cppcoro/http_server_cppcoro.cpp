//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: HTTP server, using cppcoro
//
//------------------------------------------------------------------------------

#include <boost/beast/version.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <cppcoro/async_scope.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/net/socket.hpp>
#include <experimental/resumable>
#include <iostream>
#include <memory>
#include <utility>

namespace net = cppcoro::net;

//------------------------------------------------------------------------------

namespace boost {
namespace beast {
namespace http {

namespace detail {

error_code
to_error_code(std::exception const&)
{
    error_code ec;
    ec = http::error::partial_message;
    return ec;
}

} // detail

template<
    class DynamicBuffer,
    bool isRequest, class Derived>
cppcoro::task<error_code>
read_some(
    net::socket& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser)
{
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(! parser.is_done());
    error_code ec;
    std::size_t bytes_transferred = 0;
    if(buffer.size() == 0)
        goto do_read;
    for(;;)
    {
        // invoke parser
        {
            auto const n = parser.put(buffer.data(), ec);
            bytes_transferred += n;
            buffer.consume(n);
            if(! ec)
                break;
            if(ec != http::error::need_more)
                break;
        }
    do_read:
        auto const size = read_size(buffer, 65536);
        if(size == 0)
        {
            ec = error::buffer_overflow;
            break;
        }
        auto const mbs =
            beast::detail::dynamic_buffer_prepare(
                buffer, size, ec, error::buffer_overflow);
        if(ec)
            break;
        auto const mb = beast::buffers_front(*mbs);
        std::size_t n = 0;
        try
        {
            n = co_await stream.recv(mb.data(), mb.size());
            if(n == 0)
                ec = boost::asio::error::eof;
        }
        catch(std::exception const& e)
        {
            ec = detail::to_error_code(e);
        }
        if(ec == boost::asio::error::eof)
        {
            BOOST_ASSERT(n == 0);
            if(parser.got_some())
            {
                // caller sees EOF on next read
                parser.put_eof(ec);
                if(ec)
                    break;
                BOOST_ASSERT(parser.is_done());
                break;
            }
            ec = error::end_of_stream;
            break;
        }
        if(ec)
            break;
        buffer.commit(n);
    }
    co_return ec;
}

template<
    class DynamicBuffer,
    bool isRequest, class Derived>
cppcoro::task<error_code>
read(
    net::socket& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser)
{
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    parser.eager(true);
    error_code ec;
    while(! parser.is_done())
    {
        ec = co_await read_some(stream, buffer, parser);
        if(ec)
            break;
    }
    co_return ec;
}

//------------------------------------------------------------------------------

namespace detail {

class cppcoro_write_some_lambda
{
    net::socket& stream_;
    cppcoro::task<error_code> task_;

public:
    bool invoked = false;
    std::size_t bytes_transferred = 0;

    explicit
    cppcoro_write_some_lambda(net::socket& stream)
        : stream_(stream)
    {
    }

    template<class ConstBufferSequence>
    void 
    operator()(error_code&,
        ConstBufferSequence const& buffers)
    {
        invoked = true;
        task_ =
            [](
                std::size_t& bytes_transferred,
                net::socket& stream,
                ConstBufferSequence buffers) ->
                    cppcoro::task<error_code>
            {
                error_code ec;
                auto const cb = beast::buffers_front(buffers);
                try
                {
                    bytes_transferred = co_await stream.send(cb.data(), cb.size());;
                }
                catch(std::exception const& e)
                {
                    ec = detail::to_error_code(e);
                }
                return ec;
            }(bytes_transferred, stream_, buffers);
    }

    cppcoro::task<beast::error_code>
    wait() noexcept
    {
        return std::move(task_);
    }
};

} // detail

template<bool isRequest, class Body, class Fields>
cppcoro::task<error_code>
write_some(
    net::socket& stream,
    serializer<isRequest, Body, Fields>& sr)
{
    error_code ec;
    if(! sr.is_done())
    {
        detail::cppcoro_write_some_lambda f(stream);
        sr.next(ec, f);
        if(! ec)
        {
            ec = co_await f.wait();
            if(f.invoked)
                sr.consume(f.bytes_transferred);
        }
    }
    co_return ec;
}

template<bool isRequest, class Body, class Fields>
cppcoro::task<error_code>
write(
    net::socket& stream,
    serializer<isRequest, Body, Fields>& sr)
{
    error_code ec;
    std::size_t bytes_transferred = 0;
    sr.split(false);
    for(;;)
    {
        ec = co_await write_some(stream, sr);
        if(ec)
            break;
        if(sr.is_done())
            break;
    }
    co_return ec;
}

} // http
} // beast
} // boost

//------------------------------------------------------------------------------

namespace beast = boost::beast;
namespace http = boost::beast::http;

//------------------------------------------------------------------------------

// Return a reasonable mime type based on the extension of a file.
boost::beast::string_view
mime_type(boost::beast::string_view path)
{
    using boost::beast::iequals;
    auto const ext = [&path]
    {
        auto const pos = path.rfind(".");
        if(pos == boost::beast::string_view::npos)
            return boost::beast::string_view{};
        return path.substr(pos);
    }();
    if(iequals(ext, ".htm"))  return "text/html";
    if(iequals(ext, ".html")) return "text/html";
    if(iequals(ext, ".php"))  return "text/html";
    if(iequals(ext, ".css"))  return "text/css";
    if(iequals(ext, ".txt"))  return "text/plain";
    if(iequals(ext, ".js"))   return "application/javascript";
    if(iequals(ext, ".json")) return "application/json";
    if(iequals(ext, ".xml"))  return "application/xml";
    if(iequals(ext, ".swf"))  return "application/x-shockwave-flash";
    if(iequals(ext, ".flv"))  return "video/x-flv";
    if(iequals(ext, ".png"))  return "image/png";
    if(iequals(ext, ".jpe"))  return "image/jpeg";
    if(iequals(ext, ".jpeg")) return "image/jpeg";
    if(iequals(ext, ".jpg"))  return "image/jpeg";
    if(iequals(ext, ".gif"))  return "image/gif";
    if(iequals(ext, ".bmp"))  return "image/bmp";
    if(iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
    if(iequals(ext, ".tiff")) return "image/tiff";
    if(iequals(ext, ".tif"))  return "image/tiff";
    if(iequals(ext, ".svg"))  return "image/svg+xml";
    if(iequals(ext, ".svgz")) return "image/svg+xml";
    return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string
path_cat(
    boost::beast::string_view base,
    boost::beast::string_view path)
{
    if(base.empty())
        return path.to_string();
    std::string result = base.to_string();
#if BOOST_MSVC
    char constexpr path_separator = '\\';
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
    for(auto& c : result)
        if(c == '/')
            c = path_separator;
#else
    char constexpr path_separator = '/';
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
#endif
    return result;
}

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template<
    class Body, class Allocator,
    class Send>
void
handle_request(
    beast::string_view doc_root,
    http::request<Body, http::basic_fields<Allocator>>&& req,
    Send& send)
{
    // Returns a bad request response
    auto const bad_request =
    [&req](boost::beast::string_view why)
    {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = why.to_string();
        res.prepare_payload();
        return res;
    };

    // Returns a not found response
    auto const not_found =
    [&req](boost::beast::string_view target)
    {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + target.to_string() + "' was not found.";
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    auto const server_error =
    [&req](boost::beast::string_view what)
    {
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + what.to_string() + "'";
        res.prepare_payload();
        return res;
    };

    // Make sure we can handle the method
    if( req.method() != http::verb::get &&
        req.method() != http::verb::head)
        return send(bad_request("Unknown HTTP-method"));

    // Request path must be absolute and not contain "..".
    if( req.target().empty() ||
        req.target()[0] != '/' ||
        req.target().find("..") != boost::beast::string_view::npos)
        return send(bad_request("Illegal request-target"));

    // Build the path to the requested file
    std::string path = path_cat(doc_root, req.target());
    if(req.target().back() == '/')
        path.append("index.html");

    // Attempt to open the file
    boost::beast::error_code ec;
    http::file_body::value_type body;
    body.open(path.c_str(), boost::beast::file_mode::scan, ec);

    // Handle the case where the file doesn't exist
    if(ec == boost::system::errc::no_such_file_or_directory)
        return send(not_found(req.target()));

    // Handle an unknown error
    if(ec)
        return send(server_error(ec.message()));

    // Cache the size since we need it after the move
    auto const size = body.size();

    // Respond to HEAD request
    if(req.method() == http::verb::head)
    {
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, mime_type(path));
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
    }

    // Respond to GET request
    http::response<http::file_body> res{
        std::piecewise_construct,
        std::make_tuple(std::move(body)),
        std::make_tuple(http::status::ok, req.version())};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, mime_type(path));
    res.content_length(size);
    res.keep_alive(req.keep_alive());
    return send(std::move(res));
}

//------------------------------------------------------------------------------

// Report a failure
void
fail(boost::system::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

class write_lambda
{
    net::socket& stream_;
    cppcoro::task<beast::error_code> task_;

public:
    write_lambda(net::socket& stream)
        : stream_(stream)
    {
    }

    template<bool isRequest, class Body, class Fields>
    void
    operator()(http::message<isRequest, Body, Fields>&& m)
    {
        task_ =
            []( net::socket& stream,
                http::message<isRequest, Body, Fields> m) ->
                    cppcoro::task<beast::error_code>
            {
                beast::error_code ec;
                http::serializer<isRequest, Body, Fields> sr(m);
                while(! sr.is_done())
                {
                    ec = co_await write_some(stream, sr);
                    if(ec)
                        break;
                }
                co_return ec;
            }(stream_, std::move(m));
    }

    cppcoro::task<beast::error_code>
    wait() noexcept
    {
        return std::move(task_);
    }
};

// Handles an HTTP client connection
cppcoro::task<>
do_http_session(
    beast::string_view doc_root,
    net::socket& sock_
)
{
    net::socket sock(std::move(sock_));
    try
    {
        beast::error_code ec;
        for(;;)
        {
            http::request_parser<http::string_body> parser;
            beast::flat_buffer buffer;
            ec = co_await http::read(sock, buffer, parser);
            if(ec)
            {
                fail(ec, "read");
                break;
            }
            cppcoro::task<beast::error_code> st;
            write_lambda write(sock);
            handle_request(doc_root, parser.release(), write);
            ec = co_await write.wait(); // small hack because operator co_await don't chain
            if(ec)
            {
                fail(ec, "write");
                break;
            }
        }
    }
    catch(std::exception const&)
    {
    }
}

// Accepts incoming connections and launches the sessions
cppcoro::task<>
do_listen(
    beast::string_view doc_root,
    net::ipv4_endpoint ep,
    cppcoro::io_service& ios)
{
    cppcoro::async_scope scope;
    net::socket acceptor(net::socket::create_tcpv4(ios));
    {
        BOOL b = TRUE;
        ::setsockopt(
            acceptor.native_handle(),
            SOL_SOCKET,
            SO_REUSEADDR,
            reinterpret_cast<char const*>(&b),
            sizeof(b));
    }
    acceptor.bind(ep);
    acceptor.listen(3);
    for(;;)
    {
        net::socket sock(net::socket::create_tcpv4(ios));
        co_await acceptor.accept(sock);
        scope.spawn(do_http_session(doc_root, sock));
    }
};

//------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc != 5)
    {
        std::cerr <<
            "Usage: http-server-cppcoro <address> <port> <doc_root> <threads>\n" <<
            "Example:\n" <<
            "    http-server-cppcoro 0.0.0.0 8080 . 1\n";
        return EXIT_FAILURE;
    }
    auto const address = net::ipv4_address::from_string(argv[1]);
    if(! address)
    {
        std::cerr <<
            "Invalid IPv4 address: '" << argv[1] << "'\n";
        return EXIT_FAILURE;
    }

    auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
    auto const doc_root = boost::beast::string_view(argv[3]);
    auto const threads = std::max<int>(1, std::atoi(argv[4]));

    // The io_service is required for all I/O
    cppcoro::io_service ios;

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for(auto i = threads - 1; i > 0; --i)
        v.emplace_back(
        [&ios]
        {
            ios.process_events();
        });

    // Small hack to launch the app and process events on main thread
    cppcoro::sync_wait(cppcoro::when_all(
        [&]() -> cppcoro::task<>
        {
            auto stop = cppcoro::on_scope_exit(
                [&]
                {
                    ios.stop();
                });

            // Create and launch a listening port
            co_await do_listen(
                doc_root,
                net::ipv4_endpoint(*address, port),
                ios);
        }(),
        [&]() -> cppcoro::task<>
        {
            ios.process_events();
            co_return;
        }()
    ));

    return EXIT_SUCCESS;
}
