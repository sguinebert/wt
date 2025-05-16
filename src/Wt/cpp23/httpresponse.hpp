#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <array>
#include <string_view>

// 2) Generic constexpr_concat utility
// 1) Your generic concat helper
template <size_t N1, size_t N2>
consteval auto constexpr_concat(auto hdr, const char (&body)[N2]) {
  std::array<char, N1 + N2 - 1> arr{};
  for (size_t i = 0; i < N1 - 1; ++i)
    arr[i] = hdr[i];
  for (size_t i = 0; i < N2 - 1; ++i)
    arr[N1 - 1 + i] = static_cast<char>(body[i]);
  return arr;
}

// 2) A little machinery to build your header for any body size
template <std::size_t BodySize> struct HttpHeader {
// stringify the BodySize at compile time
    // Template function to convert std::size_t N to a constexpr character array
    static constexpr char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    /**
 * @struct to_string_t
 * @brief Provides the ability to convert any integral to a string at compile-time.
 * @tparam N Number to convert
 * @tparam base Desired base, can be from 2 to 36
 */
    template<std::size_t N, int base, typename char_type,
             std::enable_if_t<(base > 1 && base < sizeof(digits)), int> = 0>
    class to_string_t {

        consteval static auto buflen() noexcept {
            unsigned int len = N > 0 ? 1 : 2;
            for (auto n = N; n; len++, n /= base);
            return len;
        }

        char_type buf[buflen()] = {};

    public:
        /**
     * Constructs the object, filling `buf` with the string representation of N.
     */
        consteval to_string_t() noexcept {
            auto ptr = end();
            *--ptr = '\0';

            if (N != 0) {
                for (auto n = N; n; n /= base)
                    *--ptr = digits[(N < 0 ? -1 : 1) * (n % base)];
                if (N < 0)
                    *--ptr = '-';
            } else {
                buf[0] = '0';
            }
        }

        // Support implicit casting to `char *` or `const char *`.
        consteval operator char_type *() noexcept { return buf; }
        consteval operator const char_type *() const noexcept { return buf; }

        consteval auto size() const noexcept { return sizeof(buf) / sizeof(buf[0]); }

        // Element access
        consteval auto data() noexcept { return buf; }
        consteval auto data() const noexcept { return buf; }
        consteval auto& operator[](unsigned int i) noexcept { return buf[i]; }
        consteval const auto& operator[](unsigned int i) const noexcept { return buf[i]; }
        consteval auto& front() noexcept { return buf[0]; }
        consteval const auto& front() const noexcept { return buf[0]; }
        consteval auto& back() noexcept { return buf[size() - 1]; }
        consteval const auto& back() const noexcept { return buf[size() - 1]; }

        // Iterators
        consteval auto begin() noexcept { return buf; }
        consteval auto begin() const noexcept { return buf; }
        consteval auto end() noexcept { return buf + size(); }
        consteval auto end() const noexcept { return buf + size(); }
    };

    template <typename... Views>
    static consteval std::size_t total_size(const Views&... views) {
        return (views.size() + ...);
    }

    // Concatenate string views into a std::array at compile time
    template <std::size_t TotalSize, typename... Views>
    static consteval auto concat_string_views(const Views&... views) {
        std::array<char, TotalSize> result{};
        std::size_t pos = 0;
        auto copy = [&result, &pos](const auto& view) {
            for (char c : view) {
                result[pos++] = c;
            }
        };
        (copy(views), ...);
        return result;
    }

    static constexpr to_string_t<BodySize, 10, char> int_to_str_array;
    static constexpr std::string_view part1 = "HTTP/1.1 200 OK\r\nContent-Type: application/javascript; charset=utf-8\r\nContent-Length: ";
    static constexpr std::string_view content_length_view {int_to_str_array.data(), int_to_str_array.size() - 1}; // Exclude null terminator
    static constexpr std::string_view part2 = "\r\nCache-Control: public, max-age=31536000, immutable\r\n\r\n";

    // Calculate total size
    static constexpr std::size_t header_size = total_size(part1, content_length_view, part2);
    // Concatenate all parts into a single std::array
    static constexpr auto value = concat_string_views<header_size>(part1, content_length_view, part2);

    static constexpr size_t size = sizeof(value) - 1;
};

//template <std::size_t N> constexpr char HttpHeader<N>::value[];

// 3) The all-in-one ResponseBuilder
template <auto &Body> struct ResponseBuilder {
  static constexpr size_t body_size = sizeof(Body) - 1;
  using H = HttpHeader<body_size>;

  // build the full buffer (header + body)
  static constexpr auto buffer = constexpr_concat<H::header_size+1, body_size+1>(H::value, Body);

  // two views over that buffer
  static constexpr std::string_view full{buffer.data(), buffer.size()};
  static constexpr std::string_view body{buffer.data() + H::size,
                                         buffer.size() - H::size};
};

#endif // HTTPRESPONSE_HPP
