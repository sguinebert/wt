/*
 * Copyright (C) 2007 Wim Dumon, Leuven, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "Wt/WMemoryResource.h"
#include "Wt/Http/Response.h"
#include <fstream>

namespace Wt {

WMemoryResource::WMemoryResource()
{ 
  create();
}

WMemoryResource::WMemoryResource(const std::string& mimeType)
  : mimeType_(mimeType),
    data_(new std::vector<unsigned char>())
{
  create();
}

WMemoryResource::WMemoryResource(std::string_view mimeType, const std::string& path) : mimeType_(mimeType)
{
  std::ifstream file(path, std::ios::binary);
  if (!file) {
      std::cerr << "Error: File not found or unable to open file." << std::endl;
      return;
  }
  data_ = std::make_shared<std::vector<unsigned char>>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

WMemoryResource::WMemoryResource(const std::string& mimeType,
				 const std::vector<unsigned char> &data)
  : mimeType_(mimeType),
    data_(new std::vector<unsigned char>(data))
{ 
  create();
}

void WMemoryResource::create()
{
#ifdef WT_THREADED
  dataMutex_.reset(new std::mutex());
#endif // WT_THREADED
}

WMemoryResource::~WMemoryResource()
{
  beingDeleted();
}

awaitable<void> WMemoryResource::setMimeType(const std::string& mimeType)
{
  mimeType_ = mimeType;

  co_await setChanged();
}

awaitable<void> WMemoryResource::setData(const std::vector<unsigned char>& data)
{
  {
#ifdef WT_THREADED
    std::unique_lock<std::mutex> lock(*dataMutex_);
#endif // WT_THREADED

    data_.reset(new std::vector<unsigned char>(data));
  }

  co_await setChanged();
}

awaitable<void> WMemoryResource::setData(const unsigned char *data, int count)
{
  {
#ifdef WT_THREADED
    std::unique_lock<std::mutex> l(*dataMutex_);
#endif
    data_.reset(new std::vector<unsigned char>(data, data + count));
  }

  co_await setChanged();
}

const std::vector<unsigned char> WMemoryResource::data() const
{
  DataPtr data;

  {
#ifdef WT_THREADED
    std::unique_lock<std::mutex> l(*dataMutex_);
#endif
    data = data_;
  }

  if (!data)
    return std::vector<unsigned char>();
  else
    return *data;
}
#ifdef DEPRECATED_OK
void WMemoryResource::handleRequest(const Http::Request& request,
				    Http::Response& response)
{
  DataPtr data;
  {
#ifdef WT_THREADED
    std::unique_lock<std::mutex> l(*dataMutex_);
#endif
    data = data_;
  }

  if (!data)
    return;

  response.setMimeType(mimeType_);

  for (unsigned int i = 0; i < (*data).size(); ++i)
    response.out().put((*data)[i]);
}
#endif

#warning "change the logic whith suspend if atomic_bool modifying data_"
awaitable<void> WMemoryResource::handleRequest(http::request &request, http::response &response)
{
  DataPtr data;
  {
#ifdef WT_THREADED
    std::unique_lock<std::mutex> l(*dataMutex_);
#endif
    data = data_;
  }

  if (!data)
    co_return;

  response.setContentType(mimeType_);

  for (unsigned int i = 0; i < (*data).size(); ++i)
    response.out().put((*data)[i]);
}

}
