#include "street/core/term.hpp"
#include "street/kicks/render.hpp"

#include <array>
#include <string_view>

namespace street::kicks {
namespace {

// A high-top in profile, toe to the right: padded collar, heel panel, laces, swoosh, and a
// cupsole with a stitched midsole. Plain ASCII so it renders in any terminal font.
// clang-format off
constexpr std::array<std::string_view, 11> kSneaker = {
    R"(      ______)",
    R"(     |  __  `.)",
    R"(     | |  |   `.)",
    R"(     | |  |  ==`.)",
    R"(     | |  |   == `._______)",
    R"(     | |__|    ==  ==  == `-.___)",
    R"(     |    \_            __,-'   `-._)",
    R"(     |      `--.____,--'            `.)",
    R"( ____|________________________________))",
    R"((___________________________________.')",
    R"( `==================================')",
};
// clang-format on

} // namespace

std::string ascii_sneaker() {
    using namespace term;
    // "Chicago" colours: varsity red at the heel fading to white at the toe, white cupsole,
    // cement-grey stitching.
    constexpr std::array<Rgb, 2> upper = {Rgb{206, 17, 38}, Rgb{245, 245, 245}};
    std::string out;
    for (std::size_t row = 0; row < kSneaker.size(); ++row) {
        const std::string_view line = kSneaker[row];
        if (row + 1 == kSneaker.size())
            out += paint(line, palette::cement);
        else if (row >= 8)
            out += paint(line, palette::white);
        else
            out += gradient(line, upper);
        out += '\n';
    }
    return out;
}

} // namespace street::kicks
