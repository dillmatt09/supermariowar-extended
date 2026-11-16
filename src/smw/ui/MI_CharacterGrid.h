#pragma once

#include "gfx.h"

#include <vector>

struct COutputControl;

/// A grid of all the characters (skins) to pick one from, shown over the team select screen.
/// One controller moves the cursor; the chosen character is for the player being set up
/// (themselves, or a CPU they're choosing for).
class MI_CharacterGrid {
public:
    enum class Result {
        None,      // still choosing
        Chosen,    // a character was picked: see chosenSkin()
        Cancelled,
    };

    bool isOpen() const { return m_owner >= 0; }
    /// The controller that uses the grid
    short owner() const { return m_owner; }
    /// The player the character is chosen for
    short target() const { return m_target; }
    short chosenSkin() const { return m_cursor; }

    /// Opens the grid with the cursor on the target's current character, in the target's color
    void open(short owner, short target);
    void close();

    Result handleInput(const COutputControl& keys);
    void update();
    void draw() const;

private:
    void loadSkins(short colorID);
    bool isValid(short skin) const;
    void moveCursor(short dx, short dy);
    void scrollToCursor();
    short rowCount() const;

    short m_owner = -1;
    short m_target = -1;
    short m_cursor = 0;
    short m_topRow = 0;

    // Holding a direction moves the cursor repeatedly
    short m_holdTimer = 0;

    short m_animationTimer = 0;
    short m_animationFrame = 0;

    short m_loadedColor = -1;
    std::vector<SpriteStrip> m_skins;  // the standing and running frames of every skin
    std::vector<bool> m_valid;         // false for skins that couldn't be loaded
};
