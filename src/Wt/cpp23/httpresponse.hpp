#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <array>
#include <string_view>

// static constexpr char http_headers[] = {
//     // HTTP/1.1 status line + headers
//     'H','T','T','P','/','1','.','1',' ','2','0','0',' ','O','K','\r','\n',
//     'C','o','n','t','e','n','t','-','T','y','p','e',':','
//     ','a','p','p','l','i','c','a','t','i','o','n','/','j','a','v','a','s','c','r','i','p','t',';','
//     ','c','h','a','r','s','e','t','=','u','t','f','-','8','\r','\n',
//     'C','a','c','h','e','-','C','o','n','t','r','o','l',':','
//     ','p','u','b','l','i','c',',','
//     ','m','a','x','-','a','g','e','=','3','1','5','3','6','0','0','0',',','
//     ','i','m','m','u','t','a','b','l','e','\r','\n',
//     '\r','\n'
// };

// 2) Generic constexpr_concat utility
// 1) Your generic concat helper
template <size_t N1, size_t N2>
consteval auto constexpr_concat(const char (&hdr)[N1], const char (&body)[N2]) {
  std::array<char, N1 + N2 - 1> arr{};
  for (size_t i = 0; i < N1 - 1; ++i)
    arr[i] = hdr[i];
  for (size_t i = 0; i < N2 - 1; ++i)
    arr[N1 - 1 + i] = static_cast<char>(body[i]);
  return arr;
}

// 2) A little machinery to build your header for any body size
template <size_t BodySize> struct HttpHeader {
// stringify the BodySize at compile time
#define STR(x) #x
#define TOSTR(x) STR(x)
  static constexpr char value[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/javascript; charset=utf-8\r\n"
      "Content-Length: " TOSTR(
          BodySize) "\r\n"
                    "Cache-Control: public, max-age=31536000, immutable\r\n"
                    "\r\n";
#undef STR
#undef TOSTR

  static constexpr size_t size = sizeof(value) - 1;
};

template <size_t N> constexpr char HttpHeader<N>::value[];

// 3) The all-in-one ResponseBuilder
template <auto &Body> struct ResponseBuilder {
  static constexpr size_t body_size = sizeof(Body) - 1;
  using H = HttpHeader<body_size>;

  // build the full buffer (header + body)
  static constexpr auto buffer = constexpr_concat(H::value, Body);

  // two views over that buffer
  static constexpr std::string_view full{buffer.data(), buffer.size()};
  static constexpr std::string_view body{buffer.data() + H::size,
                                         buffer.size() - H::size};
};

#endif // HTTPRESPONSE_HPP
