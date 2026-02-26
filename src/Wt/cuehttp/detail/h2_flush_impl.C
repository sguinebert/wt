#include "Wt/cuehttp/detail/stream.hpp"


namespace Wt::http::detail {

asio::awaitable<void> h2_flush_impl(stream* s, bool deflate) {
    co_await s->http2_flush(deflate);

}

} // namespace Wt::http::detail
