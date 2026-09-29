#include "street/beats/audio.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <fstream>
#include <stdexcept>

namespace street::beats {
namespace {

constexpr std::size_t kHeaderBytes = 44;
constexpr int kChannels = 2;
constexpr int kBytesPerSample = 2;

void put_u16(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
}

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    put_u16(out, v & 0xFFFFu);
    put_u16(out, v >> 16);
}

void put_tag(std::vector<std::uint8_t>& out, const char (&tag)[5]) {
    out.insert(out.end(), tag, tag + 4);
}

std::int16_t to_pcm16(float sample) noexcept {
    const float clamped = std::clamp(sample, -1.0f, 1.0f);
    return static_cast<std::int16_t>(std::lround(clamped * 32767.0f));
}

std::uint32_t get_u32(std::span<const std::uint8_t> b, std::size_t at) {
    return std::uint32_t{b[at]} | (std::uint32_t{b[at + 1]} << 8) |
           (std::uint32_t{b[at + 2]} << 16) | (std::uint32_t{b[at + 3]} << 24);
}

std::uint16_t get_u16(std::span<const std::uint8_t> b, std::size_t at) {
    return static_cast<std::uint16_t>(b[at] | (b[at + 1] << 8));
}

bool tag_is(std::span<const std::uint8_t> b, std::size_t at, const char (&tag)[5]) {
    return std::memcmp(b.data() + at, tag, 4) == 0;
}

} // namespace

AudioBuffer::AudioBuffer(std::size_t frames, int rate)
    : sample_rate(rate), left(frames, 0.0f), right(frames, 0.0f) {}

double AudioBuffer::seconds() const noexcept {
    return sample_rate > 0 ? static_cast<double>(frames()) / sample_rate : 0.0;
}

float peak(const AudioBuffer& audio) noexcept {
    float p = 0.0f;
    for (float s : audio.left)
        p = std::max(p, std::abs(s));
    for (float s : audio.right)
        p = std::max(p, std::abs(s));
    return p;
}

double gain_to_db(double gain) noexcept {
    return gain > 1e-6 ? 20.0 * std::log10(gain) : -120.0;
}
double db_to_gain(double db) noexcept {
    return std::pow(10.0, db / 20.0);
}

double peak_dbfs(const AudioBuffer& audio) noexcept {
    return gain_to_db(peak(audio));
}

double rms_dbfs(const AudioBuffer& audio) noexcept {
    const std::size_t n = audio.left.size() + audio.right.size();
    if (n == 0) return -120.0;
    double sum = 0.0;
    for (float s : audio.left)
        sum += double{s} * s;
    for (float s : audio.right)
        sum += double{s} * s;
    return gain_to_db(std::sqrt(sum / static_cast<double>(n)));
}

std::size_t wav_size(std::size_t frames) noexcept {
    return kHeaderBytes + frames * kChannels * kBytesPerSample;
}

std::vector<std::uint8_t> encode_wav(const AudioBuffer& audio) {
    if (audio.left.size() != audio.right.size())
        throw std::invalid_argument("encode_wav: left and right channels differ in length");
    if (audio.sample_rate <= 0)
        throw std::invalid_argument("encode_wav: sample rate must be positive");
    const std::size_t data_bytes = audio.frames() * kChannels * kBytesPerSample;
    if (data_bytes > 0xFFFFFFFFull - kHeaderBytes)
        throw std::invalid_argument("encode_wav: audio too long for a WAV file (4 GB limit)");

    const auto rate = static_cast<std::uint32_t>(audio.sample_rate);
    std::vector<std::uint8_t> out;
    out.reserve(kHeaderBytes + data_bytes);
    put_tag(out, "RIFF");
    put_u32(out, static_cast<std::uint32_t>(36 + data_bytes));
    put_tag(out, "WAVE");
    put_tag(out, "fmt ");
    put_u32(out, 16); // fmt chunk size
    put_u16(out, 1);  // PCM
    put_u16(out, kChannels);
    put_u32(out, rate);
    put_u32(out, rate * kChannels * kBytesPerSample); // byte rate
    put_u16(out, kChannels * kBytesPerSample);        // block align
    put_u16(out, 8 * kBytesPerSample);                // bits per sample
    put_tag(out, "data");
    put_u32(out, static_cast<std::uint32_t>(data_bytes));
    for (std::size_t i = 0; i < audio.frames(); ++i) {
        put_u16(out, static_cast<std::uint16_t>(to_pcm16(audio.left[i])));
        put_u16(out, static_cast<std::uint16_t>(to_pcm16(audio.right[i])));
    }
    return out;
}

AudioBuffer decode_wav(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 12 || !tag_is(bytes, 0, "RIFF") || !tag_is(bytes, 8, "WAVE"))
        throw std::invalid_argument("decode_wav: not a RIFF/WAVE file");

    int channels = 0, rate = 0, bits = 0;
    std::size_t pos = 12;
    while (pos + 8 <= bytes.size()) {
        const std::size_t size = get_u32(bytes, pos + 4);
        const std::size_t body = pos + 8;
        if (size > bytes.size() - body) throw std::invalid_argument("decode_wav: truncated chunk");
        if (tag_is(bytes, pos, "fmt ")) {
            if (size < 16) throw std::invalid_argument("decode_wav: fmt chunk too small");
            if (get_u16(bytes, body) != 1)
                throw std::invalid_argument("decode_wav: only PCM is supported");
            channels = get_u16(bytes, body + 2);
            rate = static_cast<int>(get_u32(bytes, body + 4));
            bits = get_u16(bytes, body + 14);
        } else if (tag_is(bytes, pos, "data")) {
            if (bits != 16 || (channels != 1 && channels != 2) || rate <= 0)
                throw std::invalid_argument(std::format(
                    "decode_wav: unsupported format ({} ch, {} bit, {} Hz)", channels, bits, rate));
            const std::size_t block = static_cast<std::size_t>(channels) * 2;
            AudioBuffer audio(size / block, rate);
            for (std::size_t i = 0; i < audio.frames(); ++i) {
                const std::size_t at = body + i * block;
                const auto l = static_cast<std::int16_t>(get_u16(bytes, at));
                const auto r =
                    channels == 2 ? static_cast<std::int16_t>(get_u16(bytes, at + 2)) : l;
                audio.left[i] = static_cast<float>(l) / 32767.0f;
                audio.right[i] = static_cast<float>(r) / 32767.0f;
            }
            return audio;
        }
        pos = body + size + (size & 1u); // chunks are word-aligned
    }
    throw std::invalid_argument("decode_wav: no data chunk");
}

void write_wav(const std::filesystem::path& path, const AudioBuffer& audio) {
    const std::vector<std::uint8_t> bytes = encode_wav(audio);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) throw std::runtime_error(std::format("cannot open '{}' for writing", path.string()));
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    if (!file) throw std::runtime_error(std::format("failed while writing '{}'", path.string()));
}

} // namespace street::beats
