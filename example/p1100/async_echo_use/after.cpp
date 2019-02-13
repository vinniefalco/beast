namespace net = boost::asio;
using error_code = boost::system::error_code;
using dynamic_string_buffer = boost::beast::string_buffer;

template<class Stream, class DynamicBuffer, class CompletionToken>
decltype(auto)
async_echo(Stream& s, DynamicBuffer& db, CompletionToken&& ct);

template<class DynamicBuffer>
shared_dynamic_buffer(DynamicBuffer)->shared_dynamic_buffer<DynamicBuffer>;

template<class Stream>
void
run(Stream&& stream)
{
    using stream_t = std::decay_t<Stream>;

    struct connection
    {
        explicit connection(Stream&& s)
          : stream{std::forward<Stream>(s)}
        {
        }

        stream_t stream;
        dynamic_string_buffer buffer;
    };

    auto conn = std::make_unique<connection>(stream);
    auto& conn_ref = *conn;

    async_echo(conn_ref.stream,
               conn_ref.buffer,
               [conn_ = std::move(conn)](error_code ec)
               {
                   std::cout << "Echo complete: " << ec.message();
               });
}
