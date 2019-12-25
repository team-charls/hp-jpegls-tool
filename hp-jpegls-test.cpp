// hp-jpegls-test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "portable_anymap_file.h"

#include "hp/jpeglsdll.h"

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <filesystem>

using std::ios;
using std::ios_base;
using std::vector;
using std::istream;
using std::ostream;
using std::stringstream;
using std::string;
using std::ifstream;
using std::byte;
using namespace std::string_literals;

const ios::openmode mode_input = ios::in | ios::binary;
const ios::openmode mode_output = ios::out | ios::binary;

constexpr int32_t log_2(int32_t n) noexcept
{
    int32_t x = 0;
    while (n > (1 << x))
    {
        ++x;
    }
    return x;
}

struct source_context_t
{
    std::vector<uint8_t> buffer;
};

struct frame_info
{
    uint32_t width;
    uint32_t height;
    uint32_t component_count;
    uint32_t bits_per_sample;
};


uint ReadBufCallback(void* context, ubyte* buffer, uint len)
{
    auto source_context = static_cast<source_context_t*>(context);

    return len;
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


BOOL WriteBufCallback(void* context, const ubyte* buffer, uint length)
{
    auto destination_context = static_cast<destination_context_t*>(context);

    return static_cast<BOOL>(destination_context->write(buffer, length));
}


vector<int> read_pnm_header(istream& pnmFile)
{
    vector<int> readValues;

    const auto first = static_cast<char>(pnmFile.get());

    // All portable anymap format (PNM) start with the character P.
    if (first != 'P')
        return readValues;

    while (readValues.size() < 4)
    {
        string bytes;
        getline(pnmFile, bytes);
        stringstream line(bytes);

        while (readValues.size() < 4)
        {
            int value = -1;
            line >> value;
            if (value <= 0)
                break;

            readValues.push_back(value);
        }
    }
    return readValues;
}



// Purpose: this function can encode an image stored in the Portable Anymap Format (PNM)
//          into the JPEG-LS format. The 2 binary formats P5 and P6 are supported:
//          Portable GrayMap: P5 = binary, extension = .pgm, 0-2^16 (gray scale)
//          Portable PixMap: P6 = binary, extension.ppm, range 0-2^16 (RGB)
bool encode_pnm(istream& pnm_source, const ostream& jls_destination)
{
    vector<int> read_values = read_pnm_header(pnm_source);
    if (read_values.size() != 4)
        return false;

    JPEGLS* encoder = JPEGLS_Create();
    if (!encoder)
        return false;

    JPEGLS_Info jpegls_info;
    JPEGLS_GetDefaultInfo(&jpegls_info);

    jpegls_info.components = read_values[0] == 6 ? 3 : 1;
    jpegls_info.width = read_values[1];
    jpegls_info.height = read_values[2];
    jpegls_info.scan[0].components = jpegls_info.components;
    jpegls_info.scan[0].interleave = INTERLEAVE_PIXEL;

    int bitsPerSample = log_2(read_values[3] + 1);
    //params.interleaveMode = params.components == 3 ? InterleaveMode::Line : InterleaveMode::None;
    //params.colorTransformation = charls::ColorTransformation::HP2;

    const int bytesPerSample = (bitsPerSample + 7) / 8;
    vector<uint8_t> inputBuffer(static_cast<size_t>(jpegls_info.width)* jpegls_info.height* bytesPerSample* jpegls_info.components);
    pnm_source.read(reinterpret_cast<char*>(inputBuffer.data()), inputBuffer.size());
    if (!pnm_source.good())
        return false;

    destination_context_t destination_context;
    source_context_t source_context;

    source_context.buffer = move(inputBuffer);
    destination_context.buffer_.resize(source_context.buffer.size());

    if (!JPEGLS_StartEncode(encoder, WriteBufCallback, &destination_context, &jpegls_info))
    {
        return false;
    }

    // encode all, reading from the raw file inFP
    if (!JPEGLS_EncodeFromCB(encoder, ReadBufCallback, &source_context))
    {
        return false;
    }


    JPEGLS_Destroy(encoder);


    //// PNM format is stored with most significant byte first (big endian).
    //if (bytesPerSample == 2)
    //{
    //    for (auto i = inputBuffer.begin(); i != inputBuffer.end(); i += 2)
    //    {
    //        iter_swap(i, i + 1);
    //    }
    //}

    //const auto rawStreamInfo = FromByteArray(inputBuffer.data(), inputBuffer.size());
    //const ByteStreamInfo jlsStreamInfo = { jls_destination.rdbuf(), nullptr, 0 };

    //size_t bytesWritten = 0;
    //JpegLsEncodeStream(jlsStreamInfo, bytesWritten, rawStreamInfo, params);
    return true;
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


class source_context_t2 final
{
public:
    explicit source_context_t2(vector<byte> buffer) :
        buffer_{ move(buffer) }
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


uint read_buffer_callback(void* context, ubyte* buffer, uint length)
{
    auto source_context = static_cast<source_context_t2*>(context);

    return source_context->read(buffer, length);
}

size_t bytes_per_sample(const uint alphabet)
{
    return alphabet > 256 ? 2 : 1;
}

size_t estimated_encoded_size(const int width, const int height, const int component_count, const int bits_per_sample)
{
    return static_cast<size_t>(width)* height*
        component_count* (bits_per_sample < 9 ? 1 : 2) + 1024;
}


void encode(const char* source_filename, const char* destination_filename)
{
    portable_anymap_file anymap_file{ source_filename };

    JPEGLS* codec = JPEGLS_Create();

    JPEGLS_Info jpegls_info;
    JPEGLS_GetDefaultInfo(&jpegls_info);
    jpegls_info.width = anymap_file.width();
    jpegls_info.height = anymap_file.height();
    jpegls_info.components = anymap_file.component_count();
    jpegls_info.scan[0].components = anymap_file.component_count();

    destination_context_t destination_context;
    destination_context.buffer_.resize(estimated_encoded_size(jpegls_info.width,
        jpegls_info.height, anymap_file.component_count(), anymap_file.bits_per_sample()));

    bool result = JPEGLS_StartEncode(codec, WriteBufCallback, &destination_context, &jpegls_info);
    if (!result)
        throw std::exception("JPEGLS_StartEncode");

    source_context_t2 source_context{ anymap_file.image_data() };
    result = JPEGLS_EncodeFromCB(codec, read_buffer_callback, &source_context);
    if (!result)
        throw std::exception("JPEGLS_EncodeFromCB");

    destination_context.buffer_.resize(destination_context.position_);
    save_file(destination_context.buffer_, destination_filename);
}

void decode(const char* source_filename, const char* destination_filename)
{
    vector<byte> source = read_file(source_filename);

    JPEGLS* codec = JPEGLS_Create();

    source_context_t2 source_context{ move(source) };
    bool result = JPEGLS_StartDecode(codec, read_buffer_callback, &source_context);
    if (!result)
        throw std::exception("JPEGLS_StartDecode failed");

    JPEGLS_Info jpegls_info;
    result = JPEGLS_GetInfo(codec, &jpegls_info);
    if (!result)
        throw std::exception("JPEGLS_GetInfo failed");

    const size_t destination_size = jpegls_info.width * jpegls_info.height * jpegls_info.components * bytes_per_sample(jpegls_info.alphabet);
    destination_context_t destination_context;
    destination_context.buffer_.resize(destination_size);

    result = JPEGLS_DecodeToCB(codec, WriteBufCallback, &destination_context);
    if (!result)
        throw std::exception("JPEGLS_DecodeToCB failed");

    JPEGLS_Destroy(codec);

    portable_anymap_file::save(jpegls_info.width, jpegls_info.height,
        jpegls_info.components, jpegls_info.alphabet, destination_context.buffer_, destination_filename);
}


int main(const int argc, const char* const argv[])
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
