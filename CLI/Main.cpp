/*
 * Copyright (c) 2023 Traian Avram. All rights reserved.
 * SPDX-License-Identifier: MIT.
 */

#include "Command.h"
#include "Core.h"
#include "Print.h"
#include "Table.h"

namespace Octopus
{

// NOTE: Currently, this function is limited to parsing strings of at most 12 characters, in
// order to avoid running into integer overflows.
static bool parse_integer_from_string(StringView string, i64& out_integer)
{
    out_integer = 0;

    if (string.empty() || string.size() > 12)
        return false;

    bool is_negative = false;
    if (string.starts_with('-'))
    {
        is_negative = true;
        string = string.substr(1);
    }

    i64 number = 0;
    for (const char character : string)
    {
        if (character < '0' || character > '9')
            return false;

        number *= 10;
        if (is_negative)
            number -= character - '0';
        else
            number += character - '0';
    }

    out_integer = number;
    return true;
}

static void print_command_syntax(const CommandSyntax& command_syntax)
{
    if (command_syntax.variables().empty())
    {
        Print::string("(void)");
        return;
    }

    for (usize index = 0; index < command_syntax.variables().size(); ++index)
    {
        const auto& variable = command_syntax.variables()[index];
        StringView variable_type;
        switch (variable.type)
        {
            case CommandSyntax::Type::String: variable_type = "String"; break;
            case CommandSyntax::Type::Integer: variable_type = "Integer"; break;
        }

        StringView separator;
        if (index != command_syntax.variables().size() - 1)
            separator = ", ";

        Print::string("[{} {}]{}", variable_type, variable.name, separator);
    }
}

static bool get_command_arguments(
    const CommandSyntax& command_syntax,
    const Vector<String>& arguments,
    Vector<String>& out_arguments_string,
    Vector<i64>& out_arguments_integer
)
{
    out_arguments_string.clear();
    out_arguments_integer.clear();

    // The syntax can't match because it doesn't have the same number of variables as the command line.
    if (command_syntax.variables().size() != arguments.size())
        return false;

    for (usize index = 0; index < command_syntax.variables().size(); ++index)
    {
        const auto& variable = command_syntax.variables()[index];
        const auto& command_line_argument = arguments[index];

        switch (variable.type)
        {
            case CommandSyntax::Type::String:
            {
                out_arguments_string.emplace_back(command_line_argument);
                break;
            }
            case CommandSyntax::Type::Integer:
            {
                i64 integer;
                if (!parse_integer_from_string(command_line_argument, integer))
                    return false;

                out_arguments_integer.emplace_back(integer);
                break;
            }
        }
    }

    return true;
}

static void print_help_command()
{
    Print::line("The following commands are available");
    Print::push_indentation();

    for (const auto& [name, command] : PrimaryCommandRegister::registers())
    {
        Print::line("* {}: {}", name, command.help_info());
        Print::push_indentation(2);

        Print::string_with_indent("Syntax:   ");
        print_command_syntax(command.syntax());
        Print::new_line();

        Print::string_with_indent("Op codes: ");
        const auto op_codes = command.operation_codes();
        for (auto it = op_codes.begin(); it != op_codes.end();)
        {
            const StringView op_code = *it;
            const StringView separator = (++it) == op_codes.end() ? StringView() : ", ";
            Print::string("'{}'{}", op_code, separator);
        }

        Print::pop_indentation(2);
        Print::new_line();
    }

    Print::push_indentation();
    Print::new_line();
}

struct CommandLineArguments
{
    Vector<String> arguments;
};

struct CommandMatch
{
    String name;
    Vector<String> arguments_string;
    Vector<i64> arguments_integer;
};

static ResultOr<bool> create_program_context(const CommandLineArguments& arguments, OwnPtr<ProgramContext>& out_context)
{
    if (arguments.arguments.empty())
    {
        Print::line("Invalid usage!");
        Print::line("Command syntax is: -'operation code' ...arguments...");
        return false;
    }

    String operation_code = arguments.arguments[0];
    if (operation_code.empty())
    {
        Print::line("Invalid usage!");
        Print::line("Operation code can't be empty.");
        return false;
    }

    if (operation_code[0] != '-')
    {
        Print::line("Invalid usage!");
        Print::line("The operation code must begin with '-'.");
        return false;
    }

    operation_code = operation_code.substr(1);
    if (operation_code.empty())
    {
        Print::line("Invalid usage!");
        Print::line("Operation code can't be empty.");
        return false;
    }

    if (operation_code == "help")
    {
        print_help_command();
        return false;
    }

    Vector<String> arguments_without_op_code;
    arguments_without_op_code.reserve(arguments.arguments.size() - 1);
    for (usize index = 1; index < arguments.arguments.size(); ++index)
        arguments_without_op_code.emplace_back(arguments.arguments[index]);

    // All the commands that match the operation code, but might not match the syntax.
    Vector<String> primary_command_candidates;

    // All the commands that match both the operation code and the syntax.
    // If this vector doesn't have exactly one element, invalid usage is invoked.
    Vector<CommandMatch> primary_command_matches;

    for (const auto& [name, command] : PrimaryCommandRegister::registers())
    {
        if (!command.operation_codes().contains(operation_code))
            continue;
        primary_command_candidates.push_back(name);

        const CommandSyntax& syntax = command.syntax();
        Vector<String> arguments_string;
        Vector<i64> arguments_integer;

        if (get_command_arguments(syntax, arguments_without_op_code, arguments_string, arguments_integer))
        {
            CommandMatch match;
            match.name = name;
            match.arguments_string = std::move(arguments_string);
            match.arguments_integer = std::move(arguments_integer);
            primary_command_matches.emplace_back(std::move(match));
        }
    }

    if (primary_command_matches.empty())
    {
        Print::line("No command with operation code '{}' matches the syntax.", operation_code);

        if (!primary_command_candidates.empty())
        {
            Print::line("The following commands match the operation code:");
            Print::push_indentation();

            for (const auto& name : primary_command_candidates)
            {
                const PrimaryCommandRegister& command = PrimaryCommandRegister::registers().at(name);
                Print::line("The command '{}' requires the syntax:", name);

                Print::push_indentation();
                Print::string_with_indent({});
                print_command_syntax(command.syntax());
                Print::new_line();
                Print::pop_indentation();
            }

            Print::pop_indentation();
        }

        return false;
    }
    if (primary_command_matches.size() > 1)
    {
        Print::line("More than one command matches both the operation code and the syntax.");
        return false;
    }

    const auto& command_name = primary_command_matches[0].name;
    const auto& command = PrimaryCommandRegister::registers().at(command_name);

    PrimaryCommandContext command_invoke_context;
    command_invoke_context.arguments_string = std::move(primary_command_matches[0].arguments_string);
    command_invoke_context.arguments_integer = std::move(primary_command_matches[0].arguments_integer);

    TRY_ASSIGN(out_context, command.callback()(command_invoke_context));
    if (!out_context)
    {
        Print::line("Failed to create the program context!");
        return false;
    }

    out_context->set_primary_command_name(command_name);
    return true;
}

static void print_help_subcommand(const PrimaryCommandRegister& primary_command)
{
    Print::line("The following subcommands are available:");
    Print::push_indentation();

    for (const auto& subcommand_name : primary_command.subcommands())
    {
        const auto& subcommand = SubcommandRegister::registers().at(subcommand_name);
        Print::line("* {}: {}", subcommand_name, subcommand.help_info());
        Print::push_indentation(2);

        Print::string_with_indent("Syntax:   ");
        print_command_syntax(subcommand.syntax());
        Print::new_line();

        Print::string_with_indent("Op codes: ");
        for (auto it = subcommand.operation_codes().begin(); it != subcommand.operation_codes().end();)
        {
            const StringView op_code = *it;
            const StringView separator = (++it) == subcommand.operation_codes().end() ? StringView() : ", ";
            Print::string("'{}'{}", op_code, separator);
        }

        Print::pop_indentation(2);
        Print::new_line();
    }

    Print::pop_indentation();
    Print::new_line();
}

static ResultOr<bool> get_subcommand(CommandMatch& out_command_match, const OwnPtr<ProgramContext>& program_context)
{
    String operation_code;
    Vector<String> arguments;

    {
        std::string command_line_string;
        std::getline(std::cin, command_line_string);
        std::stringstream command_line = std::stringstream(command_line_string);

        command_line >> operation_code;
        if (operation_code.empty())
            return false;

        while (command_line.good())
        {
            String argument;
            command_line >> argument;
            arguments.emplace_back(std::move(argument));
        }
    }

    const PrimaryCommandRegister& primary_command =
        PrimaryCommandRegister::registers().at(program_context->get_primary_command_name());

    if (operation_code == "help")
    {
        print_help_subcommand(primary_command);
        return false;
    }

    // All the subcommands that match the operation code, but might not match the syntax.
    Vector<String> subcommand_candidates;

    // All the subcommands that match both the operation code and the syntax.
    // If this vector doesn't have exactly one element, invalid usage is invoked.
    Vector<CommandMatch> subcommand_matches;

    for (const auto& subcommand_name : primary_command.subcommands())
    {
        const auto& subcommand = SubcommandRegister::registers().at(subcommand_name);
        if (!subcommand.operation_codes().contains(operation_code))
            continue;
        subcommand_candidates.push_back(subcommand_name);

        const auto& syntax = subcommand.syntax();
        Vector<String> arguments_string;
        Vector<i64> arguments_integer;

        if (get_command_arguments(syntax, arguments, arguments_string, arguments_integer))
        {
            CommandMatch match;
            match.name = subcommand_name;
            match.arguments_string = std::move(arguments_string);
            match.arguments_integer = std::move(arguments_integer);
            subcommand_matches.emplace_back(std::move(match));
        }
    }

    if (subcommand_matches.empty())
    {
        Print::line("No subcommand with operation code '{}' matches the syntax.", operation_code);
        Print::push_indentation();

        for (const auto& name : subcommand_candidates)
        {
            const SubcommandRegister& subcommand = SubcommandRegister::registers().at(name);
            Print::line("The subcommand '{}' requires the syntax:", name);

            Print::push_indentation();
            Print::string_with_indent({});
            print_command_syntax(subcommand.syntax());
            Print::pop_indentation();
            Print::new_line();
        }

        Print::pop_indentation();
        return false;
    }
    if (subcommand_matches.size() > 1)
    {
        Print::line("More than one subcommand matches both the operation code and the syntax.");
        return false;
    }

    out_command_match = subcommand_matches[0];
    return true;
}

static bool check_command_structure()
{
    for (const auto& [name, command] : PrimaryCommandRegister::registers())
    {
        for (const auto& subcommand : command.subcommands())
        {
            if (!SubcommandRegister::registers().contains(subcommand))
                return false;
        }
    }

    return true;
}

static ResultOr<void> guarded_main(const CommandLineArguments& arguments)
{
    if (!check_command_structure())
    {
        Print::line("The command structure is not valid!");
        return {};
    }

    OwnPtr<ProgramContext> program_context;
    TRY_ASSIGN(const bool success, create_program_context(arguments, program_context));
    if (!success)
        return {};

    if (!program_context->allow_subcommands())
        return {};

    while (program_context->keeps_running())
    {
        CommandMatch subcommand_match;
        Print::push_indentation();
        TRY_ASSIGN(const bool subcommand_is_valid, get_subcommand(subcommand_match, program_context));
        Print::pop_indentation();
        if (!subcommand_is_valid)
            continue;

        const auto& subcommand = SubcommandRegister::registers().at(subcommand_match.name);

        const SubcommandContext subcommand_context(
            program_context, subcommand_match.arguments_string, subcommand_match.arguments_integer
        );

        Print::push_indentation();
        auto result_or_iteration_decision = subcommand.callback()(subcommand_context);

        if (result_or_iteration_decision.is_result())
        {
            auto result_code = result_or_iteration_decision.release_result().get_code();
            Print::line(
                "Subcommand '{}' failed with result code: {}", subcommand_match.name, static_cast<u32>(result_code)
            );

            Print::pop_indentation();
            continue;
        }

        Print::pop_indentation();
        Print::new_line();

        if (result_or_iteration_decision.release_value() == IterationDecision::Break)
            break;
    }

    return {};
}

} // namespace Octopus

int main(int argument_count, char** arguments)
{
    Octopus::CommandLineArguments cmd_arguments;
    for (int index = 1; index < argument_count; ++index)
        cmd_arguments.arguments.push_back(arguments[index]);

    auto execution_result = Octopus::guarded_main(cmd_arguments);
    if (execution_result.is_result())
    {
        const int result_code = static_cast<int>(execution_result.release_result().get_code());
        Octopus::Print::line("Primary command failed with result code: {}", result_code);
        return result_code;
    }

    return 0;
}
