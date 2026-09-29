#include "commands.hpp"
#include "street/beats/beats.hpp"
#include "street/core/cli.hpp"
#include "street/core/rng.hpp"
#include "street/core/stopwatch.hpp"
#include "street/core/term.hpp"
#include "street/core/text.hpp"

#include <filesystem>
#include <format>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace street::app {
namespace {

constexpr std::size_t kWidth = 92;

cli::Command beat_command() {
    cli::Command cmd{"beat", "Synthesize a hip-hop beat from scratch and export it as WAV"};
    cmd.option("style", "S", beats::style_list(" | "), "boombap")
        .option("bpm", "N", "Tempo 40-300 (default: seeded, within the style's range)")
        .option("bars", "N", "Length in bars, 1-256", "8")
        .option("key", "K", "Key such as Am, C#m, F, Ebm, Bb (default: seeded per style)")
        .option("swing", "X", "0.5 straight .. 0.66 triplet feel, or 50-80 as a percentage")
        .option("seed", "N", "Random seed (default: random; always printed so you can replay it)")
        .option("out", "FILE", "Output WAV (default: beat-<style>-<seed>.wav)")
        .option("pattern", "FILE",
                "Render a groove file (its style line picks the kit; see examples/patterns)")
        .flag("list-styles", "Show the available styles and exit")
        .flag("dry-run", "Render and visualise, but don't write the WAV")
        .example("street beat --style trap --seed 42", "808s, hat rolls, 140-ish BPM, reproducible")
        .example("street beat --style lofi --key Ebm --bars 16",
                 "a longer lo-fi loop in E-flat minor")
        .example("street beat --pattern examples/patterns/sunday-stoop.groove --style lofi",
                 "your own groove through the lo-fi kit")
        .example("street beat --style gallop --key F#m",
                 "an NWOBHM gallop: power chords at 170-ish BPM")
        .example("street beat --style funk --dry-run", "preview without writing a file");
    return cmd;
}

beats::Style style_option(const cli::Parsed& args) {
    const auto style = beats::parse_style(args.str("style"));
    if (!style)
        throw cli::UsageError(
            std::format("unknown style '{}' (choose {})", args.str("style"), beats::style_list()));
    return *style;
}

double swing_option(const cli::Parsed& args) {
    double swing = args.number("swing");
    if (swing > 1.0) swing /= 100.0; // "--swing 62" means 62%
    if (swing < 0.5 || swing > 0.8)
        throw cli::UsageError(
            std::format("--swing must be 0.5-0.8 (or 50-80), got '{}'", args.str("swing")));
    return swing;
}

beats::BeatSpec spec_from(const cli::Parsed& args) {
    beats::BeatSpec spec;
    spec.style = style_option(args);
    spec.seed = args.given("seed") ? args.unsigned_integer("seed") : random_seed();
    const long long bars = args.integer("bars");
    if (bars < 1 || bars > 256)
        throw cli::UsageError(std::format("--bars must be 1-256, got {}", bars));
    spec.bars = static_cast<int>(bars);
    if (args.given("bpm")) {
        const long long bpm = args.integer("bpm");
        if (bpm < 40 || bpm > 300)
            throw cli::UsageError(std::format("--bpm must be 40-300, got {}", bpm));
        spec.bpm = static_cast<int>(bpm);
    }
    if (args.given("key")) {
        const auto key = beats::parse_key(args.str("key"));
        if (!key)
            throw cli::UsageError(
                std::format("unknown key '{}' (try Am, C#m, F, Ebm, Bb)", args.str("key")));
        spec.key = key->name();
    }
    if (args.given("swing")) spec.swing = swing_option(args);
    return spec;
}

beats::Beat pattern_beat(const std::filesystem::path& path, beats::BeatSpec spec,
                         bool style_given) {
    const std::string text = street::text::read_file(path);
    beats::Pattern pattern;
    try {
        pattern = beats::parse_pattern(text);
    } catch (const std::invalid_argument& e) {
        throw std::runtime_error(std::format("{}: {}", path.string(), e.what()));
    }
    if (pattern.title.empty()) spec.title = path.stem().string();
    if (pattern.style && !style_given) spec.style = *pattern.style; // the file's suggested kit
    return beats::from_pattern(std::move(pattern), spec);
}

std::string clock(double seconds) {
    const int whole = static_cast<int>(seconds);
    return std::format("{}:{:02}", whole / 60, whole % 60);
}

std::string time_axis(const beats::AudioBuffer& audio) {
    using namespace term;
    const std::string left = "  " + clock(0.0);
    const std::string right = clock(audio.seconds());
    const std::string middle = std::format("peak {:.1f} dBFS · loudness {:.1f} dBFS RMS",
                                           beats::peak_dbfs(audio), beats::rms_dbfs(audio));
    const std::size_t room = kWidth - width(left) - width(right);
    const std::size_t pad = room > width(middle) ? room - width(middle) : 2;
    return paint(left + std::string(pad / 2, ' ') + middle + std::string(pad - pad / 2, ' ') +
                     right,
                 palette::gray) +
           '\n';
}

} // namespace

int cmd_beat(Args args) {
    using namespace term;
    const cli::Command cmd = beat_command();
    const cli::Parsed parsed = cmd.parse(args);
    if (parsed.flag("help")) {
        std::cout << cmd.help();
        return 0;
    }
    if (parsed.flag("list-styles")) {
        std::cout << beats::render_style_list();
        return 0;
    }
    if (!parsed.positional().empty())
        throw cli::UsageError(std::format("unexpected argument '{}'", parsed.positional().front()));

    const beats::BeatSpec spec = spec_from(parsed);
    const bool from_file = parsed.given("pattern");
    const Stopwatch clock_render;
    const beats::Beat beat = from_file
                                 ? pattern_beat(parsed.str("pattern"), spec, parsed.given("style"))
                                 : beats::generate(spec);
    const double render_seconds = clock_render.seconds();

    std::filesystem::path out = parsed.str("out");
    if (out.empty()) {
        const std::string stem =
            from_file
                ? street::text::slug(std::filesystem::path(parsed.str("pattern")).stem().string())
                : std::string(beats::info(beat.spec.style).id);
        out = std::format("beat-{}-{}.wav", stem, beat.spec.seed);
    }

    std::cout << '\n' << beats::render_summary(beat) << '\n';
    std::cout << rule(kWidth, "STEP SEQUENCER", palette::dark) << "\n\n"
              << beats::render_grid(beat.pattern) << '\n';
    std::cout << rule(kWidth, "WAVEFORM", palette::dark) << "\n\n";
    for (const auto& line : street::text::split_lines(
             beats::render_waveform(beat.audio, static_cast<int>(kWidth) - 2, 5)))
        std::cout << "  " << line << '\n';
    std::cout << time_axis(beat.audio) << '\n';

    const std::size_t bytes = beats::wav_size(beat.audio.frames());
    const std::string facts =
        std::format("{} bars · {:.1f} s · {:.1f} MB · rendered in {:.2f} s", beat.spec.bars,
                    beat.audio.seconds(), static_cast<double>(bytes) / 1e6, render_seconds);
    if (parsed.flag("dry-run")) {
        std::cout << paint_bold("✓ ", palette::green) << "dry run, nothing written"
                  << paint(" · " + facts, palette::gray) << '\n';
    } else {
        if (out.has_parent_path()) std::filesystem::create_directories(out.parent_path());
        beats::write_wav(out, beat.audio);
        std::cout << paint_bold("✓ ", palette::green) << "wrote "
                  << paint_bold(out.string(), palette::white) << paint(" · " + facts, palette::gray)
                  << '\n';
    }
    std::string replay = std::format("street beat --style {} --seed {}",
                                     beats::info(beat.spec.style).id, beat.spec.seed);
    if (from_file) replay += " --pattern " + parsed.str("pattern");
    for (const char* option : {"bpm", "bars", "key", "swing"})
        if (parsed.given(option)) replay += std::format(" --{} {}", option, parsed.str(option));
    std::cout << paint("  replay: " + replay + " (same seed, same beat)", palette::dark) << '\n';
    return 0;
}

} // namespace street::app
