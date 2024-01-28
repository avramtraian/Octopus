/*
 * Copyright (c) 2023 Traian Avram. All rights reserved.
 * SPDX-License-Identifier: MIT.
 */

#include "Table.h"
#include "MathUtils.h"

#define YAML_CPP_STATIC_DEFINE
#include "yaml-cpp/yaml.h"

#include <format>

#define ALLOWED_GRADE_LOW  9
#define ALLOWED_GRADE_HIGH 12

#define ALLOWED_GRADE_ID_LOW  'A'
#define ALLOWED_GRADE_ID_HIGH 'F'

namespace Octopus
{

ResultOr<OwnPtr<Table>> Table::create_new()
{
    OwnPtr<Table> table = std::make_unique<Table>();
    if (!table)
        return Result(Result::OutOfMemory);

    table->m_ticket_id_generation = 1;
    table->m_entries.clear();
    return table;
}

template<typename T>
ALWAYS_INLINE ResultOr<T> get_yaml_node(YAML::Node& node, StringView field_name)
{
    auto field_node = node[field_name];
    if (!field_node || field_node.IsNull())
        return Result(Result::InvalidYAML);
    return field_node.as<T>();
}

template<>
ALWAYS_INLINE ResultOr<YAML::Node> get_yaml_node(YAML::Node& node, StringView field_name)
{
    auto field_node = node[field_name];
    if (!field_node)
        return Result(Result::InvalidYAML);
    return field_node;
}

ResultOr<OwnPtr<Table>> Table::create_from_file(const String& filepath)
{
    TRY_ASSIGN(OwnPtr<Table> table, Table::create_new());

    const std::ifstream input(filepath);
    if (!input.is_open())
        return Result(Result::InvalidFilepath);
    std::stringstream buffer;
    buffer << input.rdbuf();

    YAML::Node table_data = YAML::Load(buffer.str());
    if (!table_data)
        return Result(Result::InvalidYAML);

    TRY_ASSIGN(auto table_info, get_yaml_node<YAML::Node>(table_data, "info"));
    TRY_ASSIGN(auto table_entries, get_yaml_node<YAML::Node>(table_data, "entries"));

    if (!table_entries.IsSequence())
        return Result(Result::InvalidYAML);

    for (auto table_entry : table_entries)
    {
        TRY_ASSIGN(auto ticket_id_string, get_yaml_node<String>(table_entry, "ticket_id"));
        TRY_ASSIGN(auto first_name, get_yaml_node<String>(table_entry, "first_name"));
        TRY_ASSIGN(auto last_name, get_yaml_node<String>(table_entry, "last_name"));
        TRY_ASSIGN(auto grade, get_yaml_node<u32>(table_entry, "grade"));
        TRY_ASSIGN(auto grade_id, get_yaml_node<char>(table_entry, "grade_id"));

        TRY_ASSIGN(auto metadata, get_yaml_node<YAML::Node>(table_entry, "metadata"));
        TRY_ASSIGN(auto metadata_flags, get_yaml_node<u32>(metadata, "flags"));
        TRY_ASSIGN(auto metadata_scan_count, get_yaml_node<u32>(metadata, "scan_count"));
        TRY_ASSIGN(auto metadata_last_scan_date, get_yaml_node<String>(metadata, "last_scan_date"));

        TableEntry entry;
        entry.first_name = first_name;
        entry.last_name = last_name;
        entry.grade = static_cast<u8>(grade);
        entry.grade_id = grade_id;

        entry.metadata.flags = metadata_flags;
        entry.metadata.scan_count = metadata_scan_count;
        entry.metadata.last_scan_date = std::move(metadata_last_scan_date);

        TRY_ASSIGN(const TicketID ticket_id, transform_from_base_36<u64>(ticket_id_string));
        TRY(table->insert_entry_with_ticket_id(ticket_id, entry));
    }

    TRY_ASSIGN(auto ticket_count, get_yaml_node<u32>(table_info, "tickets"));
    if (ticket_count != table->m_entries.size())
        return Result(Result::CorruptedTable);

    return table;
}

ResultOr<void> Table::save_to_file(const String& filepath) const
{
    YAML::Emitter emitter;
    emitter << YAML::BeginMap;

    // Information about the table.
    emitter << YAML::Key << "info" << YAML::BeginMap;
    emitter << YAML::Key << "name" << YAML::Value << "CNGC-BB-2024";
    emitter << YAML::Key << "tickets" << YAML::Value << m_entries.size();
    emitter << YAML::EndMap;

    emitter << YAML::Key << "entries" << YAML::BeginSeq;

    for (const auto& [ticket_id, entry] : m_entries)
    {
        TRY(entry.check_corrupted(Result::CorruptedTable));

        emitter << YAML::BeginMap;
        emitter << YAML::Key << "ticket_id" << YAML::Value << transform_to_base_36(ticket_id);
        emitter << YAML::Key << "first_name" << YAML::Value << entry.first_name;
        emitter << YAML::Key << "last_name" << YAML::Value << entry.last_name;
        emitter << YAML::Key << "grade" << YAML::Value << static_cast<u32>(entry.grade);
        emitter << YAML::Key << "grade_id" << YAML::Value << entry.grade_id;

        emitter << YAML::Key << "metadata" << YAML::BeginMap;
        emitter << YAML::Key << "flags" << YAML::Value << static_cast<u32>(entry.metadata.flags);
        emitter << YAML::Key << "scan_count" << YAML::Value << static_cast<u32>(entry.metadata.scan_count);

        if (entry.metadata.last_scan_date[0])
            emitter << YAML::Key << "last_scan_date" << YAML::Value << entry.metadata.last_scan_date;
        else
            emitter << YAML::Key << "last_scan_date" << YAML::Value << "N/A";

        emitter << YAML::EndMap; // Metadata map.
        emitter << YAML::EndMap; // Ticket entry map.
    }

    emitter << YAML::EndSeq;
    emitter << YAML::EndMap;

    std::ofstream output(filepath);
    if (!output.is_open())
        return Result(Result::InvalidFilepath);
    output << emitter.c_str();

    return {};
}

static ResultOr<void> format_name_string(String& name)
{
    for (usize index = 0; index < name.size(); ++index)
        name[index] = static_cast<char>(std::tolower(name[index]));

    bool character_must_be_uppercase = true;
    char last_character = '-';

    String formatted_name = String();
    for (usize index = 0; index < name.size(); ++index)
    {
        auto character = name[index];
        if (character != '-' && character != ' ' && !std::isalpha(character))
            return Result(Result::InvalidString);

        if (std::isalpha(character))
        {
            if (character_must_be_uppercase)
            {
                character = static_cast<char>(std::toupper(character));
                character_must_be_uppercase = false;
            }
            else
            {
                character = static_cast<char>(std::tolower(character));
            }
        }
        else
        {
            character_must_be_uppercase = true;
            if (last_character == '-')
                continue;
        }

        last_character = character;
        formatted_name.push_back(character);
    }

    if ((last_character == '-' || last_character == ' ') && !formatted_name.empty())
        formatted_name.pop_back();

    name = formatted_name;
    return {};
}

ResultOr<void> Table::format_entry(TableEntry& entry)
{
    TRY(entry.check_corrupted());

    if (entry.grade < ALLOWED_GRADE_LOW || entry.grade > ALLOWED_GRADE_HIGH)
        return Result(Result::InvalidEntryField);

    entry.grade_id = static_cast<char>(std::toupper(entry.grade_id));
    if (entry.grade_id < ALLOWED_GRADE_ID_LOW || entry.grade_id > ALLOWED_GRADE_ID_HIGH)
        return Result(Result::InvalidEntryField);

    TRY(format_name_string(entry.first_name));
    TRY(format_name_string(entry.last_name));

    return {};
}

ResultOr<bool> Table::is_ticket_id_valid(TicketID ticket_id) const
{
    auto entry_it = m_entries.find(ticket_id);
    if (entry_it == m_entries.end())
        return false;
    return true;
}

ResultOr<bool> Table::has_generated_ticket_id_expired(GeneratedTicketID generated_ticket_id) const
{
    if (generated_ticket_id.id == invalid_ticket_id)
        return Result(Result::IdInvalid);

    if (generated_ticket_id.generation == m_ticket_id_generation)
        return false;

    return true;
}

ResultOr<Table::GeneratedTicketID> Table::generate_ticket_id()
{
    constexpr u64 ticket_id_low_range = 1;
    // NOTE: A ticket ID is represented by a 5-characters long code. A character is defined as
    // either a digit or a letter from the english alphabet, hence the 36 to the power of 5.
    constexpr u64 ticket_id_high_range = 36ull * 36ull * 36ull * 36ull * 36ull;

    TicketID ticket_id;
    bool ticket_was_generated = false;

    for (u32 try_counter = 0; !ticket_was_generated && (try_counter < 512); ++try_counter)
    {
        TRY_ASSIGN(ticket_id, generate_random_unsigned(ticket_id_low_range, ticket_id_high_range));
        if (m_entries.find(ticket_id) == m_entries.end())
            ticket_was_generated = true;
    }

    if (!ticket_was_generated)
        return Result(Result::IdGenerationFailed);

    TRY(safe_unsigned_increment(m_ticket_id_generation))
    return GeneratedTicketID(ticket_id, m_ticket_id_generation);
}

ResultOr<void> Table::insert_entry_with_ticket_id(TicketID ticket_id, TableEntry entry)
{
    TRY(entry.check_corrupted());

    if (m_entries.find(ticket_id) != m_entries.end())
        return Result(Result::IdAlreadyExists);

    TRY_ASSIGN(const bool entry_already_exists, similar_entry_already_exists(entry));
    if (entry_already_exists)
        return Result(Result::EntryAlreadyExists);

    TRY(format_entry(entry));
    TRY(safe_unsigned_increment(m_ticket_id_generation));
    m_entries.insert({ ticket_id, std::move(entry) });
    return {};
}

ResultOr<void> Table::insert_entry_with_ticket_id(GeneratedTicketID generated_ticket_id, TableEntry entry)
{
    TRY_ASSIGN(const bool has_ticket_id_expired, has_generated_ticket_id_expired(generated_ticket_id));
    if (has_ticket_id_expired)
        return Result(Result::IdExpired);

    TRY(insert_entry_with_ticket_id(generated_ticket_id.id, std::move(entry)));
    return {};
}

ResultOr<TicketID> Table::insert_entry(TableEntry entry)
{
    TRY_ASSIGN(const GeneratedTicketID generated_ticket_id, generate_ticket_id());
    TRY(insert_entry_with_ticket_id(generated_ticket_id, std::move(entry)));
    return generated_ticket_id.id;
}

ResultOr<void> Table::remove_ticket(TicketID ticket_id)
{
    auto entry_it = m_entries.find(ticket_id);
    if (entry_it == m_entries.end())
        return Result(Result::IdNotFound);

    TRY(entry_it->second.check_corrupted());
    m_entries.erase(entry_it);
    return {};
}

ResultOr<usize> Table::entry_count() const
{
    return m_entries.size();
}

ResultOr<TableEntry&> Table::get_entry(TicketID ticket_id)
{
    auto entry_it = m_entries.find(ticket_id);
    if (entry_it == m_entries.end())
        return Result(Result::IdNotFound);

    TRY(entry_it->second.check_corrupted());
    return entry_it->second;
}

ResultOr<const TableEntry&> Table::get_entry(TicketID ticket_id) const
{
    auto entry_it = m_entries.find(ticket_id);
    if (entry_it == m_entries.end())
        return Result(Result::IdNotFound);

    TRY(entry_it->second.check_corrupted());
    return entry_it->second;
}

ResultOr<Vector<TicketID>> Table::find_ticket_id_by_name(StringView first_name, StringView last_name) const
{
    Vector<TicketID> ticket_ids;

    TRY(iterate_over_entries(
        [&](TicketID ticket_id, const auto& entry) -> ResultOr<IterationDecision>
        {
            if (entry.first_name == first_name && entry.last_name == last_name)
                ticket_ids.push_back(ticket_id);
            return IterationDecision::Continue;
        }
    ));

    return ticket_ids;
}

ResultOr<bool> Table::similar_entry_already_exists(const TableEntry& entry) const
{
    TRY(entry.check_corrupted());

    for (const auto& [ticket_id, existing_entry] : m_entries)
    {
        TRY(existing_entry.check_corrupted(Result::CorruptedTable));
        if (existing_entry.grade == entry.grade && existing_entry.grade_id == entry.grade_id &&
            existing_entry.last_name == entry.last_name && existing_entry.first_name == entry.first_name)
            return true;
    }

    return false;
}

static ResultOr<void> get_current_date_and_time(TableEntryMetadata& metadata)
{
    std::time_t t = std::time(0);
    std::tm now;
    if (localtime_s(&now, &t) != 0)
        return Result(Result::UnknownFailure);

    metadata.last_scan_date = std::format(
        "{}/{}/{}-{}:{}:{}", now.tm_mday, now.tm_mon + 1, 1900 + now.tm_year, now.tm_hour, now.tm_min, now.tm_sec
    );
    return {};
}

ResultOr<void> Table::increment_ticket_scan_count(TicketID ticket_id)
{
    TRY_ASSIGN(auto& entry, get_entry(ticket_id));

    if (entry.metadata.flags & TableEntryFlag::NotScannable)
        return Result(Result::IdNotScannable);

    TRY(get_current_date_and_time(entry.metadata));
    TRY(safe_unsigned_increment(entry.metadata.scan_count));
    return {};
}

} // namespace Octopus
