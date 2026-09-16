#ifndef UI_NAV_LAYOUT_H
#define UI_NAV_LAYOUT_H

#include "../../design/ui_design_tokens.h"

/* Navigation page geometry. Keep the map and 2 x 2 data grid aligned to the
 * same 10 px page margin. The Bottom Interaction Surface occupies its shared
 * page slot only while an event is active. */
#define UI_NAV_PAGE_MARGIN       10
#define UI_NAV_MAP_TOP           62
#define UI_NAV_MAP_WIDTH        460
#define UI_NAV_MAP_HEIGHT       460
#define UI_NAV_SECTION_GAP        8
#define UI_NAV_GRID_TOP         (UI_NAV_MAP_TOP + UI_NAV_MAP_HEIGHT + \
                                 UI_NAV_SECTION_GAP)
#define UI_NAV_GRID_WIDTH       UI_NAV_MAP_WIDTH
#define UI_NAV_GRID_HEIGHT      314
#define UI_NAV_CARD_GAP           6
#define UI_NAV_CARD_WIDTH       227
#define UI_NAV_CARD_HEIGHT      154

#endif /* UI_NAV_LAYOUT_H */
