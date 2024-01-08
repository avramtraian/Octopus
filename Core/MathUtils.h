/*
 * Copyright (c) 2023 Traian Avram. All rights reserved.
 * SPDX-License-Identifier: MIT.
 */

#pragma once

#include "Core.h"
#include "Result.h"

namespace Octopus
{

NODISCARD ResultOr<u64> generate_random_unsigned(u64 closed_min, u64 closed_max);

template<typename T, typename Q>
requires ((std::is_integral_v<T> && std::is_unsigned_v<T> && std::is_integral_v<Q> && std::is_unsigned_v<Q>))
ResultOr<T> safe_truncate_unsigned(Q value)
{
    constexpr T max_value = static_cast<T>(-1);
    if (value > max_value)
        return Result(Result::IntegerOverflow);
    return static_cast<T>(value);
}

template<typename T>
requires ((std::is_integral_v<T> && std::is_unsigned_v<T>))
ResultOr<T> safe_unsigned_addition(T a, T b)
{
    constexpr T max_value = static_cast<T>(-1);
    if (max_value - a < b)
        return Result(Result::IntegerOverflow);
    return a + b;
}

template<typename T>
requires ((std::is_integral_v<T> && std::is_unsigned_v<T>))
ResultOr<void> safe_unsigned_increment(T& value)
{
    TRY_ASSIGN(T incremented, safe_unsigned_addition<T>(value, 1));
    value = incremented;
    return {};
}

template<typename T>
requires ((std::is_integral_v<T> && std::is_unsigned_v<T>))
ResultOr<T> safe_unsigned_multiplication(T a, T b)
{
    if (a == 0 || b == 0)
        return 0;

    constexpr T max_value = static_cast<T>(-1);
    const T quotient = max_value / a;

    if (quotient < b)
        return Result(Result::IntegerOverflow);

    return a * b;
}

template<typename T>
requires ((std::is_integral_v<T> && std::is_unsigned_v<T>))
String transform_to_base_36(T value)
{
    if (value == 0)
        return "0";

    String result;
    while (value != 0)
    {
        const u8 digit = value % 36;
        if (digit <= 9)
            result.push_back('0' + digit);
        else
            result.push_back('A' + (digit - 10));

        value /= 36;
    }

    std::reverse(result.begin(), result.end());
    return result;
}

template<typename T>
requires ((std::is_integral_v<T> && std::is_unsigned_v<T>))
ResultOr<T> transform_from_base_36(StringView value)
{
    T result = 0;

    for (char character : value)
    {
        character = static_cast<char>(std::toupper(character));
        if ((character < '0' || character > '9') && (character < 'A' || character > 'Z'))
            return Result(Result::InvalidParameter);

        TRY_ASSIGN(result, safe_unsigned_multiplication<T>(result, 36));
        if (character <= '9')
        {
            TRY_ASSIGN(result, safe_unsigned_addition<T>(result, character - '0'))
        }
        else
        {
            TRY_ASSIGN(result, safe_unsigned_addition<T>(result, character - 'A' + 10))
        }
    }

    return result;
}

} // namespace Octopus
