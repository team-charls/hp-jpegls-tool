// Copyright (c) Team CharLS.
// SPDX-License-Identifier: MIT

#include "pch.h"

#include "portable_anymap_file.h"

#include <hp/jpegls.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

using std::byte;
using std::cout;
using std::exception;
using std::ifstream;
using std::ios;
using std::istream;
using std::ostream;
using std::span;
using std::string;
using std::stringstream;
using std::vector;
using std::chrono::steady_clock;
using namespace std::string_literals;
using namespace hp;

namespace {

class source_context_t final
{
public:
    explicit source_context_t(vector<byte> buffer) noexcept :
        buffer_{move(buffer)}
    {
    }

    static uint32_t read_buffer_callback(void* context, byte* buffer, const uint32_t length) noexcept
    {
        auto* source_context = static_cast<source_context_t*>(context);

        return source_context->read({buffer, length});
    }

private:
    uint32_t read(const span<byte> buffer) noexcept
    {
        const size_t bytes_to_copy = std::min(buffer.size(), buffer_.size() - position_);

        memcpy(buffer.data(), &buffer_[position_], bytes_to_copy);
        position_ += bytes_to_copy;

        return bytes_to_copy;
    }

    const vector<byte> buffer_;
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
        const size_t encoded_size = position_;
        buffer_.resize(encoded_size);
    }

    [[nodiscard]] const vector<byte>& buffer() const noexcept
    {
        return buffer_;
    }

    static BOOL write_buffer_callback(void* context, const byte* buffer, const uint32_t length) noexcept
    {
        if (length == 0)
            return static_cast<BOOL>(true); // Note: calling JPEGLS_Destroy may call callback with 0 bytes.

        auto* destination_context = static_cast<destination_context_t*>(context);

        return static_cast<BOOL>(destination_context->write({buffer, length}));
    }

private:
    bool write(const std::span<const byte> buffer) noexcept
    {
        if (buffer.size() > buffer_.size() - position_)
            return false;

        memcpy(&buffer_[position_], buffer.data(), buffer.size());
        position_ += buffer.size();

        return true;
    }

    static constexpr size_t bit_to_byte_count(const size_t bit_count) noexcept
    {
        return (bit_count + 7U) / 8U;
    }

    static constexpr size_t estimated_encoded_size(const size_t width, const size_t height, const size_t component_count, const size_t bits_per_sample)
    {
        return width * height * component_count * bit_to_byte_count(bits_per_sample) + 1024;
    }

    vector<byte> buffer_;
    size_t position_{};
};


vector<byte> read_file(const char* filename)
{
    ifstream input;
    input.exceptions(ios::eofbit | ios::failbit | ios::badbit);
    input.open(filename, ios::in | ios::binary);

    input.seekg(0, ios::end);
    const auto byte_count_file = static_cast<size_t>(input.tellg());
    input.seekg(0, ios::beg);

    vector<byte> buffer(byte_count_file);
    input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());

    return buffer;
}

void save_file(const char* filename, const span<const byte> data)
{
    std::ofstream output;
    output.exceptions(ios::eofbit | ios::failbit | ios::badbit);
    output.open(filename, ios::out | ios::binary);

    output.write(reinterpret_cast<const char*>(data.data()), data.size());
}

constexpr size_t bytes_per_sample(const uint32_t alphabet) noexcept
{
    return alphabet > 256 ? 2 : 1;
}


void encode(const char* source_filename, const char* destination_filename)
{
    portable_anymap_file anymap_file{source_filename};

    const jpegls_codec codec;

    JPEGLS_Info jpegls_info;
    JPEGLS_GetDefaultInfo(&jpegls_info);
    jpegls_info.width = anymap_file.width();
    jpegls_info.height = anymap_file.height();
    jpegls_info.components = anymap_file.component_count();
    jpegls_info.alphabet = 1 << anymap_file.bits_per_sample();
    jpegls_info.scan[0].components = anymap_file.component_count();
    jpegls_info.scan[0].interleave = anymap_file.component_count() > 1 ? JPEGLS_Interleave::line : JPEGLS_Interleave::none;

    destination_context_t destination_context(jpegls_info.width,
                                              jpegls_info.height, anymap_file.component_count(), anymap_file.bits_per_sample());

    codec.start_encode(destination_context_t::write_buffer_callback, &destination_context, jpegls_info);

    source_context_t source_context{anymap_file.image_data()};

    const auto start = steady_clock::now();
    const bool result = static_cast<bool>(JPEGLS_EncodeFromCB(codec.get(), source_context_t::read_buffer_callback, &source_context));
    const auto encode_duration = steady_clock::now() - start;

    if (!result)
        throw std::exception(codec.last_message().empty() ? "JPEGLS_EncodeFromCB" : codec.last_message().c_str());

    destination_context.resize_buffer();
    save_file(destination_filename, destination_context.buffer());

    const double compression_ratio = static_cast<double>(anymap_file.image_data().size()) / destination_context.buffer().size();
    cout << "Info: original size = " << anymap_file.image_data().size() << ", encoded size = " << destination_context.buffer().size()
         << ", compression ratio = " << std::setprecision(2) << compression_ratio << ":1"
         << ", encode time = " << std::setprecision(4) << std::chrono::duration<double, std::milli>(encode_duration).count() << " ms\n";
}

void decode(const char* source_filename, const char* destination_filename)
{
    vector<byte> source = read_file(source_filename);

    const jpegls_codec codec;

    source_context_t source_context{move(source)};

    const auto start = steady_clock::now();
    codec.start_decode(source_context_t::read_buffer_callback, &source_context);

    const JPEGLS_Info jpegls_info{codec.get_info()};

    const size_t destination_size = jpegls_info.width * jpegls_info.height * jpegls_info.components * bytes_per_sample(jpegls_info.alphabet);
    destination_context_t destination_context{destination_size};

    codec.decode(destination_context_t::write_buffer_callback, &destination_context);
    const auto encode_duration = steady_clock::now() - start;

    portable_anymap_file::save(destination_filename, jpegls_info.width, jpegls_info.height,
                               jpegls_info.components, jpegls_info.alphabet, destination_context.buffer());

    cout << "Info: decode time = " << std::setprecision(4) << std::chrono::duration<double, std::milli>(encode_duration).count() << " ms\n";
}

void log_failure(const exception& error) noexcept
{
    try
    {
        std::cerr << "Unexpected failure: " << error.what() << '\n';
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
    try
    {
        if (argc < 4)
        {
            cout << "usage: hp-jpegls-test <operation (encode | decode> <input> <output>\n";
            return EXIT_FAILURE;
        }

        if (argv[1] == "encode"s)
        {
            encode(argv[2], argv[3]);
        }
        else
        {
            if (argv[1] == "decode"s)
            {
                decode(argv[2], argv[3]);
            }
            else
            {
                cout << "Unknown operation: " << argv[1] << '\n';
            }
        }

        return EXIT_SUCCESS;
    }
    catch (const exception& error)
    {
        log_failure(error);
        return EXIT_FAILURE;
    }
}
