#include "TeamColors.h"

#include "GlobalConstants.h"
#include "gfx/gfxSprite.h"

#include <algorithm>
#include <cmath>
#include <cstdio>


namespace team_colors {
namespace {
// Placeholder colors for teams 5-8
constexpr std::array<RGB, 4> GENERATED_TEAM_COLORS {{
    {140, 60, 220},   // purple
    {235, 235, 235},  // white
    {0, 200, 200},    // cyan
    {255, 105, 180},  // pink
}};
static_assert(ORIGINAL_COUNT + GENERATED_TEAM_COLORS.size() >= MAX_TEAMS);

struct HSL {
    float h;  // 0-360
    float s;  // 0-1
    float l;  // 0-1
};

HSL toHsl(RGB color)
{
    const float r = color.r / 255.f;
    const float g = color.g / 255.f;
    const float b = color.b / 255.f;
    const float max = std::max({r, g, b});
    const float min = std::min({r, g, b});
    const float chroma = max - min;

    HSL hsl {0.f, 0.f, (max + min) / 2.f};
    if (chroma == 0.f)
        return hsl;

    hsl.s = chroma / (1.f - std::fabs(2.f * hsl.l - 1.f));
    if (max == r)
        hsl.h = 60.f * std::fmod((g - b) / chroma, 6.f);
    else if (max == g)
        hsl.h = 60.f * ((b - r) / chroma + 2.f);
    else
        hsl.h = 60.f * ((r - g) / chroma + 4.f);
    if (hsl.h < 0.f)
        hsl.h += 360.f;
    return hsl;
}

RGB toRgb(HSL hsl)
{
    const float chroma = (1.f - std::fabs(2.f * hsl.l - 1.f)) * hsl.s;
    const float x = chroma * (1.f - std::fabs(std::fmod(hsl.h / 60.f, 2.f) - 1.f));
    const float m = hsl.l - chroma / 2.f;

    float r = 0.f, g = 0.f, b = 0.f;
    switch (static_cast<int>(hsl.h / 60.f) % 6) {
        case 0: r = chroma; g = x; break;
        case 1: r = x; g = chroma; break;
        case 2: g = chroma; b = x; break;
        case 3: g = x; b = chroma; break;
        case 4: r = x; b = chroma; break;
        default: r = chroma; b = x; break;
    }

    const auto toByte = [m](float value) {
        return static_cast<uint8_t>(std::lround(std::clamp(value + m, 0.f, 1.f) * 255.f));
    };
    return RGB {toByte(r), toByte(g), toByte(b)};
}

// Paints a shade with the base color: its hue, a less saturated base gives less saturated shades,
// and a lighter or darker base shifts the shades a bit
RGB paint(float saturation, float lightness, size_t team)
{
    const HSL base = toHsl(baseColor(team));
    return toRgb(HSL {
        base.h,
        std::clamp(saturation * base.s, 0.f, 1.f),
        std::clamp(lightness + (base.l - 0.5f) * 0.5f, 0.f, 1.f),
    });
}
} // namespace


RGB baseColor(size_t team)
{
    return GENERATED_TEAM_COLORS[team - ORIGINAL_COUNT];
}

RGB colorFor(const Variants& variants, size_t team)
{
    if (team < ORIGINAL_COUNT)
        return variants[team];

    if (std::all_of(variants.cbegin(), variants.cend(), [&](RGB color) { return color == variants[0]; }))
        return variants[0];  // not a team colored pixel

    // The shade is how saturated and light the original teams' colors are on average
    float saturation = 0.f;
    float lightness = 0.f;
    for (const RGB& color : variants) {
        const HSL hsl = toHsl(color);
        saturation += hsl.s / ORIGINAL_COUNT;
        lightness += hsl.l / ORIGINAL_COUNT;
    }

    return paint(saturation, lightness, team);
}

RGB tint(RGB color, size_t team)
{
    if (team < ORIGINAL_COUNT)
        return color;

    const HSL hsl = toHsl(color);
    if (hsl.s == 0.f)
        return color;  // black, white and gray have no team color

    return paint(hsl.s, hsl.l, team);
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

    // The original variants
    for (int y = 0; y < src->h; y++) {
        for (int x = 0; x < src->w; x++) {
            Uint8 r, g, b, a;
            SDL_ReadSurfacePixel(src, x, y, &r, &g, &b, &a);
            SDL_WriteSurfacePixel(dst.get(), x, y, r, g, b, a);
        }
    }

    // The generated ones, from the same pixel in each original team's variant
    const auto pixelAt = [axis](int along, int across) {
        return axis == Axis::X ? std::pair {along, across} : std::pair {across, along};
    };
    const int crossLength = axis == Axis::X ? src->h : src->w;
    for (int along = 0; along < stride; along++) {
        for (int across = 0; across < crossLength; across++) {
            Variants variants;
            Uint8 alpha = 255;
            bool fTransparent = false;
            for (size_t team = 0; team < ORIGINAL_COUNT; team++) {
                const auto [x, y] = pixelAt(stride * (firstVariant + static_cast<int>(team)) + along, across);
                RGB& color = variants[team];
                Uint8 a;
                SDL_ReadSurfacePixel(src, x, y, &color.r, &color.g, &color.b, &a);
                if (team == 0)
                    alpha = a;
                fTransparent = fTransparent || (hasKey && color == key);
            }

            for (size_t team = ORIGINAL_COUNT; team < MAX_TEAMS; team++) {
                // Where the variants' shapes differ, follow the first one
                const RGB color = fTransparent ? variants[0] : colorFor(variants, team);
                const int variant = firstVariant + static_cast<int>(variantIndex(team, trailingVariants));
                const auto [x, y] = pixelAt(stride * variant + along, across);
                SDL_WriteSurfacePixel(dst.get(), x, y, color.r, color.g, color.b, alpha);
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
