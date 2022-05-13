/*
 * Wolfenstein: Enemy Territory GPL Source Code
 * Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.
 *
 * ET: Legacy
 * Copyright (C) 2012-2018 ET:Legacy team <mail@etlegacy.com>
 *
 * This file is part of ET: Legacy - http://www.etlegacy.com
 *
 * ET: Legacy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ET: Legacy is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ET: Legacy. If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, Wolfenstein: Enemy Territory GPL Source Code is also
 * subject to certain additional terms. You should have received a copy
 * of these additional terms immediately following the terms and conditions
 * of the GNU General Public License which accompanied the source code.
 * If not, please request a copy in writing from id Software at the address below.
 *
 * id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
 */
/**
 * @file cg_draw_hud.c
 * @brief Draws the player's hud
 *
 */

#include "cg_local.h"

typedef enum
{
	STYLE_NORMAL,
	STYLE_SIMPLE
} componentStyle;

#define SKILL_ICON_SIZE     14

#define SKILLS_X 112
#define SKILLS_Y 20

#define SKILL_BAR_OFFSET    (2 * SKILL_BAR_X_INDENT)
#define SKILL_BAR_X_INDENT  0
#define SKILL_BAR_Y_INDENT  6

#define SKILL_BAR_WIDTH     (SKILL_ICON_SIZE - SKILL_BAR_OFFSET)
#define SKILL_BAR_X         (SKILL_BAR_OFFSET + SKILL_BAR_X_INDENT + SKILLS_X)
#define SKILL_BAR_X_SCALE   (SKILL_ICON_SIZE + 2)
#define SKILL_ICON_X        (SKILL_BAR_OFFSET + SKILLS_X)
#define SKILL_ICON_X_SCALE  (SKILL_ICON_SIZE + 2)
#define SKILL_BAR_Y         (SKILL_BAR_Y_INDENT - SKILL_BAR_OFFSET - SKILLS_Y)
#define SKILL_BAR_Y_SCALE   (SKILL_ICON_SIZE + 2)
#define SKILL_ICON_Y        (-(SKILL_ICON_SIZE + 2) - SKILL_BAR_OFFSET - SKILLS_Y)

#define MAXHUDS 32

int           hudCount = 0;
hudStucture_t hudlist[MAXHUDS];

hudStucture_t *activehud;

lagometer_t lagometer;

static void CG_DrawPlayerStatusHead(hudComponent_t *comp);
static void CG_DrawGunIcon(hudComponent_t *comp);
static void CG_DrawAmmoCount(hudComponent_t *comp);
static void CG_DrawPowerUps(hudComponent_t *comp);
static void CG_DrawObjectiveStatus(hudComponent_t *comp);
static void CG_DrawPlayerHealthBar(hudComponent_t *comp);
static void CG_DrawStaminaBar(hudComponent_t *comp);
static void CG_DrawBreathBar(hudComponent_t *comp);
static void CG_DrawWeapRecharge(hudComponent_t *comp);
static void CG_DrawPlayerHealth(hudComponent_t *comp);
static void CG_DrawPlayerSprint(hudComponent_t *comp);
static void CG_DrawPlayerBreath(hudComponent_t *comp);
static void CG_DrawWeaponCharge(hudComponent_t *comp);
static void CG_DrawSkills(hudComponent_t *comp);
static void CG_DrawXP(hudComponent_t *comp);
static void CG_DrawRank(hudComponent_t *comp);
static void CG_DrawLivesLeft(hudComponent_t *comp);
static void CG_DrawCursorhint_f(hudComponent_t *comp);
static void CG_DrawWeapStability_f(hudComponent_t *comp);
static void CG_DrawRespawnTimer(hudComponent_t *comp);
static void CG_DrawSpawnTimer(hudComponent_t *comp);
static void CG_DrawLocalTime(hudComponent_t *comp);
static void CG_DrawRoundTimer(hudComponent_t *comp);
static void CG_DrawDemoMessage(hudComponent_t *comp);
static void CG_DrawFPS(hudComponent_t *comp);
static void CG_DrawSnapshot(hudComponent_t *comp);
static void CG_DrawPing(hudComponent_t *comp);
static void CG_DrawSpeed(hudComponent_t *comp);
static void CG_DrawLagometer(hudComponent_t *comp);
static void CG_DrawDisconnect(hudComponent_t *comp);

/**
 * @brief Using the stringizing operator to save typing...
 */
#define HUDF(x) # x, offsetof(hudStucture_t, x), qfalse

typedef struct
{
	const char *name;
	size_t offset;
	qboolean isAlias;
	void (*draw)(hudComponent_t *comp);

} hudComponentFields_t;

/**
* @var hudComponentFields
* @brief for accessing hudStucture_t's fields in a loop
*/
static const hudComponentFields_t hudComponentFields[] =
{
	{ HUDF(compass),          CG_DrawNewCompass       },
	{ "compas",               offsetof(hudStucture_t, compass), qtrue, CG_DrawNewCompass}, // v2.78 backward compatibility
	{ HUDF(staminabar),       CG_DrawStaminaBar       },
	{ HUDF(breathbar),        CG_DrawBreathBar        },
	{ HUDF(healthbar),        CG_DrawPlayerHealthBar  },
	{ HUDF(weaponchargebar),  CG_DrawWeapRecharge     },
	{ "weaponchangebar",      offsetof(hudStucture_t, weaponchargebar), qtrue, CG_DrawWeapRecharge}, // v2.78 backward compatibility
	{ HUDF(healthtext),       CG_DrawPlayerHealth     },
	{ HUDF(xptext),           CG_DrawXP               },
	{ HUDF(ranktext),         CG_DrawRank             },
	{ HUDF(statsdisplay),     CG_DrawSkills           },
	{ HUDF(weaponicon),       CG_DrawGunIcon          },
	{ HUDF(weaponammo),       CG_DrawAmmoCount        },
	{ HUDF(fireteam),         CG_DrawFireTeamOverlay  },    // FIXME: outside cg_draw_hud
	{ HUDF(popupmessages),    CG_DrawPMItems          },    // FIXME: outside cg_draw_hud
	{ HUDF(powerups),         CG_DrawPowerUps         },
	{ HUDF(objectives),       CG_DrawObjectiveStatus  },
	{ HUDF(hudhead),          CG_DrawPlayerStatusHead },
	{ HUDF(cursorhints),      CG_DrawCursorhint_f     },
	{ HUDF(weaponstability),  CG_DrawWeapStability_f  },
	{ HUDF(livesleft),        CG_DrawLivesLeft        },
	{ HUDF(roundtimer),       CG_DrawRoundTimer       },
	{ HUDF(reinforcement),    CG_DrawRespawnTimer     },
	{ HUDF(spawntimer),       CG_DrawSpawnTimer       },
	{ HUDF(localtime),        CG_DrawLocalTime        },
	{ HUDF(votetext),         CG_DrawVote             },    // FIXME: outside cg_draw_hud
	{ HUDF(spectatortext),    CG_DrawSpectatorMessage },    // FIXME: outside cg_draw_hud
	{ HUDF(limbotext),        CG_DrawLimboMessage     },    // FIXME: outside cg_draw_hud
	{ HUDF(followtext),       CG_DrawFollow           },    // FIXME: outside cg_draw_hud
	{ HUDF(demotext),         CG_DrawDemoMessage      },
	{ HUDF(missilecamera),    CG_DrawMissileCamera    },    // FIXME: outside cg_draw_hud
	{ HUDF(sprinttext),       CG_DrawPlayerSprint     },
	{ HUDF(breathtext),       CG_DrawPlayerBreath     },
	{ HUDF(weaponchargetext), CG_DrawWeaponCharge     },
	{ HUDF(fps),              CG_DrawFPS              },
	{ HUDF(snapshot),         CG_DrawSnapshot         },
	{ HUDF(ping),             CG_DrawPing             },
	{ HUDF(speed),            CG_DrawSpeed            },
	{ HUDF(lagometer),        CG_DrawLagometer        },
	{ HUDF(disconnect),       CG_DrawDisconnect       },
	{ NULL,                   0, qfalse, NULL         },
};

/**
 * @brief CG_getActiveHUD Returns reference to an active hud structure.
 * @return
 */
hudStucture_t *CG_GetActiveHUD()
{
	return activehud;
}

/**
 * @brief CG_getComponent
 * @param[in] x
 * @param[in] y
 * @param[in] w
 * @param[in] h
 * @param[in] visible
 * @param[in] style
 * @return
 */
static ID_INLINE hudComponent_t CG_getComponent(float x, float y, float w, float h, qboolean visible, componentStyle style, float scale, const vec4_t color, int offset, void (*draw)(hudComponent_t *comp))
{
	return (hudComponent_t) { { x, y, w, h }, visible, style, scale, { color[0], color[1], color[2], color[3] }, offset, draw };
}

vec4_t HUD_Background = { 0.16f, 0.2f, 0.17f, 0.5f };
vec4_t HUD_Border     = { 0.5f, 0.5f, 0.5f, 0.5f };
vec4_t HUD_Text       = { 0.6f, 0.6f, 0.6f, 1.0f };

/**
 * @brief CG_setDefaultHudValues
 * @param[out] hud
 */
void CG_setDefaultHudValues(hudStucture_t *hud)
{
	// the Default hud
	hud->hudnumber        = 0;
	hud->compass          = CG_getComponent(Ccg_WideX(SCREEN_WIDTH) - 100 - 20 - 16, 16, 100 + 32, 100 + 32, qtrue, STYLE_NORMAL, 0.19f, colorWhite, 0, CG_DrawNewCompass);
	hud->staminabar       = CG_getComponent(4, SCREEN_HEIGHT - 92, 12, 72, qtrue, STYLE_NORMAL, 0.19f, colorWhite, 1, CG_DrawStaminaBar);
	hud->breathbar        = CG_getComponent(4, SCREEN_HEIGHT - 92, 12, 72, qtrue, STYLE_NORMAL, 0.19f, colorWhite, 2, CG_DrawBreathBar);
	hud->healthbar        = CG_getComponent(24, SCREEN_HEIGHT - 92, 12, 72, qtrue, STYLE_NORMAL, 0.19f, colorWhite, 3, CG_DrawPlayerHealthBar);
	hud->weaponchargebar  = CG_getComponent(Ccg_WideX(SCREEN_WIDTH) - 16, SCREEN_HEIGHT - 92, 12, 72, qtrue, STYLE_NORMAL, 0.19f, colorWhite, 4, CG_DrawWeapRecharge);
	hud->healthtext       = CG_getComponent(SKILLS_X - 28, SCREEN_HEIGHT - 4, 0, 0, qtrue, STYLE_NORMAL, 0.25f, colorWhite, 5, CG_DrawPlayerHealth);
	hud->xptext           = CG_getComponent(SKILLS_X + 28, SCREEN_HEIGHT - 4, 0, 0, qtrue, STYLE_NORMAL, 0.25f, colorWhite, 6, CG_DrawXP);
	hud->ranktext         = CG_getComponent(0, SCREEN_HEIGHT, 0, 0, qfalse, STYLE_NORMAL, 0.2f, colorWhite, 7, CG_DrawRank);   // disable
	hud->statsdisplay     = CG_getComponent(SKILL_ICON_X, 0, 0, 0, qtrue, STYLE_NORMAL, 0.25f, colorWhite, 8, CG_DrawSkills);
	hud->weaponicon       = CG_getComponent(Ccg_WideX(SCREEN_WIDTH) - 82, SCREEN_HEIGHT - 56, 60, 32, qtrue, STYLE_NORMAL, 0.19f, colorWhite, 9, CG_DrawGunIcon);
	hud->weaponammo       = CG_getComponent(Ccg_WideX(SCREEN_WIDTH) - 22, SCREEN_HEIGHT - 1 * (16 + 2) + 12 - 4, 0, 0, qtrue, STYLE_NORMAL, 0.25f, colorWhite, 10, CG_DrawAmmoCount);
	hud->fireteam         = CG_getComponent(10, 10, 100, 100, qtrue, STYLE_NORMAL, 0.19f, colorWhite, 11, CG_DrawFireTeamOverlay);
	hud->popupmessages    = CG_getComponent(4, 320, 72, 72, qtrue, STYLE_NORMAL, 0.19f, colorWhite, 12, CG_DrawPMItems);
	hud->powerups         = CG_getComponent(Ccg_WideX(SCREEN_WIDTH) - 40, SCREEN_HEIGHT - 136, 36, 36, qtrue, STYLE_NORMAL, 0.19f, colorWhite, 13, CG_DrawPowerUps);
	hud->objectives       = CG_getComponent(8, SCREEN_HEIGHT - 136, 36, 36, qtrue, STYLE_NORMAL, 0.19f, colorWhite, 14, CG_DrawObjectiveStatus);
	hud->hudhead          = CG_getComponent(44, SCREEN_HEIGHT - 92, 62, 80, qtrue, STYLE_NORMAL, 0.19f, colorWhite, 15, CG_DrawPlayerStatusHead);
	hud->cursorhints      = CG_getComponent(Ccg_WideX(SCREEN_WIDTH) * .5f - 24, 260, 48, 48, qtrue, STYLE_NORMAL, 0.19f, colorWhite, 16, CG_DrawCursorhint_f);
	hud->weaponstability  = CG_getComponent(50, 208, 10, 64, qtrue, STYLE_NORMAL, 0.19f, colorWhite, 17, CG_DrawWeapStability_f);
	hud->livesleft        = CG_getComponent(4, 360, 48, 24, qtrue, STYLE_NORMAL, 0.19f, colorWhite, 18, CG_DrawLivesLeft);
	hud->roundtimer       = CG_getComponent(706, 152, 52, 14, qtrue, STYLE_NORMAL, 0.19f, colorWhite, 19, CG_DrawRoundTimer);
	hud->reinforcement    = CG_getComponent(Ccg_WideX(SCREEN_WIDTH) - 60, SCREEN_HEIGHT - 70, 52, 14, qfalse, STYLE_NORMAL, 0.19f, colorLtBlue, 20, CG_DrawRespawnTimer);
	hud->spawntimer       = CG_getComponent(Ccg_WideX(SCREEN_WIDTH) - 60, SCREEN_HEIGHT - 60, 52, 14, qfalse, STYLE_NORMAL, 0.19f, colorRed, 21, CG_DrawSpawnTimer);
	hud->localtime        = CG_getComponent(706, 168, 52, 14, qtrue, STYLE_NORMAL, 0.19f, HUD_Text, 22, CG_DrawLocalTime);
	hud->votetext         = CG_getComponent(8, 224, 0, 0, qtrue, STYLE_NORMAL, 0.22f, colorWhite, 23, CG_DrawVote);
	hud->spectatortext    = CG_getComponent(8, 188, 0, 0, qtrue, STYLE_NORMAL, 0.22f, colorWhite, 24, CG_DrawSpectatorMessage);
	hud->limbotext        = CG_getComponent(8, 164, 0, 0, qtrue, STYLE_NORMAL, 0.22f, colorWhite, 25, CG_DrawLimboMessage);
	hud->followtext       = CG_getComponent(8, 164, 0, 0, qtrue, STYLE_NORMAL, 0.22f, colorWhite, 26, CG_DrawFollow);
	hud->demotext         = CG_getComponent(10, 9, 0, 0, qtrue, STYLE_SIMPLE, 0.22f, colorRed, 27, CG_DrawDemoMessage);
	hud->missilecamera    = CG_getComponent(4, 120, 160, 120, qtrue, STYLE_NORMAL, 1, colorWhite, 28, CG_DrawMissileCamera);
	hud->sprinttext       = CG_getComponent(20, SCREEN_HEIGHT - 96, 0, 0, qfalse, STYLE_NORMAL, 0.25f, colorWhite, 29, CG_DrawPlayerSprint);
	hud->breathtext       = CG_getComponent(20, SCREEN_HEIGHT - 96, 0, 0, qfalse, STYLE_NORMAL, 0.25, colorWhite, 30, CG_DrawPlayerBreath);
	hud->weaponchargetext = CG_getComponent(Ccg_WideX(SCREEN_WIDTH) - 16, SCREEN_HEIGHT - 96, 0, 0, qfalse, STYLE_NORMAL, 0.25f, colorWhite, 31, CG_DrawWeaponCharge);
	hud->fps              = CG_getComponent(706, 184, 52, 14, qtrue, STYLE_NORMAL, 0.19f, HUD_Text, 32, CG_DrawFPS);
	hud->snapshot         = CG_getComponent(706, 305, 52, 38, qfalse, STYLE_NORMAL, 0.19f, HUD_Text, 33, CG_DrawSnapshot);
	hud->ping             = CG_getComponent(706, 200, 52, 14, qtrue, STYLE_NORMAL, 0.19f, HUD_Text, 34, CG_DrawPing);
	hud->speed            = CG_getComponent(706, 275, 52, 14, qtrue, STYLE_NORMAL, 0.19f, HUD_Text, 35, CG_DrawSpeed);
	hud->lagometer        = CG_getComponent(706, 216, 52, 52, qtrue, STYLE_NORMAL, 0.19f, HUD_Text, 36, CG_DrawLagometer);
	hud->disconnect       = CG_getComponent(706, 216, 52, 52, qtrue, STYLE_NORMAL, 0.19f, colorWhite, 37, CG_DrawDisconnect);
}

/**
 * @brief CG_getHudByNumber
 * @param[in] number
 * @return
 */
static hudStucture_t *CG_getHudByNumber(int number)
{
	int           i;
	hudStucture_t *hud;

	for (i = 0; i < hudCount; i++)
	{
		hud = &hudlist[i];

		if (hud->hudnumber == number)
		{
			return hud;
		}
	}

	return NULL;
}

static int QDECL CG_HudComponentSort(const void *a, const void *b)
{
	return ((*(hudComponent_t **) a)->offset - (*(hudComponent_t **) b)->offset);
}

static void CG_HudComponentsFill(hudStucture_t *hud)
{
	int i, componentOffset;

	// setup component pointers to the components list
	for (i = 0, componentOffset = 0; hudComponentFields[i].name; i++)
	{
		if (hudComponentFields[i].isAlias)
		{
			continue;
		}
		hud->components[componentOffset++] = (hudComponent_t *)((char * )hud + hudComponentFields[i].offset);
	}
	// sort the components by their offset
	qsort(hud->components, sizeof(hud->components) / sizeof(hudComponent_t *), sizeof(hudComponent_t *), CG_HudComponentSort);
}

/**
 * @brief CG_isHudNumberAvailable checks if the hud by said number is available for use, 0 to 2 are forbidden.
 * @param[in] number
 * @return
 */
static qboolean CG_isHudNumberAvailable(int number)
{
	// values from 0 to 2 are used by the default hud's
	if (number <= 0 || number >= MAXHUDS)
	{
		Com_Printf(S_COLOR_RED "CG_isHudNumberAvailable: invalid HUD number %i, allowed values: 1 - %i\n", number, MAXHUDS);
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief CG_addHudToList
 * @param[in] hud
 */
static hudStucture_t *CG_addHudToList(hudStucture_t *hud)
{
	hudStucture_t *out = NULL;

	hudlist[hudCount] = *hud;
	out               = &hudlist[hudCount];
	hudCount++;

	CG_HudComponentsFill(out);

	return out;
}

//  HUD SCRIPT FUNCTIONS BELLOW

/**
 * @brief CG_HUD_ParseError
 * @param[in] handle
 * @param[in] format
 * @return
 */
static qboolean CG_HUD_ParseError(int handle, const char *format, ...)
{
	int         line;
	char        filename[MAX_QPATH];
	va_list     argptr;
	static char string[4096];

	va_start(argptr, format);
	Q_vsnprintf(string, sizeof(string), format, argptr);
	va_end(argptr);

	filename[0] = '\0';
	line        = 0;
	trap_PC_SourceFileAndLine(handle, filename, &line);

	Com_Printf(S_COLOR_RED "ERROR: %s, line %d: %s\n", filename, line, string);

	trap_PC_FreeSource(handle);

	return qfalse;
}

/**
 * @brief CG_RectParse
 * @param[in] handle
 * @param[in,out] r
 * @return
 */
static qboolean CG_RectParse(int handle, rectDef_t *r)
{
	float      x = 0;
	pc_token_t peakedToken;

	if (!PC_PeakToken(handle, &peakedToken))
	{
		return qfalse;
	}

	if (peakedToken.string[0] == '(')
	{
		if (!trap_PC_ReadToken(handle, &peakedToken))
		{
			return qfalse;
		}
	}

	if (PC_Float_Parse(handle, &x))
	{
		r->x = Ccg_WideX(x);
		if (PC_Float_Parse(handle, &r->y))
		{
			if (PC_Float_Parse(handle, &r->w))
			{
				if (PC_Float_Parse(handle, &r->h))
				{
					return qtrue;
				}
			}
		}
	}

	if (!PC_PeakToken(handle, &peakedToken))
	{
		return qfalse;
	}

	if (peakedToken.string[0] == ')')
	{
		if (!trap_PC_ReadToken(handle, &peakedToken))
		{
			return qfalse;
		}
	}

	return qfalse;
}

static qboolean CG_Vec4Parse(int handle, vec4_t v)
{
	float      r, g, b, a = 0;
	pc_token_t peakedToken;

	if (!PC_PeakToken(handle, &peakedToken))
	{
		return qfalse;
	}

	if (peakedToken.string[0] == '(')
	{
		if (!trap_PC_ReadToken(handle, &peakedToken))
		{
			return qfalse;
		}
	}

	if (PC_Float_Parse(handle, &r))
	{
		if (PC_Float_Parse(handle, &g))
		{
			if (PC_Float_Parse(handle, &b))
			{
				if (PC_Float_Parse(handle, &a))
				{
					v[0] = r;
					v[1] = g;
					v[2] = b;
					v[3] = a;
					return qtrue;
				}
			}
		}
	}

	if (!PC_PeakToken(handle, &peakedToken))
	{
		return qfalse;
	}

	if (peakedToken.string[0] == ')')
	{
		if (!trap_PC_ReadToken(handle, &peakedToken))
		{
			return qfalse;
		}
	}

	return qfalse;
}

/**
 * @brief CG_ParseHudComponent
 * @param[in] handle
 * @param[in] comp
 * @return
 */
static qboolean CG_ParseHudComponent(int handle, hudComponent_t *comp)
{
	//PC_Rect_Parse
	if (!CG_RectParse(handle, &comp->location))
	{
		return qfalse;
	}

	if (!PC_Int_Parse(handle, &comp->style))
	{
		return qfalse;
	}

	if (!PC_Int_Parse(handle, &comp->visible))
	{
		return qfalse;
	}

	// Optional scale and color
	pc_token_t token;
	if (!trap_PC_ReadToken(handle, &token))
	{
		return qfalse;
	}
	if (token.type == TT_NUMBER)
	{
		trap_PC_UnReadToken(handle);
		if (!PC_Float_Parse(handle, &comp->scale))
		{
			return qfalse;
		}

		if (!CG_Vec4Parse(handle, comp->color))
		{
			return qfalse;
		}
	}
	else
	{
		trap_PC_UnReadToken(handle);
	}

	return qtrue;
}

/**
 * @brief CG_ParseHUD
 * @param[in] handle
 * @return
 */
static qboolean CG_ParseHUD(int handle)
{
	int           i, componentOffset = 0;
	pc_token_t    token;
	hudStucture_t temphud;
	hudStucture_t *hud;
	qboolean      loadDefaults = qtrue;

	if (!trap_PC_ReadToken(handle, &token) || Q_stricmp(token.string, "{"))
	{
		return CG_HUD_ParseError(handle, "expected '{'");
	}

	if (!trap_PC_ReadToken(handle, &token))
	{
		return CG_HUD_ParseError(handle, "Error while parsing hud");
	}

	// if the first parameter in the hud definition is a "no-defaults" line then no default values are set
	// and the hud is plain (everything is hidden and no positions are set)
	if (!Q_stricmp(token.string, "no-defaults"))
	{
		loadDefaults = qfalse;
	}
	else
	{
		trap_PC_UnReadToken(handle);
	}

	// reset all the components, and set the offset value to 999 for sorting
	Com_Memset(&temphud, 0, sizeof(hudStucture_t));

	if (loadDefaults)
	{
		CG_setDefaultHudValues(&temphud);
	}
	else
	{
		for (i = 0; hudComponentFields[i].name; i++)
		{
			hudComponent_t *component = (hudComponent_t *)((char * )&temphud + hudComponentFields[i].offset);
			component->offset = 999;
		}
	}

	componentOffset = 0;
	while (qtrue)
	{
		if (!trap_PC_ReadToken(handle, &token))
		{
			break;
		}

		if (token.string[0] == '}')
		{
			break;
		}

		if (!Q_stricmp(token.string, "hudnumber"))
		{
			if (!PC_Int_Parse(handle, &temphud.hudnumber))
			{
				return CG_HUD_ParseError(handle, "expected integer value for hudnumber");
			}

			continue;
		}

		for (i = 0; hudComponentFields[i].name; i++)
		{
			if (!Q_stricmp(token.string, hudComponentFields[i].name))
			{
				hudComponent_t *component = (hudComponent_t *)((char * )&temphud + hudComponentFields[i].offset);
				component->offset = componentOffset++;
				component->draw   = hudComponentFields[i].draw;
				if (!CG_ParseHudComponent(handle, component))
				{
					return CG_HUD_ParseError(handle, "expected %s", hudComponentFields[i].name);
				}
				break;
			}
		}

		if (!hudComponentFields[i].name)
		{
			return CG_HUD_ParseError(handle, "unexpected token: %s", token.string);
		}
	}

	// check that the hudnumber value was set
	if (!CG_isHudNumberAvailable(temphud.hudnumber))
	{
		return CG_HUD_ParseError(handle, "Invalid hudnumber value: %i", temphud.hudnumber);
	}

	hud = CG_getHudByNumber(temphud.hudnumber);

	if (!hud)
	{
		CG_addHudToList(&temphud);
		Com_Printf("...properties for hud %i have been read.\n", temphud.hudnumber);
	}
	else
	{
		Com_Memcpy(hud, &temphud, sizeof(temphud));
		CG_HudComponentsFill(hud);
		Com_Printf("...properties for hud %i have been updated.\n", temphud.hudnumber);
	}

	return qtrue;
}

static qboolean CG_ReadHudFile(const char *filename)
{
	pc_token_t token;
	int        handle;

	handle = trap_PC_LoadSource(filename);

	if (!handle)
	{
		return qfalse;
	}

	if (!trap_PC_ReadToken(handle, &token) || Q_stricmp(token.string, "hudDef"))
	{
		return CG_HUD_ParseError(handle, "expected 'hudDef'");
	}

	if (!trap_PC_ReadToken(handle, &token) || Q_stricmp(token.string, "{"))
	{
		return CG_HUD_ParseError(handle, "expected '{'");
	}

	while (1)
	{
		if (!trap_PC_ReadToken(handle, &token))
		{
			break;
		}

		if (token.string[0] == '}')
		{
			break;
		}

		if (!Q_stricmp(token.string, "hud"))
		{
			if (!CG_ParseHUD(handle))
			{
				return qfalse;
			}
		}
		else
		{
			return CG_HUD_ParseError(handle, "unknown token '%s'", token.string);
		}
	}

	trap_PC_FreeSource(handle);

	return qtrue;
}

/**
 * @brief CG_ReadHudScripts
 */
void CG_ReadHudScripts(void)
{
	if (!CG_ReadHudFile("ui/huds.hud"))
	{
		Com_Printf("^1ERROR while reading hud file\n");
	}

	// This needs to be a .dat file to go around the file extension restrictions of the engine.
	CG_ReadHudFile("hud.dat");

	Com_Printf("...hud count: %i\n", hudCount);
}

// HUD DRAWING FUNCTIONS BELLOW

/**
 * @brief CG_DrawPicShadowed
 * @param[in] x
 * @param[in] y
 * @param[in] w
 * @param[in] h
 * @param[in] icon
 */
static void CG_DrawPicShadowed(float x, float y, float w, float h, qhandle_t icon)
{
	trap_R_SetColor(colorBlack);
	CG_DrawPic(x + 2, y + 2, w, h, icon);
	trap_R_SetColor(NULL);
	CG_DrawPic(x, y, w, h, icon);
}

/**
 * @brief CG_DrawPlayerStatusHead
 * @param[in] comp
 */
static void CG_DrawPlayerStatusHead(hudComponent_t *comp)
{
	hudHeadAnimNumber_t anim           = cg.idleAnim;
	bg_character_t      *character     = CG_CharacterForPlayerstate(&cg.snap->ps);
	bg_character_t      *headcharacter = BG_GetCharacter(cgs.clientinfo[cg.snap->ps.clientNum].team, cgs.clientinfo[cg.snap->ps.clientNum].cls);
	qhandle_t           painshader     = 0;
	rectDef_t           *headRect      = &comp->location;

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		return;
	}

	if (cg.snap->ps.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	if (cg.weaponFireTime > 500)
	{
		anim = HD_ATTACK;
	}
	else if (cg.time - cg.lastFiredWeaponTime < 500)
	{
		anim = HD_ATTACK_END;
	}
	else if (cg.time - cg.painTime < (character->hudheadanimations[HD_PAIN].numFrames * character->hudheadanimations[HD_PAIN].frameLerp))
	{
		anim = HD_PAIN;
	}
	else if (cg.time > cg.nextIdleTime)
	{
		cg.nextIdleTime = cg.time + 7000 + rand() % 1000;
		if (cg.snap->ps.stats[STAT_HEALTH] < 40)
		{
			cg.idleAnim = (hudHeadAnimNumber_t)((rand() % (HD_DAMAGED_IDLE3 - HD_DAMAGED_IDLE2 + 1)) + HD_DAMAGED_IDLE2);
		}
		else
		{
			cg.idleAnim = (hudHeadAnimNumber_t)((rand() % (HD_IDLE8 - HD_IDLE2 + 1)) + HD_IDLE2);
		}

		cg.lastIdleTimeEnd = cg.time + character->hudheadanimations[cg.idleAnim].numFrames * character->hudheadanimations[cg.idleAnim].frameLerp;
	}

	if (cg.snap->ps.stats[STAT_HEALTH] < 5)
	{
		painshader = cgs.media.hudDamagedStates[3];
	}
	else if (cg.snap->ps.stats[STAT_HEALTH] < 20)
	{
		painshader = cgs.media.hudDamagedStates[2];
	}
	else if (cg.snap->ps.stats[STAT_HEALTH] < 40)
	{
		painshader = cgs.media.hudDamagedStates[1];
	}
	else if (cg.snap->ps.stats[STAT_HEALTH] < 60)
	{
		painshader = cgs.media.hudDamagedStates[0];
	}

	if (cg.time > cg.lastIdleTimeEnd)
	{
		if (cg.snap->ps.stats[STAT_HEALTH] < 40)
		{
			cg.idleAnim = HD_DAMAGED_IDLE1;
		}
		else
		{
			cg.idleAnim = HD_IDLE1;
		}
	}

	CG_DrawPlayerHead(headRect, character, headcharacter, 180, 0, (cg.snap->ps.eFlags & EF_HEADSHOT) ? qfalse : qtrue, anim, painshader, cgs.clientinfo[cg.snap->ps.clientNum].rank, qfalse, cgs.clientinfo[cg.snap->ps.clientNum].team);
}

/**
 * @brief Get the current ammo and/or clip count of the holded weapon (if using ammo).
 * @param[out] ammo - the number of ammo left (in the current clip if using clip)
 * @param[out] clips - the total ammount of ammo in all clips (if using clip)
 * @param[out] akimboammo - the number of ammo left in the second pistol of akimbo (if using akimbo)
 */
void CG_PlayerAmmoValue(int *ammo, int *clips, int *akimboammo)
{
	centity_t     *cent;
	playerState_t *ps;
	weapon_t      weap;

	*ammo = *clips = *akimboammo = -1;

	if (cg.snap->ps.clientNum == cg.clientNum)
	{
		cent = &cg.predictedPlayerEntity;
	}
	else
	{
		cent = &cg_entities[cg.snap->ps.clientNum];
	}
	ps = &cg.snap->ps;

	weap = (weapon_t)cent->currentState.weapon;

	if (!IS_VALID_WEAPON(weap))
	{
		return;
	}

	// some weapons don't draw ammo count
	if (!GetWeaponTableData(weap)->useAmmo)
	{
		return;
	}

	if (BG_PlayerMounted(cg.snap->ps.eFlags))
	{
		return;
	}

	// total ammo in clips, grenade launcher is not a clip weapon but show the clip anyway
	if (GetWeaponTableData(weap)->useClip || (weap == WP_M7 || weap == WP_GPG40))
	{
		// current reserve
		*clips = cg.snap->ps.ammo[GetWeaponTableData(weap)->ammoIndex];

		// current clip
		*ammo = ps->ammoclip[GetWeaponTableData(weap)->clipIndex];
	}
	else
	{
		// some weapons don't draw ammo clip count text
		*ammo = ps->ammoclip[GetWeaponTableData(weap)->clipIndex] + cg.snap->ps.ammo[GetWeaponTableData(weap)->ammoIndex];
	}

	// akimbo ammo clip
	if (GetWeaponTableData(weap)->attributes & WEAPON_ATTRIBUT_AKIMBO)
	{
		*akimboammo = ps->ammoclip[GetWeaponTableData(GetWeaponTableData(weap)->akimboSideArm)->clipIndex];
	}
	else
	{
		*akimboammo = -1;
	}

	if (weap == WP_LANDMINE)
	{
		if (!cgs.gameManager)
		{
			*ammo = 0;
		}
		else
		{
			if (cgs.clientinfo[ps->clientNum].team == TEAM_AXIS)
			{
				*ammo = cgs.gameManager->currentState.otherEntityNum;
			}
			else
			{
				*ammo = cgs.gameManager->currentState.otherEntityNum2;
			}
		}
	}
}

/**
 * @brief Check if we are underwater
 * @details This check has changed to make it work for spectators following another player.
 * That's why ps.stats[STAT_AIRLEFT] has been added..
 *
 * While following high-pingers, You sometimes see the breathbar, even while they are not submerged..
 * So we check for underwater status differently when we are following others.
 * (It doesn't matter to do a more complex check for spectators.. they are not playing)
 * @return
 */
static qboolean CG_CheckPlayerUnderwater()
{
	if (cg.snap->ps.pm_flags & PMF_FOLLOW)
	{
		vec3_t origin;

		VectorCopy(cg.snap->ps.origin, origin);
		origin[2] += 36;
		return (qboolean)(CG_PointContents(origin, cg.snap->ps.clientNum) & CONTENTS_WATER);
	}

	return cg.snap->ps.stats[STAT_AIRLEFT] < HOLDBREATHTIME;
}

vec4_t bgcolor = { 1.f, 1.f, 1.f, .3f };    // bars backgound

/**
 * @brief CG_DrawPlayerHealthBar
 * @param[in] rect
 */
static void CG_DrawPlayerHealthBar(hudComponent_t *comp)
{
	vec4_t colour;
	int    flags = 1 | 4 | 16 | 64;
	float  frac;

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		return;
	}

	if (cg.snap->ps.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	CG_ColorForHealth(colour);
	colour[3] = 0.5f;

	frac = cg.snap->ps.stats[STAT_HEALTH] / (float) cg.snap->ps.stats[STAT_MAX_HEALTH];

	CG_FilledBar(comp->location.x, comp->location.y + (comp->location.h * 0.1f), comp->location.w, comp->location.h * 0.84f, colour, NULL, bgcolor, frac, flags);

	trap_R_SetColor(NULL);
	CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, cgs.media.hudSprintBar);
	CG_DrawPic(comp->location.x, comp->location.y + comp->location.h + 4, comp->location.w, comp->location.w, cgs.media.hudHealthIcon);
}

/**
 * @brief CG_DrawStaminaBar
 * @param[in] rect
 */
static void CG_DrawStaminaBar(hudComponent_t *comp)
{
	vec4_t colour = { 0.1f, 1.0f, 0.1f, 0.5f };
	vec_t  *color = colour;
	int    flags  = 1 | 4 | 16 | 64;
	float  frac   = cg.snap->ps.stats[STAT_SPRINTTIME] / (float)SPRINTTIME;

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		return;
	}

	if (cg.snap->ps.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	if (CG_CheckPlayerUnderwater())
	{
		return;
	}

	if (cg.snap->ps.powerups[PW_ADRENALINE])
	{
		if (cg.snap->ps.pm_flags & PMF_FOLLOW)
		{
			Vector4Average(colour, colorWhite, (float)sin(cg.time * .005), colour);
		}
		else
		{
			float msec = cg.snap->ps.powerups[PW_ADRENALINE] - cg.time;

			if (msec >= 0)
			{
				Vector4Average(colour, colorMdRed, (float)(.5 + sin(.2 * sqrt((double)msec) * M_TAU_F) * .5), colour);
			}
		}
	}
	else
	{
		color[0] = 1.0f - frac;
		color[1] = frac;
	}

	CG_FilledBar(comp->location.x, comp->location.y + (comp->location.h * 0.1f), comp->location.w, comp->location.h * 0.84f, color, NULL, bgcolor, frac, flags);

	trap_R_SetColor(NULL);
	CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, cgs.media.hudSprintBar);
	CG_DrawPic(comp->location.x, comp->location.y + comp->location.h + 4, comp->location.w, comp->location.w, cgs.media.hudSprintIcon);
}

/**
 * @brief Draw the breath bar
 * @param[in] rect
 */
static void CG_DrawBreathBar(hudComponent_t *comp)
{
	static vec4_t colour = { 0.1f, 0.1f, 1.0f, 0.5f };
	vec_t         *color = colour;
	int           flags  = 1 | 4 | 16 | 64;
	float         frac   = cg.snap->ps.stats[STAT_AIRLEFT] / (float)HOLDBREATHTIME;

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		return;
	}

	if (cg.snap->ps.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	if (!CG_CheckPlayerUnderwater())
	{
		return;
	}

	color[0] = 1.0f - frac;
	color[2] = frac;

	CG_FilledBar(comp->location.x, comp->location.y + (comp->location.h * 0.1f), comp->location.w, comp->location.h * 0.84f, color, NULL, bgcolor, frac, flags);

	trap_R_SetColor(NULL);
	CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, cgs.media.hudSprintBar);
	CG_DrawPic(comp->location.x, comp->location.y + comp->location.h + 4, comp->location.w, comp->location.w, cgs.media.waterHintShader);
}

/**
 * @brief Draw weapon recharge bar
 * @param rect
 */
static void CG_DrawWeapRecharge(hudComponent_t *comp)
{
	float    barFrac, chargeTime;
	int      flags  = 1 | 4 | 16;
	qboolean charge = qtrue;
	vec4_t   color;

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		return;
	}

	if (cg.snap->ps.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	// Draw power bar
	switch (cg.snap->ps.stats[STAT_PLAYER_CLASS])
	{
	case PC_ENGINEER:
		chargeTime = cg.engineerChargeTime[cg.snap->ps.persistant[PERS_TEAM] - 1];
		break;
	case PC_MEDIC:
		chargeTime = cg.medicChargeTime[cg.snap->ps.persistant[PERS_TEAM] - 1];
		break;
	case PC_FIELDOPS:
		chargeTime = cg.fieldopsChargeTime[cg.snap->ps.persistant[PERS_TEAM] - 1];
		break;
	case PC_COVERTOPS:
		chargeTime = cg.covertopsChargeTime[cg.snap->ps.persistant[PERS_TEAM] - 1];
		break;
	default:
		chargeTime = cg.soldierChargeTime[cg.snap->ps.persistant[PERS_TEAM] - 1];
		break;
	}

	// display colored charge bar if charge bar isn't full enough
	if (GetWeaponTableData(cg.predictedPlayerState.weapon)->attributes & WEAPON_ATTRIBUT_CHARGE_TIME)
	{
		int index = BG_IsSkillAvailable(cgs.clientinfo[cg.clientNum].skill,
		                                GetWeaponTableData(cg.predictedPlayerState.weapon)->skillBased,
		                                GetWeaponTableData(cg.predictedPlayerState.weapon)->chargeTimeSkill);

		float coeff = GetWeaponTableData(cg.predictedPlayerState.weapon)->chargeTimeCoeff[index];

		if (cg.time - cg.snap->ps.classWeaponTime < chargeTime * coeff)
		{
			charge = qfalse;
		}
	}
	else if ((cg.predictedPlayerState.eFlags & EF_ZOOMING || cg.predictedPlayerState.weapon == WP_BINOCULARS)
	         && cgs.clientinfo[cg.snap->ps.clientNum].cls == PC_FIELDOPS)
	{
		int index = BG_IsSkillAvailable(cgs.clientinfo[cg.clientNum].skill,
		                                GetWeaponTableData(WP_ARTY)->skillBased,
		                                GetWeaponTableData(WP_ARTY)->chargeTimeSkill);

		float coeff = GetWeaponTableData(WP_ARTY)->chargeTimeCoeff[index];

		if (cg.time - cg.snap->ps.classWeaponTime < chargeTime * coeff)
		{
			charge = qfalse;
		}
	}

	barFrac = (cg.time - cg.snap->ps.classWeaponTime) / chargeTime; // FIXME: potential DIV 0 when charge times are set to 0!

	if (barFrac > 1.0f)
	{
		barFrac = 1.0f;
	}

	if (!charge)
	{
		color[0] = 1.0f;
		color[1] = color[2] = 0.1f;
		color[3] = 0.5f;
	}
	else
	{
		color[0] = color[1] = 1.0f;
		color[2] = barFrac;
		color[3] = 0.25f + barFrac * 0.5f;
	}

	CG_FilledBar(comp->location.x, comp->location.y + (comp->location.h * 0.1f), comp->location.w, comp->location.h * 0.84f, color, NULL, bgcolor, barFrac, flags);

	trap_R_SetColor(NULL);
	CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, cgs.media.hudSprintBar);

	if (cg.snap->ps.stats[STAT_PLAYER_CLASS] == PC_FIELDOPS)
	{
		if (cg.snap->ps.ammo[WP_ARTY] & NO_AIRSTRIKE && cg.snap->ps.ammo[WP_ARTY] & NO_ARTILLERY)
		{
			trap_R_SetColor(colorRed);
		}
		else if (cg.snap->ps.ammo[WP_ARTY] & NO_AIRSTRIKE)
		{
			trap_R_SetColor(colorOrange);
		}
		else if (cg.snap->ps.ammo[WP_ARTY] & NO_ARTILLERY)
		{
			trap_R_SetColor(colorYellow);
		}
		CG_DrawPic(comp->location.x + (comp->location.w * 0.25f) - 1, comp->location.y + comp->location.h + 4, (comp->location.w * 0.5f) + 2, comp->location.w + 2, cgs.media.hudPowerIcon);
		trap_R_SetColor(NULL);
	}
	else
	{
		CG_DrawPic(comp->location.x + (comp->location.w * 0.25f) - 1, comp->location.y + comp->location.h + 4, (comp->location.w * 0.5f) + 2, comp->location.w + 2, cgs.media.hudPowerIcon);
	}
}

/**
 * @brief CG_DrawGunIcon
 * @param[in] location
 */
static void CG_DrawGunIcon(hudComponent_t *comp)
{
	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		return;
	}

	if (cg.snap->ps.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	// Draw weapon icon and overheat bar
	CG_DrawWeapHeat(&comp->location, HUD_HORIZONTAL);

	// drawn the common white icon, usage of mounted weapons don't change cg.snap->ps.weapon for real
	if (BG_PlayerMounted(cg.snap->ps.eFlags))
	{
		CG_DrawPlayerWeaponIcon(&comp->location, qtrue, ITEM_ALIGN_RIGHT, &comp->color);
		return;
	}

	if (
#ifdef FEATURE_MULTIVIEW
		cg.mvTotalClients < 1 &&
#endif
		cg_drawWeaponIconFlash.integer == 0)
	{
		CG_DrawPlayerWeaponIcon(&comp->location, qtrue, ITEM_ALIGN_RIGHT, &comp->color);
	}
	else
	{
		int ws =
#ifdef FEATURE_MULTIVIEW
			(cg.mvTotalClients > 0) ? cgs.clientinfo[cg.snap->ps.clientNum].weaponState :
#endif
			BG_simpleWeaponState(cg.snap->ps.weaponstate);

		CG_DrawPlayerWeaponIcon(&comp->location, (qboolean)(ws != WSTATE_IDLE), ITEM_ALIGN_RIGHT, ((ws == WSTATE_SWITCH || ws == WSTATE_RELOAD) ? &colorYellow : (ws == WSTATE_FIRE) ? &colorRed : &comp->color));
	}
}

/**
 * @brief CG_DrawAmmoCount
 * @param[in] x
 * @param[in] y
 */
static void CG_DrawAmmoCount(hudComponent_t *comp)
{
	int  value, value2, value3;
	char buffer[16];

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		return;
	}

	if (cg.snap->ps.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	// Draw ammo
	CG_PlayerAmmoValue(&value, &value2, &value3);

	// .25f
	if (value3 >= 0)
	{
		Com_sprintf(buffer, sizeof(buffer), "%i|%i/%i", value3, value, value2);
		CG_Text_Paint_Ext(comp->location.x - CG_Text_Width_Ext(buffer, comp->scale, 0, &cgs.media.limboFont1), comp->location.y, comp->scale, comp->scale, comp->color, buffer, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
	}
	else if (value2 >= 0)
	{
		Com_sprintf(buffer, sizeof(buffer), "%i/%i", value, value2);
		CG_Text_Paint_Ext(comp->location.x - CG_Text_Width_Ext(buffer, comp->scale, 0, &cgs.media.limboFont1), comp->location.y, comp->scale, comp->scale, comp->color, buffer, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
	}
	else if (value >= 0)
	{
		Com_sprintf(buffer, sizeof(buffer), "%i", value);
		CG_Text_Paint_Ext(comp->location.x - CG_Text_Width_Ext(buffer, comp->scale, 0, &cgs.media.limboFont1), comp->location.y, comp->scale, comp->scale, comp->color, buffer, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
	}
}

/**
 * @brief CG_DrawSkillBar
 * @param[in] x
 * @param[in] y
 * @param[in] w
 * @param[in] h
 * @param[in] skillLvl
 */
static void CG_DrawSkillBar(float x, float y, float w, float h, int skillLvl, skillType_t skill)
{
	int    i;
	float  blockheight = (h - 4) / (float)(NUM_SKILL_LEVELS - 1);
	float  draw_y      = y + h - blockheight;
	vec4_t colour;
	float  x1, y1, w1, h1;

	for (i = 1; i < NUM_SKILL_LEVELS; i++)
	{

		if (GetSkillTableData(skill)->skillLevels[i] < 0)
		{
			Vector4Set(colour, 1.f, 0.f, 0.f, .15f);
		}
		else if (skillLvl >= i)
		{
			Vector4Set(colour, 0.f, 0.f, 0.f, .4f);
		}
		else
		{
			Vector4Set(colour, 1.f, 1.f, 1.f, .15f);
		}

		CG_FillRect(x, draw_y, w, blockheight, colour);

		// draw the star only if the skill is reach and available
		if (skillLvl >= i && GetSkillTableData(skill)->skillLevels[i] >= 0)
		{
			x1 = x;
			y1 = draw_y;
			w1 = w;
			h1 = blockheight;
			CG_AdjustFrom640(&x1, &y1, &w1, &h1);

			trap_R_DrawStretchPic(x1, y1, w1, h1, 0, 0, 1.f, 0.5f, cgs.media.limboStar_roll);
		}

		CG_DrawRect_FixedBorder(x, draw_y, w, blockheight, 1, colorBlack);
		draw_y -= (blockheight + 1);
	}
}

/**
 * @brief CG_ClassSkillForPosition
 * @param[in] ci
 * @param[in] pos
 * @return
 */
skillType_t CG_ClassSkillForPosition(clientInfo_t *ci, int pos)
{
	switch (pos)
	{
	case 0:
		return BG_ClassSkillForClass(ci->cls);
	case 1:
		return SK_BATTLE_SENSE;
	case 2:
		// draw soldier level if using a heavy weapon instead of light weapons icon
		if ((BG_PlayerMounted(cg.snap->ps.eFlags) || GetWeaponTableData(cg.snap->ps.weapon)->skillBased == SK_HEAVY_WEAPONS) && ci->cls != PC_SOLDIER)
		{
			return SK_HEAVY_WEAPONS;
		}
		return SK_LIGHT_WEAPONS;
	default:
		break;
	}

	return SK_BATTLE_SENSE;
}

/**
 * @brief CG_DrawPlayerHealth
 * @param[in] x
 * @param[in] y
 */
static void CG_DrawPlayerHealth(hudComponent_t *comp)
{
	const char *str  = va("%i", cg.snap->ps.stats[STAT_HEALTH]);
	float      scale = comp->scale;
	float      w     = CG_Text_Width_Ext(str, scale, 0, &cgs.media.limboFont1);
	vec4_t     color;

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		return;
	}

	if (cg.snap->ps.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	if (cg_healthDynamicColor.integer)
	{
		int    clientNum = cg.snap->ps.clientNum;
		int    cls       = cg.snap->ps.stats[STAT_PLAYER_CLASS];
		team_t team      = cg.snap->ps.persistant[PERS_TEAM];
		int    maxHealth = CG_GetPlayerMaxHealth(clientNum, cls, team);
		CG_GetColorForHealth(cg.snap->ps.stats[STAT_HEALTH], color);
		color[3] = comp->color[3];
	}
	else
	{
		Vector4Copy(comp->color, color);
	}

	CG_Text_Paint_Ext(comp->location.x - w, comp->location.y, scale, scale, color, str, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
	CG_Text_Paint_Ext(comp->location.x + 2, comp->location.y, scale - 0.05f, scale - 0.05f, comp->color, "HP", 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
}

/**
 * @brief CG_DrawPlayerSprint
 * @param[in] x
 * @param[in] y
 */
static void CG_DrawPlayerSprint(hudComponent_t *comp)
{
	const char *str;
	const char *unit;
	float      scale = comp->scale;
	float      w;

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		return;
	}

	if (cg.snap->ps.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	if (CG_CheckPlayerUnderwater())
	{
		return;
	}

	if (cg.snap->ps.powerups[PW_ADRENALINE])
	{
		str  = va("%d", (cg.snap->ps.powerups[PW_ADRENALINE] - cg.time) / 1000);
		unit = "s";
	}
	else
	{
		str  = va("%.0f", (cg.snap->ps.stats[STAT_SPRINTTIME] / (float)SPRINTTIME) * 100);
		unit = "%";
	}

	w = CG_Text_Width_Ext(str, scale, 0, &cgs.media.limboFont1);

	CG_Text_Paint_Ext(comp->location.x - w, comp->location.y, scale, scale, comp->color, str, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
	CG_Text_Paint_Ext(comp->location.x + 2, comp->location.y, scale - 0.05f, scale - 0.05f, comp->color, unit, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
}

/**
 * @brief CG_DrawPlayerBreath
 * @param[in] x
 * @param[in] y
 */
static void CG_DrawPlayerBreath(hudComponent_t *comp)
{
	const char *str  = va("%.0f", (cg.snap->ps.stats[STAT_AIRLEFT] / (float)HOLDBREATHTIME) * 100);
	float      scale = comp->scale;
	float      w     = CG_Text_Width_Ext(str, scale, 0, &cgs.media.limboFont1);

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		return;
	}

	if (cg.snap->ps.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	if (!CG_CheckPlayerUnderwater())
	{
		return;
	}

	CG_Text_Paint_Ext(comp->location.x - w, comp->location.y, scale, scale, comp->color, str, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
	CG_Text_Paint_Ext(comp->location.x + 2, comp->location.y, scale - 0.05f, scale - 0.05f, comp->color, "%", 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
}

/**
 * @brief CG_DrawWeaponCharge
 * @param[in] x
 * @param[in] y
 */
static void CG_DrawWeaponCharge(hudComponent_t *comp)
{
	const char *str;
	float      scale = comp->scale;
	float      w;
	float      chargeTime;

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		return;
	}

	if (cg.snap->ps.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	switch (cg.snap->ps.stats[STAT_PLAYER_CLASS])
	{
	case PC_ENGINEER:
		chargeTime = cg.engineerChargeTime[cg.snap->ps.persistant[PERS_TEAM] - 1];
		break;
	case PC_MEDIC:
		chargeTime = cg.medicChargeTime[cg.snap->ps.persistant[PERS_TEAM] - 1];
		break;
	case PC_FIELDOPS:
		chargeTime = cg.fieldopsChargeTime[cg.snap->ps.persistant[PERS_TEAM] - 1];
		break;
	case PC_COVERTOPS:
		chargeTime = cg.covertopsChargeTime[cg.snap->ps.persistant[PERS_TEAM] - 1];
		break;
	default:
		chargeTime = cg.soldierChargeTime[cg.snap->ps.persistant[PERS_TEAM] - 1];
		break;
	}

	str = va("%.0f", MIN(((cg.time - cg.snap->ps.classWeaponTime) / chargeTime) * 100, 100));
	w   = CG_Text_Width_Ext(str, scale, 0, &cgs.media.limboFont1);


	CG_Text_Paint_Ext(comp->location.x - w, comp->location.y, scale, scale, comp->color, str, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
	CG_Text_Paint_Ext(comp->location.x + 2, comp->location.y, scale - 0.05f, scale - 0.05f, comp->color, "%", 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
}

/**
 * @brief CG_DrawSkills
 * @param[in] comp
 */
static void CG_DrawSkills(hudComponent_t *comp)
{
	playerState_t *ps = &cg.snap->ps;
	clientInfo_t  *ci = &cgs.clientinfo[ps->clientNum];
	skillType_t   skill;
	int           i;
	float         temp;

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		return;
	}

	if (cgs.gametype == GT_WOLF_LMS)
	{
		return;
	}

	if (cg.snap->ps.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	for (i = 0; i < 3; i++)
	{
		skill = CG_ClassSkillForPosition(ci, i);
		if (comp->style == STYLE_NORMAL)
		{
			CG_DrawSkillBar(i * SKILL_BAR_X_SCALE + SKILL_BAR_X, SCREEN_HEIGHT - (5 * SKILL_BAR_Y_SCALE) + SKILL_BAR_Y, SKILL_BAR_WIDTH, 4 * SKILL_ICON_SIZE, ci->skill[skill], skill);
			CG_DrawPic(i * SKILL_ICON_X_SCALE + SKILL_ICON_X, SCREEN_HEIGHT + SKILL_ICON_Y, SKILL_ICON_SIZE, SKILL_ICON_SIZE, cgs.media.skillPics[skill]);
		}
		else
		{
			int j        = 1;
			int skillLvl = 0;

			for (; j < NUM_SKILL_LEVELS; ++j)
			{
				if (BG_IsSkillAvailable(ci->skill, skill, j))
				{
					skillLvl++;
				}
			}

			temp = comp->location.y + (i * SKILL_ICON_SIZE * 1.7f);
			//CG_DrawPic
			CG_DrawPicShadowed(comp->location.x, temp, SKILL_ICON_SIZE, SKILL_ICON_SIZE, cgs.media.skillPics[skill]);
			CG_Text_Paint_Ext(comp->location.x + 3, temp + 24, comp->scale, comp->scale, comp->color, va("%i", skillLvl), 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
		}
	}
}

/**
 * @brief CG_DrawXP
 * @param[in] x
 * @param[in] y
 */
static void CG_DrawXP(hudComponent_t *comp)
{
	const char *str;
	float      w, scale;
	vec_t      *clr;

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		return;
	}

	if (cgs.gametype == GT_WOLF_LMS)
	{
		return;
	}

	if (cg.snap->ps.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	if (cg.time - cg.xpChangeTime < 1000)
	{
		clr = colorYellow;
	}
	else
	{
		clr = comp->color;
	}

	str   = va("%i", cg.snap->ps.stats[STAT_XP]);
	scale = comp->scale;
	w     = CG_Text_Width_Ext(str, scale, 0, &cgs.media.limboFont1);
	CG_Text_Paint_Ext(comp->location.x - w, comp->location.y, scale, scale, clr, str, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
	CG_Text_Paint_Ext(comp->location.x + 2, comp->location.y, scale - 0.05f, scale - 0.05f, clr, "XP", 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
}

/**
 * @brief CG_DrawRank
 * @param[in] x
 * @param[in] y
 */
static void CG_DrawRank(hudComponent_t *comp)
{
	const char    *str;
	float         w, scale;
	playerState_t *ps = &cg.snap->ps;

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		return;
	}

	if (cgs.gametype == GT_WOLF_LMS)
	{
		return;
	}

	if (cg.snap->ps.stats[STAT_HEALTH] <= 0)
	{
		return;
	}

	str   = va("%s", GetRankTableData(cgs.clientinfo[ps->clientNum].team, cgs.clientinfo[ps->clientNum].rank)->miniNames);
	scale = comp->scale;
	w     = CG_Text_Width_Ext(str, scale, 0, &cgs.media.limboFont1);
	CG_Text_Paint_Ext(comp->location.x - w, comp->location.y, scale, scale, comp->color, str, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
}

/**
 * @brief CG_DrawPowerUps
 * @param[in] rect
 */
static void CG_DrawPowerUps(hudComponent_t *comp)
{
	playerState_t *ps = &cg.snap->ps;

	if (ps->persistant[PERS_TEAM] == TEAM_SPECTATOR && !cgs.clientinfo[cg.clientNum].shoutcaster)
	{
		return;
	}

	// draw treasure icon if we have the flag
	if (ps->powerups[PW_REDFLAG] || ps->powerups[PW_BLUEFLAG])
	{
		trap_R_SetColor(NULL);
		CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, cgs.media.objectiveShader);
	}
	else if (ps->powerups[PW_OPS_DISGUISED])       // Disguised?
	{
		CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, ps->persistant[PERS_TEAM] == TEAM_AXIS ? cgs.media.alliedUniformShader : cgs.media.axisUniformShader);
		// show the class to the client
		CG_DrawPic(comp->location.x + 9, comp->location.y + 9, 18, 18, cgs.media.skillPics[BG_ClassSkillForClass((cg_entities[ps->clientNum].currentState.powerups >> PW_OPS_CLASS_1) & 7)]);
	}
	else if (ps->powerups[PW_ADRENALINE] > 0)       // adrenaline
	{
		vec4_t color = { 1.0, 0.0, 0.0, 1.0 };
		color[3] *= 0.5 + 0.5 * sin(cg.time / 150.0);
		trap_R_SetColor(color);
		CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, cgs.media.hudAdrenaline);
		trap_R_SetColor(NULL);
	}
	else if (ps->powerups[PW_INVULNERABLE] && !(ps->pm_flags & PMF_LIMBO))       // spawn shield
	{
		CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, cgs.media.spawnInvincibleShader);
	}
}

/**
 * @brief CG_DrawObjectiveStatus
 * @param[in] rect
 */
static void CG_DrawObjectiveStatus(hudComponent_t *comp)
{
	playerState_t *ps = &cg.snap->ps;

	if (ps->persistant[PERS_TEAM] == TEAM_SPECTATOR && !cgs.clientinfo[cg.clientNum].shoutcaster)
	{
		return;
	}

	// draw objective status icon
	if ((cg.flagIndicator & (1 << PW_REDFLAG) || cg.flagIndicator & (1 << PW_BLUEFLAG)) && (!cgs.clientinfo[cg.clientNum].shoutcaster || (cg.snap->ps.pm_flags & PMF_FOLLOW)))
	{
		// draw objective info icon (if teammates or enemies are carrying one)
		vec4_t color = { 1.f, 1.f, 1.f, 1.f };
		color[3] *= 0.67 + 0.33 * sin(cg.time / 200.0);
		trap_R_SetColor(color);

		if (cg.flagIndicator & (1 << PW_REDFLAG) && cg.flagIndicator & (1 << PW_BLUEFLAG))
		{
			if (cg.redFlagCounter > 0 && cg.blueFlagCounter > 0)
			{
				// both own and enemy flags stolen
				CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, cgs.media.objectiveBothTEShader);
			}
			else if (cg.redFlagCounter > 0 && !cg.blueFlagCounter)
			{
				// own flag stolen and enemy flag dropped
				CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, ps->persistant[PERS_TEAM] == TEAM_AXIS ? cgs.media.objectiveBothTDShader : cgs.media.objectiveBothDEShader);
			}
			else if (!cg.redFlagCounter && cg.blueFlagCounter > 0)
			{
				// own flag dropped and enemy flag stolen
				CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, ps->persistant[PERS_TEAM] == TEAM_ALLIES ? cgs.media.objectiveBothTDShader : cgs.media.objectiveBothDEShader);
			}
			else
			{
				// both own and enemy flags dropped
				CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, cgs.media.objectiveDroppedShader);
			}
			trap_R_SetColor(NULL);

			// display team flag
			color[3] = 1.f;
			trap_R_SetColor(color);
			CG_DrawPic(comp->location.x + comp->location.w / 2 - 20, comp->location.y + 28, 12, 8, ps->persistant[PERS_TEAM] == TEAM_AXIS ? cgs.media.axisFlag : cgs.media.alliedFlag);
			CG_DrawPic(comp->location.x + comp->location.w / 2 + 8, comp->location.y + 28, 12, 8, ps->persistant[PERS_TEAM] == TEAM_AXIS ? cgs.media.alliedFlag : cgs.media.axisFlag);
		}
		else if (cg.flagIndicator & (1 << PW_REDFLAG))
		{
			if (cg.redFlagCounter > 0)
			{
				CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, ps->persistant[PERS_TEAM] == TEAM_ALLIES ? cgs.media.objectiveTeamShader : cgs.media.objectiveEnemyShader);
			}
			else
			{
				CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, cgs.media.objectiveDroppedShader);
			}
			trap_R_SetColor(NULL);

			// display team flag
			color[3] = 1.f;
			trap_R_SetColor(color);
			CG_DrawPic(comp->location.x + comp->location.w / 2 + (ps->persistant[PERS_TEAM] == TEAM_AXIS ? 8 : -20), comp->location.y + 28, 12, 8, cgs.media.alliedFlag);
		}
		else if (cg.flagIndicator & (1 << PW_BLUEFLAG))
		{
			if (cg.blueFlagCounter > 0)
			{
				CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, ps->persistant[PERS_TEAM] == TEAM_AXIS ? cgs.media.objectiveTeamShader : cgs.media.objectiveEnemyShader);
			}
			else
			{
				CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, cgs.media.objectiveDroppedShader);
			}
			trap_R_SetColor(NULL);

			// display team flag
			color[3] = 1.f;
			trap_R_SetColor(color);
			CG_DrawPic(comp->location.x + comp->location.w / 2 + (ps->persistant[PERS_TEAM] == TEAM_ALLIES ? 8 : -20), comp->location.y + 28, 12, 8, cgs.media.axisFlag);
		}

		// display active flag counter
		if (cg.redFlagCounter > 1)
		{
			CG_Text_Paint_Ext(comp->location.x + comp->location.w / 2 + (ps->persistant[PERS_TEAM] == TEAM_ALLIES ? -16 : 12), comp->location.y + 38, 0.18, 0.18, colorWhite, va("%i", cg.redFlagCounter), 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
		}
		if (cg.blueFlagCounter > 1)
		{
			CG_Text_Paint_Ext(comp->location.x + comp->location.w / 2 + (ps->persistant[PERS_TEAM] == TEAM_AXIS ? -16 : 12), comp->location.y + 38, 0.18, 0.18, colorWhite, va("%i", cg.blueFlagCounter), 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
		}

		trap_R_SetColor(NULL);
	}
	else if (cgs.clientinfo[cg.clientNum].shoutcaster && !(cg.snap->ps.pm_flags & PMF_FOLLOW))
	{
		// simplified version for shoutcaster when not following players
		vec4_t color = { 1.f, 1.f, 1.f, 1.f };
		color[3] *= 0.67 + 0.33 * sin(cg.time / 200.0);
		trap_R_SetColor(color);

		if (cg.flagIndicator & (1 << PW_REDFLAG) && cg.flagIndicator & (1 << PW_BLUEFLAG))
		{
			if (cg.redFlagCounter > 0 && cg.blueFlagCounter > 0)
			{
				// both team stole an enemy flags
				CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, cgs.media.objectiveTeamShader);
			}
			else if ((cg.redFlagCounter > 0 && !cg.blueFlagCounter) || (!cg.redFlagCounter && cg.blueFlagCounter > 0))
			{
				// one flag stolen and the other flag dropped
				CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, cgs.media.objectiveTeamShader);
			}
			else
			{
				// both team dropped flags
				CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, cgs.media.objectiveDroppedShader);
			}
		}
		else if (cg.flagIndicator & (1 << PW_REDFLAG))
		{
			if (cg.redFlagCounter > 0)
			{
				CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, cgs.media.objectiveTeamShader);
			}
			else
			{
				CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, cgs.media.objectiveDroppedShader);
			}
		}
		else if (cg.flagIndicator & (1 << PW_BLUEFLAG))
		{
			if (cg.blueFlagCounter > 0)
			{
				CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, cgs.media.objectiveTeamShader);
			}
			else
			{
				CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, cgs.media.objectiveDroppedShader);
			}
		}
		trap_R_SetColor(NULL);

		// display team flag
		color[3] = 1.f;
		trap_R_SetColor(color);

		if (cg.flagIndicator & (1 << PW_REDFLAG))
		{
			CG_DrawPic(comp->location.x + comp->location.w / 2 + 8, comp->location.y + 28, 12, 8, cgs.media.alliedFlag);
		}

		if (cg.flagIndicator & (1 << PW_BLUEFLAG))
		{
			CG_DrawPic(comp->location.x + comp->location.w / 2 - 20, comp->location.y + 28, 12, 8, cgs.media.axisFlag);
		}

		// display active flag counter
		if (cg.redFlagCounter > 1)
		{
			CG_Text_Paint_Ext(comp->location.x + comp->location.w / 2 + 12, comp->location.y + 38, 0.18, 0.18, colorWhite, va("%i", cg.redFlagCounter), 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
		}
		if (cg.blueFlagCounter > 1)
		{
			CG_Text_Paint_Ext(comp->location.x + comp->location.w / 2 - 16, comp->location.y + 38, 0.18, 0.18, colorWhite, va("%i", cg.blueFlagCounter), 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
		}

		trap_R_SetColor(NULL);
	}
}

static int lastDemoScoreTime = 0;

/**
 * @brief CG_DrawDemoMessage
 */
static void CG_DrawDemoMessage(hudComponent_t *comp)
{
	char status[1024];
	char demostatus[128];
	char wavestatus[128];

	float x = comp->location.x, y = comp->location.y, fontScale = comp->scale;

	if (!comp->visible)
	{
		return;
	}

	if (!cl_demorecording.integer && !cl_waverecording.integer && !cg.demoPlayback)
	{
		return;
	}

	// poll for score
	if ((!lastDemoScoreTime || cg.time > lastDemoScoreTime) && !cg.demoPlayback)
	{
		trap_SendClientCommand("score");
		lastDemoScoreTime = cg.time + 5000; // 5 secs
	}

	if (comp->style == STYLE_NORMAL)
	{
		if (cl_demorecording.integer)
		{
			Com_sprintf(demostatus, sizeof(demostatus), __(" demo %s: %ik "), cl_demofilename.string, cl_demooffset.integer / 1024);
		}
		else
		{
			Q_strncpyz(demostatus, "", sizeof(demostatus));
		}

		if (cl_waverecording.integer)
		{
			Com_sprintf(wavestatus, sizeof(demostatus), __(" audio %s: %ik "), cl_wavefilename.string, cl_waveoffset.integer / 1024);
		}
		else
		{
			Q_strncpyz(wavestatus, "", sizeof(wavestatus));
		}
	}
	else
	{
		Q_strncpyz(demostatus, "", sizeof(demostatus));
		Q_strncpyz(wavestatus, "", sizeof(wavestatus));
	}

	Com_sprintf(status, sizeof(status), "%s%s%s", cg.demoPlayback ? __("REPLAY") : __("RECORD"), demostatus, wavestatus);

	CG_Text_Paint_Ext(x, y, fontScale, fontScale, cg.demoPlayback ? colorYellow : comp->color, status, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont2);
}

/**
 * @brief CG_DrawField
 * @param[in] x
 * @param[in] y
 * @param[in] width
 * @param[in] value
 * @param[in] charWidth
 * @param[in] charHeight
 * @param[in] dodrawpic
 * @param[in] leftAlign
 * @return
 */
int CG_DrawField(int x, int y, int width, int value, int charWidth, int charHeight, qboolean dodrawpic, qboolean leftAlign)
{
	char num[16], *ptr;
	int  l;
	int  frame;
	int  startx;

	if (width < 1)
	{
		return 0;
	}

	// draw number string
	if (width > 5)
	{
		width = 5;
	}

	switch (width)
	{
	case 1:
		value = value > 9 ? 9 : value;
		value = value < 0 ? 0 : value;
		break;
	case 2:
		value = value > 99 ? 99 : value;
		value = value < -9 ? -9 : value;
		break;
	case 3:
		value = value > 999 ? 999 : value;
		value = value < -99 ? -99 : value;
		break;
	case 4:
		value = value > 9999 ? 9999 : value;
		value = value < -999 ? -999 : value;
		break;
	}

	Com_sprintf(num, sizeof(num), "%i", value);
	l = (int)strlen(num);
	if (l > width)
	{
		l = width;
	}

	if (!leftAlign)
	{
		x -= 2 + charWidth * (l);
	}

	startx = x;

	ptr = num;
	while (*ptr && l)
	{
		if (*ptr == '-')
		{
			frame = STAT_MINUS;
		}
		else
		{
			frame = *ptr - '0';
		}

		if (dodrawpic)
		{
			CG_DrawPic(x, y, charWidth, charHeight, cgs.media.numberShaders[frame]);
		}
		x += charWidth;
		ptr++;
		l--;
	}

	return startx;
}

/**
 * @brief CG_DrawLivesLeft
 * @param[in] comp
 */
void CG_DrawLivesLeft(hudComponent_t *comp)
{
	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		return;
	}

	if (cg_gameType.integer == GT_WOLF_LMS)
	{
		return;
	}

	if (cg.snap->ps.persistant[PERS_RESPAWNS_LEFT] < 0)
	{
		return;
	}

	CG_DrawPic(comp->location.x, comp->location.y, comp->location.w, comp->location.h, cg.snap->ps.persistant[PERS_TEAM] == TEAM_ALLIES ? cgs.media.hudAlliedHelmet : cgs.media.hudAxisHelmet);

	CG_DrawField(comp->location.w - 4, comp->location.y, 3, cg.snap->ps.persistant[PERS_RESPAWNS_LEFT], 14, 20, qtrue, qtrue);
}

/**
 * @brief CG_DrawCursorhint_f
 * @param comp
 * @todo FIXME Find a better way to do it
 */
static void CG_DrawCursorhint_f(hudComponent_t *comp)
{
	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		return;
	}

	CG_DrawCursorhint(&comp->location);
}

/**
 * @brief CG_DrawWeapStability_f
 * @param[in] comp
 * @todo FIXME Find a better way to do it
 */
static void CG_DrawWeapStability_f(hudComponent_t *comp)
{
	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR)
	{
		return;
	}

	CG_DrawWeapStability(&comp->location);
}

static char statsDebugStrings[6][512];
static int  statsDebugTime[6];
static int  statsDebugTextWidth[6];
static int  statsDebugPos;

/**
 * @brief CG_InitStatsDebug
 */
void CG_InitStatsDebug(void)
{
	Com_Memset(&statsDebugStrings, 0, sizeof(statsDebugStrings));
	Com_Memset(&statsDebugTime, 0, sizeof(statsDebugTime));
	statsDebugPos = -1;
}

/**
 * @brief CG_StatsDebugAddText
 * @param[in] text
 */
void CG_StatsDebugAddText(const char *text)
{
	if (cg_debugSkills.integer)
	{
		statsDebugPos++;

		if (statsDebugPos >= 6)
		{
			statsDebugPos = 0;
		}

		Q_strncpyz(statsDebugStrings[statsDebugPos], text, 512);
		statsDebugTime[statsDebugPos]      = cg.time;
		statsDebugTextWidth[statsDebugPos] = CG_Text_Width_Ext(text, .15f, 0, &cgs.media.limboFont2);

		CG_Printf("%s\n", text);
	}
}

/**
 * @brief CG_GetCompassIcon
 * @param[in] ent
 * @param[in] drawAllVoicesChat get all icons voices chat, otherwise only request relevant icons voices chat (need medic/ammo ...)
 * @param[in] drawFireTeam draw fireteam members position
 * @param[in] drawPrimaryObj draw primary objective position
 * @param[in] drawSecondaryObj draw secondary objective position
 * @param[in] drawDynamic draw dynamic elements position (player revive, command map marker)
 * @return A valid compass icon handle otherwise 0
 */
qhandle_t CG_GetCompassIcon(entityState_t *ent, qboolean drawAllVoicesChat, qboolean drawFireTeam, qboolean drawPrimaryObj, qboolean drawSecondaryObj, qboolean drawDynamic, char *name)
{
	centity_t *cent = &cg_entities[ent->number];

	if (!cent->currentValid)
	{
		return 0;
	}

	switch (ent->eType)
	{
	case ET_PLAYER:
	{
		qboolean sameTeam = cg.predictedPlayerState.persistant[PERS_TEAM] == cgs.clientinfo[ent->clientNum].team;

		if (!cgs.clientinfo[ent->clientNum].infoValid)
		{
			return 0;
		}

		if (sameTeam && cgs.clientinfo[ent->clientNum].powerups & ((1 << PW_REDFLAG) | (1 << PW_BLUEFLAG)))
		{
			return cgs.media.objectiveShader;
		}

		if (ent->eFlags & EF_DEAD)
		{
			if (drawDynamic &&
			    ((cg.predictedPlayerState.stats[STAT_PLAYER_CLASS] == PC_MEDIC &&
			      cg.predictedPlayerState.stats[STAT_HEALTH] > 0 && ent->number == ent->clientNum && sameTeam) ||
			     (!(cg.snap->ps.pm_flags & PMF_FOLLOW) && cgs.clientinfo[cg.clientNum].shoutcaster)))
			{
				return cgs.media.medicReviveShader;
			}

			return 0;
		}

		if (sameTeam && cent->voiceChatSpriteTime > cg.time &&
		    (drawAllVoicesChat ||
		     (cg.predictedPlayerState.stats[STAT_PLAYER_CLASS] == PC_MEDIC && cent->voiceChatSprite == cgs.media.medicIcon) ||
		     (cg.predictedPlayerState.stats[STAT_PLAYER_CLASS] == PC_FIELDOPS && cent->voiceChatSprite == cgs.media.ammoIcon)))
		{
			// FIXME: not the best place to reset it
			if (cgs.clientinfo[ent->clientNum].health <= 0)
			{
				// reset
				cent->voiceChatSpriteTime = cg.time;
				return 0;
			}

			return cent->voiceChatSprite;
		}

		if (drawFireTeam && (CG_IsOnSameFireteam(cg.clientNum, ent->clientNum) || cgs.clientinfo[cg.clientNum].shoutcaster))
		{
			// draw overlapping no-shoot icon if disguised and in same team
			if (ent->powerups & (1 << PW_OPS_DISGUISED) && cg.predictedPlayerState.persistant[PERS_TEAM] == cgs.clientinfo[ent->clientNum].team)
			{
				return cgs.clientinfo[ent->clientNum].selected ? cgs.media.friendShader : 0;
			}
			return cgs.clientinfo[ent->clientNum].selected ? cgs.media.buddyShader : 0;
		}
		break;
	}
	case ET_ITEM:
	{
		gitem_t *item;

		item = BG_GetItem(ent->modelindex);

		if (item && item->giType == IT_TEAM)
		{
			if ((item->giPowerUp == PW_BLUEFLAG && cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_AXIS)
			    || (item->giPowerUp == PW_REDFLAG && cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_ALLIES))
			{
				return cgs.media.objectiveBlueShader;
			}

			return cgs.media.objectiveRedShader;
		}
		break;
	}
	case ET_EXPLOSIVE_INDICATOR:
	{
		if (drawPrimaryObj)
		{
			oidInfo_t *oidInfo = &cgs.oidInfo[cent->currentState.modelindex2];
			int       entNum   = Q_atoi(
				CG_ConfigString(ent->teamNum == TEAM_AXIS ? CS_MAIN_AXIS_OBJECTIVE : CS_MAIN_ALLIES_OBJECTIVE));

			if (name)
			{
				Q_strncpyz(name, oidInfo->name, MAX_QPATH);
			}

			if (entNum == oidInfo->entityNum || oidInfo->spawnflags & (1 << 4))
			{
				if (cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_AXIS)
				{
					return ent->teamNum == TEAM_AXIS ? cgs.media.defendShader : cgs.media.attackShader;
				}
				else
				{
					return ent->teamNum == TEAM_AXIS ? cgs.media.attackShader : cgs.media.defendShader;
				}
			}
		}

		if (drawSecondaryObj)
		{
			// draw explosives if an engineer
			if (cg.predictedPlayerState.stats[STAT_PLAYER_CLASS] == PC_ENGINEER ||
			    (cg.predictedPlayerState.stats[STAT_PLAYER_CLASS] == PC_COVERTOPS && ent->effect1Time == 1))
			{
				if (ent->teamNum == 1 && cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_AXIS)
				{
					return 0;
				}

				if (ent->teamNum == 2 && cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_ALLIES)
				{
					return 0;
				}

				return cgs.media.destroyShader;
			}
		}
		break;
	}
	case ET_CONSTRUCTIBLE_INDICATOR:
	{
		if (drawPrimaryObj)
		{
			oidInfo_t *oidInfo = &cgs.oidInfo[cent->currentState.modelindex2];
			int       entNum   = Q_atoi(CG_ConfigString(ent->teamNum == TEAM_AXIS ? CS_MAIN_AXIS_OBJECTIVE : CS_MAIN_ALLIES_OBJECTIVE));

			if (name)
			{
				Q_strncpyz(name, oidInfo->name, MAX_QPATH);
			}

			if (entNum == oidInfo->entityNum || oidInfo->spawnflags & (1 << 4))
			{
				if (cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_AXIS)
				{
					return ent->teamNum == TEAM_AXIS ? cgs.media.defendShader : cgs.media.attackShader;
				}
				else
				{
					return ent->teamNum == TEAM_AXIS ? cgs.media.attackShader : cgs.media.defendShader;
				}
			}
		}

		if (drawSecondaryObj)
		{
			// draw construction if an engineer
			if (cg.predictedPlayerState.stats[STAT_PLAYER_CLASS] == PC_ENGINEER)
			{
				if (ent->teamNum == 1 && cg.predictedPlayerState.persistant[PERS_TEAM] != TEAM_AXIS)
				{
					return 0;
				}

				if (ent->teamNum == 2 && cg.predictedPlayerState.persistant[PERS_TEAM] != TEAM_ALLIES)
				{
					return 0;
				}

				return cgs.media.constructShader;
			}
		}
		break;
	}
	case ET_TANK_INDICATOR:
	{
		if (drawPrimaryObj)
		{
			oidInfo_t *oidInfo = &cgs.oidInfo[cent->currentState.modelindex2];
			int       entNum   = Q_atoi(CG_ConfigString(ent->teamNum == TEAM_AXIS ? CS_MAIN_AXIS_OBJECTIVE : CS_MAIN_ALLIES_OBJECTIVE));

			if (name)
			{
				Q_strncpyz(name, oidInfo->name, MAX_QPATH);
			}

			if (entNum == oidInfo->entityNum || oidInfo->spawnflags & (1 << 4))
			{
				if (cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_AXIS)
				{
					return ent->teamNum == TEAM_AXIS ? cgs.media.defendShader : cgs.media.attackShader;
				}
				else
				{
					return ent->teamNum == TEAM_AXIS ? cgs.media.attackShader : cgs.media.defendShader;
				}
			}
		}

		if (drawSecondaryObj)
		{
			// FIXME: show only when relevant
			if ((ent->teamNum == 1 && cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_AXIS)
			    || (ent->teamNum == 2 && cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_ALLIES))
			{
				return cgs.media.escortShader;
			}

			return cgs.media.destroyShader;
		}
		break;
	}
	case ET_TANK_INDICATOR_DEAD:
	{
		if (drawPrimaryObj)
		{
			oidInfo_t *oidInfo = &cgs.oidInfo[cent->currentState.modelindex2];
			int       entNum   = Q_atoi(CG_ConfigString(ent->teamNum == TEAM_AXIS ? CS_MAIN_AXIS_OBJECTIVE : CS_MAIN_ALLIES_OBJECTIVE));

			if (name)
			{
				Q_strncpyz(name, oidInfo->name, MAX_QPATH);
			}

			if (entNum == oidInfo->entityNum || oidInfo->spawnflags & (1 << 4))
			{
				if (cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_AXIS)
				{
					return ent->teamNum == TEAM_AXIS ? cgs.media.defendShader : cgs.media.attackShader;
				}
				else
				{
					return ent->teamNum == TEAM_AXIS ? cgs.media.attackShader : cgs.media.defendShader;
				}
			}
		}

		if (drawSecondaryObj)
		{
			// FIXME: show only when relevant
			// draw repair if an engineer
			if (cg.predictedPlayerState.stats[STAT_PLAYER_CLASS] == PC_ENGINEER && (
					(ent->teamNum == 1 && cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_AXIS)
					|| (ent->teamNum == 2 && cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_ALLIES)))
			{
				return cgs.media.constructShader;
			}
		}
		break;
	}
	case ET_TRAP:
	{
		if (drawSecondaryObj)
		{
			if (ent->frame == 0)
			{
				return cgs.media.regroupShader;
			}

			if (ent->frame == 4)
			{
				return cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_AXIS ? cgs.media.regroupShader : cgs.media.defendShader;
			}

			if (ent->frame == 3)
			{
				return cg.predictedPlayerState.persistant[PERS_TEAM] == TEAM_ALLIES ? cgs.media.regroupShader : cgs.media.defendShader;
			}
		}
		break;
	}
	// FIXME: ET_COMMANDMAP_MARKER, ET_HEALER, ET_SUPPLIER
	//case ET_MG42_BARREL:
	//{
	//    return cgs.media.mg42HintShader;
	//}
	//case ET_CABINET_H:
	//{
	//	return cgs.media.healthHintShader;
	//}
	//caseET_CABINET_A:
	//{
	//	return cgs.media.ammoHintShader;
	//}
	default:
		break;
	}

	return 0;
}

/**
 * @brief CG_CompasMoveLocationCalc
 * @param[out] locationvalue
 * @param[in] directionplus
 * @param[in] animationout
 */
static void CG_CompasMoveLocationCalc(float *locationvalue, qboolean directionplus, qboolean animationout)
{
	if (animationout)
	{
		if (directionplus)
		{
			*locationvalue += ((cg.time - cgs.autoMapExpandTime) / 100.f) * 128.f;
		}
		else
		{
			*locationvalue -= ((cg.time - cgs.autoMapExpandTime) / 100.f) * 128.f;
		}
	}
	else
	{
		if (!directionplus)
		{
			*locationvalue += (((cg.time - cgs.autoMapExpandTime - 150.f) / 100.f) * 128.f) - 128.f;
		}
		else
		{
			*locationvalue -= (((cg.time - cgs.autoMapExpandTime - 150.f) / 100.f) * 128.f) - 128.f;
		}
	}
}

/**
 * @brief CG_CompasMoveLocation
 * @param[in] basex
 * @param[in] basey
 * @param[in] basew
 * @param[in] animationout
 */
static void CG_CompasMoveLocation(float *basex, float *basey, float basew, qboolean animationout)
{
	float x    = *basex;
	float y    = *basey;
	float cent = basew / 2;
	x += cent;
	y += cent;

	if (x < Ccg_WideX(320))
	{
		if (y < 240)
		{
			if (x < y)
			{
				//move left
				CG_CompasMoveLocationCalc(basex, qfalse, animationout);
			}
			else
			{
				//move up
				CG_CompasMoveLocationCalc(basey, qfalse, animationout);
			}
		}
		else
		{
			if (x < (SCREEN_HEIGHT - y))
			{
				//move left
				CG_CompasMoveLocationCalc(basex, qfalse, animationout);
			}
			else
			{
				//move down
				CG_CompasMoveLocationCalc(basey, qtrue, animationout);
			}
		}
	}
	else
	{
		if (y < 240)
		{
			if ((Ccg_WideX(SCREEN_WIDTH) - x) < y)
			{
				//move right
				CG_CompasMoveLocationCalc(basex, qtrue, animationout);
			}
			else
			{
				//move up
				CG_CompasMoveLocationCalc(basey, qfalse, animationout);
			}
		}
		else
		{
			if ((Ccg_WideX(SCREEN_WIDTH) - x) < (SCREEN_HEIGHT - y))
			{
				//move right
				CG_CompasMoveLocationCalc(basex, qtrue, animationout);
			}
			else
			{
				//move down
				CG_CompasMoveLocationCalc(basey, qtrue, animationout);
			}
		}
	}
}

/**
 * @brief CG_DrawNewCompass
 * @param location
 */
void CG_DrawNewCompass(hudComponent_t *comp)
{
	float      basex = comp->location.x, basey = comp->location.y - 16, basew = comp->location.w, baseh = comp->location.h;
	snapshot_t *snap;

	if (cg.nextSnap && !cg.nextFrameTeleport && !cg.thisFrameTeleport)
	{
		snap = cg.nextSnap;
	}
	else
	{
		snap = cg.snap;
	}

	if ((snap->ps.pm_flags & PMF_LIMBO && !cgs.clientinfo[cg.clientNum].shoutcaster)
#ifdef FEATURE_MULTIVIEW
	    || cg.mvTotalClients > 0
#endif
	    )
	{
		CG_DrawExpandedAutoMap();
		return;
	}

	if (!cg_altHud.integer)
	{
		if (cgs.autoMapExpanded)
		{
			if (cg.time - cgs.autoMapExpandTime < 100.f)
			{
				CG_CompasMoveLocation(&basex, &basey, basew, qtrue);
			}
			else
			{
				CG_DrawExpandedAutoMap();
				return;
			}
		}
		else
		{
			if (cg.time - cgs.autoMapExpandTime <= 150.f)
			{
				CG_DrawExpandedAutoMap();
				return;
			}
			else if ((cg.time - cgs.autoMapExpandTime > 150.f) && (cg.time - cgs.autoMapExpandTime < 250.f))
			{
				CG_CompasMoveLocation(&basex, &basey, basew, qfalse);
			}
		}
	}

	if ((snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR && !cgs.clientinfo[cg.clientNum].shoutcaster) || !cg_drawCompass.integer)
	{
		return;
	}

	CG_DrawAutoMap(basex, basey, basew, baseh);
}
/**
 * @brief CG_DrawStatsDebug
 */
static void CG_DrawStatsDebug(void)
{
	int textWidth = 0;
	int i, x, y, w, h;

	if (!cg_debugSkills.integer)
	{
		return;
	}

	for (i = 0; i < 6; i++)
	{
		if (statsDebugTime[i] + 9000 > cg.time)
		{
			if (statsDebugTextWidth[i] > textWidth)
			{
				textWidth = statsDebugTextWidth[i];
			}
		}
	}

	w = textWidth + 6;
	h = 9;
	x = SCREEN_WIDTH - w;
	y = (SCREEN_HEIGHT - 5 * (12 + 2) + 6 - 4) - 6 - h;     // don't ask

	i = statsDebugPos;

	do
	{
		vec4_t colour;

		if (statsDebugTime[i] + 9000 <= cg.time)
		{
			break;
		}

		colour[0] = colour[1] = colour[2] = .5f;
		if (cg.time - statsDebugTime[i] > 5000)
		{
			colour[3] = .5f - .5f * ((cg.time - statsDebugTime[i] - 5000) / 4000.f);
		}
		else
		{
			colour[3] = .5f ;
		}
		CG_FillRect(x, y, w, h, colour);

		colour[0] = colour[1] = colour[2] = 1.f;
		if (cg.time - statsDebugTime[i] > 5000)
		{
			colour[3] = 1.f - ((cg.time - statsDebugTime[i] - 5000) / 4000.f);
		}
		else
		{
			colour[3] = 1.f ;
		}
		CG_Text_Paint_Ext(640.f - 3 - statsDebugTextWidth[i], y + h - 2, .15f, .15f, colour, statsDebugStrings[i], 0, 0, ITEM_TEXTSTYLE_NORMAL, &cgs.media.limboFont2);

		y -= h;

		i--;
		if (i < 0)
		{
			i = 6 - 1;
		}
	}
	while (i != statsDebugPos);
}

/*
===========================================================================================
  UPPER RIGHT CORNER
===========================================================================================
*/

#define UPPERRIGHT_X 634
#define UPPERRIGHT_W 52

/**
 * @brief CG_DrawSnapshot
 * @param[in] comp
 * @return
 */
static void CG_DrawSnapshot(hudComponent_t *comp)
{
	char *s = va("t:%i", cg.snap->serverTime);
	int  w  = CG_Text_Width_Ext(s, comp->scale, 0, &cgs.media.limboFont1);
	int  w2 = (comp->location.w > w) ? comp->location.w : w;
	int  x  = comp->location.x;
	int  y  = comp->location.y;

	CG_FillRect(x, y, w2 + 5, comp->location.h, HUD_Background);
	CG_DrawRect_FixedBorder(x, y, w2 + 5, comp->location.h, 1, HUD_Border);
	CG_Text_Paint_Ext(x + ((w2 - w) / 2) + 2, y + 11, comp->scale, comp->scale, comp->color, s, 0, 0, 0, &cgs.media.limboFont1);
	s = va("sn:%i", cg.latestSnapshotNum);
	CG_Text_Paint_Ext(x + ((w2 - w) / 2) + 2, y + 23, comp->scale, comp->scale, comp->color, s, 0, 0, 0, &cgs.media.limboFont1);
	s = va("cmd:%i", cgs.serverCommandSequence);
	CG_Text_Paint_Ext(x + ((w2 - w) / 2) + 2, y + 35, comp->scale, comp->scale, comp->color, s, 0, 0, 0, &cgs.media.limboFont1);
}

/**
 * @brief CG_DrawSpeed
 * @param[in] comp
 * @return
 */
static void CG_DrawSpeed(hudComponent_t *comp)
{
	static vec_t highestSpeed, speed;
	static int   lasttime;
	char         *s, *s2 = NULL;
	int          w, w2, w3, w4;
	int          thistime;
	int          x, y, h;

	if (resetmaxspeed)
	{
		highestSpeed  = 0;
		resetmaxspeed = qfalse;
	}

	thistime = trap_Milliseconds();

	if (thistime > lasttime + 100)
	{
		speed = VectorLength(cg.predictedPlayerState.velocity);

		if (speed > highestSpeed)
		{
			highestSpeed = speed;
		}

		lasttime = thistime;
	}

	switch (cg_drawUnit.integer)
	{
	case 0:
		// Units per second
		s  = va("%.1f UPS", speed);
		s2 = va("%.1f MAX", highestSpeed);
		break;
	case 1:
		// Kilometers per hour
		s  = va("%.1f KPH", (speed / SPEED_US_TO_KPH));
		s2 = va("%.1f MAX", (highestSpeed / SPEED_US_TO_KPH));
		break;
	case 2:
		// Miles per hour
		s  = va("%.1f MPH", (speed / SPEED_US_TO_MPH));
		s2 = va("%.1f MAX", (highestSpeed / SPEED_US_TO_MPH));
		break;
	default:
		s  = "";
		s2 = "";
		break;
	}

	h = comp->location.h * ((cg_drawspeed.integer == 2) ? 2 : 1);

	w  = CG_Text_Width_Ext(s, comp->scale, 0, &cgs.media.limboFont1);
	w2 = (comp->location.w > w) ? comp->location.w : w;

	x = comp->location.x;
	y = comp->location.y;
	CG_FillRect(x, y, w2 + 5, h, HUD_Background);
	CG_DrawRect_FixedBorder(x, y, w2 + 5, h, 1, HUD_Border);
	CG_Text_Paint_Ext(x + ((w2 - w) / 2) + 2, y + 11, comp->scale, comp->scale, comp->color, s, 0, 0, 0, &cgs.media.limboFont1);

	// draw max speed on second line
	if (cg_drawspeed.integer == 2)
	{
		y += comp->location.h;
		w3 = CG_Text_Width_Ext(s2, 0.19f, 0, &cgs.media.limboFont1);
		w4 = (UPPERRIGHT_W > w3) ? UPPERRIGHT_W : w3;
		CG_Text_Paint_Ext(x + ((w4 - w3) / 2) + 2, y + 11, comp->scale, comp->scale, comp->color, s2, 0, 0, 0, &cgs.media.limboFont1);
	}
}

#define MAX_FPS_FRAMES  500

/**
 * @brief CG_DrawFPS
 * @param[in] comp
 * @return
 */
static void CG_DrawFPS(hudComponent_t *comp)
{
	static int previousTimes[MAX_FPS_FRAMES];
	static int previous;
	static int index;
	static int oldSamples;
	const char *s;
	int        t;
	int        frameTime;
	int        samples = cg_drawFPS.integer;
	int        x, y, w, w2;

	t = trap_Milliseconds(); // don't use serverTime, because that will be drifting to correct for internet lag changes, timescales, timedemos, etc

	frameTime = t - previous;
	previous  = t;

	if (samples < 4)
	{
		samples = 4;
	}
	if (samples > MAX_FPS_FRAMES)
	{
		samples = MAX_FPS_FRAMES;
	}
	if (samples != oldSamples)
	{
		index = 0;
	}

	oldSamples                     = samples;
	previousTimes[index % samples] = frameTime;
	index++;

	if (index > samples)
	{
		int i, fps;
		// average multiple frames together to smooth changes out a bit
		int total = 0;

		for (i = 0 ; i < samples ; ++i)
		{
			total += previousTimes[i];
		}

		total = total ? total : 1;

		fps = 1000 * samples / total;

		s = va("%i FPS", fps);
	}
	else
	{
		s = "estimating";
	}

	w  = CG_Text_Width_Ext(s, comp->scale, 0, &cgs.media.limboFont1);
	w2 = (comp->location.w > w) ? comp->location.w : w;

	x = comp->location.x;
	y = comp->location.y;
	CG_FillRect(x, y, w2 + 5, comp->location.h, HUD_Background);
	CG_DrawRect_FixedBorder(x, y, w2 + 5, 12 + 2, 1, HUD_Border);
	CG_Text_Paint_Ext(x + ((w2 - w) / 2) + 2, y + 11, comp->scale, comp->scale, comp->color, s, 0, 0, 0, &cgs.media.limboFont1);
}

/**
 * @brief CG_SpawnTimerText red colored spawn time text in reinforcement time HUD element.
 * @return red colored text or NULL when its not supposed to be rendered
*/
char *CG_SpawnTimerText()
{
	int msec = (cgs.timelimit * 60000.f) - (cg.time - cgs.levelStartTime);
	int seconds;
	int secondsThen;

	if (cg_spawnTimer_set.integer != -1 && cgs.gamestate == GS_PLAYING && !cgs.clientinfo[cg.clientNum].shoutcaster)
	{
		if (cgs.clientinfo[cg.clientNum].team != TEAM_SPECTATOR || (cg.snap->ps.pm_flags & PMF_FOLLOW))
		{
			int period = cg_spawnTimer_period.integer > 0 ? cg_spawnTimer_period.integer : (cgs.clientinfo[cg.snap->ps.clientNum].team == TEAM_AXIS ? cg_bluelimbotime.integer / 1000 : cg_redlimbotime.integer / 1000);
			if (period > 0) // prevent division by 0 for weird cases like limbtotime < 1000
			{
				seconds     = msec / 1000;
				secondsThen = ((cgs.timelimit * 60000.f) - cg_spawnTimer_set.integer) / 1000;
				return va("%i", period + (seconds - secondsThen) % period);
			}
		}
	}
	else if (cg_spawnTimer_set.integer != -1 && cg_spawnTimer_period.integer > 0 && cgs.gamestate != GS_PLAYING)
	{
		// We are not playing and the timer is set so reset/disable it
		// this happens for example when custom period is set by timerSet and map is restarted or changed
		trap_Cvar_Set("cg_spawnTimer_set", "-1");
	}
	return NULL;
}

/**
 * @brief CG_SpawnTimersText
 * @param[out] respawn
 * @param[out] spawntimer
 * @return
 */
static qboolean CG_SpawnTimersText(char **s, char **rt)
{
	if (cgs.gamestate != GS_PLAYING)
	{
		int limbotimeOwn, limbotimeEnemy;
		if (cgs.clientinfo[cg.snap->ps.clientNum].team == TEAM_AXIS)
		{
			limbotimeOwn   = cg_redlimbotime.integer;
			limbotimeEnemy = cg_bluelimbotime.integer;
		}
		else
		{
			limbotimeOwn   = cg_bluelimbotime.integer;
			limbotimeEnemy = cg_redlimbotime.integer;
		}

		*rt = va("%2.0i", limbotimeEnemy / 1000);
		*s  = cgs.gametype == GT_WOLF_LMS ? va("%s", CG_TranslateString("WARMUP")) : va("%2.0i", limbotimeOwn / 1000);
		return qtrue;
	}
	else if (cgs.gametype != GT_WOLF_LMS && (cgs.clientinfo[cg.clientNum].team != TEAM_SPECTATOR || (cg.snap->ps.pm_flags & PMF_FOLLOW)) && cg_drawReinforcementTime.integer > 0)
	{
		*s  = va("%2.0i", CG_CalculateReinfTime(qfalse));
		*rt = CG_SpawnTimerText();
	}
	return qfalse;
}

/**
 * @brief CG_RoundTimerText
 * @return
 */
static char *CG_RoundTimerText()
{
	qtime_t qt;
	int     msec = CG_RoundTime(&qt);
	if (msec < 0 && cgs.timelimit > 0.0f)
	{
		return "0:00"; // round ended
	}

	char *seconds = qt.tm_sec > 9 ? va("%i", qt.tm_sec) : va("0%i", qt.tm_sec);
	char *minutes = qt.tm_min > 9 ? va("%i", qt.tm_min) : va("0%i", qt.tm_min);

	return va("%s:%s", minutes, seconds);
}

/**
 * @brief CG_LocalTimeText
 * @return
 */
static char *CG_LocalTimeText()
{
	qtime_t  time;
	char     *s;
	qboolean pmtime = qfalse;

	//Fetch the local time
	trap_RealTime(&time);

	if (cg_drawTime.integer & LOCALTIME_SECOND)
	{
		if (cg_drawTime.integer & LOCALTIME_12HOUR)
		{
			if (time.tm_hour > 12)
			{
				pmtime = qtrue;
			}
			s = va("%i:%02i:%02i %s", (pmtime ? time.tm_hour - 12 : time.tm_hour), time.tm_min, time.tm_sec, (pmtime ? "PM" : "AM"));
		}
		else
		{
			s = va("%02i:%02i:%02i", time.tm_hour, time.tm_min, time.tm_sec);
		}
	}
	else
	{
		if (cg_drawTime.integer & LOCALTIME_12HOUR)
		{
			if (time.tm_hour > 12)
			{
				pmtime = qtrue;
			}
			s = va("%i:%02i %s", (pmtime ? time.tm_hour - 12 : time.tm_hour), time.tm_min, (pmtime ? "PM" : "AM"));
		}
		else
		{
			s = va("%02i:%02i", time.tm_hour, time.tm_min);
		}
	}
	return s;
}

/**
 * @brief CG_DrawRespawnTimer
 * @param respawn
 */
static void CG_DrawRespawnTimer(hudComponent_t *comp)
{
	char     *s = NULL, *rt = NULL;
	int      w;
	vec4_t   color;
	qboolean blink;
	float    blinkAlpha;

	if (cg_paused.integer)
	{
		return;
	}

	blink = CG_SpawnTimersText(&s, &rt);

	if (blink)
	{
		blinkAlpha = fabs(sin(cg.time * 0.002));
	}

	if (s)
	{
		w = CG_Text_Width_Ext(s, comp->scale, 0, &cgs.media.limboFont1);
		Com_Memcpy(color, comp->color, sizeof(vec4_t));
		color[3] = blink ? blinkAlpha : color[3];
		CG_Text_Paint_Ext(comp->location.x - w, comp->location.y, comp->scale, comp->scale, color, s, 0, 0,
		                  ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
	}
}

/**
 * @brief CG_DrawSpawnTimer
 * @param respawn
 */
static void CG_DrawSpawnTimer(hudComponent_t *comp)
{
	char     *s = NULL, *rt = NULL;
	int      w;
	vec4_t   color;
	qboolean blink;
	float    blinkAlpha;

	if (cg_paused.integer)
	{
		return;
	}

	blink = CG_SpawnTimersText(&s, &rt);

	if (blink)
	{
		blinkAlpha = fabs(sin(cg.time * 0.002));
	}

	if (rt)
	{
		w = CG_Text_Width_Ext(s, comp->scale, 0, &cgs.media.limboFont1);
		Com_Memcpy(color, comp->color, sizeof(vec4_t));
		color[3] = blink ? blinkAlpha : color[3];
		CG_Text_Paint_Ext(comp->location.x - w, comp->location.y, comp->scale, comp->scale,
		                  color, rt, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
	}
}

/**
 * @brief CG_DrawRoundTimerSimple
 * @param roundtimer
 */
static void CG_DrawRoundTimerSimple(hudComponent_t *comp)
{
	char     *s = NULL, *rt = NULL;
	vec4_t   color;
	qboolean blink;
	float    blinkAlpha;

	if (cg_paused.integer)
	{
		return;
	}

	blink = CG_SpawnTimersText(&s, &rt);

	if (blink)
	{
		blinkAlpha = fabs(sin(cg.time * 0.002));
	}

	Com_Memcpy(color, comp->color, sizeof(vec4_t));
	color[3] = blink ? blinkAlpha : color[3];
	CG_Text_Paint_Ext(comp->location.x, comp->location.y, comp->scale, comp->scale, color, CG_RoundTimerText(), 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
}

/**
 * @brief CG_DrawTimerNormal
 * @param[in] y
 * @return
 */
static void CG_DrawRoundTimerNormal(hudComponent_t *comp)
{
	char     *s = NULL, *rt = NULL, *mt;
	int      w, w2;
	int      x;
	vec4_t   color;
	float    blinkAlpha;
	qboolean blink;

	if (cg_paused.integer)
	{
		return;
	}

	blink = CG_SpawnTimersText(&s, &rt);

	if (blink)
	{
		blinkAlpha = fabs(sin(cg.time * 0.002));
	}

	mt = va("%s%s", "^7", CG_RoundTimerText());

	if (s)
	{
		s = va("^$%s%s%s", s, " ", mt);
	}
	else
	{
		s = mt;
	}

	if (rt)
	{
		s = va("^1%s%s%s", rt, " ", s);
	}

	Com_Memcpy(color, comp->color, sizeof(vec4_t));
	color[3] = blink ? blinkAlpha : color[3];

	w  = CG_Text_Width_Ext(s, comp->scale, 0, &cgs.media.limboFont1);
	w2 = (comp->location.w > w) ? comp->location.w : w;

	x = comp->location.x;
	CG_FillRect(x, comp->location.y, w2 + 5, comp->location.h, HUD_Background);
	CG_DrawRect_FixedBorder(x, comp->location.y, w2 + 5, comp->location.h, 1, HUD_Border);
	CG_Text_Paint_Ext(x + ((w2 - w) / 2) + 2, comp->location.y + 11, comp->scale, comp->scale, color, s, 0, 0, 0, &cgs.media.limboFont1);
}

/**
 * @brief CG_DrawRoundTimer
 * @param comp
 */
static void CG_DrawRoundTimer(hudComponent_t *comp)
{
	if (comp->style == STYLE_NORMAL)
	{
		CG_DrawRoundTimerNormal(comp);
	}
	else
	{
		CG_DrawRoundTimerSimple(comp);
	}
}

/**
 * @brief CG_DrawLocalTimeSimple
 * @param respawn
 */
static void CG_DrawLocalTimeSimple(hudComponent_t *comp)
{
	CG_Text_Paint_Ext(comp->location.x, comp->location.y, comp->scale, comp->scale, comp->color, CG_LocalTimeText(), 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont1);
}

/**
 * @brief CG_DrawLocalTimeNormal
 * @param y
 */
static void CG_DrawLocalTimeNormal(hudComponent_t *comp)
{
	int  w, w2, x;
	char *s = CG_LocalTimeText();

	w  = CG_Text_Width_Ext(s, comp->scale, 0, &cgs.media.limboFont1);
	w2 = (comp->location.w > w) ? comp->location.w : w;

	x = comp->location.x;
	CG_FillRect(x, comp->location.y, w2 + 5, comp->location.h, HUD_Background);
	CG_DrawRect_FixedBorder(x, comp->location.y, w2 + 5, comp->location.h, 1, HUD_Border);
	CG_Text_Paint_Ext(x + ((w2 - w) / 2) + 2, comp->location.y + 11, comp->scale, comp->scale, comp->color, s, 0, 0, 0, &cgs.media.limboFont1);
}

/**
 * @brief CG_DrawLocalTime
 * @param[in] y
 * @return
 */
static void CG_DrawLocalTime(hudComponent_t *comp)
{
	if (!(cg_drawTime.integer & LOCALTIME_ON))
	{
		return;
	}

	if (comp->style == STYLE_NORMAL)
	{
		CG_DrawLocalTimeNormal(comp);
	}
	else
	{
		CG_DrawLocalTimeSimple(comp);
	}
}

/**
 * @brief Adds the current interpolate / extrapolate bar for this frame
 */
void CG_AddLagometerFrameInfo(void)
{
	lagometer.frameSamples[lagometer.frameCount & (LAG_SAMPLES - 1)] = cg.time - cg.latestSnapshotTime;
	lagometer.frameCount++;
}

/**
 * @brief Log the ping time, server framerate and number of dropped snapshots
 * before it each time a snapshot is received.
 * @param[in] snap
 */
void CG_AddLagometerSnapshotInfo(snapshot_t *snap)
{
	unsigned int index = lagometer.snapshotCount & (LAG_SAMPLES - 1);
	int          oldest;

	// dropped packet
	if (!snap)
	{
		lagometer.snapshotSamples[index] = -1;
		lagometer.snapshotCount++;
		return;
	}

	// add this snapshot's info
	if (cg.demoPlayback)
	{
		static int lasttime = 0;

		snap->ping = (snap->serverTime - snap->ps.commandTime) - (1000 / cgs.sv_fps);

		// display snapshot time delta instead of ping
		lagometer.snapshotSamples[index] = snap->serverTime - lasttime;
		lasttime                         = snap->serverTime;
	}
	else
	{
		lagometer.snapshotSamples[index] = MAX(snap->ping - snap->ps.stats[STAT_ANTIWARP_DELAY], 0);
	}
	lagometer.snapshotAntiwarp[index] = snap->ping;  // TODO: check this for demoPlayback
	lagometer.snapshotFlags[index]    = snap->snapFlags;
	lagometer.snapshotCount++;

	// compute server framerate
	index = cgs.sampledStat.count;

	if (cgs.sampledStat.count < LAG_SAMPLES)
	{
		cgs.sampledStat.count++;
	}
	else
	{
		index -= 1;
	}

	cgs.sampledStat.samples[index].elapsed = snap->serverTime - cgs.sampledStat.lastSampleTime;
	cgs.sampledStat.samples[index].time    = snap->serverTime;

	if (cgs.sampledStat.samples[index].elapsed < 0)
	{
		cgs.sampledStat.samples[index].elapsed = 0;
	}

	cgs.sampledStat.lastSampleTime = snap->serverTime;

	cgs.sampledStat.samplesTotalElpased += cgs.sampledStat.samples[index].elapsed;

	oldest = snap->serverTime - PERIOD_SAMPLES;
	for (index = 0; index < cgs.sampledStat.count; index++)
	{
		if (cgs.sampledStat.samples[index].time > oldest)
		{
			break;
		}

		cgs.sampledStat.samplesTotalElpased -= cgs.sampledStat.samples[index].elapsed;
	}

	if (index)
	{
		memmove(cgs.sampledStat.samples, cgs.sampledStat.samples + index, sizeof(sample_t) * (cgs.sampledStat.count - index));
		cgs.sampledStat.count -= index;
	}

	cgs.sampledStat.avg = cgs.sampledStat.samplesTotalElpased > 0
	                      ? (int) (cgs.sampledStat.count / (cgs.sampledStat.samplesTotalElpased / 1000.0f) + 0.5f)
	                      : 0;
}

/**
 * @brief Draw disconnect icon for long lag
 * @param[in] y
 * @return
 */
static void CG_DrawDisconnect(hudComponent_t *comp)
{
	int        cmdNum, w, w2, x, y;
	usercmd_t  cmd;
	const char *s;

	// use same dimension as timer
	w  = CG_Text_Width_Ext("xx:xx:xx", 0.19f, 0, &cgs.media.limboFont1);
	w2 = (comp->location.w > w) ? comp->location.w : w;
	x  = comp->location.x;
	y  = comp->location.y;

	// dont draw if a demo and we're running at a different timescale
	if (cg.demoPlayback && cg_timescale.value != 1.0f)
	{
		return;
	}

	// don't draw if the server is respawning
	if (cg.serverRespawning)
	{
		return;
	}

	// don't draw if intermission is about to start
	if (cg.intermissionStarted)
	{
		return;
	}

	// draw the phone jack if we are completely past our buffers
	cmdNum = trap_GetCurrentCmdNumber() - CMD_BACKUP + 1;
	trap_GetUserCmd(cmdNum, &cmd);
	if (cmd.serverTime <= cg.snap->ps.commandTime
	    || cmd.serverTime > cg.time)        // special check for map_restart
	{
		return;
	}

	// also add text in center of screen
	s = CG_TranslateString("Connection Interrupted");
	w = CG_Text_Width_Ext(s, comp->scale, 0, &cgs.media.limboFont2);
	CG_Text_Paint_Ext(Ccg_WideX(320) - w / 2, 100, comp->scale, comp->scale, comp->color, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont2);

	// blink the icon
	if ((cg.time >> 9) & 1)
	{
		return;
	}

	CG_DrawPic(x + 1, y + 1, w2 + 3, w2 + 3, cgs.media.disconnectIcon);
}

/**
 * @brief CG_DrawPing
 * @param[in] y
 * @return
 */
static void CG_DrawPing(hudComponent_t *comp)
{
	int  curPing = cg.snap->ping;
	int  w, w2, x, y;
	char *s;

	s = va("Ping %d", curPing < 999 ? curPing : 999);
	w = CG_Text_Width_Ext(s, comp->scale, 0, &cgs.media.limboFont1);

	w2 = (comp->location.w > w) ? comp->location.w : w;

	x = comp->location.x;
	y = comp->location.y;
	CG_FillRect(x, y, w2 + 5, comp->location.h, HUD_Background);
	CG_DrawRect_FixedBorder(x, y, w2 + 5, comp->location.h, 1, HUD_Border);
	CG_Text_Paint_Ext(x + ((w2 - w) / 2) + 2, y + 11, comp->scale, comp->scale, comp->color, s, 0, 0, 0, &cgs.media.limboFont1);
}

vec4_t colorAW = { 0, 0.5, 0, 0.5f };

/**
 * @brief CG_DrawLagometer
 * @param[in] y
 * @return
 */
static void CG_DrawLagometer(hudComponent_t *comp)
{
	int   a, w, w2, x, y, i;
	float v;
	float ax, ay, aw, ah, mid, range;
	int   color;
	float vscale;

	// use same dimension as timer
	w  = CG_Text_Width_Ext("xx:xx:xx", comp->scale, 0, &cgs.media.limboFont1);
	w2 = (comp->location.w > w) ? comp->location.w : w;
	x  = comp->location.x;
	y  = comp->location.y;

	// draw the graph
	trap_R_SetColor(NULL);
	CG_FillRect(x, y, w2 + 5, comp->location.h + 5, HUD_Background);
	CG_DrawRect_FixedBorder(x, y, w2 + 5, comp->location.h + 5, 1, HUD_Border);

	ax = x;
	ay = y;
	aw = w2 + 4;
	ah = w2 + 4;
	CG_AdjustFrom640(&ax, &ay, &aw, &ah);

	color = -1;
	range = ah / 3;
	mid   = ay + range;

	vscale = range / MAX_LAGOMETER_RANGE;

	// draw the frame interpoalte / extrapolate graph
	for (a = 0 ; a < aw ; a++)
	{
		i  = (lagometer.frameCount - 1 - a) & (LAG_SAMPLES - 1);
		v  = lagometer.frameSamples[i];
		v *= vscale;
		if (v > 0)
		{
			if (color != 1)
			{
				color = 1;
				trap_R_SetColor(colorYellow);
			}
			if (v > range)
			{
				v = range;
			}
			trap_R_DrawStretchPic(ax + aw - a, mid - v, 1, v, 0, 0, 0, 0, cgs.media.whiteShader);
		}
		else if (v < 0)
		{
			if (color != 2)
			{
				color = 2;
				trap_R_SetColor(colorBlue);
			}
			v = -v;
			if (v > range)
			{
				v = range;
			}
			trap_R_DrawStretchPic(ax + aw - a, mid, 1, v, 0, 0, 0, 0, cgs.media.whiteShader);
		}
	}

	// draw the snapshot latency / drop graph
	range  = ah / 2;
	vscale = range / MAX_LAGOMETER_PING;

	for (a = 0 ; a < aw ; a++)
	{
		i = (lagometer.snapshotCount - 1 - a) & (LAG_SAMPLES - 1);
		v = lagometer.snapshotSamples[i];
		if (v > 0)
		{
			// antiwarp indicator
			if (lagometer.snapshotAntiwarp[i] > 0)
			{
				w = lagometer.snapshotAntiwarp[i] * vscale;

				if (color != 6)
				{
					color = 6;
					trap_R_SetColor(colorAW);
				}

				if (w > range)
				{
					w = range;
				}
				trap_R_DrawStretchPic(ax + aw - a, ay + ah - w - 2, 1, w, 0, 0, 0, 0, cgs.media.whiteShader);
			}

			if (lagometer.snapshotFlags[i] & SNAPFLAG_RATE_DELAYED)
			{
				if (color != 5)
				{
					color = 5;  // YELLOW for rate delay
					trap_R_SetColor(colorYellow);
				}
			}
			else
			{
				if (color != 3)
				{
					color = 3;
					trap_R_SetColor(colorGreen);
				}
			}
			v = v * vscale;
			if (v > range)
			{
				v = range;
			}
			trap_R_DrawStretchPic(ax + aw - a, ay + ah - v, 1, v, 0, 0, 0, 0, cgs.media.whiteShader);
		}
		else if (v < 0)
		{
			if (color != 4)
			{
				color = 4;      // RED for dropped snapshots
				trap_R_SetColor(colorRed);
			}
			trap_R_DrawStretchPic(ax + aw - a, ay + ah - range, 1, range, 0, 0, 0, 0, cgs.media.whiteShader);
		}
	}

	trap_R_SetColor(NULL);

	if (cg_nopredict.integer
#ifdef ALLOW_GSYNC
	    || cg_synchronousClients.integer
#endif // ALLOW_GSYNC
	    )
	{
		CG_Text_Paint_Ext(ax, ay, cg_fontScaleTP.value, cg_fontScaleTP.value, colorWhite, "snc", 0, 0, ITEM_TEXTSTYLE_SHADOWED, &cgs.media.limboFont2);
	}

	// don't draw if a demo and we're running at a different timescale
	if (!cg.demoPlayback)
	{
		CG_DrawDisconnect(&activehud->disconnect);
	}

	// add snapshots/s in top-right corner of meter
	{
		char   *result;
		vec4_t *clr;

		if (cgs.sampledStat.avg < cgs.sv_fps * 0.5f)
		{
			clr = &colorRed;
		}
		else if (cgs.sampledStat.avg < cgs.sv_fps * 0.75f)
		{
			clr = &colorYellow;
		}
		else
		{
			clr = &comp->color;
		}

		// FIXME: see warmup blinky blinky
		//if (cgs.gamestate != GS_PLAYING)
		//{
		//	color[3] = fabs(sin(cg.time * 0.002));
		//}

		// FIXME: we might do different views x/Y or in %
		//result = va("%i/%i", cgs.sampledStat.avg, cgs.sv_fps);
		result = va("%i", cgs.sampledStat.avg);

		w  = CG_Text_Width_Ext(result, comp->scale, 0, &cgs.media.limboFont1);
		w2 = (comp->location.w > w) ? comp->location.w : w;
		x  = comp->location.x;

		CG_Text_Paint_Ext(x + ((w2 - w) / 2) + 2, y + 11, comp->scale, comp->scale, *clr, result, 0, 0, 0, &cgs.media.limboFont1);
	}
}

/**
 * @brief CG_Hud_Setup
 */
void CG_Hud_Setup(void)
{
	hudStucture_t hud0;

	// Hud0 aka the Default hud
	CG_setDefaultHudValues(&hud0);
	activehud = CG_addHudToList(&hud0);

	// Read the hud files
	CG_ReadHudScripts();
}

#ifdef ETLEGACY_DEBUG

/**
 * @brief CG_PrintHudComponent
 * @param[in] name
 * @param[in] comp
 */
static void CG_PrintHudComponent(const char *name, hudComponent_t *comp)
{
	Com_Printf("%s location: X %.f Y %.f W %.f H %.f visible: %i\n", name, comp->location.x, comp->location.y, comp->location.w, comp->location.h, comp->visible);
}

/**
 * @brief CG_PrintHud
 * @param[in] hud
 */
static void CG_PrintHud(hudStucture_t *hud)
{
	int i;

	for (i = 0; hudComponentFields[i].name; i++)
	{
		if (!hudComponentFields[i].isAlias)
		{
			CG_PrintHudComponent(hudComponentFields[i].name, (hudComponent_t *)((char * )hud + hudComponentFields[i].offset));
		}
	}
}
#endif

/**
 * @brief CG_SetHud
 */
void CG_SetHud(void)
{
	if (cg_altHud.integer && activehud->hudnumber != cg_altHud.integer)
	{
		activehud = CG_getHudByNumber(cg_altHud.integer);
		if (!activehud)
		{
			Com_Printf("^3WARNING hud with number %i is not available, defaulting to 0\n", cg_altHud.integer);
			activehud = CG_getHudByNumber(0);
			trap_Cvar_Set("cg_altHud", "0");
			return;
		}

#ifdef ETLEGACY_DEBUG
		CG_PrintHud(activehud);
#endif

		Com_Printf("Setting hud to: %i\n", cg_altHud.integer);
	}
	else if (!cg_altHud.integer && activehud->hudnumber)
	{
		activehud = CG_getHudByNumber(0);
	}
}

/**
 * @brief CG_DrawActiveHud
 */
void CG_DrawActiveHud(void)
{
	unsigned int   i;
	hudComponent_t *comp;

	for (i = 0; i < HUD_COMPONENTS_NUM; i++)
	{
		comp = activehud->components[i];

		if (comp && comp->visible && comp->draw)
		{
			comp->draw(comp);
		}
	}

	// Stats Debugging
	CG_DrawStatsDebug();
}
