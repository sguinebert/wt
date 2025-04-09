/*
 * Copyright (C) 2007 Wim Dumon, Leuven, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "Wt/WMemoryResource.h"
// #include "Wt/Http/Response.h"
// #include <fstream>

namespace Wt {




















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

}
