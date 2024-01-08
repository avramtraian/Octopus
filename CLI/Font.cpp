/*
 * Copyright (c) 2023 Traian Avram. All rights reserved.
 * SPDX-License-Identifier: MIT.
 */

#include "Font.h"

#define STB_TRUETYPE_IMPLEMENTATION 1
#include <stb_truetype.h>

namespace Octopus
{

ResultOr<OwnPtr<Font>> Font::create_from_ttf(const String& filepath, float font_height, Color color)
{
    OwnPtr<Font> font = OwnPtr<Font>(new Font);

    FILE* file_descriptor = nullptr;
    if (fopen_s(&file_descriptor, filepath.c_str(), "rb") != 0)
        return Result(Result::InvalidFilepath);

    if (fseek(file_descriptor, 0, SEEK_END) != 0)
        return Result(Result::FileError);

    const usize file_size = ftell(file_descriptor);

    if (fseek(file_descriptor, 0, SEEK_SET) != 0)
        return Result(Result::FileError);

    Vector<u8> file_buffer;
    file_buffer.resize(file_size);
    if (fread_s(file_buffer.data(), file_buffer.size(), 1, file_size, file_descriptor) != file_size)
        return Result(Result::FileError);

    if (fclose(file_descriptor) != 0)
        return Result(Result::FileError);

    stbtt_fontinfo font_info;
    stbtt_InitFont(&font_info, file_buffer.data(), stbtt_GetFontOffsetForIndex(file_buffer.data(), 0));
    const float scale = stbtt_ScaleForPixelHeight(&font_info, font_height);

    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font_info, &ascent, &descent, &line_gap);
    font->m_ascent = static_cast<i32>((static_cast<float>(ascent) * scale));
    // NOTE: Negating the descent value might be confusing in the future and cause a lot of bugs
    // that would be hard to track down, so consider refactoring it.
    font->m_descent = static_cast<i32>((static_cast<float>(-descent) * scale));
    font->m_line_gap = static_cast<i32>((static_cast<float>(line_gap) * scale));

    int space_advance;
    stbtt_GetCodepointHMetrics(&font_info, ' ', &space_advance, nullptr);
    font->m_space_advance = static_cast<i32>((static_cast<float>(space_advance) * scale));

    for (u32 codepoint = '!'; codepoint <= '~'; ++codepoint)
    {
        // The codepoint has already been rasterized, so there is no need to repeat the process.
        // NOTE: This is obviously caused by a bug, so maybe it would be better to fail and report it as an error?
        if (font->m_glyphs.contains(codepoint))
            continue;

        int width, height, offset_x, offset_y;
        u8* raw_font_bitmap = stbtt_GetCodepointBitmap(
            &font_info, 0, scale, static_cast<int>(codepoint), &width, &height, &offset_x, &offset_y
        );
        auto font_bitmap = OwnPtr<u8[]>(raw_font_bitmap);

        int advance;
        stbtt_GetCodepointHMetrics(&font_info, static_cast<int>(codepoint), &advance, nullptr);
        advance = static_cast<i32>((static_cast<float>(advance) * scale));

        Glyph glyph = {};
        glyph.advance = advance;
        glyph.offset_x = offset_x;

        // NOTE: Because the stb_truetype library uses top-left as the coordinate origin system, the value
        // of the offset on the Y axis is essentially flipped.
        glyph.offset_y = -(height + offset_y);

        // NOTE: Because the stb_truetype library considers the origin of the bitmap as top-left, we
        // need to flip the image vertically. This is why we can't just copy the memory to out bitmap.
        TRY_ASSIGN(glyph.bitmap, Bitmap::create_from_color(width, height, { 0, 0, 0, 0 }));

        const u8* font_bitmap_it = font_bitmap.get();
        for (usize y = 0; y < glyph.bitmap->height(); ++y)
        {
            // NOTE: Our bitmap origin is bottom-left, so we need to flip the stb_truetype bitmap.
            Color* dst_row = glyph.bitmap->pixel_at(0, glyph.bitmap->height() - y - 1);

            for (usize x = 0; x < glyph.bitmap->width(); ++x)
            {
                const u8 alpha = *font_bitmap_it++;
                dst_row[x] = { color.red, color.green, color.blue, alpha };
            }
        }

        font->m_glyphs.insert({ codepoint, std::move(glyph) });
    }

    for (const auto& [codepoint_a, glyph_a] : font->m_glyphs)
    {
        for (const auto& [codepoint_b, glyph_b] : font->m_glyphs)
        {
            const u64 kerning_table_key = (static_cast<u64>(codepoint_a) << 32) | static_cast<u64>(codepoint_b);
            if (font->m_kerning_table.contains(kerning_table_key))
                continue;

            int kerning =
                stbtt_GetCodepointKernAdvance(&font_info, static_cast<int>(codepoint_a), static_cast<int>(codepoint_b));
            kerning = static_cast<i32>(static_cast<float>(kerning) * scale);

            // NOTE: Depending on the font, a lot of codepoint pairs might not have kerning information.
            // Storing a massive table of mostly zeroed kerning values is wasteful and inefficient.
            if (kerning != 0)
                font->m_kerning_table.insert({ kerning_table_key, kerning });
        }
    }

    // TODO: How the heck do you release a stb_truetype font? It just magically disappear?
    return font;
}

ResultOr<const Font::Glyph&> Font::get_glyph(u32 codepoint) const
{
    auto glyph_it = m_glyphs.find(codepoint);
    // NOTE: Maybe try to rasterize the missing glyph instead of returning an error result directly?
    if (glyph_it == m_glyphs.end())
        return Result(Result::FontGlyphMissing);
    return glyph_it->second;
}

i32 Font::get_kerning(u32 codepoint_a, u32 codepoint_b) const
{
    const u64 kerning_table_key = (static_cast<u64>(codepoint_a) << 32) | static_cast<u64>(codepoint_b);
    auto kerning_it = m_kerning_table.find(kerning_table_key);

    if (kerning_it == m_kerning_table.end())
        return 0;
    return kerning_it->second;
}

ResultOr<IntRect> get_text_rect(const String& text, usize offset_x, usize offset_y, const OwnPtr<Font>& font)
{
    if (text.empty())
        return IntRect { static_cast<i32>(offset_x), static_cast<i32>(offset_y), 0, 0 };

    i32 min_x = static_cast<i32>(offset_x);
    i32 min_y = static_cast<i32>(offset_y);
    i32 max_x = INT32_MIN;
    i32 max_y = INT32_MIN;

    i32 cursor_x = static_cast<i32>(offset_x);
    i32 cursor_y = static_cast<i32>(offset_y);

    for (usize index = 0; index < text.length(); ++index)
    {
        const char character = text[index];
        if (character == ' ')
        {
            cursor_x += font->space_advance();
            continue;
        }

        const u32 codepoint = static_cast<i32>(character);

        TRY_ASSIGN(const auto& glyph, font->get_glyph(codepoint));
        const i32 kerning = (index == text.length() - 1) ? 0 : font->get_kerning(codepoint, text[index + 1]);

        min_x = std::min(min_x, cursor_x + glyph.offset_x);
        min_y = std::min(min_y, cursor_y + glyph.offset_y);
        max_x = std::max(max_x, cursor_x + glyph.offset_x + static_cast<i32>(glyph.bitmap->width()));
        max_y = std::max(max_y, cursor_y + glyph.offset_y + static_cast<i32>(glyph.bitmap->height()));

        cursor_x += glyph.advance + kerning;
    }

    return IntRect { min_x, min_y, static_cast<u32>(max_x - min_x), static_cast<u32>(max_y - min_y) };
}

ResultOr<void> draw_text_to_bitmap(
    const String& text, usize offset_x, usize offset_y, Color, OwnPtr<Bitmap>& bitmap, const OwnPtr<Font>& font
)
{
    usize x = offset_x;
    usize y = offset_y + font->descent();

    for (usize index = 0; index < text.length(); ++index)
    {
        const char character = text[index];

        if (character == ' ')
        {
            x += font->space_advance();
            continue;
        }

        // NOTE: ASCII codepoints are equal to the ASCII character value.
        const u32 codepoint = static_cast<i32>(character);
        TRY_ASSIGN(const auto& glyph, font->get_glyph(codepoint));

        TRY(bitmap->fill_cropped_bitmap(x + glyph.offset_x, y + glyph.offset_y, glyph.bitmap));

        const i32 kerning = (index == text.length() - 1) ? 0 : font->get_kerning(codepoint, text[index + 1]);
        x += glyph.advance + kerning;
    }

    return {};
}

} // namespace Octopus
