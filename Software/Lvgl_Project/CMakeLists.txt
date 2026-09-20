cmake_minimum_required(VERSION 3.12.4)

# CMake 3.31+ policy:
# Normalize install() DESTINATION paths before processing.
# Guarded so older CMake versions remain compatible.
if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0177 NEW)
endif()

project(lvgl C CXX)

include(CTest)

# Set the correct FreeRTOS port for your system (e.g., Posix for WSL)
set(FREERTOS_PORT GCC_POSIX CACHE STRING "Port for FreeRTOS on Posix environment")

set(LVGL_PRO_PROJECT_DIR
    ""
    CACHE PATH "Path to LVGL Pro Project folder")

option(USE_FREERTOS "Enable FreeRTOS" OFF) # Turn this on to enable FreeRTOS

if(USE_FREERTOS)
    message(STATUS "FreeRTOS is enabled")

    # FreeRTOS integration when USE_FREERTOS is enabled
    add_library(freertos_config INTERFACE)
    target_include_directories(freertos_config SYSTEM INTERFACE ${PROJECT_SOURCE_DIR}/config)
    target_compile_definitions(freertos_config INTERFACE projCOVERAGE_TEST=0)

    # Add FreeRTOS as a subdirectory
    add_subdirectory(FreeRTOS)

    # FreeRTOS-specific include directories
    include_directories(${PROJECT_SOURCE_DIR}/FreeRTOS/include)
    include_directories(${PROJECT_SOURCE_DIR}/FreeRTOS/portable/ThirdParty/GCC/Posix)
    include_directories(${PROJECT_SOURCE_DIR}/config)

    # Add FreeRTOS sources
    file(GLOB FREERTOS_SOURCES
        "${PROJECT_SOURCE_DIR}/FreeRTOS/*.c"
        "${PROJECT_SOURCE_DIR}/FreeRTOS/portable/MemMang/heap_4.c"
        "${PROJECT_SOURCE_DIR}/FreeRTOS/portable/ThirdParty/GCC/Posix/*.c"
    )
else()
    message(STATUS "FreeRTOS is disabled")
    set(FREERTOS_SOURCES "")  # No FreeRTOS sources if FreeRTOS is disabled
endif()

# Define options for LVGL with default values (OFF)
option(LV_USE_DRAW_SDL "Use SDL draw unit" OFF)
option(LV_USE_LIBPNG "Use libpng to decode PNG" OFF)
option(LV_USE_LIBJPEG_TURBO "Use libjpeg turbo to decode JPEG" OFF)
option(LV_USE_FFMPEG "Use libffmpeg to display video using lv_ffmpeg" OFF)
option(LV_USE_FREETYPE "Use freetype library" OFF)

# Set C and C++ standards
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the output path for the executable
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

# Find and include SDL2 library
find_package(SDL2 REQUIRED)

# Add compile definitions based on the selected options
add_compile_definitions($<$<BOOL:${LV_USE_DRAW_SDL}>:LV_USE_DRAW_SDL=1>)
add_compile_definitions($<$<BOOL:${LV_USE_LIBPNG}>:LV_USE_LIBPNG=1>)
add_compile_definitions($<$<BOOL:${LV_USE_LIBJPEG_TURBO}>:LV_USE_LIBJPEG_TURBO=1>)
add_compile_definitions($<$<BOOL:${LV_USE_FFMPEG}>:LV_USE_FFMPEG=1>)

# Add LVGL subdirectory
add_subdirectory(lvgl)
target_include_directories(lvgl PUBLIC ${PROJECT_SOURCE_DIR} ${SDL2_INCLUDE_DIRS})

set(
    MAIN_SOURCES

    src/hal/hal.c
    src/perf_benchmark.c
)

# ============================================================
# 自定义 UI 源文件
# ============================================================

file(
    GLOB_RECURSE
    UI_SOURCES
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/ui/*.c"
)
# This generated source may not exist during the first configure of a clean
# checkout. Keep it out of the glob and add it explicitly below.
list(FILTER UI_SOURCES EXCLUDE REGEX "/ui_font_misans_ui_24\\.c$")
list(FILTER UI_SOURCES EXCLUDE REGEX "/ui_font_misans_nav_metric_40\\.c$")
list(FILTER UI_SOURCES EXCLUDE REGEX "/ui_font_misans_nav_action_32\\.c$")
list(FILTER UI_SOURCES EXCLUDE REGEX "/ui_font_misans_nav_unit_16\\.c$")
# Navigation V2 replaces the legacy 2 x 2 card implementation. Keep the old
# source in history for reference, but do not compile dead UI into the target.
list(FILTER UI_SOURCES EXCLUDE REGEX "/ui_nav_card\\.c$")

file(
    GLOB_RECURSE
    UI_RUNTIME_TEXT_SOURCES
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/ui/*.c"
    "${PROJECT_SOURCE_DIR}/src/ui/*.h"
)
list(FILTER UI_RUNTIME_TEXT_SOURCES EXCLUDE REGEX "/src/ui/fonts/")

find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(UI_FONT_GENERATED_FILES
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_ui_20.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_ui_24.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_metric_60.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_num_68.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_nav_metric_40.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_nav_action_32.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_nav_unit_16.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_runtime_codepoints.h"
)
add_custom_command(
    OUTPUT ${UI_FONT_GENERATED_FILES}
    COMMAND ${Python3_EXECUTABLE}
            "${PROJECT_SOURCE_DIR}/tools/generate_ui_font.py"
    DEPENDS
        "${PROJECT_SOURCE_DIR}/tools/generate_ui_font.py"
        "${PROJECT_SOURCE_DIR}/assets/fonts_src/MiSans-Semibold.ttf"
        "${PROJECT_SOURCE_DIR}/assets/fonts_src/MiSans-Heavy.ttf"
        ${UI_RUNTIME_TEXT_SOURCES}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Regenerating MiSans runtime UI subset"
    VERBATIM
)
add_custom_target(ui_font_generate DEPENDS ${UI_FONT_GENERATED_FILES})

file(
    GLOB_RECURSE
    SERVICE_SOURCES
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/services/*.c"
    "${PROJECT_SOURCE_DIR}/src/platform/pc/*.c"
)

list(
    APPEND
    MAIN_SOURCES
    ${UI_SOURCES}
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_ui_24.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_nav_metric_40.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_nav_action_32.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_nav_unit_16.c"
    ${SERVICE_SOURCES}
)
set(MAIN_LIBS lvgl lvgl::examples lvgl::demos lvgl::thorvg ${SDL2_LIBRARIES})

# Create the main executable, depending on the FreeRTOS option
if(USE_FREERTOS)
    list(APPEND MAIN_SOURCES src/freertos_main.c src/freertos/freertos_posix_port.c ${FREERTOS_SOURCES})
    list(APPEND MAIN_LIBS freertos_config freertos_kernel)
else()
    list(APPEND MAIN_SOURCES src/main.c)
endif()


if(NOT MSVC)
    list(APPEND MAIN_LIBS m pthread)
endif()

# Custom target to run the executable
add_custom_target(run COMMAND ${EXECUTABLE_OUTPUT_PATH}/main DEPENDS main)

# Conditionally include and link SDL2_image if LV_USE_DRAW_SDL is enabled
if(LV_USE_DRAW_SDL)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
    find_package(SDL2_image REQUIRED)
    target_include_directories(lvgl PUBLIC ${SDL2_IMAGE_INCLUDE_DIRS})
    list(APPEND MAIN_LIBS ${SDL2_IMAGE_LIBRARIES})
endif()

# Conditionally include and link libpng if LV_USE_LIBPNG is enabled
if(LV_USE_LIBPNG)
    find_package(PNG REQUIRED)
    target_include_directories(lvgl PUBLIC ${PNG_INCLUDE_DIRS})
    list(APPEND MAIN_LIBS ${PNG_LIBRARIES})
endif()

# Conditionally include and link libjpeg-turbo if LV_USE_LIBJPEG_TURBO is enabled
if(LV_USE_LIBJPEG_TURBO)
    find_package(JPEG REQUIRED)
    target_include_directories(lvgl PUBLIC ${JPEG_INCLUDE_DIRS})
    list(APPEND MAIN_LIBS ${JPEG_LIBRARIES})
endif()

# Conditionally include and link FFmpeg libraries if LV_USE_FFMPEG is enabled
if(LV_USE_FFMPEG)
    list(APPEND MAIN_LIBS avformat avcodec avutil swscale z)
endif()

# Conditionally include and link FreeType if LV_USE_FREETYPE is enabled
if(LV_USE_FREETYPE)
    find_package(Freetype REQUIRED)
    target_include_directories(lvgl PUBLIC ${FREETYPE_INCLUDE_DIRS})
    list(APPEND MAIN_LIBS ${FREETYPE_LIBRARIES})
endif()

if(LVGL_PRO_PROJECT_DIR)
  if(EXISTS "${LVGL_PRO_PROJECT_DIR}/CMakeLists.txt")
    add_subdirectory(${LVGL_PRO_PROJECT_DIR} ui)
    list(APPEND MAIN_LIBS lib-ui)

    target_compile_definitions(lib-ui PUBLIC LV_LVGL_H_INCLUDE_SIMPLE)
    target_include_directories(lib-ui PUBLIC
      ${LVGL_PRO_PROJECT_DIR}  # local headers like ui.h, generated files, etc.
      ${CMAKE_SOURCE_DIR}/lvgl # include "lvgl.h" works
    )
    message(STATUS "LVGL Pro Project enabled from: ${LVGL_PRO_PROJECT_DIR}")
  else()
    message(
      WARNING
        "LVGL_PRO_PROJECT_DIR specified but no CMakeLists.txt found at: ${LVGL_PRO_PROJECT_DIR}"
    )
  endif()
endif()

add_executable(main ${MAIN_SOURCES})
add_dependencies(main ui_font_generate)
target_include_directories(
    main
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_definitions(main PRIVATE LV_CONF_INCLUDE_SIMPLE)
target_link_libraries(main ${MAIN_LIBS})

if(BUILD_TESTING)
    add_executable(ui_font_coverage_test
        tests/ui_font_coverage_test.c
        src/ui/fonts/ui_font_misans_ui_20.c
        src/ui/fonts/ui_font_misans_ui_24.c
        src/ui/fonts/ui_font_misans_nav_metric_40.c
        src/ui/fonts/ui_font_misans_nav_action_32.c
        src/ui/fonts/ui_font_misans_nav_unit_16.c
        src/ui/fonts/ui_font_misans_en_16.c
        src/ui/fonts/ui_font_misans_en_18.c
    )
    add_dependencies(ui_font_coverage_test ui_font_generate)
    target_include_directories(ui_font_coverage_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}
    )
    target_compile_definitions(ui_font_coverage_test PRIVATE
        LV_CONF_INCLUDE_SIMPLE)
    target_link_libraries(ui_font_coverage_test lvgl)
    if(NOT MSVC)
        target_link_libraries(ui_font_coverage_test m pthread)
    endif()
    add_test(NAME ui_font_coverage_test COMMAND ui_font_coverage_test)

    add_executable(ui_app_state_test
        tests/ui_app_state_test.c
        src/ui/core/ui_app_state.c
        src/ui/core/ui_concurrency.c
    )
    target_include_directories(ui_app_state_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME ui_app_state_test COMMAND ui_app_state_test)

    add_executable(ui_home_data_model_test
        tests/ui_home_data_model_test.c
        src/ui/models/ui_home_data_model.c
    )
    target_include_directories(ui_home_data_model_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME ui_home_data_model_test COMMAND ui_home_data_model_test)

    add_executable(ui_weather_model_test
        tests/ui_weather_model_test.c
        src/ui/models/ui_weather_model.c
    )
    target_include_directories(ui_weather_model_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME ui_weather_model_test COMMAND ui_weather_model_test)

    add_executable(ui_back_key_gesture_test
        tests/ui_back_key_gesture_test.c
        src/ui/core/ui_back_key_gesture.c
    )
    target_include_directories(ui_back_key_gesture_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME ui_back_key_gesture_test COMMAND ui_back_key_gesture_test)

    add_executable(ui_auto_pause_test
        tests/ui_auto_pause_test.c
        src/ui/core/ui_auto_pause.c
    )
    target_include_directories(ui_auto_pause_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME ui_auto_pause_test COMMAND ui_auto_pause_test)

    add_executable(ui_core_logic_test
        tests/ui_core_logic_test.c
        src/ui/core/ui_dock.c
        src/ui/core/ui_dock_controller.c
        src/ui/core/ui_concurrency.c
        src/ui/core/ui_event_bridge.c
        src/ui/core/ui_event_bus.c
        src/ui/core/ui_input_queue.c
        src/ui/core/ui_ride_controller.c
        src/ui/core/ui_state_machine.c
        src/ui/core/ui_app_state.c
        src/services/ui_system_services.c
        src/services/ui_command_service.c
        src/services/ui_state_adapter.c
        src/services/ui_navigation_service.c
        src/services/ui_ride_history_repository.c
        src/platform/pc/ui_pc_services.c
        src/platform/pc/ui_pc_ride_history_storage.c
        src/ui/models/ui_ride_data_model.c
        src/ui/models/ui_ride_history_model.c
        src/ui/models/ui_ride_summary_model.c
        src/ui/models/ui_weather_model.c
        src/platform/stm32/ui_stm32_services.c
    )
    target_include_directories(ui_core_logic_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME ui_core_logic_test COMMAND ui_core_logic_test)

    add_executable(ui_dock_view_test
        tests/ui_dock_view_test.c
        src/ui/components/dock/ui_dock_templates.c
        src/ui/components/dock/ui_dock_view.c
        src/ui/core/ui_bottom_surface.c
        src/ui/core/ui_dock.c
        src/ui/core/ui_dock_controller.c
        src/ui/core/ui_event_bus.c
        src/ui/core/ui_motion.c
        src/ui/core/ui_sound.c
        src/ui/design/ui_component_style.c
        src/ui/fonts/ui_font_misans_ui_20.c
        src/ui/fonts/ui_font_misans_ui_24.c
        src/ui/fonts/ui_font_misans_en_18.c
        src/ui/fonts/ui_font_misans_status_16.c
    )
    target_include_directories(ui_dock_view_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}
    )
    target_compile_definitions(ui_dock_view_test PRIVATE LV_CONF_INCLUDE_SIMPLE)
    target_link_libraries(ui_dock_view_test lvgl)
    if(NOT MSVC)
        target_link_libraries(ui_dock_view_test m pthread)
    endif()
    add_test(NAME ui_dock_view_test COMMAND ui_dock_view_test)

    add_executable(ui_control_center_test
        tests/ui_control_center_test.c
        src/ui/overlays/ui_control_center.c
        src/ui/core/ui_background.c
        src/ui/core/ui_motion.c
        src/ui/fonts/ui_font_misans_ui_20.c
        src/ui/fonts/ui_font_misans_en_18.c
    )
    target_include_directories(ui_control_center_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}
    )
    target_compile_definitions(ui_control_center_test PRIVATE
        LV_CONF_INCLUDE_SIMPLE)
    target_link_libraries(ui_control_center_test lvgl)
    if(NOT MSVC)
        target_link_libraries(ui_control_center_test m pthread)
    endif()
    add_test(NAME ui_control_center_test COMMAND ui_control_center_test)

    add_executable(ui_system_control_shade_test
        tests/ui_system_control_shade_test.c
        src/ui/core/ui_system_control_shade.c
    )
    target_include_directories(ui_system_control_shade_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
    )
    add_test(NAME ui_system_control_shade_test
             COMMAND ui_system_control_shade_test)

    add_executable(ui_control_feedback_handoff_test
        tests/ui_control_feedback_handoff_test.c
        src/ui/core/ui_control_feedback_ownership.c
        src/ui/core/ui_dock.c
        src/ui/core/ui_dock_controller.c
        src/ui/core/ui_event_bus.c
    )
    target_include_directories(ui_control_feedback_handoff_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
    )
    add_test(NAME ui_control_feedback_handoff_test
             COMMAND ui_control_feedback_handoff_test)

    add_executable(ui_capsule_motion_logic_test
        tests/ui_capsule_motion_logic_test.c
        src/ui/core/ui_motion.c
    )
    target_include_directories(ui_capsule_motion_logic_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}
    )
    target_compile_definitions(ui_capsule_motion_logic_test PRIVATE
        LV_CONF_INCLUDE_SIMPLE)
    target_link_libraries(ui_capsule_motion_logic_test lvgl)
    if(NOT MSVC)
        target_link_libraries(ui_capsule_motion_logic_test m pthread)
    endif()
    add_test(NAME ui_capsule_motion_logic_test
             COMMAND ui_capsule_motion_logic_test)

    add_executable(ui_navigation_test
        tests/ui_navigation_test.c
        src/ui/pages/navigation/ui_navigation.c
        src/ui/components/navigation/ui_nav_animation.c
        src/ui/components/navigation/ui_nav_command_dock.c
        src/ui/components/navigation/ui_nav_control.c
        src/ui/components/navigation/ui_nav_data_strip.c
        src/ui/components/navigation/ui_nav_geometry.c
        src/ui/components/navigation/ui_nav_map.c
        src/ui/core/ui_concurrency.c
        src/ui/core/ui_event_bus.c
        src/ui/core/ui_motion.c
        src/services/ui_navigation_service.c
        src/ui/fonts/ui_font_misans_ui_20.c
        src/ui/fonts/ui_font_misans_ui_24.c
        src/ui/fonts/ui_font_misans_nav_metric_40.c
        src/ui/fonts/ui_font_misans_nav_action_32.c
        src/ui/fonts/ui_font_misans_nav_unit_16.c
        src/ui/fonts/ui_font_misans_en_18.c
        src/ui/fonts/ui_font_misans_metric_60.c
        src/ui/fonts/ui_font_misans_num_48.c
    )
    target_include_directories(ui_navigation_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}
    )
    target_compile_definitions(ui_navigation_test PRIVATE
        LV_CONF_INCLUDE_SIMPLE)
    target_link_libraries(ui_navigation_test lvgl)
    if(NOT MSVC)
        target_link_libraries(ui_navigation_test m pthread)
    endif()
    add_test(NAME ui_navigation_test COMMAND ui_navigation_test)

    add_executable(ui_router_lifecycle_test
        tests/ui_router_lifecycle_test.c
        src/ui/core/ui_router.c
        src/ui/core/ui_motion.c
    )
    target_include_directories(ui_router_lifecycle_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}
    )
    target_compile_definitions(ui_router_lifecycle_test PRIVATE LV_CONF_INCLUDE_SIMPLE)
    target_link_libraries(ui_router_lifecycle_test lvgl)
    if(NOT MSVC)
        target_link_libraries(ui_router_lifecycle_test m pthread)
    endif()
    add_test(NAME ui_router_lifecycle_test COMMAND ui_router_lifecycle_test)

    add_executable(ui_service_adapter_test
        tests/ui_service_adapter_test.c
        src/services/ui_state_adapter.c
        src/services/ui_navigation_service.c
        src/ui/core/ui_app_state.c
        src/ui/core/ui_input_queue.c
        src/ui/core/ui_concurrency.c
    )
    target_include_directories(ui_service_adapter_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
    )
    add_test(NAME ui_service_adapter_test COMMAND ui_service_adapter_test)

    add_executable(ui_command_service_test
        tests/ui_command_service_test.c
        src/services/ui_command_service.c
        src/ui/core/ui_concurrency.c
    )
    target_include_directories(ui_command_service_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
    )
    add_test(NAME ui_command_service_test COMMAND ui_command_service_test)

    add_executable(ui_bottom_surface_test
        tests/ui_bottom_surface_test.c
        src/ui/core/ui_bottom_surface.c
        src/ui/design/ui_component_style.c
    )
    target_include_directories(ui_bottom_surface_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}
    )
    target_compile_definitions(ui_bottom_surface_test PRIVATE
        LV_CONF_INCLUDE_SIMPLE)
    target_link_libraries(ui_bottom_surface_test lvgl)
    if(NOT MSVC)
        target_link_libraries(ui_bottom_surface_test m pthread)
    endif()
    add_test(NAME ui_bottom_surface_test COMMAND ui_bottom_surface_test)

    add_executable(ui_settings_list_test
        tests/ui_settings_list_test.c
        src/ui/components/settings/ui_settings_list.c
        src/ui/core/ui_motion.c
        src/ui/core/ui_sound.c
        src/ui/fonts/ui_font_misans_ui_20.c
        src/ui/fonts/ui_font_misans_en_18.c
    )
    target_include_directories(ui_settings_list_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}
    )
    target_compile_definitions(ui_settings_list_test PRIVATE
        LV_CONF_INCLUDE_SIMPLE)
    target_link_libraries(ui_settings_list_test lvgl)
    if(NOT MSVC)
        target_link_libraries(ui_settings_list_test m pthread)
    endif()
    add_test(NAME ui_settings_list_test COMMAND ui_settings_list_test)

    add_executable(ui_ride_summary_model_test
        tests/ui_ride_summary_model_test.c
        src/ui/models/ui_ride_summary_model.c
    )
    target_include_directories(ui_ride_summary_model_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
    )
    if(NOT MSVC)
        target_link_libraries(ui_ride_summary_model_test m)
    endif()
    add_test(NAME ui_ride_summary_model_test
             COMMAND ui_ride_summary_model_test)

    add_executable(ui_ride_history_model_test
        tests/ui_ride_history_model_test.c
        src/ui/models/ui_ride_history_model.c
    )
    target_include_directories(ui_ride_history_model_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
    )
    if(NOT MSVC)
        target_link_libraries(ui_ride_history_model_test m)
    endif()
    add_test(NAME ui_ride_history_model_test
             COMMAND ui_ride_history_model_test)

    add_executable(ui_ride_history_repository_test
        tests/ui_ride_history_repository_test.c
        src/services/ui_ride_history_repository.c
        src/ui/models/ui_ride_history_model.c
    )
    target_include_directories(ui_ride_history_repository_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
    )
    if(NOT MSVC)
        target_link_libraries(ui_ride_history_repository_test m)
    endif()
    add_test(NAME ui_ride_history_repository_test
             COMMAND ui_ride_history_repository_test)

    add_executable(ui_pc_ride_history_storage_test
        tests/ui_pc_ride_history_storage_test.c
        src/platform/pc/ui_pc_ride_history_storage.c
        src/services/ui_ride_history_repository.c
        src/ui/models/ui_ride_history_model.c
    )
    target_include_directories(ui_pc_ride_history_storage_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
    )
    if(NOT MSVC)
        target_link_libraries(ui_pc_ride_history_storage_test m)
    endif()
    add_test(NAME ui_pc_ride_history_storage_test
             COMMAND ui_pc_ride_history_storage_test)

    add_executable(ui_history_page_test
        tests/ui_history_page_test.c
        src/ui/pages/ui_history_page.c
        src/ui/pages/ui_ride_detail_page.c
        src/ui/pages/ui_page_common.c
        src/ui/components/ui_status_bar.c
        src/ui/core/ui_app_state.c
        src/ui/core/ui_background.c
        src/ui/core/ui_concurrency.c
        src/ui/core/ui_motion.c
        src/ui/core/ui_sound.c
        src/ui/models/ui_ride_history_model.c
        src/ui/fonts/ui_font_misans_ui_20.c
        src/ui/fonts/ui_font_misans_en_18.c
        src/ui/fonts/ui_font_misans_status_20.c
        src/ui/fonts/ui_font_misans_status_16.c
        src/ui/fonts/ui_font_misans_num_48.c
    )
    target_include_directories(ui_history_page_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}
    )
    target_compile_definitions(ui_history_page_test PRIVATE
        LV_CONF_INCLUDE_SIMPLE)
    target_link_libraries(ui_history_page_test lvgl)
    if(NOT MSVC)
        target_link_libraries(ui_history_page_test m pthread)
    endif()
    add_test(NAME ui_history_page_test COMMAND ui_history_page_test)

    add_executable(ui_summary_page_test
        tests/ui_summary_page_test.c
        src/ui/pages/ui_summary_page.c
        src/ui/pages/ui_page_common.c
        src/ui/components/ui_status_bar.c
        src/ui/core/ui_app_state.c
        src/ui/core/ui_background.c
        src/ui/core/ui_concurrency.c
        src/ui/core/ui_motion.c
        src/ui/core/ui_sound.c
        src/ui/fonts/ui_font_misans_ui_20.c
        src/ui/fonts/ui_font_misans_en_18.c
        src/ui/fonts/ui_font_misans_en_16.c
        src/ui/fonts/ui_font_misans_status_20.c
        src/ui/fonts/ui_font_misans_status_16.c
        src/ui/fonts/ui_font_misans_metric_60.c
        src/ui/fonts/ui_font_misans_num_48.c
    )
    target_include_directories(ui_summary_page_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}
    )
    target_compile_definitions(ui_summary_page_test PRIVATE
        LV_CONF_INCLUDE_SIMPLE)
    target_link_libraries(ui_summary_page_test lvgl)
    if(NOT MSVC)
        target_link_libraries(ui_summary_page_test m pthread)
    endif()
    add_test(NAME ui_summary_page_test COMMAND ui_summary_page_test)

    add_executable(ui_profile_achievements_test
        tests/ui_profile_achievements_test.c
        src/ui/pages/ui_profile_page.c
        src/ui/pages/ui_achievements_page.c
        src/ui/pages/ui_page_common.c
        src/ui/components/ui_status_bar.c
        src/ui/core/ui_app_state.c
        src/ui/core/ui_background.c
        src/ui/core/ui_concurrency.c
        src/ui/core/ui_motion.c
        src/ui/core/ui_sound.c
        src/ui/models/ui_home_data_model.c
        src/ui/models/ui_ride_history_model.c
        src/ui/fonts/ui_font_misans_ui_20.c
        src/ui/fonts/ui_font_misans_en_18.c
        src/ui/fonts/ui_font_misans_status_20.c
        src/ui/fonts/ui_font_misans_status_16.c
        src/ui/fonts/ui_font_misans_metric_small_46.c
        src/ui/fonts/ui_font_misans_num_48.c
    )
    target_include_directories(ui_profile_achievements_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}
    )
    target_compile_definitions(ui_profile_achievements_test PRIVATE
        LV_CONF_INCLUDE_SIMPLE)
    target_link_libraries(ui_profile_achievements_test lvgl)
    if(NOT MSVC)
        target_link_libraries(ui_profile_achievements_test m pthread)
    endif()
    add_test(NAME ui_profile_achievements_test
             COMMAND ui_profile_achievements_test)
endif()

# On Windows, GUI applications do not show a console by default,
# which hides log output. This ensures a console is available for logging.
if (WIN32)
    if (MSVC)
        target_link_options(main PRIVATE "/SUBSYSTEM:CONSOLE")
    else()
        target_link_options(main PRIVATE "-mconsole")
    endif()
endif()

# Apply additional compile options if the build type is Debug
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug mode enabled")

    target_compile_options(lvgl PRIVATE
        -pedantic-errors
        -Wall
        -Wclobbered
        -Wdeprecated
        -Wdouble-promotion
        -Wempty-body
        -Wextra
        -Wformat-security
        -Wmaybe-uninitialized
        # -Wmissing-prototypes
        -Wpointer-arith
        -Wmultichar
        -Wno-pedantic # ignored for now, we convert functions to pointers for properties table.
        -Wreturn-type
        -Wshadow
        -Wshift-negative-value
        -Wsizeof-pointer-memaccess
        -Wtype-limits
        -Wundef
        -Wuninitialized
        -Wunreachable-code
        -Wfloat-conversion
        -Wstrict-aliasing
    )

if (ASAN)
    message(STATUS "AddressSanitizer enabled")

    # Add AddressSanitizer flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
else()
    message(STATUS "AddressSanitizer disabled")
endif()
endif()
