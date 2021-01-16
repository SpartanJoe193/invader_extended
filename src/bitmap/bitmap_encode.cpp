// SPDX-License-Identifier: GPL-3.0-only

#include <invader/bitmap/bitmap_encode.hpp>
#include <invader/tag/hek/class/bitmap.hpp>
#include <invader/bitmap/color_plate_pixel.hpp>
#include <squish.h>

namespace Invader::BitmapEncode {
    static std::vector<std::byte> decode_to_32_bit(const std::byte *input_data, HEK::BitmapDataFormat input_format, std::size_t width, std::size_t height);
    
    void encode_bitmap(const std::byte *input_data, HEK::BitmapDataFormat input_format, std::byte *output_data, HEK::BitmapDataFormat output_format, std::size_t width, std::size_t height) {
        auto as_32_bit = decode_to_32_bit(input_data, input_format, width, height);
        
        // If it's already 32-bit ARGB, output it
        if(output_format == HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_A8R8G8B8) {
            std::memcpy(output_data, as_32_bit.data(), as_32_bit.size());
            return;
        }
        
        // If it's 32-bit XRGB, copy it but then set the alpha to 0xFF
        if(output_format == HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_X8R8G8B8) {
            std::memcpy(output_data, as_32_bit.data(), as_32_bit.size());
            auto *start = reinterpret_cast<ColorPlatePixel *>(output_data);
            auto *end = reinterpret_cast<ColorPlatePixel *>(output_data + as_32_bit.size());
            for(auto *i = start; i < end; i++) {
                i->alpha = 0xFF;
            }
            return;
        }
        
        eprintf_error("TODO: encode_bitmap() should encode more stuff");
        std::terminate();
    }
    
    std::vector<std::byte> encode_bitmap(const std::byte *input_data, HEK::BitmapDataFormat input_format, HEK::BitmapDataFormat output_format, std::size_t width, std::size_t height) {
        // Get our output buffer
        std::vector<std::byte> output(bitmap_data_size(input_format, width, height));
        
        // Do it
        encode_bitmap(input_data, input_format, output.data(), output_format, width, height);

        // Done
        return output;
    }
    
    std::size_t bitmap_data_size(HEK::BitmapDataFormat format, std::size_t width, std::size_t height) {
        // Determine the number of bits per pixel
        auto output_bits_per_pixel = HEK::calculate_bits_per_pixel(format);
        if(output_bits_per_pixel == 0) {
            eprintf_error("Invalid output format");
            throw std::exception();
        }
        
        // DXT stuff is stored in 4x4 blocks
        std::size_t container_width, container_height;
        switch(format) {
            case HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_DXT1:
            case HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_DXT3:
            case HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_DXT5:
                container_width = std::max(width, static_cast<std::size_t>(4));
                container_height = std::max(height, static_cast<std::size_t>(4));
                break;
            default:
                container_width = width;
                container_height = height;
                break;
        }
        
        // Done
        return (container_width * container_height * output_bits_per_pixel) / 8;
    }
    
    static std::vector<std::byte> decode_to_32_bit(const std::byte *input_data, HEK::BitmapDataFormat input_format, std::size_t width, std::size_t height) {
        // First, decode it to 32-bit A8R8G8B8 if necessary
        std::size_t pixel_count = width * height;
        std::vector<HEK::LittleEndian<std::uint32_t>> data(pixel_count);
        
        auto decode_8_bit = [&pixel_count, &input_data, &data](ColorPlatePixel (*with_what)(std::uint8_t)) {
            auto pixels_left = pixel_count;
            auto *bytes_to_add = input_data;
            auto *bytes_to_write = data.data();
            while(pixels_left) {
                auto from_pixel = *reinterpret_cast<const std::uint8_t *>(bytes_to_add);
                auto to_pixel = with_what(from_pixel);
                *bytes_to_write = ((static_cast<std::uint32_t>(to_pixel.alpha) << 24) | (static_cast<std::uint32_t>(to_pixel.red) << 16) | (static_cast<std::uint32_t>(to_pixel.green) << 8) | static_cast<std::uint32_t>(to_pixel.blue));

                pixels_left --;
                bytes_to_add += sizeof(from_pixel);
                bytes_to_write ++;
            }
        };

        auto decode_16_bit = [&pixel_count, &input_data, &data](ColorPlatePixel (*with_what)(std::uint16_t)) {
            auto pixels_left = pixel_count;
            auto *bytes_to_add = input_data;
            auto *bytes_to_write = data.data();
            while(pixels_left) {
                auto from_pixel = *reinterpret_cast<const std::uint16_t *>(bytes_to_add);
                auto to_pixel = with_what(from_pixel);
                *bytes_to_write = ((static_cast<std::uint32_t>(to_pixel.alpha) << 24) | (static_cast<std::uint32_t>(to_pixel.red) << 16) | (static_cast<std::uint32_t>(to_pixel.green) << 8) | static_cast<std::uint32_t>(to_pixel.blue));

                pixels_left --;
                bytes_to_add += sizeof(from_pixel);
                bytes_to_write ++;
            }
        };
        
        auto decode_dxt = [&width, &height, &input_format, &data, &input_data]() {
            int flags = squish::kSourceBGRA;
            
            switch(input_format) {
                case HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_DXT1:
                    flags |= squish::kDxt1;
                    break;

                case HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_DXT3:
                    flags |= squish::kDxt3;
                    break;

                case HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_DXT5:
                    flags |= squish::kDxt5;
                    break;
                    
                default:
                    std::terminate();
            }
            
            squish::DecompressImage(reinterpret_cast<squish::u8 *>(data.data()), width, height, input_data, flags);
            
            for(auto &color : data) {
                // Swap red and blue
                color = (color & 0xFF00FF00) | ((color & 0xFF0000) >> 16) | ((color & 0xFF) << 16);
            }
        };
        
        switch(input_format) {
            case HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_DXT1:
            case HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_DXT3:
            case HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_DXT5:
                decode_dxt();
                break;
                
            case HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_A8R8G8B8:
                std::memcpy(reinterpret_cast<std::byte *>(data.data()), input_data, data.size() * sizeof(data[0]));
                break;
            
            case HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_X8R8G8B8:
                std::memcpy(reinterpret_cast<std::byte *>(data.data()), input_data, data.size() * sizeof(data[0]));
                for(std::size_t i = 0; i < pixel_count; i++) {
                    data[i] = data[i].read() | static_cast<std::uint32_t>(0xFF000000);
                }
                break;

            // 16-bit color
            case HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_A1R5G5B5:
                decode_16_bit(ColorPlatePixel::convert_from_16_bit<1,5,5,5>);
                break;
            case HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_R5G6B5:
                decode_16_bit(ColorPlatePixel::convert_from_16_bit<0,5,6,5>);
                break;
            case HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_A4R4G4B4:
                decode_16_bit(ColorPlatePixel::convert_from_16_bit<4,4,4,4>);
                break;

            // Monochrome
            case HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_A8Y8:
                decode_16_bit(ColorPlatePixel::convert_from_a8y8);
                break;
            case HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_A8:
                decode_8_bit(ColorPlatePixel::convert_from_a8);
                break;
            case HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_Y8:
                decode_8_bit(ColorPlatePixel::convert_from_y8);
                break;
            case HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_AY8:
                decode_8_bit(ColorPlatePixel::convert_from_ay8);
                break;

            // p8
            case HEK::BitmapDataFormat::BITMAP_DATA_FORMAT_P8_BUMP:
                decode_8_bit(ColorPlatePixel::convert_from_p8);
                break;
                
            default:
                throw std::exception();
        }
        
        return std::vector<std::byte>(reinterpret_cast<std::byte *>(data.data()), reinterpret_cast<std::byte *>(data.data()) + data.size() * sizeof(data[0]));
    }
}
