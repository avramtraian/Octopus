/*
 * Copyright (c) 2023 Traian Avram. All rights reserved.
 * SPDX-License-Identifier: MIT.
 */

#include "Print.h"
#include <iostream>

namespace Octopus
{

u32 Print::s_indentation_level = 0;
String Print::s_indentation_buffer;
static constexpr u32 indentation_character_count = 2;

void Print::string(StringView string)
{
    std::cout << string;
}

void Print::string_with_indent(StringView string)
{
    std::cout << s_indentation_buffer << string;
}

void Print::line(StringView line)
{
    std::cout << s_indentation_buffer;
    std::cout << line;
    std::cout << '\n';
}

void Print::line_with_indent(u32 indentation_level, StringView line)
{
    s_indentation_level += indentation_level;
    const u32 additional_indentation = indentation_level * indentation_character_count;
    for (u32 indent_level = 0; indent_level < additional_indentation; ++indent_level)
        s_indentation_buffer.push_back(' ');

    Print::line(line);

    for (u32 indent_level = 0; indent_level < additional_indentation; ++indent_level)
        s_indentation_buffer.pop_back();
    s_indentation_level -= indentation_level;
}

void Print::new_line()
{
    std::cout << '\n';
}

void Print::push_indentation(u32 level /*= 1*/)
{
    s_indentation_level += level;
    const u32 additional_indentation = level * indentation_character_count;
    for (u32 indent_level = 0; indent_level < additional_indentation; ++indent_level)
        s_indentation_buffer.push_back(' ');
}

void Print::pop_indentation(u32 level /*= 1*/)
{
    s_indentation_level += level;
    const u32 additional_indentation = level * indentation_character_count;
    for (u32 indent_level = 0; indent_level < additional_indentation; ++indent_level)
        s_indentation_buffer.pop_back();
}

} // namespace Octopus
