/* X-Chat
 * Copyright (C) 1998 Peter Zelezny.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/*
 *  port to ekg2:
 *  Copyright (C) 2007 Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *			
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define GTK_DISABLE_DEPRECATED

/* fix includes */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include <gtk/gtkstock.h>

#include "main.h"
#include "palette.h"

#include "inline_pngs.h"
#include "inline_pngs_gg.h"

/* XXX, todo: colors[0..15] are ok, than colors[16..31] are not ok [duplication of above], invistigate xchat sources if they needed, fix and remove. */
#warning "XXX, colors[16..31] remove!"

GdkColor colors[] = {
#if XCHAT_COLORS
	/* colors for xtext */
	{0, 0x0000, 0x0000, 0x0000},	/* 17 black [30 V] */
	{0, 0xc3c3, 0x3b3b, 0x3b3b},	/* 20 red [31 V] */
	{0, 0x2a3d, 0x8ccc, 0x2a3d},	/* 19 green [32 V] */
	{0, 0xd999, 0xa6d3, 0x4147},	/* 24 yellow [33 V] */
	{0, 0x35c2, 0x35c2, 0xb332},	/* 18 blue [34 V] */
	{0, 0x8000, 0x2666, 0x7fff},	/* 22 purple [35] V */
	{0, 0x199a, 0x5555, 0x5555},	/* 26 aqua [36] V */
	{0, 0xcccc, 0xcccc, 0xcccc},	/* 16 white [37 V] */

	{0, 0x4c4c, 0x4c4c, 0x4c4c},	/* 30 grey [30b V] */
	{0, 0xc7c7, 0x3232, 0x3232},	/* 21 light red [31b V] */
	{0, 0x3d70, 0xcccc, 0x3d70},	/* 25 green [32b V] */
	{0, 0x6666, 0x3636, 0x1f1f},	/* 23 orange [33b V] */
	{0, 0x451e, 0x451e, 0xe666},	/* 28 blue [34b V] */
	{0, 0xb0b0, 0x3737, 0xb0b0},	/* 29 light purple [35b V] */
	{0, 0x2eef, 0x8ccc, 0x74df},	/* 27 light aqua [36b V] */
	{0, 0x9595, 0x9595, 0x9595},	/* 31 light grey [37b V] */

	{0, 0xcccc, 0xcccc, 0xcccc},	/* 16 white */
	{0, 0x0000, 0x0000, 0x0000},	/* 17 black */
	{0, 0x35c2, 0x35c2, 0xb332},	/* 18 blue */
	{0, 0x2a3d, 0x8ccc, 0x2a3d},	/* 19 green */
	{0, 0xc3c3, 0x3b3b, 0x3b3b},	/* 20 red */
	{0, 0xc7c7, 0x3232, 0x3232},	/* 21 light red */
	{0, 0x8000, 0x2666, 0x7fff},	/* 22 purple */
	{0, 0x6666, 0x3636, 0x1f1f},	/* 23 orange */
	{0, 0xd999, 0xa6d3, 0x4147},	/* 24 yellow */
	{0, 0x3d70, 0xcccc, 0x3d70},	/* 25 green */
	{0, 0x199a, 0x5555, 0x5555},	/* 26 aqua */
	{0, 0x2eef, 0x8ccc, 0x74df},	/* 27 light aqua */
	{0, 0x451e, 0x451e, 0xe666},	/* 28 blue */
	{0, 0xb0b0, 0x3737, 0xb0b0},	/* 29 light purple */
	{0, 0x4c4c, 0x4c4c, 0x4c4c},	/* 30 grey */
	{0, 0x9595, 0x9595, 0x9595},	/* 31 light grey */

	{0, 0xffff, 0xffff, 0xffff},	/* 32 marktext Fore (white) */
	{0, 0x3535, 0x6e6e, 0xc1c1},	/* 33 marktext Back (blue) */
	{0, 0x0000, 0x0000, 0x0000},	/* 34 foreground (black) */
	{0, 0xf0f0, 0xf0f0, 0xf0f0},	/* 35 background (white) */
	{0, 0xcccc, 0x1010, 0x1010},	/* 36 marker line (red) */

	/* colors for GUI */
	{0, 0x9999, 0x0000, 0x0000},	/* 37 tab New Data (dark red) */
	{0, 0x0000, 0x0000, 0xffff},	/* 38 tab Nick Mentioned (blue) */
	{0, 0xffff, 0x0000, 0x0000},	/* 39 tab New Message (red) */
	{0, 0x9595, 0x9595, 0x9595},	/* 40 away user (grey) */
#else
/* these are from http://xchat.org/files/themes/blacktheme.zip ;) */

	{0, 0x0000, 0x0000, 0x0000},	/* 17 */
	{0, 0xdddd, 0x0000, 0x0000},	/* 20 */
	{0, 0x0000, 0xcccc, 0x0000},	/* 19 */
	{0, 0xeeee, 0xdddd, 0x2222},	/* 24 */
	{0, 0x0000, 0x0000, 0xcccc},	/* 18 */
	{0, 0xbbbb, 0x0000, 0xbbbb},	/* 22 */
	{0, 0x0000, 0xcccc, 0xcccc},	/* 26 */
	{0, 0xcf3c, 0xcf3c, 0xcf3c},	/* 16 */

	{0, 0x7777, 0x7777, 0x7777},	/* 30 */
	{0, 0xaaaa, 0x0000, 0x0000},	/* 21 */
	{0, 0x3333, 0xdede, 0x5555},	/* 25 */
	{0, 0xffff, 0xaaaa, 0x0000},	/* 23 */
	{0, 0x0000, 0x0000, 0xffff},	/* 28 */
	{0, 0xeeee, 0x2222, 0xeeee},	/* 29 */
	{0, 0x3333, 0xeeee, 0xffff},	/* 27 */
	{0, 0x9999, 0x9999, 0x9999},	/* 31 */
/* duplicate */
	{0, 0xcf3c, 0xcf3c, 0xcf3c},
	{0, 0x0000, 0x0000, 0x0000},
	{0, 0x0000, 0x0000, 0xcccc},
	{0, 0x0000, 0xcccc, 0x0000},
	{0, 0xdddd, 0x0000, 0x0000},
	{0, 0xaaaa, 0x0000, 0x0000},
	{0, 0xbbbb, 0x0000, 0xbbbb},
	{0, 0xffff, 0xaaaa, 0x0000},
	{0, 0xeeee, 0xdddd, 0x2222},
	{0, 0x3333, 0xdede, 0x5555},
	{0, 0x0000, 0xcccc, 0xcccc},
	{0, 0x3333, 0xeeee, 0xffff},
	{0, 0x0000, 0x0000, 0xffff},
	{0, 0xeeee, 0x2222, 0xeeee},
	{0, 0x7777, 0x7777, 0x7777},
	{0, 0x9999, 0x9999, 0x9999},

	{0, 0x0000, 0x0000, 0x0000},
	{0, 0xa4a4, 0xdfdf, 0xffff},
	{0, 0xdf3c, 0xdf3c, 0xdf3c},
	{0, 0x0000, 0x0000, 0x0000},
	{0, 0xcccc, 0x1010, 0x1010},

	{0, 0x8c8c, 0x1010, 0x1010},
	{0, 0x0000, 0x0000, 0xffff},
	{0, 0xf5f5, 0x0000, 0x0000},
	{0, 0x9999, 0x9999, 0x9999},
#endif
};

#define MAX_COL 40


void palette_alloc(GtkWidget *widget)
{
	int i;
	static int done_alloc = FALSE;
	GdkColormap *cmap;

	if (!done_alloc) {	/* don't do it again */
		done_alloc = TRUE;
		cmap = gtk_widget_get_colormap(widget);
		for (i = MAX_COL; i >= 0; i--)
			gdk_colormap_alloc_color(cmap, &colors[i], FALSE, TRUE);
	}
}

GdkPixbuf *pix_ekg2;
GdkPixbuf *pixs[STATUS_PIXBUFS];
GdkPixbuf *gg_pixs[STATUS_PIXBUFS];

void pixmaps_init(void)
{
	/* XXX, here load from file, or inline from .h */
		/* gdk_pixbuf_new_from_file() */
		/* gdk_pixbuf_new_from_inline() */
	pix_ekg2 = NULL;

	memset(gg_pixs, 0, sizeof(gg_pixs));

	gg_pixs[PIXBUF_AVAIL] = gdk_pixbuf_new_from_inline(-1, gg_avail, FALSE, 0);
	gg_pixs[PIXBUF_AWAY] = gdk_pixbuf_new_from_inline(-1, gg_away, FALSE, 0);
	gg_pixs[PIXBUF_INVISIBLE] = gdk_pixbuf_new_from_inline(-1, gg_invisible, FALSE, 0);
	gg_pixs[PIXBUF_NOTAVAIL] = gdk_pixbuf_new_from_inline(-1, gg_notavail, FALSE, 0);
	
	pixs[PIXBUF_FFC] = gdk_pixbuf_new_from_inline(-1, ffc, FALSE, 0);
	pixs[PIXBUF_AVAIL] = gdk_pixbuf_new_from_inline(-1, avail, FALSE, 0);
	pixs[PIXBUF_AWAY] = gdk_pixbuf_new_from_inline(-1, away, FALSE, 0);
	pixs[PIXBUF_DND] = gdk_pixbuf_new_from_inline(-1, dnd, FALSE, 0);
	pixs[PIXBUF_XA] = gdk_pixbuf_new_from_inline(-1, xa, FALSE, 0);
	pixs[PIXBUF_INVISIBLE] = gdk_pixbuf_new_from_inline(-1, invisible, FALSE, 0);
	pixs[PIXBUF_NOTAVAIL] = gdk_pixbuf_new_from_inline(-1, notavail, FALSE, 0);
	pixs[PIXBUF_ERROR] = gdk_pixbuf_new_from_inline(-1, icon_error, FALSE, 0);
	pixs[PIXBUF_UNKNOWN] = gdk_pixbuf_new_from_inline(-1, icon_unknown, FALSE, 0);
}

