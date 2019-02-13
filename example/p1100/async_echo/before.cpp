#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

namespace net = boost::asio;
using error_code = boost::system::error_code;

template<class Stream, class DynamicBuffer, class CompletionToken>
decltype(auto)
async_echo(Stream& s, DynamicBuffer&& db, CompletionToken&& ct)
{
    net::async_result<CompletionToken, void(error_code)> init{ct};
    using io_ex_t = decltype(s.get_executor());
    using buffer_t = std::decay_t<DynamicBuffer>;
    using handler_t = decltype(init)::completion_handler_type;
    struct operation
    {
        using executor_type = net::associated_executor_t<handler_t, io_ex_t>;

        executor_type get_executor() const noexcept
        {
            return net::get_associated_executor(handler_, wg_.get_executor());
        }

        void operator()(error_code ec = {}, std::size_t n = 0)
        {
            if (ec)
                handler_(ec);
            else if ((do_read_ = !do_read_))
                return net::async_read(
                  stream_,
                  std::move(buffer_), // Leaves buffer_ in a moved-from state
                  net::transfer_at_least(1),
                  std::move(*this));
            else
                return net::async_write(
                  stream_,
                  std::move(buffer_), // possible UB, buffer_ was moved-from
                  std::move(*this));
        }

        handler_t handler_;
        buffer_t buffer_;
        Stream& stream_;
        net::executor_work_guard<io_ex_t> wg_{stream_.get_executor()};
        bool do_read_ = false;
    };

    operation op{std::move(init.completion_handler), std::move(db), s};
    op();
    return init.result.get();
}
