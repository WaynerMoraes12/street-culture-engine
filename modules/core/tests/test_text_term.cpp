#include <doctest/doctest.h>

#include <vector>

#include "street/core/table.hpp"
#include "street/core/term.hpp"
#include "street/core/text.hpp"

namespace text = street::text;
namespace term = street::term;

TEST_CASE("trim, split and join") {
    CHECK(text::trim("  hoops \t\n") == "hoops");
    CHECK(text::trim("   ").empty());
    CHECK(text::split("a,b,,c", ',') == std::vector<std::string>{"a", "b", "", "c"});
    CHECK(text::split("a,b,,c", ',', true) == std::vector<std::string>{"a", "b", "c"});
    CHECK(text::split_lines("one\r\ntwo\n\nthree\n") ==
          std::vector<std::string>{"one", "two", "", "three"});
    CHECK(text::split_lines("").empty());
    CHECK(text::join({"kick", "snare", "hat"}, " | ") == "kick | snare | hat");
}

TEST_CASE("case conversion understands Portuguese accents") {
    CHECK(text::to_lower("CORAÇÃO É OURO") == "coração é ouro");
    CHECK(text::to_upper("coração é ouro") == "CORAÇÃO É OURO");
    CHECK(text::iequals("TrAp", "trap"));
    CHECK_FALSE(text::iequals("trap", "traps"));
    CHECK(text::starts_with_ci("Boom-Bap", "boom"));
}

TEST_CASE("fold_accents strips diacritics") {
    CHECK(text::fold_accents("ação, pé, você, vovô, tênis, Straße") ==
          "acao, pe, voce, vovo, tenis, Strasse");
    CHECK(text::fold_accents("plain ascii") == "plain ascii");
}

TEST_CASE("utf8 round trip and length") {
    const std::string s = "tênis ▁▂█ ok";
    const auto decoded = text::utf8_decode(s);
    CHECK(decoded.size() == 12);
    CHECK(text::utf8_encode(decoded) == s);
    CHECK(text::utf8_length(s) == 12);
    CHECK(text::utf8_decode("\xFF").front() == U'�');
    CHECK(text::utf8_decode("\xC3").front() == U'�'); // truncated sequence
}

TEST_CASE("slug makes file-safe names") {
    CHECK(text::slug("Air Jordan 1 'Chicago'") == "air-jordan-1-chicago");
    CHECK(text::slug("  São Paulo Funk!! ") == "sao-paulo-funk");
}

TEST_CASE("number formatting") {
    CHECK(text::thousands(0) == "0");
    CHECK(text::thousands(999) == "999");
    CHECK(text::thousands(1000) == "1,000");
    CHECK(text::thousands(1234567) == "1,234,567");
    CHECK(text::thousands(-12345) == "-12,345");
    CHECK(text::money(17000) == "$170.00");
    CHECK(text::money(123456789) == "$1,234,567.89");
    CHECK(text::money(-505) == "-$5.05");
    CHECK(text::percent(0.1234) == "12.3%");
    CHECK(text::signed_percent(0.5) == "+50.0%");
    CHECK(text::signed_percent(-0.025, 2) == "-2.50%");
    CHECK(text::signed_percent(-0.00001) == "+0.0%");
    CHECK(text::fixed(3.14159, 2) == "3.14");
}

TEST_CASE("read_file reports missing files") {
    CHECK_THROWS_AS(text::read_file("definitely/not/here.txt"), std::runtime_error);
}

TEST_CASE("terminal width ignores ANSI escapes and counts code points") {
    term::set_color_enabled(true);
    const std::string colored = term::paint("tênis", term::palette::red);
    CHECK(colored != "tênis");
    CHECK(term::width(colored) == 5);
    CHECK(term::strip_ansi(colored) == "tênis");
    CHECK(term::width(term::pad_right(colored, 8)) == 8);
    CHECK(term::width(term::center("ab", 7)) == 7);
    term::set_color_enabled(false);
    CHECK(term::paint("plain", term::palette::red) == "plain");
    CHECK(term::gradient("abc", term::palette::red, term::palette::blue) == "abc");
}

TEST_CASE("truncate, repeat, meter and sparkline") {
    term::set_color_enabled(false);
    CHECK(term::truncate("crossover", 5) == "cros…");
    CHECK(term::truncate("short", 10) == "short");
    CHECK(term::repeat("ab", 3) == "ababab");
    CHECK(term::width(term::meter(0.5, 10)) == 10);
    CHECK(term::meter(1.0, 4) == "████");
    CHECK(term::meter(0.0, 3) == "   ");
    const std::vector<double> values = {1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(term::sparkline(values) == "▁▂▃▄▅▆▇█");
    const std::vector<double> flat = {2, 2, 2};
    CHECK(term::sparkline(flat) == "▄▄▄");
    CHECK(term::sparkline({}).empty());
}

TEST_CASE("panel and rule have consistent widths") {
    term::set_color_enabled(false);
    const std::vector<std::string> lines = {"line one", "a much longer line of text"};
    const auto box = text::split_lines(term::panel("Title", lines));
    REQUIRE(box.size() == 4);
    for (const auto& l : box) CHECK(term::width(l) == term::width(box.front()));
    CHECK(term::width(term::rule(30, "Stats")) == 30);
    CHECK(term::width(term::rule(12)) == 12);
}

TEST_CASE("table aligns columns and keeps every line the same width") {
    term::set_color_enabled(false);
    street::Table table({"Player", "PTS", "REB"});
    table.align_right_from(1)
        .add_row({"Ace Mercer", "31", "7"})
        .add_separator()
        .add_row({"J. 'Flash' Dávila", "9", "12"});
    CHECK(table.rows() == 2);
    const auto lines = text::split_lines(table.render());
    REQUIRE(lines.size() == 7);
    for (const auto& l : lines) CHECK(term::width(l) == term::width(lines.front()));
    CHECK(lines[3].find("│  31 │") != std::string::npos); // right aligned under "PTS"
}
