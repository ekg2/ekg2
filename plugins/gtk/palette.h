extern GdkColor colors[];

#define COL_MARK_FG 32
#define COL_MARK_BG 33
#define COL_FG 34
#define COL_BG 35
#define COL_MARKER 36
#define COL_NEW_DATA 37
#define COL_HILIGHT 38
#define COL_NEW_MSG 39
#define COL_AWAY 40

void palette_alloc(GtkWidget *widget);
void pixmaps_init(void);

extern GdkPixbuf *pix_ekg2;
extern GdkPixbuf *pixs[];
extern GdkPixbuf *gg_pixs[];

#define PIXBUF_FFC 0
#define PIXBUF_AVAIL 1
#define PIXBUF_AWAY 2
#define PIXBUF_DND 3
#define PIXBUF_XA 4
#define PIXBUF_INVISIBLE 5
#define PIXBUF_NOTAVAIL 6
#define PIXBUF_ERROR 7
#define PIXBUF_UNKNOWN 8

#define STATUS_PIXBUFS 9 /* FFC, AVAIL, AWAY, DND, XA, INVISIBLE, NOTAVAIL, ERROR, UNKNOWN */

