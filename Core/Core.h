/*
 * Copyright (c) 2023 Traian Avram. All rights reserved.
 * SPDX-License-Identifier: MIT.
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <new>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define ALWAYS_INLINE __forceinline
#define NODISCARD     [[nodiscard]]
#define UNSAFE

#define INTERNAL_STRINGIFY(x)      #x
#define INTERNAL_CONCATENATE(x, y) x##y

#define STRINGIFY(x)      INTERNAL_STRINGIFY(x)
#define CONCATENATE(x, y) INTERNAL_CONCATENATE(x, y)
#define BIT(x)            (1 << (x))

#define VERIFY(expression) \
    if (!(expression))     \
    {                      \
        __debugbreak();    \
    }

#define OCT_NONCOPYABLE(type_name)        \
    type_name(const type_name&) = delete; \
    type_name& operator=(const type_name&) = delete;

#define OCT_NONMOVABLE(type_name)             \
    type_name(type_name&&) noexcept = delete; \
    type_name& operator=(type_name&&) noexcept = delete;

namespace Octopus
{

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using usize = size_t;
using uintptr = usize;

using String = std::string;
using StringView = std::string_view;
template<typename KeyType, typename ValueType>
using HashMap = std::unordered_map<KeyType, ValueType>;
template<typename KeyType, typename ValueType>
using Map = std::map<KeyType, ValueType>;
template<typename T>
using OwnPtr = std::unique_ptr<T>;
template<typename T>
using HashSet = std::unordered_set<T>;
template<typename T>
using Optional = std::optional<T>;
template<typename T>
using RefPtr = std::shared_ptr<T>;
template<typename T>
using Span = std::span<T>;
template<typename T>
using Vector = std::vector<T>;

enum class IterationDecision : u8
{
    Break,
    Continue,
};

enum class MatchDecision : u8
{
    No,
    Yes,
};

} // namespace Octopus
