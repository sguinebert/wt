/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include <fstream>

#include "Wt/WLogger.h"
#include "Wt/WFileResource.h"

namespace Wt {

LOGGER("WFileResource");

WFileResource::WFileResource()
{ }

WFileResource::WFileResource(const std::string& fileName)
  : fileName_(fileName)
{ }

WFileResource::WFileResource(const std::string& mimeType,
			     const std::string& fileName)
  : WStreamResource(mimeType),
    fileName_(fileName)
{ }

WFileResource::~WFileResource()
{
  beingDeleted();
}

awaitable<void> WFileResource::setFileName(const std::string& fileName)
{
  fileName_ = fileName;
  co_await setChanged();
}
#ifdef DEPRECATED_OK
void WFileResource::handleRequest(const Http::Request& request,
				  Http::Response& response)
{
  std::ifstream r(fileName_.c_str(), std::ios::in | std::ios::binary);
  if (!r) {
    LOG_ERROR("Could not open file for reading: {}", fileName_);
  }
  handleRequestPiecewise(request, response, r);
}
#endif

#warning "WFileResource::handleRequest need test"
awaitable<void> WFileResource::handleRequest(http::request &request, http::response &response)
{
#if defined(BOOST_ASIO_HAS_IO_URING) || defined(ASIO_HAS_IO_URING)
  auto executor = co_await asio::this_coro::executor;
  asio::random_access_file file(executor, fileName_, asio::random_access_file::read_only);

  ::uint64_t startByte = 0;
  ::uint64_t beyondLastByte = 0;
  if (startByte == 0) {
    /*
     * Initial request (not a continuation)
     */
    if (!file.is_open()) {
        response.status(404);
        co_return;
    } else
        response.status(200);

    /*
     * See if we should return a range.
     */
    std::istream::pos_type isize = file.size();

    http::request::ByteRangeSpecifier ranges = request.getRanges(isize);

    if (!ranges.isSatisfiable()) {
        std::ostringstream contentRange;
        contentRange << "bytes */" << isize;
        response.status(416); // Requested range not satisfiable
        response.addHeader("Content-Range", contentRange.str());
        co_return;
    }

    if (ranges.size() == 1) {
        response.status(206);
        startByte = ranges[0].firstByte();
        //beyondLastByte_ = std::streamsize(ranges[0].lastByte() + 1);
        beyondLastByte = ranges[0].lastByte() + 1;

        std::ostringstream contentRange;
        contentRange << "bytes " << startByte << "-"
                     << beyondLastByte - 1 << "/" << isize;
        response.addHeader("Content-Range", contentRange.str());
        response.length(beyondLastByte - startByte);
    } else {
        //beyondLastByte_ = std::streamsize(isize);
        beyondLastByte = ::uint64_t(isize);
        response.length(beyondLastByte);
    }

    response.setContentType(mimeType_);
  }

  auto totalsize = beyondLastByte - startByte;
  auto buffersize = bufferSize();

  std::vector<char> buffer(buffersize); // Buffer for reading data
  auto offset = startByte;


//  auto filesize = file.size();
//  response.length(filesize);

  if(totalsize > buffersize)
      response.chunked();

  while(offset < beyondLastByte)
  {
      auto [ec, nsize] = co_await file.async_read_some_at(offset, asio::buffer(buffer), use_nothrow_awaitable);

      if (ec == boost::asio::error::eof) {
          break; // End of file reached, stop reading
      } else if (ec) {
          throw boost::system::system_error{ec}; // Handle other errors
      }

      offset += nsize;
      // Send the data through HTTP response
      // std::string(buffer.data(), bytes_read);
      response.body().write(buffer.data(), nsize);
      if(totalsize > buffersize)
          co_await response.chunk_flush();
  }

  file.close();

#else
  std::ifstream r(fileName_.c_str(), std::ios::in | std::ios::binary);
  if (!r) {
      LOG_ERROR("Could not open file for reading: {}", fileName_);
  }
  co_await handleRequestPiecewise(request, response, r);
#endif


  co_return;
}

}
