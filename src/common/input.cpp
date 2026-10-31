#include "input.h"

#include "GameValues.h"
#include "GlobalConstants.h"

#include <algorithm>
#include <cstdio>
#include <vector>

extern CGameValues game_values;
extern std::vector<SDL_Joystick*> joysticks;
extern int joystickcount;

namespace {
// Intentional cast from -1 as a keyboard sentinel
constexpr SDL_JoystickID KEYBOARD_DEVICE_ID = static_cast<SDL_JoystickID>(DEVICE_KEYBOARD);

// Joystick events carry an SDL instance ID, but player devices are indexes into `joysticks`
SDL_JoystickID deviceIndexFromJoystickID(SDL_JoystickID id)
{
    for (int i = 0; i < joystickcount; i++) {
        if (joysticks[i] && SDL_GetJoystickID(joysticks[i]) == id)
            return static_cast<SDL_JoystickID>(i);
    }
    return KEYBOARD_DEVICE_ID;
}
} // namespace

short OpenJoystick(SDL_JoystickID id)
{
    if (deviceIndexFromJoystickID(id) != KEYBOARD_DEVICE_ID)
        return -1;  // SDL also reports the joysticks found at startup as added

    SDL_Joystick* joystick = SDL_OpenJoystick(id);
    if (!joystick) {
        printf("[input] Couldn't open joystick %u: %s\n", static_cast<unsigned>(id), SDL_GetError());
        return -1;
    }

    // Reuse the first disconnected slot, so a joystick that reconnects gets its old device index back
    auto slot = std::find(joysticks.begin(), joysticks.end(), nullptr);
    if (slot == joysticks.end())
        slot = joysticks.insert(slot, nullptr);
    *slot = joystick;
    joystickcount = static_cast<int>(joysticks.size());

    const short device = static_cast<short>(slot - joysticks.begin());
    printf("[input] Joystick %d connected: %s\n", device, SDL_GetJoystickName(joystick));
    return device;
}

void CloseJoystick(SDL_JoystickID id)
{
    for (size_t device = 0; device < joysticks.size(); device++) {
        if (joysticks[device] && SDL_GetJoystickID(joysticks[device]) == id) {
            printf("[input] Joystick %d disconnected\n", static_cast<int>(device));
            SDL_CloseJoystick(joysticks[device]);
            joysticks[device] = nullptr;
        }
    }
}

SDL_Joystick* GetJoystick(short device)
{
    return device >= 0 && device < joystickcount ? joysticks[device] : nullptr;
}

CPlayerInput::CPlayerInput()
{
    for (int iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++) {
        for (int iKey = 0; iKey < NUM_KEYS; iKey++) {
			outputControls[iPlayer].keys[iKey].fPressed = false;
			outputControls[iPlayer].keys[iKey].fDown = false;
		}
	}

    iPressedKey = 0;
}

//Pass in 0 for game and 1 for menu
//Clear old button pushed states
void CPlayerInput::ClearPressedKeys(short iGameState)
{
    for (int iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++) {
        COutputControl* outputControl = &outputControls[iPlayer];
        for (int iKey = 0; iKey < NUM_KEYS; iKey++) {
            outputControl->keys[iKey].fPressed = false;
        }
    }

    iPressedKey = 0;
}

void CPlayerInput::ClearGameActionKeys()
{
    for (int iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++) {
		COutputControl * outputControl = &outputControls[iPlayer];

		// 0-3: direction keys, 4: turbo
        for (int iKey = 5; iKey < NUM_KEYS; iKey++) {
			outputControl->keys[iKey].fPressed = false;
			outputControl->keys[iKey].fDown = false;
		}
	}

	iPressedKey = 0;
}

//Clear all button pushed and down states
//Call this when switching from menu to game
void CPlayerInput::ResetKeys()
{
    for (int iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++) {
        for (int iKey = 0; iKey < NUM_KEYS; iKey++) {
			outputControls[iPlayer].keys[iKey].fPressed = false;
			outputControls[iPlayer].keys[iKey].fDown = false;
		}
	}

	iPressedKey = 0;
}

//Called during game loop to read input events and see if
//configured keys were pressed.  If they were, then turn on
//key flags to be used by game logic
//iGameState == 0 for in game and 1 for menu
void CPlayerInput::Update(SDL_Event event, short iGameState)
{
    if (event.type == SDL_EVENT_JOYSTICK_ADDED) {
        const short device = OpenJoystick(event.jdevice.which);
        if (device >= 0)
            AssignJoystick(device);
        return;
    }
    if (event.type == SDL_EVENT_JOYSTICK_REMOVED) {
        CloseJoystick(event.jdevice.which);
        return;
    }

	bool fFound = false;
    for (short iPlayer = -1; iPlayer < MAX_PLAYERS; iPlayer++) {
		CInputControl * inputControl;
		COutputControl * outputControl;
		SDL_JoystickID iDeviceID = KEYBOARD_DEVICE_ID;

		//Allow keyboard input from player 1 at all times (even when he is configured to use joystick)
        if (iPlayer == -1) {
            if (iGameState == 1 && inputControls[0]->iDevice != DEVICE_KEYBOARD) {
				inputControl = &game_values.inputConfiguration[0][0].inputGameControls[1];
				outputControl = &outputControls[0];
				iDeviceID = game_values.inputConfiguration[0][0].iDevice;
            } else {
				continue;
			}
        } else {
			if (!inputControls[iPlayer])
				continue;

			inputControl = &inputControls[iPlayer]->inputGameControls[iGameState];
			outputControl = &outputControls[iPlayer];
			iDeviceID = inputControls[iPlayer]->iDevice;
		}

        if (iDeviceID == KEYBOARD_DEVICE_ID) {
            if (SDL_EVENT_KEY_DOWN == event.type) {
                for (int iKey = 0; iKey < NUM_KEYS && !fFound; iKey++) {
                    if (inputControl->keys[iKey] == event.key.key) {
						fFound = true;

						//Ignore input for cpu controlled players
						if (iGameState == 0 && game_values.playercontrol[iPlayer] != 1 && iKey < 6)
							continue;

						if (!outputControl->keys[iKey].fDown)
							outputControl->keys[iKey].fPressed = true;

						outputControl->keys[iKey].fDown = true;
					}
				}

				iPressedKey = event.key.key;
            } else if (SDL_EVENT_KEY_UP == event.type) {
                for (int iKey = 0; iKey < NUM_KEYS && !fFound; iKey++) {
                    if (inputControl->keys[iKey] == event.key.key) {
						fFound = true;

						//Ignore input for cpu controlled players
						if (iGameState == 0 && game_values.playercontrol[iPlayer] != 1 && iKey < 6)
							continue;

						outputControl->keys[iKey].fDown = false;
					}
				}
            } else if (SDL_EVENT_MOUSE_MOTION == event.type) {
                for (int iKey = 0; iKey < NUM_KEYS && !fFound; iKey++) {
                    if (inputControl->keys[iKey] >= MOUSE_UP) {
						if ((inputControl->keys[iKey] == MOUSE_UP && event.motion.yrel < -MOUSE_Y_DEAD_ZONE) ||
							(inputControl->keys[iKey] == MOUSE_DOWN && event.motion.yrel > MOUSE_Y_DEAD_ZONE) ||
							(inputControl->keys[iKey] == MOUSE_LEFT && event.motion.xrel < -MOUSE_X_DEAD_ZONE) ||
							(inputControl->keys[iKey] == MOUSE_RIGHT && event.motion.xrel > MOUSE_X_DEAD_ZONE) ||
                                (inputControl->keys[iKey] >= MOUSE_BUTTON_START && (event.motion.state & SDL_BUTTON_MASK(inputControl->keys[iKey] - MOUSE_BUTTON_START)))) {
							fFound = true;

							//Ignore input for cpu controlled players
							if (iGameState == 0 && game_values.playercontrol[iPlayer] != 1 && iKey < 6)
								continue;

							if (!outputControl->keys[iKey].fDown)
								outputControl->keys[iKey].fPressed = true;

							outputControl->keys[iKey].fDown = true;
                        } else {
							//Ignore input for cpu controlled players
							if (iGameState == 0 && game_values.playercontrol[iPlayer] != 1 && iKey < 6)
								continue;

							//Mouse scroll wheel up/down events happen on same frame so ignore up event (and clear it in the ClearPressedKeys() method)
							if (inputControl->keys[iKey] == MOUSE_BUTTON_START + 4 || inputControl->keys[iKey] == MOUSE_BUTTON_START + 5)
								continue;

							outputControl->keys[iKey].fDown = false;
						}
					}
				}
            } else if (SDL_EVENT_MOUSE_BUTTON_DOWN == event.type) {
                for (int iKey = 0; iKey < NUM_KEYS && !fFound; iKey++) {
                    if (inputControl->keys[iKey] == event.button.button + MOUSE_BUTTON_START) {
						fFound = true;

						//Ignore input for cpu controlled players
						if (iGameState == 0 && game_values.playercontrol[iPlayer] != 1 && iKey < 6)
							continue;

						if (!outputControl->keys[iKey].fDown)
							outputControl->keys[iKey].fPressed = true;

						outputControl->keys[iKey].fDown = true;
					}
				}
            } else if (SDL_EVENT_MOUSE_BUTTON_UP == event.type) {
                for (int iKey = 0; iKey < NUM_KEYS && !fFound; iKey++) {
                    if (inputControl->keys[iKey] == event.button.button + MOUSE_BUTTON_START) {
						fFound = true;

						//Mouse scroll wheel up/down events happen on same frame so ignore up event (and clear it in the ClearPressedKeys() method)
						if (inputControl->keys[iKey] == MOUSE_BUTTON_START + 4 || inputControl->keys[iKey] == MOUSE_BUTTON_START + 5)
							continue;

						//Ignore input for cpu controlled players
						if (iGameState == 0 && game_values.playercontrol[iPlayer] != 1 && iKey < 6)
							continue;

						outputControl->keys[iKey].fDown = false;
					}
				}
			}
        } else {
            if (SDL_EVENT_JOYSTICK_HAT_MOTION == event.type) {
				if (iDeviceID != deviceIndexFromJoystickID(event.jhat.which))
					continue;

                for (int iKey = 0; iKey < NUM_KEYS; iKey++) {
                    if (inputControl->keys[iKey] >= JOY_HAT_UP && inputControl->keys[iKey] <= JOY_HAT_RIGHT) {
						if ((inputControl->keys[iKey] == JOY_HAT_UP && (event.jhat.value & SDL_HAT_UP)) ||
							(inputControl->keys[iKey] == JOY_HAT_DOWN && (event.jhat.value & SDL_HAT_DOWN)) ||
							(inputControl->keys[iKey] == JOY_HAT_LEFT && (event.jhat.value & SDL_HAT_LEFT)) ||
                                (inputControl->keys[iKey] == JOY_HAT_RIGHT && (event.jhat.value & SDL_HAT_RIGHT))) {
							fFound = true;

							//Ignore input for cpu controlled players
							if (iGameState == 0 && game_values.playercontrol[iPlayer] != 1 && iKey < 6)
								continue;

							if (!outputControl->keys[iKey].fDown)
								outputControl->keys[iKey].fPressed = true;

							outputControl->keys[iKey].fDown = true;
                        } else {
							//Ignore input for cpu controlled players
							if (iGameState == 0 && game_values.playercontrol[iPlayer] != 1 && iKey < 6)
								continue;

							outputControl->keys[iKey].fDown = false;
						}
					}
				}
            } else if (SDL_EVENT_JOYSTICK_BUTTON_DOWN == event.type) {
				if (iDeviceID != deviceIndexFromJoystickID(event.jbutton.which))
					continue;

                for (int iKey = 0; iKey < NUM_KEYS && !fFound; iKey++) {
                    if (inputControl->keys[iKey] == event.jbutton.button + JOY_BUTTON_START) {
						fFound = true;

						//Ignore input for cpu controlled players
						if (iGameState == 0 && game_values.playercontrol[iPlayer] != 1 && iKey < 6)
							continue;

						if (!outputControl->keys[iKey].fDown)
							outputControl->keys[iKey].fPressed = true;

						outputControl->keys[iKey].fDown = true;
					}
				}
            } else if (SDL_EVENT_JOYSTICK_BUTTON_UP == event.type) {
				if (iDeviceID != deviceIndexFromJoystickID(event.jbutton.which))
					continue;

                for (int iKey = 0; iKey < NUM_KEYS && !fFound; iKey++) {
                    if (inputControl->keys[iKey] == event.jbutton.button + JOY_BUTTON_START) {
						fFound = true;

						//Ignore input for cpu controlled players
						if (iGameState == 0 && game_values.playercontrol[iPlayer] != 1 && iKey < 6)
							continue;

						outputControl->keys[iKey].fDown = false;
					}
				}
            } else if (SDL_EVENT_JOYSTICK_AXIS_MOTION == event.type) {
				if (iDeviceID != deviceIndexFromJoystickID(event.jaxis.which))
					continue;

                for (int iKey = 0; iKey < NUM_KEYS; iKey++) {
					bool fUseJoystickInput = false;
					bool fJoystickDown = false;

                    if (event.jaxis.axis == 0 && inputControl->keys[iKey] == JOY_STICK_1_LEFT) {
						fUseJoystickInput = true;

						if (event.jaxis.value < -JOYSTICK_DEAD_ZONE)
							fJoystickDown = true;
                    } else if (event.jaxis.axis == 0 && inputControl->keys[iKey] == JOY_STICK_1_RIGHT) {
						fUseJoystickInput = true;

						if (event.jaxis.value > JOYSTICK_DEAD_ZONE)
							fJoystickDown = true;
                    } else if (event.jaxis.axis == 1 && inputControl->keys[iKey] == JOY_STICK_1_UP) {
						fUseJoystickInput = true;

						if (event.jaxis.value < -JOYSTICK_DEAD_ZONE)
							fJoystickDown = true;
                    } else if (event.jaxis.axis == 1 && inputControl->keys[iKey] == JOY_STICK_1_DOWN) {
						fUseJoystickInput = true;

						if (event.jaxis.value > JOYSTICK_DEAD_ZONE)
							fJoystickDown = true;
                    } else if (event.jaxis.axis == 2 && inputControl->keys[iKey] == JOY_STICK_2_LEFT) {
						fUseJoystickInput = true;

						if (event.jaxis.value < -JOYSTICK_DEAD_ZONE)
							fJoystickDown = true;
                    } else if (event.jaxis.axis == 2 && inputControl->keys[iKey] == JOY_STICK_2_RIGHT) {
						fUseJoystickInput = true;

						if (event.jaxis.value > JOYSTICK_DEAD_ZONE)
							fJoystickDown = true;
                    } else if (event.jaxis.axis == 3 && inputControl->keys[iKey] == JOY_STICK_2_UP) {
						fUseJoystickInput = true;

						if (event.jaxis.value < -JOYSTICK_DEAD_ZONE)
							fJoystickDown = true;
                    } else if (event.jaxis.axis == 3 && inputControl->keys[iKey] == JOY_STICK_2_DOWN) {
						fUseJoystickInput = true;

						if (event.jaxis.value > JOYSTICK_DEAD_ZONE)
							fJoystickDown = true;
					}

                    if (fUseJoystickInput) {
						//Ignore input for cpu controlled players
						if (iGameState == 0 && game_values.playercontrol[iPlayer] != 1 && iKey < 6)
							continue;

                        if (fJoystickDown) {
							fFound = true;

							if (!outputControl->keys[iKey].fDown)
								outputControl->keys[iKey].fPressed = true;

							outputControl->keys[iKey].fDown = true;
                        } else {
							outputControl->keys[iKey].fDown = false;
						}
					}
				}
			}
		}

		//This line might be causing input from some players not to be read
		//if (fFound)
			//break;
	}
}

void CPlayerInput::AssignJoystick(short device)
{
    // Only hand out game controllers, as some keyboard and mouse receivers show up as joysticks too.
    // Other joysticks can still be picked in the controls menu.
    SDL_Joystick* joystick = GetJoystick(device);
    if (!joystick || !SDL_IsGamepad(SDL_GetJoystickID(joystick)))
        return;

    for (short iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++) {
        if (inputControls[iPlayer] && inputControls[iPlayer]->iDevice == device)
            return;
    }

    for (short iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++) {
        if (inputControls[iPlayer] && inputControls[iPlayer]->iDevice == DEVICE_KEYBOARD) {
            CInputPlayerControl& joystickControl = game_values.inputConfiguration[iPlayer][1];
            joystickControl.iDevice = device;
            inputControls[iPlayer] = &joystickControl;
            printf("[input] Joystick %d assigned to player %d\n", device, iPlayer + 1);
            return;
        }
    }
}
