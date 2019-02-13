#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/executor_work_guard.hpp>

namespace net = boost::asio;
using error_code = boost::system::error_code;

template<class Stream, class DynamicBuffer, class CompletionToken>
decltype(auto)
async_echo(Stream& s, DynamicBuffer&& db, CompletionToken&& ct);

template<class DynamicBuffer>
class shared_dynamic_buffer
{
public:
    using const_buffers_type = DynamicBuffer::const_buffers_type;
    using mutable_buffers_type = DynamicBuffer::mutable_buffers_type;

    template<class DB>
    explicit shared_dynamic_buffer(DB&& db)
      buf_(std::make_shared<DynamicBuffer>(std::forward<DB>(db)))
    {
    }

    std::size_t size() const noexcept;
    std::size_t max_size() const noexcept;
    std::size_t capacity() const noexcept;
    const_buffers_type data() const noexcept;
    mutable_buffers_type prepare(std::size_t n);
    void commit(std::size_t n);
    void consume(std::size_t n);

private:
    // Need const to prevent move from invalidating.
    std::shared_ptr<DynamicBuffer> const buf_;
};

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
        std::string string;
    };

    auto conn = std::make_unique<connection>(stream);
    auto& conn_ref = *conn;

    async_echo(conn_ref.stream,
               shared_dynamic_buffer(net::dynamic_buffer(conn_ref.string)),
               [conn_ = std::move(conn)](error_code ec)
               {
                   std::cout << "Echo complete: " << ec.message();
               });
}
