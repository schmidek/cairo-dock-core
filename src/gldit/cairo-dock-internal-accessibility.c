/**
* This file is a part of the Cairo-Dock project
*
* Copyright : (C) see the 'copyright' file.
* E-mail    : see the 'copyright' file.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 3
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include "cairo-dock-modules.h"
#include "cairo-dock-config.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-load.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-dock-facility.h"

CairoConfigAccessibility myAccessibility;
extern CairoDock *g_pMainDock;
extern CairoDockHidingEffect *g_pHidingBackend;

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigAccessibility *pAccessibility)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	//\____________________ Visibilite
	int iVisibility = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "visibility", &bFlushConfFileNeeded, -1, NULL, NULL);  // -1 pour pouvoir intercepter le cas ou la cle n'existe pas.
	gboolean bRaiseOnShortcut = FALSE;
	
	gchar *cShortkey = cairo_dock_get_string_key_value (pKeyFile, "Accessibility", "raise shortcut", &bFlushConfFileNeeded, NULL, "Position", NULL);
	
	pAccessibility->cHideEffect = cairo_dock_get_string_key_value (pKeyFile, "Accessibility", "hide effect", &bFlushConfFileNeeded, NULL, NULL, NULL);
	
	if (iVisibility == -1)  // option nouvelle 2.1.3
	{
		if (g_key_file_get_boolean (pKeyFile, "Accessibility", "reserve space", NULL))
			iVisibility = CAIRO_DOCK_VISI_KEEP_ABOVE;
		else if (g_key_file_get_boolean (pKeyFile, "Accessibility", "pop-up", NULL))  // on force au nouveau mode.
		{
			iVisibility = CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY;
			pAccessibility->cHideEffect = g_strdup_printf ("Fade out");  // on force a "fade out" pour garder le meme effet.
			g_key_file_set_string (pKeyFile, "Accessibility", "hide effect", pAccessibility->cHideEffect);
		}
		else if (g_key_file_get_boolean (pKeyFile, "Accessibility", "auto-hide", NULL))
			iVisibility = CAIRO_DOCK_VISI_AUTO_HIDE;
		else if (g_key_file_get_boolean (pKeyFile, "Accessibility", "auto quick hide on max", NULL))
			iVisibility = CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY;
		else if (cShortkey)
		{
			iVisibility = CAIRO_DOCK_VISI_SHORTKEY;
			pAccessibility->cRaiseDockShortcut = cShortkey;
			cShortkey = NULL;
		}
		else
			iVisibility = CAIRO_DOCK_VISI_KEEP_ABOVE;
		
		g_key_file_set_integer (pKeyFile, "Accessibility", "visibility", iVisibility);
	}
	else
	{
		if (pAccessibility->cHideEffect == NULL)  // nouvelle option 2.2.0, cela a change l'ordre du menu.
		{
			// avant c'etait : KEEP_ABOVE, RESERVE, KEEP_BELOW, AUTO_HIDE,       HIDE_ON_MAXIMIZED,   SHORTKEY
			// mtn c'est     : KEEP_ABOVE, RESERVE, KEEP_BELOW, HIDE_ON_OVERLAP, HIDE_ON_OVERLAP_ANY, AUTO_HIDE, VISI_SHORTKEY,
			if (iVisibility == 2)  // on force au nouveau mode.
			{
				iVisibility = CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY;
				pAccessibility->cHideEffect = g_strdup_printf ("Fade out");  // on force a "fade out" pour garder le meme effet.
				g_key_file_set_integer (pKeyFile, "Accessibility", "visibility", iVisibility);
				g_key_file_set_string (pKeyFile, "Accessibility", "hide effect", pAccessibility->cHideEffect);
			}
			else if (iVisibility == 3)
			{
				iVisibility = CAIRO_DOCK_VISI_AUTO_HIDE;
				g_key_file_set_integer (pKeyFile, "Accessibility", "visibility", iVisibility);
			}
			else if (iVisibility == 4)
			{
				iVisibility = CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY;
				g_key_file_set_integer (pKeyFile, "Accessibility", "visibility", iVisibility);
			}
			else if (iVisibility == 5)
			{
				iVisibility = CAIRO_DOCK_VISI_SHORTKEY;
				g_key_file_set_integer (pKeyFile, "Accessibility", "visibility", iVisibility);
			}
		}
		if (iVisibility == CAIRO_DOCK_VISI_SHORTKEY)
		{
			if (cShortkey == NULL)
			{
				cd_warning ("no shortcut defined to make the dock appear, we'll keep it above.");
				iVisibility = CAIRO_DOCK_VISI_KEEP_ABOVE;
			}
			else
			{
				pAccessibility->cRaiseDockShortcut = cShortkey;
				cShortkey = NULL;
			}
		}
	}
	pAccessibility->iVisibility = iVisibility;
	g_free (cShortkey);
	if (pAccessibility->cHideEffect == NULL) 
	{
		pAccessibility->cHideEffect = g_strdup_printf ("Move down");
		g_key_file_set_string (pKeyFile, "Accessibility", "hide effect", pAccessibility->cHideEffect);
	}
	
	pAccessibility->iCallbackMethod = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "callback", &bFlushConfFileNeeded, 0, NULL, NULL);
	
	if (pAccessibility->iCallbackMethod == 3)
	{
		if (! g_key_file_has_key (pKeyFile, "Accessibility", "zone size", NULL))
		{
			pAccessibility->iZoneWidth = 100;
			pAccessibility->iZoneHeight = 10;
			int list[2] = {pAccessibility->iZoneWidth, pAccessibility->iZoneHeight};
			g_key_file_set_integer_list (pKeyFile, "Accessibility", "zone size", list, 2);
		}
		cairo_dock_get_size_key_value_helper (pKeyFile, "Accessibility", "zone ", bFlushConfFileNeeded, pAccessibility->iZoneWidth, pAccessibility->iZoneHeight);
		if (pAccessibility->iZoneWidth < 20)
			pAccessibility->iZoneWidth = 20;
		if (pAccessibility->iZoneHeight < 2)
			pAccessibility->iZoneHeight = 2;
		pAccessibility->cZoneImage = cairo_dock_get_string_key_value (pKeyFile, "Accessibility", "callback image", &bFlushConfFileNeeded, 0, "Background", NULL);
		pAccessibility->fZoneAlpha = 1.;  // on laisse l'utilisateur definir la transparence qu'il souhaite directement dans l'image.
	}
	
	//\____________________ Autres parametres.
	pAccessibility->iMaxAuthorizedWidth = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "max_authorized_width", &bFlushConfFileNeeded, 0, "Position", NULL);  // obsolete, cache en conf.
	pAccessibility->bExtendedMode = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "extended", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	
	pAccessibility->iUnhideDockDelay = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "unhide delay", &bFlushConfFileNeeded, 0, NULL, NULL);
	
	pAccessibility->bAutoHideOnFullScreen = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "auto quick hide", &bFlushConfFileNeeded, FALSE, "TaskBar", NULL);
	//pAccessibility->bAutoHideOnFullScreen = FALSE;
	
	//\____________________ sous-docks.
	pAccessibility->iLeaveSubDockDelay = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "leaving delay", &bFlushConfFileNeeded, 330, "System", NULL);
	pAccessibility->iShowSubDockDelay = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "show delay", &bFlushConfFileNeeded, 300, "System", NULL);
	if (!g_key_file_has_key (pKeyFile, "Accessibility", "show_on_click", NULL))
	{
		pAccessibility->bShowSubDockOnClick = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "show on click", &bFlushConfFileNeeded, FALSE, "System", NULL);
		g_key_file_set_integer (pKeyFile, "Accessibility", "show_on_click", pAccessibility->bShowSubDockOnClick ? 1 : 0);
		bFlushConfFileNeeded = TRUE;
	}
	else
		pAccessibility->bShowSubDockOnClick = (cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "show_on_click", &bFlushConfFileNeeded, 0, NULL, NULL) == 1);
	
	//\____________________ lock
	pAccessibility->bLockAll = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "lock all", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	pAccessibility->bLockIcons = pAccessibility->bLockAll || cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "lock icons", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	
	return bFlushConfFileNeeded;
}


static void reset_config (CairoConfigAccessibility *pAccessibility)
{
	g_free (pAccessibility->cRaiseDockShortcut);
	g_free (pAccessibility->cHideEffect);
	g_free (pAccessibility->cZoneImage);
}


static void _init_hiding (CairoDock *pDock, gpointer data)
{
	if (pDock->bIsShowing || pDock->bIsHiding)
	{
		g_pHidingBackend->init (pDock);
	}
}
static void reload (CairoConfigAccessibility *pPrevAccessibility, CairoConfigAccessibility *pAccessibility)
{
	CairoDock *pDock = g_pMainDock;
	
	if (! pDock)
	{
		if (pPrevAccessibility->cRaiseDockShortcut != NULL)
			cd_keybinder_unbind (pPrevAccessibility->cRaiseDockShortcut, (CDBindkeyHandler) cairo_dock_raise_from_shortcut);
		return ;
	}
	
	//\_______________ Shortcut.
	if (pPrevAccessibility->cRaiseDockShortcut != NULL)  // il y'a un ancien raccourci.
	{
		if (pAccessibility->cRaiseDockShortcut == NULL || strcmp (pAccessibility->cRaiseDockShortcut, pPrevAccessibility->cRaiseDockShortcut) != 0)  // le raccourci a change, ou n'est plus d'actualite => on le un-bind ou on le re-bind.
		{
			cd_keybinder_unbind (pPrevAccessibility->cRaiseDockShortcut, (CDBindkeyHandler) cairo_dock_raise_from_shortcut);
			
			if (pAccessibility->cRaiseDockShortcut != NULL)  // le raccourci a seulement change, mais l'option est toujours d'actualite, le set_visibility ne verra pas la difference, donc on re-binde.
			{
				if (! cd_keybinder_bind (pAccessibility->cRaiseDockShortcut, (CDBindkeyHandler) cairo_dock_raise_from_shortcut, NULL))  // le bind n'a pas pu se faire.
				{
					g_free (pAccessibility->cRaiseDockShortcut);
					pAccessibility->cRaiseDockShortcut = NULL;
					pAccessibility->iVisibility = CAIRO_DOCK_VISI_KEEP_ABOVE;
				}
			}
		}
	}
	
	//\_______________ Max Size.
	if (pAccessibility->iMaxAuthorizedWidth != pPrevAccessibility->iMaxAuthorizedWidth ||
		pAccessibility->bExtendedMode != pPrevAccessibility->bExtendedMode)
	{
		cairo_dock_set_all_views_to_default (1);  // 1 <=> tous les docks racines. met a jour la taille et reserve l'espace.
	}
	
	//\_______________ Hiding effect.
	if (cairo_dock_strings_differ (pAccessibility->cHideEffect, pPrevAccessibility->cHideEffect))
	{
		g_pHidingBackend = cairo_dock_get_hiding_effect (pAccessibility->cHideEffect);
		if (g_pHidingBackend && g_pHidingBackend->init)
		{
			cairo_dock_foreach_root_docks ((GFunc)_init_hiding, NULL);  // si le dock est en cours d'animation, comme le backend est nouveau, il n'a donc pas ete initialise au debut de l'animation => on le fait ici.
		}
	}
	
	//\_______________ Callback zone.
	if (cairo_dock_strings_differ (pAccessibility->cZoneImage, pPrevAccessibility->cZoneImage) ||
		pAccessibility->iZoneWidth != pPrevAccessibility->iZoneWidth ||
		pAccessibility->iZoneHeight != pPrevAccessibility->iZoneHeight ||
		pAccessibility->fZoneAlpha != pPrevAccessibility->fZoneAlpha)
	{
		cairo_dock_load_visible_zone (pAccessibility->cZoneImage, pAccessibility->iZoneWidth, pAccessibility->iZoneHeight, pAccessibility->fZoneAlpha);
		
		cairo_dock_redraw_root_docks (FALSE);  // FALSE <=> main dock inclus.
	}
	
	cairo_dock_set_dock_visibility (pDock, pAccessibility->iVisibility);
}



DEFINE_PRE_INIT (Accessibility)
{
	pModule->cModuleName = "Accessibility";
	pModule->cTitle = N_("Visibility");
	pModule->cIcon = "icon-visibility.svg";
	pModule->cDescription = N_("Do you like your dock to be always visible,\n or on the contrary unobtrusive?\nConfigure the way you access your docks and sub-docks!");  // How do you access to your docks ?
	pModule->iCategory = CAIRO_DOCK_CATEGORY_BEHAVIOR;
	pModule->iSizeOfConfig = sizeof (CairoConfigAccessibility);
	pModule->iSizeOfData = 0;
	
	pModule->reload = (CairoDockInternalModuleReloadFunc) reload;
	pModule->get_config = (CairoDockInternalModuleGetConfigFunc) get_config;
	pModule->reset_config = (CairoDockInternalModuleResetConfigFunc) reset_config;
	pModule->reset_data = NULL;
	
	pModule->pConfig = &myAccessibility;
	pModule->pData = NULL;
}
