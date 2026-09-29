#include "street/beats/audio.hpp"
#include "street/core/text.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <doctest/doctest.h>
#include <filesystem>
#include <stdexcept>
#include <vector>

using namespace street::beats;

namespace {

std::uint32_t u32(const std::vector<std::uint8_t>& b, std::size_t at) {
    return b[at] | (b[at + 1] << 8) | (b[at + 2] << 16) | (std::uint32_t{b[at + 3]} << 24);
}
std::uint16_t u16(const std::vector<std::uint8_t>& b, std::size_t at) {
    return static_cast<std::uint16_t>(b[at] | (b[at + 1] << 8));
}
std::int16_t s16(const std::vector<std::uint8_t>& b, std::size_t at) {
    return static_cast<std::int16_t>(u16(b, at));
}
bool tag(const std::vector<std::uint8_t>& b, std::size_t at, const char* t) {
    return std::equal(t, t + 4, b.begin() + static_cast<std::ptrdiff_t>(at));
}

} // namespace

TEST_CASE("AudioBuffer reports frames and duration") {
    const AudioBuffer audio(44100 * 3);
    CHECK(audio.frames() == 132300);
    CHECK(audio.seconds() == doctest::Approx(3.0));
    CHECK(audio.right.size() == audio.left.size());
    CHECK(AudioBuffer{}.seconds() == 0.0);
}

TEST_CASE("encode_wav writes a canonical 16-bit stereo PCM header") {
    AudioBuffer audio(3);
    const auto bytes = encode_wav(audio);
    REQUIRE(bytes.size() == wav_size(3));
    CHECK(bytes.size() == 44 + 3 * 4);
    CHECK(tag(bytes, 0, "RIFF"));
    CHECK(u32(bytes, 4) == bytes.size() - 8);
    CHECK(tag(bytes, 8, "WAVE"));
    CHECK(tag(bytes, 12, "fmt "));
    CHECK(u32(bytes, 16) == 16);
    CHECK(u16(bytes, 20) == 1);         // PCM
    CHECK(u16(bytes, 22) == 2);         // stereo
    CHECK(u32(bytes, 24) == 44100);     // sample rate
    CHECK(u32(bytes, 28) == 44100 * 4); // byte rate
    CHECK(u16(bytes, 32) == 4);         // block align
    CHECK(u16(bytes, 34) == 16);        // bits
    CHECK(tag(bytes, 36, "data"));
    CHECK(u32(bytes, 40) == 12);
}

TEST_CASE("samples are rounded, clamped and interleaved left/right") {
    AudioBuffer audio(3);
    audio.left = {1.0f, -1.0f, 0.5f};
    audio.right = {2.0f, -3.0f, -0.25f}; // out of range values clamp instead of wrapping
    const auto b = encode_wav(audio);
    CHECK(s16(b, 44) == 32767);
    CHECK(s16(b, 46) == 32767);
    CHECK(s16(b, 48) == -32767);
    CHECK(s16(b, 50) == -32767);
    CHECK(s16(b, 52) == 16384);
    CHECK(s16(b, 54) == -8192);
}

TEST_CASE("decode_wav round-trips encode_wav within one LSB") {
    AudioBuffer audio(1000, 22050);
    for (std::size_t i = 0; i < audio.frames(); ++i) {
        audio.left[i] = static_cast<float>(std::sin(0.05 * static_cast<double>(i)) * 0.8);
        audio.right[i] = static_cast<float>(std::cos(0.03 * static_cast<double>(i)) * 0.5);
    }
    const auto bytes = encode_wav(audio);
    const AudioBuffer back = decode_wav(bytes);
    REQUIRE(back.frames() == audio.frames());
    CHECK(back.sample_rate == 22050);
    for (std::size_t i = 0; i < audio.frames(); ++i) {
        CHECK(std::abs(back.left[i] - audio.left[i]) <= 1.0f / 32767.0f);
        CHECK(std::abs(back.right[i] - audio.right[i]) <= 1.0f / 32767.0f);
    }
}

TEST_CASE("decode_wav skips unknown chunks and rejects malformed files") {
    AudioBuffer audio(2);
    audio.left = {0.5f, -0.5f};
    audio.right = {0.25f, 0.0f};
    std::vector<std::uint8_t> bytes = encode_wav(audio);
    // Splice a "LIST" chunk (odd size, so it needs a pad byte) between fmt and data.
    const std::vector<std::uint8_t> list = {'L', 'I', 'S', 'T', 3, 0, 0, 0, 'a', 'b', 'c', 0};
    bytes.insert(bytes.begin() + 36, list.begin(), list.end());
    const AudioBuffer back = decode_wav(bytes);
    CHECK(back.frames() == 2);
    CHECK(back.left[0] == doctest::Approx(0.5).epsilon(1e-4));

    const std::vector<std::uint8_t> junk = {'n', 'o', 'p', 'e'};
    CHECK_THROWS_AS(decode_wav(junk), std::invalid_argument);
    std::vector<std::uint8_t> truncated = encode_wav(audio);
    truncated.resize(40);
    CHECK_THROWS_AS(decode_wav(truncated), std::invalid_argument);
}

TEST_CASE("encode_wav validates its input") {
    AudioBuffer bad(4);
    bad.right.pop_back();
    CHECK_THROWS_AS(encode_wav(bad), std::invalid_argument);
    AudioBuffer no_rate(4);
    no_rate.sample_rate = 0;
    CHECK_THROWS_AS(encode_wav(no_rate), std::invalid_argument);
}

TEST_CASE("write_wav writes the encoded bytes to disk") {
    const auto path = std::filesystem::temp_directory_path() / "street_beats_test_write.wav";
    AudioBuffer audio(100);
    audio.left[10] = 0.5f;
    write_wav(path, audio);
    const std::string bytes = street::text::read_file(path);
    CHECK(bytes.size() == wav_size(100));
    CHECK(bytes.substr(0, 4) == "RIFF");
    std::filesystem::remove(path);
    CHECK_THROWS_AS(write_wav(std::filesystem::path("no-such-dir-street") / "sub" / "x.wav", audio),
                    std::runtime_error);
}

TEST_CASE("level meters") {
    AudioBuffer audio(4);
    audio.left = {0.5f, -0.5f, 0.5f, -0.5f};
    audio.right = {0.5f, -0.5f, 0.5f, -0.5f};
    CHECK(peak(audio) == doctest::Approx(0.5));
    CHECK(peak_dbfs(audio) == doctest::Approx(-6.0206).epsilon(1e-3));
    CHECK(rms_dbfs(audio) == doctest::Approx(-6.0206).epsilon(1e-3));
    CHECK(peak_dbfs(AudioBuffer(8)) == -120.0);
    CHECK(db_to_gain(gain_to_db(0.3)) == doctest::Approx(0.3));
}
