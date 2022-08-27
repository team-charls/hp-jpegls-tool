// Copyright (c) Team CharLS.
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cassert>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <span>

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
        pnm_file.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
        pnm_file.open(filename, std::ios::in | std::ios::binary);

        const std::vector header_info{read_header(pnm_file)};
        if (header_info.size() != 4)
            throw std::ios_base::failure("Incorrect PNM header");

        component_count_ = header_info[0] == 6 ? 3 : 1;
        width_ = header_info[1];
        height_ = header_info[2];
        bits_per_sample_ = max_value_to_bits_per_sample(header_info[3]);

        const size_t bytes_per_sample{(bits_per_sample_ + 7) / 8};
        input_buffer_.resize(width_ * height_ * bytes_per_sample * component_count_);
        pnm_file.read(reinterpret_cast<char*>(input_buffer_.data()), input_buffer_.size());

        convert_to_little_endian_if_needed();
    }

    [[nodiscard]] size_t width() const noexcept
    {
        return width_;
    }

    [[nodiscard]] size_t height() const noexcept
    {
        return height_;
    }

    [[nodiscard]] size_t component_count() const noexcept
    {
        return component_count_;
    }

    [[nodiscard]] size_t bits_per_sample() const noexcept
    {
        return bits_per_sample_;
    }

    [[nodiscard]] std::vector<std::byte>& image_data() noexcept
    {
        return input_buffer_;
    }

    [[nodiscard]] const std::vector<std::byte>& image_data() const noexcept
    {
        return input_buffer_;
    }

    static void save(const char* filename, const size_t width, const size_t height, const size_t component_count, const size_t alphabet, const std::span<const std::byte> image_data)
    {
        std::ofstream output;
        output.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
        output.open(filename, std::ios::out | std::ios::binary);

        const int component_count_id{component_count == 3 ? 6 : 5};
        output << 'P' << component_count_id << '\n' << width << ' ' << height << '\n' << alphabet - 1 << '\n';
        output.write(reinterpret_cast<const char*>(image_data.data()), image_data.size());
    }

private:
    [[nodiscard]] static std::vector<size_t> read_header(std::istream& pnm_file)
    {
        std::vector<size_t> result;

        // All portable anymap format (PNM) start with the character P.
        if (const auto first{static_cast<char>(pnm_file.get())}; first != 'P')
            throw std::istream::failure("Missing P");

        while (result.size() < 4)
        {
            std::string bytes;
            std::getline(pnm_file, bytes);
            std::stringstream line(bytes);

            while (result.size() < 4)
            {
                int value{-1};
                line >> value;
                if (value <= 0)
                    break;

                result.push_back(static_cast<size_t>(value));
            }
        }
        return result;
    }

    [[nodiscard]] static constexpr uint32_t log2_floor(const uint32_t n) noexcept
    {
        assert(n != 0 && "log2 is not defined for 0");
        return 31 - std::countl_zero(n);
    }

    [[nodiscard]] static constexpr uint32_t max_value_to_bits_per_sample(const uint32_t max_value) noexcept
    {
        assert(max_value > 0);
        return log2_floor(max_value) + 1;
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

    size_t component_count_;
    size_t width_;
    size_t height_;
    size_t bits_per_sample_;
    std::vector<std::byte> input_buffer_;
};
