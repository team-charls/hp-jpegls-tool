﻿// Copyright (c) Team CharLS.
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <fstream>

namespace charls_test {

// Purpose: this class can read an image stored in the Portable Anymap Format (PNM).
//          The 2 binary formats P5 and P6 are supported:
//          Portable GrayMap: P5 = binary, extension = .pgm, 0-2^16 (gray scale)
//          Portable PixMap: P6 = binary, extension.ppm, range 0-2^16 (RGB)
class portable_anymap_file final
{
public:
    /// <exception cref="ifstream::failure">Thrown when the input file cannot be read.</exception>
    explicit portable_anymap_file(const char* filename)
    {
        std::ifstream pnm_file;
        pnm_file.exceptions(std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit);
        pnm_file.open(filename, std::ifstream::in | std::ifstream::binary);

        std::vector<int> header_info = read_header(pnm_file);
        if (header_info.size() != 4)
            throw std::ios_base::failure("Incorrect PNM header");

        component_count_ = header_info[0] == 6 ? 3 : 1;
        width_ = header_info[1];
        height_ = header_info[2];
        bits_per_sample_ = log_2(header_info[3] + 1);

        const int bytes_per_sample = (bits_per_sample_ + 7) / 8;
        input_buffer_.resize(static_cast<size_t>(width_) * height_ * bytes_per_sample * component_count_);
        pnm_file.read(reinterpret_cast<char*>(input_buffer_.data()), input_buffer_.size());

        convert_to_little_endian_if_needed();
    }

    int width() const noexcept
    {
        return width_;
    }

    int height() const noexcept
    {
        return height_;
    }

    int component_count() const noexcept
    {
        return component_count_;
    }

    int bits_per_sample() const noexcept
    {
        return bits_per_sample_;
    }

    std::vector<uint8_t>& image_data() noexcept
    {
        return input_buffer_;
    }

    const std::vector<uint8_t>& image_data() const noexcept
    {
        return input_buffer_;
    }

    static void save(int width, int height, int component_count, int alphabet, const std::vector<uint8_t>& image_data, const char* filename)
    {
        std::ofstream pnm_file;
        pnm_file.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
        pnm_file.open(filename, std::ios::out | std::ios::binary);

        pnm_file.put('P');
        pnm_file.put('5');
        pnm_file << '\n' << width << ' ' << height << '\n';
        pnm_file << alphabet -1 << '\n';
        pnm_file.write(reinterpret_cast<const char*>(image_data.data()), image_data.size());
    }

private:
    static std::vector<int> read_header(std::istream& pnm_file)
    {
        std::vector<int> result;

        const auto first = static_cast<char>(pnm_file.get());

        // All portable anymap format (PNM) start with the character P.
        if (first != 'P')
            throw std::istream::failure("Missing P");

        while (result.size() < 4)
        {
            std::string bytes;
            std::getline(pnm_file, bytes);
            std::stringstream line(bytes);

            while (result.size() < 4)
            {
                int value = -1;
                line >> value;
                if (value <= 0)
                    break;

                result.push_back(value);
            }
        }
        return result;
    }

    constexpr int32_t log_2(int32_t n) noexcept
    {
        int32_t x = 0;
        while (n > (1 << x))
        {
            ++x;
        }
        return x;
    }

    void convert_to_little_endian_if_needed() noexcept
    {
        // Anymap files with multi byte pixels are stored in big endian format in the file.
        if (bits_per_sample_ > 8)
        {
            for (size_t i = 0; i < input_buffer_.size() - 1; i += 2)
            {
                std::swap(input_buffer_[i], input_buffer_[i + 1]);
            }
        }
    }

    int component_count_;
    int width_;
    int height_;
    int bits_per_sample_;
    std::vector<uint8_t> input_buffer_;
};

} // namespace charls_test
