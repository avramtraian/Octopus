/*
 * Copyright (c) 2023 Traian Avram. All rights reserved.
 * SPDX-License-Identifier: MIT.
 */

#pragma once

#include "Bitmap.h"
#include "Core.h"
#include "Result.h"

namespace Octopus
{

///
/// A simple font interface that uses the nothings/stb_truetype library to read .ttf font files
/// and rasterize the font glyphs to bitmaps.
///
class Font
{
public:
    OCT_NONCOPYABLE(Font)
    OCT_NONMOVABLE(Font)
    ~Font() = default;

    struct Glyph
    {
        OwnPtr<Bitmap> bitmap;
        i32 advance;
        i32 offset_x;
        i32 offset_y;
    };

public:
    static ResultOr<OwnPtr<Font>> create_from_ttf(const String& filepath, float font_height, Color color);

    /// Currently, if the codepoint doesn't have an associated glyph this function will fail.
    /// Under no circumstances it will rasterize the missing codepoint and insert a new glyph into the table.
    ResultOr<const Glyph&> get_glyph(u32 codepoint) const;

    /// If no kerning information is available, it will return zero.
    NODISCARD i32 get_kerning(u32 codepoint_a, u32 codepoint_b) const;

    NODISCARD ALWAYS_INLINE i32 space_advance() const { return m_space_advance; }
    NODISCARD ALWAYS_INLINE i32 ascent() const { return m_ascent; }
    NODISCARD ALWAYS_INLINE i32 descent() const { return m_descent; }
    NODISCARD ALWAYS_INLINE i32 line_gap() const { return m_line_gap; }

private:
    Font() = default;

private:
    HashMap<u32, Glyph> m_glyphs;
    HashMap<u64, i32> m_kerning_table;

    i32 m_space_advance;
    i32 m_ascent;
    i32 m_descent;
    i32 m_line_gap;
};

ResultOr<IntRect> get_text_rect(const String& text, usize offset_x, usize offset_y, const OwnPtr<Font>& font);

ResultOr<void> draw_text_to_bitmap(
    const String& text, usize offset_x, usize offset_y, Color color, OwnPtr<Bitmap>& bitmap, const OwnPtr<Font>& font
);

} // namespace Octopus
