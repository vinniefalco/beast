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
// Example: HTTP server, task based: Awaitable, Sender/Receiver, CompletionToken
//
//------------------------------------------------------------------------------

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <experimental/resumable>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "async_task.hpp"
#include "spawn.hpp"
#include "task.hpp"
#include "tasks.hpp"

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace http = boost::beast::http;    // from <boost/beast/http.hpp>
namespace beast = boost::beast;

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
    boost::beast::string_view doc_root,
    http::request<Body, http::basic_fields<Allocator>>&& req,
    Send&& send)
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

template<class Stream>
class write_lambda
{
    using task_type = beast::task<
        std::tuple<beast::error_code, std::size_t>>;
    Stream& stream_;
    task_type task_;

public:
    explicit
    write_lambda(Stream& stream)
        : stream_(stream)
    {
    }

    template<bool isRequest, class Body, class Fields>
    void
    operator()(http::message<isRequest, Body, Fields> m)
    {
        task_ =
            []( Stream& stream,
                http::message<
                    isRequest, Body, Fields> m) ->
                task_type
                
            {
                return co_await http::async_write(stream, std::move(m));
            }(stream_, std::move(m));
    }

    auto
    wait()
    {
        return std::move(task_);
    }
};

template<class Protocol>
auto
async_accept(
    boost::asio::basic_socket_acceptor<Protocol>& acceptor,
    boost::asio::basic_socket<Protocol>& socket)
{
    return beast::make_async_task<
        void(beast::error_code)>(
        [&](auto&& token)
        {
            using CompletionToken =
                std::decay_t<decltype(token)>;
            return acceptor.async_accept(socket,
                std::forward<CompletionToken>(token));
        });
}

//------------------------------------------------------------------------------

// Report a failure
void
fail(
    std::string_view what,
    std::string_view why)
{
#if 0
    boost::asio::post(
        [s = std::string(what) + ": " + std::string(why) + "\n"]
        {
            static std::mutex m;
            std::lock_guard<std::mutex> lock(m);
            std::cerr << s;
        });
#else
    auto s =
        std::string(what) + ": " +
        std::string(why) + "\n";
    std::cerr << s;
#endif
}

// Report a failure
void
fail(
    char const* what,
    boost::system::error_code ec)
{
    fail(what, ec.message());
}

// Handles an HTTP client connection
template<class Stream>
beast::detached_task
http_session(
    beast::string_view doc_root,
    Stream stream
)
{
    try
    {
        beast::flat_buffer buffer;
        for(;;)
        {
            http::request_parser<http::string_body> parser;
            {
                auto[ec, bytes_transferred] = co_await
                    http::async_read(stream, buffer, parser);
                if(ec)
                {
                    fail("read", ec);
                    break;
                }
            }
            {
                write_lambda write(stream);
                handle_request(doc_root, parser.release(), write);
                auto[ec, bytes_transferred] = co_await write.wait();
                if(ec)
                {
                    fail("write", ec);
                    break;
                }
            }
        }
    }
    catch(std::exception const& e)
    {
        fail("http_session", e.what());
    }
}

// Handles incoming connections
beast::detached_task
listen(
    boost::asio::io_context& ioc,
    tcp::endpoint endpoint,
    boost::string_view doc_root)
{
    tcp::acceptor acceptor(ioc);

    {
        boost::system::error_code ec;
        acceptor.open(endpoint.protocol(), ec);
        if(ec)
            co_return fail("open", ec);
        acceptor.set_option(boost::asio::socket_base::reuse_address(true), ec);
        if(ec)
            co_return fail("set_option", ec);
        acceptor.bind(endpoint, ec);
        if(ec)
            co_return fail("bind", ec);
        acceptor.listen(
            boost::asio::socket_base::max_listen_connections, ec);
        if(ec)
            co_return fail("listen", ec);
    }

    tcp::socket socket(ioc);
    for(;;)
    {
        auto[ec] = co_await async_accept(acceptor, socket);
        if(ec == boost::asio::error::operation_aborted)
            break;
        if(ec)
        {
            fail("accept", ec);
            continue;
        }
        beast::co_spawn(
            acceptor.get_executor(),
            http_session(doc_root, std::move(socket)));
    }
}

//------------------------------------------------------------------------------

#ifndef NO_LEAK_CHECK
# ifdef BOOST_MSVC
#  ifndef WIN32_LEAN_AND_MEAN // VC_EXTRALEAN
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   undef WIN32_LEAN_AND_MEAN
#  else
#   include <windows.h>
#  endif
# endif
#endif

int main(int argc, char* argv[])
{
#ifndef NO_LEAK_CHECK
# if BOOST_MSVC
    {
        int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
        flags |= _CRTDBG_LEAK_CHECK_DF;
        _CrtSetDbgFlag(flags);
    }
# endif
#endif
    // Check command line arguments.
    if (argc != 5)
    {
        std::cerr <<
            "Usage: http-server-task <address> <port> <doc_root> <threads>\n"
            "Example:\n"
            "    http-server-task 0.0.0.0 8080 . 1\n";
        return EXIT_FAILURE;
    }
    auto const address = boost::asio::ip::make_address(argv[1]);
    auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
    auto const doc_root = beast::string_view(argv[3]);
    auto const threads = std::max<int>(1, std::atoi(argv[4]));

    // The io_context is required for all I/O
    boost::asio::io_context ioc{threads};

    // Capture SIGINT and SIGTERM to perform a clean shutdown
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait(
        [&](boost::system::error_code const&, int)
        {
            // Stop the `io_context`. This will cause `run()`
            // to return immediately, eventually destroying the
            // `io_context` and all of the sockets in it.
            ioc.stop();
        });

    // Create and launch a listening port
    beast::co_spawn(ioc.get_executor(), listen(
        ioc, tcp::endpoint{address, port}, doc_root));

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for(auto i = threads - 1; i > 0; --i)
        v.emplace_back(
        [&ioc]
        {
            ioc.run();
        });
    ioc.run();

    // (If we get here, it means we got a SIGINT or SIGTERM)

    // Block until all the threads exit
    for(auto& t : v)
        t.join();

    return EXIT_SUCCESS;
}
