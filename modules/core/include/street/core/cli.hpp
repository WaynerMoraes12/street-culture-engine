#pragma once
// Declarative command-line parsing with auto-generated help.
//
//   street::cli::Command cmd{"beat", "Generate a hip-hop beat"};
//   cmd.option("bpm", "N", "Tempo in beats per minute", "90")
//      .flag("loop", "Render a seamless loop");
//   auto args = cmd.parse(tokens);            // throws street::cli::UsageError
//   if (args.flag("help")) { std::cout << cmd.help(); return 0; }
//   int bpm = static_cast<int>(args.integer("bpm"));
//
// Accepted syntax: --name value, --name=value, --flag, -h, and positionals.
// A bare "--" ends option parsing.

#include <cstdint>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace street::cli {

struct UsageError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Parsed {
public:
    bool flag(std::string_view name) const;
    /// True if the option or flag was explicitly present on the command line.
    bool given(std::string_view name) const;
    /// Value of an option, or its declared default when absent.
    const std::string& str(std::string_view name) const;
    long long integer(std::string_view name) const;          ///< throws UsageError
    double number(std::string_view name) const;              ///< throws UsageError
    std::uint64_t unsigned_integer(std::string_view name) const; ///< throws UsageError
    const std::vector<std::string>& positional() const noexcept { return positional_; }

private:
    friend class Command;
    std::map<std::string, std::string, std::less<>> values_;
    std::map<std::string, bool, std::less<>> flags_;
    std::map<std::string, bool, std::less<>> given_;
    std::vector<std::string> positional_;
};

class Command {
public:
    Command(std::string name, std::string summary);

    Command& option(std::string name, std::string value_hint, std::string help,
                    std::string default_value = {});
    Command& flag(std::string name, std::string help);
    Command& positional(std::string name, std::string help);
    Command& example(std::string command_line, std::string explanation = {});

    Parsed parse(std::span<const std::string> args) const;
    std::string help() const;

    const std::string& name() const noexcept { return name_; }
    const std::string& summary() const noexcept { return summary_; }

private:
    struct Spec {
        std::string name, hint, help, default_value;
        bool is_flag = false;
    };
    const Spec* find(std::string_view name) const;

    std::string name_, summary_;
    std::vector<Spec> specs_;
    std::vector<std::pair<std::string, std::string>> positionals_;
    std::vector<std::pair<std::string, std::string>> examples_;
};

} // namespace street::cli
