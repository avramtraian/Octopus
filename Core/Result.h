/*
 * Copyright (c) 2023 Traian Avram. All rights reserved.
 * SPDX-License-Identifier: MIT.
 */

#pragma once

#include "Core.h"

namespace Octopus
{

class Result
{
public:
    enum Code : u8
    {
        /// Failure codes.
        UnknownFailure,
        IdInvalid,
        IdGenerationFailed,
        IdAlreadyExists,
        IdNotFound,
        IdExpired,
        IdNotScannable,
        EntryAlreadyExists,
        IntegerOverflow,
        InvalidParameter,
        InvalidEntryField,
        InvalidString,
        InvalidFilepath,
        FontGlyphMissing,
        ScanDateTooLong,

        /// Error codes.
        UnknownError,
        OutOfMemory,
        FileError,
        CorruptedTable,
        CorruptedTableEntry,
        InvalidYAML,
        BufferOverflow,
    };

public:
    ALWAYS_INLINE Result(Code code)
        : m_code(code)
    {
    }

    NODISCARD ALWAYS_INLINE Code get_code() const { return m_code; }

private:
    Code m_code;
};

template<typename T>
class NODISCARD ResultOr
{
public:
    ALWAYS_INLINE ResultOr(T value)
        : m_is_result(false)
        , m_value_storage(std::move(value))
    {
    }

    ALWAYS_INLINE ResultOr(Result result)
        : m_is_result(true)
        , m_result_storage(result)
    {
    }

    ALWAYS_INLINE ~ResultOr()
    {
        if (m_is_result)
            m_result_storage.~Result();
        else
            m_value_storage.~T();
    }

public:
    NODISCARD ALWAYS_INLINE bool is_result() const { return m_is_result; }
    NODISCARD ALWAYS_INLINE T release_value() { return std::move(m_value_storage); }
    NODISCARD ALWAYS_INLINE Result release_result() { return m_result_storage; }

private:
    bool m_is_result;

    union
    {
        T m_value_storage;
        Result m_result_storage;
    };
};

template<typename T>
class NODISCARD ResultOr<T&>
{
public:
    ALWAYS_INLINE ResultOr(T& value)
        : m_is_result(false)
        , m_value_storage(&value)
    {
    }

    ALWAYS_INLINE ResultOr(Result result)
        : m_is_result(true)
        , m_result_storage(result)
    {
    }

    ALWAYS_INLINE ~ResultOr()
    {
        if (m_is_result)
            m_result_storage.~Result();
    }

public:
    NODISCARD ALWAYS_INLINE bool is_result() const { return m_is_result; }
    NODISCARD ALWAYS_INLINE T& release_value() { return *m_value_storage; }
    NODISCARD ALWAYS_INLINE Result release_result() { return m_result_storage; }

private:
    bool m_is_result;

    union
    {
        T* m_value_storage;
        Result m_result_storage;
    };
};

template<>
class NODISCARD ResultOr<void>
{
public:
    ALWAYS_INLINE ResultOr()
        : m_is_result(false)
    {
    }

    ALWAYS_INLINE ResultOr(Result result)
        : m_is_result(true)
    {
        new (m_result_storage) Result(result);
    }

    ALWAYS_INLINE ~ResultOr()
    {
        if (m_is_result)
            reinterpret_cast<Result*>(m_result_storage)->~Result();
    }

public:
    NODISCARD ALWAYS_INLINE bool is_result() const { return m_is_result; }
    ALWAYS_INLINE void release_value() {}
    NODISCARD ALWAYS_INLINE Result release_result() { return *reinterpret_cast<Result*>(m_result_storage); }

private:
    bool m_is_result;
    alignas(Result) u8 m_result_storage[sizeof(Result)];
};

} // namespace Octopus

#define TRY(expression)                          \
    {                                            \
        auto _result_or = expression;            \
        if (_result_or.is_result()) [[unlikely]] \
            return _result_or.release_result();  \
    }

#define TRY_ASSIGN(variable_declaration, expression)                 \
    auto CONCATENATE(_result_or_, __LINE__) = expression;            \
    if (CONCATENATE(_result_or_, __LINE__).is_result()) [[unlikely]] \
        return CONCATENATE(_result_or_, __LINE__).release_result();  \
    variable_declaration = CONCATENATE(_result_or_, __LINE__).release_value();
