#pragma once
// ============================================================================
// ASUN — Untyped Value support (C++17 or later, header-only, depends on asun.hpp)
//
// Provides a generic Value tree (null / bool / int / double / string / array)
// for cases where the schema is unknown at compile time, used for conformance
// testing and schema-less consumers. Re-uses SIMD scanning, fast itoa/dtoa,
// and the escape table from <asun.hpp> so the typed code paths are not
// regressed.
//
//   asun::Value v = asun::decode_value(input);
//   std::string s = asun::encode_value(v);
//
// Design notes (performance):
//   * `Value` is a tagged struct (no std::variant indirection) so element
//     access is a single load + branch; arrays are stored as std::vector for
//     cache locality.
//   * Plain-string scanning re-uses the existing SIMD plain-delimiter helpers.
//   * Encoding falls through `detail::append_str` (SIMD quoting check) and
//     `detail::append_i64` / `detail::append_f64` (the same fast number
//     formatters used by the typed encoder).
// ============================================================================

#ifndef ASUN_VALUE_HPP
#define ASUN_VALUE_HPP

#include "asun.hpp"

#include <vector>
#include <string>
#include <string_view>
#include <cstdint>
#include <cstring>

namespace asun {

// ============================================================================
// Value: untyped tagged tree
// ============================================================================
struct Value {
    enum Tag : uint8_t { Null, Bool, Int, Double, String, Array };

    Tag tag = Null;
    bool b = false;
    int64_t i = 0;
    double d = 0.0;
    std::string s;
    std::vector<Value> arr;

    Value() = default;

    static Value null_()                      { Value v; v.tag = Null; return v; }
    static Value boolean(bool x)              { Value v; v.tag = Bool; v.b = x; return v; }
    static Value integer(int64_t x)           { Value v; v.tag = Int; v.i = x; return v; }
    static Value floating(double x)           { Value v; v.tag = Double; v.d = x; return v; }
    static Value string(std::string x)        { Value v; v.tag = String; v.s = std::move(x); return v; }
    static Value array(std::vector<Value> x)  { Value v; v.tag = Array; v.arr = std::move(x); return v; }

    // Deep equality. Cross-type int/double compare numerically (matches the
    // semantics used by the conformance harness in other languages).
    bool operator==(const Value& o) const {
        if (tag == Int && o.tag == Double)    return numeric_equal(static_cast<double>(i), o.d);
        if (tag == Double && o.tag == Int)    return numeric_equal(d, static_cast<double>(o.i));
        if (tag != o.tag) return false;
        switch (tag) {
            case Null:   return true;
            case Bool:   return b == o.b;
            case Int:    return i == o.i;
            case Double: return numeric_equal(d, o.d);
            case String: return s == o.s;
            case Array: {
                if (arr.size() != o.arr.size()) return false;
                for (size_t k = 0; k < arr.size(); k++)
                    if (!(arr[k] == o.arr[k])) return false;
                return true;
            }
        }
        return false;
    }
    bool operator!=(const Value& o) const { return !(*this == o); }

    static bool numeric_equal(double a, double b) {
        if (a == b) return true;
        double diff = a > b ? a - b : b - a;
        double aa = a < 0 ? -a : a;
        double bb = b < 0 ? -b : b;
        double scale = aa > bb ? aa : bb;
        double tol = scale * 1e-12;
        if (tol < 1e-12) tol = 1e-12;
        return diff <= tol;
    }

    // Diagnostic stringifier (JSON-like, used only for failure messages).
    std::string to_diagnostic() const {
        std::string out; append_diagnostic(out); return out;
    }
    void append_diagnostic(std::string& out) const {
        switch (tag) {
            case Null:   out.append("null"); break;
            case Bool:   out.append(b ? "true" : "false"); break;
            case Int:    detail::append_i64(out, i); break;
            case Double: detail::append_f64(out, d); break;
            case String:
                out.push_back('"');
                for (char c : s) {
                    if (c == '"' || c == '\\') { out.push_back('\\'); out.push_back(c); }
                    else if (c == '\n') out.append("\\n");
                    else if (c == '\r') out.append("\\r");
                    else if (c == '\t') out.append("\\t");
                    else if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8]; std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                        out.append(buf);
                    } else out.push_back(c);
                }
                out.push_back('"');
                break;
            case Array:
                out.push_back('[');
                for (size_t k = 0; k < arr.size(); k++) {
                    if (k) out.push_back(',');
                    arr[k].append_diagnostic(out);
                }
                out.push_back(']');
                break;
        }
    }
};

// ============================================================================
// Untyped decode
// ============================================================================
namespace detail {

// Skip ws+comments and report when an unterminated /* … */ is encountered.
inline void skip_ws_strict(const char*& pos, const char* end) {
    for (;;) {
        while (pos < end) {
            char c = *pos;
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { pos++; continue; }
            break;
        }
        if (pos + 1 < end && pos[0] == '/' && pos[1] == '*') {
            const char* start = pos;
            pos += 2;
            bool closed = false;
            while (pos + 1 < end) {
                if (pos[0] == '*' && pos[1] == '/') { pos += 2; closed = true; break; }
                pos++;
            }
            if (!closed) {
                (void)start;
                throw Error("unterminated comment");
            }
            continue;
        }
        return;
    }
}

// Classify an already-trimmed plain token into the appropriate Value scalar.
inline Value classify_plain(std::string_view tok) {
    if (tok.empty()) return Value::string(std::string());
    if (tok == "true")  return Value::boolean(true);
    if (tok == "false") return Value::boolean(false);

    // Try integer: optional '-' followed by digits (with -0 normalised to 0).
    size_t k = 0;
    bool neg = false;
    if (k < tok.size() && tok[k] == '-') { neg = true; k++; }
    if (k < tok.size()) {
        bool all_digits = true;
        for (size_t j = k; j < tok.size(); j++) {
            char c = tok[j];
            if (c < '0' || c > '9') { all_digits = false; break; }
        }
        if (all_digits) {
            uint64_t v = 0;
            bool overflow = false;
            const uint64_t lim = neg ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX;
            for (size_t j = k; j < tok.size(); j++) {
                int d = tok[j] - '0';
                if (v > (lim - (uint64_t)d) / 10) { overflow = true; break; }
                v = v * 10 + (uint64_t)d;
            }
            if (!overflow) {
                int64_t i = neg ? (v == 0 ? 0 : -(int64_t)(v - 1) - 1) : (int64_t)v;
                return Value::integer(i);
            }
            // overflow → fall through to string (preserve token verbatim)
        }
    }

    // Try float — strict ABNF:
    //   float = ["-"] 1*DIGIT [ "." 1*DIGIT ] [ ("e"/"E") ["+"/"-"] 1*DIGIT ]
    // Both fractional and exponent parts (if present) MUST have ≥1 digit.
    // Leading "+" is forbidden. Tokens like "5.", ".5", "+5", "1e", "1e+"
    // therefore fall through to plain-string per the type-priority cascade.
    auto looks_like_float = [&]() -> bool {
        size_t j = 0;
        if (j < tok.size() && tok[j] == '-') j++;
        size_t int_start = j;
        while (j < tok.size() && tok[j] >= '0' && tok[j] <= '9') j++;
        if (j == int_start) return false;
        bool has_frac_or_exp = false;
        if (j < tok.size() && tok[j] == '.') {
            j++;
            size_t frac_start = j;
            while (j < tok.size() && tok[j] >= '0' && tok[j] <= '9') j++;
            if (j == frac_start) return false;
            has_frac_or_exp = true;
        }
        if (j < tok.size() && (tok[j] == 'e' || tok[j] == 'E')) {
            j++;
            if (j < tok.size() && (tok[j] == '+' || tok[j] == '-')) j++;
            size_t exp_start = j;
            while (j < tok.size() && tok[j] >= '0' && tok[j] <= '9') j++;
            if (j == exp_start) return false;
            has_frac_or_exp = true;
        }
        return has_frac_or_exp && j == tok.size();
    };
    if (looks_like_float()) {
        std::string buf(tok);
        char* endp = nullptr;
        double dv = std::strtod(buf.c_str(), &endp);
        if (endp && *endp == '\0') return Value::floating(dv);
    }

    // Fallback: string. Apply plain-token escape unwrap (same set as decoder).
    std::string out;
    out.reserve(tok.size());
    for (size_t j = 0; j < tok.size(); j++) {
        char c = tok[j];
        if (c == '\\' && j + 1 < tok.size()) {
            char esc = tok[j + 1];
            switch (esc) {
                case 'n': out.push_back('\n'); j++; continue;
                case 'r': out.push_back('\r'); j++; continue;
                case 't': out.push_back('\t'); j++; continue;
                case 'b': out.push_back('\b'); j++; continue;
                case 'f': out.push_back('\f'); j++; continue;
                case '\\': out.push_back('\\'); j++; continue;
                case '"': out.push_back('"'); j++; continue;
                case ',': out.push_back(','); j++; continue;
                case '(': out.push_back('('); j++; continue;
                case ')': out.push_back(')'); j++; continue;
                case '[': out.push_back('['); j++; continue;
                case ']': out.push_back(']'); j++; continue;
                case '{': out.push_back('{'); j++; continue;
                case '}': out.push_back('}'); j++; continue;
                case '@': out.push_back('@'); j++; continue;
                case '<': out.push_back('<'); j++; continue;
                case '>': out.push_back('>'); j++; continue;
                case ':': out.push_back(':'); j++; continue;
                case 'u':
                    if (j + 5 < tok.size()) {
                        char hex[5] = {tok[j+2], tok[j+3], tok[j+4], tok[j+5], 0};
                        unsigned long cp = std::strtoul(hex, nullptr, 16);
                        if (cp < 0x80) out.push_back(static_cast<char>(cp));
                        else if (cp < 0x800) {
                            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        } else {
                            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        }
                        j += 5;
                        continue;
                    }
                    break;
                default: break;
            }
        }
        out.push_back(c);
    }
    return Value::string(std::move(out));
}

// Forward declaration for recursion.
inline Value parse_value_inner(const char*& pos, const char* end);

inline Value parse_array(const char*& pos, const char* end) {
    if (pos >= end || *pos != '[') throw Error("expected '['");
    pos++;
    std::vector<Value> items;
    bool first = true;
    for (;;) {
        skip_ws_strict(pos, end);
        if (pos >= end) throw Error("unterminated array");
        if (*pos == ']') { pos++; break; }
        if (!first) {
            if (*pos != ',') throw Error("expected ',' in array");
            pos++;
            skip_ws_strict(pos, end);
            if (pos >= end) throw Error("unterminated array");
            if (*pos == ']') { pos++; break; } // trailing comma
        }
        first = false;
        if (*pos == ',' || *pos == ']') {
            // sparse null: [,] / [1,,3]
            items.push_back(Value::null_());
            continue;
        }
        items.push_back(parse_value_inner(pos, end));
    }
    return Value::array(std::move(items));
}

inline Value parse_value_inner(const char*& pos, const char* end) {
    skip_ws_strict(pos, end);
    if (pos >= end) return Value::null_();
    char c = *pos;
    if (c == '[') return parse_array(pos, end);
    if (c == '"') return Value::string(detail::parse_quoted_string(pos, end));
    if (c == '(') {
        // Inside an array, only "()" is allowed (null marker). A non-empty
        // tuple at value position is invalid.
        pos++;
        skip_ws_strict(pos, end);
        if (pos < end && *pos == ')') { pos++; return Value::null_(); }
        throw Error("unexpected '(' in untyped value position");
    }

    // Plain token: scan until top-level structural delimiter.
    const char* start = pos;
    while (pos < end) {
        char b = *pos;
        if (b == ',' || b == ']' || b == ')' || b == '}') break;
        if (b == '\\' && pos + 1 < end) { pos += 2; continue; }
        pos++;
    }
    const char* tok_end = pos;
    while (tok_end > start && (tok_end[-1] == ' ' || tok_end[-1] == '\t' ||
                               tok_end[-1] == '\n' || tok_end[-1] == '\r')) tok_end--;
    while (start < tok_end && (*start == ' ' || *start == '\t' ||
                               *start == '\n' || *start == '\r')) start++;
    return classify_plain(std::string_view(start, tok_end - start));
}

} // namespace detail

inline Value decode_value(std::string_view input) {
    const char* pos = input.data();
    const char* end = pos + input.size();
    detail::skip_ws_strict(pos, end);
    if (pos >= end) return Value::null_();

    Value out;
    char c = *pos;
    if (c == '(') {
        // Top-level "()" is null; any other paren-form is a bare tuple → error.
        pos++;
        detail::skip_ws_strict(pos, end);
        if (pos < end && *pos == ')') { pos++; out = Value::null_(); }
        else throw Error("bare tuple is not a valid top-level value");
    } else if (c == '[') {
        out = detail::parse_array(pos, end);
    } else if (c == '"') {
        out = Value::string(detail::parse_quoted_string(pos, end));
    } else {
        // Top-level plain token: spec says trailing whitespace is part of the
        // trim, internal whitespace is preserved (e.g. "hello world" → string).
        const char* start = pos;
        while (pos < end) {
            char b = *pos;
            if (b == ',' || b == ']' || b == ')' || b == '}') break;
            if (b == '\\' && pos + 1 < end) { pos += 2; continue; }
            pos++;
        }
        const char* tok_end = pos;
        while (tok_end > start && (tok_end[-1] == ' ' || tok_end[-1] == '\t' ||
                                   tok_end[-1] == '\n' || tok_end[-1] == '\r')) tok_end--;
        while (start < tok_end && (*start == ' ' || *start == '\t' ||
                                   *start == '\n' || *start == '\r')) start++;
        out = detail::classify_plain(std::string_view(start, tok_end - start));
    }

    detail::skip_ws_strict(pos, end);
    if (pos < end) throw Error("trailing characters after value");
    return out;
}

// ============================================================================
// Untyped encode
// ============================================================================
namespace detail {

inline void encode_value_into(std::string& buf, const Value& v) {
    switch (v.tag) {
        case Value::Null:   buf.append("()"); return;
        case Value::Bool:   buf.append(v.b ? "true" : "false"); return;
        case Value::Int:    detail::append_i64(buf, v.i); return;
        case Value::Double: detail::append_f64(buf, v.d); return;
        case Value::String: detail::append_str(buf, v.s); return;
        case Value::Array:
            buf.push_back('[');
            for (size_t k = 0; k < v.arr.size(); k++) {
                if (k) buf.push_back(',');
                encode_value_into(buf, v.arr[k]);
            }
            buf.push_back(']');
            return;
    }
}

} // namespace detail

inline std::string encode_value(const Value& v) {
    std::string out;
    out.reserve(64);
    detail::encode_value_into(out, v);
    return out;
}

} // namespace asun

#endif // ASUN_VALUE_HPP
