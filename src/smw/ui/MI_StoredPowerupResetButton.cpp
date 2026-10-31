#include "MI_StoredPowerupResetButton.h"

#include "GameValues.h"
#include "ResourceManager.h"

extern CResourceManager* rm;
extern CGameValues game_values;


MI_StoredPowerupResetButton::MI_StoredPowerupResetButton(gfxSprite* nspr, short x, short y, std::string name, short width, TextAlign align) :
    MI_Button(nspr, x, y, std::move(name), width, align)
{}


void MI_StoredPowerupResetButton::Draw()
{
    if (!m_visible)
        return;

    MI_Button::Draw();

    // One slot per player, right-aligned; 4 players use the original 30px spacing
    const short iSpacing = MAX_PLAYERS > 4 ? 26 : 30;
    const short iFirstX = m_pos.x + iWidth - 22 - MAX_PLAYERS * iSpacing;
    for (short iPowerup = 0; iPowerup < MAX_PLAYERS; iPowerup++) {
        rm->spr_selectfield.draw(iFirstX + iPowerup * iSpacing, m_pos.y + 4, {188, 88, 24, 24});
        rm->spr_storedpowerupsmall.draw(iFirstX + 4 + iPowerup * iSpacing, m_pos.y + 8, {game_values.storedpowerups[iPowerup] * 16, 0, 16, 16});
    }
}
