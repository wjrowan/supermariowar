#include "doctest.h"

#include "GlobalConstants.h"
#include "gfx/gfxSprite.h"
#include "gfx/TeamColors.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>


namespace {
RGB readPixel(const gfxSprite& sprite, int x, int y)
{
    RGB color {};
    SDL_ReadSurfacePixel(sprite.getSurface(), x, y, &color.r, &color.g, &color.b, nullptr);
    return color;
}

float hueOf(RGB color)
{
    const float r = color.r / 255.f, g = color.g / 255.f, b = color.b / 255.f;
    const float max = std::max({r, g, b}), min = std::min({r, g, b});
    const float chroma = max - min;
    float hue = max == r ? 60.f * std::fmod((g - b) / chroma, 6.f)
        : max == g ? 60.f * ((b - r) / chroma + 2.f)
        : 60.f * ((r - g) / chroma + 4.f);
    return hue < 0.f ? hue + 360.f : hue;
}

float lightnessOf(RGB color)
{
    return (std::max({color.r, color.g, color.b}) + std::min({color.r, color.g, color.b})) / 510.f;
}

float hueDistance(float a, float b)
{
    const float distance = std::fabs(a - b);
    return std::min(distance, 360.f - distance);
}

// A light and a dark team shade, as red, green, yellow and blue
const team_colors::Variants LIGHT_SHADE {{{248, 96, 96}, {96, 248, 96}, {248, 248, 96}, {96, 96, 248}}};
const team_colors::Variants DARK_SHADE {{{128, 0, 0}, {0, 128, 0}, {128, 128, 0}, {0, 0, 128}}};
// A pixel that's the same for every team, eg. a wooden hammer handle
const RGB SHARED {150, 90, 30};
const RGB KEY {255, 0, 255};

/// A sheet with an uncolored variant, then the 4 original team ones, along X.
/// Rows: a light team shade, a dark team shade, a shared pixel, a transparent pixel.
gfxSprite makeSheet(int stride)
{
    const int variants = 1 + static_cast<int>(team_colors::ORIGINAL_COUNT);
    SdlSurfacePtr surface(SDL_CreateSurface(stride * variants, 4, SDL_PIXELFORMAT_XRGB8888));
    for (int variant = 0; variant < variants; variant++) {
        const RGB light = variant == 0 ? RGB {200, 200, 200} : LIGHT_SHADE[variant - 1];
        const RGB dark = variant == 0 ? RGB {60, 60, 60} : DARK_SHADE[variant - 1];
        for (int x = variant * stride; x < (variant + 1) * stride; x++) {
            SDL_WriteSurfacePixel(surface.get(), x, 0, light.r, light.g, light.b, 255);
            SDL_WriteSurfacePixel(surface.get(), x, 1, dark.r, dark.g, dark.b, 255);
            SDL_WriteSurfacePixel(surface.get(), x, 2, SHARED.r, SHARED.g, SHARED.b, 255);
            SDL_WriteSurfacePixel(surface.get(), x, 3, KEY.r, KEY.g, KEY.b, 255);
        }
    }
    SDL_SetSurfaceColorKey(surface.get(), true, SDL_MapSurfaceRGB(surface.get(), KEY.r, KEY.g, KEY.b));
    return gfxSprite(std::move(surface), std::nullopt);
}
} // namespace


TEST_CASE("Original teams keep their own colors")
{
    for (size_t team = 0; team < team_colors::ORIGINAL_COUNT; team++)
        CHECK(team_colors::colorFor(LIGHT_SHADE, team) == LIGHT_SHADE[team]);
}

TEST_CASE("Pixels that are the same for every team aren't recolored")
{
    const team_colors::Variants shared {{SHARED, SHARED, SHARED, SHARED}};
    for (size_t team = team_colors::ORIGINAL_COUNT; team < MAX_TEAMS; team++)
        CHECK(team_colors::colorFor(shared, team) == SHARED);
}

TEST_CASE("Team colored pixels get the base color's hue and keep their shade")
{
    for (size_t team = team_colors::ORIGINAL_COUNT; team < MAX_TEAMS; team++) {
        const RGB base = team_colors::baseColor(team);
        const RGB light = team_colors::colorFor(LIGHT_SHADE, team);
        const RGB dark = team_colors::colorFor(DARK_SHADE, team);

        if (base.r == base.g && base.g == base.b) {
            // a gray base (eg. white) gives gray shades
            CHECK((light.r == light.g && light.g == light.b));
            CHECK((dark.r == dark.g && dark.g == dark.b));
        } else {
            CHECK(hueDistance(hueOf(light), hueOf(base)) < 3.f);
            CHECK(hueDistance(hueOf(dark), hueOf(base)) < 3.f);
        }
        CHECK(lightnessOf(dark) < lightnessOf(light));
    }
}

TEST_CASE("Generated teams have different colors")
{
    for (size_t team = team_colors::ORIGINAL_COUNT; team < MAX_TEAMS; team++) {
        for (size_t other = 0; other < team; other++)
            CHECK(team_colors::colorFor(LIGHT_SHADE, team) != team_colors::colorFor(LIGHT_SHADE, other));
    }
}

TEST_CASE("Extending a sprite sheet adds a variant for each generated team")
{
    const int stride = 3;
    gfxSprite sheet = makeSheet(stride);

    REQUIRE(team_colors::extendSheet(sheet, team_colors::Axis::X, stride, 1));
    CHECK(sheet.getWidth() == stride * (1 + MAX_TEAMS));
    CHECK(sheet.getHeight() == 4);

    // the uncolored and original variants are copied as they are
    CHECK(readPixel(sheet, 0, 0) == RGB {200, 200, 200});
    CHECK(readPixel(sheet, stride * 1, 0) == LIGHT_SHADE[0]);
    CHECK(readPixel(sheet, stride * 4, 1) == DARK_SHADE[3]);

    for (size_t team = team_colors::ORIGINAL_COUNT; team < MAX_TEAMS; team++) {
        const int x = stride * (1 + static_cast<int>(team));
        CHECK(readPixel(sheet, x, 0) == team_colors::colorFor(LIGHT_SHADE, team));
        CHECK(readPixel(sheet, x, 1) == team_colors::colorFor(DARK_SHADE, team));
        CHECK(readPixel(sheet, x, 2) == SHARED);
        CHECK(readPixel(sheet, x, 3) == KEY);
    }

    Uint32 key = 0;
    CHECK(SDL_GetSurfaceColorKey(sheet.getSurface(), &key));
}

TEST_CASE("Sprite sheets with an unknown layout are left unchanged")
{
    gfxSprite sheet = makeSheet(3);
    // 5 variants of 3 pixels don't match 4 team variants of 4 pixels
    CHECK_FALSE(team_colors::extendSheet(sheet, team_colors::Axis::X, 4));
    CHECK(sheet.getWidth() == 15);
}

TEST_CASE("Generated team variants go after the trailing variants")
{
    // 4 team variants followed by one other (eg. the gray "all teams" announcement icon)
    const int stride = 2;
    const RGB gray {128, 128, 128};
    SdlSurfacePtr surface(SDL_CreateSurface(stride * 5, 1, SDL_PIXELFORMAT_XRGB8888));
    for (int variant = 0; variant < 5; variant++) {
        const RGB color = variant < 4 ? LIGHT_SHADE[variant] : gray;
        for (int x = 0; x < stride; x++)
            SDL_WriteSurfacePixel(surface.get(), variant * stride + x, 0, color.r, color.g, color.b, 255);
    }
    gfxSprite sheet(std::move(surface), std::nullopt);

    REQUIRE(team_colors::extendSheet(sheet, team_colors::Axis::X, stride, 0, 1));
    CHECK(sheet.getWidth() == stride * (MAX_TEAMS + 1));

    // the trailing variant stays where it was
    CHECK(readPixel(sheet, stride * 4, 0) == gray);
    for (size_t team = 0; team < MAX_TEAMS; team++) {
        const size_t index = team_colors::variantIndex(team, 1);
        CHECK(index == (team < 4 ? team : team + 1));
        CHECK(readPixel(sheet, stride * static_cast<int>(index), 0) == team_colors::colorFor(LIGHT_SHADE, team));
    }
}

TEST_CASE("Tinting paints a single color with the team's base color")
{
    const RGB red {237, 66, 80};
    for (size_t team = 0; team < team_colors::ORIGINAL_COUNT; team++)
        CHECK(team_colors::tint(red, team) == red);

    for (size_t team = team_colors::ORIGINAL_COUNT; team < MAX_TEAMS; team++) {
        // black, white and gray have no team color
        CHECK(team_colors::tint(RGB {0, 0, 0}, team) == RGB {0, 0, 0});
        CHECK(team_colors::tint(RGB {255, 255, 255}, team) == RGB {255, 255, 255});
        CHECK(team_colors::tint(RGB {104, 136, 120}, team) != RGB {104, 136, 120});

        const RGB base = team_colors::baseColor(team);
        const RGB tinted = team_colors::tint(red, team);
        if (base.r != base.g || base.g != base.b)
            CHECK(hueDistance(hueOf(tinted), hueOf(base)) < 3.f);
    }
}
