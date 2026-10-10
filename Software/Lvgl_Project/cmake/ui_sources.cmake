# Shared implementation selection: no entry/HAL/PC/test source belongs here.
file(GLOB_RECURSE UI_COMMON_UI_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/ui/*.c")
list(FILTER UI_COMMON_UI_SOURCES EXCLUDE REGEX "/ui_nav_card\\.c$")
# Fixed checked-in generated list also works after a PC clean-first build.
set(UI_GENERATED_FONT_NAMES ui_20 ui_24 metric_60 zone_24 num_68 nav_metric_40 nav_letter_40 nav_action_32 nav_unit_16)
foreach(name IN LISTS UI_GENERATED_FONT_NAMES)
    set(font_path "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_${name}.c")
    list(REMOVE_ITEM UI_COMMON_UI_SOURCES "${font_path}")
    list(APPEND UI_COMMON_UI_SOURCES "${font_path}")
endforeach()
file(GLOB_RECURSE UI_COMMON_SERVICE_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/services/*.c")
set(UI_CORE_SOURCES ${UI_COMMON_UI_SOURCES} ${UI_COMMON_SERVICE_SOURCES})
