// hp-jpegls-test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "portable_anymap_file.h"

#include "hp/jpeglsdll.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using std::byte;
using std::exception;
using std::ifstream;
using std::ios;
using std::istream;
using std::ostream;
using std::string;
using std::stringstream;
using std::vector;
using namespace std::string_literals;

namespace hp {

class jpegls_codec final
{
public:
    jpegls_codec()
    {
        JPEGLS_SetMessageCallback(get(), message_callback, this);
    }

    [[nodiscard]] JPEGLS* get() const
    {
        return codec_.get();
    }

private:
    [[nodiscard]] static JPEGLS* create_codec()
    {
        JPEGLS* codec = JPEGLS_Create();
        if (!codec)
            throw std::bad_alloc();

        return codec;
    }

    static void destroy_codec(JPEGLS* codec) noexcept
    {
        JPEGLS_Destroy(codec);
    }

    static void message_callback(void* context, const char* message)
    {
        auto this_pointer = static_cast<jpegls_codec*>(context);
        this_pointer->last_message_ = message;
    }

    std::string last_message_;
    std::unique_ptr<JPEGLS, void (*)(JPEGLS*)> codec_{create_codec(), destroy_codec};
};

} // namespace hp

namespace {

class source_context_t final
{
public:
    explicit source_context_t(vector<byte> buffer) :
        buffer_{move(buffer)}
    {
    }

    uint read(ubyte* buffer, const uint length)
    {
        const size_t bytes_to_copy = std::min(length, buffer_.size() - position_);

        memcpy(buffer, &buffer_[position_], bytes_to_copy);
        position_ += bytes_to_copy;

        return bytes_to_copy;
    }

private:
    const vector<byte> buffer_;
    size_t position_{};
};

uint read_buffer_callback(void* context, ubyte* buffer, const uint length)
{
    auto source_context = static_cast<source_context_t*>(context);

    return source_context->read(buffer, length);
}

struct destination_context_t final
{
    vector<byte> buffer_;
    size_t position_{};

    bool write(const ubyte* buffer, uint length)
    {
        if (length > buffer_.size() - position_)
            return false;

        memcpy(&buffer_[position_], buffer, length);
        position_ += length;
        return true;
    }
};


BOOL write_buffer_callback(void* context, const ubyte* buffer, const uint length)
{
    auto destination_context = static_cast<destination_context_t*>(context);

    return static_cast<BOOL>(destination_context->write(buffer, length));
}

vector<byte> read_file(const char* filename)
{
    ifstream input;
    input.exceptions(ios::eofbit | ios::failbit | ios::badbit);
    input.open(filename, ios::in | ios::binary);

    input.seekg(0, ios::end);
    const auto byte_count_file = static_cast<int>(input.tellg());
    input.seekg(0, ios::beg);

    vector<byte> buffer(byte_count_file);
    input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());

    return buffer;
}

void save_file(const std::vector<byte>& data, const char* filename)
{
    std::ofstream output;
    output.exceptions(ios::eofbit | ios::failbit | ios::badbit);
    output.open(filename, ios::out | ios::binary);

    output.write(reinterpret_cast<const char*>(data.data()), data.size());
}

size_t bytes_per_sample(const uint alphabet) noexcept
{
    return alphabet > 256 ? 2 : 1;
}

size_t estimated_encoded_size(const int width, const int height, const int component_count, const int bits_per_sample)
{
    return static_cast<size_t>(width) * height *
               component_count * (bits_per_sample < 9 ? 1 : 2) +
           1024;
}

void encode(const char* source_filename, const char* destination_filename)
{
    portable_anymap_file anymap_file{source_filename};

    const hp::jpegls_codec codec;

    JPEGLS_Info jpegls_info;
    JPEGLS_GetDefaultInfo(&jpegls_info);
    jpegls_info.width = anymap_file.width();
    jpegls_info.height = anymap_file.height();
    jpegls_info.components = anymap_file.component_count();
    jpegls_info.scan[0].components = anymap_file.component_count();

    destination_context_t destination_context;
    destination_context.buffer_.resize(estimated_encoded_size(jpegls_info.width,
                                                              jpegls_info.height, anymap_file.component_count(), anymap_file.bits_per_sample()));

    bool result = JPEGLS_StartEncode(codec.get(), write_buffer_callback, &destination_context, &jpegls_info);
    if (!result)
        throw std::exception("JPEGLS_StartEncode");

    source_context_t source_context{anymap_file.image_data()};
    result = JPEGLS_EncodeFromCB(codec.get(), read_buffer_callback, &source_context);
    if (!result)
        throw std::exception("JPEGLS_EncodeFromCB");

    destination_context.buffer_.resize(destination_context.position_);
    save_file(destination_context.buffer_, destination_filename);
}

void decode(const char* source_filename, const char* destination_filename)
{
    vector<byte> source = read_file(source_filename);

    const hp::jpegls_codec codec;

    source_context_t source_context{move(source)};
    bool result = JPEGLS_StartDecode(codec.get(), read_buffer_callback, &source_context);
    if (!result)
        throw std::exception("JPEGLS_StartDecode failed");

    JPEGLS_Info jpegls_info;
    result = JPEGLS_GetInfo(codec.get(), &jpegls_info);
    if (!result)
        throw std::exception("JPEGLS_GetInfo failed");

    const size_t destination_size = jpegls_info.width * jpegls_info.height * jpegls_info.components * bytes_per_sample(jpegls_info.alphabet);
    destination_context_t destination_context;
    destination_context.buffer_.resize(destination_size);

    result = JPEGLS_DecodeToCB(codec.get(), write_buffer_callback, &destination_context);
    if (!result)
        throw std::exception("JPEGLS_DecodeToCB failed");

    portable_anymap_file::save(jpegls_info.width, jpegls_info.height,
                               jpegls_info.components, jpegls_info.alphabet, destination_context.buffer_, destination_filename);
}

} // namespace

int main(const int argc, const char* const argv[])
{
    try
    {
        if (argc < 4)
        {
            std::cout << "usage: hp-jpegls-test <operation (encode | decode> <input> <output>\n";
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
                std::cout << "Unknown operation: " << argv[1] << '\n';
            }
        }
    }
    catch (const exception& error)
    {
        std::cout << "Unexpected failure: " << error.what() << '\n';
    }
}
