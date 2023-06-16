/*
 * Copyright (C) 2009 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

// bugfix for https://svn.boost.org/trac/boost/ticket/5722
#include <Wt/AsioWrapper/asio.hpp>

#include "Wt/Http/Client.h"
#include "Wt/WApplication.h"
#include "Wt/WIOService.h"
#include "Wt/WEnvironment.h"
#include "Wt/WLogger.h"
#include "Wt/WServer.h"
#include "Wt/Utils.h"

#include "Wt/WebController.h"
#include "web/WebSession.h"
#include "web/WebUtils.h"

#include <memory>
#include <sstream>
#include <boost/algorithm/string.hpp>

#ifdef WT_THREADED
#include <mutex>
#endif // WT_THREADED

#ifdef WT_WITH_SSL

#include <Wt/AsioWrapper/ssl.hpp>
#include "web/SslUtils.h"

#endif // WT_WITH_SSL

#ifdef WT_WIN32
#define strcasecmp _stricmp
#endif

using Wt::AsioWrapper::asio::ip::tcp;

namespace {
constexpr const int STATUS_NO_CONTENT = 204;
constexpr const int STATUS_MOVED_PERMANENTLY = 301;
constexpr const int STATUS_FOUND = 302;
constexpr const int STATUS_SEE_OTHER = 303;
constexpr const int STATUS_TEMPORARY_REDIRECT = 307;
}

namespace Wt {

namespace asio = AsioWrapper::asio;

LOGGER("Http.Client");

  namespace Http {


class Client::Impl : public std::enable_shared_from_this<Client::Impl>
{
    friend class Client;
public:
  struct ChunkState {
    enum class State { Size, Data, Complete, Error } state;
    std::size_t size;
    int parsePos;
  };

  Impl(Client *client,
       const std::shared_ptr<WebSession>& session,
       asio::io_context& io_context)
    : ioService_(io_context),
      strand_(io_context),
      resolver_(ioService_),
      method_(Http::Method::Get),
      client_(client),
      session_(session),
      timeout_(0),
      maximumResponseSize_(0),
      responseSize_(0),
      postSignals_(session != nullptr),
      aborted_(false)
  { }

  virtual ~Impl() { }

  void removeClient()
  {
#ifdef WT_THREADED
    std::lock_guard<std::mutex> lock(clientMutex_);
#endif // WT_THREADED
    client_ = nullptr;
  }

  void setTimeout(std::chrono::steady_clock::duration timeout) { 
    timeout_ = timeout; 
  }

  void setMaximumResponseSize(std::size_t bytes) {
    maximumResponseSize_ = bytes;
  }

  awaitable<void> request(Http::Method method,
                          const std::string protocol,
                          const std::string auth,
                          const std::string server,
                          int port,
                          const std::string path,
                          std::shared_ptr<Impl> ptr,
                          bool emitdone)
  {
    const char *methodNames_[] = { "GET", "POST", "PUT", "DELETE", "PATCH", "HEAD" };

    method_ = method;
    //request_ = message;

    std::ostream request_stream(&requestBuf_);
    request_stream << methodNames_[static_cast<unsigned int>(method)] << " " << path << " HTTP/1.1\r\n";
    if ((protocol == "http" && port == 80) || (protocol == "https" && port == 443))
      request_stream << "Host: " << server << "\r\n";
    else
      request_stream << "Host: " << server << ":"
                     << std::to_string(port) << "\r\n";

    if (!auth.empty())
      request_stream << "Authorization: Basic " 
		     << Wt::Utils::base64Encode(auth) << "\r\n";

    bool haveContentLength = false;
    for (unsigned i = 0; i < request_.headers().size(); ++i) {
      const Message::Header& h = request_.headers()[i];
      if (strcasecmp(h.name().c_str(), "Content-Length") == 0)
        haveContentLength = true;
      request_stream << h.name() << ": " << h.value() << "\r\n";
    }

    if ((method == Http::Method::Post || method == Http::Method::Put || method == Http::Method::Delete || method == Http::Method::Patch) && !haveContentLength)
      request_stream << "Content-Length: " << request_.body().length() << "\r\n";

    request_stream << "\r\n";

    if (method == Http::Method::Post || method == Http::Method::Put || method == Http::Method::Delete || method == Http::Method::Patch)
      request_stream << request_.body();

    tcp::resolver::query query(server, std::to_string(port));

    //startTimer();

    auto [ec, endpoint_iterator] = co_await resolver_.async_resolve(query, use_nothrow_awaitable);

    //    handleResolve(ec, endpoint);
    cancelTimer();

    if (!ec && !aborted_) {
      // Attempt a connection to the first endpoint in the list.
      // Each endpoint will be tried until we successfully establish
      // a connection.
      tcp::endpoint endpoint = *endpoint_iterator;

      //startTimer();

      //co_await asyncConnect(endpoint);
//      asyncConnect(endpoint, std::bind(&Impl::handleConnect,
//                                          shared_from_this(),
//                                          std::placeholders::_1,
//                                          ++endpoint_iterator));
      //if TCP or SSL
      for(;;)
      {

        auto [ec] = co_await socket().lowest_layer().async_connect(endpoint, use_nothrow_awaitable);

        //handleConnect(ec, ++endpoint_iterator);

        /* Within strand */

        cancelTimer();

        if (!ec && !aborted_) {
            // The connection was successful. Do the handshake (SSL only)
            //startTimer();

            auto err  = co_await asyncHandshake();//  asyncHandshake(strand_.wrap(std::bind(&Impl::handleHandshake, shared_from_this(), std::placeholders::_1)));

            /* handleHandshake */
            cancelTimer();

            if (!err && !aborted_) {
                // The handshake was successful. Send the request.
                //startTimer();

                //asyncWriteRequest(strand_.wrap(std::bind(&Impl::handleWriteRequest, shared_from_this(), std::placeholders::_1, std::placeholders::_2)));
                auto [ec, bytes] = co_await asio::async_write(socket(), requestBuf_, use_nothrow_awaitable);

                /* handleWriteRequest */

                cancelTimer();

                if (!ec && !aborted_) {
                    // Read the response status line.
                    //startTimer();

                    //asyncReadUntil("\r\n", strand_.wrap(std::bind(&Impl::handleReadStatusLine, shared_from_this(), std::placeholders::_1, std::placeholders::_2)));
                    auto [ecc, readbytes] = co_await asio::async_read_until(socket(), responseBuf_, "\r\n", use_nothrow_awaitable);

                    co_await handleReadStatusLine(ecc, readbytes);
                    co_return; //END of request

                } else {
                    if (aborted_)
                        err_ = asio::error::operation_aborted;
                    else
                        err_ = ec;
                    co_await complete();
                }

            } else {
                if (aborted_)
                    err_ = asio::error::operation_aborted;
                else
                    err_ = err;
                co_await complete();
            }


        } else if (endpoint_iterator != tcp::resolver::iterator()) {
            // The connection failed. Try the next endpoint in the list.
            socket().close();

            /* Within strand */

            cancelTimer();

            if (!ec && !aborted_) {
                // Attempt a connection to the first endpoint in the list.
                // Each endpoint will be tried until we successfully establish
                // a connection.
                ++endpoint_iterator;
                endpoint = *endpoint_iterator;

                //startTimer();
                continue; //co_await asyncConnect(endpoint);
                //asyncConnect(endpoint, strand_.wrap(std::bind(&Impl::handleConnect, shared_from_this(), std::placeholders::_1, ++endpoint_iterator)));
            } else {
                if (aborted_)
                    err_ = asio::error::operation_aborted;
                else
                    err_ = ec;
                co_await complete();
                break;
            }
        } else {
            if (aborted_)
                err_ = asio::error::operation_aborted;
            else
                err_ = ec;
            co_await complete();
            break;
        }
      }



    } else {
      if (aborted_)
        err_ = asio::error::operation_aborted;
      else
        err_ = ec;
      co_await complete();
    }
    co_return;
  }

  void asyncStop()
  {
    ioService_.post(std::bind(&Impl::stop, shared_from_this()));
  }

//  tcp::socket& vsocket()
//  {
//    if (auto cptr = std::get_if<std::unique_ptr<asio::ssl::stream<tcp::socket>>>(&connection_)) {
//        return cptr->get()->next_layer();
//    }
//    return *std::get<std::unique_ptr<tcp::socket>>(connection_);
//  }

protected:
//  std::variant<std::unique_ptr<tcp::socket>, std::unique_ptr<asio::ssl::stream<tcp::socket>>> connection_;
//    awaitable<AsioWrapper::error_code> asyncHandshake(asio::ssl::stream<tcp::socket>& socket) {
//        if (verifyEnabled_) {
//            socket.set_verify_mode(asio::ssl::verify_peer);
//            LOG_DEBUG("verifying that peer is {}", hostName_);
//            socket.set_verify_callback(asio::ssl::rfc2818_verification(hostName_));
//        }
//        auto [ec] = co_await socket.async_handshake(asio::ssl::stream_base::client, use_nothrow_awaitable);
//        co_return ec;
//    }
    awaitable<AsioWrapper::error_code> asyncHandshake(tcp::socket& socket)
    {
        co_return AsioWrapper::error_code();
    }


  typedef std::function<void(const AsioWrapper::error_code&)> ConnectHandler;
  typedef std::function<void(const AsioWrapper::error_code&, const std::size_t&)> IOHandler;
  virtual tcp::socket& socket() = 0;
//  virtual void asyncConnect(tcp::endpoint& endpoint, const ConnectHandler& handler) = 0;
  virtual awaitable<AsioWrapper::error_code> asyncHandshake() = 0;
//  virtual void asyncWriteRequest(const IOHandler& handler) = 0;
//  virtual void asyncReadUntil(const std::string& s, const IOHandler& handler) = 0;
//  virtual void asyncRead(const IOHandler& handler) = 0;

private:
  void stop()
  {
    /* Within strand */

    aborted_ = true;

    try {
      if (socket().is_open()) {
        AsioWrapper::error_code ignored_ec;
        socket().shutdown(tcp::socket::shutdown_both, ignored_ec);
        socket().close();
      }
    } catch (std::exception& e) {
      LOG_INFO("Client::abort(), stop(), ignoring error: {}", e.what());
    }
  }

  awaitable<void> watchdog(steady_clock::time_point& deadline)
  {
    asio::steady_timer timer(co_await boost::asio::this_coro::executor);
    deadline = steady_clock::now() + timeout_;

    auto now = steady_clock::now();
    while (deadline > now)
    {
      timer.expires_at(deadline);
      //co_await timer.async_wait(asio::bind_cancellation_slot(timer_cancel_.slot(), use_nothrow_awaitable));
      co_await timer.async_wait(use_nothrow_awaitable);
      now = steady_clock::now();
    }
    //kill
    //co_await close();
    AsioWrapper::error_code ignored_ec;
    socket().shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
    err_ = asio::error::timed_out;
    co_return;
  }

//  void startTimer()
//  {
//    timer_.expires_from_now(timeout_);
//    timer_.async_wait(strand_.wrap(std::bind(&Impl::timeout, shared_from_this(), std::placeholders::_1)));
//  }

  void cancelTimer()
  {
    /* Within strand */
    deadline_ = std::max(deadline_, steady_clock::now() + timeout_);
    //timer_.cancel();
  }

  void timeout(const AsioWrapper::error_code& e)
  {
    /* Within strand */

    if (e != asio::error::operation_aborted) {
      AsioWrapper::error_code ignored_ec;
      socket().shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);

      err_ = asio::error::timed_out;
    }
  }

//  awaitable<void> handleResolve(const AsioWrapper::error_code& err, tcp::resolver::iterator endpoint_iterator)
//  {
//    /* Within strand */

//    cancelTimer();

//    if (!err && !aborted_) {
//      // Attempt a connection to the first endpoint in the list.
//      // Each endpoint will be tried until we successfully establish
//      // a connection.
//      tcp::endpoint endpoint = *endpoint_iterator;

//      startTimer();

//      //co_await asyncConnect(endpoint);
//      asyncConnect(endpoint, strand_.wrap(std::bind(&Impl::handleConnect, shared_from_this(), std::placeholders::_1, ++endpoint_iterator)));
//    } else {
//      if (aborted_)
//        err_ = asio::error::operation_aborted;
//      else
//        err_ = err;
//      co_await complete();
//    }
//    co_return;
//  }
 
//  void handleConnect(const AsioWrapper::error_code& err,
//		     tcp::resolver::iterator endpoint_iterator)
//  {
//    /* Within strand */

//    cancelTimer();

//    if (!err && !aborted_) {
//      // The connection was successful. Do the handshake (SSL only)
//      startTimer();
//      asyncHandshake(); //asyncHandshake(strand_.wrap(std::bind(&Impl::handleHandshake, shared_from_this(), std::placeholders::_1)));
//    } else if (endpoint_iterator != tcp::resolver::iterator()) {
//      // The connection failed. Try the next endpoint in the list.
//      socket().close();

//      handleResolve(AsioWrapper::error_code(), endpoint_iterator);
//    } else {
//      if (aborted_)
//        err_ = asio::error::operation_aborted;
//      else
//        err_ = err;
//      complete();
//    }
//  }

//  awaitable<void> handleHandshake(const AsioWrapper::error_code& err)
//  {
//    /* Within strand */

//    cancelTimer();

//    if (!err && !aborted_) {
//      // The handshake was successful. Send the request.
//      startTimer();
//      asyncWriteRequest
//	(strand_.wrap
//	 (std::bind(&Impl::handleWriteRequest,
//		      shared_from_this(),
//		      std::placeholders::_1,
//		      std::placeholders::_2)));
//    } else {
//      if (aborted_)
//        err_ = asio::error::operation_aborted;
//      else
//        err_ = err;
//      co_await complete();
//    }
//    co_return;
//  }

//  awaitable<void> handleWriteRequest(const AsioWrapper::error_code& err,
//			  const std::size_t&)
//  {
//    /* Within strand */

//    cancelTimer();

//    if (!err && !aborted_) {
//      // Read the response status line.
//      startTimer();
//      asyncReadUntil
//	("\r\n",
//	 strand_.wrap
//	 (std::bind(&Impl::handleReadStatusLine,
//		      shared_from_this(),
//		      std::placeholders::_1,
//		      std::placeholders::_2)));
//    } else {
//      if (aborted_)
//        err_ = asio::error::operation_aborted;
//      else
//        err_ = err;
//      co_await complete();
//    }
//    co_return;
//  }

  bool addResponseSize(std::size_t s)
  {
    responseSize_ += s;

    if (maximumResponseSize_ && responseSize_ > maximumResponseSize_) {
      err_ = asio::error::message_size;
      return false;
    }

    return true;
  }

  awaitable<void> handleReadStatusLine(const AsioWrapper::error_code& err, const std::size_t& s)
  {
    /* Within strand */

    cancelTimer();

    if (!err && !aborted_)
    {
      if (!addResponseSize(s))
      {
        co_await complete();
        co_return;
      }

      // Check that response is OK.
      std::istream response_stream(&responseBuf_);
      std::string http_version;
      response_stream >> http_version;
      unsigned int status_code;
      response_stream >> status_code;
      std::string status_message;
      std::getline(response_stream, status_message);
      if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
#ifdef WT_ASIO_IS_BOOST_ASIO
	err_ = boost::system::errc::make_error_code
	  (boost::system::errc::protocol_error);
#else
	err_ = std::make_error_code(std::errc::protocol_error);
#endif
        co_await complete();
        co_return;
      }

      LOG_DEBUG("{} {}", status_code, status_message);

      response_.setStatus(status_code);

      // Read the response headers, which are terminated by a blank line.
      //startTimer();

      //asyncReadUntil("\r\n\r\n", strand_.wrap(std::bind(&Impl::handleReadHeaders, shared_from_this(), std::placeholders::_1, std::placeholders::_2)));
      auto [ec, readbytes] = co_await asio::async_read_until(socket(), responseBuf_, "\r\n\r\n", use_nothrow_awaitable);
      co_await handleReadHeaders(ec, readbytes);
    }
    else
    {
      if (aborted_)
        err_ = asio::error::operation_aborted;
      else
        err_ = err;
      co_await complete();
    }
    co_return;
  }

  awaitable<void> handleReadHeaders(const AsioWrapper::error_code& err, const std::size_t& s)
  {
    /* Within strand */

    cancelTimer();

    if (!err && !aborted_) {
      if (!addResponseSize(s)) {
        co_await complete();
        co_return;
      }

      chunkedResponse_ = false;
      contentLength_ = -1;

      // Process the response headers.
      std::istream response_stream(&responseBuf_);
      std::string header;
      while (std::getline(response_stream, header) && header != "\r")
      {
        std::size_t i = header.find(':');
        if (i != std::string::npos)
        {
          std::string name = boost::trim_copy(header.substr(0, i));
          std::string value = boost::trim_copy(header.substr(i+1));
          response_.addHeader(name, value);

          if (boost::iequals(name, "Transfer-Encoding") &&
              boost::iequals(value, "chunked")) {
            chunkedResponse_ = true;
            chunkState_.size = 0;
            chunkState_.parsePos = 0;
            chunkState_.state = ChunkState::State::Size;
          } else if (method_ != Http::Method::Head &&
                         boost::iequals(name, "Content-Length")) {
            std::stringstream ss(value);
            ss >> contentLength_;
          }
        }
      }

      if (postSignals_) {
        auto session = session_.lock();
        if (session) {
          auto server = session->controller()->server();
          server->post(session->sessionId(), std::bind(&Impl::emitHeadersReceived, shared_from_this()));
        }
      } else {
        co_await emitHeadersReceived();
      }

      bool done = method_ == Http::Method::Head || response_.status() == STATUS_NO_CONTENT || contentLength_ == 0;
      // Write whatever content we already have to output.
      if (responseBuf_.size() > 0) {
        std::stringstream ss;
        ss << &responseBuf_;
        done = co_await addBodyText(ss.str());
      }

      if (!done)
      {
        // Start reading remaining data until EOF.
        //startTimer();
        //asyncRead(strand_.wrap(std::bind(&Impl::handleReadContent, shared_from_this(), std::placeholders::_1, std::placeholders::_2)));
        auto [ec, read] = co_await asio::async_read(socket(), responseBuf_, asio::transfer_at_least(1), use_nothrow_awaitable);
        co_await handleReadContent(ec, read);

      } else {
        co_await complete();
      }
    } else {
      if (!aborted_)
        err_ = asio::error::operation_aborted;
      else
        err_ = err;
      co_await complete();
    }
  }

  awaitable<void> handleReadContent(const AsioWrapper::error_code& err, const std::size_t& s)
  {
    /* Within strand */

    cancelTimer();

    if (!err && !aborted_) {
      if (!addResponseSize(s)) {
        co_await complete();
        co_return;
      }

      std::stringstream ss;
      ss << &responseBuf_;

      bool done = co_await addBodyText(ss.str());

      if (!done) {
        // Continue reading remaining data until EOF.
        //startTimer();
        //asyncRead(strand_.wrap(std::bind(&Impl::handleReadContent, shared_from_this(), std::placeholders::_1, std::placeholders::_2)));
        auto [ec, read] = co_await asio::async_read(socket(), responseBuf_, asio::transfer_at_least(1), use_nothrow_awaitable);
        co_await handleReadContent(ec, read);
      } else {
        co_await complete();
      }
    } else if (!aborted_
               && err != asio::error::eof
	       && err != asio::error::shut_down
	       && err != asio::error::bad_descriptor
	       && err != asio::error::operation_aborted
	       && err.value() != 335544539) {
      err_ = err;
      co_await complete();
    } else {
      if (aborted_)
        err_ = asio::error::operation_aborted;
      co_await complete();
    }
    co_return;
  }

  // Returns whether we're done (caller must call complete())
  awaitable<bool> addBodyText(const std::string& text)
  {
    if (chunkedResponse_) {
      co_await chunkedDecode(text);
      if (chunkState_.state == ChunkState::State::Error) {
        protocolError();
        co_return true;
      } else if (chunkState_.state == ChunkState::State::Complete) {
        co_return true;
      } else
        co_return false;
    } else {
      if (maximumResponseSize_)
        response_.addBodyText(text);

      LOG_DEBUG("Data: {}", text);
      co_await haveBodyData(text);

      co_return (contentLength_ >= 0) && (response_.body().size() >= contentLength_);
    }
  }

  awaitable<void> chunkedDecode(const std::string& text)
  {
    std::string::const_iterator pos = text.begin();
    while (pos != text.end()) {
      switch (chunkState_.state) {
      case ChunkState::State::Size: {
	unsigned char ch = *(pos++);

	switch (chunkState_.parsePos) {
	case -2:
	  if (ch != '\r') {
        chunkState_.state = ChunkState::State::Error; co_return;
	  }

	  chunkState_.parsePos = -1;

	  break;
	case -1:
	  if (ch != '\n') {
        chunkState_.state = ChunkState::State::Error; co_return;
	  }

	  chunkState_.parsePos = 0;
	  
	  break;
	case 0:
	  if (ch >= '0' && ch <= '9') {
	    chunkState_.size <<= 4;
	    chunkState_.size |= (ch - '0');
	  } else if (ch >= 'a' && ch <= 'f') {
	    chunkState_.size <<= 4;
	    chunkState_.size |= (10 + ch - 'a');
	  } else if (ch >= 'A' && ch <= 'F') {
	    chunkState_.size <<= 4;
	    chunkState_.size |= (10 + ch - 'A');
	  } else if (ch == '\r') {
	    chunkState_.parsePos = 2;
	  } else if (ch == ';') {
	    chunkState_.parsePos = 1;
	  } else {
         chunkState_.state = ChunkState::State::Error; co_return;
	  }

	  break;
	case 1:
	  /* Ignoring extensions and syntax for now */
	  if (ch == '\r')
	    chunkState_.parsePos = 2;

	  break;
	case 2:
	  if (ch != '\n') {
        chunkState_.state = ChunkState::State::Error; co_return;
	  }

	  if (chunkState_.size == 0) {
        chunkState_.state = ChunkState::State::Complete; co_return;
	  }
	    
	  chunkState_.state = ChunkState::State::Data;
	}

	break;
      }
      case ChunkState::State::Data: {
	std::size_t thisChunk
	  = std::min(std::size_t(text.end() - pos), chunkState_.size);
	std::string text = std::string(pos, pos + thisChunk);
	if (maximumResponseSize_)
      response_.addBodyText(text);

	LOG_DEBUG("Chunked data: {}", text);
    co_await haveBodyData(text);
	chunkState_.size -= thisChunk;
	pos += thisChunk;

	if (chunkState_.size == 0) {
	  chunkState_.parsePos = -2;
	  chunkState_.state = ChunkState::State::Size;
	}
	break;
      }
      default:
	assert(false); // Illegal state
      }
    }
    co_return;
  }

  void protocolError()
  {
#ifdef WT_ASIO_IS_BOOST_ASIO
    err_ = boost::system::errc::make_error_code
      (boost::system::errc::protocol_error);
#else
    err_ = std::make_error_code(std::errc::protocol_error);
#endif
  }

  awaitable<void> complete()
  {
    stop();
    if (postSignals_) {
      auto session = session_.lock();
      if (session) {
        auto server = session->controller()->server();
        server->post(session->sessionId(), std::bind(&Impl::emitDone, shared_from_this()));
      }
    } else {
      co_await emitDone();
    }
    co_return;
  }

  awaitable<void> haveBodyData(std::string text)
  {
    if (postSignals_) {
      auto session = session_.lock();
      if (session) {
        auto server = session->controller()->server();
        server->post(session->sessionId(), std::bind(&Impl::emitBodyReceived, shared_from_this(), text));
      }
    } else {
      co_await emitBodyReceived(text);
    }
    co_return;
  }

  awaitable<void> emitDone()
  {
#ifdef WT_THREADED
    std::lock_guard<std::mutex> lock(clientMutex_);
#endif // WT_THREADED
    if (client_) {
      if (client_->followRedirect()) {
        co_await client_->handleRedirect(method_,
                                         err_,
                                         response_,
                                         request_);
      } else {
        co_await client_->emitDone(err_, response_);
      }
    }
    co_return;
  }

  awaitable<void> emitHeadersReceived() {
#ifdef WT_THREADED
    std::lock_guard<std::mutex> lock(clientMutex_);
#endif // WT_THREADED
    if (client_) {
      co_await client_->emitHeadersReceived(response_);
    }
    co_return;
  }

  awaitable<void> emitBodyReceived(std::string& text) {
#ifdef WT_THREADED
    std::lock_guard<std::mutex> lock(clientMutex_);
#endif // WT_THREADED
    if (client_) {
      co_await client_->emitBodyReceived(text);
    }
    co_return;
  }

protected:
  asio::io_context& ioService_;
  AsioWrapper::strand strand_;
  tcp::resolver resolver_;
  asio::streambuf requestBuf_;
  asio::streambuf responseBuf_;
  Http::Message request_;
  Http::Method method_;

  bool verifyEnabled_;
  std::string hostName_;

private:
#ifdef WT_THREADED
  std::mutex clientMutex_;
#endif // WT_THREADED
  Client *client_;
  std::weak_ptr<WebSession> session_;
  steady_clock::time_point deadline_;
  std::chrono::steady_clock::duration timeout_;
  std::size_t maximumResponseSize_, responseSize_;
  bool chunkedResponse_;
  ChunkState chunkState_;
  std::size_t contentLength_;
  AsioWrapper::error_code err_;
  Message response_;
  bool postSignals_;
  bool aborted_;
};

class Client::TcpImpl final : public Client::Impl
{
public:
  TcpImpl(Client *client,
          const std::shared_ptr<WebSession>& session,
          asio::io_context& ioService)
    : Impl(client, session, ioService),
      socket_(ioService_)
  { }

protected:
  virtual tcp::socket& socket() override
  {
    return socket_;
  }

//  virtual void asyncConnect(tcp::endpoint& endpoint,
//			    const ConnectHandler& handler) override
//  {
//    socket_.async_connect(endpoint, handler);
//  }

  virtual awaitable<AsioWrapper::error_code> asyncHandshake() override
  {
    //handler(AsioWrapper::error_code());
    co_return AsioWrapper::error_code();
  }

//  virtual void asyncWriteRequest(const IOHandler& handler) override
//  {
//    asio::async_write(socket_, requestBuf_, handler);
//  }

//  virtual void asyncReadUntil(const std::string& s,
//			      const IOHandler& handler) override
//  {
//    asio::async_read_until(socket_, responseBuf_, s, handler);
//  }

//  virtual void asyncRead(const IOHandler& handler) override
//  {
//    asio::async_read(socket_, responseBuf_,
//                            asio::transfer_at_least(1), handler);
//  }

private:
  tcp::socket socket_;
};

#ifdef WT_WITH_SSL

class Client::SslImpl final : public Client::Impl
{
public:
  SslImpl(Client *client,
          const std::shared_ptr<WebSession>& session,
          asio::io_context& ioService,
          bool verifyEnabled,
	  asio::ssl::context& context,
	  const std::string& hostName)
    : Impl(client, session, ioService),
      socket_(ioService_, context),
      verifyEnabled_(verifyEnabled),
      hostName_(hostName)
  {
#ifndef OPENSSL_NO_TLSEXT
    if (!SSL_set_tlsext_host_name(socket_.native_handle(), hostName.c_str())) {
      LOG_ERROR("could not set tlsext host.");
    }
#endif
  }

protected:
  virtual tcp::socket& socket() override
  {
    return socket_.next_layer();
  }

//  virtual void asyncConnect(tcp::endpoint& endpoint,
//			    const ConnectHandler& handler) override
//  {
//    socket_.lowest_layer().async_connect(endpoint, handler);
//  }

  virtual awaitable<AsioWrapper::error_code> asyncHandshake() override
  {
    if (verifyEnabled_) {
      socket_.set_verify_mode(asio::ssl::verify_peer);
      LOG_DEBUG("verifying that peer is {}", hostName_);
      socket_.set_verify_callback(asio::ssl::rfc2818_verification(hostName_));
    }
    auto [ec] = co_await socket_.async_handshake(asio::ssl::stream_base::client, use_nothrow_awaitable);
    co_return ec;
  }

//  virtual void asyncWriteRequest(const IOHandler& handler) override
//  {
//    asio::async_write(socket_, requestBuf_, handler);
//  }

//  virtual void asyncReadUntil(const std::string& s,
//			      const IOHandler& handler) override
//  {
//    asio::async_read_until(socket_, responseBuf_, s, handler);
//  }

//  virtual void asyncRead(const IOHandler& handler) override
//  {
//    asio::async_read(socket_, responseBuf_,
//                            asio::transfer_at_least(1), handler);
//  }

private:
  typedef asio::ssl::stream<tcp::socket> ssl_socket;

  ssl_socket socket_;
  bool verifyEnabled_;
  std::string hostName_;
};
#endif // WT_WITH_SSL

Client::Client()
    : io_context_(0),
    timeout_(std::chrono::seconds{10}),
    maximumResponseSize_(64*1024),
#ifdef WT_WITH_SSL
    verifyEnabled_(true),
#else
    verifyEnabled_(false),
#endif
    followRedirect_(false),
    redirectCount_(0),
    maxRedirects_(20)
{ }

Client::Client(asio::io_context& ioService)
    : io_context_(&ioService),
    timeout_(std::chrono::seconds{10}),
    maximumResponseSize_(64*1024),
#ifdef WT_WITH_SSL
    verifyEnabled_(true),
#else
    verifyEnabled_(false),
#endif
    followRedirect_(false),
    redirectCount_(0),
    maxRedirects_(20)
{ }

Client::~Client()
{
  abort();
  auto impl = impl_.lock();
  if (impl) {
    impl->removeClient();
  }
}

void Client::setSslCertificateVerificationEnabled(bool enabled)
{
  verifyEnabled_ = enabled;
}

void Client::abort()
{
  std::shared_ptr<Impl> impl = impl_.lock();
  if (impl) {
    impl->asyncStop();
  }
}

bool Client::parseUrl(const std::string &url, URL &parsedUrl)
{
  std::size_t i = url.find("://");
  if (i == std::string::npos) {
    LOG_ERROR("ill-formed URL: {}", url);
    return false;
  }

  parsedUrl.protocol = url.substr(0, i);
  std::string rest = url.substr(i + 3);
  // find auth
  std::size_t l = rest.find('@');
  // find host
  std::size_t j = rest.find('/');
  if (l != std::string::npos &&
      (j == std::string::npos || j > l)) {
    // above check: userinfo can not contain a forward slash
    // path may contain @ (issue #7272)
    parsedUrl.auth = rest.substr(0, l);
    parsedUrl.auth = Wt::Utils::urlDecode(parsedUrl.auth);
    rest = rest.substr(l+1);
    if (j != std::string::npos) {
      j -= l + 1;
    }
  }

  if (j == std::string::npos) {
    parsedUrl.host = rest;
    parsedUrl.path = "/";
  } else {
    parsedUrl.host = rest.substr(0, j);
    parsedUrl.path = rest.substr(j);
  }

  std::size_t k = parsedUrl.host.find(':');
  if (k != std::string::npos) {
    try {
      parsedUrl.port = Utils::stoi(parsedUrl.host.substr(k + 1));
    } catch (std::exception& e) {
      LOG_ERROR("invalid port: {}", parsedUrl.host.substr(k + 1));
      return false;
    }
    parsedUrl.host = parsedUrl.host.substr(0, k);
  } else {
    if (parsedUrl.protocol == "http")
      parsedUrl.port = 80;
    else if (parsedUrl.protocol == "https")
      parsedUrl.port = 443;
    else
      parsedUrl.port = 80; // protocol will not be handled anyway
  }

  return true;
}

void Client::setTimeout(std::chrono::steady_clock::duration timeout)
{
  timeout_ = timeout;
}

void Client::setMaximumResponseSize(std::size_t bytes)
{
  maximumResponseSize_ = bytes;
}

void Client::setSslVerifyFile(const std::string& file)
{
  verifyFile_ = file;
}

void Client::setSslVerifyPath(const std::string& path)
{
  verifyPath_ = path;
}

bool Client::get(const std::string& url)
{
  return request(Http::Method::Get, url, Message());
}

bool Client::get(const std::string& url,
                 const std::vector<Message::Header> headers)
{
  Message m(headers);
  return request(Http::Method::Get, url, m);
}

bool Client::head(const std::string& url)
{
  return request(Http::Method::Head, url, Message());
}

bool Client::head(const std::string& url,
                  const std::vector<Message::Header> headers)
{
  Message m(headers);
  return request(Http::Method::Head, url, m);
}

bool Client::post(const std::string& url, const Message& message)
{
  return request(Http::Method::Post, url, message);
}

bool Client::put(const std::string& url, const Message& message)
{
  return request(Http::Method::Put, url, message);
}

bool Client::deleteRequest(const std::string& url, const Message& message)
{
  return request(Http::Method::Delete, url, message);
}

bool Client::patch(const std::string& url, const Message& message)
{
  return request(Http::Method::Patch, url, message);
}

bool Client::request(Http::Method method, const std::string& url,
                     const Message& message)
{
  asio::io_context *io_context = io_context_;

  WApplication *app = WApplication::instance();

  auto impl = impl_.lock();
  if (impl) {
    LOG_ERROR("another request is in progress");
    return false;
  }

  WebSession *session = nullptr;

  if (app && !io_context) {
    // Use WServer's IO service, and post events to WApplication
    session = app->session();
    //auto server = session->controller()->server();
    io_context = thread_context;// &server->ioService();
  } else if (!io_context) {
    // Take IO service from server
    auto server = WServer::instance();

    if (server)
      io_context = thread_context; //&server->ioService();
    else {
      LOG_ERROR("requires a WIOService for async I/O");
      return false;
    }
  }

  URL parsedUrl;

  if (!parseUrl(url, parsedUrl))
    return false;

  if (parsedUrl.protocol == "http") {
    impl = std::make_shared<TcpImpl>(this, session ? session->shared_from_this() : nullptr, *io_context);
    impl_ = impl;

#ifdef WT_WITH_SSL
  } else if (parsedUrl.protocol == "https") {
    asio::ssl::context context = Ssl::createSslContext(*io_context, verifyEnabled_);

    if (!verifyFile_.empty() || !verifyPath_.empty()) {
      if (!verifyFile_.empty())
	context.load_verify_file(verifyFile_);
      if (!verifyPath_.empty())
	context.add_verify_path(verifyPath_);
    }

    impl = std::make_shared<SslImpl>(this,
                                     session ? session->shared_from_this() : nullptr,
                                     *io_context,
                                     verifyEnabled_,
                                     context,
                                     parsedUrl.host);
    impl_ = impl;
#endif // WT_WITH_SSL

  } else {
    LOG_ERROR("unsupported protocol: {}", parsedUrl.protocol);
    return false;
  }

  impl->request_ = message;

  impl->setTimeout(timeout_);
  impl->setMaximumResponseSize(maximumResponseSize_);

  co_spawn(*io_context_, impl->request(method,
                                       parsedUrl.protocol,
                                       parsedUrl.auth,
                                       parsedUrl.host,
                                       parsedUrl.port,
                                       parsedUrl.path,
                                       impl->shared_from_this(), true) || impl->watchdog(impl->deadline_), detached);


//  co_spawn(*io_context_, impl->request(method,
//                                       parsedUrl.protocol,
//                                       parsedUrl.auth,
//                                       parsedUrl.host,
//                                       parsedUrl.port,
//                                       parsedUrl.path,
//                                       message), detached);

  return true;
}

awaitable<Message> Client::co_request(Http::Method method, const std::string &url, const Message &message)
{
  asio::io_context *io_context = io_context_;

  WApplication *app = WApplication::instance();

  //auto impl = impl_.lock();

  WebSession *session = nullptr;

  if (app && !io_context) {
    // Use WServer's IO service, and post events to WApplication
    session = app->session();
    auto server = session->controller()->server();
    io_context = &server->ioService().get();// &server->ioService();
  } else if (!io_context) {
    // Take IO service from server
    auto server = WServer::instance();

    if (server)
      io_context = &server->ioService().get(); //&server->ioService();
    else {
      LOG_ERROR("requires a WIOService for async I/O");
      co_return Message();
    }
  }

  URL parsedUrl;

  if (!parseUrl(url, parsedUrl))
    co_return Message();

  if (parsedUrl.protocol == "http") {
    auto tcpimpl_ = std::make_shared<TcpImpl>(this, session ? session->shared_from_this() : nullptr, *io_context);

    tcpimpl_->request_ = message;

    tcpimpl_->setTimeout(timeout_);
    tcpimpl_->setMaximumResponseSize(maximumResponseSize_);

    co_await tcpimpl_->request(method,
                               parsedUrl.protocol,
                               parsedUrl.auth,
                               parsedUrl.host,
                               parsedUrl.port,
                               parsedUrl.path,
                               tcpimpl_->shared_from_this(),
                               false);
    co_return tcpimpl_->response_;

#ifdef WT_WITH_SSL
  } else if (parsedUrl.protocol == "https") {
    asio::ssl::context context = Ssl::createSslContext(*io_context, verifyEnabled_);

    if (!verifyFile_.empty() || !verifyPath_.empty()) {
      if (!verifyFile_.empty())
        context.load_verify_file(verifyFile_);
      if (!verifyPath_.empty())
        context.add_verify_path(verifyPath_);
    }
    auto sslimpl_ = std::make_shared<SslImpl>(this,
                                              session ? session->shared_from_this() : nullptr,
                                              *io_context,
                                              verifyEnabled_,
                                              context,
                                              parsedUrl.host);

    sslimpl_->request_ = message;

    sslimpl_->setTimeout(timeout_);
    sslimpl_->setMaximumResponseSize(maximumResponseSize_);

    co_await sslimpl_->request(method,
                               parsedUrl.protocol,
                               parsedUrl.auth,
                               parsedUrl.host,
                               parsedUrl.port,
                               parsedUrl.path,
                               sslimpl_->shared_from_this(),
                               false);

    co_return sslimpl_->response_;
#endif // WT_WITH_SSL

  } else {
    LOG_ERROR("unsupported protocol: {}", parsedUrl.protocol);
    co_return Message();
  }

  co_return Message();
}

bool Client::followRedirect() const
{
  return followRedirect_;
}

void Client::setFollowRedirect(bool followRedirect)
{
  followRedirect_ = followRedirect;
}

int Client::maxRedirects() const
{
  return maxRedirects_;
}

void Client::setMaxRedirects(int maxRedirects)
{
  maxRedirects_ = maxRedirects;
}

awaitable<void> Client::handleRedirect(Http::Method method,
                                       AsioWrapper::error_code err,
                                       const Message& response, const Message& request)
{
  impl_.reset();
  int status = response.status();
  if (!err && (((status == STATUS_MOVED_PERMANENTLY ||
                 status == STATUS_FOUND ||
                 status == STATUS_TEMPORARY_REDIRECT) && method == Http::Method::Get) ||
               status == STATUS_SEE_OTHER)) {
    const std::string *newUrl = response.getHeader("Location");
    ++ redirectCount_;
    if (newUrl)
    {
      if (redirectCount_ <= maxRedirects_)
      {
        get(*newUrl, request.headers());
        co_return;
      }
      else
      {
        LOG_WARN("Redirect count of {} exceeded! Redirect URL: {}", maxRedirects_, *newUrl);
      }
    }
  }
  co_await emitDone(err, response);
}

awaitable<void> Client::emitDone(AsioWrapper::error_code err, const Message& response)
{
  impl_.reset();
  redirectCount_ = 0;
  co_await done_.emit(err, (Message&)response);
}

awaitable<void> Client::emitHeadersReceived(Message& response)
{
  co_await headersReceived_.emit(response);
}

awaitable<void> Client::emitBodyReceived(std::string& data)
{
  co_await bodyDataReceived_.emit(data);
}



  }
}
