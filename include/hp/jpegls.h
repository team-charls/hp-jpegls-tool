#pragma once

#include "jpeglsdll.h"

#include <string>
#include <exception>
#include <memory>

namespace hp {

class jpegls_codec final
{
public:
    jpegls_codec() noexcept(false)
    {
        JPEGLS_SetMessageCallback(get(), message_callback, this);
    }

    void start_decode(const JPEGLS_ReadBufCallback read_callback, void* context) const
    {
        const bool result = static_cast<bool>(JPEGLS_StartDecode(get(), read_callback, context));
        if (!result)
            throw std::exception(("JPEGLS_StartDecode failed: " + last_message_).c_str());
    }

    void start_encode(const JPEGLS_WriteBufCallback write_buffer_callback, void* context, const JPEGLS_Info& info) const
    {
        const bool result = static_cast<bool>(JPEGLS_StartEncode(get(), write_buffer_callback, context, &info));
        if (!result)
            throw std::exception(("JPEGLS_StartEncode: " + last_message_).c_str());
    }

    [[nodiscard]] JPEGLS_Info get_info() const
    {
        JPEGLS_Info jpegls_info;
        const bool result = static_cast<bool>(JPEGLS_GetInfo(get(), &jpegls_info));
        if (!result)
            throw std::exception(("JPEGLS_GetInfo failed: " + last_message_).c_str());

        return jpegls_info;
    }

    void decode(const JPEGLS_WriteBufCallback write_buffer_callback, void* context) const
    {
        const bool result = static_cast<bool>(JPEGLS_DecodeToCB(get(), write_buffer_callback, context));
        if (!result)
            throw std::exception(("JPEGLS_DecodeToCB failed: " + last_message_).c_str());
    }

    void encode(JPEGLS_ReadBufCallback read_buffer_callback, void* context) const
    {
        const bool result = static_cast<bool>(JPEGLS_EncodeFromCB(get(), read_buffer_callback, context));
        if (!result)
            throw std::exception(("JPEGLS_EncodeFromCB: " + last_message_).c_str());
    }

    [[nodiscard]] JPEGLS* get() const noexcept
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

    mutable std::string last_message_; // Note: mutable as it can be updated by means of a callback function.
    std::unique_ptr<JPEGLS, void (*)(JPEGLS*)> codec_{create_codec(), destroy_codec};
};

} // namespace hp
