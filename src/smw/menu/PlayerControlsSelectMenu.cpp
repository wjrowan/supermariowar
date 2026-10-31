#include "PlayerControlsSelectMenu.h"

#include "ResourceManager.h"
#include "ui/MI_Button.h"
#include "ui/MI_Image.h"
#include "ui/MI_Text.h"

extern CResourceManager* rm;

UI_PlayerControlsSelectMenu::UI_PlayerControlsSelectMenu()
    : UI_Menu()
{
    static_assert(MENU_CODE_TO_PLAYER_1_CONTROLS + MAX_PLAYERS - 1 <= MENU_CODE_TO_PLAYER_8_CONTROLS);

    // 4 players keep the original spacing, more players start higher to fit above "Back"
    const short iFirstY = MAX_PLAYERS > 4 ? 80 : 140;
    for (short iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++) {
        const std::string label = "Player " + std::to_string(iPlayer + 1);
        miPlayerControlsButtons[iPlayer] = new MI_Button(&rm->spr_selectfield, 120, iFirstY + iPlayer * 40, label, 400, TextAlign::CENTER);
        miPlayerControlsButtons[iPlayer]->SetCode(static_cast<MenuCodeEnum>(MENU_CODE_TO_PLAYER_1_CONTROLS + iPlayer));
    }

    miPlayerControlsBackButton = new MI_Button(&rm->spr_selectfield, 544, 432, "Back", 80, TextAlign::CENTER);
    miPlayerControlsBackButton->SetCode(MENU_CODE_TO_MAIN_MENU);

    miPlayerControlsLeftHeaderBar = new MI_Image(&rm->menu_plain_field, 0, 0, 0, 0, 320, 32, 1, 1, 0);
    miPlayerControlsMenuRightHeaderBar = new MI_Image(&rm->menu_plain_field, 320, 0, 192, 0, 320, 32, 1, 1, 0);
    miPlayerControlsMenuHeaderText = new MI_HeaderText("Player Controls Menu", 320, 5);

    MI_Button* miFirstButton = miPlayerControlsButtons.front();
    MI_Button* miLastButton = miPlayerControlsButtons.back();
    for (short iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++) {
        MI_Button* miUp = iPlayer > 0 ? miPlayerControlsButtons[iPlayer - 1] : miPlayerControlsBackButton;
        MI_Button* miDown = iPlayer < MAX_PLAYERS - 1 ? miPlayerControlsButtons[iPlayer + 1] : miPlayerControlsBackButton;
        AddControl(miPlayerControlsButtons[iPlayer], miUp, miDown, NULL, miPlayerControlsBackButton);
    }
    AddControl(miPlayerControlsBackButton, miLastButton, miFirstButton, miFirstButton, NULL);

    AddNonControl(miPlayerControlsLeftHeaderBar);
    AddNonControl(miPlayerControlsMenuRightHeaderBar);
    AddNonControl(miPlayerControlsMenuHeaderText);

    setInitialFocus(miFirstButton);
    SetCancelCode(MENU_CODE_TO_MAIN_MENU);
}
