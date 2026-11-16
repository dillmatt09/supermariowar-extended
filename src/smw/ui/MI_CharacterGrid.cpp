#include "MI_CharacterGrid.h"

#include "FileList.h"
#include "GameValues.h"
#include "GlobalConstants.h"
#include "input.h"
#include "RandomNumberGenerator.h"
#include "ResourceManager.h"

#include <algorithm>
#include <iostream>

extern CGameValues game_values;
extern CResourceManager* rm;
extern SkinList* skinlist;
extern SDL_Surface* blitdest;

namespace {
// Layout, in screen pixels (below the header bar of the screen)
constexpr short DIALOG_X = 16;
constexpr short DIALOG_Y = 40;
constexpr short DIALOG_W = 608;
constexpr short DIALOG_H = 424;
constexpr short COLUMNS = 14;
constexpr short VISIBLE_ROWS = 7;
constexpr short CELL = 40;
constexpr short GRID_X = (640 - COLUMNS * CELL) / 2;
constexpr short GRID_Y = 84;
constexpr short NAME_Y = GRID_Y + VISIBLE_ROWS * CELL + 10;
constexpr short HINT_Y = NAME_Y + 36;

// Holding a direction: first repeat after this many frames, then every REPEAT_RATE frames
constexpr short REPEAT_DELAY = 20;
constexpr short REPEAT_RATE = 4;

void fillRect(short x, short y, short w, short h, Uint8 r, Uint8 g, Uint8 b)
{
    SDL_Rect rect {x, y, w, h};
    SDL_FillSurfaceRect(blitdest, &rect, SDL_MapSurfaceRGB(blitdest, r, g, b));
}

void frameRect(short x, short y, short w, short h, short t, Uint8 r, Uint8 g, Uint8 b)
{
    fillRect(x, y, w, t, r, g, b);
    fillRect(x, y + h - t, w, t, r, g, b);
    fillRect(x, y, t, h, r, g, b);
    fillRect(x + w - t, y, t, h, r, g, b);
}

/// Draws the menu dialog frame (menu_dialog.png, 512x480 with 16px borders) at any size
void drawDialog(short x, short y, short w, short h)
{
    const gfxSprite& dialog = rm->menu_dialog;
    constexpr short B = 16;
    constexpr short SRC_W = 512;
    constexpr short SRC_H = 480;
    constexpr short MID_W = SRC_W - 2 * B;
    constexpr short MID_H = SRC_H - 2 * B;

    for (short dy = B; dy < h - B; dy += MID_H) {
        const short ph = std::min<short>(MID_H, h - B - dy);
        for (short dx = B; dx < w - B; dx += MID_W) {
            const short pw = std::min<short>(MID_W, w - B - dx);
            dialog.draw(x + dx, y + dy, {B, B, pw, ph});
        }
        dialog.draw(x, y + dy, {0, B, B, ph});
        dialog.draw(x + w - B, y + dy, {SRC_W - B, B, B, ph});
    }
    for (short dx = B; dx < w - B; dx += MID_W) {
        const short pw = std::min<short>(MID_W, w - B - dx);
        dialog.draw(x + dx, y, {B, 0, pw, B});
        dialog.draw(x + dx, y + h - B, {B, SRC_H - B, pw, B});
    }
    dialog.draw(x, y, {0, 0, B, B});
    dialog.draw(x + w - B, y, {SRC_W - B, 0, B, B});
    dialog.draw(x, y + h - B, {0, SRC_H - B, B, B});
    dialog.draw(x + w - B, y + h - B, {SRC_W - B, SRC_H - B, B, B});
}

/// "4matsy_Darknut" -> "4matsy Darknut"
std::string displayName(const std::string& name)
{
    std::string result = name;
    std::replace(result.begin(), result.end(), '_', ' ');
    return result;
}
}  // namespace

void MI_CharacterGrid::open(short owner, short target)
{
    m_owner = owner;
    m_target = target;
    m_holdTimer = 0;
    loadSkins(game_values.colorids[target]);

    m_cursor = game_values.skinids[target];
    if (m_cursor < 0 || m_cursor >= static_cast<short>(m_skins.size()))
        m_cursor = 0;
    m_topRow = 0;
    scrollToCursor();
}

void MI_CharacterGrid::close()
{
    m_owner = -1;
    m_target = -1;
}

void MI_CharacterGrid::loadSkins(short colorID)
{
    if (colorID == m_loadedColor && m_skins.size() == skinlist->count())
        return;

    m_skins.clear();
    m_valid.clear();
    m_skins.reserve(skinlist->count());
    for (size_t i = 0; i < skinlist->count(); i++) {
        try {
            m_skins.push_back(gfx_loadmenuskin(skinlist->at(i).path, colorID, false));
            m_valid.push_back(true);
        } catch (const std::string& what) {
            std::cout << "ERROR: " << what << std::endl;
            m_skins.emplace_back();
            m_valid.push_back(false);
        }
    }
    m_loadedColor = colorID;
}

bool MI_CharacterGrid::isValid(short skin) const
{
    return skin >= 0 && skin < static_cast<short>(m_valid.size()) && m_valid[skin];
}

short MI_CharacterGrid::rowCount() const
{
    return static_cast<short>((m_skins.size() + COLUMNS - 1) / COLUMNS);
}

void MI_CharacterGrid::scrollToCursor()
{
    const short row = m_cursor / COLUMNS;
    if (row < m_topRow)
        m_topRow = row;
    else if (row >= m_topRow + VISIBLE_ROWS)
        m_topRow = row - VISIBLE_ROWS + 1;
}

void MI_CharacterGrid::moveCursor(short dx, short dy)
{
    const short count = static_cast<short>(m_skins.size());
    if (count == 0)
        return;

    if (dx != 0) {
        // Left and right go through the characters in order, wrapping around
        m_cursor = static_cast<short>((m_cursor + dx + count) % count);
    } else {
        // Up and down change the row, wrapping from the bottom to the top
        const short rows = rowCount();
        short row = m_cursor / COLUMNS;
        const short column = m_cursor % COLUMNS;
        do {
            row = static_cast<short>((row + dy + rows) % rows);
        } while (row * COLUMNS + column >= count && row != m_cursor / COLUMNS);
        m_cursor = std::min<short>(row * COLUMNS + column, count - 1);
    }
    scrollToCursor();
}

MI_CharacterGrid::Result MI_CharacterGrid::handleInput(const COutputControl& keys)
{
    if (keys.menu_cancel.fPressed || keys.menu_scrollfast.fPressed)
        return Result::Cancelled;

    if (keys.menu_select.fPressed) {
        if (isValid(m_cursor))
            return Result::Chosen;
        return Result::None;
    }

    if (keys.menu_random.fPressed && !m_skins.empty()) {
        do {
            m_cursor = static_cast<short>(RANDOM_INT(m_skins.size()));
        } while (!isValid(m_cursor) && std::find(m_valid.begin(), m_valid.end(), true) != m_valid.end());
        scrollToCursor();
    }

    struct Direction {
        const CKeyState& key;
        short dx, dy;
    };
    const Direction directions[] = {
        {keys.menu_left, -1, 0}, {keys.menu_right, 1, 0}, {keys.menu_up, 0, -1}, {keys.menu_down, 0, 1}};

    bool held = false;
    for (const Direction& d : directions) {
        if (d.key.fPressed) {
            moveCursor(d.dx, d.dy);
            m_holdTimer = 0;
        } else if (d.key.fDown) {
            held = true;
            if (++m_holdTimer >= REPEAT_DELAY) {
                moveCursor(d.dx, d.dy);
                m_holdTimer = REPEAT_DELAY - REPEAT_RATE;
            }
        }
    }
    if (!held)
        m_holdTimer = 0;

    return Result::None;
}

void MI_CharacterGrid::update()
{
    if (++m_animationTimer > 7) {
        m_animationTimer = 0;
        m_animationFrame = m_animationFrame == PGFX_STANDING_R ? PGFX_RUNNING_R : PGFX_STANDING_R;
    }
}

void MI_CharacterGrid::draw() const
{
    if (!isOpen())
        return;

    drawDialog(DIALOG_X, DIALOG_Y, DIALOG_W, DIALOG_H);

    // "Choose a Character", and for whom
    rm->menu_font_large.drawCentered(320, DIALOG_Y + 14, "Choose a Character");
    rm->spr_menu_boxed_numbers.draw(DIALOG_X + 24, DIALOG_Y + 16, {m_target * 16, game_values.colorids[m_target] * 16, 16, 16});

    const short count = static_cast<short>(m_skins.size());
    for (short row = 0; row < VISIBLE_ROWS; row++) {
        for (short column = 0; column < COLUMNS; column++) {
            const short skin = (m_topRow + row) * COLUMNS + column;
            if (skin >= count)
                break;

            const short x = GRID_X + column * CELL;
            const short y = GRID_Y + row * CELL;
            const bool selected = skin == m_cursor;

            if (selected)
                fillRect(x + 1, y + 1, CELL - 2, CELL - 2, 255, 230, 120);
            else
                fillRect(x + 1, y + 1, CELL - 2, CELL - 2, 40, 40, 64);

            if (isValid(skin)) {
                const short frame = selected ? m_animationFrame : PGFX_STANDING_R;
                m_skins[skin][frame].draw(x + (CELL - 32) / 2, y + (CELL - 32) / 2, {0, 0, 32, 32});
            }

            if (selected)
                frameRect(x, y, CELL, CELL, 2, 248, 88, 0);
            if (skin == game_values.skinids[m_target] && !game_values.randomskin[m_target])
                frameRect(x + 3, y + 3, CELL - 6, CELL - 6, 1, 255, 255, 255);
        }
    }

    // Where the visible rows are, when there are more than fit
    if (rowCount() > VISIBLE_ROWS) {
        const short pages = static_cast<short>((rowCount() + VISIBLE_ROWS - 1) / VISIBLE_ROWS);
        const short page = std::min<short>(pages, static_cast<short>((m_topRow + VISIBLE_ROWS - 1) / VISIBLE_ROWS + 1));
        rm->menu_font_large.drawRightJustified(DIALOG_X + DIALOG_W - 24, DIALOG_Y + 14,
            std::to_string(page) + "/" + std::to_string(pages));
    }

    if (m_cursor >= 0 && m_cursor < count)
        rm->menu_font_large.drawCentered(320, NAME_Y, displayName(skinlist->at(m_cursor).name));

    rm->menu_font_small.drawCentered(320, HINT_Y, "Select: Choose  Random: Random  Cancel: Back");
}
