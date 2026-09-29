#include "street/bars/render.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <vector>

#include "street/bars/lexicon.hpp"
#include "street/bars/phonetics.hpp"
#include "street/core/term.hpp"
#include "street/core/text.hpp"

namespace street::bars {
namespace {

using term::Rgb;
namespace palette = term::palette;

// Distinct hues for rhyme families, cycled when a verse has more families than colours.
constexpr Rgb kFamilyColors[] = {
    palette::gold,  palette::pink, palette::cyan,  palette::green,  palette::orange,
    palette::purple, {163, 230, 53}, palette::red, {125, 211, 252}, palette::teal,
    {250, 150, 200}, palette::hardwood, palette::blue, {255, 236, 140},
};

Rgb family_color(int group) {
    if (group < 0) return palette::gray;
    constexpr auto n = static_cast<int>(std::size(kFamilyColors));
    return kFamilyColors[group % n];
}

constexpr std::size_t kMeterCells = 6;
constexpr double kMeterFull = 16.0; ///< syllables per bar that fill the meter

/// Wraps space-separated chunks into lines of at most `first` cells (then `rest` cells
/// for continuation lines). `styled` holds the same chunks with colour codes applied, so
/// widths are measured on the plain text.
std::vector<std::string> wrap(const std::vector<std::string>& plain,
                              const std::vector<std::string>& styled, std::size_t first,
                              std::size_t rest) {
    std::vector<std::string> lines;
    std::string current;
    std::size_t used = 0;
    for (std::size_t i = 0; i < plain.size(); ++i) {
        const std::size_t w = term::width(plain[i]);
        const std::size_t width = lines.empty() ? first : rest;
        if (used > 0 && used + 1 + w > width) {
            lines.push_back(std::move(current));
            current.clear();
            used = 0;
        }
        if (used > 0) {
            current += ' ';
            ++used;
        }
        current += styled[i];
        used += w;
    }
    if (!current.empty() || lines.empty()) lines.push_back(std::move(current));
    return lines;
}

/// The lyric of one line, chunked at spaces, with rhyming words painted in their
/// family colour (plain text when colours are off).
void lyric_chunks(const Line& line, std::vector<std::string>& plain, std::vector<std::string>& styled) {
    const std::string& text = line.text;
    std::size_t pos = 0;
    while (pos < text.size()) {
        while (pos < text.size() && text[pos] == ' ') ++pos;
        if (pos >= text.size()) break;
        std::size_t end = text.find(' ', pos);
        if (end == std::string::npos) end = text.size();
        std::string chunk_plain = text.substr(pos, end - pos);
        std::string chunk_styled;
        std::size_t cursor = pos;
        for (const Word& word : line.words) {
            if (word.begin < pos || word.end > end) continue;
            chunk_styled += text.substr(cursor, word.begin - cursor);
            const std::string_view w(text.data() + word.begin, word.end - word.begin);
            chunk_styled += word.group >= 0 ? term::paint_bold(w, family_color(word.group))
                                            : std::string(w);
            cursor = word.end;
        }
        chunk_styled += text.substr(cursor, end - cursor);
        plain.push_back(std::move(chunk_plain));
        styled.push_back(std::move(chunk_styled));
        pos = end;
    }
}

std::string cell(std::string_view label, std::string_view value, std::size_t label_w,
                 std::size_t value_w) {
    return term::paint(term::pad_right(label, label_w), palette::gray) +
           term::paint_bold(term::pad_left(value, value_w), palette::white);
}

std::string stats_panel(const Analysis& a, const RenderOptions& options, std::size_t width) {
    // Round first so the label always agrees with the number shown next to it.
    const double sps = std::round(a.syllables_per_second(options.bpm) * 10.0) / 10.0;
    std::vector<std::string> rows;
    rows.push_back(cell("words", std::to_string(a.total_words), 11, 6) + "    " +
                   cell("unique", std::to_string(a.unique_words), 9, 6) + "    " +
                   cell("richness", text::percent(a.vocabulary_richness), 10, 6));
    rows.push_back(cell("syllables", std::to_string(a.total_syllables), 11, 6) + "    " +
                   cell("bars", std::to_string(a.bars), 9, 6) + "    " +
                   cell("per bar", text::fixed(a.syllables_per_bar(), 1), 10, 6));
    rows.push_back(cell("families", std::to_string(a.rhyme_groups), 11, 6) + "    " +
                   cell("internal", std::to_string(a.internal_rhymes), 9, 6) + "    " +
                   cell("rhymed syl", std::to_string(a.rhymed_syllables), 10, 6));
    rows.emplace_back();

    const Rgb density_color = term::lerp(palette::cyan, palette::pink, a.rhyme_density / 0.6);
    rows.push_back(term::paint(term::pad_right("rhyme density", 16), palette::gray) +
                   term::paint_bold(term::pad_left(text::percent(a.rhyme_density), 6), density_color) +
                   "  " + term::paint(term::meter(a.rhyme_density / 0.6, 24), density_color));

    std::string multi = a.longest_multisyllabic == 0
                            ? std::string("no rhymes found")
                            : std::format("{} syllable{}", a.longest_multisyllabic,
                                          a.longest_multisyllabic == 1 ? "" : "s");
    if (a.longest_multisyllabic > 0 && !a.longest_multisyllabic_words[0].empty()) {
        const std::string pair = "\"" + a.longest_multisyllabic_words[0] + "\" / \"" +
                                 a.longest_multisyllabic_words[1] + "\"";
        multi += term::paint("  " + pair, palette::gray);
    }
    rows.push_back(term::paint(term::pad_right("longest multi", 16), palette::gray) +
                   term::paint_bold(multi, palette::white));

    const Rgb flow_color = term::lerp(palette::teal, palette::orange, sps / 8.0);
    rows.push_back(term::paint(term::pad_right("flow speed", 16), palette::gray) +
                   term::paint_bold(std::format("{} syl/s", text::fixed(sps, 1)), flow_color) +
                   term::paint(std::format(" at {} BPM  ", options.bpm), palette::gray) +
                   term::paint_bold(std::string(flow_label(sps)), flow_color));

    for (std::string& row : rows) row = term::truncate(row, width - 4);
    return term::panel("FLOW REPORT", rows, palette::dark, width - 4);
}

std::string families_legend(const Analysis& a, std::size_t width) {
    std::string out = term::paint_bold(" RHYME FAMILIES", palette::gold) + '\n';
    if (a.families.empty()) return out + term::paint("   none — every line stands alone\n", palette::gray);
    constexpr std::size_t kSoundCol = 12;
    const std::size_t indent = 3 + 2 + kSoundCol;
    for (const RhymeFamily& family : a.families) {
        const Rgb color = family_color(family.id);
        std::vector<std::string> plain;
        for (std::size_t i = 0; i < family.words.size(); ++i)
            plain.push_back(family.words[i] + (i + 1 < family.words.size() ? " ·" : ""));
        std::vector<std::string> styled;
        for (const std::string& p : plain) styled.push_back(term::paint(p, color));
        const std::size_t room = width > indent + 10 ? width - indent : 10;
        const std::vector<std::string> wrapped = wrap(plain, styled, room, room);
        const std::string sound = term::truncate(family.sound, kSoundCol - 1);
        for (std::size_t i = 0; i < wrapped.size(); ++i) {
            out += i == 0 ? "   " + term::paint("■", color) + " " +
                                term::paint_bold(term::pad_right(sound, kSoundCol), color)
                          : std::string(indent, ' ');
            out += wrapped[i] + '\n';
        }
    }
    return out;
}

std::string header(const Analysis& a, const RenderOptions& options, std::size_t width) {
    std::string title = term::paint_bold(" BARS", palette::pink) +
                        term::paint("  ·  ", palette::dark) +
                        term::paint(std::string(language_name(a.language)), palette::white) +
                        term::paint("  ·  ", palette::dark) +
                        term::paint(std::format("{} bars", a.bars), palette::white) +
                        term::paint("  ·  ", palette::dark) +
                        term::paint(std::format("{} BPM", options.bpm), palette::white);
    std::string out = term::truncate(title, width) + '\n';
    if (!a.scheme.empty())
        out += term::truncate(term::paint(" scheme  ", palette::gray) +
                                  term::paint_bold(a.scheme, palette::gold),
                              width) +
               '\n';
    out += " " + term::rule(width - 2, {}, palette::dark) + '\n';
    return out;
}

} // namespace

std::string_view flow_label(double sps) noexcept {
    if (sps < 3.0) return "laid-back";
    if (sps < 5.0) return "steady";
    if (sps < 7.0) return "rapid-fire";
    return "chopper";
}

std::string render(const Analysis& a, const RenderOptions& options) {
    const auto width = static_cast<std::size_t>(std::clamp(options.width, 60, 160));
    std::string out = header(a, options, width);

    const std::size_t number_w = std::max<std::size_t>(2, std::to_string(a.bars).size());
    // " nn  A  12 ██████  lyric"
    const std::size_t prefix_w = 1 + number_w + 2 + 1 + 2 + 2 + 1 + kMeterCells + 2;
    const std::size_t lyric_w = width > prefix_w + 20 ? width - prefix_w : 20;

    int bar = 0;
    for (const Line& line : a.lines) {
        if (line.header) {
            out += std::string(prefix_w, ' ') + term::paint(line.text, palette::gray) + '\n';
            continue;
        }
        if (line.is_break()) {
            out += '\n';
            continue;
        }
        ++bar;
        const int group = line.words.back().group;
        const double fill = static_cast<double>(line.syllables) / kMeterFull;
        const Rgb meter_color = term::lerp(palette::teal, palette::pink, fill);

        std::string prefix = " " + term::paint(term::pad_left(std::to_string(bar), number_w), palette::dark);
        prefix += "  " + term::paint_bold(std::string(1, line.scheme), family_color(group));
        prefix += "  " + term::paint(term::pad_left(std::to_string(line.syllables), 2), palette::white);
        prefix += " " + term::paint(term::meter(fill, kMeterCells), meter_color) + "  ";

        std::vector<std::string> plain;
        std::vector<std::string> styled;
        lyric_chunks(line, plain, styled);
        const std::vector<std::string> wrapped = wrap(plain, styled, lyric_w, lyric_w - 2);
        for (std::size_t i = 0; i < wrapped.size(); ++i)
            out += (i == 0 ? prefix : std::string(prefix_w + 2, ' ')) + wrapped[i] + '\n';
    }
    if (a.bars == 0) out += term::paint("  (no lyrics)\n", palette::gray);

    if (options.show_stats) {
        out += '\n' + stats_panel(a, options, width);
        out += '\n' + families_legend(a, width);
    }
    return out;
}

std::string render_rhymes(std::string_view word, Language lang, std::size_t limit, int width_in) {
    const auto width = static_cast<std::size_t>(std::clamp(width_in, 60, 160));
    if (lang == Language::Auto) lang = detect_language(word);
    const Pronunciation p = pronounce(word, lang);
    const std::vector<RhymeMatch> matches = find_rhymes(word, lang, limit);

    std::string out = term::paint_bold(" RHYMES", palette::pink) + term::paint("  ·  ", palette::dark) +
                      term::paint_bold(std::string(word), palette::white) +
                      term::paint("  ·  ", palette::dark) +
                      term::paint(std::string(language_name(lang)), palette::white) +
                      term::paint("  ·  sound ", palette::gray) +
                      term::paint_bold(sound_label(p.vowel_skeleton(), lang), palette::gold) +
                      term::paint(std::format("  ·  {} syllable{}", p.syllables(),
                                              p.syllables() == 1 ? "" : "s"),
                                  palette::gray) +
                      '\n';
    out += " " + term::rule(width - 2, {}, palette::dark) + '\n';

    constexpr std::size_t kLabel = 10;
    for (const RhymeKind kind : {RhymeKind::Perfect, RhymeKind::Slant}) {
        std::vector<std::string> plain;
        for (const RhymeMatch& m : matches)
            if (m.kind == kind) plain.push_back(m.word);
        for (std::size_t i = 0; i + 1 < plain.size(); ++i) plain[i] += " ·";
        const Rgb color = kind == RhymeKind::Perfect ? palette::gold : palette::cyan;
        std::vector<std::string> styled;
        for (const std::string& s : plain) styled.push_back(term::paint(s, color));
        const std::string label = " " + term::pad_right(std::string(rhyme_kind_name(kind)), kLabel - 1);
        if (plain.empty()) {
            out += term::paint_bold(label, color) + term::paint("—", palette::dark) + '\n';
            continue;
        }
        const std::vector<std::string> wrapped = wrap(plain, styled, width - kLabel - 1, width - kLabel - 1);
        for (std::size_t i = 0; i < wrapped.size(); ++i)
            out += (i == 0 ? term::paint_bold(label, color) : std::string(kLabel, ' ')) + wrapped[i] + '\n';
    }
    return out;
}

} // namespace street::bars
