#pragma once
// ============================================================================
// ASON — Array-Schema Object Notation  (C++17 header-only, SIMD-accelerated)
//
// API:
//   ason::encode(object)           -> std::string       (serialize single struct)
//   ason::encode(vector)           -> std::string       (serialize vector)
//   ason::encode_typed(object)     -> std::string       (serialize with type annotations)
//   ason::encode_typed(vector)     -> std::string       (serialize vector typed)
//   ason::decode<T>(str)           -> T                 (deserialize, auto-detects single/{vec})
//   ason::encode_bin(object)       -> std::string       (binary serialize)
//   ason::decode_bin<T>(str)       -> T                 (binary deserialize)
//
// Reflection macro:
//   ASON_FIELDS(StructName, (field1, "name1", "type1"), (field2, "name2", "type2"), ...)
//
// All parsing is zero-copy (string_view) where possible.
// Uses SIMD (NEON / SSE2) for string scanning and escaping.
// ============================================================================

#ifndef ASON_HPP
#define ASON_HPP

#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <unordered_map>
#include <type_traits>
#include <stdexcept>
#include <array>
#include <tuple>
#include <charconv>
#include <algorithm>

// SIMD headers
#if defined(__aarch64__) || defined(_M_ARM64)
  #include <arm_neon.h>
  #define ASON_NEON 1
#elif defined(__x86_64__) || defined(_M_X64)
  #include <immintrin.h>
  #define ASON_SSE2 1
#endif

namespace ason {

// ============================================================================
// Error
// ============================================================================
class Error : public std::runtime_error {
public:
    int pos;
    explicit Error(const std::string& msg, int p = -1)
        : std::runtime_error(msg), pos(p) {}
};

// ============================================================================
// SIMD utilities
// ============================================================================
namespace simd {

static constexpr int LANES = 16;

#if defined(ASON_NEON)

inline uint16_t movemask(uint8x16_t v) {
    uint16x8_t high_bits = vreinterpretq_u16_u8(vshrq_n_u8(v, 7));
    uint32x4_t paired16  = vreinterpretq_u32_u16(vsraq_n_u16(high_bits, high_bits, 7));
    uint64x2_t paired32  = vreinterpretq_u64_u32(vsraq_n_u32(paired16, paired16, 14));
    uint8x16_t paired64  = vreinterpretq_u8_u64(vsraq_n_u64(paired32, paired32, 28));
    uint16_t lo = vgetq_lane_u8(paired64, 0);
    uint16_t hi = vgetq_lane_u8(paired64, 8);
    return lo | (hi << 8);
}

// Find first byte in [ptr, ptr+len) matching any of: quote("), backslash(\), or control (<0x20)
// Returns offset or len if none found.
inline size_t find_quote_or_special(const uint8_t* ptr, size_t len) {
    size_t i = 0;
    uint8x16_t vquote = vdupq_n_u8('"');
    uint8x16_t vbslash = vdupq_n_u8('\\');
    uint8x16_t vctrl = vdupq_n_u8(0x1F);
    for (; i + LANES <= len; i += LANES) {
        uint8x16_t chunk = vld1q_u8(ptr + i);
        uint8x16_t eq_q = vceqq_u8(chunk, vquote);
        uint8x16_t eq_b = vceqq_u8(chunk, vbslash);
        uint8x16_t le_c = vcleq_u8(chunk, vctrl);
        uint8x16_t any  = vorrq_u8(vorrq_u8(eq_q, eq_b), le_c);
        uint16_t mask = movemask(any);
        if (mask) return i + __builtin_ctz(mask);
    }
    for (; i < len; i++) {
        uint8_t b = ptr[i];
        if (b == '"' || b == '\\' || b < 0x20) return i;
    }
    return len;
}

// Check if any byte in s needs quoting (control chars, structural chars)
inline bool has_special_chars(const uint8_t* ptr, size_t len) {
    // Structural: , ( ) [ ] " \ and control < 0x20
    size_t i = 0;
    uint8x16_t vcomma = vdupq_n_u8(',');
    uint8x16_t vlp    = vdupq_n_u8('(');
    uint8x16_t vrp    = vdupq_n_u8(')');
    uint8x16_t vlb    = vdupq_n_u8('[');
    uint8x16_t vrb    = vdupq_n_u8(']');
    uint8x16_t vquote = vdupq_n_u8('"');
    uint8x16_t vbslash= vdupq_n_u8('\\');
    uint8x16_t vctrl  = vdupq_n_u8(0x1F);
    for (; i + LANES <= len; i += LANES) {
        uint8x16_t chunk = vld1q_u8(ptr + i);
        uint8x16_t r = vcleq_u8(chunk, vctrl);
        r = vorrq_u8(r, vceqq_u8(chunk, vcomma));
        r = vorrq_u8(r, vceqq_u8(chunk, vlp));
        r = vorrq_u8(r, vceqq_u8(chunk, vrp));
        r = vorrq_u8(r, vceqq_u8(chunk, vlb));
        r = vorrq_u8(r, vceqq_u8(chunk, vrb));
        r = vorrq_u8(r, vceqq_u8(chunk, vquote));
        r = vorrq_u8(r, vceqq_u8(chunk, vbslash));
        if (movemask(r)) return true;
    }
    for (; i < len; i++) {
        uint8_t b = ptr[i];
        if (b < 0x20 || b == ',' || b == '(' || b == ')' ||
            b == '[' || b == ']' || b == '"' || b == '\\')
            return true;
    }
    return false;
}

#elif defined(ASON_SSE2)

inline size_t find_quote_or_special(const uint8_t* ptr, size_t len) {
    size_t i = 0;
    __m128i vquote = _mm_set1_epi8('"');
    __m128i vbslash = _mm_set1_epi8('\\');
    __m128i vctrl = _mm_set1_epi8(0x1F);
    for (; i + LANES <= len; i += LANES) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr + i));
        __m128i eq_q = _mm_cmpeq_epi8(chunk, vquote);
        __m128i eq_b = _mm_cmpeq_epi8(chunk, vbslash);
        // cmple unsigned: max(chunk, vctrl)==vctrl
        __m128i mx = _mm_max_epu8(chunk, vctrl);
        __m128i le_c = _mm_cmpeq_epi8(mx, vctrl);
        __m128i any = _mm_or_si128(_mm_or_si128(eq_q, eq_b), le_c);
        int mask = _mm_movemask_epi8(any);
        if (mask) return i + __builtin_ctz(mask);
    }
    for (; i < len; i++) {
        uint8_t b = ptr[i];
        if (b == '"' || b == '\\' || b < 0x20) return i;
    }
    return len;
}

inline bool has_special_chars(const uint8_t* ptr, size_t len) {
    size_t i = 0;
    __m128i vcomma  = _mm_set1_epi8(',');
    __m128i vlp     = _mm_set1_epi8('(');
    __m128i vrp     = _mm_set1_epi8(')');
    __m128i vlb     = _mm_set1_epi8('[');
    __m128i vrb     = _mm_set1_epi8(']');
    __m128i vquote  = _mm_set1_epi8('"');
    __m128i vbslash = _mm_set1_epi8('\\');
    __m128i vctrl   = _mm_set1_epi8(0x1F);
    for (; i + LANES <= len; i += LANES) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr + i));
        __m128i mx = _mm_max_epu8(chunk, vctrl);
        __m128i r = _mm_cmpeq_epi8(mx, vctrl);
        r = _mm_or_si128(r, _mm_cmpeq_epi8(chunk, vcomma));
        r = _mm_or_si128(r, _mm_cmpeq_epi8(chunk, vlp));
        r = _mm_or_si128(r, _mm_cmpeq_epi8(chunk, vrp));
        r = _mm_or_si128(r, _mm_cmpeq_epi8(chunk, vlb));
        r = _mm_or_si128(r, _mm_cmpeq_epi8(chunk, vrb));
        r = _mm_or_si128(r, _mm_cmpeq_epi8(chunk, vquote));
        r = _mm_or_si128(r, _mm_cmpeq_epi8(chunk, vbslash));
        if (_mm_movemask_epi8(r)) return true;
    }
    for (; i < len; i++) {
        uint8_t b = ptr[i];
        if (b < 0x20 || b == ',' || b == '(' || b == ')' ||
            b == '[' || b == ']' || b == '"' || b == '\\')
            return true;
    }
    return false;
}

#else // scalar fallback

inline size_t find_quote_or_special(const uint8_t* ptr, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t b = ptr[i];
        if (b == '"' || b == '\\' || b < 0x20) return i;
    }
    return len;
}

inline bool has_special_chars(const uint8_t* ptr, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t b = ptr[i];
        if (b < 0x20 || b == ',' || b == '(' || b == ')' ||
            b == '[' || b == ']' || b == '"' || b == '\\')
            return true;
    }
    return false;
}

#endif

} // namespace simd

// ============================================================================
// Fast integer formatting (itoa)
// ============================================================================
namespace detail {

static constexpr char DEC_DIGITS[201] =
    "0001020304050607080910111213141516171819"
    "2021222324252627282930313233343536373839"
    "4041424344454647484950515253545556575859"
    "6061626364656667686970717273747576777879"
    "8081828384858687888990919293949596979899";

inline void append_u64(std::string& buf, uint64_t v) {
    if (v < 10) { buf.push_back('0' + static_cast<char>(v)); return; }
    if (v < 100) {
        auto idx = v * 2;
        buf.push_back(DEC_DIGITS[idx]);
        buf.push_back(DEC_DIGITS[idx + 1]);
        return;
    }
    char tmp[20];
    int i = 20;
    while (v >= 100) {
        auto rem = v % 100;
        v /= 100;
        i -= 2;
        tmp[i]     = DEC_DIGITS[rem * 2];
        tmp[i + 1] = DEC_DIGITS[rem * 2 + 1];
    }
    if (v >= 10) {
        auto idx = v * 2;
        i -= 2;
        tmp[i]     = DEC_DIGITS[idx];
        tmp[i + 1] = DEC_DIGITS[idx + 1];
    } else {
        i--;
        tmp[i] = '0' + static_cast<char>(v);
    }
    buf.append(tmp + i, 20 - i);
}

inline void append_i64(std::string& buf, int64_t v) {
    if (v < 0) {
        buf.push_back('-');
        append_u64(buf, static_cast<uint64_t>(-v));
    } else {
        append_u64(buf, static_cast<uint64_t>(v));
    }
}

inline void append_f64(std::string& buf, double v) {
    if (std::isnan(v) || std::isinf(v)) { buf.push_back('0'); return; }
    // Integer-valued float
    double intpart;
    double frac = std::modf(v, &intpart);
    if (frac == 0.0) {
        auto iv = static_cast<int64_t>(v);
        if (static_cast<double>(iv) == v) {
            append_i64(buf, iv);
            buf.append(".0", 2);
            return;
        }
    }
    // Fast path: 1 decimal place
    double v10 = v * 10.0;
    double frac10 = v10 - std::trunc(v10);
    if (frac10 == 0.0 && std::abs(v10) < 1e18) {
        auto vi = static_cast<int64_t>(v10);
        if (vi < 0) { buf.push_back('-'); vi = -vi; }
        auto ip = static_cast<uint64_t>(vi) / 10;
        auto fp = static_cast<uint8_t>(static_cast<uint64_t>(vi) % 10);
        append_u64(buf, ip);
        buf.push_back('.');
        buf.push_back('0' + fp);
        return;
    }
    // Fast path: 2 decimal places
    double v100 = v * 100.0;
    double frac100 = v100 - std::trunc(v100);
    if (frac100 == 0.0 && std::abs(v100) < 1e18) {
        auto vi = static_cast<int64_t>(v100);
        if (vi < 0) { buf.push_back('-'); vi = -vi; }
        auto ip = static_cast<uint64_t>(vi) / 100;
        auto fi = static_cast<size_t>(static_cast<uint64_t>(vi) % 100);
        append_u64(buf, ip);
        buf.push_back('.');
        buf.push_back(DEC_DIGITS[fi * 2]);
        char d2 = DEC_DIGITS[fi * 2 + 1];
        if (d2 != '0') buf.push_back(d2);
        return;
    }
    // General: use std::to_chars (faster than snprintf)
    char tmp[32];
    auto [ptr, ec] = std::to_chars(tmp, tmp + sizeof(tmp), v);
    if (ec == std::errc()) {
        std::string_view sv(tmp, ptr - tmp);
        buf.append(sv.data(), sv.size());
        // Ensure decimal point for float identity
        if (sv.find('.') == std::string_view::npos && sv.find('e') == std::string_view::npos) {
            buf.append(".0", 2);
        }
    } else {
        int n = std::snprintf(tmp, sizeof(tmp), "%.17g", v);
        buf.append(tmp, n);
    }
}

// ============================================================================
// String quoting / escaping helpers
// ============================================================================

inline bool string_needs_quoting(std::string_view s) {
    if (s.empty()) return true;
    if (s.front() == ' ' || s.back() == ' ') return true;
    if (s == "true" || s == "false") return true;

    auto ptr = reinterpret_cast<const uint8_t*>(s.data());
    auto len = s.size();

    bool has_special = simd::has_special_chars(ptr, len);
    if (has_special) return true;

    // Check if it looks like a number -> must quote
    size_t start = 0;
    if (len > 0 && s[0] == '-') start = 1;
    if (start < len) {
        bool could_be_number = true;
        for (size_t i = start; i < len; i++) {
            char c = s[i];
            if (!((c >= '0' && c <= '9') || c == '.')) { could_be_number = false; break; }
        }
        if (could_be_number && len > start) return true;
    }
    return false;
}

inline void append_escaped(std::string& buf, std::string_view s) {
    buf.push_back('"');
    auto ptr = reinterpret_cast<const uint8_t*>(s.data());
    auto len = s.size();
    size_t start = 0;
    while (start < len) {
        size_t pos = simd::find_quote_or_special(ptr + start, len - start);
        buf.append(s.data() + start, pos);
        start += pos;
        if (start >= len) break;
        uint8_t b = ptr[start];
        switch (b) {
            case '"':  buf.append("\\\"", 2); break;
            case '\\': buf.append("\\\\", 2); break;
            case '\n': buf.append("\\n", 2); break;
            case '\t': buf.append("\\t", 2); break;
            default:   buf.push_back(b); break;  // other control chars pass through
        }
        start++;
    }
    buf.push_back('"');
}

inline void append_str(std::string& buf, std::string_view s) {
    if (string_needs_quoting(s)) {
        append_escaped(buf, s);
    } else {
        buf.append(s.data(), s.size());
    }
}

// ============================================================================
// Escape table for serialization
// ============================================================================

static constexpr uint8_t ESCAPE_CHAR[256] = {
    0,0,0,0,0,0,0,0,0,'t','n',0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,'"',0,0,0,0,0,0,0,0,0,0,0,0,0,   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,     0,0,0,0,0,0,0,0,0,0,0,0,'\\',0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

} // namespace detail

// ============================================================================
// Type traits for ASON field reflection
// ============================================================================

// Primary trait: specialised via ASON_FIELDS macro
template <typename T>
struct AsonFields {
    static constexpr bool defined = false;
};

// ============================================================================
// Type-name helpers for typed schema annotation
// ============================================================================
namespace detail {

template <typename T> struct TypeName { static constexpr const char* value = nullptr; };
template <> struct TypeName<bool>           { static constexpr const char* value = "bool"; };
template <> struct TypeName<int8_t>         { static constexpr const char* value = "int"; };
template <> struct TypeName<int16_t>        { static constexpr const char* value = "int"; };
template <> struct TypeName<int32_t>        { static constexpr const char* value = "int"; };
template <> struct TypeName<int64_t>        { static constexpr const char* value = "int"; };
template <> struct TypeName<uint8_t>        { static constexpr const char* value = "int"; };
template <> struct TypeName<uint16_t>       { static constexpr const char* value = "int"; };
template <> struct TypeName<uint32_t>       { static constexpr const char* value = "int"; };
template <> struct TypeName<uint64_t>       { static constexpr const char* value = "int"; };
template <> struct TypeName<float>          { static constexpr const char* value = "float"; };
template <> struct TypeName<double>         { static constexpr const char* value = "float"; };
template <> struct TypeName<std::string>    { static constexpr const char* value = "str"; };
template <> struct TypeName<std::string_view>{ static constexpr const char* value = "str"; };
template <> struct TypeName<char>           { static constexpr const char* value = "str"; };

} // namespace detail

// ============================================================================
// Forward declarations (SFINAE-constrained for struct types)
// ============================================================================

template <typename T>
std::enable_if_t<AsonFields<T>::defined, void>
dump_value(std::string& buf, const T& v);

template <typename T>
std::enable_if_t<AsonFields<T>::defined, void>
load_value(const char*& pos, const char* end, T& out);

template <typename T>
std::enable_if_t<AsonFields<T>::defined, void>
dump_bin_value(std::string& buf, const T& v);

template <typename T>
std::enable_if_t<AsonFields<T>::defined, void>
load_bin_value(const char*& pos, const char* end, T& out);

// ============================================================================
// dump_value specializations
// ============================================================================

inline void dump_value(std::string& buf, bool v) {
    buf.append(v ? "true" : "false");
}
inline void dump_value(std::string& buf, int8_t v) { detail::append_i64(buf, v); }
inline void dump_value(std::string& buf, int16_t v) { detail::append_i64(buf, v); }
inline void dump_value(std::string& buf, int32_t v) { detail::append_i64(buf, v); }
inline void dump_value(std::string& buf, int64_t v) { detail::append_i64(buf, v); }
inline void dump_value(std::string& buf, uint8_t v) { detail::append_u64(buf, v); }
inline void dump_value(std::string& buf, uint16_t v) { detail::append_u64(buf, v); }
inline void dump_value(std::string& buf, uint32_t v) { detail::append_u64(buf, v); }
inline void dump_value(std::string& buf, uint64_t v) { detail::append_u64(buf, v); }
inline void dump_value(std::string& buf, float v) { detail::append_f64(buf, v); }
inline void dump_value(std::string& buf, double v) { detail::append_f64(buf, v); }
inline void dump_value(std::string& buf, char v) { detail::append_str(buf, std::string_view(&v, 1)); }

inline void dump_value(std::string& buf, const std::string& v) {
    detail::append_str(buf, v);
}
inline void dump_value(std::string& buf, std::string_view v) {
    detail::append_str(buf, v);
}
inline void dump_value(std::string& buf, const char* v) {
    detail::append_str(buf, std::string_view(v));
}

template <typename T>
void dump_value(std::string& buf, const std::vector<T>& v) {
    buf.push_back('[');
    for (size_t i = 0; i < v.size(); i++) {
        if (i > 0) buf.push_back(',');
        dump_value(buf, v[i]);
    }
    buf.push_back(']');
}

template <typename T>
void dump_value(std::string& buf, const std::optional<T>& v) {
    if (v.has_value()) {
        dump_value(buf, *v);
    }
    // else: empty (null)
}

template <typename K, typename V>
void dump_value(std::string& buf, const std::unordered_map<K, V>& m) {
    buf.push_back('[');
    bool first = true;
    for (auto& [k, v] : m) {
        if (!first) buf.push_back(',');
        first = false;
        buf.push_back('(');
        dump_value(buf, k);
        buf.push_back(',');
        dump_value(buf, v);
        buf.push_back(')');
    }
    buf.push_back(']');
}

// Struct dump — requires AsonFields specialization
template <typename T>
std::enable_if_t<AsonFields<T>::defined, void>
dump_value(std::string& buf, const T& v) {
    buf.push_back('(');
    AsonFields<T>::dump_fields(buf, v);
    buf.push_back(')');
}

// ============================================================================
// Parser helpers (zero-copy)
// ============================================================================
namespace detail {

inline void skip_whitespace(const char*& pos, const char* end) {
    while (pos < end) {
        char c = *pos;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { pos++; continue; }
        break;
    }
}

inline void skip_whitespace_and_comments(const char*& pos, const char* end) {
    for (;;) {
        skip_whitespace(pos, end);
        if (pos + 1 < end && pos[0] == '/' && pos[1] == '*') {
            pos += 2;
            while (pos + 1 < end) {
                if (pos[0] == '*' && pos[1] == '/') { pos += 2; break; }
                pos++;
            }
        } else {
            return;
        }
    }
}

inline bool at_value_end(const char* pos, const char* end) {
    if (pos >= end) return true;
    char c = *pos;
    return c == ',' || c == ')' || c == ']';
}

inline bool is_delim(char c) {
    return c == ',' || c == ')' || c == ']' || c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Parse quoted string — handles escape sequences; zero-copy when no escapes
inline std::string parse_quoted_string(const char*& pos, const char* end) {
    pos++; // skip opening "
    const char* start = pos;

    // Fast scan for closing quote without escapes (SIMD-accelerated)
    auto ptr = reinterpret_cast<const uint8_t*>(pos);
    size_t remaining = end - pos;
    size_t offset = simd::find_quote_or_special(ptr, remaining);
    const char* scan = pos + offset;

    if (scan < end && *scan == '"') {
        // No escapes — direct construction (skip ArrayList build)
        pos = scan + 1;
        return std::string(start, static_cast<size_t>(scan - start));
    }

    // Slow path: has escapes
    std::string result;
    if (scan > start) result.append(start, scan - start);
    pos = scan;

    while (pos < end) {
        char b = *pos;
        if (b == '"') { pos++; return result; }
        if (b == '\\') {
            pos++;
            if (pos >= end) throw Error("unclosed string");
            char esc = *pos; pos++;
            switch (esc) {
                case '"':  result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case 'n':  result.push_back('\n'); break;
                case 't':  result.push_back('\t'); break;
                case ',':  result.push_back(','); break;
                case '(':  result.push_back('('); break;
                case ')':  result.push_back(')'); break;
                case '[':  result.push_back('['); break;
                case ']':  result.push_back(']'); break;
                case 'u': {
                    if (pos + 4 > end) throw Error("invalid unicode escape");
                    char hex[5] = {pos[0], pos[1], pos[2], pos[3], 0};
                    unsigned long cp = std::strtoul(hex, nullptr, 16);
                    // Simple UTF-8 encode
                    if (cp < 0x80) {
                        result.push_back(static_cast<char>(cp));
                    } else if (cp < 0x800) {
                        result.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                        result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    } else {
                        result.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                        result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                        result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    }
                    pos += 4;
                    break;
                }
                default: throw Error(std::string("invalid escape: \\") + esc);
            }
        } else {
            result.push_back(b);
            pos++;
        }
    }
    throw Error("unclosed string");
}

// Parse plain (unquoted) value — returns string_view into original buffer
inline std::string parse_plain_value(const char*& pos, const char* end) {
    const char* start = pos;
    bool has_escape = false;
    while (pos < end) {
        char b = *pos;
        if (b == ',' || b == ')' || b == ']') break;
        if (b == '\\') { has_escape = true; pos += 2; continue; }
        pos++;
    }
    const char* vend = pos;
    // Trim trailing whitespace
    while (vend > start && (vend[-1] == ' ' || vend[-1] == '\t')) vend--;
    // Trim leading whitespace
    while (start < vend && (*start == ' ' || *start == '\t')) start++;

    if (!has_escape) {
        return std::string(start, vend - start);
    }
    // Unescape
    std::string result;
    result.reserve(vend - start);
    for (const char* p = start; p < vend; ) {
        if (*p == '\\') {
            p++;
            if (p >= vend) throw Error("unexpected EOF in escape");
            switch (*p) {
                case ',': result.push_back(','); break;
                case '(': result.push_back('('); break;
                case ')': result.push_back(')'); break;
                case '[': result.push_back('['); break;
                case ']': result.push_back(']'); break;
                case '"': result.push_back('"'); break;
                case '\\':result.push_back('\\'); break;
                case 'n': result.push_back('\n'); break;
                case 't': result.push_back('\t'); break;
                case 'u': {
                    if (p + 4 >= vend) throw Error("invalid unicode escape");
                    char hex[5] = {p[1], p[2], p[3], p[4], 0};
                    unsigned long cp = std::strtoul(hex, nullptr, 16);
                    if (cp < 0x80) {
                        result.push_back(static_cast<char>(cp));
                    } else if (cp < 0x800) {
                        result.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                        result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    } else {
                        result.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                        result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                        result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    }
                    p += 4;
                    break;
                }
                default: throw Error(std::string("invalid escape: \\") + *p);
            }
            p++;
        } else {
            result.push_back(*p);
            p++;
        }
    }
    return result;
}

inline std::string parse_string_value(const char*& pos, const char* end) {
    skip_whitespace_and_comments(pos, end);
    if (pos >= end) return {};
    if (*pos == '"') return parse_quoted_string(pos, end);
    return parse_plain_value(pos, end);
}

// Stack-based schema result — zero heap allocation
struct ParsedSchema {
    static constexpr int MAX_FIELDS = 64;
    std::string_view fields[MAX_FIELDS];
    int count = 0;
};

// Parse schema: {field1,field2,...} or {field1:type1,...}
// Returns field names as string_views (zero-copy, no heap allocation).
inline ParsedSchema parse_schema(const char*& pos, const char* end) {
    if (pos >= end || *pos != '{') throw Error("expected '{'");
    pos++;
    ParsedSchema result;
    for (;;) {
        skip_whitespace(pos, end);
        if (pos >= end) throw Error("unexpected EOF in schema");
        if (*pos == '}') { pos++; break; }
        if (result.count > 0) {
            if (*pos != ',') throw Error("expected ','");
            pos++;
            skip_whitespace(pos, end);
        }
        // Parse field name
        const char* start = pos;
        while (pos < end) {
            char b = *pos;
            if (b == ',' || b == '}' || b == ':' || b == ' ' || b == '\t') break;
            pos++;
        }
        if (result.count < ParsedSchema::MAX_FIELDS) {
            result.fields[result.count++] = std::string_view(start, pos - start);
        }
        skip_whitespace(pos, end);
        // Skip optional type annotation after ':'
        if (pos < end && *pos == ':') {
            pos++;
            skip_whitespace(pos, end);
            if (pos < end && *pos == '{') {
                // Nested struct schema — skip balanced braces
                int depth = 0;
                while (pos < end) {
                    if (*pos == '{') depth++;
                    else if (*pos == '}') { depth--; if (depth == 0) { pos++; break; } }
                    pos++;
                }
            } else if (pos < end && *pos == '[') {
                // Array type annotation — skip balanced brackets
                int depth = 0;
                while (pos < end) {
                    if (*pos == '[') depth++;
                    else if (*pos == ']') { depth--; if (depth == 0) { pos++; break; } }
                    pos++;
                }
            } else if (pos + 3 <= end && pos[0] == 'm' && pos[1] == 'a' && pos[2] == 'p') {
                pos += 3;
                if (pos < end && *pos == '[') {
                    int depth = 0;
                    while (pos < end) {
                        if (*pos == '[') depth++;
                        else if (*pos == ']') { depth--; if (depth == 0) { pos++; break; } }
                        pos++;
                    }
                }
            } else {
                while (pos < end) {
                    char b = *pos;
                    if (b == ',' || b == '}' || b == ' ' || b == '\t') break;
                    pos++;
                }
            }
        }
    }
    return result;
}

// Skip balanced delimiters
inline void skip_balanced(const char*& pos, const char* end, char open, char close) {
    int depth = 0;
    while (pos < end) {
        char b = *pos; pos++;
        if (b == open) depth++;
        else if (b == close) { depth--; if (depth == 0) return; }
    }
    throw Error("unbalanced brackets");
}

// Skip any value
inline void skip_value(const char*& pos, const char* end) {
    skip_whitespace_and_comments(pos, end);
    if (pos >= end) return;
    switch (*pos) {
        case '"': pos++; while (pos < end) { if (*pos == '"') { pos++; return; } if (*pos == '\\') pos++; pos++; } break;
        case '(': skip_balanced(pos, end, '(', ')'); break;
        case '[': skip_balanced(pos, end, '[', ']'); break;
        default:
            while (pos < end) { char b = *pos; if (b == ',' || b == ')' || b == ']') return; pos++; }
            break;
    }
}

// Skip remaining comma-separated values in a tuple until ')' is found.
// Used when the target struct has fewer fields than the source data.
inline void skip_remaining_tuple_values(const char*& pos, const char* end) {
    for (;;) {
        skip_whitespace_and_comments(pos, end);
        if (pos >= end || *pos == ')') return;
        if (*pos == ',') { pos++; skip_whitespace_and_comments(pos, end); if (pos >= end || *pos == ')') return; }
        else return;
        skip_value(pos, end);
    }
}

} // namespace detail

// ============================================================================
// load_value specializations
// ============================================================================

inline void load_value(const char*& pos, const char* end, bool& out) {
    detail::skip_whitespace_and_comments(pos, end);
    if (pos + 4 <= end && pos[0] == 't' && pos[1] == 'r' && pos[2] == 'u' && pos[3] == 'e') {
        if (pos + 4 >= end || detail::is_delim(pos[4])) { out = true; pos += 4; return; }
    }
    if (pos + 5 <= end && pos[0] == 'f' && pos[1] == 'a' && pos[2] == 'l' && pos[3] == 's' && pos[4] == 'e') {
        if (pos + 5 >= end || detail::is_delim(pos[5])) { out = false; pos += 5; return; }
    }
    throw Error("invalid bool");
}

inline void load_value(const char*& pos, const char* end, int64_t& out) {
    detail::skip_whitespace_and_comments(pos, end);
    bool neg = false;
    if (pos < end && *pos == '-') { neg = true; pos++; }
    uint64_t val = 0;
    int digits = 0;
    while (pos < end && *pos >= '0' && *pos <= '9') {
        val = val * 10 + (*pos - '0');
        pos++; digits++;
    }
    if (digits == 0) throw Error("invalid number");
    out = neg ? -static_cast<int64_t>(val) : static_cast<int64_t>(val);
}

inline void load_value(const char*& pos, const char* end, uint64_t& out) {
    detail::skip_whitespace_and_comments(pos, end);
    uint64_t val = 0;
    int digits = 0;
    while (pos < end && *pos >= '0' && *pos <= '9') {
        val = val * 10 + (*pos - '0');
        pos++; digits++;
    }
    if (digits == 0) throw Error("invalid number");
    out = val;
}

inline void load_value(const char*& pos, const char* end, double& out) {
    detail::skip_whitespace_and_comments(pos, end);
    const char* start = pos;
    if (pos < end && *pos == '-') pos++;
    while (pos < end && *pos >= '0' && *pos <= '9') pos++;
    if (pos < end && *pos == '.') {
        pos++;
        while (pos < end && *pos >= '0' && *pos <= '9') pos++;
    }
    if (pos == start || (pos == start + 1 && *start == '-'))
        throw Error("invalid number");
    auto [ptr, ec] = std::from_chars(start, pos, out);
    if (ec != std::errc() || ptr != pos) throw Error("invalid float");
}

inline void load_value(const char*& pos, const char* end, int8_t& out) {
    int64_t v; load_value(pos, end, v); out = static_cast<int8_t>(v);
}
inline void load_value(const char*& pos, const char* end, int16_t& out) {
    int64_t v; load_value(pos, end, v); out = static_cast<int16_t>(v);
}
inline void load_value(const char*& pos, const char* end, int32_t& out) {
    int64_t v; load_value(pos, end, v); out = static_cast<int32_t>(v);
}
inline void load_value(const char*& pos, const char* end, uint8_t& out) {
    uint64_t v; load_value(pos, end, v); out = static_cast<uint8_t>(v);
}
inline void load_value(const char*& pos, const char* end, uint16_t& out) {
    uint64_t v; load_value(pos, end, v); out = static_cast<uint16_t>(v);
}
inline void load_value(const char*& pos, const char* end, uint32_t& out) {
    uint64_t v; load_value(pos, end, v); out = static_cast<uint32_t>(v);
}
inline void load_value(const char*& pos, const char* end, float& out) {
    double v; load_value(pos, end, v); out = static_cast<float>(v);
}

inline void load_value(const char*& pos, const char* end, char& out) {
    std::string s = detail::parse_string_value(pos, end);
    out = s.empty() ? '\0' : s[0];
}

inline void load_value(const char*& pos, const char* end, std::string& out) {
    out = detail::parse_string_value(pos, end);
}

template <typename T>
void load_value(const char*& pos, const char* end, std::vector<T>& out) {
    detail::skip_whitespace_and_comments(pos, end);
    if (pos >= end || *pos != '[') throw Error("expected '['");
    pos++;
    out.clear();
    bool first = true;
    for (;;) {
        detail::skip_whitespace_and_comments(pos, end);
        if (pos >= end || *pos == ']') { pos++; break; }
        if (!first) {
            if (*pos == ',') {
                pos++;
                detail::skip_whitespace_and_comments(pos, end);
                if (pos < end && *pos == ']') { pos++; break; }
            } else break;
        }
        first = false;
        T elem;
        load_value(pos, end, elem);
        out.push_back(std::move(elem));
    }
}

template <typename T>
void load_value(const char*& pos, const char* end, std::optional<T>& out) {
    detail::skip_whitespace_and_comments(pos, end);
    if (detail::at_value_end(pos, end)) {
        out = std::nullopt;
        return;
    }
    T val;
    load_value(pos, end, val);
    out = std::move(val);
}

template <typename K, typename V>
void load_value(const char*& pos, const char* end, std::unordered_map<K, V>& out) {
    detail::skip_whitespace_and_comments(pos, end);
    if (pos >= end || *pos != '[') throw Error("expected '['");
    pos++;
    out.clear();
    bool first = true;
    for (;;) {
        detail::skip_whitespace_and_comments(pos, end);
        if (pos >= end || *pos == ']') { pos++; break; }
        if (!first) {
            if (*pos == ',') {
                pos++;
                detail::skip_whitespace_and_comments(pos, end);
                if (pos < end && *pos == ']') { pos++; break; }
            } else break;
        }
        first = false;
        if (pos >= end || *pos != '(') throw Error("expected '(' in map entry");
        pos++;
        K key; load_value(pos, end, key);
        detail::skip_whitespace_and_comments(pos, end);
        if (pos < end && *pos == ',') pos++;
        V val; load_value(pos, end, val);
        detail::skip_whitespace_and_comments(pos, end);
        if (pos < end && *pos == ')') pos++;
        out[std::move(key)] = std::move(val);
    }
}

// Struct load — requires AsonFields specialization
template <typename T>
std::enable_if_t<AsonFields<T>::defined, void>
load_value(const char*& pos, const char* end, T& out) {
    detail::skip_whitespace_and_comments(pos, end);

    // If starts with '{', it has an inline schema
    if (pos < end && *pos == '{') {
        auto schema = detail::parse_schema(pos, end);
        detail::skip_whitespace_and_comments(pos, end);
        if (pos >= end || *pos != ':') throw Error("expected ':'");
        pos++;
        detail::skip_whitespace_and_comments(pos, end);
        // Build field map on stack: schema index -> struct field index
        int field_map[detail::ParsedSchema::MAX_FIELDS];
        for (int fi = 0; fi < schema.count; fi++)
            field_map[fi] = AsonFields<T>::find_field(schema.fields[fi]);
        // Parse tuple
        if (pos >= end || *pos != '(') throw Error("expected '('");
        pos++;
        for (int i = 0; i < schema.count; i++) {
            detail::skip_whitespace_and_comments(pos, end);
            if (pos < end && *pos == ')') break;
            if (i > 0) {
                if (*pos == ',') { pos++; detail::skip_whitespace_and_comments(pos, end); if (pos < end && *pos == ')') break; }
                else if (*pos == ')') break;
                else throw Error("expected ',' or ')'");
            }
            if (field_map[i] >= 0) {
                AsonFields<T>::load_field(pos, end, out, field_map[i]);
            } else {
                detail::skip_value(pos, end);
            }
        }
        detail::skip_remaining_tuple_values(pos, end);
        detail::skip_whitespace_and_comments(pos, end);
        if (pos < end && *pos == ')') pos++;
        return;
    }

    // Positional tuple: (val1,val2,...)
    if (pos < end && *pos == '(') {
        pos++;
        constexpr int N = AsonFields<T>::field_count;
        for (int i = 0; i < N; i++) {
            detail::skip_whitespace_and_comments(pos, end);
            if (pos < end && *pos == ')') break;
            if (i > 0) {
                if (*pos == ',') { pos++; detail::skip_whitespace_and_comments(pos, end); if (pos < end && *pos == ')') break; }
                else if (*pos == ')') break;
                else throw Error("expected ',' or ')'");
            }
            AsonFields<T>::load_field(pos, end, out, i);
        }
        detail::skip_remaining_tuple_values(pos, end);
        detail::skip_whitespace_and_comments(pos, end);
        if (pos < end && *pos == ')') pos++;
        return;
    }

    throw Error("expected '{' or '(' for struct");
}

// ============================================================================
// Binary dump_bin_value / load_bin_value specializations
// ============================================================================

template <typename T>
inline std::enable_if_t<std::is_arithmetic_v<T>, void>
dump_bin_value(std::string& buf, const T& v) {
    if constexpr (std::is_same_v<T, bool>) {
        buf.push_back(v ? 1 : 0);
    } else {
        buf.append(reinterpret_cast<const char*>(&v), sizeof(T));
    }
}

template <typename T>
inline std::enable_if_t<std::is_arithmetic_v<T>, void>
load_bin_value(const char*& pos, const char* end, T& out) {
    if constexpr (std::is_same_v<T, bool>) {
        out = (*pos != 0);
        pos += 1;
    } else {
        std::memcpy(&out, pos, sizeof(T));
        pos += sizeof(T);
    }
}

inline void dump_bin_value(std::string& buf, const std::string& v) {
    uint32_t len = v.size();
    buf.append(reinterpret_cast<const char*>(&len), 4);
    buf.append(v);
}

inline void dump_bin_value(std::string& buf, std::string_view v) {
    uint32_t len = v.size();
    buf.append(reinterpret_cast<const char*>(&len), 4);
    buf.append(v);
}

inline void load_bin_value(const char*& pos, const char* end, std::string& out) {
    uint32_t len;
    std::memcpy(&len, pos, 4);
    pos += 4;
    out.assign(pos, len);
    pos += len;
}

inline void load_bin_value(const char*& pos, const char* end, std::string_view& out) {
    uint32_t len;
    std::memcpy(&len, pos, 4);
    pos += 4;
    out = std::string_view(pos, len);
    pos += len;
}

template <typename T>
inline void dump_bin_value(std::string& buf, const std::vector<T>& v) {
    uint32_t len = v.size();
    buf.append(reinterpret_cast<const char*>(&len), 4);
    if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
        buf.append(reinterpret_cast<const char*>(v.data()), len * sizeof(T));
    } else {
        for (const auto& item : v) {
            dump_bin_value(buf, item);
        }
    }
}

template <typename T>
inline void load_bin_value(const char*& pos, const char* end, std::vector<T>& out) {
    uint32_t len;
    std::memcpy(&len, pos, 4);
    pos += 4;
    out.resize(len);
    if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
        std::memcpy(out.data(), pos, len * sizeof(T));
        pos += len * sizeof(T);
    } else {
        for (uint32_t i = 0; i < len; i++) {
            load_bin_value(pos, end, out[i]);
        }
    }
}

template <typename T>
inline void dump_bin_value(std::string& buf, const std::optional<T>& v) {
    if (v.has_value()) {
        buf.push_back(1);
        dump_bin_value(buf, *v);
    } else {
        buf.push_back(0);
    }
}

template <typename T>
inline void load_bin_value(const char*& pos, const char* end, std::optional<T>& out) {
    uint8_t has_val = *pos++;
    if (has_val) {
        T inner{};
        load_bin_value(pos, end, inner);
        out = std::move(inner);
    } else {
        out = std::nullopt;
    }
}

template <typename K, typename V>
inline void dump_bin_value(std::string& buf, const std::unordered_map<K, V>& m) {
    uint32_t len = m.size();
    buf.append(reinterpret_cast<const char*>(&len), 4);
    for (const auto& [k, v] : m) {
        dump_bin_value(buf, k);
        dump_bin_value(buf, v);
    }
}

template <typename K, typename V>
inline void load_bin_value(const char*& pos, const char* end, std::unordered_map<K, V>& out) {
    uint32_t len;
    std::memcpy(&len, pos, 4);
    pos += 4;
    out.clear();
    out.reserve(len);
    for (uint32_t i = 0; i < len; i++) {
        K k;
        load_bin_value(pos, end, k);
        V v;
        load_bin_value(pos, end, v);
        out.emplace(std::move(k), std::move(v));
    }
}

template <typename T>
inline std::enable_if_t<AsonFields<T>::defined, void>
dump_bin_value(std::string& buf, const T& v) {
    AsonFields<T>::dump_bin_fields(buf, v);
}

template <typename T>
inline std::enable_if_t<AsonFields<T>::defined, void>
load_bin_value(const char*& pos, const char* end, T& out) {
    AsonFields<T>::load_bin_fields(pos, end, out);
}

// ============================================================================
// Public API
// ============================================================================

// is_vector / is_optional traits
namespace detail {
template <typename T> struct is_vector : std::false_type {};
template <typename T> struct is_vector<std::vector<T>> : std::true_type {};
template <typename T> struct is_optional : std::false_type {};
template <typename T> struct is_optional<std::optional<T>> : std::true_type {};

// write_field_schema: recursively write schema annotation for a field type.
// Struct fields get :{f1,f2,...}, vector<struct> fields get :[{f1,f2,...}],
// primitives get :type only in typed mode.
template <typename FieldType>
inline void write_field_schema(std::string& buf, bool typed) {
    using T = std::decay_t<FieldType>;
    if constexpr (AsonFields<T>::defined) {
        // Nested struct: field:{f1,f2,...}
        buf.push_back(':');
        buf.push_back('{');
        AsonFields<T>::write_schema(buf, typed);
        buf.push_back('}');
    } else if constexpr (is_vector<T>::value) {
        using Elem = typename T::value_type;
        if constexpr (AsonFields<Elem>::defined) {
            // Vector of structs: field:[{f1,f2,...}]
            buf.append(":[{", 3);
            AsonFields<Elem>::write_schema(buf, typed);
            buf.append("}]", 2);
        } else if (typed) {
            auto tn = TypeName<Elem>::value;
            if (tn) { buf.push_back(':'); buf.push_back('['); buf.append(tn); buf.push_back(']'); }
        }
    } else if constexpr (is_optional<T>::value) {
        using Inner = typename T::value_type;
        if constexpr (AsonFields<Inner>::defined) {
            buf.push_back(':');
            buf.push_back('{');
            AsonFields<Inner>::write_schema(buf, typed);
            buf.push_back('}');
        } else if (typed) {
            auto tn = TypeName<Inner>::value;
            if (tn) { buf.push_back(':'); buf.append(tn); }
        }
    } else if (typed) {
        auto tn = TypeName<T>::value;
        if (tn) { buf.push_back(':'); buf.append(tn); }
    }
}
} // namespace detail

// encode: struct -> ASON string  {field1,field2,...}:(val1,val2,...)
// encode: vector<struct> -> ASON string  [{field1,field2,...}]:(val1,...),(val2,...),...
template <typename T>
std::string encode(const T& v) {
    if constexpr (detail::is_vector<T>::value) {
        using Elem = typename T::value_type;
        static_assert(AsonFields<Elem>::defined, "ASON_FIELDS not defined for element type");
        std::string buf;
        buf.reserve(v.size() * 64 + 128);
        buf.push_back('[');
        buf.push_back('{');
        AsonFields<Elem>::write_schema(buf, false);
        buf.push_back('}');
        buf.push_back(']');
        buf.push_back(':');
        for (size_t i = 0; i < v.size(); i++) {
            if (i > 0) buf.push_back(',');
            buf.push_back('(');
            AsonFields<Elem>::dump_fields(buf, v[i]);
            buf.push_back(')');
        }
        return buf;
    } else {
        static_assert(AsonFields<T>::defined, "ASON_FIELDS not defined for this type");
        std::string buf;
        buf.reserve(256);
        buf.push_back('{');
        AsonFields<T>::write_schema(buf, false);
        buf.push_back('}');
        buf.push_back(':');
        buf.push_back('(');
        AsonFields<T>::dump_fields(buf, v);
        buf.push_back(')');
        return buf;
    }
}

// encode_typed: struct or vector<struct> -> ASON string with type annotations
template <typename T>
std::string encode_typed(const T& v) {
    if constexpr (detail::is_vector<T>::value) {
        using Elem = typename T::value_type;
        static_assert(AsonFields<Elem>::defined, "ASON_FIELDS not defined for element type");
        std::string buf;
        buf.reserve(v.size() * 64 + 128);
        buf.push_back('[');
        buf.push_back('{');
        AsonFields<Elem>::write_schema(buf, true);
        buf.push_back('}');
        buf.push_back(']');
        buf.push_back(':');
        for (size_t i = 0; i < v.size(); i++) {
            if (i > 0) buf.push_back(',');
            buf.push_back('(');
            AsonFields<Elem>::dump_fields(buf, v[i]);
            buf.push_back(')');
        }
        return buf;
    } else {
        static_assert(AsonFields<T>::defined, "ASON_FIELDS not defined for this type");
        std::string buf;
        buf.reserve(256);
        buf.push_back('{');
        AsonFields<T>::write_schema(buf, true);
        buf.push_back('}');
        buf.push_back(':');
        buf.push_back('(');
        AsonFields<T>::dump_fields(buf, v);
        buf.push_back(')');
        return buf;
    }
}

// ---------------------------------------------------------------------------
// Pretty-format: smart indentation for ASON output
// ---------------------------------------------------------------------------
//   Simple structures stay inline:   {name:str, age:int}:(Alice, 30)
//   Complex structures expand with 2-space indentation.
namespace detail {

constexpr int PRETTY_MAX_WIDTH = 100;

inline std::vector<int> build_match_table(const std::string& src) {
    int n = static_cast<int>(src.size());
    std::vector<int> mat(n, -1);
    std::vector<int> stk;
    bool in_quote = false;
    for (int i = 0; i < n; i++) {
        if (in_quote) {
            if (src[i] == '\\' && i + 1 < n) { i++; continue; }
            if (src[i] == '"') in_quote = false;
            continue;
        }
        switch (src[i]) {
            case '"': in_quote = true; break;
            case '{': case '(': case '[': stk.push_back(i); break;
            case '}': case ')': case ']':
                if (!stk.empty()) {
                    int j = stk.back(); stk.pop_back();
                    mat[j] = i; mat[i] = j;
                }
                break;
        }
    }
    return mat;
}

struct PrettyFmt {
    const std::string& src;
    const std::vector<int>& mat;
    std::string out;
    int pos = 0;
    int depth = 0;

    void write_indent() {
        for (int i = 0; i < depth; i++) out.append("  ");
    }

    void write_quoted() {
        out.push_back('"'); pos++;
        while (pos < (int)src.size()) {
            char ch = src[pos]; out.push_back(ch); pos++;
            if (ch == '\\' && pos < (int)src.size()) { out.push_back(src[pos]); pos++; }
            else if (ch == '"') break;
        }
    }

    void write_inline(int start, int end) {
        int d = 0; bool inq = false;
        for (int i = start; i < end; i++) {
            char ch = src[i];
            if (inq) {
                out.push_back(ch);
                if (ch == '\\' && i + 1 < end) { i++; out.push_back(src[i]); }
                else if (ch == '"') inq = false;
                continue;
            }
            switch (ch) {
                case '"': inq = true; out.push_back(ch); break;
                case '{': case '(': case '[': d++; out.push_back(ch); break;
                case '}': case ')': case ']': d--; out.push_back(ch); break;
                case ',': out.push_back(','); if (d == 1) out.push_back(' '); break;
                default: out.push_back(ch); break;
            }
        }
    }

    void write_value() {
        while (pos < (int)src.size()) {
            char ch = src[pos];
            if (ch == ',' || ch == ')' || ch == '}' || ch == ']') break;
            if (ch == '"') write_quoted(); else { out.push_back(ch); pos++; }
        }
    }

    void write_element(int boundary) {
        while (pos < boundary && src[pos] != ',') {
            char ch = src[pos];
            if (ch == '{' || ch == '(' || ch == '[') write_group();
            else if (ch == '"') write_quoted();
            else { out.push_back(ch); pos++; }
        }
    }

    void write_group() {
        if (pos >= (int)src.size()) return;
        char ch = src[pos];
        if (ch != '{' && ch != '(' && ch != '[') { write_value(); return; }

        // Special case: [{...}] array schema — fuse brackets
        if (ch == '[' && pos + 1 < (int)src.size() && src[pos + 1] == '{') {
            int cb = mat[pos + 1], ck = mat[pos];
            if (cb >= 0 && ck >= 0 && cb + 1 == ck) {
                int width = ck - pos + 1;
                if (width <= PRETTY_MAX_WIDTH) {
                    write_inline(pos, ck + 1); pos = ck + 1; return;
                }
                out.push_back('['); pos++;
                write_group();
                out.push_back(']'); pos++;
                return;
            }
        }

        int close = mat[pos];
        if (close < 0) { out.push_back(ch); pos++; return; }
        int width = close - pos + 1;
        if (width <= PRETTY_MAX_WIDTH) { write_inline(pos, close + 1); pos = close + 1; return; }

        char close_ch = src[close];
        out.push_back(ch); pos++;
        if (pos >= close) { out.push_back(close_ch); pos = close + 1; return; }

        out.push_back('\n'); depth++;
        bool first = true;
        while (pos < close) {
            if (src[pos] == ',') pos++;
            if (!first) { out.push_back(','); out.push_back('\n'); }
            first = false;
            write_indent();
            write_element(close);
        }
        out.push_back('\n'); depth--;
        write_indent();
        out.push_back(close_ch); pos = close + 1;
    }

    void write_object_top() {
        write_group();
        if (pos < (int)src.size() && src[pos] == ':') {
            out.push_back(':'); pos++;
            if (pos < (int)src.size()) {
                int cl = mat[pos];
                if (cl >= 0 && cl - pos + 1 <= PRETTY_MAX_WIDTH) {
                    write_inline(pos, cl + 1); pos = cl + 1;
                } else {
                    out.push_back('\n'); depth++;
                    write_indent(); write_group(); depth--;
                }
            }
        }
    }

    void write_array_top() {
        out.push_back('['); pos++;
        write_group();
        if (pos < (int)src.size() && src[pos] == ']') { out.push_back(']'); pos++; }
        if (pos < (int)src.size() && src[pos] == ':') { out.append(":\n"); pos++; }
        depth++;
        bool first = true;
        while (pos < (int)src.size()) {
            if (src[pos] == ',') pos++;
            if (pos >= (int)src.size()) break;
            if (!first) { out.push_back(','); out.push_back('\n'); }
            first = false;
            write_indent(); write_group();
        }
        out.push_back('\n'); depth--;
    }

    void write_top() {
        if (pos >= (int)src.size()) return;
        if (src[pos] == '[' && pos + 1 < (int)src.size() && src[pos + 1] == '{')
            write_array_top();
        else if (src[pos] == '{')
            write_object_top();
        else
            out.append(src.substr(pos));
    }
};

} // namespace detail

// pretty_format: reformat compact ASON with smart indentation
inline std::string pretty_format(const std::string& compact) {
    if (compact.empty()) return compact;
    auto mat = detail::build_match_table(compact);
    detail::PrettyFmt f{compact, mat, {}, 0, 0};
    f.out.reserve(compact.size() * 2);
    f.write_top();
    return std::move(f.out);
}

// encode_pretty: struct or vector<struct> -> pretty-formatted ASON
template <typename T>
std::string encode_pretty(const T& v) {
    return pretty_format(encode(v));
}

// encode_pretty_typed: struct or vector<struct> -> pretty-formatted ASON with type annotations
template <typename T>
std::string encode_pretty_typed(const T& v) {
    return pretty_format(encode_typed(v));
}

// decode: ASON string -> T (auto-detects single struct vs vector)
// For single: {schema}:(data)
// For vector: [{schema}]:(data1),(data2),...
template <typename T>
T decode(std::string_view input) {
    if constexpr (detail::is_vector<T>::value) {
        using Elem = typename T::value_type;
        static_assert(AsonFields<Elem>::defined, "ASON_FIELDS not defined for element type");
        const char* pos = input.data();
        const char* end = pos + input.size();
        detail::skip_whitespace_and_comments(pos, end);
        if (pos >= end || *pos != '[') throw Error("expected '[' for vec");
        pos++;
        if (pos >= end || *pos != '{') throw Error("expected '{' after '['");
        auto schema = detail::parse_schema(pos, end);
        detail::skip_whitespace_and_comments(pos, end);
        if (pos >= end || *pos != ']') throw Error("expected ']' after schema");
        pos++;
        detail::skip_whitespace_and_comments(pos, end);
        if (pos >= end || *pos != ':') throw Error("expected ':'");
        pos++;
        int field_map[detail::ParsedSchema::MAX_FIELDS];
        for (int fi = 0; fi < schema.count; fi++)
            field_map[fi] = AsonFields<Elem>::find_field(schema.fields[fi]);
        T result;
        for (;;) {
            detail::skip_whitespace_and_comments(pos, end);
            if (pos >= end) break;
            if (*pos != '(') break;
            pos++;
            Elem elem{};
            for (int i = 0; i < schema.count; i++) {
                detail::skip_whitespace_and_comments(pos, end);
                if (pos < end && *pos == ')') break;
                if (i > 0) {
                    if (*pos == ',') { pos++; detail::skip_whitespace_and_comments(pos, end); if (pos < end && *pos == ')') break; }
                    else if (*pos == ')') break;
                    else throw Error("expected ',' or ')'");
                }
                if (field_map[i] >= 0) {
                    AsonFields<Elem>::load_field(pos, end, elem, field_map[i]);
                } else {
                    detail::skip_value(pos, end);
                }
            }
            detail::skip_remaining_tuple_values(pos, end);
            detail::skip_whitespace_and_comments(pos, end);
            if (pos < end && *pos == ')') pos++;
            result.push_back(std::move(elem));

            detail::skip_whitespace_and_comments(pos, end);
            if (pos < end && *pos == ',') {
                pos++;
                detail::skip_whitespace_and_comments(pos, end);
                if (pos >= end || *pos != '(') break;
            }
        }
        return result;
    } else {
        static_assert(AsonFields<T>::defined, "ASON_FIELDS not defined for this type");
        const char* pos = input.data();
        const char* end = pos + input.size();
        detail::skip_whitespace_and_comments(pos, end);
        T result{};
        if (pos >= end || *pos != '{') throw Error("expected '{'");
        auto schema = detail::parse_schema(pos, end);
        detail::skip_whitespace_and_comments(pos, end);
        if (pos >= end || *pos != ':') throw Error("expected ':'");
        pos++;
        detail::skip_whitespace_and_comments(pos, end);
        int field_map[detail::ParsedSchema::MAX_FIELDS];
        for (int fi = 0; fi < schema.count; fi++)
            field_map[fi] = AsonFields<T>::find_field(schema.fields[fi]);
        if (pos >= end || *pos != '(') throw Error("expected '('");
        pos++;
        for (int i = 0; i < schema.count; i++) {
            detail::skip_whitespace_and_comments(pos, end);
            if (pos < end && *pos == ')') break;
            if (i > 0) {
                if (*pos == ',') { pos++; detail::skip_whitespace_and_comments(pos, end); if (pos < end && *pos == ')') break; }
                else if (*pos == ')') break;
                else throw Error("expected ',' or ')'");
            }
            if (field_map[i] >= 0) {
                AsonFields<T>::load_field(pos, end, result, field_map[i]);
            } else {
                detail::skip_value(pos, end);
            }
        }
        detail::skip_remaining_tuple_values(pos, end);
        detail::skip_whitespace_and_comments(pos, end);
        if (pos < end && *pos == ')') pos++;
        detail::skip_whitespace_and_comments(pos, end);
        if (pos < end) throw Error("trailing characters after decoded value");
        return result;
    }
}

// encode_bin: struct -> ASON-BIN string
template <typename T>
std::string encode_bin(const T& v) {
    std::string buf;
    buf.reserve(256);
    dump_bin_value(buf, v);
    return buf;
}

// decode_bin: ASON-BIN string -> struct
template <typename T>
T decode_bin(std::string_view input) {
    T result{};
    const char* pos = input.data();
    const char* end = pos + input.size();
    load_bin_value(pos, end, result);
    return result;
}

// ============================================================================
// ASON_FIELDS macro — reflection for structs
// ============================================================================

// Helper macros
#define ASON_FIELD_NAME(field, name, ...) name
#define ASON_FIELD_TYPE(field, name, type_str, ...) type_str
#define ASON_FIELD_MEMBER(field, ...) field

// Count fields
#define ASON_PP_NARG(...)  ASON_PP_NARG_(__VA_ARGS__, ASON_PP_RSEQ_N())
#define ASON_PP_NARG_(...) ASON_PP_ARG_N(__VA_ARGS__)
#define ASON_PP_ARG_N(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,N,...) N
#define ASON_PP_RSEQ_N() 32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0

// Expansion helpers
#define ASON_EXPAND(...) __VA_ARGS__
#define ASON_CAT(a,b) ASON_CAT_(a,b)
#define ASON_CAT_(a,b) a##b

// Per-field schema emission (idx-based, for use with ASON_FOR_EACH)
#define ASON_SCHEMA_ITEM_1(idx, f) \
    if (idx > 0) buf.push_back(','); \
    buf.append(ASON_FIELD_NAME f); \
    ::ason::detail::write_field_schema<decltype(std::declval<Self>().ASON_FIELD_MEMBER f)>(buf, typed);

// Per-field dump (idx-based, for use with ASON_FOR_EACH)
#define ASON_DUMP_ITEM_1(idx, f) \
    if (idx > 0) buf.push_back(','); ::ason::dump_value(buf, v.ASON_FIELD_MEMBER f);

// Per-field binary dump
#define ASON_DUMP_BIN_ITEM_1(idx, f) \
    ::ason::dump_bin_value(buf, v.ASON_FIELD_MEMBER f);

// Per-field load dispatch
#define ASON_LOAD_CASE_1(idx, f) \
    case idx: ::ason::load_value(pos, end, out.ASON_FIELD_MEMBER f); break;

// Per-field binary load
#define ASON_LOAD_BIN_ITEM_1(idx, f) \
    ::ason::load_bin_value(pos, end, out.ASON_FIELD_MEMBER f);

// Per-field name match
#define ASON_FIELDMAP_1(idx, f) \
    if (name == ASON_FIELD_NAME f) return idx;

// Main macro for 1-32 fields -- we use X-macro style

#define ASON_FOR_EACH_1(what,  f1)                         what(0,f1)
#define ASON_FOR_EACH_2(what,  f1,f2)                      ASON_FOR_EACH_1(what,f1)  what(1,f2)
#define ASON_FOR_EACH_3(what,  f1,f2,f3)                   ASON_FOR_EACH_2(what,f1,f2) what(2,f3)
#define ASON_FOR_EACH_4(what,  f1,f2,f3,f4)                ASON_FOR_EACH_3(what,f1,f2,f3) what(3,f4)
#define ASON_FOR_EACH_5(what,  f1,f2,f3,f4,f5)             ASON_FOR_EACH_4(what,f1,f2,f3,f4) what(4,f5)
#define ASON_FOR_EACH_6(what,  f1,f2,f3,f4,f5,f6)          ASON_FOR_EACH_5(what,f1,f2,f3,f4,f5) what(5,f6)
#define ASON_FOR_EACH_7(what,  f1,f2,f3,f4,f5,f6,f7)       ASON_FOR_EACH_6(what,f1,f2,f3,f4,f5,f6) what(6,f7)
#define ASON_FOR_EACH_8(what,  f1,f2,f3,f4,f5,f6,f7,f8)    ASON_FOR_EACH_7(what,f1,f2,f3,f4,f5,f6,f7) what(7,f8)
#define ASON_FOR_EACH_9(what,  f1,f2,f3,f4,f5,f6,f7,f8,f9) ASON_FOR_EACH_8(what,f1,f2,f3,f4,f5,f6,f7,f8) what(8,f9)
#define ASON_FOR_EACH_10(what, f1,f2,f3,f4,f5,f6,f7,f8,f9,f10) ASON_FOR_EACH_9(what,f1,f2,f3,f4,f5,f6,f7,f8,f9) what(9,f10)
#define ASON_FOR_EACH_11(what, f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11) ASON_FOR_EACH_10(what,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10) what(10,f11)
#define ASON_FOR_EACH_12(what, f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12) ASON_FOR_EACH_11(what,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11) what(11,f12)
#define ASON_FOR_EACH_13(what, f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13) ASON_FOR_EACH_12(what,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12) what(12,f13)
#define ASON_FOR_EACH_14(what, f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14) ASON_FOR_EACH_13(what,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13) what(13,f14)
#define ASON_FOR_EACH_15(what, f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15) ASON_FOR_EACH_14(what,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14) what(14,f15)
#define ASON_FOR_EACH_16(what, f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16) ASON_FOR_EACH_15(what,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15) what(15,f16)
#define ASON_FOR_EACH_17(what, f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17) ASON_FOR_EACH_16(what,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16) what(16,f17)
#define ASON_FOR_EACH_18(what, f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18) ASON_FOR_EACH_17(what,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17) what(17,f18)
#define ASON_FOR_EACH_19(what, f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,f19) ASON_FOR_EACH_18(what,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18) what(18,f19)
#define ASON_FOR_EACH_20(what, f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,f19,f20) ASON_FOR_EACH_19(what,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,f19) what(19,f20)

#define ASON_FOR_EACH_(N, what, ...) ASON_CAT(ASON_FOR_EACH_, N)(what, __VA_ARGS__)
#define ASON_FOR_EACH(what, ...) ASON_FOR_EACH_(ASON_PP_NARG(__VA_ARGS__), what, __VA_ARGS__)

// ASON_FIELDS — the main user-facing macro
// Usage:
//   struct User { int64_t id; std::string name; bool active; };
//   ASON_FIELDS(User,
//     (id,   "id",   "int"),
//     (name, "name", "str"),
//     (active,"active","bool"))
//
// The third element (type string) is used for typed schema output.

#define ASON_FIELDS(StructName, ...) \
    template <> struct ason::AsonFields<StructName> { \
        using Self = StructName; \
        static constexpr bool defined = true; \
        static constexpr int field_count = ASON_PP_NARG(__VA_ARGS__); \
        static void write_schema(std::string& buf, bool typed) { \
            ASON_FOR_EACH(ASON_SCHEMA_ITEM_1, __VA_ARGS__) \
        } \
        static void dump_fields(std::string& buf, const Self& v) { \
            ASON_FOR_EACH(ASON_DUMP_ITEM_1, __VA_ARGS__) \
        } \
        static void dump_bin_fields(std::string& buf, const Self& v) { \
            ASON_FOR_EACH(ASON_DUMP_BIN_ITEM_1, __VA_ARGS__) \
        } \
        static int find_field(std::string_view name) { \
            ASON_FOR_EACH(ASON_FIELDMAP_1, __VA_ARGS__) \
            return -1; \
        } \
        static void build_field_map(const ::ason::detail::ParsedSchema& schema, int* out) { \
            for (int i = 0; i < schema.count; i++) \
                out[i] = find_field(schema.fields[i]); \
        } \
        static void load_field(const char*& pos, const char* end, Self& out, int idx) { \
            switch (idx) { \
                ASON_FOR_EACH(ASON_LOAD_CASE_1, __VA_ARGS__) \
                default: ::ason::detail::skip_value(pos, end); break; \
            } \
        } \
        static void load_bin_fields(const char*& pos, const char* end, Self& out) { \
            ASON_FOR_EACH(ASON_LOAD_BIN_ITEM_1, __VA_ARGS__) \
        } \
    };

// Convenience: ASON_FIELDS with auto-inferred types (no explicit type strings needed)
// For typed output, types are deduced from the member types via TypeName<>.

} // namespace ason

#endif // ASON_HPP
