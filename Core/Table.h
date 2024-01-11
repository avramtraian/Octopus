/*
 * Copyright (c) 2023 Traian Avram. All rights reserved.
 * SPDX-License-Identifier: MIT.
 */

#pragma once

#include "Core.h"
#include "Result.h"

namespace Octopus
{

using TicketID = u64;
static constexpr TicketID invalid_ticket_id = 0;
static constexpr u64 invalid_ticket_generation = 0;

struct TableEntryFlag
{
    enum : u8
    {
        None = 0,
        NotScannable = BIT(0)
    };
};

struct TableEntryMetadata
{
    u32 flags = TableEntryFlag::None;
    u32 scan_count = 0;
    String last_scan_date;
};

/// Packs the 4 given characters into a 32-bit unsigned integer, with the first character
/// being the most significant byte and the last character the least significant byte.
#define FOUR_BYTE_HEADER(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | ((d) << 0))

/// Four-byte tag that every table entry must begin with.
static constexpr u32 table_entry_tag = FOUR_BYTE_HEADER('O', 'P', 'T', 'E');

struct TableEntry
{
public:
    /// If this value is different than table_entry_tag the table entry is corrupted
    /// and an error must be raised.
    u32 _entry_tag = table_entry_tag;
    TableEntryMetadata metadata;

    String first_name;
    String last_name;
    u8 grade = 0;
    char grade_id = 0;

public:
    NODISCARD ALWAYS_INLINE bool is_corrupted() const { return _entry_tag != table_entry_tag; }
    ALWAYS_INLINE ResultOr<void> check_corrupted(Result::Code result_code = Result::CorruptedTableEntry) const
    {
        return is_corrupted() ? Result(result_code) : ResultOr<void>();
    }
};

class Table
{
public:
    struct GeneratedTicketID
    {
        GeneratedTicketID& operator=(const GeneratedTicketID&) = delete;
        GeneratedTicketID(TicketID id, u64 generation)
            : id(id)
            , generation(generation)
        {
        }

        const TicketID id;
        const u64 generation;
    };

public:
    static ResultOr<OwnPtr<Table>> create_new();
    static ResultOr<OwnPtr<Table>> create_from_file(const String& filepath);
    ResultOr<void> save_to_file(const String& filepath) const;

    static ResultOr<void> format_entry(TableEntry& entry);

public:
    ResultOr<bool> is_ticket_id_valid(TicketID ticket_id) const;
    ResultOr<bool> has_generated_ticket_id_expired(GeneratedTicketID generated_ticket_id) const;
    ResultOr<GeneratedTicketID> generate_ticket_id();

    ResultOr<void> insert_entry_with_ticket_id(TicketID ticket_id, TableEntry entry);
    ResultOr<void> insert_entry_with_ticket_id(GeneratedTicketID generated_ticket_id, TableEntry entry);
    ResultOr<TicketID> insert_entry(TableEntry entry);

    ResultOr<void> remove_ticket(TicketID ticket_id);

    ResultOr<usize> entry_count() const;
    ResultOr<TableEntry&> get_entry(TicketID ticket_id);
    ResultOr<const TableEntry&> get_entry(TicketID ticket_id) const;
    ResultOr<Vector<TicketID>> find_ticket_id_by_name(StringView first_name, StringView last_name) const;

    template<typename Func>
    ALWAYS_INLINE ResultOr<void> iterate_over_entries(Func callback)
    {
        for (auto& [ticket_id, entry] : m_entries)
        {
            TRY(entry.check_corrupted(Result::CorruptedTable));
            TRY_ASSIGN(const IterationDecision decision, callback(ticket_id, entry));
            if (decision == IterationDecision::Break)
                break;
        }

        return {};
    }

    template<typename Func>
    ALWAYS_INLINE ResultOr<void> iterate_over_entries(Func callback) const
    {
        for (const auto& [ticket_id, entry] : m_entries)
        {
            TRY(entry.check_corrupted(Result::CorruptedTable));
            TRY_ASSIGN(const IterationDecision decision, callback(ticket_id, entry));
            if (decision == IterationDecision::Break)
                break;
        }

        return {};
    }

    ResultOr<void> increment_ticket_scan_count(TicketID ticket_id);

private:
    ResultOr<bool> similar_entry_already_exists(const TableEntry& entry) const;

private:
    Map<TicketID, TableEntry> m_entries;
    u64 m_ticket_id_generation = invalid_ticket_generation;
};

} // namespace Octopus
