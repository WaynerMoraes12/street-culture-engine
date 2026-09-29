#pragma once
// Audio buffers, level metering and the WAV file format.
//
// Everything in the beats engine is rendered into an AudioBuffer: two planar
// channels of 32-bit floats where ±1.0 is digital full scale. Only at the very
// end is the buffer quantised to 16-bit PCM and wrapped in a RIFF/WAVE header,
// written byte by byte in little-endian order so the output is identical on
// every host regardless of its native endianness.

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace street::beats {

inline constexpr int kSampleRate = 44100;

/// Planar stereo audio. Invariant: left.size() == right.size().
struct AudioBuffer {
    int sample_rate = kSampleRate;
    std::vector<float> left, right;

    AudioBuffer() = default;
    /// Silent buffer of `frames` samples per channel.
    explicit AudioBuffer(std::size_t frames, int rate = kSampleRate);

    std::size_t frames() const noexcept { return left.size(); }
    double seconds() const noexcept;
};

/// Largest absolute sample value across both channels (linear, 1.0 = full scale).
float peak(const AudioBuffer& audio) noexcept;
/// Peak level in dBFS; -infinity-safe (silence reports -120 dB).
double peak_dbfs(const AudioBuffer& audio) noexcept;
/// RMS level of both channels combined, in dBFS (silence reports -120 dB).
double rms_dbfs(const AudioBuffer& audio) noexcept;

double gain_to_db(double gain) noexcept;
double db_to_gain(double db) noexcept;

/// Size in bytes of the WAV file encode_wav() produces for `frames` stereo frames.
std::size_t wav_size(std::size_t frames) noexcept;

/// Encodes 16-bit PCM stereo WAV (44-byte canonical header). Samples are clamped to
/// [-1, 1] and rounded to the nearest integer. Throws std::invalid_argument if the
/// channels differ in length or the sample rate is not positive.
std::vector<std::uint8_t> encode_wav(const AudioBuffer& audio);

/// Decodes a 16-bit PCM WAV (mono or stereo; mono is duplicated to both channels).
/// Unknown chunks are skipped. Throws std::invalid_argument on malformed input.
AudioBuffer decode_wav(std::span<const std::uint8_t> bytes);

/// encode_wav() + write to disk. Throws std::runtime_error naming the path on failure.
void write_wav(const std::filesystem::path& path, const AudioBuffer& audio);

} // namespace street::beats
