/*
 * Copyright (c) 2023 Traian Avram. All rights reserved.
 * SPDX-License-Identifier: MIT.
 */

#include "Command.h"
#include "MathUtils.h"
#include "Print.h"

namespace Octopus
{

PRIMARY_COMMAND_CALLBACK(primary_command_open_database)
{
    const String& database_filepath = context.arguments_string[0];
    TRY_ASSIGN(OwnPtr<Table> table, Table::create_from_file(database_filepath));

    OwnPtr<ProgramContext> program_context = OwnPtr<ProgramContext>(new ProgramContext(std::move(table), true));
    if (!program_context)
        return Result(Result::OutOfMemory);
    return program_context;
}

PRIMARY_COMMAND_CALLBACK(primary_command_create_database)
{
    (void)context;
    TRY_ASSIGN(OwnPtr<Table> table, Table::create_new());

    OwnPtr<ProgramContext> program_context = OwnPtr<ProgramContext>(new ProgramContext(std::move(table), true));
    if (!program_context)
        return Result(Result::OutOfMemory);
    return program_context;
}

SUBCOMMAND_CALLBACK(subcommand_save)
{
    const String& save_filepath = context.arguments_string[0];
    auto& table = context.program_context->table();
    TRY(table->save_to_file(save_filepath));
    Print::line("Database file successfully saved to '{}'.", save_filepath);
    return IterationDecision::Continue;
}

SUBCOMMAND_CALLBACK(subcommand_emit)
{
    auto& table = context.program_context->table();
    if (!table)
        return Result(Result::UnknownError);

    const i64 grade = context.arguments_integer[0];
    if (grade < 0)
        return Result(Result::InvalidParameter);
    if (context.arguments_string[2].size() != 1)
        return Result(Result::InvalidParameter);

    TableEntry entry;
    entry.first_name = context.arguments_string[1];
    entry.last_name = context.arguments_string[0];
    entry.grade_id = context.arguments_string[2].back();
    TRY_ASSIGN(entry.grade, safe_truncate_unsigned<u8>(static_cast<u64>(grade)));

    TRY_ASSIGN(const TicketID ticket_id, table->insert_entry(entry));
    TRY_ASSIGN(const TableEntry& inserted_entry, table->get_entry(ticket_id));

    Print::line("The following ticket was emitted:");
    Print::push_indentation();

    Print::line("ID:         {}", transform_to_base_36(ticket_id));
    Print::line("First name: {}", inserted_entry.first_name);
    Print::line("Last name:  {}", inserted_entry.last_name);
    Print::line("Grade:      {}{}", static_cast<u32>(inserted_entry.grade), inserted_entry.grade_id);

    Print::pop_indentation();
    return IterationDecision::Continue;
}

SUBCOMMAND_CALLBACK(subcommand_remove)
{
    auto& table = context.program_context->table();
    if (!table)
        return Result(Result::UnknownError);

    const String& ticket_id_as_string = context.arguments_string[0];
    TRY_ASSIGN(const TicketID ticket_id, transform_from_base_36<u64>(ticket_id_as_string));

    auto result_or_entry = table->get_entry(ticket_id);
    if (result_or_entry.is_result())
    {
        if (result_or_entry.release_result().get_code() == Result::Code::IdNotFound)
        {
            Print::line("Ticket ID '{}' is not valid.", ticket_id_as_string);
            return IterationDecision::Continue;
        }
        return result_or_entry.release_result();
    }

    const TableEntry removed_entry = result_or_entry.release_value();

    auto result_or_void = table->remove_ticket(ticket_id);
    if (result_or_void.is_result())
    {
        if (result_or_void.release_result().get_code() == Result::Code::IdNotFound)
        {
            Print::line("Ticket ID '{}' is not valid.", ticket_id_as_string);
            return IterationDecision::Continue;
        }
        return result_or_void.release_result();
    }

    Print::line("The following entry was removed:");
    Print::LocalIndent local_indent;
    Print::line("First name: {}", removed_entry.first_name);
    Print::line("Last name:  {}", removed_entry.last_name);
    Print::line("Grade:      {}{}", removed_entry.grade, removed_entry.grade_id);

    return IterationDecision::Continue;
}

SUBCOMMAND_CALLBACK(subcommand_scan)
{
    auto& table = context.program_context->table();
    if (!table)
        return Result(Result::UnknownError);

    const String& ticket_id_string = context.arguments_string[0];
    TRY_ASSIGN(const TicketID ticket_id, transform_from_base_36<u64>(ticket_id_string));
    auto result_or_entry = table->get_entry(ticket_id);

    if (result_or_entry.is_result())
    {
        auto result_id = result_or_entry.release_result().get_code();
        if (result_id == Result::IdNotFound)
        {
            Print::line("Ticket ID '{}' is not valid.", ticket_id_string);
            return IterationDecision::Continue;
        }

        return Result(result_id);
    }

    TableEntry& entry = result_or_entry.release_value();

    if (entry.metadata.scan_count == 0)
    {
        Print::line("Ticket ID '{}' was never scanned before.", ticket_id_string);
        Print::push_indentation();
        Print::line("Name:  {} {}", entry.last_name, entry.first_name);
        Print::line("Grade: {}{}", static_cast<u32>(entry.grade), entry.grade_id);
        Print::pop_indentation();
    }
    else
    {
        Print::line("Ticket ID '{}' was scanned {} times.", ticket_id_string, entry.metadata.scan_count);
        Print::push_indentation();
        Print::line("Name:           {} {}", entry.last_name, entry.first_name);
        Print::line("Grade:          {}{}", static_cast<u32>(entry.grade), entry.grade_id);
        Print::line("Last scan date: {}", entry.metadata.last_scan_date);
        Print::pop_indentation();
    }

    TRY(table->increment_ticket_scan_count(ticket_id));

    // TODO: The database should be saved to disk, in case the program will crash later.
    //       This will ensure that all tickets scanned until that moment were correctly registered.

    return IterationDecision::Continue;
}

SUBCOMMAND_CALLBACK(subcommand_change)
{
    const auto& table = context.program_context->table();
    if (!table)
        return Result(Result::UnknownError);

    const String& ticket_id_as_string = context.arguments_string[0];
    TRY_ASSIGN(const TicketID ticket_id, transform_from_base_36<u64>(ticket_id_as_string));
    TRY_ASSIGN(TableEntry & entry, table->get_entry(ticket_id));

    const String& new_first_name = context.arguments_string[2];
    const String& new_last_name = context.arguments_string[1];
    const i64 new_grade = context.arguments_integer[0];
    const String& new_grade_id = context.arguments_string[3];

    if (new_grade < 0)
        return Result(Result::InvalidParameter);
    if (new_grade_id.size() != 1)
        return Result(Result::InvalidParameter);

    TableEntry new_entry;
    new_entry.first_name = new_first_name;
    new_entry.last_name = new_last_name;
    TRY_ASSIGN(new_entry.grade, safe_truncate_unsigned<u8>(static_cast<u64>(new_grade)));
    new_entry.grade_id = new_grade_id[0];
    TRY(Table::format_entry(new_entry));

    if (entry.first_name != new_entry.first_name)
        Print::line("First name: {} -> {}", entry.first_name, new_entry.first_name);

    if (entry.last_name != new_entry.last_name)
        Print::line("Last name:  {} -> {}", entry.last_name, new_entry.last_name);

    if (entry.grade != new_entry.grade)
        Print::line("Grade:      {} -> {}", static_cast<u32>(entry.grade), static_cast<u32>(entry.grade));

    if (entry.grade_id != new_entry.grade_id)
        Print::line("Grade ID:   {} -> {}", entry.grade_id, new_entry.grade_id);

    entry = new_entry;
    return IterationDecision::Continue;
}

SUBCOMMAND_CALLBACK(subcommand_print)
{
    const auto& table = context.program_context->table();
    if (!table)
        return Result(Result::UnknownError);

    HashMap<u8, HashMap<char, u32>> number_of_tickets_per_class;

    for (u8 grade = 9; grade <= 12; ++grade)
    {
        for (char grade_id = 'A'; grade_id <= 'F'; ++grade_id)
        {
            std::map<String, TicketID> tickets_in_class;

            TRY(table->iterate_over_entries(
                [&](TicketID ticket_id, const TableEntry& entry) -> ResultOr<IterationDecision>
                {
                    if (entry.grade == grade && entry.grade_id == grade_id)
                    {
                        const String full_name = entry.last_name + " " + entry.first_name;
                        tickets_in_class[full_name] = ticket_id;
                    }

                    return IterationDecision::Continue;
                }
            ));

            number_of_tickets_per_class[grade][grade_id] = tickets_in_class.size();

            if (tickets_in_class.empty())
                continue;

            Print::line("Class {}{} ({} tickets):", static_cast<u32>(grade), grade_id, tickets_in_class.size());
            Print::LocalIndent local_indent;

            for (const auto& [full_name, ticket_id] : tickets_in_class)
            {
                const auto ticket_id_string = transform_to_base_36(ticket_id);
                Print::line("{}: {}", ticket_id_string, full_name);
            }

            Print::new_line();
        }
    }

    TRY_ASSIGN(const usize total_ticket_count, context.program_context->table()->entry_count());
    Print::line("Total tickets count: {}", total_ticket_count);
    Print::line("----------------");

    for (u8 grade = 9; grade <= 12; ++grade)
    {
        for (char grade_id = 'A'; grade_id <= 'F'; ++grade_id)
        {
            const u32 ticket_count = number_of_tickets_per_class[grade][grade_id];
            if (ticket_count == 0)
                continue;

            const String padding = (grade < 10) ? " " : String();
            Print::line("{}{}:{} {}", static_cast<u32>(grade), grade_id, padding, ticket_count);
        }

        if (grade != 12)
            Print::line("----------------");
    }

    return IterationDecision::Continue;
}

// NOTE: clang-format doesn't handle initializer lists very well, so we disable it while
// defining the primary commands. It makes the code a lot easier to read.
// clang-format off
// NOLINTBEGIN

static PrimaryCommandRegister s_open_database_command(
    "open_database", { "db" },
    { { CommandSyntax::Type::String, "database_filepath" } },
    { "save", "emit", "remove", "change", "scan", "print" },
    primary_command_open_database,
    "Opens a database from a file."
);

static PrimaryCommandRegister s_create_database_command(
    "create_database", { "db" },
    {},
    { "save", "emit", "remove", "change", "scan", "print" },
    primary_command_create_database,
    "Creates a new empty memory-only database."
);

static SubcommandRegister s_save_subcommand(
    "save", { "save" },
    { { CommandSyntax::Type::String, "save_filepath" } },
    subcommand_save,
    "Saves the current database to a file."
);

static SubcommandRegister s_emit_subcommand(
    "emit", { "emit", "e" },
    {
        { CommandSyntax::Type::String, "last_name" },
        { CommandSyntax::Type::String, "first_name" },
        { CommandSyntax::Type::Integer, "grade" },
        { CommandSyntax::Type::String, "grade_id" }
    },
    subcommand_emit,
    "Emits a new ticket, by generating a new ticket ID."
);

static SubcommandRegister s_remove_subcommand(
    "remove", { "remove", "rem" },
    {
        { CommandSyntax::Type::String, "ticket_id" },
    },
    subcommand_remove,
    "Removes a ticket ID from the database."
);

static SubcommandRegister s_scan_subcommand(
    "scan", { "scan", "s" },
    {
        { CommandSyntax::Type::String, "ticket_id" }
    },
    subcommand_scan,
    "Scans a ticket ID."
);

static SubcommandRegister s_change_subcommand(
    "change", { "change" },
    {
        { CommandSyntax::Type::String, "ticket_id" },
        { CommandSyntax::Type::String, "last_name" },
        { CommandSyntax::Type::String, "first_name" },
        { CommandSyntax::Type::Integer, "grade" },
        { CommandSyntax::Type::String, "grade_id" }
    },
    subcommand_change,
    "Changes the details of the entry associated with the given ticket id."
);

static SubcommandRegister s_print_subcommand(
    "print", { "print" },
    {},
    subcommand_print,
    "Prints all tickets to the console."
);

// NOLINTEND
// clang-format on

} // namespace Octopus
