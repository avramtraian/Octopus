/*
 * Copyright (c) 2023 Traian Avram. All rights reserved.
 * SPDX-License-Identifier: MIT.
 */

#pragma once

#include "Core.h"
#include "Result.h"

namespace Octopus
{

struct Color
{
    u8 red = 0;
    u8 green = 0;
    u8 blue = 0;
    u8 alpha = 0;

    NODISCARD ALWAYS_INLINE static Color white() { return Color { 255, 255, 255, 255 }; }
    NODISCARD ALWAYS_INLINE static Color black() { return Color { 0, 0, 0, 255 }; }
};

struct IntRect
{
    i32 offset_x;
    i32 offset_y;
    u32 width;
    u32 height;
};

class Bitmap
{
public:
    OCT_NONCOPYABLE(Bitmap)
    OCT_NONMOVABLE(Bitmap)
    ~Bitmap() = default;

public:
    static ResultOr<OwnPtr<Bitmap>> create_from_file(const String& filepath);
    static ResultOr<OwnPtr<Bitmap>> create_from_memory(usize width, usize height, const Color* pixels);
    static ResultOr<OwnPtr<Bitmap>> create_from_color(usize width, usize height, Color solid_color);

    ResultOr<void> save_to_file(const String& filepath) const;

    /// Rotates the bitmap in the clockwise direction with rotation_count * 90 degrees.
    static ResultOr<OwnPtr<Bitmap>> rotate(const OwnPtr<Bitmap>& bitmap, u32 rotation_count);

    static ResultOr<OwnPtr<Bitmap>> flip_horizontal(const OwnPtr<Bitmap>& bitmap);
    static ResultOr<OwnPtr<Bitmap>> flip_vertical(const OwnPtr<Bitmap>& bitmap);

public:
    NODISCARD ALWAYS_INLINE usize width() const { return m_width; }
    NODISCARD ALWAYS_INLINE usize height() const { return m_height; }
    NODISCARD ALWAYS_INLINE const Color* pixels() const { return m_pixels.data(); }

    Color* pixel_at(usize offset_x, usize offset_y);
    const Color* pixel_at(usize offset_x, usize offset_y) const;

    ResultOr<void> fill_rect(const IntRect& rect, Color color);
    ResultOr<void> fill_opaque_rect(const IntRect& rect, Color color);

    ResultOr<void> fill_cropped_rect(const IntRect& rect, Color color);
    ResultOr<void> fill_cropped_opaque_rect(const IntRect& rect, Color color);

    ResultOr<void> fill_bitmap(i32 offset_x, i32 offset_y, const OwnPtr<Bitmap>& source_bitmap);
    ResultOr<void> fill_opaque_bitmap(i32 offset_x, i32 offset_y, const OwnPtr<Bitmap>& source_bitmap);

    ResultOr<void> fill_cropped_bitmap(i32 offset_x, i32 offset_y, const OwnPtr<Bitmap>& source_bitmap);
    ResultOr<void> fill_cropped_opaque_bitmap(i32 offset_x, i32 offset_y, const OwnPtr<Bitmap>& source_bitmap);

private:
    Bitmap() = default;

private:
    usize m_width;
    usize m_height;
    Vector<Color> m_pixels;
};

} // namespace Octopus
