//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/beast/_experimental/unit_test/suite.hpp>

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/asio/buffer.hpp>
#include <utility>

namespace boost {
namespace beast {
namespace http {

//----------------------------------------------------------
/*
    Two Body types:

    RegularBody
        - Parser writes directly into body buffers
        - Always support both read and write

    Body
        Parser-provided buffer
        - e.g. file_body

    http::message
        * SemiRegular
        * Only allows RegularBody
        * Movable (Copyable?)
        * Assignable
        * Swappable
        * has clear()
        * readable/writable

    http::response<file_body> // NO LONGER A THING

    Notes:

    * To work around SSL's lack of scatter/gather, the
      serialization of HTTP headers should flatten
      everything first.

    * Perhaps HTTP fields should always be stored flat.

    std::string
    FILE*
*/

namespace {

//--------------------------------------
//
// buffer I/O
//
//--------------------------------------

class any_mutable_buffers
{
    net::mutable_buffer b_[4];
    std::size_t n_ = 0;

public:
    using iterator =
        net::mutable_buffer const*;
    
    iterator
    begin() const noexcept
    {
        return &b_[0];
    }
    
    iterator
    end() const noexcept
    {
        return &b_[n_];
    }
};

struct any_const_buffers
{
    net::const_buffer b_[4];
    std::size_t n_ = 0;

public:
    using iterator =
        net::const_buffer const*;
    
    iterator
    begin() const noexcept
    {
        return &b_[0];
    }
    
    iterator
    end() const noexcept
    {
        return &b_[n_];
    }
};

struct buffer_output_sequence
{
    virtual
    void
    write( any_mutable_buffers mbs ) = 0;

};

struct direct_output_sequence
{
    virtual
    any_mutable_buffers
    prepare() = 0;

    virtual
    void
    commit(std::size_t n) = 0;
};

struct buffer_input_sequence
{
};

template<class SyncReadStream>
void
read_some(
    SyncReadStream& stream,
    direct_output_sequence& out)
{
    (void)stream,out;
}

//--------------------------------------

struct headers_view
{
};

struct message_base
{
};

template<class Body>
struct request
    : message_base
{
};

template<class Body>
struct response
    : message_base
{
};

// A RegularBody using std::string
struct string_body
{
    using value_type = std::string;
};

// A RegularBody using std::vector
struct vector_body
{
    using value_type = std::vector<char>;
};

//--------------------------------------

class parser
{
public:
    bool
    is_header_done() const noexcept
    {
        return true;
    }

    bool
    is_done() const noexcept
    {
        return true;
    }

    any_mutable_buffers
    prepare()
    {
        return {};
    }

    void
    commit(std::size_t n)
    {
        (void)n;
    }
};

class request_parser
    : public parser
{
};

class response_parser
    : public parser
{
};

//--------------------------------------

class write_file_body
{
public:
    write_file_body() = default;

    explicit
    write_file_body(
        char const* szpath)
    {
        (void)szpath;
    }
};

class write_buffer_body
{
public:
    write_buffer_body(
        void* data,
        std::size_t size)
    {
        (void)data,size;
    }
};

//--------------------------------------

template<
    class SyncReadStream>
void
read_header(
    SyncReadStream& stream,
    parser& p)
{
    (void)stream,p;
    
    while(! p.is_header_done())
    {
        auto const bytes_transferred =
            stream.read_some(p.prepare());
        p.commit(bytes_transferred);
    }
}

template<
    class SyncReadStream,
    class Body>
Body
read_some(
    SyncReadStream& stream,
    parser& p,
    Body&& body)
{
    (void)stream, p, body;
    return body;
}

template<
    class SyncReadStream,
    class Body>
Body
read_some(
    SyncReadStream& stream,
    parser& p,
    Body&& body,
    error_code& ec)
{
    (void)stream, p, body;
    return body;
}

template<
    class SyncReadStream,
    class Body>
void
read(
    SyncReadStream& stream,
    parser& p,
    Body&& body)
{
    read_header(stream, p);
    while(! p.is_done())
    {
        auto b = read_some(
            stream, p, std::move(body));
    }
}

// Read an entire message
template<
    class SyncReadStream>
void
read(
    SyncReadStream& stream,
    parser& p,
    message_base& m)
{
    (void)stream,p,m;

    read_header(stream, p);

}

} // (anon)

//----------------------------------------------------------

class connection
{
    request_parser rp_;
};

class sandbox_test : public beast::unit_test::suite
{
public:
    void
    demoRead()
    {
        net::io_context ioc;
        test::stream sock(ioc);

        // read a request
        // using std::string as the body
        {
            request_parser rp;
            request<string_body> req;

            read( sock, rp, req );

            // read another
            read( sock, rp, req );
        }

        // read a response header,
        // then read a conditional body
        {
            response_parser rp;
            read_header( sock, rp );

            bool cond = false;
            if(cond)
            {
                response<string_body> res;
                read( sock, rp, res );
            }
            else
            {
                response<vector_body> res;
                read( sock, rp, res );
            }
        }

        // read a response header,
        // then read the body into a file
        {
            response_parser rp;
            read_header( sock, rp );
            read( sock, rp,
                write_file_body(
                    "C:\\Users\\vinnie\\src\\download.txt"));
        }

        // read a response header,
        // then read the body into a file
        // using a local variable for the body.
        {
            response_parser rp;
            write_file_body b;//( rp );
            //b.try_set_shared_write(...);
            //b.open("C:\\Users\\vinnie\\src\\download.txt", ec);
            (void)b;
            read( sock, rp, b );
        }

        // read a request header
        // then read the body a buffer-at-a-time
        // and forward it to another connection (e.g. proxy)
        {
            response_parser rp;
            read_header( sock, rp );
            do
            {
                char buf[8192];
                // read_body_some?
                read( sock, rp,
                    write_buffer_body(
                        buf, sizeof(buf)));

                read_some( sock, rp, write_buffer_body(buf,sizeof(buf)));
            }
            while(! rp.is_done());
        }
    }

    void
    demoWrite()
    {
    }

    void
    run() override
    {
        demoRead();
        demoWrite();
        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,sandbox);

} // http
} // beast
} // boost
