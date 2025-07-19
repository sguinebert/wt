// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2007 Wim Dumon, Leuven, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WMEMORY_RESOURCE_H_
#define WMEMORY_RESOURCE_H_

#include <mutex>
#include <string>
#include <thread>

#include <Wt/WResource.h>

namespace Wt {

/*! \class WMemoryResource Wt/WMemoryResource.h Wt/WMemoryResource.h
 *  \brief A resource which streams data from memory
 *
 * Use this resource if you want to serve resource data from memory. This
 * is suitable for relatively small resources, which still require some
 * computation.
 *
 * If creating the data requires computation which you would like to
 * post-pone until the resource is served, then you may want to
 * directly reimplement WResource instead and compute the data on the
 * fly while streaming.
 *
 * Usage examples:
 * \code
 * auto imageResource = std::make_shared<Wt::WMemoryResource>("image/gif");
 *
 * static const unsigned char gifData[]
 *    = { 0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00,
 *        0x80, 0x00, 0x00, 0xdb, 0xdf, 0xef, 0x00, 0x00, 0x00, 0x21,
 *        0xf9, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00,
 *        0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x44,
 *        0x01, 0x00, 0x3b };
 *
 * imageResource->setData(gifData, 43);
 * auto image = std::make_unique<Wt::WImage>(Wt::WLink(imageResource), "1 transparent pixel");
 * \endcode
 *
 * \sa WFileResource.
 */
//template<bool immutable = true>
class WT_API WMemoryResource : public WResource
{
    typedef std::shared_ptr< const std::vector<unsigned char> > DataPtr;
public:
  /*! \brief Creates a new resource.
   *
   * You must call setMimeType() and setData() before using the resource.
   */
    WMemoryResource()
    {
        create();
    }

  /*! \brief Creates a new resource with given mime-type.
   *
   * You must call setData() before using the resource.
   */
  WMemoryResource(const std::string& mimeType)
      : mimeType_(mimeType),
      data_(new std::vector<unsigned char>())
  {
      create();
  }

  // WMemoryResource(std::string_view mimeType, const std::string& path) : mimeType_(mimeType)
  // {
  //     std::ifstream file(path, std::ios::binary);
  //     if (!file) {
  //         std::cerr << "Error: File not found or unable to open file." << std::endl;
  //         return;
  //     }
  //     data_ = std::make_shared<std::vector<unsigned char>>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  //     create();
  // }

  //WMemoryResource(std::string_view mimeType, DataPtr& data);


  /*! \brief Creates a new resource with given mime-type and data
   */
  WMemoryResource(const std::string& mimeType, const std::vector<unsigned char>& data)
      : mimeType_(mimeType),
      data_(new std::vector<unsigned char>(data))
  {
      create();
  }

  ~WMemoryResource()
  {
      beingDeleted();
  }

  /*! \brief Sets new data for the resource to serve.
   */
  awaitable<void> setData(const std::vector<unsigned char> &data) /*requires (!immutable)*/
  {
      {
#ifdef WT_THREADED
          std::unique_lock<std::mutex> lock(*dataMutex_);
#endif // WT_THREADED

          data_.reset(new std::vector<unsigned char>(data));
      }

      co_await setChanged();
  }

  /*! \brief Sets new data for the resource to serve.
   *
   * Sets the data from using the first \p count bytes from the
   * C-style \p data array.
   */
  awaitable<void> setData(const unsigned char *data, int count) /*requires (!immutable)*/
  {
      {
#ifdef WT_THREADED
          std::unique_lock<std::mutex> l(*dataMutex_);
#endif
          data_.reset(new std::vector<unsigned char>(data, data + count));
      }

      co_await setChanged();
  }

  /*! \brief Returns the data this resource will serve.
   */
  const std::vector<unsigned char> data() const
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

  /*! \brief Returns the mime-type.
   */
  const std::string mimeType() const { return mimeType_; }

  /*! \brief Sets the mime-type.
   */
  awaitable<void> setMimeType(const std::string& mimeType)
  {
      mimeType_ = mimeType;

      co_await setChanged();
  }
#ifdef DEPRECATED_OK
  virtual void handleRequest(const Http::Request& request, Http::Response& response) override;
#endif

#warning "change the logic whith suspend if atomic_bool modifying data_"
  virtual awaitable<void> handleRequest(http::request& request, http::response& response) override
  {
      //  DataPtr data;
      //  {
      //#ifdef WT_THREADED
      //    std::unique_lock<std::mutex> l(*dataMutex_);
      //#endif
      //    auto data = data_;
      //  }
#ifndef __EMSCRIPTEN__
      if (!data_)
          co_return;

      response.setContentType(mimeType_);

      if (!data_->empty()) {
          response.buffer().append(data_->data(), data_->data() + data_->size());
      }
#endif
      co_return;
  }

private:

  std::string mimeType_;
  DataPtr data_; //    typedef std::shared_ptr< const std::vector<unsigned char> > DataPtr;

  std::shared_ptr<std::mutex> dataMutex_;

  void create()
  {
#ifdef WT_THREADED
      dataMutex_.reset(new std::mutex());
#endif // WT_THREADED
  }
};

}

#endif // WMEMORY_RESOURCE_H_
