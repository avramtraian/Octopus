/*
 * Copyright (c) 2023 Traian Avram. All rights reserved.
 * SPDX-License-Identifier: MIT.
 */

#include "Bitmap.h"
#include "MathUtils.h"

#include <PNG/png.h>

namespace Octopus
{

// https://gist.github.com/niw/5963798
ResultOr<OwnPtr<Bitmap>> Bitmap::create_from_file(const String& filepath)
{
    FILE* file_descriptor = nullptr;
    if (fopen_s(&file_descriptor, filepath.c_str(), "rb") != 0)
        return Result(Result::InvalidFilepath);

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
        return Result(Result::UnknownError);

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return Result(Result::UnknownError);
    }

    png_init_io(png_ptr, file_descriptor);
    png_read_info(png_ptr, info_ptr);

    const usize width = png_get_image_width(png_ptr, info_ptr);
    const usize height = png_get_image_height(png_ptr, info_ptr);
    const u8 bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    const u8 color_type = png_get_color_type(png_ptr, info_ptr);

    // Read any color_type into 8bit depth, RGBA format.
    // See http://www.libpng.org/pub/png/libpng-manual.txt

    if (bit_depth == 16)
        png_set_strip_16(png_ptr);

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);

    // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);

    // These color_type don't have an alpha channel then fill it with 0xff.
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    Vector<png_byte*> rows;
    rows.resize(height, nullptr);
    const usize bytes_per_row = png_get_rowbytes(png_ptr, info_ptr);
    for (auto*& row : rows)
        row = static_cast<png_byte*>(png_malloc(png_ptr, width * 4));

    png_read_image(png_ptr, rows.data());

    Vector<Color> pixels;
    pixels.resize(width * height);
    Color* pixel_it = pixels.data();

    for (auto row_it = rows.rbegin(); row_it != rows.rend(); ++row_it)
    {
        const png_byte* row_data = *row_it;
        for (usize x = 0; x < width; ++x)
        {
            pixel_it->red = *row_data++;
            pixel_it->green = *row_data++;
            pixel_it->blue = *row_data++;
            pixel_it->alpha = *row_data++;
            ++pixel_it;
        }
    }

    for (auto* row : rows)
        png_free(png_ptr, row);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    if (fclose(file_descriptor) != 0)
        return Result(Result::UnknownError);

    OwnPtr<Bitmap> bitmap = OwnPtr<Bitmap>(new Bitmap);
    if (!bitmap)
        return Result(Result::OutOfMemory);

    bitmap->m_width = width;
    bitmap->m_height = height;
    bitmap->m_pixels = std::move(pixels);
    return bitmap;
}

ResultOr<OwnPtr<Bitmap>> Bitmap::create_from_memory(usize width, usize height, const Color* pixels)
{
    OwnPtr<Bitmap> bitmap = OwnPtr<Bitmap>(new Bitmap);
    if (!bitmap)
        return Result(Result::OutOfMemory);

    bitmap->m_width = width;
    bitmap->m_height = height;
    bitmap->m_pixels.resize(width * height);
    std::memcpy(bitmap->m_pixels.data(), pixels, bitmap->m_pixels.size() * sizeof(Color));

    return bitmap;
}

ResultOr<OwnPtr<Bitmap>> Bitmap::create_from_color(usize width, usize height, Color solid_color)
{
    OwnPtr<Bitmap> bitmap = OwnPtr<Bitmap>(new Bitmap);
    if (!bitmap)
        return Result(Result::OutOfMemory);

    bitmap->m_width = width;
    bitmap->m_height = height;
    bitmap->m_pixels.resize(width * height);

    for (usize index = 0; index < bitmap->m_pixels.size(); ++index)
        bitmap->m_pixels[index] = solid_color;

    return bitmap;
}

// https://stackoverflow.com/questions/1821806/how-to-encode-png-to-buffer-using-libpng
ResultOr<void> Bitmap::save_to_file(const String& filepath) const
{
    FILE* file_descriptor = nullptr;
    if (fopen_s(&file_descriptor, filepath.c_str(), "wb") != 0)
        return Result(Result::InvalidFilepath);

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
        return Result(Result::UnknownError);

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_write_struct(&png_ptr, NULL);
        return Result(Result::UnknownError);
    }

    TRY_ASSIGN(const u32 png_width, safe_truncate_unsigned<u32>(m_width));
    TRY_ASSIGN(const u32 png_height, safe_truncate_unsigned<u32>(m_height));

    png_set_IHDR(
        png_ptr,
        info_ptr,
        png_width,
        png_height,
        8,
        PNG_COLOR_TYPE_RGB_ALPHA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );

    const usize bytes_per_row = m_width * 4;
    Vector<png_byte*> rows;
    rows.resize(m_height, nullptr);

    for (usize y = 0; y < m_height; ++y)
    {
        png_byte* row_data = static_cast<u8*>(png_malloc(png_ptr, bytes_per_row));
        // NOTE: libpng expects image origin to be the top-left corner, but we use
        // the left-bottom corner as the origin. In order to perform this transformation,
        // we flip the rows of the image.
        rows[m_height - y - 1] = row_data;

        for (usize x = 0; x < m_width; ++x)
        {
            const Color& pixel = m_pixels[y * m_width + x];
            *row_data++ = pixel.red;
            *row_data++ = pixel.green;
            *row_data++ = pixel.blue;
            *row_data++ = pixel.alpha;
        }
    }

    png_init_io(png_ptr, file_descriptor);
    png_set_rows(png_ptr, info_ptr, rows.data());
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    for (auto* row : rows)
        png_free(png_ptr, row);

    png_destroy_write_struct(&png_ptr, &info_ptr);
    if (fclose(file_descriptor) != 0)
        return Result(Result::UnknownError);

    return {};
}

ResultOr<OwnPtr<Bitmap>> Bitmap::rotate(const OwnPtr<Bitmap>& bitmap, u32 rotation_count)
{
    OwnPtr<Bitmap> rotated_bitmap = OwnPtr<Bitmap>(new Bitmap);
    if (!rotated_bitmap)
        return Result(Result::OutOfMemory);

    rotated_bitmap->m_pixels.resize(bitmap->m_width * bitmap->m_height);
    Color* dst_pixels = rotated_bitmap->m_pixels.data();

    const Color* src_pixels = bitmap->pixels();
    const usize src_width = bitmap->width();
    const usize src_height = bitmap->height();

    switch (rotation_count % 4)
    {
        case 1:
        {
            rotated_bitmap->m_width = bitmap->m_height;
            rotated_bitmap->m_height = bitmap->m_width;
            const usize dst_width = rotated_bitmap->m_width;
            const usize dst_height = rotated_bitmap->m_height;

            for (usize source_x = 0; source_x < src_width; ++source_x)
                for (usize source_y = 0; source_y < src_height; ++source_y)
                    dst_pixels[(dst_height - source_x - 1) * dst_width + source_y] =
                        src_pixels[source_y * src_width + source_x];
        }
        break;

        case 2:
        {
            rotated_bitmap->m_width = bitmap->m_width;
            rotated_bitmap->m_height = bitmap->m_height;
            const usize dst_width = rotated_bitmap->m_width;

            for (usize y = 0; y < src_height; ++y)
                for (usize x = 0; x < src_width; ++x)
                    dst_pixels[y * dst_width + x] = src_pixels[(src_height - y - 1) * src_width + (src_width - x - 1)];
        }
        break;

        case 3:
        {
            rotated_bitmap->m_width = bitmap->m_height;
            rotated_bitmap->m_height = bitmap->m_width;
            const usize dst_width = rotated_bitmap->m_width;
            const usize dst_height = rotated_bitmap->m_height;

            for (usize source_x = 0; source_x < src_width; ++source_x)
                for (usize source_y = 0; source_y < src_height; ++source_y)
                    dst_pixels[source_x * dst_width + source_y] =
                        src_pixels[(src_height - source_y - 1) * src_width + source_x];
        }
        break;

        default:
        {
            rotated_bitmap->m_width = bitmap->m_width;
            rotated_bitmap->m_height = bitmap->m_height;
            std::memcpy(dst_pixels, src_pixels, rotated_bitmap->m_pixels.size() * sizeof(Color));
        }
        break;
    }

    return rotated_bitmap;
}

ResultOr<OwnPtr<Bitmap>> Bitmap::flip_horizontal(const OwnPtr<Bitmap>& bitmap)
{
    OwnPtr<Bitmap> flipped_bitmap = OwnPtr<Bitmap>(new Bitmap);
    if (!flipped_bitmap)
        return Result(Result::OutOfMemory);

    flipped_bitmap->m_width = bitmap->m_width;
    flipped_bitmap->m_height = bitmap->m_height;
    flipped_bitmap->m_pixels.resize(flipped_bitmap->m_width * flipped_bitmap->m_height);

    Color* dst_pixels = flipped_bitmap->m_pixels.data();
    const Color* src_pixels = bitmap->pixels();

    for (usize y = 0; y < bitmap->m_height; ++y)
        for (usize x = 0; x < bitmap->m_width; ++x)
            dst_pixels[y * flipped_bitmap->m_width + x] = src_pixels[(bitmap->m_height - y - 1) * bitmap->m_width + x];

    return flipped_bitmap;
}

ResultOr<OwnPtr<Bitmap>> Bitmap::flip_vertical(const OwnPtr<Bitmap>& bitmap)
{
    OwnPtr<Bitmap> flipped_bitmap = OwnPtr<Bitmap>(new Bitmap);
    if (!flipped_bitmap)
        return Result(Result::OutOfMemory);

    flipped_bitmap->m_width = bitmap->m_width;
    flipped_bitmap->m_height = bitmap->m_height;
    flipped_bitmap->m_pixels.resize(flipped_bitmap->m_width * flipped_bitmap->m_height);

    Color* dst_pixels = flipped_bitmap->m_pixels.data();
    const Color* src_pixels = bitmap->pixels();

    for (usize y = 0; y < bitmap->m_height; ++y)
        for (usize x = 0; x < bitmap->m_width; ++x)
            dst_pixels[y * flipped_bitmap->m_width + x] = src_pixels[y * bitmap->m_width + (bitmap->m_width - x - 1)];

    return flipped_bitmap;
}

///
/// https://en.wikipedia.org/wiki/Alpha_compositing#Straight_versus_premultiplied
/// https://stackoverflow.com/questions/43626737/blending-transparent-colors
///
/// This method of blending colors works correctly in all cases, but it is very slow to use.
/// Cheaper solutions exist (if the existing color is always fully opaque for example) and they
/// should be used instead of the following.
///
inline static Color blend_colors(Color existing, Color composed_over)
{
    const float r_b = static_cast<float>(existing.red) * (1.0F / 255.0F);
    const float g_b = static_cast<float>(existing.green) * (1.0F / 255.0F);
    const float b_b = static_cast<float>(existing.blue) * (1.0F / 255.0F);
    const float a_b = static_cast<float>(existing.alpha) * (1.0F / 255.0F);

    const float r_a = static_cast<float>(composed_over.red) * (1.0F / 255.0F);
    const float g_a = static_cast<float>(composed_over.green) * (1.0F / 255.0F);
    const float b_a = static_cast<float>(composed_over.blue) * (1.0F / 255.0F);
    const float a_a = static_cast<float>(composed_over.alpha) * (1.0F / 255.0F);

    const float a_0 = a_a + a_b * (1.0F - a_a);
    const float a_0_inv = 1.0F / a_0;

    const float r_0 = (r_a * a_a + r_b * a_b * (1 - a_a)) * a_0_inv;
    const float g_0 = (g_a * a_a + g_b * a_b * (1 - a_a)) * a_0_inv;
    const float b_0 = (b_a * a_a + b_b * a_b * (1 - a_a)) * a_0_inv;

    Color blended;
    blended.red = static_cast<u8>(r_0 * 255.0F);
    blended.green = static_cast<u8>(g_0 * 255.0F);
    blended.blue = static_cast<u8>(b_0 * 255.0F);
    blended.alpha = static_cast<u8>(a_0 * 255.0F);
    return blended;
}

Color* Bitmap::pixel_at(usize offset_x, usize offset_y)
{
    if (offset_x > m_width || offset_y > m_height)
        return nullptr;
    return m_pixels.data() + ((offset_y * m_width) + offset_x);
}

const Color* Bitmap::pixel_at(usize offset_x, usize offset_y) const
{
    if (offset_x > m_width || offset_y > m_height)
        return nullptr;
    return m_pixels.data() + ((offset_y * m_width) + offset_x);
}

ResultOr<void> Bitmap::fill_rect(const IntRect& rect, Color color)
{
    if (rect.offset_x < 0 || rect.offset_x + rect.width > m_width)
        return Result(Result::InvalidParameter);
    if (rect.offset_y < 0 || rect.offset_y + rect.height > m_height)
        return Result(Result::InvalidParameter);

    for (usize y = rect.offset_y; y < rect.offset_y + rect.height; ++y)
    {
        Color* row_pixels = m_pixels.data() + (y * m_width + rect.offset_x);
        for (usize x = 0; x < rect.width; ++x)
            row_pixels[x] = blend_colors(row_pixels[x], color);
    }

    return {};
}

ResultOr<void> Bitmap::fill_opaque_rect(const IntRect& rect, Color color)
{
    if (rect.offset_x < 0 || rect.offset_x + rect.width > m_width)
        return Result(Result::InvalidParameter);
    if (rect.offset_y < 0 || rect.offset_y + rect.height > m_height)
        return Result(Result::InvalidParameter);

    for (usize y = rect.offset_y; y < rect.offset_y + rect.height; ++y)
    {
        Color* row_pixels = m_pixels.data() + (y * m_width + rect.offset_x);
        for (usize x = 0; x < rect.width; ++x)
            row_pixels[x] = color;
    }

    return {};
}

static IntRect get_cropped_rect(const IntRect& rect, usize width, usize height)
{
    i32 min_x = rect.offset_x;
    i32 min_y = rect.offset_y;
    i64 max_x = rect.offset_x + rect.width;
    i64 max_y = rect.offset_y + rect.height;

    min_x = std::clamp(min_x, 0, static_cast<i32>(width));
    min_y = std::clamp(min_y, 0, static_cast<i32>(height));
    max_x = std::clamp<i64>(max_x, 0, static_cast<i64>(width));
    max_y = std::clamp<i64>(max_y, 0, static_cast<i64>(height));

    return { min_x, min_y, static_cast<u32>((max_x - min_x)), static_cast<u32>((max_y - min_y)) };
}

ResultOr<void> Bitmap::fill_cropped_rect(const IntRect& rect, Color color)
{
    const IntRect cropped_rect = get_cropped_rect(rect, m_width, m_height);

    // The rectangle is degenerated, so there is nothing to fill.
    if (cropped_rect.width == 0 || cropped_rect.height == 0)
        return {};

    TRY(fill_rect(cropped_rect, color));
    return {};
}

ResultOr<void> Bitmap::fill_cropped_opaque_rect(const IntRect& rect, Color color)
{
    const IntRect cropped_rect = get_cropped_rect(rect, m_width, m_height);

    // The rectangle is degenerated, so there is nothing to fill.
    if (cropped_rect.width == 0 || cropped_rect.height == 0)
        return {};

    TRY(fill_opaque_rect(cropped_rect, color));
    return {};
}

ResultOr<void> Bitmap::fill_bitmap(i32 offset_x, i32 offset_y, const OwnPtr<Bitmap>& source_bitmap)
{
    if (offset_x < 0 || offset_x + source_bitmap->m_width > m_width)
        return Result(Result::InvalidParameter);
    if (offset_y < 0 || offset_y + source_bitmap->m_height > m_height)
        return Result(Result::InvalidParameter);

    for (usize y = offset_y; y < offset_y + source_bitmap->m_height; ++y)
    {
        Color* dst_row_pixels = m_pixels.data() + (y * m_width + offset_x);
        const Color* src_row_pixels = source_bitmap->m_pixels.data() + ((y - offset_y) * source_bitmap->m_width);

        for (usize x = 0; x < source_bitmap->m_width; ++x)
            dst_row_pixels[x] = blend_colors(dst_row_pixels[x], src_row_pixels[x]);
    }

    return {};
}

ResultOr<void> Bitmap::fill_opaque_bitmap(i32 offset_x, i32 offset_y, const OwnPtr<Bitmap>& source_bitmap)
{
    if (offset_x < 0 || offset_x + source_bitmap->m_width > m_width)
        return Result(Result::InvalidParameter);
    if (offset_y < 0 || offset_y + source_bitmap->m_height > m_height)
        return Result(Result::InvalidParameter);

    for (usize y = offset_y; y < offset_y + source_bitmap->m_height; ++y)
    {
        Color* dst_row_pixels = m_pixels.data() + (y * m_width + offset_x);
        const Color* src_row_pixels = source_bitmap->m_pixels.data() + ((y - offset_y) * source_bitmap->m_width);

        for (usize x = 0; x < source_bitmap->m_width; ++x)
            dst_row_pixels[x] = src_row_pixels[x];
    }

    return {};
}

ResultOr<void> Bitmap::fill_cropped_bitmap(i32 offset_x, i32 offset_y, const OwnPtr<Bitmap>& source_bitmap)
{
    const IntRect cropped_rect = get_cropped_rect(
        { offset_x, offset_y, static_cast<u32>(source_bitmap->width()), static_cast<u32>(source_bitmap->height()) },
        m_width,
        m_height
    );

    // The rectangle is degenerated, so there is nothing to fill.
    if (cropped_rect.width == 0 || cropped_rect.height == 0)
        return {};

    const i32 bitmap_offset_x = cropped_rect.offset_x - offset_x;
    const i32 bitmap_offset_y = cropped_rect.offset_y - offset_y;

    for (usize y = 0; y < cropped_rect.height; ++y)
    {
        Color* dst_row_pixels = m_pixels.data() + ((cropped_rect.offset_y + y) * m_width + cropped_rect.offset_x);
        const Color* src_row_pixels =
            source_bitmap->m_pixels.data() + ((bitmap_offset_y + y) * source_bitmap->m_width + bitmap_offset_x);

        for (usize x = 0; x < cropped_rect.width; ++x)
            dst_row_pixels[x] = blend_colors(dst_row_pixels[x], src_row_pixels[x]);
    }

    return {};
}

ResultOr<void> Bitmap::fill_cropped_opaque_bitmap(i32 offset_x, i32 offset_y, const OwnPtr<Bitmap>& source_bitmap)
{
    const IntRect cropped_rect = get_cropped_rect(
        { offset_x, offset_y, static_cast<u32>(source_bitmap->width()), static_cast<u32>(source_bitmap->height()) },
        m_width,
        m_height
    );

    // The rectangle is degenerated, so there is nothing to fill.
    if (cropped_rect.width == 0 || cropped_rect.height == 0)
        return {};

    const i32 bitmap_offset_x = cropped_rect.offset_x - offset_x;
    const i32 bitmap_offset_y = cropped_rect.offset_y - offset_y;

    for (usize y = 0; y < cropped_rect.height; ++y)
    {
        Color* dst_row_pixels = m_pixels.data() + ((cropped_rect.offset_y + y) * m_width + cropped_rect.offset_x);
        const Color* src_row_pixels =
            source_bitmap->m_pixels.data() + ((bitmap_offset_y + y) * source_bitmap->m_width + bitmap_offset_x);

        for (usize x = 0; x < cropped_rect.width; ++x)
            dst_row_pixels[x] = src_row_pixels[x];
    }

    return {};
}

} // namespace Octopus
