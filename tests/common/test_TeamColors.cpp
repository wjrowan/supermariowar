#include "doctest.h"

#include "GlobalConstants.h"
#include "gfx/gfxSprite.h"
#include "gfx/TeamColors.h"

#include <SDL3/SDL.h>


namespace {
RGB readPixel(const gfxSprite& sprite, int x, int y)
{
    RGB color {};
    SDL_ReadSurfacePixel(sprite.getSurface(), x, y, &color.r, &color.g, &color.b, nullptr);
    return color;
}

/// A sheet with an uncolored variant and the 4 original team ones, along X
gfxSprite makeSheet(int stride)
{
    const int variants = 1 + static_cast<int>(team_colors::ORIGINAL_COUNT);
    SdlSurfacePtr surface(SDL_CreateSurface(stride * variants, 2, SDL_PIXELFORMAT_XRGB8888));
    const RGB variantColors[] = {{128, 128, 128}, {224, 32, 32}, {32, 224, 32}, {224, 224, 32}, {32, 32, 224}};
    for (int variant = 0; variant < variants; variant++) {
        for (int x = 0; x < stride; x++) {
            const RGB& color = variantColors[variant];
            SDL_WriteSurfacePixel(surface.get(), variant * stride + x, 0, color.r, color.g, color.b, 255);
            SDL_WriteSurfacePixel(surface.get(), variant * stride + x, 1, 255, 0, 255, 255);  // color key
        }
    }
    SDL_SetSurfaceColorKey(surface.get(), true, SDL_MapSurfaceRGB(surface.get(), 255, 0, 255));
    return gfxSprite(std::move(surface), std::nullopt);
}
} // namespace


TEST_CASE("Original team colors are unchanged")
{
    const RGB red {224, 32, 32};
    for (size_t team = 0; team < team_colors::ORIGINAL_COUNT; team++) {
        CHECK(team_colors::sourceTeam(team) == team);
        CHECK(team_colors::colorFor(red, team) == red);
    }
}

TEST_CASE("Generated team colors keep colors without a hue")
{
    for (size_t team = team_colors::ORIGINAL_COUNT; team < MAX_TEAMS; team++) {
        CHECK(team_colors::colorFor(RGB {0, 0, 0}, team) == RGB {0, 0, 0});
        CHECK(team_colors::colorFor(RGB {255, 255, 255}, team) == RGB {255, 255, 255});
        CHECK(team_colors::colorFor(RGB {90, 90, 90}, team) == RGB {90, 90, 90});
    }
}

TEST_CASE("Generated team colors differ from their source team")
{
    const RGB originals[] = {{224, 32, 32}, {32, 224, 32}, {224, 224, 32}, {32, 32, 224}};
    for (size_t team = team_colors::ORIGINAL_COUNT; team < MAX_TEAMS; team++) {
        const size_t source = team_colors::sourceTeam(team);
        REQUIRE(source < team_colors::ORIGINAL_COUNT);
        CHECK(team_colors::colorFor(originals[source], team) != originals[source]);
    }
}

TEST_CASE("Extending a sprite sheet adds a variant for each generated team")
{
    const int stride = 3;
    gfxSprite sheet = makeSheet(stride);

    REQUIRE(team_colors::extendSheet(sheet, team_colors::Axis::X, stride, 1));
    CHECK(sheet.getWidth() == stride * (1 + MAX_TEAMS));
    CHECK(sheet.getHeight() == 2);

    // the uncolored and original variants are copied as they are
    CHECK(readPixel(sheet, 0, 0) == RGB {128, 128, 128});
    CHECK(readPixel(sheet, stride * 1, 0) == RGB {224, 32, 32});
    CHECK(readPixel(sheet, stride * 4, 0) == RGB {32, 32, 224});

    for (size_t team = team_colors::ORIGINAL_COUNT; team < MAX_TEAMS; team++) {
        const int x = stride * (1 + static_cast<int>(team));
        const RGB source = readPixel(sheet, stride * (1 + static_cast<int>(team_colors::sourceTeam(team))), 0);
        CHECK(readPixel(sheet, x, 0) == team_colors::colorFor(source, team));
        // the transparent color stays transparent
        CHECK(readPixel(sheet, x, 1) == RGB {255, 0, 255});
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
