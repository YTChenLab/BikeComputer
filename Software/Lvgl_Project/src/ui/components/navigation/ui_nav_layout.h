#ifndef UI_NAV_LAYOUT_H
#define UI_NAV_LAYOUT_H

#include "../../design/ui_design_tokens.h"

/* Navigation V2 uses the same product bottom edge as all other pages. */
#define UI_NAV_PAGE_MARGIN       10
#define UI_NAV_MAP_TOP           62
#define UI_NAV_MAP_WIDTH        460
#define UI_NAV_MAP_HEIGHT       (UI_PAGE_CONTENT_BOTTOM - UI_NAV_MAP_TOP - UI_NAV_DATA_HEIGHT - UI_NAV_DOCK_HEIGHT - 2)
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
