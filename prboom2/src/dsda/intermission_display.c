//
// Copyright(C) 2021 by Ryan Krafnick
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
//	DSDA Intermission Display
//

#include "hu_lib.h"
#include "hu_stuff.h"

#include "dsda/command_display.h"
#include "dsda/global.h"
#include "dsda/settings.h"
#include "dsda/split_tracker.h"
#include "dsda/hud.h"

#include "intermission_display.h"

#define DSDA_INTERMISSION_TIME_X 2
#define DSDA_INTERMISSION_TIME_Y 1

static dsda_text_t dsda_intermission_time;
static dsda_text_t dsda_intermission_total;

void dsda_InitIntermissionDisplay(patchnum_t* font) {
  HUlib_initTextLine(
    &dsda_intermission_time.text,
    DSDA_INTERMISSION_TIME_X,
    DSDA_INTERMISSION_TIME_Y,
    font,
    HU_FONTSTART,
    CR_GRAY,
    VPT_ALIGN_LEFT
  );

  HUlib_initTextLine(
    &dsda_intermission_total.text,
    DSDA_INTERMISSION_TIME_X,
    DSDA_INTERMISSION_TIME_Y + 8,
    font,
    HU_FONTSTART,
    CR_GRAY,
    VPT_ALIGN_LEFT
  );
}

extern int leveltime, totalleveltimes;

static int dsda_SplitComparisonDelta(dsda_split_time_t* split_time) {
  return split_time->ref ? split_time->ref_delta : split_time->best_delta;
}

static void dsda_UpdateIntermissionTime(dsda_split_t* split) {
  char* s;
  char delta[16];
  char color;

  delta[0] = '\0';
  color = HUlib_Color(CR_GRAY);

  if (split && !split->first_time) {
    const char* sign;
    int diff;

    diff = dsda_SplitComparisonDelta(&split->leveltime);
    sign = diff >= 0 ? "+" : "-";
    color = diff >= 0 ? HUlib_Color(CR_GRAY) : HUlib_Color(CR_GREEN);
    diff = abs(diff);

    if (diff >= 2100) {
      snprintf(
        delta, sizeof(delta),
        " (%s%d:%05.2f)",
        sign, diff / 35 / 60, (float)(diff % (60 * 35)) / 35
      );
    }
    else {
      snprintf(
        delta, sizeof(delta),
        " (%s%04.2f)",
        sign, (float)(diff % (60 * 35)) / 35
      );
    }
  }

  snprintf(
    dsda_intermission_time.msg,
    sizeof(dsda_intermission_time.msg),
    "\x1b%c%d:%05.2f",
    color, leveltime / 35 / 60,
    (float)(leveltime % (60 * 35)) / 35
  );

  strcat(dsda_intermission_time.msg, delta);

  dsda_RefreshHudText(&dsda_intermission_time);
}

static void dsda_UpdateIntermissionTotal(dsda_split_t* split) {
  char* s;
  char delta[16];
  char color;

  delta[0] = '\0';
  color = HUlib_Color(CR_GRAY);

  if (split && !split->first_time) {
    const char* sign;
    int diff;

    diff = dsda_SplitComparisonDelta(&split->totalleveltimes) / 35;
    sign = diff >= 0 ? "+" : "-";
    color = diff >= 0 ? HUlib_Color(CR_GRAY) : HUlib_Color(CR_GREEN);
    diff = abs(diff);

    if (diff >= 60) {
      snprintf(
        delta, sizeof(delta),
        " (%s%d:%02d)",
        sign, diff / 60, diff % 60
      );
    }
    else {
      snprintf(
        delta, sizeof(delta),
        " (%s%d)",
        sign, diff % 60
      );
    }
  }

  snprintf(
    dsda_intermission_total.msg,
    sizeof(dsda_intermission_total.msg),
    "\x1b%c%d:%02d",
    color, totalleveltimes / 35 / 60,
    (totalleveltimes / 35) % 60
  );

  strcat(dsda_intermission_total.msg, delta);

  dsda_RefreshHudText(&dsda_intermission_total);
}

void dsda_DrawIntermissionDisplay(void) {
  char* s;
  dsda_split_t* split;

  split = dsda_CurrentSplit();

  dsda_UpdateIntermissionTime(split);
  dsda_UpdateIntermissionTotal(split);

  HUlib_drawTextLine(&dsda_intermission_time.text, false);
  HUlib_drawTextLine(&dsda_intermission_total.text, false);

  if (dsda_CommandDisplay()) dsda_DrawCommandDisplay();
}
