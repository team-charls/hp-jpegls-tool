// Copyright (c) Team CharLS.
// SPDX-License-Identifier: MIT

import std;
import portable_anymap_file;
import hp.jpegls;
import argparse;
import <cassert>;

using std::byte;
using std::format;
using std::ifstream;
using std::ios;
using std::ofstream;
using std::println;
using std::runtime_error;
using std::span;
using std::string;
using std::string_view;
using std::stringstream;
using std::tuple;
using std::int32_t;
using std::uint32_t;
using std::unexpected;
using std::vector;
using std::chrono::duration;
using std::chrono::steady_clock;
using namespace std::string_literals;
using namespace hp;
using argparse::ArgumentParser;
namespace fs = std::filesystem;

namespace {

constexpr int exit_success{0};
constexpr int exit_failure{1};

const char* const input_argument{"input"};
const char* const output_argument{"output"};
const char* const interleave_mode_argument{"--interleave-mode"};
const char* const near_lossless_argument{"--near-lossless"};
const char* const color_transform_argument{"--color-transform"};
constexpr int32_t default_interleave_mode{-1};

[[nodiscard]]
int32_t get_interleave_mode_argument(const ArgumentParser& command)
{
    const auto interleave_mode{command.present<int32_t>(interleave_mode_argument)};
    return interleave_mode.value_or(default_interleave_mode);
}

[[nodiscard]]
uint32_t get_near_lossless_argument(const ArgumentParser& command)
{
    static constexpr uint32_t default_near_lossless{0};
    const auto near{command.present<uint32_t>(near_lossless_argument)};
    return near.value_or(default_near_lossless);
}

[[nodiscard]]
uint32_t get_color_transform_argument(const ArgumentParser& command)
{
    static constexpr uint32_t default_color_transform{0};
    const auto near{command.present<uint32_t>(color_transform_argument)};
    return near.value_or(default_color_transform);
}

class source_context_t final
{
public:
    explicit source_context_t(vector<byte> buffer) noexcept :
        buffer_{std::move(buffer)}
    {
    }

    [[nodiscard]] static uint32_t read_buffer_callback(void* context, byte* buffer, const uint32_t length) noexcept
    {
        auto* source_context{static_cast<source_context_t*>(context)};
        return source_context->read({buffer, length});
    }

private:
    [[nodiscard]] uint32_t read(const span<byte> buffer) noexcept
    {
        const size_t bytes_to_copy{std::min(buffer.size(), buffer_.size() - position_)};
        memcpy(buffer.data(), &buffer_[position_], bytes_to_copy);
        position_ += bytes_to_copy;

        return bytes_to_copy;
    }

    vector<byte> buffer_;
    size_t position_{};
};


class destination_context_t final
{
public:
    explicit destination_context_t(const size_t size) :
        buffer_(size)
    {
    }

    destination_context_t(const size_t width, const size_t height, const size_t component_count, const size_t bits_per_sample) :
        buffer_(estimated_encoded_size(width, height, component_count, bits_per_sample))
    {
    }

    void resize_buffer()
    {
        const size_t encoded_size{position_};
        buffer_.resize(encoded_size);
    }

    [[nodiscard]] span<const byte> buffer() const noexcept
    {
        return buffer_;
    }

    [[nodiscard]] static BOOL write_buffer_callback(void* context, const byte* buffer, const uint32_t length) noexcept
    {
        if (length == 0)
            return static_cast<BOOL>(true); // Note: calling JPEGLS_Destroy may call callback with 0 bytes.

        auto* destination_context{static_cast<destination_context_t*>(context)};

        return static_cast<BOOL>(destination_context->write({buffer, length}));
    }

    void convert_buffer_to_big_endian() noexcept
    {
        for (size_t i{}; i < buffer_.size() - 1; i += 2)
        {
            std::swap(buffer_[i], buffer_[i + 1]);
        }
    }

private:
    [[nodiscard]] bool write(const span<const byte> buffer) noexcept
    {
        if (buffer.size() > buffer_.size() - position_)
            return false;

        memcpy(&buffer_[position_], buffer.data(), buffer.size());
        position_ += buffer.size();

        return true;
    }

    [[nodiscard]] static constexpr size_t bit_to_byte_count(const size_t bit_count) noexcept
    {
        return (bit_count + 7U) / 8U;
    }

    [[nodiscard]] static constexpr size_t estimated_encoded_size(const size_t width, const size_t height, const size_t component_count, const size_t bits_per_sample)
    {
        return width * height * component_count * bit_to_byte_count(bits_per_sample) + 1024;
    }

    vector<byte> buffer_;
    size_t position_{};
};

void open_stream(auto& stream, const string_view filename, std::ios::openmode mode)
{
    stream.exceptions(ios::eofbit | ios::failbit | ios::badbit);
    stream.open(filename, mode);
}

[[nodiscard]] vector<byte> read_file(const string_view filename)
{
    ifstream input;
    open_stream(input, filename, ios::in | ios::binary);

    input.seekg(0, ios::end);
    const auto byte_count_file = static_cast<size_t>(input.tellg());
    input.seekg(0, ios::beg);

    vector<byte> buffer(byte_count_file);
    input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());

    return buffer;
}

void save_file(const string_view filename, const span<const byte> data)
{
    ofstream output;
    open_stream(output, filename, ios::out | ios::binary);

    output.write(reinterpret_cast<const char*>(data.data()), data.size());
}

[[nodiscard]] constexpr size_t bytes_per_sample(const uint32_t alphabet) noexcept
{
    return alphabet > 256 ? 2U : 1U;
}

void encode(const string_view source_filename, const string_view destination_filename, int32_t interleave_mode, uint32_t near_lossless, uint32_t color_transformation)
{
    portable_anymap_file anymap_file{source_filename};

    const jpegls_codec codec;

    JPEGLS_Info jpegls_info;
    JPEGLS_GetDefaultInfo(&jpegls_info);
    jpegls_info.width = anymap_file.width();
    jpegls_info.height = anymap_file.height();
    jpegls_info.alphabet = 1U << anymap_file.bits_per_sample();
    jpegls_info.components = anymap_file.component_count();
    jpegls_info.scan[0].alphabet = jpegls_info.alphabet;
    jpegls_info.scan[0].loss = near_lossless;
    jpegls_info.scan[0].colorXForm = static_cast<JPEGLS_ColorXForm>(color_transformation);

    if (interleave_mode == default_interleave_mode)
    {
        interleave_mode = anymap_file.component_count() > 1 ? static_cast<int32_t>(JPEGLS_Interleave::pixel) : static_cast<int32_t>(JPEGLS_Interleave::none);
    }

    jpegls_info.scan[0].interleave = static_cast<JPEGLS_Interleave>(interleave_mode);
    if (jpegls_info.scan[0].interleave == JPEGLS_Interleave::none)
    {
        jpegls_info.scan[0].components = 1;
    }
    else 
    {
        jpegls_info.scan[0].components = anymap_file.component_count();
    }

    destination_context_t destination_context(jpegls_info.width,
                                              jpegls_info.height, anymap_file.component_count(), anymap_file.bits_per_sample());

    codec.start_encode(destination_context_t::write_buffer_callback, &destination_context, jpegls_info);

    source_context_t source_context{anymap_file.image_data()};
    const auto start_point{steady_clock::now()};
    codec.encode(source_context_t::read_buffer_callback, &source_context); // Note: encode only works for 8-bit data.
    const auto encode_duration{steady_clock::now() - start_point};

    destination_context.resize_buffer();
    save_file(destination_filename, destination_context.buffer());

    const double compression_ratio{static_cast<double>(anymap_file.image_data().size()) / destination_context.buffer().size()};
    println("Info: original size = {:>10}, interleave mode = {}, near lossless = {}, color transformation = {}",
            anymap_file.image_data().size(), interleave_mode, near_lossless, color_transformation);
    println("      encoded size  = {:>10}, compression ratio = {:.2f}, encode time = {:.4f} ms ",
            destination_context.buffer().size(), compression_ratio, duration<double, std::milli>(encode_duration).count());
}

void decode(const string_view source_filename, const string_view destination_filename)
{
    vector source{read_file(source_filename)};
    source_context_t source_context{std::move(source)};

    const jpegls_codec codec;

    const auto start_point{steady_clock::now()};
    codec.start_decode(source_context_t::read_buffer_callback, &source_context);

    const auto [width, height, alphabet, components, doRestart, restartInterval, samplingX, samplingY, componentId, scanCount, scan]{codec.get_info()};

    const size_t destination_size{width * height * components * bytes_per_sample(alphabet)};
    destination_context_t destination_context{destination_size};

    codec.decode(destination_context_t::write_buffer_callback, &destination_context);
    const auto encode_duration{steady_clock::now() - start_point};

    if (bytes_per_sample(alphabet) > 1U)
    {
        // The HP decoder will return the pixels in little endian.
        // Anymap files with multibyte pixels are stored in big endian format in the file.
        destination_context.convert_buffer_to_big_endian();
    }

    portable_anymap_file::save(destination_filename, width, height,
                               components, alphabet, destination_context.buffer());

    println("Info: decode time = {:.4f} ms", duration<double, std::milli>(encode_duration).count());
}


void log_failure(const runtime_error& error) noexcept
{
    try
    {
        println(std::cerr, "Unexpected failure: {}", error.what());
    }
    catch (...)
    {
        // Catch and ignore all exceptions,to ensure a noexcept log function (but warn in debug builds)
        assert(false);
    }
}

} // namespace


int main(const int argc, const char* const argv[])
{
    ArgumentParser program("hp-jpegls-tool");
    program.add_description("HP JPEG-LS Tool");

    ArgumentParser encode_command("encode");
    encode_command.add_description("Encode a binary Netpbm file to a JPEG-LS file");
    encode_command.add_argument(input_argument).help("The binary Netpbm file to encode to JPEG-LS (required)");
    encode_command.add_argument(output_argument)
        .nargs(0, 1)
        .help("The output JPEG-LS file path. If not specified, the output file is created "
              "with the same name as the input file and a .jls extension");
    encode_command.add_argument(interleave_mode_argument)
        .scan<'i', int32_t>()
        .help("Interleave mode parameter (optional: default = 0 for 1 component, 2 for > 1 component)");
    encode_command.add_argument(near_lossless_argument)
        .scan<'u', uint32_t>()
        .help("NEAR parameter (optional: default = 0)");
    encode_command.add_argument(color_transform_argument)
        .scan<'u', uint32_t>()
        .help("Color transformation parameter (optional: default = 0)");

    program.add_subparser(encode_command);

    ArgumentParser decode_command("decode");
    decode_command.add_description("Decode a JPEG-LS file to a binary Netpbm file");
    decode_command.add_argument(input_argument).help("The JPEG-LS file to decode to a binary Netpbm file (required)");
    decode_command.add_argument(output_argument)
        .nargs(0, 1)
        .help("The output Netpbm file path. If not specified, the output filename is based on the input filename");
    program.add_subparser(decode_command);

    try
    {
        program.parse_args(argc, argv);

        if (program.is_subcommand_used(encode_command))
        {
            const auto input_filename{encode_command.get<string>(input_argument)};
            auto output_filename{encode_command.present<string>(output_argument)};
            if (!output_filename.has_value())
            {
                output_filename = fs::path(input_filename).replace_extension(".jls").string();
            }

            encode(input_filename, *output_filename, get_interleave_mode_argument(encode_command),
                get_near_lossless_argument(encode_command), get_color_transform_argument(encode_command));
        }
        else if (program.is_subcommand_used(decode_command))
        {
            const auto input_filename{decode_command.get<string>(input_argument)};
            auto output_filename{decode_command.present<string>(output_argument)};
            if (!output_filename.has_value())
            {
                output_filename = fs::path(input_filename).replace_extension(".pnm").string();
            }

            decode(input_filename, *output_filename);
        }
        else
        {
            println("{}", program.help().str());
            return exit_failure;
        }

        return exit_success;
    }
    catch (const runtime_error& error)
    {
        log_failure(error);
        return exit_failure;
    }
}
