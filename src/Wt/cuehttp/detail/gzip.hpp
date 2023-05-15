/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef CUEHTTP_GZIP_HPP_
#define CUEHTTP_GZIP_HPP_

//#ifdef ENABLE_GZIP

#include <zlib.h>
#include <string>
#include <array>
#include <vector>

#include "noncopyable.hpp"
#include "common.hpp"

namespace Wt {
namespace http {
namespace detail {

static constexpr std::uint64_t threshold = 500000000; //2048;

struct gzip final : safe_noncopyable {
    static bool compress(std::string_view src, std::string& dst, int level = 8) {
        z_stream stream;
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;
        stream.avail_in = Z_NULL;
        stream.next_in = Z_NULL;
        constexpr int flag{15 + 16};
        if (deflateInit2(&stream, level, Z_DEFLATED, flag, 9, Z_DEFAULT_STRATEGY) != Z_OK) {
            return false;
        }

        stream.next_in = (unsigned char*)src.data();
        stream.avail_in = static_cast<unsigned int>(src.length());

        do {
            unsigned char temp[4096];
            stream.next_out = temp;
            stream.avail_out = sizeof(temp);
            const auto code = deflate(&stream, Z_FINISH);
            dst.append((char*)temp, static_cast<unsigned>(stream.next_out - temp));
            if (code == Z_STREAM_END) {
                break;
            }
        } while (stream.avail_out == 0);

        return deflateEnd(&stream) == Z_OK;
    }

    static bool compress(std::string_view src, std::vector<std::array<char, 4096> *>& buffers, int level = 8) {
        z_stream stream;
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;
        stream.avail_in = Z_NULL;
        stream.next_in = Z_NULL;
        constexpr int flag{15 + 16};
        if (deflateInit2(&stream, level, Z_DEFLATED, flag, 9, Z_DEFAULT_STRATEGY) != Z_OK) {
            return false;
        }

        stream.next_in = (unsigned char*)src.data();
        stream.avail_in = static_cast<unsigned int>(src.length());

        do {
            std::array<char, 4096> *dst = nullptr;// detail::pooler.malloc();
            buffers.push_back(dst);
            stream.next_out = (unsigned char*)dst->data();
            stream.avail_out = dst->size();
            const auto code = deflate(&stream, Z_FINISH);
            if (code == Z_STREAM_END) {
                break;
            }
        } while (stream.avail_out == 0);

        return deflateEnd(&stream) == Z_OK;
    }

    bool inflate(unsigned char* in, size_t size, unsigned char out[], bool& hasMore)
    {
        z_stream zInState_;
        //LOG_DEBUG("wthttp: ws: inflate frame");
        if (!hasMore) {
            zInState_.avail_in = size;
            zInState_.next_in = in;
        }
        hasMore = true;

        zInState_.avail_out = 16 * 1024;
        zInState_.next_out = out;
        int ret = ::inflate(&zInState_, Z_SYNC_FLUSH);

        switch(ret) {
        case Z_NEED_DICT:
            //LOG_ERROR("inflate : no dictionary found in frame");
            return false;
        case Z_DATA_ERROR:
            //LOG_ERROR("inflate : data error");
            return false;
        case Z_MEM_ERROR:
            //LOG_ERROR("inflate : memory error");
            return false;
        default:
            break;
        }

        //read_ += 16384 - zInState_.avail_out;
        //LOG_DEBUG("wthttp: ws: inflate - Size before {} size after {}", size, 16384 - zInState_.avail_out);

        if(zInState_.avail_out != 0)
            hasMore = false;

        return true;
    }
};

}  // namespace detail
}  // namespace http
}  // namespace cue

//#endif  // ENABLE_GZIP

#endif  // CUEHTTP_GZIP_HPP_
