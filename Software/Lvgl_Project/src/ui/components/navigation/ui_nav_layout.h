#ifndef UI_NAV_LAYOUT_H
#define UI_NAV_LAYOUT_H

#include "../../design/ui_design_tokens.h"

/* Navigation V2 fixed geometry. The page status bar ends at y=56 and the
 * existing status divider occupies the following few pixels, so the map starts
 * at y=62. The remaining 792 px are split into a 500 px map, a 92 px data
 * strip and a 200 px command region. */
#define UI_NAV_PAGE_MARGIN       10
#define UI_NAV_MAP_TOP           62
#define UI_NAV_MAP_WIDTH        460
#define UI_NAV_MAP_HEIGHT       500
#define UI_NAV_DATA_TOP         (UI_NAV_MAP_TOP + UI_NAV_MAP_HEIGHT)
#define UI_NAV_DATA_WIDTH       UI_NAV_MAP_WIDTH
#define UI_NAV_DATA_HEIGHT       92
#define UI_NAV_DOCK_REGION_TOP  (UI_NAV_DATA_TOP + UI_NAV_DATA_HEIGHT)
#define UI_NAV_DOCK_X            10
#define UI_NAV_DOCK_Y           (UI_NAV_DOCK_REGION_TOP + 2)
#define UI_NAV_DOCK_WIDTH       460
#define UI_NAV_DOCK_HEIGHT      188
#define UI_NAV_DOCK_BOTTOM      (UI_NAV_DOCK_Y + UI_NAV_DOCK_HEIGHT)

#endif /* UI_NAV_LAYOUT_H */
