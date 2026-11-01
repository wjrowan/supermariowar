#ifndef INPUT_H
#define INPUT_H

#include "GlobalConstants.h"

#include <SDL3/SDL.h>

#include <stdint.h>

//This set of classes takes SDL input events, sees if a
//player configured button is pressed, and sets the output
//key structure if that button is down/pressed.
//This way you just need to call Update(SDL_Event) once a frame
//and then to use all the player input, just call
//CPlayerInput.outputControls.{key}.{presstype} to get the key state you want

#define NUM_KEYS 8

#define DEVICE_KEYBOARD     -1

#define JOYSTICK_DEAD_ZONE  16384
#define MOUSE_X_DEAD_ZONE   0
#define MOUSE_Y_DEAD_ZONE   5

#define MOUSE_UP            323
#define MOUSE_DOWN          324
#define MOUSE_LEFT          325
#define MOUSE_RIGHT         326
#define MOUSE_BUTTON_START  327

#define JOY_STICK_1_UP      0
#define JOY_STICK_1_DOWN    1
#define JOY_STICK_1_LEFT    2
#define JOY_STICK_1_RIGHT   3

#define JOY_STICK_2_UP      4
#define JOY_STICK_2_DOWN    5
#define JOY_STICK_2_LEFT    6
#define JOY_STICK_2_RIGHT   7

#define JOY_HAT_UP          8
#define JOY_HAT_DOWN        9
#define JOY_HAT_LEFT        10
#define JOY_HAT_RIGHT       11
#define JOY_BUTTON_START    12

#define KEY_NONE            SDLK_UNKNOWN


struct CKeyState {
	bool fDown;
	bool fPressed;
};

struct CInputControl {
    union {
        struct {
			SDL_Keycode menu_up, menu_down, menu_left, menu_right, menu_select, menu_cancel, menu_random, menu_scrollfast;
		};
        struct {
			SDL_Keycode game_left, game_right, game_jump, game_down, game_turbo, game_powerup, game_start, game_cancel;
		};
		SDL_Keycode keys[NUM_KEYS];
	};
};

struct COutputControl {
    union {
        struct {
			CKeyState menu_up, menu_down, menu_left, menu_right, menu_select, menu_cancel, menu_random, menu_scrollfast;
		};
        struct {
			CKeyState game_left, game_right, game_jump, game_down, game_turbo, game_powerup, game_start, game_cancel;
		};
		CKeyState keys[NUM_KEYS];
	};

    void copyFrom(const COutputControl& other) {
        for (uint8_t k = 0; k < NUM_KEYS; k++) {
            keys[k] = other.keys[k];
        }
    }

	bool operator == (const COutputControl& control) const {
		for (uint8_t k = 0; k < NUM_KEYS; k++)
			if (this->keys[k].fDown != control.keys[k].fDown
				|| this->keys[k].fPressed != control.keys[k].fPressed)
					return false;
		return true;
	}

	bool operator != (const COutputControl& control) const {
		return !(*this == control);
	}
};

struct CInputPlayerControl {
	short iDevice;
	CInputControl inputGameControls[2]; //0 == game controls, 1 == menu controls
};

class CPlayerInput
{
	public:

		CPlayerInput();

		//Clears key pressed states
		void ClearPressedKeys(short iGameState);
		void ClearGameActionKeys();
		void ResetKeys();
		//Updates what commands the user is sending
		void Update(SDL_Event event, short iGameState);

		//Gives a game controller to the first player on the keyboard who didn't choose it,
		//unless a player already uses it (eg. a controller that reconnected)
		void AssignJoystick(short device);

		//Points to the input controls in the global inputConfiguration class
		CInputPlayerControl * inputControls[MAX_PLAYERS];

		//Use these structures to see what input has been pressed
		COutputControl outputControls[MAX_PLAYERS];

		//Players who picked the keyboard in the controls menu, so they don't get a controller automatically
		bool fKeyboardChosen[MAX_PLAYERS];

		SDL_Keycode iPressedKey;
};

/// Opens a connected game controller, reusing the device index of a disconnected one.
/// Returns its device index, or -1 if it's already open, isn't a game controller or couldn't be opened.
short OpenJoystick(SDL_JoystickID id);
/// Closes a disconnected joystick. Its device index is kept for when it comes back.
void CloseJoystick(SDL_JoystickID id);
/// The joystick of a device index, or nullptr if it's not connected.
SDL_Joystick* GetJoystick(short device);

#endif // INPUT_H
