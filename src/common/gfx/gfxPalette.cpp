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

        ColorSheet sheet {};
        for (size_t team = 0; team < MAX_TEAMS; team++) {
            const size_t imageTeam = team < teamsInImage ? team : team_colors::sourceTeam(team) % teamsInImage;
            for (size_t playerState = 0; playerState < PlayerPalette::COUNT; playerState++) {
                const size_t imageY = 1 + imageTeam * PlayerPalette::COUNT + playerState;
                const RGB color = getRgb(sprite.getSurface(), x, static_cast<int>(imageY));
                const size_t relativeY = team * PlayerPalette::COUNT + playerState;
                sheet.replacements[relativeY] = team < teamsInImage ? color : team_colors::colorFor(color, team);
            }
        }

        m_colorsheets.emplace(std::move(key), std::move(sheet));
    }

    if (SDL_MUSTLOCK(sprite.getSurface()))
        SDL_UnlockSurface(sprite.getSurface());

    return true;
}
