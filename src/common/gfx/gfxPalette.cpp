#include "gfxPalette.h"

#include "gfx.h"
#include "gfx/TeamColors.h"

#include <format>
#include <vector>

#include <SDL3_image/SDL_image.h>

#include <cstdio>

namespace fs = std::filesystem;


std::optional<RGB> gfxPalette::replacementFor(RGB keyColor, size_t teamIndex, PlayerPalette playerState) const
{
    const auto it = m_colorsheets.find(keyColor);
    return it != m_colorsheets.cend()
        ? std::make_optional(it->second.replacementFor(teamIndex, playerState))
        : std::nullopt;
}


bool gfxPalette::load(const fs::path& path)
{
    m_colorsheets.clear();

    const gfxSprite sprite = ImageLoader(path).withoutColorKey().withoutOptimization().create();

    if (SDL_MUSTLOCK(sprite.getSurface()))
        SDL_LockSurface(sprite.getSurface());

    const int baseRows = 1 + NUM_BASE_TEAM_COLORS * PlayerPalette::COUNT;
    const int fullRows = 1 + MAX_PLAYERS * PlayerPalette::COUNT;
    if (sprite.getHeight() < baseRows)
        throw std::format("The palette {} has too few rows", path.string());

    auto teamColor = [&](int x, size_t team, size_t playerState) {
        return getRgb(sprite.getSurface(), x, 1 + team * PlayerPalette::COUNT + playerState);
    };

    if (sprite.getHeight() >= fullRows) {
        // The palette has colors for every team, use them as they are
        for (int x = 0; x < sprite.getWidth(); x++) {
            ColorSheet sheet {};
            for (size_t team = 0; team < MAX_PLAYERS; team++) {
                for (size_t playerState = 0; playerState < PlayerPalette::COUNT; playerState++)
                    sheet.replacements[team * PlayerPalette::COUNT + playerState] = teamColor(x, team, playerState);
            }
            m_colorsheets.emplace(getRgb(sprite.getSurface(), x, 0), std::move(sheet));
        }
    } else {
        // The palette only has the 4 original teams, generate the rest.
        // In the skins, the team color is marked with the shades of red (the other key colors,
        // like the blue of the overalls, can also differ between teams, but they're not the team color).
        // Only these columns are recolored, everything else is copied from the base team.
        auto isTeamColorKey = [](RGB key) {
            return key.r > 0 && key.g == 0 && key.b == 0;
        };

        std::array<std::vector<RGB>, NUM_BASE_TEAM_COLORS> teamColorSamples;
        for (int x = 0; x < sprite.getWidth(); x++) {
            if (!isTeamColorKey(getRgb(sprite.getSurface(), x, 0)))
                continue;
            for (size_t team = 0; team < NUM_BASE_TEAM_COLORS; team++)
                teamColorSamples[team].push_back(teamColor(x, team, PlayerPalette::normal));
        }

        std::array<float, NUM_BASE_TEAM_COLORS> baseHues {};
        for (size_t team = 0; team < NUM_BASE_TEAM_COLORS; team++)
            baseHues[team] = teamcolors::dominantHue(teamColorSamples[team]);

        for (int x = 0; x < sprite.getWidth(); x++) {
            const RGB key = getRgb(sprite.getSurface(), x, 0);
            const bool recolor = isTeamColorKey(key);

            ColorSheet sheet {};
            for (size_t playerState = 0; playerState < PlayerPalette::COUNT; playerState++) {
                std::array<RGB, NUM_BASE_TEAM_COLORS> baseColors {};
                for (size_t team = 0; team < NUM_BASE_TEAM_COLORS; team++) {
                    baseColors[team] = teamColor(x, team, playerState);
                    sheet.replacements[team * PlayerPalette::COUNT + playerState] = baseColors[team];
                }

                for (size_t i = 0; i < teamcolors::GENERATED_COLORS.size(); i++) {
                    const size_t team = NUM_BASE_TEAM_COLORS + i;
                    const size_t baseTeam = teamcolors::GENERATED_COLORS[i].baseTeam;
                    sheet.replacements[team * PlayerPalette::COUNT + playerState] = recolor
                        ? teamcolors::generatePaletteColors(baseColors, baseHues)[i]
                        : baseColors[baseTeam];
                }
            }

            m_colorsheets.emplace(key, std::move(sheet));
        }
    }

    if (SDL_MUSTLOCK(sprite.getSurface()))
        SDL_UnlockSurface(sprite.getSurface());

    return true;
}
