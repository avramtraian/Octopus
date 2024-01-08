/*
 * Copyright (c) 2023 Traian Avram. All rights reserved.
 * SPDX-License-Identifier: MIT.
 */

#include "Bitmap.h"
#include "Command.h"
#include "Font.h"
#include "MathUtils.h"
#include "Print.h"

namespace Octopus
{

static OwnPtr<Font> g_ticket_name_font;
static OwnPtr<Font> g_ticket_id_font;

static ResultOr<void>
draw_centered_text(const String& text, usize offset_x, usize offset_y, OwnPtr<Bitmap>& bitmap, const OwnPtr<Font>& font)
{
    TRY_ASSIGN(const IntRect text_rect, get_text_rect(text, 0, 0, font));
    const usize text_offset_x = offset_x - (text_rect.width / 2);
    const usize text_offset_y = offset_y - (text_rect.height / 2);

    TRY(draw_text_to_bitmap(text, text_offset_x, text_offset_y, {}, bitmap, font));
    return {};
}

class TicketAtlas
{
public:
    OCT_NONCOPYABLE(TicketAtlas);
    OCT_NONMOVABLE(TicketAtlas);

    struct CachedTicket
    {
        String name;
        String ticket_id;
    };

public:
    TicketAtlas(const OwnPtr<Bitmap>& ticket_template, usize rows_per_page, usize columns_per_page)
        : m_ticket_template(ticket_template)
        , m_rows_per_page(rows_per_page)
        , m_columns_per_page(columns_per_page)
    {
    }

    void register_to_generate(String name, String ticket_id)
    {
        CachedTicket cached_ticket;
        cached_ticket.name = std::move(name);
        cached_ticket.ticket_id = std::move(ticket_id);
        m_cached_tickets.emplace_back(std::move(cached_ticket));
    }

    ResultOr<void> generate()
    {
        const usize tickets_per_sheet = m_rows_per_page * m_columns_per_page;
        const usize full_sheets = m_cached_tickets.size() / tickets_per_sheet;
        const usize remainder = m_cached_tickets.size() % tickets_per_sheet;

        // Generate sheets that are completely full.
        for (usize sheet_index = 0; sheet_index < full_sheets; ++sheet_index)
            TRY(generate_sheet({ m_cached_tickets.data() + (sheet_index * tickets_per_sheet), tickets_per_sheet }));

        if (remainder > 0)
        {
            // Generate the last sheet, that is partially occupied.
            TRY(generate_sheet({ m_cached_tickets.data() + (full_sheets * tickets_per_sheet), remainder }));
        }

        return {};
    }

    NODISCARD ALWAYS_INLINE const Vector<OwnPtr<Bitmap>>& sheets() const { return m_sheets; }

private:
    static ResultOr<void>
    generate_ticket(const CachedTicket& cached_ticket, usize offset_x, usize offset_y, OwnPtr<Bitmap>& sheet_bitmap)
    {
        constexpr usize name_offset_x = 657;
        constexpr usize name_offset_y = 150;

        TRY(draw_centered_text(
            cached_ticket.name, offset_x + name_offset_x, offset_y + name_offset_y, sheet_bitmap, g_ticket_name_font
        ));
        return {};
    }

    static ResultOr<void> generate_ticket_rotated(
        const CachedTicket& cached_ticket, usize offset_x, usize offset_y, OwnPtr<Bitmap>& sheet_bitmap
    )
    {
        constexpr usize id_offset_x = 303;
        constexpr usize id_offset_y = 315;

        TRY(draw_centered_text(
            cached_ticket.ticket_id, offset_x + id_offset_x, offset_y + id_offset_y, sheet_bitmap, g_ticket_id_font
        ));
        return {};
    }

    ResultOr<void> generate_sheet(Span<CachedTicket> cached_tickets)
    {
        if (cached_tickets.size() > m_rows_per_page * m_columns_per_page)
            return Result(Result::InvalidParameter);

        const usize sheet_width = m_columns_per_page * m_ticket_template->width();
        const usize sheet_height = m_rows_per_page * m_ticket_template->height();
        TRY_ASSIGN(OwnPtr<Bitmap> sheet_bitmap, Bitmap::create_from_color(sheet_width, sheet_height, Color::white()));

        {
            usize cached_ticket_index = 0;
            for (usize row = 0; row < m_rows_per_page; ++row)
            {
                for (usize column = 0; column < m_columns_per_page; ++column)
                {
                    if (cached_ticket_index >= cached_tickets.size())
                        break;

                    const usize offset_x = column * m_ticket_template->width();
                    const usize offset_y = row * m_ticket_template->height();

                    TRY(sheet_bitmap->fill_opaque_bitmap(offset_x, offset_y, m_ticket_template));
                    TRY(generate_ticket(cached_tickets[cached_ticket_index], offset_x, offset_y, sheet_bitmap));

                    ++cached_ticket_index;
                }
            }
        }

        // Rotate the bitmap.
        TRY_ASSIGN(sheet_bitmap, Bitmap::rotate(sheet_bitmap, 1));

        {
            usize cached_ticket_index = 0;
            for (usize row = 0; row < m_rows_per_page; ++row)
            {
                for (usize column = 0; column < m_columns_per_page; ++column)
                {
                    if (cached_ticket_index >= cached_tickets.size())
                        break;

                    // NOTE: Hard to explain. The sheet is rotated.
                    const usize offset_x = row * m_ticket_template->height();
                    const usize offset_y = (m_columns_per_page - column - 1) * m_ticket_template->width();
                    TRY(generate_ticket_rotated(cached_tickets[cached_ticket_index], offset_x, offset_y, sheet_bitmap));

                    ++cached_ticket_index;
                }
            }
        }

        // Rotate back the bitmap.
        TRY_ASSIGN(sheet_bitmap, Bitmap::rotate(sheet_bitmap, 3));
        m_sheets.emplace_back(std::move(sheet_bitmap));
        return {};
    }

private:
    const OwnPtr<Bitmap>& m_ticket_template;
    usize m_rows_per_page;
    usize m_columns_per_page;
    Vector<OwnPtr<Bitmap>> m_sheets;
    Vector<CachedTicket> m_cached_tickets;
};

static ResultOr<void>
register_tickets_for_grade(const OwnPtr<Table>& table, TicketAtlas& atlas, u8 grade, char grade_id)
{
    TRY(table->iterate_over_entries(
        [&](TicketID ticket_id, const TableEntry& entry) -> ResultOr<IterationDecision>
        {
            if (entry.grade == grade && entry.grade_id == grade_id)
            {
                auto ticket_id_string = transform_to_base_36(ticket_id);
                for (i32 index = ticket_id_string.length() - 1; index >= 0; --index)
                {
                    if (index != 0)
                        ticket_id_string.insert(index, " ");
                }

                atlas.register_to_generate(entry.last_name + " " + entry.first_name, ticket_id_string);
            }

            return IterationDecision::Continue;
        }
    ));

    return {};
}

PRIMARY_COMMAND_CALLBACK(primary_command_write_tickets)
{
    const String& database_filepath = context.arguments_string[0];
    TRY_ASSIGN(OwnPtr<Table> table, Table::create_from_file(database_filepath));

    TRY_ASSIGN(OwnPtr<Bitmap> ticket_bitmap, Bitmap::create_from_file(context.arguments_string[1]));

    // TODO: These hard-coded are only temporary. Maybe pass these file paths as arguments to the command?
    TRY_ASSIGN(g_ticket_name_font, Font::create_from_ttf("Ananda Personal Use.ttf", 78.0F, { 174, 105, 16 }));
    TRY_ASSIGN(g_ticket_id_font, Font::create_from_ttf("MartianMono-Regular.ttf", 70.0F, { 0, 0, 0 }));

    TicketAtlas atlas = TicketAtlas(ticket_bitmap, 4, 2);

    for (u8 grade = 9; grade <= 12; ++grade)
        for (char grade_id = 'A'; grade_id <= 'F'; ++grade_id)
            TRY(register_tickets_for_grade(table, atlas, grade, grade_id));

    TRY(atlas.generate());

    const auto& sheets = atlas.sheets();
    for (usize index = 0; index < sheets.size(); ++index)
    {
        String filepath = context.arguments_string[2] + "/";
        filepath.append(std::to_string(index));
        filepath.append(".png");

        TRY(sheets[index]->save_to_file(filepath));
    }

    auto program_context = OwnPtr<ProgramContext>(new ProgramContext(std::move(table), false));
    if (!program_context)
        return Result(Result::OutOfMemory);
    return program_context;
}

// NOTE: clang-format doesn't handle initializer lists very well, so we disable it while
// defining the primary commands. It makes the code a lot easier to read.
// clang-format off
// NOLINTBEGIN

static PrimaryCommandRegister s_write_tickets_command(
    "write_tickets", { "wt" },
    {
        { CommandSyntax::Type::String, "database_filepath" },
        { CommandSyntax::Type::String, "ticket_image_filepath" },
        { CommandSyntax::Type::String, "destination_folder_path" }
    },
    {},
    primary_command_write_tickets,
    "Writes the tickets from a database."
);

// NOLINTEND
// clang-format on

} // namespace Octopus
