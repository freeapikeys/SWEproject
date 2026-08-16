/* ==========================================================================
 *  AURA :: icons.h  --  hand-built vector icon set
 *
 *  Every glyph is drawn on a 24x24 grid with the canvas rasteriser, so icons
 *  stay sharp at any size and can be tinted or animated like any other art.
 * ========================================================================== */
#ifndef AURA_ICONS_H
#define AURA_ICONS_H

#include "canvas.h"

enum {
    IC_NONE = 0,
    IC_PLANE,          IC_TAKEOFF,      IC_LANDING,      IC_RADAR,
    IC_LUGGAGE,        IC_BELT,         IC_TAG,          IC_BOX,
    IC_CHAT,           IC_SPARK,        IC_BRAIN,        IC_CPU,
    IC_SHIELD,         IC_SCAN,         IC_USERS,        IC_USER,
    IC_GATE,           IC_TICKET,       IC_SEAT,         IC_PASSPORT,
    IC_CHART,          IC_TREND,        IC_GAUGE,        IC_LAYERS,
    IC_CLOCK,          IC_CALENDAR,     IC_SEARCH,       IC_FILTER,
    IC_CHECK,          IC_CLOSE,        IC_ALERT,        IC_INFO,
    IC_ARROW_L,        IC_ARROW_R,      IC_ARROW_U,      IC_ARROW_D,
    IC_CHEV_L,         IC_CHEV_R,       IC_CHEV_U,       IC_CHEV_D,
    IC_PLAY,           IC_PAUSE,        IC_FFWD,         IC_REFRESH,
    IC_SETTINGS,       IC_BELL,         IC_LOCK,         IC_PIN,
    IC_FILE,           IC_DATABASE,     IC_SAVE,         IC_FOLDER,
    IC_PLUS,           IC_MINUS,        IC_TRASH,        IC_EDIT,
    IC_EYE,            IC_SEND,         IC_MIC,          IC_GRID,
    IC_SUN,            IC_CLOUD,        IC_RAIN,         IC_WIND,
    IC_FUEL,           IC_TRUCK,        IC_TOWER,        IC_RUNWAY,
    IC_BOLT,           IC_HEART,        IC_STAR,         IC_GLOBE,
    IC_MENU,           IC_MORE,         IC_LINK,         IC_DOWNLOAD,
    IC_COUNT
};

/* draw an icon centred on (cx,cy), fitted to `size` pixels */
void icon_draw(Canvas *c, int id, float cx, float cy, float size, Color col);
void icon_draw_w(Canvas *c, int id, float cx, float cy, float size, Color col,
                 float stroke);

#endif /* AURA_ICONS_H */
