/*
 * Copyright (c) 2023 Traian Avram. All rights reserved.
 * SPDX-License-Identifier: MIT.
 */

#pragma once

#include "Core.h"
#include <format>

namespace Octopus
{

class Print
{
public:
    struct LocalIndent
    {
        ALWAYS_INLINE LocalIndent() { Print::push_indentation(); }
        ALWAYS_INLINE ~LocalIndent() { Print::pop_indentation(); }
    };

public:
    static void string(StringView string);
    static void string_with_indent(StringView string);
    static void line(StringView line);
    static void line_with_indent(u32 indentation_level, StringView line);

    static void new_line();

    static void push_indentation(u32 level = 1);
    static void pop_indentation(u32 level = 1);
    ALWAYS_INLINE static u32 get_indentation_level() { return s_indentation_level; }

    template<typename... Args>
    ALWAYS_INLINE static void string(StringView string_format, Args&&... arguments)
    {
        const String formatted = std::vformat(string_format, std::make_format_args(std::forward<Args>(arguments)...));
        Print::string(formatted);
    }

    template<typename... Args>
    ALWAYS_INLINE static void string_with_indent(StringView string_format, Args&&... arguments)
    {
        const String formatted = std::vformat(string_format, std::make_format_args(std::forward<Args>(arguments)...));
        Print::string_with_indent(formatted);
    }

    template<typename... Args>
    ALWAYS_INLINE static void line(StringView line_format, Args&&... arguments)
    {
        const String formatted = std::vformat(line_format, std::make_format_args(std::forward<Args>(arguments)...));
        Print::line(formatted);
    }

    template<typename... Args>
    ALWAYS_INLINE static void line_with_indent(u32 indent_level, StringView line_format, Args&&... arguments)
    {
        const String formatted = std::vformat(line_format, std::make_format_args(std::forward<Args>(arguments)...));
        Print::line_with_indent(indent_level, formatted);
    }

private:
    static u32 s_indentation_level;
    static String s_indentation_buffer;
};

} // namespace Octopus
