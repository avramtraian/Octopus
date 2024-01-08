/*
 * Copyright (c) 2023 Traian Avram. All rights reserved.
 * SPDX-License-Identifier: MIT.
 */

#pragma once

#include "Core.h"
#include "Result.h"
#include "Table.h"

namespace Octopus
{

class CommandSyntax
{
public:
    enum class Type : u8
    {
        String,
        Integer,
    };

    struct Variable
    {
        Type type;
        String name;
    };

public:
    CommandSyntax(std::initializer_list<Variable> variables)
        : m_variables(variables)
    {
    }

    NODISCARD const Vector<Variable>& variables() const { return m_variables; }

private:
    Vector<Variable> m_variables;
};

struct PrimaryCommandContext
{
    Vector<String> arguments_string;
    Vector<i64> arguments_integer;
};

class ProgramContext
{
public:
    ProgramContext(OwnPtr<Table>&& table, bool allow_subcommands)
        : m_table(std::move(table))
        , m_allow_subcommands(allow_subcommands)
    {
    }

public:
    NODISCARD ALWAYS_INLINE bool keeps_running() const { return m_keeps_running; }
    ALWAYS_INLINE void exit_program() { m_keeps_running = false; }

    ALWAYS_INLINE void set_primary_command_name(String command_name) { m_primary_command_name = command_name; }
    ALWAYS_INLINE const String& get_primary_command_name() const { return m_primary_command_name; }

    NODISCARD ALWAYS_INLINE OwnPtr<Table>& table() { return m_table; }
    NODISCARD ALWAYS_INLINE const OwnPtr<Table>& table() const { return m_table; }

    NODISCARD ALWAYS_INLINE bool allow_subcommands() const { return m_allow_subcommands; }

private:
    bool m_keeps_running = true;
    String m_primary_command_name;
    OwnPtr<Table> m_table;
    bool m_allow_subcommands;
};

using PrimaryCommandCallback = ResultOr<OwnPtr<ProgramContext>> (*)(const PrimaryCommandContext& context);
#define PRIMARY_COMMAND_CALLBACK(function_name) \
    static ResultOr<OwnPtr<ProgramContext>> function_name(const PrimaryCommandContext& context)

class PrimaryCommandRegister
{
public:
    ALWAYS_INLINE PrimaryCommandRegister(
        StringView name,
        HashSet<String> operation_codes,
        CommandSyntax syntax,
        HashSet<String> subcommands,
        PrimaryCommandCallback callback,
        StringView help_info
    )
        : m_operation_codes(std::move(operation_codes))
        , m_syntax(std::move(syntax))
        , m_subcommands(std::move(subcommands))
        , m_callback(callback)
        , m_help_info(help_info)
    {
        VERIFY(!s_registers.contains(String(name)));
        s_registers.insert({ String(name), std::move(*this) });
    }

    NODISCARD static const HashMap<String, PrimaryCommandRegister>& registers() { return s_registers; }

    NODISCARD const HashSet<String>& operation_codes() const { return m_operation_codes; }
    NODISCARD const CommandSyntax& syntax() const { return m_syntax; }
    NODISCARD const HashSet<String>& subcommands() const { return m_subcommands; }
    NODISCARD PrimaryCommandCallback callback() const { return m_callback; }
    NODISCARD StringView help_info() const { return m_help_info; }

private:
    static HashMap<String, PrimaryCommandRegister> s_registers;

private:
    HashSet<String> m_operation_codes;
    CommandSyntax m_syntax;
    HashSet<String> m_subcommands;
    PrimaryCommandCallback m_callback;
    StringView m_help_info;
};

struct SubcommandContext
{
    SubcommandContext(
        const OwnPtr<ProgramContext>& program_context, Vector<String> arguments_string, Vector<i64> arguments_integer
    )
        : program_context(program_context)
        , arguments_string(arguments_string)
        , arguments_integer(arguments_integer)
    {
    }

    const OwnPtr<ProgramContext>& program_context;
    Vector<String> arguments_string;
    Vector<i64> arguments_integer;
};

/// Returns whether or not to keep processing subcommands.
using SubcommandCallback = ResultOr<IterationDecision> (*)(const SubcommandContext& context);
#define SUBCOMMAND_CALLBACK(function_name) ResultOr<IterationDecision> function_name(const SubcommandContext& context)

class SubcommandRegister
{
public:
    ALWAYS_INLINE SubcommandRegister(
        StringView name,
        HashSet<String> operation_codes,
        CommandSyntax syntax,
        SubcommandCallback callback,
        StringView help_info
    )
        : m_operation_codes(std::move(operation_codes))
        , m_syntax(std::move(syntax))
        , m_callback(callback)
        , m_help_info(help_info)
    {
        VERIFY(!s_registers.contains(String(name)));
        s_registers.insert({ String(name), std::move(*this) });
    }

    NODISCARD static const HashMap<String, SubcommandRegister>& registers() { return s_registers; }

    NODISCARD const HashSet<String>& operation_codes() const { return m_operation_codes; }
    NODISCARD const CommandSyntax& syntax() const { return m_syntax; }
    NODISCARD SubcommandCallback callback() const { return m_callback; }
    NODISCARD StringView help_info() const { return m_help_info; }

private:
    static HashMap<String, SubcommandRegister> s_registers;

private:
    HashSet<String> m_operation_codes;
    CommandSyntax m_syntax;
    SubcommandCallback m_callback;
    StringView m_help_info;
};

} // namespace Octopus
