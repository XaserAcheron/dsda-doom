//
// Copyright(C) 2022 by Ryan Krafnick
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	DSDA Weapon Text HUD Component
//

#include "base.h"

#include "weapon_text.h"

static dsda_text_t component;

static void dsda_UpdateComponentText(char* str, size_t max_size) {
  player_t* player;

  player = &players[displayplayer];

  snprintf(
    str,
    max_size,
    "WPN \x1b%c%c\x1b%c %c %c %c %c %c %c %c %c",
    player->powers[pw_strength] ? HUlib_Color(CR_BLUE) : HUlib_Color(CR_GREEN),
    player->weaponowned[0] ? '1' : ' ',
    HUlib_Color(CR_GREEN),
    player->weaponowned[1] ? '2' : ' ',
    player->weaponowned[2] ? '3' : ' ',
    player->weaponowned[3] ? '4' : ' ',
    player->weaponowned[4] ? '5' : ' ',
    player->weaponowned[5] ? '6' : ' ',
    player->weaponowned[6] ? '7' : ' ',
    player->weaponowned[7] ? '8' : ' ',
    player->weaponowned[8] ? '9' : ' '
  );
}

void dsda_InitWeaponTextHC(int x_offset, int y_offset, int vpt) {
  dsda_InitTextHC(&component, x_offset, y_offset, vpt);
}

void dsda_UpdateWeaponTextHC(void) {
  dsda_UpdateComponentText(component.msg, sizeof(component.msg));
  dsda_RefreshHudText(&component);
}

void dsda_DrawWeaponTextHC(void) {
  HUlib_drawTextLine(&component.text, false);
}

void dsda_EraseWeaponTextHC(void) {
  HUlib_eraseTextLine(&component.text);
}
