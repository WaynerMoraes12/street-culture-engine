#pragma once
// UTF-8 aware string helpers and human-friendly number formatting.

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace street::text {

std::string_view trim(std::string_view s) noexcept;
std::vector<std::string> split(std::string_view s, char delimiter, bool skip_empty = false);
/// Splits on '\n', dropping a trailing '\r' from each line (handles CRLF files).
std::vector<std::string> split_lines(std::string_view s);
std::string join(const std::vector<std::string>& parts, std::string_view separator);

bool starts_with_ci(std::string_view s, std::string_view prefix) noexcept;
bool iequals(std::string_view a, std::string_view b) noexcept; ///< ASCII case-insensitive

/// Lowercases ASCII and Latin-1 accented capitals ("ÇÃO" -> "ção").
std::string to_lower(std::string_view s);
/// Uppercases ASCII and Latin-1 accented letters ("ção" -> "ÇÃO").
std::string to_upper(std::string_view s);
/// Replaces Latin-1 accented letters with their ASCII base ("ação" -> "acao", "ß" -> "ss").
std::string fold_accents(std::string_view s);

std::u32string utf8_decode(std::string_view s);    ///< invalid bytes become U+FFFD
std::string utf8_encode(std::u32string_view s);
void utf8_append(std::string& out, char32_t cp);
std::size_t utf8_length(std::string_view s) noexcept; ///< number of code points

/// "air jordan 1 'chicago'" -> "air-jordan-1-chicago" — safe for file names.
std::string slug(std::string_view s);

std::string thousands(std::int64_t value);            ///< 1234567 -> "1,234,567"
std::string money(std::int64_t cents);                ///< 123456 -> "$1,234.56"
std::string percent(double fraction, int decimals = 1);         ///< 0.123 -> "12.3%"
std::string signed_percent(double fraction, int decimals = 1);  ///< 0.123 -> "+12.3%"
std::string fixed(double value, int decimals);                  ///< 3.14159, 2 -> "3.14"

/// Reads a whole file as bytes. Throws std::runtime_error with the path on failure.
std::string read_file(const std::filesystem::path& path);

} // namespace street::text
