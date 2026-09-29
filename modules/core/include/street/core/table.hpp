#pragma once
// Box-drawn tables with per-column alignment; cells may contain ANSI colors.

#include <string>
#include <vector>

#include "street/core/term.hpp"

namespace street {

enum class Align { Left, Right, Center };

class Table {
public:
    Table() = default;
    explicit Table(std::vector<std::string> headers);

    Table& title(std::string text);
    Table& align(std::size_t column, Align alignment);
    /// Right-aligns every column from `first` onward (typical for numeric stats).
    Table& align_right_from(std::size_t first);
    Table& accent(term::Rgb color);
    Table& add_row(std::vector<std::string> cells);
    Table& add_separator();

    std::size_t rows() const noexcept;
    std::string render() const;

private:
    struct Row {
        std::vector<std::string> cells;
        bool separator = false;
    };
    std::string title_;
    std::vector<std::string> headers_;
    std::vector<Align> aligns_;
    std::vector<Row> rows_;
    term::Rgb accent_ = term::palette::gray;
};

} // namespace street
