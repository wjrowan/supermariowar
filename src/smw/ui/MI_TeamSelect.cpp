#include "MI_TeamSelect.h"

#include "gfx.h"
#include "Game.h"
#include "GameValues.h"
#include "RandomNumberGenerator.h"
#include "ResourceManager.h"
#include "FileList.h"
#include "ui/MI_Image.h"

extern CGameValues game_values;
extern CResourceManager* rm;

extern SkinList *skinlist;

/**************************************
 * MI_TeamSelect Class
 **************************************/

MI_TeamSelect::MI_TeamSelect(gfxSprite * spr_background_ref, short x, short y) :
    UI_Control(x, y)
{
    spr = spr_background_ref;
    miImage = new MI_Image(spr, m_pos.x, m_pos.y, 0, 0, 416, 256, 1, 1, 0);

    for (short iTeam = 0; iTeam < MAX_TEAMS; iTeam++) {
        iTeamCounts[iTeam] = game_values.teamcounts[iTeam];

        for (short iSlot = 0; iSlot < MAX_TEAM_SIZE; iSlot++)
            iTeamIDs[iTeam][iSlot] = game_values.teamids[iTeam][iSlot];
    }

    for (short iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++)
        fReady[iPlayer] = false;

    iAnimationTimer = 0;
    iAnimationFrame = 0;
    iRandomAnimationFrame = 0;
}

MI_TeamSelect::~MI_TeamSelect()
{
    delete miImage;
}

void MI_TeamSelect::Update()
{
    if (++iAnimationTimer > 7) {
        iAnimationTimer = 0;

        iAnimationFrame += 2;
        if (iAnimationFrame > 2)
            iAnimationFrame = 0;

        iRandomAnimationFrame += 32;
        if (iRandomAnimationFrame >= 128)
            iRandomAnimationFrame = 0;
    }
}

void MI_TeamSelect::Draw()
{
    if (!m_visible)
        return;

    miImage->Draw();

    // Placeholder layout until there is art for 8 teams: 8 narrow columns over the 4 team background,
    // with players packed closer together in big teams
    const short iColumnWidth = 384 / MAX_TEAMS;
    for (short iTeam = 0; iTeam < MAX_TEAMS; iTeam++) {
        const short iColumnX = m_pos.x + 16 + iTeam * iColumnWidth + (iColumnWidth - 32) / 2;
        const short iItemSpacing = iTeamCounts[iTeam] <= 4 ? 36 : 22;

        for (short iTeamItem = 0; iTeamItem < iTeamCounts[iTeam]; iTeamItem++) {
            short iPlayerID = iTeamIDs[iTeam][iTeamItem];
            const short iItemY = m_pos.y + 28 + iTeamItem * iItemSpacing;

            if (game_values.randomskin[iPlayerID])
                spr->draw(iColumnX - 5, iItemY, {416, fReady[iPlayerID] ? 0 : iRandomAnimationFrame, 42, 32});
            else
                rm->spr_player[iPlayerID][fReady[iPlayerID] ? 0 : iAnimationFrame].draw(iColumnX, iItemY, {0, 0, 32, 32});

            rm->spr_menu_boxed_numbers.draw(iColumnX - 4, iItemY + 20, {iPlayerID * 16, game_values.colorids[iPlayerID] * 16, 16, 16});
        }
    }

    // A narrower ready panel per player, made from the edges of the original
    const short iPanelWidth = App::screenWidth / MAX_PLAYERS;
    for (short iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++) {
        if (game_values.playercontrol[iPlayer] == 0)
            continue;

        const short iPanelX = iPlayer * iPanelWidth;
        rm->spr_player_select_ready.draw(iPanelX, 368, {0, 0, iPanelWidth / 2, 96});
        rm->spr_player_select_ready.draw(iPanelX + iPanelWidth / 2, 368, {128 - iPanelWidth / 2, 0, iPanelWidth / 2, 96});

        rm->spr_menu_boxed_numbers.draw(iPanelX + 8, 388, {iPlayer * 16, game_values.colorids[iPlayer] * 16, 16, 16});
        rm->menu_font_small.drawChopRight(iPanelX + 26, 404 - rm->menu_font_small.getHeight(), iPanelWidth - 34, game_values.randomskin[iPlayer] ? "Random" : skinlist->at(game_values.skinids[iPlayer]).name.c_str());

        rm->spr_player_select_ready.draw(iPanelX + (iPanelWidth - 34) / 2, 408, {128, (!fReady[iPlayer] ? 0 : (game_values.playercontrol[iPlayer] == 1 ? 32 : 64)), 34, 32});
    }

    if (fAllReady) {
        rm->menu_plain_field.draw(m_pos.x + 108, m_pos.y + App::screenHeight * 0.47f, {0, 160, 100, 32});
        rm->menu_plain_field.draw(m_pos.x + 208, m_pos.y + App::screenHeight * 0.47f, {412, 160, 100, 32});
        rm->menu_font_large.drawCentered(App::screenWidth/2, m_pos.y + App::screenHeight * 0.48f, "Continue");
    }
}

MenuCodeEnum MI_TeamSelect::SendInput(CPlayerInput * playerInput)
{
    for (short iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++) {
        COutputControl * playerKeys = &game_values.playerInput.outputControls[iPlayer];

        if (game_values.playercontrol[iPlayer] > 0 && !fReady[iPlayer]) { //if this player is player or cpu
            if (playerKeys->menu_left.fPressed) {
                if (playerKeys->menu_right.fDown)
                    game_values.randomskin[iPlayer] = !game_values.randomskin[iPlayer];
                else
                    FindNewTeam(iPlayer, -1);
            }

            if (playerKeys->menu_right.fPressed) {
                if (playerKeys->menu_left.fDown)
                    game_values.randomskin[iPlayer] = !game_values.randomskin[iPlayer];
                else
                    FindNewTeam(iPlayer, 1);
            }

            //Scroll up/down through player skins
            if (!game_values.randomskin[iPlayer]) {
                if (playerKeys->menu_up.fPressed) {
                    do {
                        if (playerKeys->menu_down.fDown) {
                            game_values.skinids[iPlayer] = RANDOM_INT(skinlist->count());
                        } else {
                            if (--game_values.skinids[iPlayer] < 0)
                                game_values.skinids[iPlayer] = (short)skinlist->count() - 1;
                        }
                    } while (!rm->LoadMenuSkin(iPlayer, game_values.skinids[iPlayer], game_values.colorids[iPlayer], false));
                } else if (playerKeys->menu_up.fDown) {
                    if (iFastScroll[iPlayer] == 0) {
                        if (++iFastScrollTimer[iPlayer] > 40) {
                            iFastScroll[iPlayer] = 1;
                        }
                    } else {
                        if (++iFastScrollTimer[iPlayer] > 5) {
                            do {
                                if (--game_values.skinids[iPlayer] < 0)
                                    game_values.skinids[iPlayer] = (short)skinlist->count() - 1;
                            } while (!rm->LoadMenuSkin(iPlayer, game_values.skinids[iPlayer], game_values.colorids[iPlayer], false));

                            iFastScrollTimer[iPlayer] = 0;
                        }
                    }
                }

                if (playerKeys->menu_down.fPressed) {
                    do {
                        if (playerKeys->menu_up.fDown) {
                            game_values.skinids[iPlayer] = RANDOM_INT( skinlist->count());
                        } else {
                            if (++game_values.skinids[iPlayer] >= static_cast<short>(skinlist->count()))
                                game_values.skinids[iPlayer] = 0;
                        }
                    } while (!rm->LoadMenuSkin(iPlayer, game_values.skinids[iPlayer], game_values.colorids[iPlayer], false));
                } else if (playerKeys->menu_down.fDown) {
                    if (iFastScroll[iPlayer] == 0) {
                        if (++iFastScrollTimer[iPlayer] > 40) {
                            iFastScroll[iPlayer] = 1;
                        }
                    } else {
                        if (++iFastScrollTimer[iPlayer] > 5) {
                            do {
                                if (++game_values.skinids[iPlayer] >= static_cast<short>(skinlist->count()))
                                    game_values.skinids[iPlayer] = 0;
                            } while (!rm->LoadMenuSkin(iPlayer, game_values.skinids[iPlayer], game_values.colorids[iPlayer], false));

                            iFastScrollTimer[iPlayer] = 0;
                        }
                    }
                }

                if ((!playerKeys->menu_up.fDown && !playerKeys->menu_down.fDown) ||
                        (playerKeys->menu_up.fDown && playerKeys->menu_down.fDown)) {
                    iFastScroll[iPlayer] = 0;
                    iFastScrollTimer[iPlayer] = 0;
                }
            } else {
                iFastScroll[iPlayer] = 0;
                iFastScrollTimer[iPlayer] = 0;
            }

            if (playerKeys->menu_random.fPressed) {
                if (playerKeys->menu_scrollfast.fDown) {
                    game_values.randomskin[iPlayer] = !game_values.randomskin[iPlayer];
                } else if (!game_values.randomskin[iPlayer]) {
                    do {
                        game_values.skinids[iPlayer] = RANDOM_INT(skinlist->count());
                    } while (!rm->LoadMenuSkin(iPlayer, game_values.skinids[iPlayer], game_values.colorids[iPlayer], false));
                }
            }
        }

        if (playerInput->outputControls[iPlayer].menu_select.fPressed) {
            fReady[iPlayer] = true;

            if (fAllReady && (DEVICE_KEYBOARD != playerInput->inputControls[iPlayer]->iDevice || iPlayer == 0)) {
                fModifying = false;
                printf("MI_TeamSelect::SendInput MENU_CODE_TO_GAME_SETUP_MENU\n");
                return MENU_CODE_TO_GAME_SETUP_MENU;
            }

            fAllReady = true;
            for (short i = 0; i < MAX_PLAYERS; i++) {
                if (!fReady[i]) {
                    fAllReady = false;
                    break;
                }
            }
        }

        if (playerInput->outputControls[iPlayer].menu_cancel.fPressed) {
            if (game_values.playercontrol[iPlayer] > 0 && fReady[iPlayer]) {
                fReady[iPlayer] = false;
                fAllReady = false;
            } else {
                if (DEVICE_KEYBOARD != playerInput->inputControls[iPlayer]->iDevice || iPlayer == 0) {
                    fModifying = false;
                    return MENU_CODE_BACK_TO_MATCH_SELECTION_MENU;
                }
            }
        }
        if (DEVICE_KEYBOARD != playerInput->inputControls[iPlayer]->iDevice) {
            short team = GetTeam(iPlayer);
            if (SDL_Joystick* joystick = GetJoystick(playerInput->inputControls[iPlayer]->iDevice))
                gfx_setjoystickteamcolor(joystick, team, fReady[iPlayer] ? 1.0 : 0.5);
        }
    }

    return MENU_CODE_NONE;
}

MenuCodeEnum MI_TeamSelect::Modify(bool modify)
{
    fModifying = modify;
    return MENU_CODE_MODIFY_ACCEPTED;
}

void MI_TeamSelect::FindNewTeam(short iPlayerID, short iDirection)
{
    for (short iTeam = 0; iTeam < MAX_TEAMS; iTeam++) {
        for (short iTeamItem = 0; iTeamItem < iTeamCounts[iTeam]; iTeamItem++) {
            if (iTeamIDs[iTeam][iTeamItem] == iPlayerID) {
                iTeamCounts[iTeam]--;

                for (int iMovePlayer = iTeamItem; iMovePlayer < iTeamCounts[iTeam]; iMovePlayer++)
                    iTeamIDs[iTeam][iMovePlayer] = iTeamIDs[iTeam][iMovePlayer + 1];

                short iNewTeam = iTeam;
                bool fOnlyTeam = true;

                do {
                    iNewTeam += iDirection;

                    if (iNewTeam < 0)
                        iNewTeam = MAX_TEAMS - 1;
                    else if (iNewTeam >= MAX_TEAMS)
                        iNewTeam = 0;

                    fOnlyTeam = true;
                    for (int iMovePlayer = 0; iMovePlayer < MAX_TEAMS; iMovePlayer++) {
                        if (iMovePlayer == iNewTeam)
                            continue;

                        if (iTeamCounts[iMovePlayer] > 0) {
                            fOnlyTeam = false;
                            break;
                        }
                    }
                } while (fOnlyTeam);

                iTeamIDs[iNewTeam][iTeamCounts[iNewTeam]] = iPlayerID;
                iTeamCounts[iNewTeam]++;

                if (game_values.teamcolors) {
                    game_values.colorids[iPlayerID] = iNewTeam;

                    //Skip skins that are invalid
                    while (!rm->LoadMenuSkin(iPlayerID, game_values.skinids[iPlayerID], iNewTeam, false)) {
                        if (++game_values.skinids[iPlayerID] >= static_cast<short>(skinlist->count()))
                            game_values.skinids[iPlayerID] = 0;
                    }
                }

                return;
            }
        }
    }
}

void MI_TeamSelect::Reset()
{
    for (short iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++) {
        short iTeamID;
        short iSlotID;
        bool fFound = false;
        for (iTeamID = 0; iTeamID < MAX_TEAMS; iTeamID++) {
            for (iSlotID = 0; iSlotID < iTeamCounts[iTeamID]; iSlotID++) {
                if (iTeamIDs[iTeamID][iSlotID] == iPlayer) {
                    fFound = true;
                    break;
                }
            }

            if (fFound)
                break;
        }

        if (fFound) {
            //Need to remove the player
            if (game_values.playercontrol[iPlayer] == 0) {
                iTeamCounts[iTeamID]--;

                if (iTeamCounts[iTeamID] > iSlotID) {
                    for (short iSlot = iSlotID; iSlot < iTeamCounts[iTeamID]; iSlot++) {
                        iTeamIDs[iTeamID][iSlot] = iTeamIDs[iTeamID][iSlot + 1];
                    }
                }
            }
        } else {
            //A new player was added so find a spot for him
            if (game_values.playercontrol[iPlayer] > 0) {
                short iLookForNewTeam = iPlayer;

                while (iTeamCounts[iLookForNewTeam] >= MAX_TEAM_SIZE) {
                    if (++iLookForNewTeam >= MAX_TEAMS)
                        iLookForNewTeam = 0;
                }

                iTeamIDs[iLookForNewTeam][iTeamCounts[iLookForNewTeam]] = iPlayer;
                iTeamCounts[iLookForNewTeam]++;

                if (game_values.teamcolors)
                    game_values.colorids[iPlayer] = iLookForNewTeam;
            }
        }
    }

    //Check to see if there is only one team and if so, split them up

    short iCountTeams = 0;
    short iLastTeam = 0;
    for (short iTeamID = 0; iTeamID < MAX_TEAMS; iTeamID++) {
        if (iTeamCounts[iTeamID] > 0) {
            iCountTeams++;
            iLastTeam = iTeamID;
        }
    }

    if (iCountTeams == 1) {
        short iLookForNewTeam = iLastTeam;
        if (++iLookForNewTeam >= MAX_TEAMS)
            iLookForNewTeam = 0;

        iTeamCounts[iLastTeam]--;
        short iPlayer = iTeamIDs[iLastTeam][iTeamCounts[iLastTeam]];
        iTeamIDs[iLookForNewTeam][iTeamCounts[iLookForNewTeam]] = iPlayer;
        iTeamCounts[iLookForNewTeam]++;

        if (game_values.teamcolors)
            game_values.colorids[iPlayer] = iLookForNewTeam;
    }

    iAnimationTimer = 0;
    iAnimationFrame = 0;

    fAllReady = true;

    for (short iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++) {
        iFastScroll[iPlayer] = 0;
        iFastScrollTimer[iPlayer] = 0;

        if (game_values.playercontrol[iPlayer] == 1) {
            fReady[iPlayer] = false;
            fAllReady = false;
        } else {
            fReady[iPlayer] = true;
        }

        //If debug build, then ignore the player ready stuff
#ifdef _DEBUG
        fReady[iPlayer] = true;
        fAllReady = true;
#endif

        if (game_values.playercontrol[iPlayer] == 0)
            continue;

        if (game_values.teamcolors)
            game_values.colorids[iPlayer] = GetTeam(iPlayer);
        else
            game_values.colorids[iPlayer] = iPlayer;

        //Skip skins that are invalid
        //TODO: this loops forever in emscripten build
        while (!rm->LoadMenuSkin(iPlayer, game_values.skinids[iPlayer], game_values.colorids[iPlayer], false)) {
            if (++game_values.skinids[iPlayer] >= static_cast<short>(skinlist->count()))
                game_values.skinids[iPlayer] = 0;
        }
    }
}

short MI_TeamSelect::OrganizeTeams()
{
    iNumTeams = 0;
    for (short iTeam = 0; iTeam < MAX_TEAMS; iTeam++) {
        game_values.teamcounts[iTeam] = 0;

        if (iTeamCounts[iTeam] > 0) {
            for (short iTeamSpot = 0; iTeamSpot < MAX_TEAM_SIZE; iTeamSpot++)
                game_values.teamids[iNumTeams][iTeamSpot] = iTeamIDs[iTeam][iTeamSpot];

            game_values.teamcounts[iNumTeams] = iTeamCounts[iTeam];
            iNumTeams++;
        }
    }

    return iNumTeams;
}

short MI_TeamSelect::GetTeam(short iPlayerID)
{
    for (short iTeam = 0; iTeam < MAX_TEAMS; iTeam++) {
        for (short iSlot = 0; iSlot < iTeamCounts[iTeam]; iSlot++) {
            if (iTeamIDs[iTeam][iSlot] == iPlayerID) {
                return iTeam;
            }
        }
    }

    return -1;
}
