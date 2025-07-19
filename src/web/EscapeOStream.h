// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WT_ESCAPE_OSTREAM_H_
#define WT_ESCAPE_OSTREAM_H_

#include <Wt/WStringStream.h>

#ifndef WT_DBO_ESCAPEOSTREAM
#define WT_ESCAPEOSTREAM_API WT_API
#else // WT_DBO_ESCAPEOSTREAM
#define WT_ESCAPEOSTREAM_API WTDBO_API
#endif // WT_DBO_ESCAPEOSTREAM

#include <array>
#include <bitset>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>

#ifdef _MSC_VER
#include <intrin.h>  // MSVC
#else
#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#include <immintrin.h>   // for __m512i, _mm512_set1_epi8, etc.
#else
// On non-x86 (e.g. WASM) we skip the SIMD/CPUID path
#endif

#endif

static inline bool hasAVX512() {
    int info[4] = {0};

#ifdef _MSC_VER
    __cpuidex(info, 7, 0);
#elif !defined(__EMSCRIPTEN__)
    __cpuid_count(7, 0, info[0], info[1], info[2], info[3]);
#else
    // Emscripten does not support __cpuid_count, so we skip this check
    return false;  // AVX512 is not supported in Emscripten
#endif

    return (info[1] & (1 << 16)) != 0;  // Check AVX-512F (bit 16 of EBX)
}
static inline bool hasAVX2() {
    int info[4] = {0};

#ifdef _MSC_VER
    __cpuidex(info, 7, 0);
#elif !defined(__EMSCRIPTEN__)
    __cpuid_count(7, 0, info[0], info[1], info[2], info[3]);
#else
    // Emscripten does not support __cpuid_count, so we skip this check
    return false;  // AVX512 is not supported in Emscripten
#endif

    return (info[1] & (1 << 5)) != 0;  // Check AVX2 (bit 5 of EBX)
}

// Struct to represent a character and its escaped form
struct Entry {
    char c;
    std::string_view s;
};

// Define the rule sets as constexpr arrays (matching the query)
constexpr std::array htmlAttributeEntries = {
    Entry{ '&', "&amp;" },
    Entry{ '\"', "&#34;" },
    Entry{ '<', "&lt;" }
};

constexpr std::array plainTextEntries = {
    Entry{ '&', "&amp;" },
    Entry{ '>', "&gt;" },
    Entry{ '<', "&lt;" }
};

constexpr std::array plainTextNewLinesEntries = {
    Entry{ '&', "&amp;" },
    Entry{ '>', "&gt;" },
    Entry{ '<', "&lt;" },
    Entry{ '\n', "<br />" }
};

constexpr std::array jsStringLiteralSQuoteEntries = {
    Entry{'\\', "\\\\"},
    Entry{'\n', "\\n"},
    Entry{'\r', "\\r"},
    Entry{'\t', "\\t"},
    Entry{'\'', "\\'"}
};

constexpr std::array jsStringLiteralDQuoteEntries = {
    Entry{'\\', "\\\\"},
    Entry{'\n', "\\n"},
    Entry{'\r', "\\r"},
    Entry{'\t', "\\t"},
    Entry{'"', "\\\""}
};

// Enum to identify rule sets
enum class RuleSet {
    HtmlAttribute,
    PlainText,
    PlainTextNewLines,
    JsStringLiteralSQuote,
    JsStringLiteralDQuote
};

// Helper to get entries for a given rule set
template <RuleSet RS>
constexpr auto getEntries() {
    if constexpr (RS == RuleSet::HtmlAttribute) return htmlAttributeEntries;
    else if constexpr (RS == RuleSet::PlainText) return plainTextEntries;
    else if constexpr (RS == RuleSet::PlainTextNewLines) return plainTextNewLinesEntries;
    else if constexpr (RS == RuleSet::JsStringLiteralSQuote) return jsStringLiteralSQuoteEntries;
    else if constexpr (RS == RuleSet::JsStringLiteralDQuote) return jsStringLiteralDQuoteEntries;
}

// Compute a bitset for a rule set at compile time
template <typename Entries>
constexpr auto computeBitset(const Entries& entries) {
    std::bitset<256> bs;
    for (const auto& e : entries) {
        bs.set(static_cast<unsigned char>(e.c));
    }
    return bs;
}

// Precomputed bitsets for each rule set
constexpr auto bitsetHtmlAttribute = computeBitset(htmlAttributeEntries);
constexpr auto bitsetPlainText = computeBitset(plainTextEntries);
constexpr auto bitsetPlainTextNewLines = computeBitset(plainTextNewLinesEntries);
constexpr auto bitsetJsStringLiteralSQuote = computeBitset(jsStringLiteralSQuoteEntries);
constexpr auto bitsetJsStringLiteralDQuote = computeBitset(jsStringLiteralDQuoteEntries);

// Helper to get bitset for a given rule set
template <RuleSet RS>
constexpr auto getBitset() {
    if constexpr (RS == RuleSet::HtmlAttribute) return bitsetHtmlAttribute;
    else if constexpr (RS == RuleSet::PlainText) return bitsetPlainText;
    else if constexpr (RS == RuleSet::PlainTextNewLines) return bitsetPlainTextNewLines;
    else if constexpr (RS == RuleSet::JsStringLiteralSQuote) return bitsetJsStringLiteralSQuote;
    else if constexpr (RS == RuleSet::JsStringLiteralDQuote) return bitsetJsStringLiteralDQuote;
}

// Escape function for a single rule set
// template <RuleSet RS, typename Out>
// void escapeString(const std::string& input, Out&& output) {
//     constexpr auto entries = getEntries<RS>();
//     constexpr auto special = getBitset<RS>();

//     for (char c : input) {
//         if (special[static_cast<unsigned char>(c)]) {
//             for (const auto& e : entries) {
//                 if (c == e.c) {
//                     output.append(e.s);
//                     break;
//                 }
//             }
//         } else {
//             output += c;
//         }
//     }
// }

#if defined(__AVX512F__) && defined(__AVX512VL__) && defined(__AVX512BW__)
template<RuleSet RS>
struct AVX512Optimized {
    static size_t escape(const char *in, size_t len, char *out) {
        const char *const finalin = in + len;
        const char *const initout = out;
        __m512i solidus = _mm512_set1_epi8('\\'); //prepare 64bytes (512bits) ['\', '\', etc...]
        __m512i quote = _mm512_set1_epi8('"'); //prepare 64bytes (512bits) ['"', '"', etc...]
        __m512i squote = _mm512_set1_epi8('\''); //prepare 64bytes (512bits) ['\'', '\'', etc...]

        __m512i newline = _mm512_set1_epi8('\n');   //  "newline"
        __m512i cr = _mm512_set1_epi8('\r'); //  "carriage_return"
        __m512i tab = _mm512_set1_epi8('\t');   // "tab"

        __m512i solidus16 = _mm512_set1_epi16('\\'); //prepare 64bytes (512bits) ['\', 0, '\', 0, etc...]
        for (; in + 32 <= finalin; in += 32) {
            __m256i input = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in)); //load first 32bytes of in string to 32bytes (256bits) simd register [char0, char1,...]
            __m512i input1 = _mm512_cvtepu8_epi16(input); // wide the input simd 32bytes (256bits) to a 64bytes (512bits) [char0, 0, char1, 0, ..., char31, 0]
            __m512i shifted_input1 = _mm512_bslli_epi128(input1, 1); //shift right the input 64bytes (512bits) [0, char0, 0, char1, 0, ..., 0, char31]

            __mmask64 is_solidus = _mm512_cmpeq_epi8_mask(input1, solidus);// Compare each byte in input1 to '\', creating a 64-bit mask (1 where '\', 0 elsewhere)
            __mmask64 is_quote;
            if constexpr(RS == RuleSet::JsStringLiteralDQuote)
                is_quote = _mm512_cmpeq_epi8_mask(input1, quote);// Compare each byte in input1 to '"', creating a 64-bit mask (1 where '"', 0 elsewhere)
            else
                is_quote = _mm512_cmpeq_epi8_mask(input1, squote);

            __mmask64 is_newline = _mm512_cmpeq_epi8_mask(input1, newline);
            __mmask64 is_cr = _mm512_cmpeq_epi8_mask(input1, cr);
            __mmask64 is_tab = _mm512_cmpeq_epi8_mask(input1, tab);
            // Combine masks
            __mmask64 special_mask = _kor_mask64(_kor_mask64(is_solidus, is_quote),
                                                 _kor_mask64(is_newline, _kor_mask64(is_cr, is_tab)));
            //__mmask64 is_quote_or_solidus = _kor_mask64(is_solidus, is_quote);// Combine masks with OR: 1 where byte is either '\', '"', or both, 0 elsewhere
            __mmask64 to_keep = _kor_mask64(special_mask, 0xaaaaaaaaaaaaaaaa);// OR with alternating 1s (every even bit set), keeping original bytes and escape positions
            __m512i escaped = _mm512_or_si512(shifted_input1, solidus16); // Insert potential escape chars: OR shifted input with solidus16, e.g., ['\', h, '\', e, ...]
            _mm512_mask_compressstoreu_epi8(out, to_keep, escaped);// Store only bytes where to_keep is 1, compressing escaped and original bytes into out
            out += _mm_popcnt_u64(_cvtmask64_u64(to_keep));// Advance out pointer by the count of 1s in to_keep (number of bytes written)
        }
        for (; in < finalin; in++) {
            if ((*in == '\\') || (*in == '"')) {
                *out = '\\';
                out++;
            }
            *out = *in;
            out++;
        }
        return out - initout;
    }
};
#endif

// Template struct to mix multiple rule sets
template <RuleSet... RS>
struct MixedRules {

    template <RuleSet T>
    static consteval auto contains() {
        return ((RS == T) || ...);
    }

    static consteval auto Htmlfirst() {
        if(!contains<RuleSet::JsStringLiteralSQuote>() && !contains<RuleSet::JsStringLiteralDQuote>())
            return true;
        constexpr RuleSet rules[] = { RS... };
        return rules[0] != RuleSet::JsStringLiteralSQuote && rules[0] != RuleSet::JsStringLiteralDQuote;
    }

    // Compute combined bitset by OR-ing individual bitsets
    static consteval std::bitset<256> computeMixedBitset() {
        std::bitset<256> bs;
        ([&] { bs |= getBitset<RS>(); }(), ...);
        return bs;
    }

    // Compute combined escape mappings (later rules override earlier ones)
    static consteval auto computeMixedEntries() {
        std::array<std::pair<char, std::string_view>, 256> mapping{};
        ([&] {
            for (const auto& e : getEntries<RS>()) {
                mapping[static_cast<unsigned char>(e.c)] = {e.c, std::string_view(e.s)};
            }
        }(), ...);
        return mapping;
    }

    static constexpr std::bitset<256> special = computeMixedBitset();
    static constexpr auto entries = computeMixedEntries();
    static constexpr auto htmlfirst = Htmlfirst();
    // alignas(32) static constexpr uint64_t lookup[4] = {
    //     special.to_ullong(),                     // Bits 0-63
    //     (special >> 64).to_ullong(),            // Bits 64-127
    //     (special >> 128).to_ullong(),           // Bits 128-191
    //     (special >> 192).to_ullong()            // Bits 192-255
    // };
    // Compute a 64-bit chunk of the bitset at compile time
    template<bool js512 = false>
    constexpr static uint64_t computeBitmaskChunk(size_t start, size_t end) {
        uint64_t mask = 0;
        std::bitset<256> bs = getBitset<RuleSet::JsStringLiteralSQuote>();
        bs |= getBitset<RuleSet::JsStringLiteralDQuote>();
        for (size_t i = start; i < end && i < 256; ++i) {
            if(js512 && bs.test(i)) {
                continue;
            }
            if (special.test(i)) {
                mask |= (1ULL << (i - start));
            }
        }
        return mask;
    }

    // Precompute the bitset as four 64-bit chunks
    alignas(32) static constexpr uint64_t lookup[4] = {
        computeBitmaskChunk(0, 64),    // Bits 0-63
        computeBitmaskChunk(64, 128),  // Bits 64-127
        computeBitmaskChunk(128, 192), // Bits 128-191
        computeBitmaskChunk(192, 256)  // Bits 192-255
    };
    alignas(32) static constexpr uint64_t lookup512[4] = {
        computeBitmaskChunk<true>(0, 64),    // Bits 0-63
        computeBitmaskChunk<true>(64, 128),  // Bits 64-127
        computeBitmaskChunk<true>(128, 192), // Bits 128-191
        computeBitmaskChunk<true>(192, 256)  // Bits 192-255
    };

    template<typename Out>
    static void escape(std::string_view input, Out& output) {
#if defined(__AVX512__)
        if(hasAVX512()) {
            if constexpr(htmlfirst) {
                output.resize(input.size() * 2);
                AVX2escape(input, output);
                Out fromoutput(std::move(output));
                input = std::string_view(fromoutput.data(), fromoutput.size());
            }

            const char* s = input.data();
            size_t len = input.size();
            size_t i = 0;

            std::size_t outputsize = 0;
            output.resize(input.size() * 2);
            if constexpr(contains<RuleSet::JsStringLiteralSQuote>())
            {
                using Escaper = AVX512Optimized<RuleSet::JsStringLiteralSQuote>;
                for (; i + 31 < len; i += 32) {
                    outputsize = Escaper::escape(s + i, 32, output.data() + outputsize);
                }
            }
            else if constexpr (contains<RuleSet::JsStringLiteralDQuote>())
            {
                using Escaper = AVX512Optimized<RuleSet::JsStringLiteralDQuote>;
                for (; i + 31 < len; i += 32) {
                    outputsize = Escaper::escape(s + i, 32, output.data() + outputsize);
                }
            }
            if constexpr ((!contains<RuleSet::HtmlAttribute>() && !contains<RuleSet::PlainText>() && !contains<RuleSet::PlainTextNewLines>()) || htmlfirst)
                return;

            Out fromoutput(std::move(output));
            input = std::string_view(fromoutput.data(), fromoutput.size());
        }
#endif
        output.resize(input.size() * 2);
        AVX2escape(input, output);
    }

    // Escape function using mixed rules
    // static void scalar_escape(const std::string& input, std::string& output) {
    //     for (char c : input) {
    //         if (special[static_cast<unsigned char>(c)]) {
    //             const auto& esc = entries[static_cast<unsigned char>(c)];
    //             if (esc.first != 0) { // Check if character is mapped
    //                 output.append(esc.second);
    //             } else {
    //                 output += c; // Fallback (shouldn’t happen with proper rules)
    //             }
    //         } else {
    //             output += c;
    //         }
    //     }
    // }

    // AVX2-accelerated escaping (max 32 bytes per iteration)
    template<typename OUT>
    static void AVX2escape(std::string_view input, OUT& output) {
        const char* s = input.data();
        size_t len = input.size();
        size_t i = 0;

// Precompute lookup table at runtime (once)
// alignas(32) static uint8_t lookup[256] = {};
// static bool initialized = false;
// if (!initialized) {
//     for (size_t j = 0; j < 256; ++j) {
//         lookup[j] = special.test(j) ? 0xFF : 0x00;
//     }
//     initialized = true;
// }
// #if defined(__AVX512__)
//     if(hasAVX512()) {
//         std::size_t outputsize = 0;
//         output.resize(input.size() * 2);
//         if constexpr(contains<RuleSet::JsStringLiteralSQuote>())
//         {
//             auto escaper = AVX512Optimized<RuleSet::JsStringLiteralSQuote>();
//             for (; i + 31 < len; i += 32) {
//                 outputsize = escaper.escape(s + i, 32, output.data() + outputsize);
//             }
//         }
//         else if constexpr (contains<RuleSet::JsStringLiteralDQuote>())
//         {
//             auto escaper = AVX512Optimized<RuleSet::JsStringLiteralDQuote>();
//             for (; i + 31 < len; i += 32) {
//                 outputsize = escaper.escape(s + i, 32, output.data() + outputsize);
//             }
//         }
//     }
// #endif
#if defined(__AVX2__)
        if(!hasAVX2()) {
            // AVX2 loop: 32 bytes at a time
            for (; i + 31 < len; i += 32) {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s + i));
                // Use chunk as indices into the lookup table
                __m256i lookupVec = _mm256_shuffle_epi8(
                    _mm256_loadu_si256(reinterpret_cast<const __m256i*>(hasAVX512() ? lookup512 : lookup)),
                    _mm256_and_si256(chunk, _mm256_set1_epi8(0xFF)) // Mask to 0-255
                    );
                uint32_t mask = _mm256_movemask_epi8(lookupVec);

                // If no bytes need escaping, append the whole chunk
                if (mask == 0) {
                    output.append(s + i, 32);
                } else {
                    // Process the mask using std::countr_zero
                    size_t current = i; // Tracks the start of the current non-special run
                    while (mask != 0) {
                        // Find the position of the next set bit
                        int pos = std::countr_zero(mask);

                        // Append non-special bytes before the special byte
                        if (current < i + pos) {
                            output.append(s + current, i + pos - current);
                        }

                        // Escape the special byte at i + pos
                        char c = s[i + pos];
                        const auto& e = entries[static_cast<unsigned char>(c)];
                        if (e.first != 0) { // Assuming e.first == 0 means no escaping needed
                            output.append(e.second);
                        }

                        // Move past the processed byte
                        current = i + pos + 1;

                        // Clear the least significant set bit
                        mask &= (mask - 1);
                    }

                    // Append any remaining non-special bytes in the chunk
                    if (current < i + 32) {
                        output.append(s + current, i + 32 - current);
                    }
                }
            }
        }
#else
#warning "AVX2 not available or compiler flag not present -> with CMAKE : add set(CMAKE_CXX_FLAGS \"${CMAKE_CXX_FLAGS} -march=native\")"
#endif

        // Scalar fallback for remaining bytes
        for (; i < len; ++i) {
            char c = s[i];
            if (special[static_cast<unsigned char>(c)]) {
                const auto& e = entries[static_cast<unsigned char>(c)];
                if (e.first != 0) output.append(e.second);
            } else {
                output += c;
            }
        }
        output.push_back('\0'); // Null-terminate the output string
        output.resize(output.size() + 1); // Resize to fit the null terminator

        //std::cerr << "escape character: " << output << std::endl;
    }
};


namespace Wt {

#ifdef WT_DBO_ESCAPEOSTREAM
namespace Dbo {
#endif

class WT_ESCAPEOSTREAM_API EscapeOStream
{
public:
  enum RuleSet { Empty = 0, HtmlAttribute = 1,
		 JsStringLiteralSQuote = 2, JsStringLiteralDQuote = 3, 
                 Plain = 4, PlainTextNewLines = 5 };

  EscapeOStream();
  EscapeOStream(std::ostream& sink);
  EscapeOStream(WStringStream& sink);
  EscapeOStream(EscapeOStream& other);

  EscapeOStream(EscapeOStream&& other) noexcept = default; //will suppress the final version
  EscapeOStream& operator=(EscapeOStream&& other) noexcept { stream_ = other.stream_; return *this; } //will suppress the final version

  void pushEscape(RuleSet rules);
  void popEscape();

#ifdef WT_TARGET_JAVA
  EscapeOStream& push();
#endif // WT_TARGET_JAVA

  void append(const std::string& s, const EscapeOStream& rules);
  void append(std::string_view s, const EscapeOStream& rules);
  void append(const char *s, std::size_t len);

  EscapeOStream& operator<< (char);


  EscapeOStream& operator<< (const std::string& s);
  EscapeOStream& operator<< (std::string_view s);

  EscapeOStream& operator<< (int);
  EscapeOStream& operator<< (unsigned int);
  EscapeOStream& operator<< (long long);
  EscapeOStream& operator<< (bool);
  EscapeOStream& operator<< (const EscapeOStream& other);

  const char *c_str(); // for default constructor, can return 0
  std::string str() const; // for default constructor

  bool empty() const;
  void clear();

private:
  WStringStream own_stream_;
  WStringStream& stream_;

  struct Entry {
    char c;
    std::string s;
  };
  std::vector<Entry> mixed_;
  std::string special_;
  const char *c_special_;

  void mixRules();
  void put(const char *s, const EscapeOStream& rules);

  void sAppend(char c);
  void sAppend(const char *s, int length);
  void sAppend(const std::string& s);

  std::vector<RuleSet> ruleSets_;

  static const std::vector<Entry> standardSets_[6];
  static const std::string standardSetsSpecial_[6];

  static const Entry htmlAttributeEntries_[3];
  static const Entry jsStringLiteralSQuoteEntries_[5];
  static const Entry jsStringLiteralDQuoteEntries_[5];
  static const Entry plainTextEntries_[3];
  static const Entry plainTextNewLinesEntries_[4];
};

#ifdef WT_DBO_ESCAPEOSTREAM
} // namespace Dbo
#endif

} // namespace Wt

#endif // ESCAPE_OSTREAM_H_
