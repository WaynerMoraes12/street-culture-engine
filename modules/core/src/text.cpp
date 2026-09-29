#include "street/core/text.hpp"

#include <cmath>
#include <format>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace street::text {
namespace {

bool is_space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
char ascii_lower(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}
char ascii_upper(char c) noexcept {
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}

// ASCII spelling of U+00C0 .. U+00FF (encoded in UTF-8 as 0xC3 0x80 .. 0xC3 0xBF).
constexpr std::string_view kFolded[64] = {
    "A", "A", "A", "A", "A", "A", "AE", "C",  // À Á Â Ã Ä Å Æ Ç
    "E", "E", "E", "E", "I", "I", "I",  "I",  // È É Ê Ë Ì Í Î Ï
    "D", "N", "O", "O", "O", "O", "O",  "x",  // Ð Ñ Ò Ó Ô Õ Ö ×
    "O", "U", "U", "U", "U", "Y", "TH", "ss", // Ø Ù Ú Û Ü Ý Þ ß
    "a", "a", "a", "a", "a", "a", "ae", "c",  // à á â ã ä å æ ç
    "e", "e", "e", "e", "i", "i", "i",  "i",  // è é ê ë ì í î ï
    "d", "n", "o", "o", "o", "o", "o",  "/",  // ð ñ ò ó ô õ ö ÷
    "o", "u", "u", "u", "u", "y", "th", "y",  // ø ù ú û ü ý þ ÿ
};

constexpr unsigned char kLatin1Lead = 0xC3;

} // namespace

std::string_view trim(std::string_view s) noexcept {
    std::size_t begin = 0;
    std::size_t end = s.size();
    while (begin < end && is_space(s[begin])) ++begin;
    while (end > begin && is_space(s[end - 1])) --end;
    return s.substr(begin, end - begin);
}

std::vector<std::string> split(std::string_view s, char delimiter, bool skip_empty) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t pos = s.find(delimiter, start);
        const std::string_view piece =
            s.substr(start, pos == std::string_view::npos ? std::string_view::npos : pos - start);
        if (!(skip_empty && piece.empty())) parts.emplace_back(piece);
        if (pos == std::string_view::npos) break;
        start = pos + 1;
    }
    return parts;
}

std::vector<std::string> split_lines(std::string_view s) {
    std::vector<std::string> lines;
    if (s.empty()) return lines;
    lines = split(s, '\n');
    if (s.back() == '\n') lines.pop_back();
    for (auto& line : lines)
        if (!line.empty() && line.back() == '\r') line.pop_back();
    return lines;
}

std::string join(const std::vector<std::string>& parts, std::string_view separator) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) out += separator;
        out += parts[i];
    }
    return out;
}

bool starts_with_ci(std::string_view s, std::string_view prefix) noexcept {
    return s.size() >= prefix.size() && iequals(s.substr(0, prefix.size()), prefix);
}

bool iequals(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (ascii_lower(a[i]) != ascii_lower(b[i])) return false;
    return true;
}

std::string to_lower(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        const auto byte = static_cast<unsigned char>(s[i]);
        if (byte == kLatin1Lead && i + 1 < s.size()) {
            const auto next = static_cast<unsigned char>(s[i + 1]);
            out += s[i];
            // U+00C0..U+00DE are capitals (except × at U+00D7); lowercase is +0x20.
            out += (next >= 0x80 && next <= 0x9E && next != 0x97) ? static_cast<char>(next + 0x20)
                                                                  : s[i + 1];
            ++i;
        } else {
            out += ascii_lower(s[i]);
        }
    }
    return out;
}

std::string to_upper(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        const auto byte = static_cast<unsigned char>(s[i]);
        if (byte == kLatin1Lead && i + 1 < s.size()) {
            const auto next = static_cast<unsigned char>(s[i + 1]);
            out += s[i];
            out += (next >= 0xA0 && next <= 0xBE && next != 0xB7) ? static_cast<char>(next - 0x20)
                                                                  : s[i + 1];
            ++i;
        } else {
            out += ascii_upper(s[i]);
        }
    }
    return out;
}

std::string fold_accents(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        const auto byte = static_cast<unsigned char>(s[i]);
        if (byte == kLatin1Lead && i + 1 < s.size()) {
            const auto next = static_cast<unsigned char>(s[i + 1]);
            if (next >= 0x80 && next <= 0xBF) {
                out += kFolded[next - 0x80];
                ++i;
                continue;
            }
        }
        out += s[i];
    }
    return out;
}

void utf8_append(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp <= 0x10FFFF) {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        utf8_append(out, U'�');
    }
}

std::u32string utf8_decode(std::string_view s) {
    std::u32string out;
    out.reserve(s.size());
    std::size_t i = 0;
    while (i < s.size()) {
        const auto b0 = static_cast<unsigned char>(s[i]);
        int extra = 0;
        char32_t cp = 0;
        if (b0 < 0x80) {
            cp = b0;
        } else if ((b0 & 0xE0) == 0xC0) {
            cp = b0 & 0x1F;
            extra = 1;
        } else if ((b0 & 0xF0) == 0xE0) {
            cp = b0 & 0x0F;
            extra = 2;
        } else if ((b0 & 0xF8) == 0xF0) {
            cp = b0 & 0x07;
            extra = 3;
        } else {
            out += U'�';
            ++i;
            continue;
        }
        if (i + static_cast<std::size_t>(extra) >= s.size()) { // truncated sequence
            out += U'�';
            ++i;
            continue;
        }
        bool valid = true;
        for (int k = 1; k <= extra; ++k) {
            const auto bk = static_cast<unsigned char>(s[i + static_cast<std::size_t>(k)]);
            if ((bk & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            cp = (cp << 6) | (bk & 0x3F);
        }
        if (!valid) {
            out += U'�';
            ++i;
            continue;
        }
        out += cp;
        i += static_cast<std::size_t>(extra) + 1;
    }
    return out;
}

std::string utf8_encode(std::u32string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char32_t cp : s) utf8_append(out, cp);
    return out;
}

std::size_t utf8_length(std::string_view s) noexcept {
    std::size_t n = 0;
    for (char c : s)
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++n;
    return n;
}

std::string slug(std::string_view s) {
    const std::string folded = to_lower(fold_accents(s));
    std::string out;
    for (char c : folded) {
        const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (alnum) {
            out += c;
        } else if (!out.empty() && out.back() != '-') {
            out += '-';
        }
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out;
}

std::string thousands(std::int64_t value) {
    const bool negative = value < 0;
    // Work in unsigned space so INT64_MIN does not overflow on negation.
    const std::uint64_t magnitude =
        negative ? 0ull - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
    const std::string digits = std::to_string(magnitude);
    std::string out;
    const std::size_t lead = digits.size() % 3;
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (i != 0 && i % 3 == lead) out += ',';
        out += digits[i];
    }
    return negative ? "-" + out : out;
}

std::string money(std::int64_t cents) {
    const bool negative = cents < 0;
    const std::uint64_t magnitude =
        negative ? 0ull - static_cast<std::uint64_t>(cents) : static_cast<std::uint64_t>(cents);
    const auto dollars = static_cast<std::int64_t>(magnitude / 100);
    return std::format("{}${}.{:02}", negative ? "-" : "", thousands(dollars), magnitude % 100);
}

std::string fixed(double value, int decimals) {
    const double tiny = 0.5 * std::pow(10.0, -decimals);
    if (std::abs(value) < tiny) value = 0.0; // avoid "-0.0"
    return std::format("{:.{}f}", value, decimals);
}

std::string percent(double fraction, int decimals) { return fixed(fraction * 100.0, decimals) + "%"; }

std::string signed_percent(double fraction, int decimals) {
    const std::string body = fixed(fraction * 100.0, decimals);
    return (body.front() == '-' ? body : "+" + body) + "%";
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error(std::format("cannot open '{}'", path.string()));
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

} // namespace street::text
