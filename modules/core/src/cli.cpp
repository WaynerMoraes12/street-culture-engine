#include "street/core/cli.hpp"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <format>

#include "street/core/term.hpp"

namespace street::cli {
namespace {

template <class Map>
auto& lookup(Map& map, std::string_view name, std::string_view kind) {
    const auto it = map.find(name);
    if (it == map.end())
        throw std::logic_error(std::format("{} '--{}' was never declared", kind, name));
    return it->second;
}

bool looks_like_number(std::string_view s) {
    if (s.size() < 2 || s[0] != '-') return false;
    return std::all_of(s.begin() + 1, s.end(), [](char c) { return (c >= '0' && c <= '9') || c == '.'; });
}

} // namespace

bool Parsed::flag(std::string_view name) const { return lookup(flags_, name, "flag"); }

bool Parsed::given(std::string_view name) const {
    const auto it = given_.find(name);
    return it != given_.end() && it->second;
}

const std::string& Parsed::str(std::string_view name) const { return lookup(values_, name, "option"); }

long long Parsed::integer(std::string_view name) const {
    const std::string& v = str(name);
    long long out = 0;
    const auto [ptr, ec] = std::from_chars(v.data(), v.data() + v.size(), out);
    if (ec != std::errc{} || ptr != v.data() + v.size() || v.empty())
        throw UsageError(std::format("--{} expects an integer, got '{}'", name, v));
    return out;
}

std::uint64_t Parsed::unsigned_integer(std::string_view name) const {
    const std::string& v = str(name);
    std::uint64_t out = 0;
    const auto [ptr, ec] = std::from_chars(v.data(), v.data() + v.size(), out);
    if (ec != std::errc{} || ptr != v.data() + v.size() || v.empty())
        throw UsageError(std::format("--{} expects a non-negative integer, got '{}'", name, v));
    return out;
}

double Parsed::number(std::string_view name) const {
    const std::string& v = str(name);
    // strtod instead of from_chars<double>: the latter is still missing from some libc++ builds.
    char* end = nullptr;
    errno = 0;
    const double out = std::strtod(v.c_str(), &end);
    if (v.empty() || end != v.c_str() + v.size() || errno == ERANGE)
        throw UsageError(std::format("--{} expects a number, got '{}'", name, v));
    return out;
}

Command::Command(std::string name, std::string summary)
    : name_(std::move(name)), summary_(std::move(summary)) {
    specs_.push_back({"help", "", "Show this help", "", true});
}

Command& Command::option(std::string name, std::string value_hint, std::string help,
                         std::string default_value) {
    specs_.push_back({std::move(name), std::move(value_hint), std::move(help),
                      std::move(default_value), false});
    return *this;
}

Command& Command::flag(std::string name, std::string help) {
    specs_.push_back({std::move(name), "", std::move(help), "", true});
    return *this;
}

Command& Command::positional(std::string name, std::string help) {
    positionals_.emplace_back(std::move(name), std::move(help));
    return *this;
}

Command& Command::example(std::string command_line, std::string explanation) {
    examples_.emplace_back(std::move(command_line), std::move(explanation));
    return *this;
}

const Command::Spec* Command::find(std::string_view name) const {
    const auto it = std::find_if(specs_.begin(), specs_.end(), [&](const Spec& s) { return s.name == name; });
    return it == specs_.end() ? nullptr : &*it;
}

Parsed Command::parse(std::span<const std::string> args) const {
    Parsed p;
    for (const auto& spec : specs_) {
        if (spec.is_flag)
            p.flags_[spec.name] = false;
        else
            p.values_[spec.name] = spec.default_value;
    }

    bool options_done = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& token = args[i];
        if (!options_done && token == "--") {
            options_done = true;
            continue;
        }
        if (!options_done && token == "-h") {
            p.flags_["help"] = true;
            p.given_["help"] = true;
            continue;
        }
        if (!options_done && token.size() > 2 && token.starts_with("--")) {
            const std::string_view body = std::string_view(token).substr(2);
            const std::size_t eq = body.find('=');
            const std::string name{body.substr(0, eq)};
            const Spec* spec = find(name);
            if (spec == nullptr)
                throw UsageError(std::format("unknown option '--{}' (see 'street {} --help')", name, name_));
            if (spec->is_flag) {
                if (eq != std::string_view::npos)
                    throw UsageError(std::format("flag '--{}' does not take a value", name));
                p.flags_[name] = true;
            } else if (eq != std::string_view::npos) {
                p.values_[name] = std::string(body.substr(eq + 1));
            } else if (i + 1 < args.size() && !args[i + 1].starts_with("--")) {
                p.values_[name] = args[++i];
            } else {
                throw UsageError(std::format("option '--{}' expects a value <{}>", name, spec->hint));
            }
            p.given_[name] = true;
            continue;
        }
        if (!options_done && token.size() > 1 && token[0] == '-' && !looks_like_number(token))
            throw UsageError(std::format("unknown option '{}' (see 'street {} --help')", token, name_));
        p.positional_.push_back(token);
    }
    return p;
}

std::string Command::help() const {
    using namespace term;
    const auto heading = [](std::string_view s) { return paint_bold(s, palette::gold) + '\n'; };

    std::string out;
    out += emphasize("street " + name_) + paint(" — " + summary_, palette::gray) + "\n\n";

    out += heading("USAGE");
    std::string usage = "  street " + name_ + " [options]";
    for (const auto& [pname, phelp] : positionals_) usage += " [" + pname + "]";
    out += usage + "\n\n";

    if (!positionals_.empty()) {
        out += heading("ARGUMENTS");
        for (const auto& [pname, phelp] : positionals_)
            out += "  " + pad_right(paint(pname, palette::cyan), 24) + phelp + '\n';
        out += '\n';
    }

    out += heading("OPTIONS");
    std::vector<const Spec*> ordered;
    for (const auto& s : specs_)
        if (s.name != "help") ordered.push_back(&s);
    ordered.push_back(&specs_.front());
    for (const Spec* s : ordered) {
        std::string lhs = s->name == "help" ? "-h, --help" : "--" + s->name;
        if (!s->is_flag) lhs += " <" + s->hint + ">";
        std::string rhs = s->help;
        if (!s->is_flag && !s->default_value.empty())
            rhs += paint(" (default: " + s->default_value + ")", palette::gray);
        const std::size_t lw = std::max<std::size_t>(24, width(lhs) + 2);
        out += "  " + paint(pad_right(lhs, lw), palette::cyan) + rhs + '\n';
    }

    if (!examples_.empty()) {
        out += '\n' + heading("EXAMPLES");
        for (const auto& [line, why] : examples_) {
            out += "  " + paint("$ ", palette::gray) + line + '\n';
            if (!why.empty()) out += "      " + paint(why, palette::gray) + '\n';
        }
    }
    return out;
}

} // namespace street::cli
