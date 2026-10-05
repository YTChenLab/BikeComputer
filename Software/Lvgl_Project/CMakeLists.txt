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
if(BUILD_TESTING)
    add_executable(ui_online_places_test tests/ui_online_places_test.c src/services/ui_online_places.c
        src/platform/pc/ui_pc_online_places.c)
    target_include_directories(ui_online_places_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
    if(WIN32)
        target_link_libraries(ui_online_places_test winhttp)
    endif()
    if(NOT MSVC)
        target_link_libraries(ui_online_places_test m)
    endif()
    add_test(NAME ui_online_places_test COMMAND ui_online_places_test)
    add_executable(ui_offline_places_test tests/ui_offline_places_test.c src/services/ui_offline_places.c
        src/services/ui_nav_search.c src/ui/core/ui_concurrency.c)
    target_include_directories(ui_offline_places_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
    if(NOT MSVC)
        target_link_libraries(ui_offline_places_test m)
    endif()
    add_test(NAME ui_offline_places_test COMMAND ui_offline_places_test)
    set_tests_properties(ui_offline_places_test PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    add_executable(ui_nav_raster_test tests/ui_nav_raster_test.c
        src/services/ui_nav_raster.c src/ui/core/ui_concurrency.c)
    target_include_directories(ui_nav_raster_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME ui_nav_raster_test COMMAND ui_nav_raster_test)
    add_executable(ui_offline_color_test tests/ui_offline_color_test.c
        src/services/ui_offline_tiles.c src/services/ui_offline_graph.c)
    target_include_directories(ui_offline_color_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
    if(NOT MSVC)
        target_link_libraries(ui_offline_color_test m)
    endif()
    add_test(NAME ui_offline_color_test COMMAND ui_offline_color_test)
    set_tests_properties(ui_offline_color_test PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    add_executable(ui_offline_map_test tests/ui_offline_map_test.c
        src/services/ui_offline_map.c src/services/ui_offline_route.c)
    target_include_directories(ui_offline_map_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
    if(NOT MSVC)
        target_link_libraries(ui_offline_map_test m)
    endif()
    add_test(NAME ui_offline_map_test COMMAND ui_offline_map_test)
    set_tests_properties(ui_offline_map_test PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
endif()

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
option(UI_PC_FAST_MAP_RENDERING "Optimize software drawing while retaining debug symbols and assertions" ON)
if(UI_PC_FAST_MAP_RENDERING AND NOT MSVC)
    target_compile_options(lvgl PRIVATE -O2)
    set_source_files_properties(src/services/ui_offline_tiles.c src/services/ui_offline_graph.c
        PROPERTIES COMPILE_OPTIONS "-O2")
endif()
target_include_directories(lvgl PUBLIC ${PROJECT_SOURCE_DIR} ${SDL2_INCLUDE_DIRS})

set(
    MAIN_SOURCES

    src/hal/hal.c
    src/hal/simulator_view.c
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
# Generated font sources can be removed by `cmake --build --clean-first` before
# CMake re-runs. Never let their presence at configure time decide whether the
# main target links them; exclude every generated source from the glob and add
# the complete fixed list explicitly below.
list(FILTER UI_SOURCES EXCLUDE REGEX "/ui_font_misans_ui_20\\.c$")
list(FILTER UI_SOURCES EXCLUDE REGEX "/ui_font_misans_ui_24\\.c$")
list(FILTER UI_SOURCES EXCLUDE REGEX "/ui_font_misans_metric_60\\.c$")
list(FILTER UI_SOURCES EXCLUDE REGEX "/ui_font_misans_zone_24\\.c$")
list(APPEND UI_SOURCES "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_zone_24.c")
list(FILTER UI_SOURCES EXCLUDE REGEX "/ui_font_misans_num_68\\.c$")
list(FILTER UI_SOURCES EXCLUDE REGEX "/ui_font_misans_nav_metric_40\\.c$")
list(FILTER UI_SOURCES EXCLUDE REGEX "/ui_font_misans_nav_letter_40\\.c$")
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
    "${PROJECT_SOURCE_DIR}/src/services/*.c"
    "${PROJECT_SOURCE_DIR}/src/services/*.h"
    "${PROJECT_SOURCE_DIR}/src/platform/*.c"
    "${PROJECT_SOURCE_DIR}/src/platform/*.h"
)
list(FILTER UI_RUNTIME_TEXT_SOURCES EXCLUDE REGEX "/src/ui/fonts/")

find_package(Python3 COMPONENTS Interpreter REQUIRED)
if(BUILD_TESTING)
    add_test(NAME map_places_test COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/map_places_test.py)
    add_test(NAME osm_pbf_test COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/osm_pbf_test.py)
endif()
set(UI_FONT_GENERATED_FILES
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_ui_20.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_ui_24.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_metric_60.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_zone_24.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_num_68.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_nav_metric_40.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_nav_letter_40.c"
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
        "${PROJECT_SOURCE_DIR}/tools/ui_extra_characters.txt"
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
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_ui_20.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_ui_24.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_metric_60.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_num_68.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_nav_metric_40.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_nav_letter_40.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_nav_action_32.c"
    "${PROJECT_SOURCE_DIR}/src/ui/fonts/ui_font_misans_nav_unit_16.c"
    ${SERVICE_SOURCES}
)
set(MAIN_LIBS lvgl lvgl::examples lvgl::demos lvgl::thorvg ${SDL2_LIBRARIES})
if(WIN32)
    list(APPEND MAIN_LIBS winhttp)
endif()

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
add_custom_target(run COMMAND ${EXECUTABLE_OUTPUT_PATH}/main
    DEPENDS main WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

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

# Keep the Windows simulator runnable without a toolchain PATH.
if(WIN32 AND TARGET SDL2::SDL2)
    get_target_property(UI_SDL2_LIBRARY_TYPE SDL2::SDL2 TYPE)
    if(UI_SDL2_LIBRARY_TYPE STREQUAL "SHARED_LIBRARY")
        add_custom_command(TARGET main POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:SDL2::SDL2>" "$<TARGET_FILE_DIR:main>"
            COMMENT "Copying SDL2 runtime beside the simulator"
            VERBATIM)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(simulator_view_test tests/simulator_view_test.c src/hal/simulator_view.c)
    target_include_directories(simulator_view_test PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR})
    target_compile_definitions(simulator_view_test PRIVATE LV_CONF_INCLUDE_SIMPLE)
    target_link_libraries(simulator_view_test lvgl ${SDL2_LIBRARIES})
    if(NOT MSVC)
        target_link_libraries(simulator_view_test m pthread)
    endif()
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/simulator_view_test_work")
    add_test(NAME simulator_view_test COMMAND simulator_view_test)
    set_tests_properties(simulator_view_test PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/simulator_view_test_work"
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")
    add_library(ui_navigation_adapter_example OBJECT docs/examples/ui_navigation_adapter_example.c)
    target_include_directories(ui_navigation_adapter_example PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR})
    target_compile_definitions(ui_navigation_adapter_example PRIVATE LV_CONF_INCLUDE_SIMPLE)
    target_link_libraries(ui_navigation_adapter_example lvgl)
    add_library(ui_board_adapter_example OBJECT docs/examples/ui_board_adapter_example.c)
    target_include_directories(ui_board_adapter_example PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR})
    target_compile_definitions(ui_board_adapter_example PRIVATE LV_CONF_INCLUDE_SIMPLE)
    target_link_libraries(ui_board_adapter_example lvgl)
    add_executable(ui_app_integration_test tests/ui_app_integration_test.c
        ${UI_SOURCES} ${SERVICE_SOURCES}
        src/platform/pc/ui_pc_navigation.c
        ${UI_FONT_GENERATED_FILES})
    target_include_directories(ui_app_integration_test PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR})
    target_compile_definitions(ui_app_integration_test PRIVATE LV_CONF_INCLUDE_SIMPLE)
    target_link_libraries(ui_app_integration_test lvgl)
    if(WIN32)
        target_link_libraries(ui_app_integration_test winhttp)
    endif()
    if(NOT MSVC)
        target_link_libraries(ui_app_integration_test m pthread)
    endif()
    add_test(NAME ui_app_integration_test COMMAND ui_app_integration_test)
    add_test(NAME ui_app_partial_render_test COMMAND ui_app_integration_test --partial)
    add_test(NAME ui_offline_navigation_integration COMMAND ui_app_integration_test --offline)
    add_test(NAME ui_offline_navigation_partial COMMAND ui_app_integration_test --offline-partial)
    set_tests_properties(ui_offline_navigation_integration ui_offline_navigation_partial
        PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    set_tests_properties(ui_app_partial_render_test PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    set_tests_properties(ui_app_integration_test PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    add_executable(ui_hardware_input_test tests/ui_hardware_input_test.c src/ui/core/ui_hardware_input.c)
    target_include_directories(ui_hardware_input_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME ui_hardware_input_test COMMAND ui_hardware_input_test)
    add_executable(ui_data_contract_test tests/ui_data_contract_test.c
        src/ui/models/ui_ride_data_model.c src/ui/models/ui_ride_summary_model.c
        src/ui/core/ui_concurrency.c)
    target_include_directories(ui_data_contract_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
    if(NOT MSVC)
        target_link_libraries(ui_data_contract_test m)
    endif()
    add_test(NAME ui_data_contract_test COMMAND ui_data_contract_test)
    add_executable(ui_font_coverage_test
        tests/ui_font_coverage_test.c
        src/ui/fonts/ui_font_misans_ui_20.c
        src/ui/fonts/ui_font_misans_ui_24.c
        src/ui/fonts/ui_font_misans_zone_24.c
        src/ui/fonts/ui_font_misans_nav_metric_40.c
        src/ui/fonts/ui_font_misans_nav_letter_40.c
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
        src/ui/core/ui_concurrency.c
        tests/ui_weather_model_test.c
        src/ui/models/ui_weather_model.c
    )
    target_include_directories(ui_weather_model_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME ui_weather_model_test COMMAND ui_weather_model_test)

    add_executable(ui_ride_report_model_test
        tests/ui_ride_report_model_test.c
        src/ui/models/ui_ride_report_model.c
    )
    target_include_directories(ui_ride_report_model_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src)
    if(NOT MSVC)
        target_link_libraries(ui_ride_report_model_test m)
    endif()
    add_test(NAME ui_ride_report_model_test COMMAND ui_ride_report_model_test)

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
        src/services/ui_preferences.c src/ui/models/ui_dashboard_layout.c
        src/platform/pc/ui_pc_preferences.c
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
        src/platform/pc/ui_pc_navigation.c
        src/services/ui_nav_session.c
        src/services/ui_nav_store.c
        src/services/ui_nav_map_provider.c
        src/platform/pc/ui_pc_ride_history_storage.c
        src/ui/models/ui_ride_data_model.c
        src/ui/models/ui_ride_history_model.c
        src/ui/models/ui_ride_summary_model.c
        src/ui/models/ui_weather_model.c
        src/platform/stm32/ui_stm32_services.c
        src/ui/core/ui_hardware_input.c
    )
    target_include_directories(ui_core_logic_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME ui_core_logic_test COMMAND ui_core_logic_test)
    add_executable(ui_preferences_test tests/ui_preferences_test.c
        src/services/ui_preferences.c src/ui/models/ui_dashboard_layout.c src/services/ui_device_info.c src/ui/core/ui_concurrency.c)
    target_include_directories(ui_preferences_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME ui_preferences_test COMMAND ui_preferences_test)

    add_executable(ui_dashboard_layout_test tests/ui_dashboard_layout_test.c
        src/ui/models/ui_dashboard_layout.c)
    target_include_directories(ui_dashboard_layout_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
    if(UNIX)
        target_link_libraries(ui_dashboard_layout_test m)
    endif()
    add_test(NAME ui_dashboard_layout_test COMMAND ui_dashboard_layout_test)

    add_executable(ui_dock_view_test
        tests/ui_dock_view_test.c
        src/ui/components/dock/ui_dock_templates.c
        src/ui/components/ui_liquid_selection.c
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

    add_executable(ui_nav_session_test tests/ui_nav_session_test.c
        src/services/ui_nav_session.c src/services/ui_nav_map_provider.c src/ui/core/ui_concurrency.c)
    target_include_directories(ui_nav_session_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
    if(NOT MSVC)
        target_link_libraries(ui_nav_session_test m pthread)
    endif()
    add_test(NAME ui_nav_session_test COMMAND ui_nav_session_test)
    add_executable(ui_nav_store_test tests/ui_nav_store_test.c
        src/services/ui_nav_store.c src/services/ui_nav_session.c
        src/services/ui_navigation_service.c src/ui/core/ui_concurrency.c)
    target_include_directories(ui_nav_store_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
    if(NOT MSVC)
        target_link_libraries(ui_nav_store_test m pthread)
    endif()
    add_test(NAME ui_nav_store_test COMMAND ui_nav_store_test)
    add_executable(ui_navigation_test
        tests/ui_navigation_test.c
        src/services/ui_nav_search.c
        src/ui/components/navigation/ui_nav_search_panel.c
        src/ui/pages/navigation/ui_navigation.c
        src/ui/pages/navigation/ui_nav_workspace.c
        src/services/ui_nav_store.c
        src/ui/core/ui_bottom_surface.c
        src/ui/design/ui_component_style.c
        src/services/ui_nav_session.c
        src/services/ui_nav_map_provider.c
        src/services/ui_nav_raster.c
        src/ui/core/ui_text.c
        src/ui/components/navigation/ui_nav_animation.c
        src/ui/components/navigation/ui_nav_visual.c
        src/ui/components/navigation/ui_nav_capsule_row.c
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
        src/ui/fonts/ui_font_misans_nav_letter_40.c
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
        src/services/ui_state_adapter.c
        src/ui/core/ui_app_state.c
        tests/ui_command_service_test.c
        src/services/ui_command_service.c
        src/ui/core/ui_concurrency.c
    )
    target_include_directories(ui_command_service_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
    )
    add_test(NAME ui_command_service_test COMMAND ui_command_service_test)

    add_executable(ui_bottom_surface_test
        src/ui/core/ui_motion.c
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
        src/ui/components/ui_liquid_selection.c
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
        src/ui/core/ui_concurrency.c
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
        src/services/ui_ride_history_repository.c
        tests/ui_history_page_test.c
        src/ui/pages/ui_history_page.c
        src/ui/pages/ui_ride_detail_page.c
        src/ui/pages/ui_ride_report_page.c
        src/ui/pages/ui_page_common.c
        src/ui/components/ui_status_bar.c
        src/ui/core/ui_app_state.c
        src/ui/core/ui_background.c
        src/ui/core/ui_concurrency.c
        src/ui/core/ui_motion.c
        src/ui/core/ui_sound.c
        src/ui/models/ui_ride_history_model.c
        src/ui/models/ui_ride_report_model.c
        src/ui/fonts/ui_font_misans_ui_20.c
        src/ui/fonts/ui_font_misans_en_18.c
        src/ui/fonts/ui_font_misans_status_20.c
        src/ui/fonts/ui_font_misans_status_16.c
        src/ui/fonts/ui_font_misans_metric_60.c
        src/ui/fonts/ui_font_misans_num_48.c
    )
    target_include_directories(ui_history_page_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}
    )
    target_compile_definitions(ui_history_page_test PRIVATE
        LV_CONF_INCLUDE_SIMPLE)
    target_link_libraries(ui_history_page_test lvgl)
    target_sources(ui_history_page_test PRIVATE src/ui/fonts/ui_font_misans_ui_24.c)
    if(NOT MSVC)
        target_link_libraries(ui_history_page_test m pthread)
    endif()
    add_test(NAME ui_history_page_test COMMAND ui_history_page_test)

    add_executable(ui_summary_page_test
        tests/ui_summary_page_test.c
        src/ui/pages/ui_summary_page.c
        src/ui/pages/ui_ride_report_page.c
        src/ui/pages/ui_page_common.c
        src/ui/components/ui_status_bar.c
        src/ui/core/ui_app_state.c
        src/ui/core/ui_background.c
        src/ui/core/ui_concurrency.c
        src/ui/core/ui_motion.c
        src/ui/core/ui_sound.c
        src/ui/models/ui_ride_report_model.c
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
    target_sources(ui_summary_page_test PRIVATE src/ui/fonts/ui_font_misans_ui_24.c)
    if(NOT MSVC)
        target_link_libraries(ui_summary_page_test m pthread)
    endif()
    add_test(NAME ui_summary_page_test COMMAND ui_summary_page_test)

    add_executable(ui_profile_achievements_test
        tests/ui_profile_achievements_test.c
        src/ui/pages/ui_profile_page.c
        src/ui/components/ui_liquid_selection.c
        src/ui/models/ui_dashboard_layout.c
        src/ui/pages/ui_achievements_page.c
        src/ui/models/ui_achievement_model.c
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
        src/ui/fonts/ui_font_misans_metric_60.c
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

if(BUILD_TESTING)
    foreach(ui_test ui_control_center_test ui_navigation_test ui_profile_achievements_test ui_settings_list_test)
        target_sources(${ui_test} PRIVATE src/ui/core/ui_text.c
            src/ui/fonts/ui_font_misans_ui_24.c)
    endforeach()
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
