#include "audio_studio/audio_decoder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <utility>
#include <vector>

namespace audio_studio {
namespace {

std::uint16_t u16(const unsigned char* b) noexcept {
    return static_cast<std::uint16_t>(b[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(b[1]) << 8U);
}
std::uint32_t u32(const unsigned char* b) noexcept {
    return static_cast<std::uint32_t>(b[0]) |
           (static_cast<std::uint32_t>(b[1]) << 8U) |
           (static_cast<std::uint32_t>(b[2]) << 16U) |
           (static_cast<std::uint32_t>(b[3]) << 24U);
}
bool tag(const unsigned char* b, const char* value) noexcept {
    return b[0] == static_cast<unsigned char>(value[0]) &&
           b[1] == static_cast<unsigned char>(value[1]) &&
           b[2] == static_cast<unsigned char>(value[2]) &&
           b[3] == static_cast<unsigned char>(value[3]);
}
bool fail(std::string* error, const char* message) {
    if (error) *error = message;
    return false;
}

class WavStreamDecoder final : public AudioDecoder {
public:
    bool open(const std::string& path, std::string* error) {
        if (error) error->clear();
        stream_.open(path, std::ios::binary | std::ios::ate);
        if (!stream_) return fail(error, "Could not open the audio file.");
        const auto end = stream_.tellg();
        if (end < 12) return fail(error, "WAV file is too small to contain a RIFF header.");
        const auto file_size = static_cast<std::uint64_t>(end);
        std::array<unsigned char, 12U> header{};
        stream_.seekg(0, std::ios::beg);
        if (!stream_.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size())))
            return fail(error, "Could not read the WAV header.");
        if (!tag(header.data(), "RIFF") || !tag(header.data() + 8U, "WAVE"))
            return fail(error, "File is not a RIFF/WAVE file.");
        const std::uint64_t riff_end = static_cast<std::uint64_t>(u32(header.data() + 4U)) + 8U;
        if (riff_end < 12U || riff_end > file_size)
            return fail(error, "RIFF size is invalid or the file is truncated.");

        bool have_fmt = false, have_data = false;
        std::uint16_t encoding = 0U, bits = 0U, block_align = 0U;
        std::uint32_t rate = 0U;
        std::uint64_t cursor = 12U;
        while (cursor + 8U <= riff_end) {
            stream_.seekg(static_cast<std::streamoff>(cursor), std::ios::beg);
            std::array<unsigned char, 8U> ch{};
            if (!stream_.read(reinterpret_cast<char*>(ch.data()), static_cast<std::streamsize>(ch.size())))
                return fail(error, "WAV contains an incomplete chunk header.");
            const auto size = u32(ch.data() + 4U);
            const auto payload = cursor + 8U;
            const auto end_payload = payload + size;
            const auto padded_end = end_payload + (size & 1U);
            if (end_payload < payload || padded_end < end_payload || padded_end > riff_end)
                return fail(error, "WAV contains a truncated or out-of-bounds chunk.");

            if (tag(ch.data(), "fmt ")) {
                if (have_fmt || size < 16U) return fail(error, "WAV format chunk is duplicated or too short.");
                std::array<unsigned char, 40U> fmt{};
                const auto to_read = static_cast<std::size_t>(size < fmt.size() ? size : fmt.size());
                stream_.seekg(static_cast<std::streamoff>(payload), std::ios::beg);
                if (!stream_.read(reinterpret_cast<char*>(fmt.data()), static_cast<std::streamsize>(to_read)))
                    return fail(error, "Could not read the WAV format chunk.");
                encoding = u16(fmt.data());
                const auto channels = u16(fmt.data() + 2U);
                rate = u32(fmt.data() + 4U);
                const auto byte_rate = u32(fmt.data() + 8U);
                block_align = u16(fmt.data() + 12U);
                bits = u16(fmt.data() + 14U);
                if (encoding == 0xfffeU) {
                    if (size < 40U) return fail(error, "WAVE_FORMAT_EXTENSIBLE format chunk is too short.");
                    const auto cb = u16(fmt.data() + 16U);
                    const auto valid = u16(fmt.data() + 18U);
                    const auto subformat = u32(fmt.data() + 24U);
                    const std::array<unsigned char,12U> tail{
                        fmt[28],fmt[29],fmt[30],fmt[31],fmt[32],fmt[33],
                        fmt[34],fmt[35],fmt[36],fmt[37],fmt[38],fmt[39]};
                    const std::array<unsigned char,12U> canonical{
                        0x00U,0x00U,0x10U,0x00U,0x80U,0x00U,
                        0x00U,0xaaU,0x00U,0x38U,0x9bU,0x71U};
                    if (cb < 22U || cb > size - 18U || valid != bits ||
                        tail != canonical || (subformat != 1U && subformat != 3U))
                        return fail(error, "Unsupported WAVE_FORMAT_EXTENSIBLE subformat or valid-bit layout.");
                    encoding = static_cast<std::uint16_t>(subformat);
                }
                const bool pcm = encoding == 1U &&
                    (bits == 8U || bits == 16U || bits == 24U || bits == 32U);
                const bool fp = encoding == 3U && bits == 32U;
                const auto bytes_per_sample = static_cast<std::uint16_t>(bits / 8U);
                if ((!pcm && !fp) || (channels != 1U && channels != 2U) ||
                    rate < 8000U || rate > 192000U ||
                    block_align != channels * bytes_per_sample ||
                    byte_rate != rate * static_cast<std::uint32_t>(block_align))
                    return fail(error, "Supported WAV input is mono/stereo PCM 8/16/24/32-bit or IEEE float32.");
                format_ = {rate, channels};
                have_fmt = true;
            } else if (tag(ch.data(), "data")) {
                if (have_data) return fail(error, "WAV contains multiple data chunks.");
                data_offset_ = payload;
                data_size_ = size;
                have_data = true;
            }
            cursor = padded_end;
        }
        if (cursor != riff_end) return fail(error, "WAV contains an incomplete chunk header.");
        if (!have_fmt || !have_data) return fail(error, "WAV is missing its format or data chunk.");
        if (block_align == 0U || data_size_ == 0U || data_size_ % block_align != 0U ||
            data_offset_ + data_size_ > riff_end)
            return fail(error, "WAV audio data is empty, unaligned, or out of bounds.");
        encoding_ = encoding;
        bits_ = bits;
        block_align_ = block_align;
        total_frames_ = data_size_ / block_align_;
        return seek_frame(0U, error);
    }

    AudioFormat format() const noexcept override { return format_; }
    std::uint64_t total_frames() const noexcept override { return total_frames_; }
    std::uint64_t position_frames() const noexcept override { return position_frames_; }

    bool seek_frame(std::uint64_t frame, std::string* error) override {
        if (error) error->clear();
        if (frame > total_frames_) return fail(error, "Requested frame is outside the audio stream.");
        const std::uint64_t byte_offset = data_offset_ + frame * block_align_;
        if (byte_offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))
            return fail(error, "Requested audio position is too large for this platform.");
        stream_.clear();
        stream_.seekg(static_cast<std::streamoff>(byte_offset), std::ios::beg);
        if (!stream_) return fail(error, "Could not seek to the requested audio frame.");
        position_frames_ = frame;
        return true;
    }

    bool read_frames(float* destination, std::size_t requested, std::size_t& read,
                     std::string* error) override {
        if (error) error->clear();
        read = 0U;
        if (requested == 0U) return true;
        if (destination == nullptr) return fail(error, "Audio destination buffer is null.");
        if (requested > std::numeric_limits<std::size_t>::max() / block_align_)
            return fail(error, "Requested audio block is too large.");
        const auto frames = static_cast<std::size_t>(
            std::min<std::uint64_t>(requested, total_frames_ - position_frames_));
        if (frames == 0U) return true;
        const std::size_t bytes = frames * block_align_;
        std::vector<unsigned char> raw(bytes);
        if (!stream_.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(bytes)))
            return fail(error, "Could not read the complete WAV audio block.");
        const std::size_t bytes_per_sample = bits_ / 8U;
        const std::size_t samples = frames * format_.channel_count;
        for (std::size_t i = 0; i < samples; ++i) {
            const auto* p = raw.data() + i * bytes_per_sample;
            float value = 0.0F;
            if (encoding_ == 3U) {
                const std::uint32_t raw_float = u32(p);
                std::memcpy(&value, &raw_float, sizeof(value));
                if (!std::isfinite(value)) return fail(error, "WAV contains a non-finite float sample.");
            } else if (bits_ == 8U) {
                value = (static_cast<int>(p[0]) - 128) / 128.0F;
            } else if (bits_ == 16U) {
                const auto v = static_cast<std::int16_t>(u16(p));
                value = v < 0 ? static_cast<float>(v) / 32768.0F : static_cast<float>(v) / 32767.0F;
            } else if (bits_ == 24U) {
                std::int32_t v = static_cast<std::int32_t>(p[0]) |
                    (static_cast<std::int32_t>(p[1]) << 8U) |
                    (static_cast<std::int32_t>(p[2]) << 16U);
                const std::int64_t signed_v = (v & 0x800000) ? static_cast<std::int64_t>(v) - 0x1000000LL : v;
                value = signed_v < 0 ? static_cast<float>(signed_v / 8388608.0) :
                                       static_cast<float>(signed_v / 8388607.0);
            } else {
                const auto raw_int = u32(p);
                const std::int64_t v = raw_int >= 0x80000000U ?
                    static_cast<std::int64_t>(raw_int) - 0x100000000LL : raw_int;
                value = v < 0 ? static_cast<float>(v / 2147483648.0) :
                                static_cast<float>(v / 2147483647.0);
            }
            destination[i] = value;
        }
        position_frames_ += frames;
        read = frames;
        return true;
    }

private:
    std::ifstream stream_;
    AudioFormat format_{};
    std::uint64_t data_offset_{0U}, data_size_{0U}, total_frames_{0U}, position_frames_{0U};
    std::uint16_t encoding_{0U}, bits_{0U}, block_align_{0U};
};

}  // namespace

std::unique_ptr<AudioDecoder> open_audio_decoder(const std::string& path, std::string* error) {
    auto decoder = std::make_unique<WavStreamDecoder>();
    if (!decoder->open(path, error)) return nullptr;
    return decoder;
}

}  // namespace audio_studio
