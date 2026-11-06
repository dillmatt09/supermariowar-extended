#include "gfxFont.h"

#include "Localization.h"

#include "gfx.h"

#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <set>
#include <array>
#include <cmath>
#include <format>
#include <iostream>
#include <map>

extern SDL_Surface * blitdest;
extern SDL_Surface * screen;

namespace {
constexpr int ASCII_FIRST_PRINTABLE = 33;
constexpr int ASCII_LAST_PRINTABLE = 126;

/// Decodes the next UTF-8 character; invalid bytes are returned as they are
char32_t nextCodepoint(std::string_view text, size_t& pos)
{
    const auto byte = [&](size_t i) { return static_cast<unsigned char>(text[i]); };
    const unsigned char first = byte(pos);

    int length = 1;
    char32_t cp = first;
    if (first >= 0xF0 && first < 0xF8) { length = 4; cp = first & 0x07; }
    else if (first >= 0xE0) { length = 3; cp = first & 0x0F; }
    else if (first >= 0xC0) { length = 2; cp = first & 0x1F; }

    if (length == 1 || pos + length > text.size()) {
        pos++;
        return first;
    }
    for (int i = 1; i < length; i++) {
        const unsigned char cont = byte(pos + i);
        if ((cont & 0xC0) != 0x80) {
            pos++;
            return first;
        }
        cp = (cp << 6) | (cont & 0x3F);
    }
    pos += length;
    return cp;
}

/// A similar looking plain ASCII character, for fonts that don't have the character (eg. 'é' -> 'e')
char asciiFallback(char32_t cp)
{
    static const std::map<char32_t, char> table = [] {
        std::map<char32_t, char> t;
        const auto add = [&t](std::u32string_view from, char to) {
            for (char32_t c : from)
                t[c] = to;
        };
        add(U"ÀÁÂÃÄÅÆ", 'A'); add(U"àáâãäåæ", 'a');
        add(U"Ç", 'C'); add(U"ç", 'c');
        add(U"ÈÉÊË", 'E'); add(U"èéêë", 'e');
        add(U"ÌÍÎÏ", 'I'); add(U"ìíîï", 'i');
        add(U"Ñ", 'N'); add(U"ñ", 'n');
        add(U"ÒÓÔÕÖØŒ", 'O'); add(U"òóôõöøœ", 'o');
        add(U"ÙÚÛÜ", 'U'); add(U"ùúûü", 'u');
        add(U"ÝŸ", 'Y'); add(U"ýÿ", 'y');
        add(U"ß", 's');
        // ¡ and ¿ are left out: the pixel fonts have no inverted marks, and an upright one reads wrong
        add(U"«»“”„", '"'); add(U"‘’‚", '\'');
        add(U"–—", '-'); add(U"…", '.');
        add(U"\u00A0\u202F", ' ');
        return t;
    }();

    const auto it = table.find(cp);
    return it != table.end() ? it->second : 0;
}

/// The extra characters rendered from TTF/OTF fonts, besides printable ASCII
constexpr std::u32string_view EXTRA_CHARACTERS =
    U"¡¿«»ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÑÒÓÔÕÖØÙÚÛÜÝàáâãäåæçèéêëìíîïñòóôõöøùúûüýÿßŒœŸ‘’“”„…–—€°";

bool ensureTtfInit()
{
    if (TTF_WasInit() > 0)
        return true;
    return TTF_Init();
}

struct TtfFontDeleter {
    void operator()(TTF_Font* font) const { TTF_CloseFont(font); }
};
using TtfFontPtr = std::unique_ptr<TTF_Font, TtfFontDeleter>;

float luminance(RGB c) {
    return 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
}

/// The look of a bitmap font: its height, outline color and the fill color of each row
struct FontStyle {
    int height = 0;
    RGB outline {0, 0, 0};
    std::vector<RGB> rowFill;
    int capHeight = 0;      // rows of a capital letter, with its outline
    int baselineRow = 0;    // last row of a capital letter, with its outline
    int sampleWidth = 0;    // width of SIZE_SAMPLE in the bitmap font
};

/// A typical line of menu text, to compare the widths of fonts
constexpr std::string_view SIZE_SAMPLE = "Graphics Options Menu, Press Any Key To Continue - Player 1 Wins!";

FontStyle readFontStyle(const std::filesystem::path& path)
{
    auto raw = SdlSurfacePtr(IMG_Load(path.generic_string().c_str()));
    if (!raw)
        throw std::format("Couldn't load {}: {}", path.generic_string(), SDL_GetError());
    auto surf = SdlSurfacePtr(SDL_ConvertSurface(raw.get(), SDL_PIXELFORMAT_ARGB8888));

    FontStyle style;
    style.height = surf->h - 1;  // the first row contains the character markers

    const RGB key = getRgb(surf.get(), 0, 1);
    const auto isBackground = [&](RGB c) { return c == key || c == colors::MAGENTA; };

    // The outline is the darkest frequent color
    std::map<std::tuple<uint8_t, uint8_t, uint8_t>, int> counts;
    int total = 0;
    for (int y = 1; y < surf->h; y++) {
        for (int x = 0; x < surf->w; x++) {
            const RGB c = getRgb(surf.get(), x, y);
            if (!isBackground(c)) {
                counts[{c.r, c.g, c.b}]++;
                total++;
            }
        }
    }
    float darkest = 1000.f;
    for (const auto& [color, count] : counts) {
        const RGB c {std::get<0>(color), std::get<1>(color), std::get<2>(color)};
        if (count * 50 >= total && luminance(c) < darkest) {
            darkest = luminance(c);
            style.outline = c;
        }
    }

    // The fill color of each row is the average of the not-outline pixels
    style.rowFill.resize(style.height, RGB {255, 255, 255});
    int lastFilled = -1;
    for (int row = 0; row < style.height; row++) {
        long r = 0, g = 0, b = 0, n = 0;
        for (int x = 0; x < surf->w; x++) {
            const RGB c = getRgb(surf.get(), x, row + 1);
            if (isBackground(c) || luminance(c) < darkest + 40.f)
                continue;
            r += c.r; g += c.g; b += c.b; n++;
        }
        if (n > 0) {
            style.rowFill[row] = {uint8_t(r / n), uint8_t(g / n), uint8_t(b / n)};
            if (lastFilled < 0) {
                for (int prev = 0; prev < row; prev++)
                    style.rowFill[prev] = style.rowFill[row];
            }
            lastFilled = row;
        } else if (lastFilled >= 0) {
            style.rowFill[row] = style.rowFill[lastFilled];
        }
    }
    // Measure the letters, so that a TTF font can be drawn at the same size
    const std::vector<std::pair<int, int>> glyphs = [&] {
        std::vector<std::pair<int, int>> areas;  // start, width of each character from '!'
        int x = 0;
        const auto isMarker = [&](int px) { return getRgb(surf.get(), px, 0) == colors::MAGENTA; };
        while (x < surf->w && !isMarker(x))
            x++;
        while (x < surf->w) {
            while (x < surf->w && isMarker(x))
                x++;
            const int start = x;
            while (x < surf->w && !isMarker(x))
                x++;
            if (x > start)
                areas.emplace_back(start, x - start);
        }
        return areas;
    }();
    const auto glyphOf = [&](char c) -> const std::pair<int, int>* {
        const size_t i = static_cast<size_t>(c - ASCII_FIRST_PRINTABLE);
        return i < glyphs.size() ? &glyphs[i] : nullptr;
    };

    // The bitmap fonts use the width of '!' for spaces
    for (char c : SIZE_SAMPLE) {
        if (const auto* g = glyphOf(c == ' ' ? '!' : c))
            style.sampleWidth += g->second;
    }

    style.capHeight = style.height;
    style.baselineRow = style.height - 1;
    if (const auto* g = glyphOf('H')) {
        int top = -1, bottom = -1;
        for (int row = 0; row < style.height; row++) {
            for (int px = g->first; px < g->first + g->second; px++) {
                if (!isBackground(getRgb(surf.get(), px, row + 1))) {
                    if (top < 0)
                        top = row;
                    bottom = row;
                    break;
                }
            }
        }
        if (top >= 0) {
            style.capHeight = bottom - top + 1;
            style.baselineRow = bottom;
        }
    }
    return style;
}

Uint32* pixelAt(SDL_Surface* surf, int x, int y) {
    return reinterpret_cast<Uint32*>(static_cast<Uint8*>(surf->pixels) + y * surf->pitch) + x;
}

Uint8 alphaAt(SDL_Surface* surf, int x, int y) {
    if (x < 0 || y < 0 || x >= surf->w || y >= surf->h)
        return 0;
    return static_cast<Uint8>(*pixelAt(surf, x, y) >> 24);
}
} // namespace


gfxFont::gfxFont(const std::filesystem::path& path)
{
    const std::string path_str = path.generic_string();

    std::cout << "loading font " << path_str << " ...";
    auto surf = SdlSurfacePtr(IMG_Load(path_str.c_str()));
    if (!surf) {
        throw std::format("Couldn't load {}: {}", path_str, SDL_GetError());
    }

    if (SDL_MUSTLOCK(surf.get()))
        SDL_LockSurface(surf.get());

    std::vector<Uint32> first_row;
    first_row.reserve(surf->w);
    for (int x = 0; x < surf->w; x++)
        first_row.emplace_back(getRawPixel(surf.get(), x, 0));

    if (SDL_MUSTLOCK(surf.get()))
        SDL_UnlockSurface(surf.get());

    m_glyph_areas.reserve(127);

    const Uint32 raw_magenta = SDL_MapSurfaceRGB(surf.get(), 255, 0, 255);
    const int width = static_cast<int>(first_row.size());
    int x = 0;

    // Find first marker
    while (x < width && first_row[x] != raw_magenta)
        x++;

    while (x < width) {
        // Find marker end
        while (x < width && first_row[x] == raw_magenta)
            x++;

        if (width <= x)
            break;

        const int start = x;

        // Grow until next marker
        while (x < width && first_row[x] != raw_magenta)
            x++;

        const int width = x - start;
        m_glyph_areas.emplace_back(start, width);
    }

    if (m_glyph_areas.empty())
        throw std::format("Didn't find any characters on font image {}", path_str);

    m_glyph_areas.shrink_to_fit();
    m_space_width = m_glyph_areas.front().w;

    const Uint32 color_key = getRawPixel(surf.get(), 0, 1);
    if (!SDL_SetSurfaceColorKey(surf.get(), true, color_key))
        throw std::format("Couldn't set color key on font image: {}", SDL_GetError());

    auto surf_opti = SdlSurfacePtr(SDL_ConvertSurface(surf.get(), screen->format));
    if (!surf_opti)
        throw std::format("Couldn't convert {} to the display's pixel format: {}", path_str, SDL_GetError());

    if (!SDL_SetSurfaceRLE(surf_opti.get(), 1))
        throw std::format("Couldn't set RLE acceleration for {}: {}", path_str, SDL_GetError());

    m_sprite = gfxSprite(std::move(surf_opti), std::nullopt);
    std::cout << "done" << std::endl;
}

const gfxFont::GlyphArea* gfxFont::findGlyph(char32_t cp) const
{
    if (cp >= ASCII_FIRST_PRINTABLE && cp <= ASCII_LAST_PRINTABLE) {
        const size_t idx = cp - ASCII_FIRST_PRINTABLE;
        return idx < m_glyph_areas.size() ? &m_glyph_areas[idx] : nullptr;
    }

    const auto it = m_extra_glyphs.find(cp);
    if (it != m_extra_glyphs.end())
        return &it->second;

    const char fallback = asciiFallback(cp);
    if (fallback > ' ')
        return findGlyph(static_cast<char32_t>(fallback));

    return nullptr;
}

void gfxFont::draw(int x, int y, std::string_view text, int dst_min_x, int dst_max_x) const
{
    text = l10n::translate(text);
    size_t pos = 0;
    while (pos < text.size()) {
        if (dst_max_x < x)
            break;

        const char32_t cp = nextCodepoint(text, pos);
        if (cp < ASCII_FIRST_PRINTABLE || asciiFallback(cp) == ' ') {
            x += m_space_width;
            continue;
        }

        const GlyphArea* glyph = findGlyph(cp);
        if (!glyph)
            continue;

        const SDL_Rect src {
            glyph->x, 1,
            glyph->w, m_sprite.getHeight() - 1,
        };
        if (dst_min_x <= x + src.w)
            m_sprite.draw(x, y, src);

        x += glyph->w + m_letter_spacing;
    }
}

void gfxFont::setAlpha(Uint8 alpha)
{
    if (!SDL_SetSurfaceBlendMode(m_sprite.getSurface(), SDL_BLENDMODE_BLEND)) {
        fprintf(stderr, "\n ERROR: couldn't set blend mode on font surface: %s\n", SDL_GetError());
        return;
    }
    if (!SDL_SetSurfaceAlphaMod(m_sprite.getSurface(), alpha)) {
        fprintf(stderr, "\n ERROR: couldn't set alpha on font surface: %s\n", SDL_GetError());
        return;
    }
}

int gfxFont::getHeight() const
{
    return m_sprite.getHeight() - 1;
}

int gfxFont::getWidth(std::string_view text) const
{
    text = l10n::translate(text);
    int width = 0;
    size_t pos = 0;
    while (pos < text.size()) {
        const char32_t cp = nextCodepoint(text, pos);
        if (cp < ASCII_FIRST_PRINTABLE || asciiFallback(cp) == ' ') {
            width += m_space_width;
            continue;
        }
        if (const GlyphArea* glyph = findGlyph(cp))
            width += glyph->w + m_letter_spacing;
    }
    return width;
}


std::string gfxFont::fontFamilyName(const std::filesystem::path& ttfPath)
{
    if (!ensureTtfInit())
        return {};

    auto font = TtfFontPtr(TTF_OpenFont(ttfPath.generic_string().c_str(), 12.f));
    if (!font)
        return {};

    const char* family = TTF_GetFontFamilyName(font.get());
    const char* style = TTF_GetFontStyleName(font.get());
    std::string name = family ? family : ttfPath.stem().string();
    if (style && *style && std::string_view(style) != "Regular")
        name += std::string(" ") + style;
    return name;
}


gfxFont::gfxFont(const std::filesystem::path& ttfPath, const std::filesystem::path& styleReferencePath,
    const TtfOptions& options)
{
    const std::vector<std::string>& fitTexts = options.fitTexts;
    const int fitWidth = options.fitWidth;
    const std::string path_str = ttfPath.generic_string();
    std::cout << "loading font " << path_str << " ...";

    if (!ensureTtfInit())
        throw std::format("Couldn't initialize SDL_ttf: {}", SDL_GetError());

    const FontStyle style = readFontStyle(styleReferencePath);
    constexpr int OUTLINE = 1;
    // Fonts made to fit a small space share the outline between letters
    const int letterSpacing = fitWidth > 0 ? -OUTLINE : 0;
    m_letter_spacing = letterSpacing;

    auto font = TtfFontPtr(TTF_OpenFont(path_str.c_str(), static_cast<float>(style.height)));
    if (!font)
        throw std::format("Couldn't load {}: {}", path_str, SDL_GetError());

    // Find the largest size where the capital letters are no taller, and a typical line of text no
    // wider, than in the bitmap font
    TTF_SetFontHinting(font.get(), TTF_HINTING_MONO);
    const auto fits = [&]() {
        int capTop = 0;
        if (!TTF_GetGlyphMetrics(font.get(), 'H', nullptr, nullptr, nullptr, &capTop, nullptr))
            return TTF_GetFontHeight(font.get()) + 2 * OUTLINE <= style.height;
        if (capTop + 2 * OUTLINE > style.capHeight)
            return false;

        const auto widthOf = [&](std::string_view text) {
            int width = 0;
            size_t pos = 0;
            while (pos < text.size()) {
                const char32_t c = nextCodepoint(text, pos);
                int advance = 0;
                if (!TTF_GetGlyphMetrics(font.get(), c, nullptr, nullptr, nullptr, nullptr, &advance) || advance == 0)
                    advance = static_cast<int>(TTF_GetFontSize(font.get()) * 1.5f);  // from a fallback font (see below)
                width += advance + (c == ' ' ? OUTLINE : 2 * OUTLINE + letterSpacing);
            }
            return width;
        };
        if (fitWidth > 0) {
            for (const std::string& text : fitTexts) {
                if (widthOf(text) > fitWidth)
                    return false;
            }
            return true;
        }
        return style.sampleWidth <= 0 || widthOf(SIZE_SAMPLE) <= style.sampleWidth;
    };
    for (float size = static_cast<float>(style.height); size > 5.f; size -= 0.5f) {
        TTF_SetFontSize(font.get(), size);
        if (fits())
            break;
    }

    // Put the baseline where the bitmap font has it, as long as the descenders still fit
    const int cellHeight = style.height;
    const int fontHeight = TTF_GetFontHeight(font.get());
    const int ascent = TTF_GetFontAscent(font.get());
    int baselineY = style.baselineRow - OUTLINE + 1 - ascent;
    baselineY = std::min(baselineY, cellHeight - OUTLINE - fontHeight);
    baselineY = std::max(baselineY, OUTLINE - (ascent - [&] {
        int capTop = ascent;
        TTF_GetGlyphMetrics(font.get(), 'H', nullptr, nullptr, nullptr, &capTop, nullptr);
        return capTop;
    }()));

    std::u32string characters;
    for (char32_t c = ASCII_FIRST_PRINTABLE; c <= ASCII_LAST_PRINTABLE; c++)
        characters += c;
    {
        std::set<char32_t> seen(characters.begin(), characters.end());
        for (std::u32string_view extra : {EXTRA_CHARACTERS, std::u32string_view(options.characters)}) {
            for (char32_t c : extra) {
                if (c > ' ' && seen.insert(c).second)
                    characters += c;
            }
        }
    }

    // The fonts to take the characters from that the font doesn't have; opened when first needed,
    // and sized so that a full-width character fits in the height of the bitmap font
    struct Fallback {
        std::filesystem::path path;
        TtfFontPtr font;
        bool opened = false;
        int top = 0;  // where the rendered characters go in the cell
    };
    std::vector<Fallback> fallbacks;
    for (const auto& path : options.fallbacks)
        fallbacks.push_back({path, nullptr});
    const auto openFallback = [&](Fallback& fb) {
        fb.opened = true;
        fb.font = TtfFontPtr(TTF_OpenFont(fb.path.generic_string().c_str(), TTF_GetFontSize(font.get())));
        if (!fb.font)
            return;
        TTF_SetFontHinting(fb.font.get(), TTF_HINTING_MONO);

        // Measure a typical character of the script: an ideograph, a kana, a hangul or a cyrillic letter
        char32_t sample = 0;
        for (char32_t c : {U'国', U'あ', U'한', U'Ж', U'H'}) {
            if (TTF_FontHasGlyph(fb.font.get(), c)) {
                sample = c;
                break;
            }
        }
        int minY = 0, maxY = 0;
        const auto measure = [&] {
            return sample && TTF_GetGlyphMetrics(fb.font.get(), sample, nullptr, nullptr, &minY, &maxY, nullptr);
        };
        for (float size = TTF_GetFontSize(font.get()) * 1.5f; size > 5.f; size -= 0.5f) {
            TTF_SetFontSize(fb.font.get(), size);
            if (!measure() || maxY - minY + 2 * OUTLINE <= cellHeight)
                break;
        }
        // Center the typical character in the cell
        const int inkHeight = maxY - minY;
        fb.top = (cellHeight - inkHeight) / 2 - (TTF_GetFontAscent(fb.font.get()) - maxY);
    };
    const auto fontFor = [&](char32_t c) -> std::pair<TTF_Font*, int> {
        if (TTF_FontHasGlyph(font.get(), c))
            return {font.get(), baselineY};
        for (Fallback& fb : fallbacks) {
            if (!fb.opened)
                openFallback(fb);
            if (fb.font && TTF_FontHasGlyph(fb.font.get(), c))
                return {fb.font.get(), fb.top};
        }
        return {nullptr, 0};
    };

    // Render every character, colorize them in the style of the bitmap font
    struct Rendered {
        char32_t cp;
        SdlSurfacePtr surf;
    };
    std::vector<Rendered> rendered;
    int atlasWidth = 0;
    const SDL_Color white {255, 255, 255, 255};
    for (char32_t c : characters) {
        const bool isAscii = c >= ASCII_FIRST_PRINTABLE && c <= ASCII_LAST_PRINTABLE;
        const auto [glyphFont, glyphTop] = fontFor(c);
        if (!glyphFont && !isAscii)
            continue;  // no font has it (the ASCII characters keep their place in the atlas)
        TTF_Font* const renderFont = glyphFont ? glyphFont : font.get();
        auto glyph = SdlSurfacePtr(glyphFont ? TTF_RenderGlyph_Blended(renderFont, c, white) : nullptr);
        SdlSurfacePtr argb;
        int advance = 0;
        TTF_GetGlyphMetrics(renderFont, c, nullptr, nullptr, nullptr, nullptr, &advance);
        if (glyph)
            argb = SdlSurfacePtr(SDL_ConvertSurface(glyph.get(), SDL_PIXELFORMAT_ARGB8888));

        const int width = std::max(advance, argb ? argb->w : 0) + 2 * OUTLINE;
        auto cell = SdlSurfacePtr(SDL_CreateSurface(width, cellHeight, SDL_PIXELFORMAT_ARGB8888));
        SDL_FillSurfaceRect(cell.get(), nullptr, 0);

        if (argb) {
            // fill alpha at the cell position
            const auto fillAlpha = [&](int x, int y) { return alphaAt(argb.get(), x - OUTLINE, y - glyphTop); };
            for (int y = 0; y < cellHeight; y++) {
                for (int x = 0; x < width; x++) {
                    const Uint8 fill = fillAlpha(x, y);
                    Uint8 edge = 0;
                    for (int dy = -OUTLINE; dy <= OUTLINE; dy++)
                        for (int dx = -OUTLINE; dx <= OUTLINE; dx++)
                            edge = std::max(edge, fillAlpha(x + dx, y + dy));

                    if (edge == 0)
                        continue;

                    // Fill on top of the outline
                    const RGB fc = style.rowFill[std::min<size_t>(y, style.rowFill.size() - 1)];
                    const RGB oc = style.outline;
                    const float t = fill / 255.f;
                    const RGB c {
                        uint8_t(std::lround(oc.r + (fc.r - oc.r) * t)),
                        uint8_t(std::lround(oc.g + (fc.g - oc.g) * t)),
                        uint8_t(std::lround(oc.b + (fc.b - oc.b) * t)),
                    };
                    *pixelAt(cell.get(), x, y) = (Uint32(edge) << 24) | (Uint32(c.r) << 16) | (Uint32(c.g) << 8) | c.b;
                }
            }
        }

        atlasWidth += cell->w;
        rendered.push_back({c, std::move(cell)});
    }

    // Put them on one sheet; the first row is empty, like the marker row of the bitmap fonts
    auto atlas = SdlSurfacePtr(SDL_CreateSurface(std::max(1, atlasWidth), cellHeight + 1, SDL_PIXELFORMAT_ARGB8888));
    SDL_FillSurfaceRect(atlas.get(), nullptr, 0);
    SDL_SetSurfaceBlendMode(atlas.get(), SDL_BLENDMODE_NONE);

    int x = 0;
    for (const Rendered& r : rendered) {
        SDL_SetSurfaceBlendMode(r.surf.get(), SDL_BLENDMODE_NONE);
        SDL_Rect dst {x, 1, r.surf->w, r.surf->h};
        SDL_BlitSurface(r.surf.get(), nullptr, atlas.get(), &dst);

        const GlyphArea area {x, r.surf->w};
        if (r.cp >= ASCII_FIRST_PRINTABLE && r.cp <= ASCII_LAST_PRINTABLE)
            m_glyph_areas.push_back(area);
        else
            m_extra_glyphs.emplace(r.cp, area);
        x += r.surf->w;
    }

    int spaceAdvance = 0;
    TTF_GetGlyphMetrics(font.get(), ' ', nullptr, nullptr, nullptr, nullptr, &spaceAdvance);
    m_space_width = spaceAdvance + OUTLINE;

    SDL_SetSurfaceBlendMode(atlas.get(), SDL_BLENDMODE_BLEND);
    m_sprite = gfxSprite(std::move(atlas), std::nullopt);
    std::cout << "done" << std::endl;
}
