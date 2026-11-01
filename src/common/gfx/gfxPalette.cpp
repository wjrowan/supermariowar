#include "gfxPalette.h"

#include "gfx.h"
#include "gfx/TeamColors.h"

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

    // The first row has the key colors, then each team has a row for every player state.
    // Teams without rows in the image get generated colors.
    const size_t teamsInImage = static_cast<size_t>(sprite.getHeight() - 1) / PlayerPalette::COUNT;
    if (teamsInImage == 0) {
        fprintf(stderr, "[gfx] %s has no team colors\n", path.generic_string().c_str());
        return false;
    }

    if (SDL_MUSTLOCK(sprite.getSurface()))
        SDL_LockSurface(sprite.getSurface());

    for (int x = 0; x < sprite.getWidth(); x++) {
        RGB key = getRgb(sprite.getSurface(), x, 0);

        const auto colorAt = [&](size_t team, size_t playerState) {
            return getRgb(sprite.getSurface(), x, static_cast<int>(1 + team * PlayerPalette::COUNT + playerState));
        };

        ColorSheet sheet {};
        for (size_t team = 0; team < MAX_TEAMS; team++) {
            for (size_t playerState = 0; playerState < PlayerPalette::COUNT; playerState++) {
                RGB color;
                if (team < teamsInImage) {
                    color = colorAt(team, playerState);
                } else if (teamsInImage >= team_colors::ORIGINAL_COUNT) {
                    team_colors::Variants variants;
                    for (size_t original = 0; original < team_colors::ORIGINAL_COUNT; original++)
                        variants[original] = colorAt(original, playerState);
                    color = team_colors::colorFor(variants, team);
                } else {
                    color = colorAt(team % teamsInImage, playerState);
                }
                sheet.replacements[team * PlayerPalette::COUNT + playerState] = color;
            }
        }

        m_colorsheets.emplace(std::move(key), std::move(sheet));
    }

    if (SDL_MUSTLOCK(sprite.getSurface()))
        SDL_UnlockSurface(sprite.getSurface());

    return true;
}
