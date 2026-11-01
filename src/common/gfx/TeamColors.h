#pragma once

#include "gfx/Color.h"

#include <array>
#include <cstddef>

class gfxSprite;


/// The original art has colors for 4 teams. Until there is art for more, the colors of teams 5-8
/// are generated: art that has a variant per team is compared, the parts that differ between the
/// variants are the team colored ones, and those get repainted with the new team's base color.
namespace team_colors {

/// How many team colors the original art has.
constexpr size_t ORIGINAL_COUNT = 4;

/// The colors of one pixel (or palette entry) in the art of each original team.
using Variants = std::array<RGB, ORIGINAL_COUNT>;

/// The base color that a generated team's art is painted with.
RGB baseColor(size_t team);

/// The color of a pixel for `team`, given its colors in the original teams' art.
/// Original teams get their own color. Pixels that are the same for every original team
/// aren't team colored, and stay unchanged. The others get the hue and saturation of the team's
/// base color, keeping the lightness of the original shade (so dark shades stay dark).
RGB colorFor(const Variants& variants, size_t team);

enum class Axis { X, Y };

/// Extends a sprite sheet that has a variant for each original team, laid out along `axis`,
/// `stride` pixels apart, with a variant for every team up to MAX_TEAMS.
/// `firstVariant` is the number of variants before the first team one (eg. an uncolored one),
/// `trailingVariants` the number of other variants after the last one; the new teams go after those.
/// Sheets that don't have exactly ORIGINAL_COUNT team variants are left unchanged.
/// Returns whether the sheet was extended.
bool extendSheet(gfxSprite& sprite, Axis axis, int stride, int firstVariant = 0, int trailingVariants = 0);

/// The index of a team's variant in a sheet extended with `trailingVariants`, not counting `firstVariant`.
size_t variantIndex(size_t team, int trailingVariants);

} // namespace team_colors
