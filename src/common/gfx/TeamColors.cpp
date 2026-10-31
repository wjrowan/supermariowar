#include "TeamColors.h"

#include "GlobalConstants.h"
#include "gfx/gfxSprite.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>


namespace team_colors {
namespace {
struct GeneratedTeam {
    size_t source;
    float hueShift;  // in degrees
};

// Placeholder colors for teams 5-8
constexpr std::array<GeneratedTeam, 4> GENERATED_TEAMS {{
    {3, 60.f},   // blue -> purple
    {0, 30.f},   // red -> orange
    {1, 60.f},   // green -> cyan
    {0, -40.f},  // red -> pink
}};
static_assert(ORIGINAL_COUNT + GENERATED_TEAMS.size() >= MAX_TEAMS);


RGB shiftHue(RGB color, float degrees)
{
    const float r = color.r / 255.f;
    const float g = color.g / 255.f;
    const float b = color.b / 255.f;
    const float max = std::max({r, g, b});
    const float min = std::min({r, g, b});
    const float chroma = max - min;
    if (chroma == 0.f)
        return color;

    float hue;
    if (max == r)
        hue = 60.f * std::fmod((g - b) / chroma, 6.f);
    else if (max == g)
        hue = 60.f * ((b - r) / chroma + 2.f);
    else
        hue = 60.f * ((r - g) / chroma + 4.f);

    hue = std::fmod(hue + degrees, 360.f);
    if (hue < 0.f)
        hue += 360.f;

    // Back to RGB, keeping the original saturation and brightness
    const float x = chroma * (1.f - std::fabs(std::fmod(hue / 60.f, 2.f) - 1.f));
    float r1 = 0.f, g1 = 0.f, b1 = 0.f;
    switch (static_cast<int>(hue / 60.f) % 6) {
        case 0: r1 = chroma; g1 = x; break;
        case 1: r1 = x; g1 = chroma; break;
        case 2: g1 = chroma; b1 = x; break;
        case 3: g1 = x; b1 = chroma; break;
        case 4: r1 = x; b1 = chroma; break;
        default: r1 = chroma; b1 = x; break;
    }

    const auto toByte = [min](float value) {
        return static_cast<uint8_t>(std::lround(std::clamp(value + min, 0.f, 1.f) * 255.f));
    };
    return RGB {toByte(r1), toByte(g1), toByte(b1)};
}
} // namespace


size_t sourceTeam(size_t team)
{
    return team < ORIGINAL_COUNT ? team : GENERATED_TEAMS[team - ORIGINAL_COUNT].source;
}

RGB colorFor(RGB sourceColor, size_t team)
{
    if (team < ORIGINAL_COUNT)
        return sourceColor;

    return shiftHue(sourceColor, GENERATED_TEAMS[team - ORIGINAL_COUNT].hueShift);
}

size_t variantIndex(size_t team, int trailingVariants)
{
    return team < ORIGINAL_COUNT ? team : team + static_cast<size_t>(trailingVariants);
}

bool extendSheet(gfxSprite& sprite, Axis axis, int stride, int firstVariant, int trailingVariants)
{
    SDL_Surface* src = sprite.getSurface();
    if (!src)
        return false;

    const int length = axis == Axis::X ? src->w : src->h;
    const int originalVariants = firstVariant + static_cast<int>(ORIGINAL_COUNT) + trailingVariants;
    if (length != stride * originalVariants)
        return false;  // the sheet already has more teams, or a layout we don't know

    const int newLength = stride * (originalVariants + MAX_TEAMS - static_cast<int>(ORIGINAL_COUNT));
    SdlSurfacePtr dst(SDL_CreateSurface(
        axis == Axis::X ? newLength : src->w,
        axis == Axis::Y ? newLength : src->h,
        src->format));
    if (!dst) {
        fprintf(stderr, "[gfx] Couldn't extend sprite sheet with team colors: %s\n", SDL_GetError());
        return false;
    }

    Uint32 keyPixel = 0;
    const bool hasKey = SDL_GetSurfaceColorKey(src, &keyPixel);
    RGB key {};
    if (hasKey)
        SDL_GetRGB(keyPixel, SDL_GetPixelFormatDetails(src->format), SDL_GetSurfacePalette(src), &key.r, &key.g, &key.b);

    SDL_LockSurface(src);
    SDL_LockSurface(dst.get());

    const auto copyPixel = [&](int srcX, int srcY, int dstX, int dstY, size_t team) {
        Uint8 r, g, b, a;
        SDL_ReadSurfacePixel(src, srcX, srcY, &r, &g, &b, &a);
        RGB color {r, g, b};
        if (!hasKey || color != key)
            color = colorFor(color, team);
        SDL_WriteSurfacePixel(dst.get(), dstX, dstY, color.r, color.g, color.b, a);
    };

    // The original variants
    for (int y = 0; y < src->h; y++) {
        for (int x = 0; x < src->w; x++)
            copyPixel(x, y, x, y, 0);
    }

    // The generated ones
    const int crossLength = axis == Axis::X ? src->h : src->w;
    for (size_t team = ORIGINAL_COUNT; team < MAX_TEAMS; team++) {
        const int srcStart = stride * (firstVariant + static_cast<int>(sourceTeam(team)));
        const int dstStart = stride * (firstVariant + static_cast<int>(variantIndex(team, trailingVariants)));
        for (int along = 0; along < stride; along++) {
            for (int across = 0; across < crossLength; across++) {
                if (axis == Axis::X)
                    copyPixel(srcStart + along, across, dstStart + along, across, team);
                else
                    copyPixel(across, srcStart + along, across, dstStart + along, team);
            }
        }
    }

    SDL_UnlockSurface(dst.get());
    SDL_UnlockSurface(src);

    // Keep how the original was set up by the image loader
    if (hasKey)
        SDL_SetSurfaceColorKey(dst.get(), true, keyPixel);

    SDL_BlendMode blendMode;
    if (SDL_GetSurfaceBlendMode(src, &blendMode))
        SDL_SetSurfaceBlendMode(dst.get(), blendMode);

    Uint8 alpha;
    if (SDL_GetSurfaceAlphaMod(src, &alpha))
        SDL_SetSurfaceAlphaMod(dst.get(), alpha);

    if (SDL_SurfaceHasRLE(src))
        SDL_SetSurfaceRLE(dst.get(), true);

    const std::optional<int> wrap = sprite.isWrapping() ? std::optional<int>(640) : std::nullopt;
    sprite = gfxSprite(std::move(dst), wrap);
    return true;
}

} // namespace team_colors
