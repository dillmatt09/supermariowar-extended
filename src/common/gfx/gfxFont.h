#pragma once

#include "gfx/gfxSprite.h"

#include <filesystem>
#include <limits>
#include <string_view>
#include <unordered_map>
#include <vector>


/// A font, that is either a bitmap font image of the graphics pack (the "Pixel" style),
/// or a TTF/OTF font rendered in the style of a bitmap font.
/// Text is UTF-8; characters missing from the font are drawn without their accents if possible.
class gfxFont {
public:
    explicit gfxFont() = default;
    /// Loads a bitmap font image
    explicit gfxFont(const std::filesystem::path& path);
    /// Renders a TTF/OTF font with the size and colors of a bitmap font image
    gfxFont(const std::filesystem::path& ttfPath, const std::filesystem::path& styleReferencePath);

    void draw(int x, int y, std::string_view text,
        int dst_min_x = std::numeric_limits<int>::min(),
        int dst_max_x = std::numeric_limits<int>::max()) const;

    void drawCentered(int x, int y, std::string_view text) const {
        draw(x - getWidth(text) / 2, y, text);
    }
    void drawChopCentered(int x, int y, int width, std::string_view text) const {
        width /= 2;
        draw(x - getWidth(text) / 2, y, text, x - width, x + width);
    }
    void drawRightJustified(int x, int y, std::string_view text) const {
        draw(x - getWidth(text), y, text);
    }
    void drawChopRight(int x, int y, int width, std::string_view text) const {
        draw(x, y, text, x, x + width);
    }
    void drawChopLeft(int x, int y, int width, std::string_view text) const {
        draw(x - getWidth(text), y, text, x - width, x);
    }

    void setAlpha(Uint8 alpha);

    int getHeight() const;
    int getWidth(std::string_view text) const;

    /// The name of a TTF/OTF font file (its family name), or an empty string if it can't be loaded
    static std::string fontFamilyName(const std::filesystem::path& ttfPath);

private:
    gfxSprite m_sprite;

    struct GlyphArea {
        int x = 0;
        int w = 0;
    };
    std::vector<GlyphArea> m_glyph_areas;  // printable ASCII characters, starting from '!'
    std::unordered_map<char32_t, GlyphArea> m_extra_glyphs;  // other characters (eg. accented letters)
    int m_space_width = 0;

    const GlyphArea* findGlyph(char32_t codepoint) const;
};
