#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "street/core/cli.hpp"
#include "street/core/term.hpp"

using street::cli::Command;
using street::cli::UsageError;

namespace {

Command beat_command() {
    Command cmd{"beat", "Generate a hip-hop beat"};
    cmd.option("style", "name", "Beat style", "boombap")
        .option("bpm", "N", "Tempo", "90")
        .option("swing", "0-1", "Swing amount", "0.55")
        .option("seed", "N", "Random seed")
        .flag("loop", "Render a seamless loop")
        .positional("out", "Output file")
        .example("street beat --style trap", "a trap beat");
    return cmd;
}

std::vector<std::string> v(std::initializer_list<const char*> items) {
    return {items.begin(), items.end()};
}

} // namespace

TEST_CASE("defaults apply when options are absent") {
    const auto cmd = beat_command();
    const auto args = cmd.parse({});
    CHECK(args.str("style") == "boombap");
    CHECK(args.integer("bpm") == 90);
    CHECK(args.number("swing") == doctest::Approx(0.55));
    CHECK_FALSE(args.flag("loop"));
    CHECK_FALSE(args.flag("help"));
    CHECK_FALSE(args.given("bpm"));
    CHECK(args.positional().empty());
}

TEST_CASE("space and equals syntaxes, flags and positionals") {
    const auto cmd = beat_command();
    const auto tokens = v({"--style", "trap", "--bpm=140", "--loop", "out.wav", "--seed", "7"});
    const auto args = cmd.parse(tokens);
    CHECK(args.str("style") == "trap");
    CHECK(args.integer("bpm") == 140);
    CHECK(args.flag("loop"));
    CHECK(args.given("bpm"));
    CHECK(args.unsigned_integer("seed") == 7);
    REQUIRE(args.positional().size() == 1);
    CHECK(args.positional()[0] == "out.wav");
}

TEST_CASE("help flag in both spellings") {
    const auto cmd = beat_command();
    CHECK(cmd.parse(v({"-h"})).flag("help"));
    CHECK(cmd.parse(v({"--help"})).flag("help"));
}

TEST_CASE("double dash ends option parsing") {
    const auto cmd = beat_command();
    const auto args = cmd.parse(v({"--", "--loop", "-x"}));
    CHECK_FALSE(args.flag("loop"));
    CHECK(args.positional() == std::vector<std::string>{"--loop", "-x"});
}

TEST_CASE("negative numbers are accepted as values") {
    const auto cmd = beat_command();
    const auto args = cmd.parse(v({"--bpm", "-5"}));
    CHECK(args.integer("bpm") == -5);
}

TEST_CASE("usage errors are descriptive") {
    const auto cmd = beat_command();
    CHECK_THROWS_AS(cmd.parse(v({"--nope"})), UsageError);
    CHECK_THROWS_AS(cmd.parse(v({"-x"})), UsageError);
    CHECK_THROWS_AS(cmd.parse(v({"--bpm"})), UsageError);
    CHECK_THROWS_AS(cmd.parse(v({"--bpm", "--loop"})), UsageError);
    CHECK_THROWS_AS(cmd.parse(v({"--loop=yes"})), UsageError);
    CHECK_THROWS_AS(cmd.parse(v({"--bpm", "fast"})).integer("bpm"), UsageError);
    CHECK_THROWS_AS(cmd.parse(v({"--swing", "lots"})).number("swing"), UsageError);
    CHECK_THROWS_AS(cmd.parse(v({"--seed", "-1"})).unsigned_integer("seed"), UsageError);
    try {
        (void)cmd.parse(v({"--stlye", "trap"}));
        FAIL("expected UsageError");
    } catch (const UsageError& e) {
        CHECK(std::string(e.what()).find("--stlye") != std::string::npos);
    }
}

TEST_CASE("undeclared names are programming errors, not usage errors") {
    const auto args = beat_command().parse({});
    CHECK_THROWS_AS((void)args.str("tempo"), std::logic_error);
    CHECK_THROWS_AS((void)args.flag("verbose"), std::logic_error);
}

TEST_CASE("generated help lists options, defaults and examples") {
    street::term::set_color_enabled(false);
    const std::string help = beat_command().help();
    CHECK(help.find("street beat") != std::string::npos);
    CHECK(help.find("--style <name>") != std::string::npos);
    CHECK(help.find("(default: boombap)") != std::string::npos);
    CHECK(help.find("--loop") != std::string::npos);
    CHECK(help.find("-h, --help") != std::string::npos);
    CHECK(help.find("$ street beat --style trap") != std::string::npos);
}
