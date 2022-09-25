//
// Copyright(C) 2020 by Ryan Krafnick
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
//	DSDA Console
//

#include "doomstat.h"
#include "g_game.h"
#include "hu_lib.h"
#include "hu_stuff.h"
#include "i_system.h"
#include "lprintf.h"
#include "m_cheat.h"
#include "m_menu.h"
#include "m_misc.h"
#include "s_sound.h"
#include "v_video.h"

#include "dsda.h"
#include "dsda/build.h"
#include "dsda/brute_force.h"
#include "dsda/configuration.h"
#include "dsda/demo.h"
#include "dsda/exhud.h"
#include "dsda/global.h"
#include "dsda/playback.h"
#include "dsda/settings.h"
#include "dsda/tracker.h"
#include "dsda/utility.h"

#include "console.h"

extern patchnum_t hu_font2[HU_FONTSIZE];

#define target_player players[consoleplayer]

#define CONSOLE_ENTRY_SIZE 64

#define CF_NEVER  0x00
#define CF_DEMO   0x01
#define CF_STRICT 0x02
#define CF_ALWAYS (CF_DEMO|CF_STRICT)

static char console_prompt[CONSOLE_ENTRY_SIZE + 3] = { '$', ' ' };
static char console_message[CONSOLE_ENTRY_SIZE + 3] = { ' ', ' ' };
static char* console_entry = console_prompt + 2;
static char* console_message_entry = console_message + 2;
static char last_console_entry[CONSOLE_ENTRY_SIZE + 1];
static int console_entry_index;
static hu_textline_t hu_console_prompt;
static hu_textline_t hu_console_message;

static char** dsda_console_script_lines[CONSOLE_SCRIPT_COUNT];

static void dsda_DrawConsole(void) {
  V_FillRect(0, 0, 0, SCREENWIDTH, 16 * SCREENHEIGHT / 200, 0);
  HUlib_drawTextLine(&hu_console_prompt, false);
  HUlib_drawTextLine(&hu_console_message, false);
}

menu_t dsda_ConsoleDef = {
  0,
  NULL,
  NULL,
  dsda_DrawConsole,
  0, 0,
  0, MENUF_TEXTINPUT
};

static dboolean dsda_ExecuteConsole(const char* command_line);

static void dsda_UpdateConsoleDisplay(void) {
  const char* s;

  s = console_prompt;
  HUlib_clearTextLine(&hu_console_prompt);
  while (*s) HUlib_addCharToTextLine(&hu_console_prompt, *(s++));
  HUlib_addCharToTextLine(&hu_console_prompt, '_');

  s = console_message;
  HUlib_clearTextLine(&hu_console_message);
  while (*s) HUlib_addCharToTextLine(&hu_console_message, *(s++));
}

static void dsda_ResetConsoleEntry(void) {
  memset(console_entry, 0, CONSOLE_ENTRY_SIZE);
  console_entry_index = 0;
  dsda_UpdateConsoleDisplay();
}

dboolean dsda_OpenConsole(void) {
  static dboolean firsttime = true;

  if (gamestate != GS_LEVEL)
    return false;

  if (firsttime) {
    firsttime = false;

    HUlib_initTextLine(
      &hu_console_prompt,
      0,
      8,
      hu_font2,
      HU_FONTSTART,
      CR_GRAY,
      VPT_ALIGN_LEFT_TOP
    );

    HUlib_initTextLine(
      &hu_console_message,
      0,
      0,
      hu_font2,
      HU_FONTSTART,
      CR_GRAY,
      VPT_ALIGN_LEFT_TOP
    );
  }

  M_StartControlPanel();
  M_SetupNextMenu(&dsda_ConsoleDef);
  dsda_ResetConsoleEntry();

  return true;
}

static dboolean console_PlayerSetHealth(const char* command, const char* args) {
  int health;

  if (sscanf(args, "%i", &health)) {
    target_player.mo->health = health;
    target_player.health = health;

    return true;
  }

  return false;
}

static dboolean console_PlayerSetArmor(const char* command, const char* args) {
  int arg_count;
  int armorpoints, armortype;

  arg_count = sscanf(args, "%i %i", &armorpoints, &armortype);

  if (arg_count != 2 || (armortype != 1 && armortype != 2))
    armortype = target_player.armortype;

  if (arg_count) {
    target_player.armorpoints[ARMOR_ARMOR] = armorpoints;

    if (armortype == 0) armortype = 1;
    target_player.armortype = armortype;

    return true;
  }

  return false;
}

static dboolean console_PlayerGiveWeapon(const char* command, const char* args) {
  dboolean P_GiveWeapon(player_t *player, weapontype_t weapon, dboolean dropped);
  void TryPickupWeapon(player_t * player, pclass_t weaponClass,
                       weapontype_t weaponType, mobj_t * weapon,
                       const char *message);

  int weapon;

  if (sscanf(args, "%i", &weapon)) {
    if (hexen) {
      mobj_t mo;

      if (weapon < 0 || weapon >= HEXEN_NUMWEAPONS)
        return false;

      memset(&mo, 0, sizeof(mo));

      mo.intflags |= MIF_FAKE;

      TryPickupWeapon(&target_player, target_player.pclass, weapon, &mo, "WEAPON");
    }
    else {
      if (weapon < 0 || weapon >= NUMWEAPONS)
        return false;

      P_GiveWeapon(&target_player, weapon, false);
    }

    return true;
  }

  return false;
}

static dboolean console_PlayerGiveAmmo(const char* command, const char* args) {
  int ammo;
  int amount;
  int arg_count;

  arg_count = sscanf(args, "%i %i", &ammo, &amount);

  if (arg_count == 2) {
    if (ammo < 0 || ammo >= g_numammo || amount <= 0)
      return false;

    if (hexen) {
      target_player.ammo[ammo] += amount;
      if (target_player.ammo[ammo] > MAX_MANA)
        target_player.ammo[ammo] = MAX_MANA;
    }
    else {
      target_player.ammo[ammo] += amount;
      if (target_player.ammo[ammo] > target_player.maxammo[ammo])
        target_player.ammo[ammo] = target_player.maxammo[ammo];
    }

    return true;
  }
  else if (arg_count == 1) {
    if (ammo < 0 || ammo >= g_numammo)
      return false;

    if (hexen)
      target_player.ammo[ammo] = MAX_MANA;
    else
      target_player.ammo[ammo] = target_player.maxammo[ammo];

    return true;
  }

  return false;
}

static dboolean console_PlayerSetAmmo(const char* command, const char* args) {
  int ammo;
  int amount;

  if (sscanf(args, "%i %i", &ammo, &amount) == 2) {
    if (ammo < 0 || ammo >= g_numammo || amount < 0)
      return false;

    if (hexen) {
      target_player.ammo[ammo] = amount;
      if (target_player.ammo[ammo] > MAX_MANA)
        target_player.ammo[ammo] = MAX_MANA;
    }
    else {
      target_player.ammo[ammo] = amount;
      if (target_player.ammo[ammo] > target_player.maxammo[ammo])
        target_player.ammo[ammo] = target_player.maxammo[ammo];
    }

    return true;
  }

  return false;
}

static dboolean console_PlayerGiveKey(const char* command, const char* args) {
  extern int playerkeys;

  int key;

  if (sscanf(args, "%i", &key)) {
    if (key < 0 || key >= NUMCARDS)
      return false;

    target_player.cards[key] = true;
    playerkeys |= 1 << key;

    return true;
  }

  return false;
}

static dboolean console_PlayerRemoveKey(const char* command, const char* args) {
  extern int playerkeys;

  int key;

  if (sscanf(args, "%i", &key)) {
    if (key < 0 || key >= NUMCARDS)
      return false;

    target_player.cards[key] = false;
    playerkeys &= ~(1 << key);

    return true;
  }

  return false;
}

static dboolean console_PlayerGivePower(const char* command, const char* args) {
  dboolean P_GivePower(player_t *player, int power);
  void SB_Start(void);

  int power;
  int duration = -1;

  if (sscanf(args, "%i %i", &power, &duration)) {
    if (power < 0 || power >= NUMPOWERS ||
        power == pw_shield || power == pw_health2 || power == pw_minotaur)
      return false;

    target_player.powers[power] = 0;
    P_GivePower(&target_player, power);
    if (power != pw_strength)
      target_player.powers[power] = duration;

    if (raven) SB_Start();

    return true;
  }

  return false;
}

static dboolean console_PlayerRemovePower(const char* command, const char* args) {
  void SB_Start(void);

  int power;

  if (sscanf(args, "%i", &power)) {
    if (power < 0 || power >= NUMPOWERS ||
        power == pw_shield || power == pw_health2 || power == pw_minotaur)
      return false;

    target_player.powers[power] = 0;

    if (power == pw_invulnerability) {
      target_player.mo->flags2 &= ~(MF2_INVULNERABLE | MF2_REFLECTIVE);
      if (target_player.pclass == PCLASS_CLERIC)
      {
        target_player.mo->flags2 &= ~(MF2_DONTDRAW | MF2_NONSHOOTABLE);
        target_player.mo->flags &= ~(MF_SHADOW | MF_ALTSHADOW);
      }
    }
    else if (power == pw_invisibility)
      target_player.mo->flags &= ~MF_SHADOW;
    else if (power == pw_flight) {
      if (target_player.mo->z != target_player.mo->floorz)
      {
        target_player.centering = true;
      }
      target_player.mo->flags2 &= ~MF2_FLY;
      target_player.mo->flags &= ~MF_NOGRAVITY;
    }
    else if (power == pw_weaponlevel2 && heretic) {
      if ((target_player.readyweapon == wp_phoenixrod)
          && (target_player.psprites[ps_weapon].state
              != &states[HERETIC_S_PHOENIXREADY])
          && (target_player.psprites[ps_weapon].state
              != &states[HERETIC_S_PHOENIXUP]))
      {
        P_SetPsprite(&target_player, ps_weapon, HERETIC_S_PHOENIXREADY);
        target_player.ammo[am_phoenixrod] -= USE_PHRD_AMMO_2;
        target_player.refire = 0;
      }
      else if ((target_player.readyweapon == wp_gauntlets)
               || (target_player.readyweapon == wp_staff))
      {
        target_player.pendingweapon = target_player.readyweapon;
      }
    }

    if (raven) SB_Start();

    return true;
  }

  return false;
}

static dboolean console_PlayerSetCoordinate(const char* args, int* dest) {
  int x, x_frac = 0;
  double x_double;

  if (sscanf(args, "%i.%i", &x, &x_frac)) {
    *dest = FRACUNIT * x;

    if (args[0] == '-')
      *dest -= x_frac;
    else
      *dest += x_frac;

    return true;
  }

  return false;
}

static dboolean console_PlayerSetX(const char* command, const char* args) {
  return console_PlayerSetCoordinate(args, &target_player.mo->x);
}

static dboolean console_PlayerSetY(const char* command, const char* args) {
  return console_PlayerSetCoordinate(args, &target_player.mo->y);
}

static dboolean console_PlayerSetZ(const char* command, const char* args) {
  return console_PlayerSetCoordinate(args, &target_player.mo->z);
}

static void console_PlayerRoundCoordinate(int* x) {
  int bits = *x & 0xffff;
  if (!bits) return;

  if (*x > 0) {
    if (bits >= 0x8000)
      *x = (*x & ~0xffff) + FRACUNIT;
    else
      *x = *x & ~0xffff;
  }
  else {
    if (bits < 0x8000)
      *x = (*x & ~0xffff) - FRACUNIT;
    else
      *x = *x & ~0xffff;
  }
}

static dboolean console_PlayerRoundX(const char* command, const char* args) {
  console_PlayerRoundCoordinate(&target_player.mo->x);

  return true;
}

static dboolean console_PlayerRoundY(const char* command, const char* args) {
  console_PlayerRoundCoordinate(&target_player.mo->y);

  return true;
}

static dboolean console_PlayerRoundXY(const char* command, const char* args) {
  console_PlayerRoundCoordinate(&target_player.mo->x);
  console_PlayerRoundCoordinate(&target_player.mo->y);

  return true;
}

static dboolean console_DemoExport(const char* command, const char* args) {
  char name[CONSOLE_ENTRY_SIZE];

  if (sscanf(args, "%s", name) == 1) {
    dsda_ExportDemo(name);
    return true;
  }

  return false;
}

static dboolean console_DemoStart(const char* command, const char* args) {
  char name[CONSOLE_ENTRY_SIZE];

  if (sscanf(args, "%s", name) == 1)
    return dsda_StartDemoSegment(name);

  return false;
}

static dboolean console_DemoStop(const char* command, const char* args) {
  if (!demorecording)
    return false;

  G_CheckDemoStatus();
  dsda_UpdateStrictMode();

  return true;
}

static dboolean console_TrackerAddLine(const char* command, const char* args) {
  int id;

  if (sscanf(args, "%i", &id))
    return dsda_TrackLine(id);

  return false;
}

static dboolean console_TrackerRemoveLine(const char* command, const char* args) {
  int id;

  if (sscanf(args, "%i", &id))
    return dsda_UntrackLine(id);

  return false;
}

static dboolean console_TrackerAddLineDistance(const char* command, const char* args) {
  int id;

  if (sscanf(args, "%i", &id))
    return dsda_TrackLineDistance(id);

  return false;
}

static dboolean console_TrackerRemoveLineDistance(const char* command, const char* args) {
  int id;

  if (sscanf(args, "%i", &id))
    return dsda_UntrackLineDistance(id);

  return false;
}

static dboolean console_TrackerAddSector(const char* command, const char* args) {
  int id;

  if (sscanf(args, "%i", &id))
    return dsda_TrackSector(id);

  return false;
}

static dboolean console_TrackerRemoveSector(const char* command, const char* args) {
  int id;

  if (sscanf(args, "%i", &id))
    return dsda_UntrackSector(id);

  return false;
}

static dboolean console_TrackerAddMobj(const char* command, const char* args) {
  int id;

  if (sscanf(args, "%i", &id))
    return dsda_TrackMobj(id);

  return false;
}

static dboolean console_TrackerRemoveMobj(const char* command, const char* args) {
  int id;

  if (sscanf(args, "%i", &id))
    return dsda_UntrackMobj(id);

  return false;
}

static dboolean console_TrackerAddPlayer(const char* command, const char* args) {
  return dsda_TrackPlayer(0);
}

static dboolean console_TrackerRemovePlayer(const char* command, const char* args) {
  return dsda_UntrackPlayer(0);
}

static dboolean console_TrackerReset(const char* command, const char* args) {
  dsda_WipeTrackers();

  return true;
}

static dboolean console_JumpToTic(const char* command, const char* args) {
  int tic;

  if (sscanf(args, "%i", &tic)) {
    if (tic < 0)
      return false;

    dsda_JumpToLogicTic(tic);

    return true;
  }

  return false;
}

static dboolean console_JumpByTic(const char* command, const char* args) {
  int tic;

  if (sscanf(args, "%i", &tic)) {
    tic = logictic + tic;

    dsda_JumpToLogicTic(tic);

    return true;
  }

  return false;
}

static dboolean console_BuildMF(const char* command, const char* args) {
  int x;

  return sscanf(args, "%i", &x) && dsda_BuildMF(x);
}

static dboolean console_BuildMB(const char* command, const char* args) {
  int x;

  return sscanf(args, "%i", &x) && dsda_BuildMB(x);
}

static dboolean console_BuildSR(const char* command, const char* args) {
  int x;

  return sscanf(args, "%i", &x) && dsda_BuildSR(x);
}

static dboolean console_BuildSL(const char* command, const char* args) {
  int x;

  return sscanf(args, "%i", &x) && dsda_BuildSL(x);
}

static dboolean console_BuildTR(const char* command, const char* args) {
  int x;

  return sscanf(args, "%i", &x) && dsda_BuildTR(x);
}

static dboolean console_BuildTL(const char* command, const char* args) {
  int x;

  return sscanf(args, "%i", &x) && dsda_BuildTL(x);
}

static dboolean console_BruteForceStart(const char* command, const char* args) {
  int depth;
  int forwardmove_min, forwardmove_max;
  int sidemove_min, sidemove_max;
  int angleturn_min, angleturn_max;
  char condition_args[CONSOLE_ENTRY_SIZE];
  int arg_count;

  dsda_ResetBruteForceConditions();

  arg_count = sscanf(
    args, "%i %i:%i %i:%i %i:%i %[^;]", &depth,
    &forwardmove_min, &forwardmove_max,
    &sidemove_min, &sidemove_max,
    &angleturn_min, &angleturn_max,
    condition_args
  );

  if (arg_count == 8) {
    int i;
    char** conditions;

    conditions = dsda_SplitString(condition_args, ",");

    if (!conditions)
      return false;

    for (i = 0; conditions[i]; ++i) {
      dsda_bf_attribute_t attribute;
      dsda_bf_operator_t operator;
      fixed_t value;
      char attr_s[4] = { 0 };
      char oper_s[3] = { 0 };

      if (sscanf(conditions[i], "skip %i", &value) == 1) {
        if (value >= numlines || value < 0)
          return false;

        dsda_AddMiscBruteForceCondition(dsda_bf_line_skip, value);
      }
      else if (sscanf(conditions[i], "act %i", &value) == 1) {
        if (value >= numlines || value < 0)
          return false;

        dsda_AddMiscBruteForceCondition(dsda_bf_line_activation, value);
      }
      else if (sscanf(conditions[i], "%3s %2s %i", attr_s, oper_s, &value) == 3) {
        int attr_i, oper_i;

        for (attr_i = 0; attr_i < dsda_bf_attribute_max; ++attr_i)
          if (!strcmp(attr_s, dsda_bf_attribute_names[attr_i]))
            break;

        if (attr_i == dsda_bf_attribute_max)
          return false;

        for (oper_i = dsda_bf_limit_trio_zero; oper_i < dsda_bf_limit_trio_max; ++oper_i)
          if (!strcmp(oper_s, dsda_bf_limit_names[oper_i])) {
            dsda_SetBruteForceTarget(attr_i, oper_i, value);
            continue;
          }

        for (oper_i = 0; oper_i < dsda_bf_operator_max; ++oper_i)
          if (!strcmp(oper_s, dsda_bf_operator_names[oper_i]))
            break;

        if (oper_i == dsda_bf_operator_max)
          return false;

        dsda_AddBruteForceCondition(attr_i, oper_i, value);
      }
      else if (sscanf(conditions[i], "%3s %4s", attr_s, oper_s) == 2) {
        int attr_i, oper_i;

        for (attr_i = 0; attr_i < dsda_bf_attribute_max; ++attr_i)
          if (!strcmp(attr_s, dsda_bf_attribute_names[attr_i]))
            break;

        if (attr_i == dsda_bf_attribute_max)
          return false;

        for (oper_i = dsda_bf_limit_duo_zero; oper_i < dsda_bf_limit_duo_max; ++oper_i)
          if (!strcmp(oper_s, dsda_bf_limit_names[oper_i]))
            break;

        if (oper_i == dsda_bf_limit_duo_max)
          return false;

        dsda_SetBruteForceTarget(attr_i, oper_i, 0);
      }
    }

    Z_Free(conditions);

    dsda_StartBruteForce(depth,
                         forwardmove_min, forwardmove_max,
                         sidemove_min, sidemove_max,
                         angleturn_min, angleturn_max);

    return true;
  }

  return false;
}

static dboolean console_BuildTurbo(const char* command, const char* args) {
  dsda_ToggleBuildTurbo();

  return true;
}

static dboolean console_Exit(const char* command, const char* args) {
  extern void M_ClearMenus(void);

  M_ClearMenus();

  return true;
}

static dboolean console_BasicCheat(const char* command, const char* args) {
  return M_CheatEntered(command, args);
}

static dboolean console_CheatFullClip(const char* command, const char* args) {
  target_player.cheats ^= CF_INFINITE_AMMO;
  return true;
}

static dboolean console_Freeze(const char* command, const char* args) {
  dsda_ToggleFrozenMode();
  return true;
}

static dboolean console_NoSleep(const char* command, const char* args) {
  int i;

  for (i = 0; i < numsectors; ++i)
    sectors[i].soundtarget = target_player.mo;

  return true;
}

static dboolean console_ScriptRunLine(const char* line) {
  if (strlen(line) && line[0] != '#' && line[0] != '!' && line[0] != '/') {
    if (strlen(line) >= CONSOLE_ENTRY_SIZE) {
      lprintf(LO_ERROR, "Script line too long: \"%s\" (limit %d)\n", line, CONSOLE_ENTRY_SIZE);
      return false;
    }

    if (!dsda_ExecuteConsole(line)) {
      lprintf(LO_ERROR, "Script line failed: \"%s\"\n", line);
      return false;
    }
  }

  return true;
}

static dboolean console_ScriptRun(const char* command, const char* args) {
  char name[CONSOLE_ENTRY_SIZE];
  dboolean ret = true;

  if (sscanf(args, "%s", name)) {
    char* filename;
    char* buffer;

    filename = I_FindFile(name, "");

    if (filename) {
      if (M_ReadFileToString(filename, &buffer) != -1) {
        char* line;

        for (line = strtok(buffer, "\n;"); line; line = strtok(NULL, "\n;"))
          if (!console_ScriptRunLine(line)) {
            ret = false;
            break;
          }

        Z_Free(buffer);
      }
      else {
        lprintf(LO_ERROR, "Unable to read script file (%s)\n", filename);
        ret = false;
      }

      Z_Free(filename);
    }
    else {
      lprintf(LO_ERROR, "Cannot find script file (%s)\n", name);
      ret = false;
    }

    return ret;
  }

  return false;
}

static dboolean console_Check(const char* command, const char* args) {
  char name[CONSOLE_ENTRY_SIZE];

  if (sscanf(args, "%s", name)) {
    char* summary;

    summary = dsda_ConfigSummary(name);

    if (summary) {
      lprintf(LO_INFO, "%s\n", summary);
      Z_Free(summary);
      return true;
    }
  }

  return false;
}

static dboolean console_ChangeConfig(const char* command, const char* args, dboolean persist) {
  char name[CONSOLE_ENTRY_SIZE];
  char value_string[CONSOLE_ENTRY_SIZE];
  int value_int;

  if (sscanf(args, "%s %d", name, &value_int)) {
    int id;

    id = dsda_ConfigIDByName(name);
    if (id) {
      dsda_UpdateIntConfig(id, value_int, persist);
      return true;
    }
  }
  else if (sscanf(args, "%s %s", name, value_string)) {
    int id;

    id = dsda_ConfigIDByName(name);
    if (id) {
      dsda_UpdateStringConfig(id, value_string, persist);
      return true;
    }
  }

  return false;
}

static dboolean console_Assign(const char* command, const char* args) {
  return console_ChangeConfig(command, args, false);
}

static dboolean console_Update(const char* command, const char* args) {
  return console_ChangeConfig(command, args, true);
}

typedef dboolean (*console_command_t)(const char*, const char*);

typedef struct {
  const char* command_name;
  console_command_t command;
  int flags;
} console_command_entry_t;

static console_command_entry_t console_commands[] = {
  // commands
  { "player.set_health", console_PlayerSetHealth, CF_NEVER },
  { "player.set_armor", console_PlayerSetArmor, CF_NEVER },
  { "player.give_weapon", console_PlayerGiveWeapon, CF_NEVER },
  { "player.give_ammo", console_PlayerGiveAmmo, CF_NEVER },
  { "player.set_ammo", console_PlayerSetAmmo, CF_NEVER },
  { "player.give_key", console_PlayerGiveKey, CF_NEVER },
  { "player.remove_key", console_PlayerRemoveKey, CF_NEVER },
  { "player.give_power", console_PlayerGivePower, CF_NEVER },
  { "player.remove_power", console_PlayerRemovePower, CF_NEVER },
  { "player.set_x", console_PlayerSetX, CF_NEVER },
  { "player.set_y", console_PlayerSetY, CF_NEVER },
  { "player.set_z", console_PlayerSetZ, CF_NEVER },
  { "player.round_x", console_PlayerRoundX, CF_NEVER },
  { "player.round_y", console_PlayerRoundY, CF_NEVER },
  { "player.round_xy", console_PlayerRoundXY, CF_NEVER },

  { "script.run", console_ScriptRun, CF_ALWAYS },
  { "check", console_Check, CF_ALWAYS },
  { "assign", console_Assign, CF_ALWAYS },
  { "update", console_Update, CF_ALWAYS },

  // tracking
  { "tracker.add_line", console_TrackerAddLine, CF_DEMO },
  { "t.al", console_TrackerAddLine, CF_DEMO },
  { "tracker.remove_line", console_TrackerRemoveLine, CF_DEMO },
  { "t.rl", console_TrackerRemoveLine, CF_DEMO },
  { "tracker.add_line_distance", console_TrackerAddLineDistance, CF_DEMO },
  { "t.ald", console_TrackerAddLineDistance, CF_DEMO },
  { "tracker.remove_line_distance", console_TrackerRemoveLineDistance, CF_DEMO },
  { "t.rld", console_TrackerRemoveLineDistance, CF_DEMO },
  { "tracker.add_sector", console_TrackerAddSector, CF_DEMO },
  { "t.as", console_TrackerAddSector, CF_DEMO },
  { "tracker.remove_sector", console_TrackerRemoveSector, CF_DEMO },
  { "t.rs", console_TrackerRemoveSector, CF_DEMO },
  { "tracker.add_mobj", console_TrackerAddMobj, CF_DEMO },
  { "t.am", console_TrackerAddMobj, CF_DEMO },
  { "tracker.remove_mobj", console_TrackerRemoveMobj, CF_DEMO },
  { "t.rm", console_TrackerRemoveMobj, CF_DEMO },
  { "tracker.add_player", console_TrackerAddPlayer, CF_DEMO },
  { "t.ap", console_TrackerAddPlayer, CF_DEMO },
  { "tracker.remove_player", console_TrackerRemovePlayer, CF_DEMO },
  { "t.rp", console_TrackerRemovePlayer, CF_DEMO },
  { "tracker.reset", console_TrackerReset, CF_DEMO },
  { "t.r", console_TrackerReset, CF_DEMO },

  // traversing time
  { "jump.to_tic", console_JumpToTic, CF_DEMO },
  { "jump.by_tic", console_JumpByTic, CF_DEMO },

  // build mode
  { "brute_force.start", console_BruteForceStart, CF_DEMO },
  { "bf.start", console_BruteForceStart, CF_DEMO },
  { "build.turbo", console_BuildTurbo, CF_DEMO },
  { "b.turbo", console_BuildTurbo, CF_DEMO },
  { "mf", console_BuildMF, CF_DEMO },
  { "mb", console_BuildMB, CF_DEMO },
  { "sr", console_BuildSR, CF_DEMO },
  { "sl", console_BuildSL, CF_DEMO },
  { "tr", console_BuildTR, CF_DEMO },
  { "tl", console_BuildTL, CF_DEMO },

  // demos
  { "demo.export", console_DemoExport, CF_ALWAYS },
  { "demo.start", console_DemoStart, CF_NEVER },
  { "demo.stop", console_DemoStop, CF_ALWAYS },

  // cheats
  { "idchoppers", console_BasicCheat, CF_DEMO },
  { "iddqd", console_BasicCheat, CF_DEMO },
  { "idkfa", console_BasicCheat, CF_DEMO },
  { "idfa", console_BasicCheat, CF_DEMO },
  { "idspispopd", console_BasicCheat, CF_DEMO },
  { "idclip", console_BasicCheat, CF_DEMO },
  { "idmypos", console_BasicCheat, CF_DEMO },
  { "idrate", console_BasicCheat, CF_DEMO },
  { "iddt", console_BasicCheat, CF_DEMO },
  { "iddst", console_BasicCheat, CF_DEMO },
  { "iddkt", console_BasicCheat, CF_DEMO },
  { "iddit", console_BasicCheat, CF_DEMO },
  { "idclev", console_BasicCheat, CF_DEMO },
  { "idmus", console_BasicCheat, CF_DEMO },

  { "tntcomp", console_BasicCheat, CF_DEMO },
  { "tntem", console_BasicCheat, CF_DEMO },
  { "tnthom", console_BasicCheat, CF_DEMO },
  { "tntka", console_BasicCheat, CF_DEMO },
  { "tntsmart", console_BasicCheat, CF_DEMO },
  { "tntpitch", console_BasicCheat, CF_DEMO },
  { "tntfast", console_BasicCheat, CF_DEMO },
  { "tntice", console_BasicCheat, CF_DEMO },
  { "tntpush", console_BasicCheat, CF_DEMO },

  { "notarget", console_BasicCheat, CF_DEMO },
  { "fly", console_BasicCheat, CF_DEMO },
  { "fullclip", console_CheatFullClip, CF_NEVER },
  { "freeze", console_Freeze, CF_NEVER },
  { "nosleep", console_NoSleep, CF_NEVER },

  { "quicken", console_BasicCheat, CF_DEMO },
  { "ponce", console_BasicCheat, CF_DEMO },
  { "kitty", console_BasicCheat, CF_DEMO },
  { "massacre", console_BasicCheat, CF_DEMO },
  { "rambo", console_BasicCheat, CF_DEMO },
  { "skel", console_BasicCheat, CF_DEMO },
  { "shazam", console_BasicCheat, CF_DEMO },
  { "ravmap", console_BasicCheat, CF_DEMO },
  { "cockadoodledoo", console_BasicCheat, CF_DEMO },
  { "gimme", console_BasicCheat, CF_DEMO },
  { "engage", console_BasicCheat, CF_DEMO },

  { "satan", console_BasicCheat, CF_DEMO },
  { "clubmed", console_BasicCheat, CF_DEMO },
  { "butcher", console_BasicCheat, CF_DEMO },
  { "nra", console_BasicCheat, CF_DEMO },
  { "indiana", console_BasicCheat, CF_DEMO },
  { "locksmith", console_BasicCheat, CF_DEMO },
  { "sherlock", console_BasicCheat, CF_DEMO },
  { "casper", console_BasicCheat, CF_DEMO },
  { "init", console_BasicCheat, CF_DEMO },
  { "mapsco", console_BasicCheat, CF_DEMO },
  { "deliverance", console_BasicCheat, CF_DEMO },
  { "shadowcaster", console_BasicCheat, CF_DEMO },
  { "visit", console_BasicCheat, CF_DEMO },
  { "puke", console_BasicCheat, CF_DEMO },

  // exit
  { "exit", console_Exit, CF_ALWAYS },
  { "quit", console_Exit, CF_ALWAYS },
  { NULL }
};

static void dsda_AddConsoleMessage(const char* message) {
  strncpy(console_message_entry, message, CONSOLE_ENTRY_SIZE);
}

static dboolean dsda_AuthorizeCommand(console_command_entry_t* entry) {
  if (!(entry->flags & CF_DEMO) && (demorecording || demoplayback)) {
    dsda_AddConsoleMessage("command not allowed in demo mode");
    return false;
  }

  if (!(entry->flags & CF_STRICT) && dsda_StrictMode()) {
    dsda_AddConsoleMessage("command not allowed in strict mode");
    return false;
  }

  return true;
}

static dboolean dsda_ExecuteConsole(const char* command_line) {
  char command[CONSOLE_ENTRY_SIZE];
  char args[CONSOLE_ENTRY_SIZE];
  int scan_count;
  dboolean ret = true;

  scan_count = sscanf(command_line, "%s %[^;]", command, args);

  if (scan_count) {
    console_command_entry_t* entry;

    if (scan_count == 1) args[0] = '\0';

    for (entry = console_commands; entry->command; entry++) {
      if (!stricmp(command, entry->command_name)) {
        if (dsda_AuthorizeCommand(entry)) {
          if (entry->command(command, args)) {
            dsda_AddConsoleMessage("command executed");
            S_StartSound(NULL, g_sfx_console);
          }
          else {
            dsda_AddConsoleMessage("command invalid");
            ret = false;
            S_StartSound(NULL, g_sfx_oof);
          }
        }
        else {
          S_StartSound(NULL, g_sfx_oof);
          ret = false;
        }

        break;
      }
    }

    if (!entry->command) {
      dsda_AddConsoleMessage("command unknown");
      S_StartSound(NULL, g_sfx_oof);
      ret = false;
    }
  }

  dsda_ResetConsoleEntry();

  return ret;
}

void dsda_UpdateConsoleText(char* text) {
  int i;
  int length;

  length = strlen(text);

  for (i = 0; i < length; ++i) {
    if (text[i] < 32 || text[i] > 126)
      continue;

    console_entry[console_entry_index] = tolower(text[i]);
    if (console_entry_index < CONSOLE_ENTRY_SIZE)
      ++console_entry_index;
  }

  dsda_UpdateConsoleDisplay();
}

void dsda_UpdateConsole(int action) {
  if (action == MENU_BACKSPACE && console_entry_index > 0) {
    --console_entry_index;
    console_entry[console_entry_index] = '\0';
    dsda_UpdateConsoleDisplay();
  }
  else if (action == MENU_ENTER) {
    int line;
    char* entry;
    char** lines;

    strcpy(last_console_entry, console_entry);

    entry = Z_Strdup(console_entry);
    lines = dsda_SplitString(entry, ";");
    for (line = 0; lines[line]; ++line)
      dsda_ExecuteConsole(lines[line]);

    Z_Free(entry);
    Z_Free(lines);
  }
  else if (action == MENU_UP) {
    strcpy(console_entry, last_console_entry);
    console_entry_index = strlen(console_entry);
    dsda_UpdateConsoleDisplay();
  }
}

void dsda_ExecuteConsoleScript(int i) {
  int line;

  if (gamestate != GS_LEVEL || i < 0 || i >= CONSOLE_SCRIPT_COUNT)
    return;

  if (!dsda_console_script_lines[i]) {
    char* dup;

    dup = Z_Strdup(dsda_StringConfig(dsda_config_script_0 + i));
    dsda_console_script_lines[i] = dsda_SplitString(dup, ";");
  }

  for (line = 0; dsda_console_script_lines[i][line]; ++line)
    if (!console_ScriptRunLine(dsda_console_script_lines[i][line])) {
      doom_printf("Script %d failed", i);

      return;
    }

  doom_printf("Script %d executed", i);
}
