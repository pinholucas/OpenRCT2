/*****************************************************************************
 * Copyright (c) 2014-2019 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <algorithm>
#include <iterator>
#include <openrct2-ui/interface/Dropdown.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2/Context.h>
#include <openrct2/Input.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/localisation/Localisation.h>
#include <openrct2/sprites.h>

// The maximum number of rows to list before items overflow into new columns
constexpr int32_t DROPDOWN_TEXT_MAX_ROWS = 32;

constexpr int32_t DROPDOWN_ITEM_HEIGHT = 12;

static constexpr const uint8_t _appropriateImageDropdownItemsPerRow[34] = {
    1, 1, 1, 1, 2, 2, 3, 3, 4, 3, 5, 4, 4, 5, 5, 5, 4, 5, 6, 5, 5, 7, 4, 5, 6, 5, 6, 6, 6, 6, 6, 8, 8, 8,
};

enum
{
    WIDX_BACKGROUND,
};

static rct_widget window_dropdown_widgets[] = {
    { WWT_IMGBTN, 0, 0, 0, 0, 0, static_cast<uint32_t>(SPR_NONE), STR_NONE },
    { WIDGETS_END },
};

static int32_t _dropdown_num_columns;
static int32_t _dropdown_num_rows;
static int32_t _dropdown_item_width;
static int32_t _dropdown_item_height;
static bool _dropdown_list_vertically;

int32_t gDropdownNumItems;
rct_string_id gDropdownItemsFormat[DROPDOWN_ITEMS_MAX_SIZE];
int64_t gDropdownItemsArgs[DROPDOWN_ITEMS_MAX_SIZE];
static std::bitset<DROPDOWN_ITEMS_MAX_SIZE> _dropdownItemsChecked = {};
static std::bitset<DROPDOWN_ITEMS_MAX_SIZE> _dropdownItemsDisabled = {};
bool gDropdownIsColour;
int32_t gDropdownLastColourHover;
int32_t gDropdownHighlightedIndex;
int32_t gDropdownDefaultIndex;

bool dropdown_is_checked(int32_t index)
{
    if (index < 0 || index >= static_cast<int32_t>(std::size(_dropdownItemsDisabled)))
    {
        return false;
    }
    return _dropdownItemsChecked[index];
}

bool dropdown_is_disabled(int32_t index)
{
    if (index < 0 || index >= static_cast<int32_t>(std::size(_dropdownItemsDisabled)))
    {
        return true;
    }
    return _dropdownItemsDisabled[index];
}

void dropdown_set_checked(int32_t index, bool value)
{
    if (index < 0 || index >= static_cast<int32_t>(std::size(_dropdownItemsDisabled)))
    {
        return;
    }
    _dropdownItemsChecked[index] = value;
}

void dropdown_set_disabled(int32_t index, bool value)
{
    if (index < 0 || index >= static_cast<int32_t>(std::size(_dropdownItemsDisabled)))
    {
        return;
    }
    _dropdownItemsDisabled[index] = value;
}

static void window_dropdown_paint(rct_window* w, rct_drawpixelinfo* dpi);

// clang-format off
static rct_window_event_list window_dropdown_events = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    window_dropdown_paint,
    nullptr
};
// clang-format on

/**
 * Shows a text dropdown menu.
 *  rct2: 0x006ECFB9
 *
 * @param x (cx)
 * @param y (dx)
 * @param extray (di)
 * @param flags (bh)
 * @param num_items (bx)
 * @param colour (al)
 */
void window_dropdown_show_text(const ScreenCoordsXY& screenPos, int32_t extray, uint8_t colour, uint8_t flags, size_t num_items)
{
    int32_t string_width, max_string_width;
    char buffer[256];

    // Calculate the longest string width
    max_string_width = 0;
    for (size_t i = 0; i < num_items; i++)
    {
        format_string(buffer, 256, gDropdownItemsFormat[i], static_cast<void*>(&gDropdownItemsArgs[i]));
        gCurrentFontSpriteBase = FONT_SPRITE_BASE_MEDIUM;
        string_width = gfx_get_string_width(buffer);
        max_string_width = std::max(string_width, max_string_width);
    }

    window_dropdown_show_text_custom_width(screenPos, extray, colour, 0, flags, num_items, max_string_width + 3);
}

/**
 * Shows a text dropdown menu.
 *  rct2: 0x006ECFB9, although 0x006ECE50 is real version
 *
 * @param x (cx)
 * @param y (dx)
 * @param extray (di)
 * @param flags (bh)
 * @param num_items (bx)
 * @param colour (al)
 * @param custom_height (ah) requires flag set as well
 */
void window_dropdown_show_text_custom_width(
    const ScreenCoordsXY& screenPos, int32_t extray, uint8_t colour, uint8_t custom_height, uint8_t flags, size_t num_items,
    int32_t width)
{
    rct_window* w;

    input_set_flag(static_cast<INPUT_FLAGS>(INPUT_FLAG_DROPDOWN_STAY_OPEN | INPUT_FLAG_DROPDOWN_MOUSE_UP), false);
    if (flags & DROPDOWN_FLAG_STAY_OPEN)
        input_set_flag(INPUT_FLAG_DROPDOWN_STAY_OPEN, true);

    window_dropdown_close();

    // Set and calculate num items, rows and columns
    _dropdown_item_width = width;
    _dropdown_item_height = (flags & DROPDOWN_FLAG_CUSTOM_HEIGHT) ? custom_height : DROPDOWN_ITEM_HEIGHT;
    gDropdownNumItems = static_cast<int32_t>(num_items);
    // There must always be at least one column to prevent dividing by zero
    if (gDropdownNumItems == 0)
    {
        _dropdown_num_columns = 1;
        _dropdown_num_rows = 0;
    }
    else
    {
        _dropdown_num_columns = (gDropdownNumItems + DROPDOWN_TEXT_MAX_ROWS - 1) / DROPDOWN_TEXT_MAX_ROWS;
        _dropdown_num_rows = (gDropdownNumItems + _dropdown_num_columns - 1) / _dropdown_num_columns;
    }

    // Text dropdowns are listed horizontally
    _dropdown_list_vertically = true;

    width = _dropdown_item_width * _dropdown_num_columns + 3;
    int32_t height = _dropdown_item_height * _dropdown_num_rows + 3;
    int32_t screenWidth = context_get_width();
    int32_t screenHeight = context_get_height();
    auto boundedScreenPos = screenPos;
    if (screenPos.x + width > screenWidth)
        boundedScreenPos.x = std::max(0, screenWidth - width);
    if (screenPos.y + height > screenHeight)
        boundedScreenPos.y = std::max(0, screenHeight - height);

    window_dropdown_widgets[WIDX_BACKGROUND].right = width;
    window_dropdown_widgets[WIDX_BACKGROUND].bottom = height;

    // Create the window
    w = window_create(
        boundedScreenPos + ScreenCoordsXY{ 0, extray }, window_dropdown_widgets[WIDX_BACKGROUND].right + 1,
        window_dropdown_widgets[WIDX_BACKGROUND].bottom + 1, &window_dropdown_events, WC_DROPDOWN, WF_STICK_TO_FRONT);
    w->widgets = window_dropdown_widgets;
    if (colour & COLOUR_FLAG_TRANSLUCENT)
        w->flags |= WF_TRANSPARENT;
    w->colours[0] = colour;

    // Input state
    gDropdownHighlightedIndex = -1;
    _dropdownItemsDisabled.reset();
    _dropdownItemsChecked.reset();
    gDropdownIsColour = false;
    gDropdownDefaultIndex = -1;
    input_set_state(INPUT_STATE_DROPDOWN_ACTIVE);
}

/**
 * Shows an image dropdown menu.
 *  rct2: 0x006ECFB9
 *
 * @param x (cx)
 * @param y (dx)
 * @param extray (di)
 * @param flags (bh)
 * @param numItems (bx)
 * @param colour (al)
 * @param itemWidth (bp)
 * @param itemHeight (ah)
 * @param numColumns (bl)
 */
void window_dropdown_show_image(
    int32_t x, int32_t y, int32_t extray, uint8_t colour, uint8_t flags, int32_t numItems, int32_t itemWidth,
    int32_t itemHeight, int32_t numColumns)
{
    int32_t width, height;
    rct_window* w;

    input_set_flag(static_cast<INPUT_FLAGS>(INPUT_FLAG_DROPDOWN_STAY_OPEN | INPUT_FLAG_DROPDOWN_MOUSE_UP), false);
    if (flags & DROPDOWN_FLAG_STAY_OPEN)
        input_set_flag(INPUT_FLAG_DROPDOWN_STAY_OPEN, true);

    // Close existing dropdown
    window_dropdown_close();

    // Set and calculate num items, rows and columns
    _dropdown_item_width = itemWidth;
    _dropdown_item_height = itemHeight;
    gDropdownNumItems = numItems;
    // There must always be at least one column to prevent dividing by zero
    if (gDropdownNumItems == 0)
    {
        _dropdown_num_columns = 1;
        _dropdown_num_rows = 0;
    }
    else
    {
        _dropdown_num_columns = numColumns;
        _dropdown_num_rows = gDropdownNumItems / _dropdown_num_columns;
        if (gDropdownNumItems % _dropdown_num_columns != 0)
            _dropdown_num_rows++;
    }

    // image dropdowns are listed horizontally
    _dropdown_list_vertically = false;

    // Calculate position and size
    width = _dropdown_item_width * _dropdown_num_columns + 3;
    height = _dropdown_item_height * _dropdown_num_rows + 3;

    int32_t screenWidth = context_get_width();
    int32_t screenHeight = context_get_height();
    if (x + width > screenWidth)
        x = std::max(0, screenWidth - width);
    if (y + height > screenHeight)
        y = std::max(0, screenHeight - height);
    window_dropdown_widgets[WIDX_BACKGROUND].right = width;
    window_dropdown_widgets[WIDX_BACKGROUND].bottom = height;

    // Create the window
    w = window_create(
        ScreenCoordsXY(x, y + extray), window_dropdown_widgets[WIDX_BACKGROUND].right + 1,
        window_dropdown_widgets[WIDX_BACKGROUND].bottom + 1, &window_dropdown_events, WC_DROPDOWN, WF_STICK_TO_FRONT);
    w->widgets = window_dropdown_widgets;
    if (colour & COLOUR_FLAG_TRANSLUCENT)
        w->flags |= WF_TRANSPARENT;
    w->colours[0] = colour;

    // Input state
    gDropdownHighlightedIndex = -1;
    _dropdownItemsDisabled.reset();
    _dropdownItemsChecked.reset();
    gDropdownIsColour = false;
    gDropdownDefaultIndex = -1;
    input_set_state(INPUT_STATE_DROPDOWN_ACTIVE);
}

void window_dropdown_close()
{
    window_close_by_class(WC_DROPDOWN);
}

static void window_dropdown_paint(rct_window* w, rct_drawpixelinfo* dpi)
{
    int32_t cell_x, cell_y, l, t, r, b, item, image, colour;

    window_draw_widgets(w, dpi);

    int32_t highlightedIndex = gDropdownHighlightedIndex;
    for (int32_t i = 0; i < gDropdownNumItems; i++)
    {
        if (_dropdown_list_vertically)
        {
            cell_x = i / _dropdown_num_rows;
            cell_y = i % _dropdown_num_rows;
        }
        else
        {
            cell_x = i % _dropdown_num_columns;
            cell_y = i / _dropdown_num_columns;
        }

        if (gDropdownItemsFormat[i] == DROPDOWN_SEPARATOR)
        {
            l = w->windowPos.x + 2 + (cell_x * _dropdown_item_width);
            t = w->windowPos.y + 2 + (cell_y * _dropdown_item_height);
            r = l + _dropdown_item_width - 1;
            t += (_dropdown_item_height / 2);
            b = t;

            if (w->colours[0] & COLOUR_FLAG_TRANSLUCENT)
            {
                translucent_window_palette palette = TranslucentWindowPalettes[BASE_COLOUR(w->colours[0])];
                gfx_filter_rect(dpi, l, t, r, b, palette.highlight);
                gfx_filter_rect(dpi, l, t + 1, r, b + 1, palette.shadow);
            }
            else
            {
                gfx_fill_rect(dpi, l, t, r, b, ColourMapA[w->colours[0]].mid_dark);
                gfx_fill_rect(dpi, l, t + 1, r, b + 1, ColourMapA[w->colours[0]].lightest);
            }
        }
        else
        {
            //
            if (i == highlightedIndex)
            {
                l = w->windowPos.x + 2 + (cell_x * _dropdown_item_width);
                t = w->windowPos.y + 2 + (cell_y * _dropdown_item_height);
                r = l + _dropdown_item_width - 1;
                b = t + _dropdown_item_height - 1;
                gfx_filter_rect(dpi, l, t, r, b, PALETTE_DARKEN_3);
            }

            item = gDropdownItemsFormat[i];
            if (item == DROPDOWN_FORMAT_LAND_PICKER || item == DROPDOWN_FORMAT_COLOUR_PICKER)
            {
                // Image item
                image = static_cast<uint32_t>(gDropdownItemsArgs[i]);
                if (item == DROPDOWN_FORMAT_COLOUR_PICKER && highlightedIndex == i)
                    image++;

                gfx_draw_sprite(
                    dpi, image, w->windowPos.x + 2 + (cell_x * _dropdown_item_width),
                    w->windowPos.y + 2 + (cell_y * _dropdown_item_height), 0);
            }
            else
            {
                // Text item
                if (i < DROPDOWN_ITEMS_MAX_SIZE)
                {
                    if (dropdown_is_checked(i))
                    {
                        item++;
                    }
                }

                // Calculate colour
                colour = NOT_TRANSLUCENT(w->colours[0]);
                if (i == highlightedIndex)
                    colour = COLOUR_WHITE;
                if (dropdown_is_disabled(i))
                    if (i < DROPDOWN_ITEMS_MAX_SIZE)
                        colour = NOT_TRANSLUCENT(w->colours[0]) | COLOUR_FLAG_INSET;

                // Draw item string
                gfx_draw_string_left_clipped(
                    dpi, item, static_cast<void*>(&gDropdownItemsArgs[i]), colour,
                    w->windowPos.x + 2 + (cell_x * _dropdown_item_width), w->windowPos.y + 2 + (cell_y * _dropdown_item_height),
                    w->width - 5);
            }
        }
    }
}

/**
 * New function based on 6e914e
 * returns -1 if index is invalid
 */
int32_t dropdown_index_from_point(const ScreenCoordsXY& loc, rct_window* w)
{
    int32_t top = loc.y - w->windowPos.y - 2;
    if (top < 0)
        return -1;

    int32_t left = loc.x - w->windowPos.x;
    if (left >= w->width)
        return -1;
    left -= 2;
    if (left < 0)
        return -1;

    int32_t column_no = left / _dropdown_item_width;
    if (column_no >= _dropdown_num_columns)
        return -1;

    int32_t row_no = top / _dropdown_item_height;
    if (row_no >= _dropdown_num_rows)
        return -1;

    int32_t dropdown_index;
    if (_dropdown_list_vertically)
        dropdown_index = column_no * _dropdown_num_rows + row_no;
    else
        dropdown_index = row_no * _dropdown_num_columns + column_no;

    if (dropdown_index >= gDropdownNumItems)
        return -1;

    return dropdown_index;
}

/**
 *  rct2: 0x006ED43D
 */
void window_dropdown_show_colour(rct_window* w, rct_widget* widget, uint8_t dropdownColour, uint8_t selectedColour)
{
    int32_t defaultIndex = -1;
    // Set items
    for (uint64_t i = 0; i < COLOUR_COUNT; i++)
    {
        if (selectedColour == i)
            defaultIndex = i;

        gDropdownItemsFormat[i] = DROPDOWN_FORMAT_COLOUR_PICKER;
        gDropdownItemsArgs[i] = (i << 32) | (SPRITE_ID_PALETTE_COLOUR_1(i) | SPR_PALETTE_BTN);
    }

    // Show dropdown
    window_dropdown_show_image(
        w->windowPos.x + widget->left, w->windowPos.y + widget->top, widget->bottom - widget->top + 1, dropdownColour,
        DROPDOWN_FLAG_STAY_OPEN, COLOUR_COUNT, 12, 12, _appropriateImageDropdownItemsPerRow[COLOUR_COUNT]);

    gDropdownIsColour = true;
    gDropdownLastColourHover = -1;
    gDropdownDefaultIndex = defaultIndex;
}

uint32_t dropdown_get_appropriate_image_dropdown_items_per_row(uint32_t numItems)
{
    return numItems < std::size(_appropriateImageDropdownItemsPerRow) ? _appropriateImageDropdownItemsPerRow[numItems] : 8;
}
