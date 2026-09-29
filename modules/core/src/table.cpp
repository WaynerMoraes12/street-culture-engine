#include "street/core/table.hpp"

#include <algorithm>

namespace street {
namespace {

std::string aligned(const std::string& cell, std::size_t w, Align a) {
    switch (a) {
    case Align::Right: return term::pad_left(cell, w);
    case Align::Center: return term::center(cell, w);
    case Align::Left: break;
    }
    return term::pad_right(cell, w);
}

} // namespace

Table::Table(std::vector<std::string> headers) : headers_(std::move(headers)) {}

Table& Table::title(std::string text) {
    title_ = std::move(text);
    return *this;
}

Table& Table::align(std::size_t column, Align alignment) {
    if (aligns_.size() <= column) aligns_.resize(column + 1, Align::Left);
    aligns_[column] = alignment;
    return *this;
}

Table& Table::align_right_from(std::size_t first) {
    const std::size_t n = std::max(headers_.size(), aligns_.size());
    for (std::size_t c = first; c < std::max(n, first + 1); ++c) align(c, Align::Right);
    return *this;
}

Table& Table::accent(term::Rgb color) {
    accent_ = color;
    return *this;
}

Table& Table::add_row(std::vector<std::string> cells) {
    rows_.push_back({std::move(cells), false});
    return *this;
}

Table& Table::add_separator() {
    rows_.push_back({{}, true});
    return *this;
}

std::size_t Table::rows() const noexcept {
    return static_cast<std::size_t>(
        std::count_if(rows_.begin(), rows_.end(), [](const Row& r) { return !r.separator; }));
}

std::string Table::render() const {
    std::size_t ncols = headers_.size();
    for (const auto& row : rows_) ncols = std::max(ncols, row.cells.size());
    if (ncols == 0) return {};

    std::vector<std::size_t> widths(ncols, 0);
    for (std::size_t c = 0; c < headers_.size(); ++c) widths[c] = term::width(headers_[c]);
    for (const auto& row : rows_)
        for (std::size_t c = 0; c < row.cells.size(); ++c)
            widths[c] = std::max(widths[c], term::width(row.cells[c]));

    const auto align_of = [&](std::size_t c) { return c < aligns_.size() ? aligns_[c] : Align::Left; };
    const auto border = [&](std::string_view left, std::string_view mid, std::string_view right) {
        std::string line{left};
        for (std::size_t c = 0; c < ncols; ++c) {
            line += term::repeat("─", widths[c] + 2);
            line += c + 1 < ncols ? mid : right;
        }
        return term::paint(line, accent_) + '\n';
    };
    const auto content = [&](const std::vector<std::string>& cells, bool header) {
        std::string line = term::paint("│", accent_);
        for (std::size_t c = 0; c < ncols; ++c) {
            const std::string cell = c < cells.size() ? cells[c] : std::string{};
            const std::string body = aligned(cell, widths[c], header ? Align::Left : align_of(c));
            line += ' ' + (header ? term::emphasize(body) : body) + ' ';
            line += term::paint("│", accent_);
        }
        return line + '\n';
    };

    std::string out;
    if (!title_.empty()) out += term::paint_bold(title_, accent_) + '\n';
    out += border("╭", "┬", "╮");
    if (!headers_.empty()) {
        out += content(headers_, true);
        out += border("├", "┼", "┤");
    }
    for (const auto& row : rows_)
        out += row.separator ? border("├", "┼", "┤") : content(row.cells, false);
    out += border("╰", "┴", "╯");
    return out;
}

} // namespace street
