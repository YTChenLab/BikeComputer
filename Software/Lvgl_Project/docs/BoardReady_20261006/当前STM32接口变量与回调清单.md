# 当前 STM32 接口变量与回调清单（Board-Ready 后）

最终源码路径：`D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project`。版本仍为 1.5.1；日期 2026-10-06。

这是一份新增的当前源码清单。原 `STM32接入说明_20261006/STM32接口变量与回调清单.md/.csv` 保持原样，代表重构前基线，旧行号不能用于定位当前源码。

单位、传感器来源与原理图映射沿用原接入指南；调用任务、启动顺序及变更说明见本目录的 `03_STM32待实现接口.md` 和 `05_原指南差异.md`。


## `ui_stm32_service_port_t`

源码：[src/platform/stm32/ui_stm32_services.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/platform/stm32/ui_stm32_services.h:25)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `bool (*request_gnss_fix)(void *context);` | 28 |
| `bool (*save_ride)(void *context);` | 29 |
| `bool (*retry_save)(void *context);` | 30 |
| `bool (*dispatch_command)(const ui_command_t *command, void *context);` | 31 |
| `bool (*request_history_page_worker)(void *context);` | 32 |
| `void (*tick)(uint32_t elapsed_ms, void *context);` | 33 |
| `void (*lock)(void *context);` | 34 |
| `void (*unlock)(void *context);` | 35 |
| `void *context;` | 36 |
| `void (*cancel_command)(uint32_t command_id, void *context);` | 37 |


## `ui_stm32_device_status_t`

源码：[src/platform/stm32/ui_stm32_services.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/platform/stm32/ui_stm32_services.h:96)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint8_t kind;` | 97 |
| `char status[64], feedback[96];` | 98 |
| `uint32_t color;` | 99 |


## `ui_ride_field_quality_t`

源码：[src/ui/models/ui_ride_data_model.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/models/ui_ride_data_model.h:27)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t sampled_at_ms;` | 28 |
| `uint32_t stale_after_ms;` | 29 |
| `ui_data_source_t source;` | 30 |


## `ui_ride_data_t`

源码：[src/ui/models/ui_ride_data_model.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/models/ui_ride_data_model.h:35)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `float speed;` | 37 |
| `uint16_t heart_rate;` | 38 |
| `uint16_t power;` | 39 |
| `uint16_t cadence;` | 40 |
| `float grade;` | 41 |
| `float distance;` | 42 |
| `float average_speed;` | 43 |
| `float maximum_speed;` | 44 |
| `int32_t altitude_m;` | 45 |
| `uint32_t ascent_m;` | 46 |
| `uint32_t calories_kcal;` | 47 |
| `int8_t temperature_c;` | 48 |
| `uint8_t heart_rate_zone;` | 49 |
| `uint8_t current_lap;` | 50 |
| `uint8_t total_laps;` | 51 |
| `uint32_t ride_seconds;` | 52 |
| `uint8_t clock_hour;` | 53 |
| `uint8_t clock_minute;` | 54 |
| `uint8_t satellites;` | 55 |
| `uint8_t battery;` | 56 |
| `uint32_t sample_time_ms;` | 60 |
| `uint64_t timer_ms;` | 61 |
| `int64_t timestamp_utc;` | 62 |
| `uint32_t session_id;` | 63 |
| `double latitude;` | 64 |
| `double longitude;` | 65 |
| `bool quality_known;` | 66 |
| `uint32_t capability_flags;` | 67 |
| `uint32_t valid_flags;` | 68 |
| `ui_ride_field_quality_t quality[UI_RIDE_FIELD_COUNT];` | 69 |


## `ui_state_adapter_snapshot_t`

源码：[src/services/ui_state_adapter.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_state_adapter.h:24)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint8_t clock_hour;` | 26 |
| `uint8_t clock_minute;` | 27 |
| `uint8_t battery_percent;` | 28 |
| `bool battery_charging;` | 29 |
| `bool status_quality_known, clock_valid, battery_valid;` | 30 |
| `uint8_t satellites;` | 31 |
| `bool gps_enabled;` | 32 |
| `bool wifi_enabled;` | 34 |
| `bool wifi_connected;` | 35 |
| `bool bluetooth_enabled;` | 36 |
| `bool bluetooth_connected;` | 37 |
| `char wifi_name[UI_APP_STATE_NAME_MAX];` | 38 |
| `char bluetooth_name[UI_APP_STATE_NAME_MAX];` | 39 |
| `uint8_t sound_level;` | 41 |
| `uint8_t brightness_level;` | 42 |
| `bool power_save_enabled;` | 43 |
| `bool touch_locked;` | 44 |
| `bool media_playing;` | 46 |
| `uint8_t media_track;` | 47 |
| `ui_media_mode_t media_mode;` | 48 |
| `bool navigation_active;` | 50 |


## `ui_weather_data_t`

源码：[src/ui/models/ui_weather_model.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/models/ui_weather_model.h:17)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `int8_t temperature_c;` | 19 |
| `int8_t feels_like_c;` | 20 |
| `uint8_t wind_kmh;` | 21 |
| `uint8_t rain_percent;` | 22 |
| `uint16_t air_quality_index;` | 23 |
| `ui_weather_condition_t condition;` | 24 |
| `uint32_t version;` | 25 |
| `bool quality_known, valid;` | 26 |
| `uint32_t sampled_at_ms, stale_after_ms;` | 27 |


## `ui_preferences_t`

源码：[src/services/ui_preferences.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_preferences.h:7)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `char user_name[32], tagline[48];` | 8 |
| `uint16_t heart_zone_bounds[4], power_zone_bounds[4];` | 9 |
| `uint32_t idle_sleep_ms;` | 10 |
| `uint8_t brightness, sound;` | 11 |
| `bool auto_pause, reduced_motion;` | 12 |
| `uint8_t dashboard_fields[UI_DASHBOARD_SLOT_COUNT], dashboard_preset;` | 13 |
| `uint32_t revision;` | 14 |


## `ui_preferences_port_t`

源码：[src/services/ui_preferences.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_preferences.h:16)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `bool (*load)(uint8_t blob[UI_PREFERENCES_BLOB_SIZE], void *context);` | 17 |
| `bool (*store)(const uint8_t blob[UI_PREFERENCES_BLOB_SIZE], void *context);` | 18 |
| `bool (*request_worker)(void *context);` | 19 |
| `void *context;` | 20 |


## `ui_device_info_t`

源码：[src/services/ui_device_info.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_device_info.h:5)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `char device_model[48], hardware_target[48], firmware_version[32], health[48];` | 6 |
| `uint64_t storage_free_bytes;` | 7 |
| `bool storage_ready;` | 8 |


## `ui_command_t`

源码：[src/services/ui_command_service.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_command_service.h:51)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t id;` | 53 |
| `ui_command_type_t type;` | 54 |
| `int32_t value;` | 55 |
| `uint32_t timeout_ms;` | 56 |


## `ui_command_result_t`

源码：[src/services/ui_command_service.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_command_service.h:59)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t id;` | 61 |
| `ui_command_type_t type;` | 62 |
| `ui_command_status_t status;` | 63 |
| `int32_t error_code;` | 64 |
| `int32_t value;` | 65 |


## `ui_command_backend_t`

源码：[src/services/ui_command_service.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_command_service.h:68)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `bool (*dispatch)(const ui_command_t *command, void *context);` | 70 |
| `void (*tick)(uint32_t elapsed_ms, void *context);` | 71 |
| `void *context;` | 72 |
| `void (*cancel)(uint32_t command_id, void *context);` | 74 |


## `ui_input_message_t`

源码：[src/ui/core/ui_input.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/core/ui_input.h:34)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `ui_input_event_t type;` | 36 |
| `int32_t x;` | 37 |
| `int32_t y;` | 38 |


## `ui_event_t`

源码：[src/ui/core/ui_event.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/core/ui_event.h:79)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `ui_event_type_t type;` | 81 |
| `ui_event_priority_t priority;` | 82 |
| `uint32_t source_id;` | 83 |
| `uint32_t dedupe_key;` | 84 |
| `uint32_t timeout_ms;` | 85 |
| `int32_t value;` | 86 |
| `int32_t minimum;` | 87 |
| `int32_t maximum;` | 88 |
| `bool flag;` | 89 |
| `char title[UI_EVENT_TITLE_MAX];` | 90 |
| `char message[UI_EVENT_MESSAGE_MAX];` | 91 |
| `char primary_action[UI_EVENT_ACTION_MAX];` | 92 |
| `char secondary_action[UI_EVENT_ACTION_MAX];` | 93 |


## `nav_model_t`

源码：[src/services/ui_navigation_service.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_navigation_service.h:54)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `float speed;` | 56 |
| `float distance;` | 57 |
| `uint32_t remaining_ascent_m;` | 58 |
| `uint8_t eta_hour;` | 59 |
| `uint8_t eta_min;` | 60 |
| `double latitude;` | 61 |
| `double longitude;` | 62 |
| `float heading;` | 63 |
| `uint8_t zoom;` | 64 |
| `nav_direction_t direction;` | 65 |
| `uint32_t direction_distance;` | 66 |
| `nav_ui_state_t ui_state;` | 67 |
| `char road_name[64];` | 68 |
| `bool demo;` | 69 |
| `bool position_valid;` | 70 |
| `uint32_t sampled_at_ms, stale_after_ms;` | 71 |
| `bool map_geometry_valid;` | 74 |
| `double map_origin_latitude, map_origin_longitude;` | 75 |
| `float meters_per_pixel;` | 76 |
| `uint8_t route_point_count;` | 77 |
| `double route_latitude[16], route_longitude[16];` | 78 |
| `bool session_bound, guidance_valid, speed_valid, distance_valid, eta_valid, heading_valid;` | 80 |
| `uint32_t session_id, route_id, route_revision, step_id;` | 81 |
| `uint8_t coordinate_system;` | 82 |
| `bool junction_valid, next_valid, exit_valid, off_route, reroute_failed;` | 83 |
| `bool exit_position_valid;` | 84 |
| `double exit_latitude,exit_longitude;` | 85 |
| `double junction_latitude, junction_longitude;` | 86 |
| `nav_maneuver_t next_direction;` | 87 |
| `uint32_t next_distance_m;` | 88 |
| `uint8_t roundabout_exit;` | 89 |


## `ui_nav_point_t`

源码：[src/services/ui_nav_session.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_session.h:9)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `double latitude, longitude;` | 9 |
| `ui_nav_coord_t system;` | 9 |


## `ui_nav_item_t`

源码：[src/services/ui_nav_session.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_session.h:31)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t id, revision;` | 32 |
| `char name[64], detail[96];` | 33 |
| `ui_nav_point_t point;` | 34 |
| `uint32_t distance_m, duration_s, size_kib;` | 35 |
| `bool distance_valid, duration_valid, start_allowed, demo, saved;` | 36 |
| `bool size_valid,progress_valid;` | 37 |
| `uint8_t progress_percent;` | 38 |
| `ui_nav_map_status_t map_status;` | 39 |
| `uint32_t ascent_m;` | 40 |
| `bool ascent_valid,coverage_known,coverage_complete;` | 41 |
| `char version_name[24],coverage[48];` | 42 |
| `uint8_t thumbnail_count;` | 43 |
| `ui_nav_point_t thumbnail[16];` | 44 |


## `ui_nav_catalog_t`

源码：[src/services/ui_nav_session.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_session.h:46)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `ui_nav_list_t kind;` | 47 |
| `uint32_t request_id;` | 48 |
| `uint32_t offset, total;` | 49 |
| `uint8_t count;` | 50 |
| `ui_nav_item_t items[UI_NAV_CATALOG_MAX];` | 51 |
| `bool storage_valid;` | 52 |
| `uint32_t storage_total_kib,storage_free_kib;` | 53 |


## `ui_nav_request_t`

源码：[src/services/ui_nav_session.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_session.h:55)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t id, session_id, route_id, route_revision, offset;` | 56 |
| `ui_nav_request_kind_t kind;` | 57 |
| `ui_nav_source_t source;` | 58 |
| `ui_nav_list_t list;` | 59 |
| `ui_nav_item_t destination;` | 60 |
| `uint32_t timeout_ms;` | 61 |


## `ui_nav_result_t`

源码：[src/services/ui_nav_session.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_session.h:63)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t request_id, session_id;` | 64 |
| `ui_nav_result_code_t code;` | 65 |
| `ui_nav_item_t route;` | 66 |
| `ui_nav_catalog_t catalog;` | 67 |


## `ui_nav_session_t`

源码：[src/services/ui_nav_session.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_session.h:69)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t version, id;` | 70 |
| `ui_nav_session_state_t state;` | 71 |
| `ui_nav_source_t source;` | 72 |
| `ui_nav_item_t destination, route;` | 73 |
| `bool position_valid, phone_connected;` | 74 |
| `ui_nav_point_t position;` | 75 |
| `uint32_t position_sampled_at_ms,position_stale_after_ms;` | 76 |
| `bool elapsed_valid,travelled_valid,ascent_valid;` | 77 |
| `uint32_t elapsed_s,travelled_m,ascent_m;` | 78 |


## `ui_nav_backend_t`

源码：[src/services/ui_nav_session.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_session.h:80)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `bool (*dispatch)(const ui_nav_request_t *request, void *context);` | 81 |
| `void (*cancel)(uint32_t request_id, void *context);` | 82 |
| `void (*tick)(uint32_t elapsed_ms, void *context);` | 83 |
| `void *context;` | 84 |


## `ui_nav_local_point_t`

源码：[src/services/ui_nav_map_provider.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_map_provider.h:10)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `float east_m,north_m;` | 10 |


## `ui_nav_area_t`

源码：[src/services/ui_nav_map_provider.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_map_provider.h:12)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint8_t count;` | 12 |
| `ui_nav_area_kind_t kind;` | 12 |
| `ui_nav_local_point_t points[UI_NAV_MAP_AREA_POINT_MAX];` | 12 |


## `ui_nav_map_label_t`

源码：[src/services/ui_nav_map_provider.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_map_provider.h:14)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `ui_nav_local_point_t point;` | 14 |
| `uint8_t kind;` | 14 |
| `char text[48];` | 14 |


## `ui_nav_road_t`

源码：[src/services/ui_nav_map_provider.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_map_provider.h:15)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `ui_nav_point_t a, b;` | 15 |
| `uint8_t level;` | 15 |


## `ui_nav_map_request_t`

源码：[src/services/ui_nav_map_provider.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_map_provider.h:17)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t id, route_id, route_revision;` | 18 |
| `ui_nav_point_t center;` | 19 |
| `float meters_per_pixel, heading;` | 20 |
| `uint16_t width, height;` | 22 |
| `ui_nav_map_purpose_t purpose;` | 23 |


## `ui_nav_map_frame_t`

源码：[src/services/ui_nav_map_provider.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_map_provider.h:25)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t request_id, route_id, route_revision, map_revision;` | 26 |
| `ui_nav_point_t origin;` | 27 |
| `float meters_per_pixel;` | 28 |
| `ui_nav_result_code_t status;` | 29 |
| `uint32_t raster_id;` | 30 |
| `float raster_latitude_ref;` | 31 |
| `bool demo;` | 32 |
| `bool destination_valid;` | 33 |
| `ui_nav_point_t destination;` | 34 |
| `uint8_t road_count, point_count;` | 35 |
| `ui_nav_road_t roads[UI_NAV_MAP_ROAD_MAX];` | 36 |
| `ui_nav_point_t points[UI_NAV_WINDOW_POINT_MAX];` | 37 |
| `uint8_t area_count,label_count;` | 38 |
| `ui_nav_area_t areas[UI_NAV_MAP_AREA_MAX];` | 39 |
| `ui_nav_map_label_t labels[UI_NAV_MAP_LABEL_MAX];` | 40 |


## `ui_nav_map_backend_t`

源码：[src/services/ui_nav_map_provider.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_map_provider.h:42)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `bool (*request)(const ui_nav_map_request_t *request, void *context);` | 43 |
| `void (*cancel)(uint32_t request_id, void *context);` | 44 |
| `void *context;` | 45 |


## `ui_nav_search_request_t`

源码：[src/services/ui_nav_search.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_search.h:4)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t id;` | 4 |
| `char query[UI_PLACE_QUERY_MAX];` | 4 |
| `ui_nav_point_t center;` | 4 |
| `bool center_valid;` | 4 |


## `ui_nav_search_backend_t`

源码：[src/services/ui_nav_search.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_search.h:5)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `bool (*request)(const ui_nav_search_request_t *,void *);` | 5 |
| `void (*cancel)(uint32_t,void *);` | 5 |
| `void *context;` | 5 |


## `ui_place_t`

源码：[src/services/ui_offline_places.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_places.h:6)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t id;` | 6 |
| `ui_nav_point_t point;` | 6 |
| `char name[64],detail[64];` | 6 |
| `uint8_t kind;` | 6 |
| `bool entrance;` | 6 |
| `ui_nav_point_t navigation_point;` | 7 |
| `bool navigation_point_valid,online;` | 7 |


## `ui_place_results_t`

源码：[src/services/ui_offline_places.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_places.h:8)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t request_id;` | 8 |
| `ui_nav_result_code_t code;` | 8 |
| `uint8_t count;` | 8 |
| `bool limited;` | 8 |
| `uint8_t online_status;` | 8 |
| `ui_place_t items[UI_PLACE_RESULT_MAX];` | 8 |


## `ui_offline_places_t`

源码：[src/services/ui_offline_places.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_places.h:9)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `ui_offline_map_io_t io;` | 9 |
| `uint32_t revision,records,keys,records_offset,keys_offset;` | 9 |


## `ui_place_stats_t`

源码：[src/services/ui_offline_places.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_places.h:10)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t bytes,scanned;` | 10 |
| `bool limited;` | 10 |


## `ui_offline_map_io_t`

源码：[src/services/ui_offline_map.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_map.h:6)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `bool (*read_at)(void *context,uint32_t offset,void *bytes,size_t count);` | 7 |
| `uint32_t size;` | 8 |
| `void *context;` | 8 |


## `ui_offline_map_t`

源码：[src/services/ui_offline_map.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_map.h:10)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `ui_offline_map_io_t io;` | 11 |
| `uint32_t revision,cell,cols,rows;` | 11 |
| `int32_t west,south,east,north;` | 12 |
| `char version_name[16],name[48];` | 13 |


## `ui_offline_map_stats_t`

源码：[src/services/ui_offline_map.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_map.h:15)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t tiles,bytes,roads_seen,areas_seen;` | 15 |
| `bool roads_limited,areas_limited;` | 15 |


## `ui_offline_graph_t`

源码：[src/services/ui_offline_graph.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_graph.h:4)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `ui_offline_map_io_t io;` | 5 |
| `uint8_t validation_workspace[4096];` | 5 |
| `uint32_t revision,nodes,edges,names,node_offset,edge_offset,name_offset;` | 5 |
| `int32_t west,south,east,north;` | 6 |


## `ui_offline_graph_stats_t`

源码：[src/services/ui_offline_graph.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_graph.h:8)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t expanded,bytes;` | 8 |
| `float start_snap_m,end_snap_m;` | 8 |
| `bool cancelled;` | 8 |


## `ui_offline_route_point_t`

源码：[src/services/ui_offline_route.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_route.h:6)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `ui_nav_point_t position;` | 6 |
| `float cumulative_m;` | 6 |
| `char road[48];` | 6 |


## `ui_offline_route_t`

源码：[src/services/ui_offline_route.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_route.h:7)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t id,revision,count;` | 7 |
| `float distance_m;` | 7 |
| `char name[48];` | 7 |
| `ui_offline_route_point_t points[UI_OFFLINE_ROUTE_POINT_MAX];` | 7 |


## `ui_offline_tile_level_t`

源码：[src/services/ui_offline_tiles.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_tiles.h:8)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `float mpp;` | 8 |
| `uint32_t cols,rows,first;` | 8 |


## `ui_offline_tiles_t`

源码：[src/services/ui_offline_tiles.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_tiles.h:9)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `ui_offline_map_io_t io;` | 10 |
| `uint8_t validation_workspace[1024];` | 11 |
| `uint32_t revision,directory,total,label_offset,label_bytes;` | 12 |
| `int32_t west,south,east,north;` | 13 |
| `float latitude_ref,meters_lon,meters_lat;` | 14 |
| `uint32_t level_count;` | 15 |
| `ui_offline_tile_level_t levels[UI_OFFLINE_TILE_LEVEL_MAX];` | 16 |


## `ui_offline_tile_label_t`

源码：[src/services/ui_offline_tiles.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_tiles.h:18)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `float x,y;` | 18 |
| `uint8_t kind;` | 18 |
| `char text[48];` | 18 |


## `ui_offline_tile_cache_entry_t`

源码：[src/services/ui_offline_tiles.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_tiles.h:19)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t key,age;` | 19 |
| `bool valid;` | 19 |
| `uint8_t label_count;` | 19 |
| `ui_offline_tile_label_t labels[16];` | 19 |


## `ui_offline_tile_cache_t`

源码：[src/services/ui_offline_tiles.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_tiles.h:20)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint8_t decode_workspace[4096];` | 21 |
| `uint16_t *pixels;` | 22 |
| `unsigned slots;` | 22 |
| `uint32_t clock;` | 22 |
| `ui_offline_tile_cache_entry_t entries[UI_OFFLINE_TILE_CACHE_MAX];` | 23 |


## `ui_offline_tiles_stats_t`

源码：[src/services/ui_offline_tiles.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_tiles.h:25)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t bytes,hits,misses,tiles;` | 25 |
| `float mpp;` | 25 |


## `ui_ride_history_checkpoint_t`

源码：[src/services/ui_ride_history_repository.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_ride_history_repository.h:30)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `ui_ride_history_totals_t totals;` | 31 |
| `uint64_t last_committed_id;` | 32 |
| `ui_ride_record_t pending;` | 33 |
| `bool pending_valid;` | 34 |
| `bool pending_is_new;` | 35 |


## `ui_ride_history_storage_t`

源码：[src/services/ui_ride_history_repository.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_ride_history_repository.h:46)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `ui_ride_history_load_cb_t load;` | 48 |
| `ui_ride_history_store_cb_t store;` | 49 |
| `ui_ride_history_now_cb_t now_utc;` | 50 |
| `void *context;` | 51 |
| `ui_ride_history_checkpoint_load_cb_t load_checkpoint;` | 55 |
| `ui_ride_history_checkpoint_store_cb_t store_checkpoint;` | 56 |
| `ui_ride_history_page_cb_t load_page;` | 57 |
| `ui_ride_history_result_t (*append_record)(const ui_ride_record_t *record, void *context);` | 60 |


## `ui_ride_summary_t`

源码：[src/ui/models/ui_ride_summary_model.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/models/ui_ride_summary_model.h:10)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `float distance_km;` | 12 |
| `uint32_t duration_seconds;` | 13 |
| `uint32_t moving_time_seconds;` | 14 |
| `uint8_t metric_flags;` | 15 |
| `float average_speed_kmh;` | 16 |
| `float maximum_speed_kmh;` | 17 |
| `uint32_t ascent_m;` | 18 |
| `uint32_t descent_m;` | 19 |
| `int32_t maximum_elevation_m;` | 20 |
| `uint32_t calories_kcal;` | 21 |
| `bool has_heart_rate;` | 22 |
| `uint16_t average_heart_rate;` | 23 |
| `uint16_t maximum_heart_rate;` | 24 |
| `uint16_t minimum_heart_rate;` | 25 |
| `bool has_power;` | 26 |
| `uint16_t average_power;` | 27 |
| `uint16_t maximum_power;` | 28 |
| `bool has_cadence;` | 29 |
| `uint16_t average_cadence;` | 30 |
| `uint16_t maximum_cadence;` | 31 |
| `bool has_temperature;` | 32 |
| `int16_t average_temperature_tenths_c;` | 33 |
| `ui_ride_report_trace_t trace;` | 34 |
| `bool saved;` | 35 |
| `uint32_t version;` | 36 |
| `uint32_t session_id;` | 37 |
| `int64_t started_at_utc;` | 38 |
| `int64_t finished_at_utc;` | 39 |


## `ui_ride_record_t`

源码：[src/ui/models/ui_ride_history_model.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/models/ui_ride_history_model.h:13)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint64_t id;` | 15 |
| `int64_t started_at_utc;` | 16 |
| `int64_t finished_at_utc;` | 17 |
| `float distance_km;` | 18 |
| `uint32_t duration_seconds;` | 19 |
| `uint32_t moving_time_seconds;` | 20 |
| `uint8_t metric_flags;` | 21 |
| `float average_speed_kmh;` | 22 |
| `float maximum_speed_kmh;` | 23 |
| `uint32_t ascent_m;` | 24 |
| `uint32_t descent_m;` | 25 |
| `int32_t maximum_elevation_m;` | 26 |
| `uint32_t calories_kcal;` | 27 |
| `bool has_heart_rate;` | 28 |
| `uint16_t average_heart_rate;` | 29 |
| `uint16_t maximum_heart_rate;` | 30 |
| `uint16_t minimum_heart_rate;` | 31 |
| `bool has_power;` | 32 |
| `uint16_t average_power;` | 33 |
| `uint16_t maximum_power;` | 34 |
| `bool has_cadence;` | 35 |
| `uint16_t average_cadence;` | 36 |
| `uint16_t maximum_cadence;` | 37 |
| `bool has_temperature;` | 38 |
| `int16_t average_temperature_tenths_c;` | 39 |
| `ui_ride_report_trace_t trace;` | 40 |


## `ui_ride_history_totals_t`

源码：[src/ui/models/ui_ride_history_model.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/models/ui_ride_history_model.h:43)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `double total_distance_km;` | 45 |
| `uint64_t total_duration_seconds;` | 46 |
| `uint64_t total_ascent_m;` | 47 |
| `uint32_t ride_count;` | 48 |


## `ui_ride_report_trace_t`

源码：[src/ui/models/ui_ride_report_data.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/models/ui_ride_report_data.h:21)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint16_t point_count;` | 23 |
| `bool time_is_estimated;` | 24 |
| `uint16_t speed_tenths[UI_RIDE_REPORT_POINT_MAX];` | 25 |
| `uint16_t heart_rate[UI_RIDE_REPORT_POINT_MAX];` | 26 |
| `uint16_t power[UI_RIDE_REPORT_POINT_MAX];` | 27 |
| `uint16_t cadence[UI_RIDE_REPORT_POINT_MAX];` | 28 |
| `int16_t elevation_m[UI_RIDE_REPORT_POINT_MAX];` | 29 |
| `uint32_t time_seconds[UI_RIDE_REPORT_POINT_MAX];` | 30 |
| `uint32_t valid_fields[UI_RIDE_REPORT_POINT_MAX];` | 31 |
| `int32_t latitude_microdegrees[UI_RIDE_REPORT_POINT_MAX];` | 32 |
| `int32_t longitude_microdegrees[UI_RIDE_REPORT_POINT_MAX];` | 33 |
| `uint32_t heart_rate_zone_seconds[UI_RIDE_REPORT_ZONE_COUNT];` | 34 |
| `uint32_t power_zone_seconds[UI_RIDE_REPORT_ZONE_COUNT];` | 35 |


## `ui_background_port_t`

源码：[src/ui/core/ui_background.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/core/ui_background.h:18)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `const void *(*load)(const char *resource_id, void *context);` | 19 |
| `void (*release)(const void *source, void *context);` | 20 |
| `void *context;` | 21 |


## `ui_device_connection_port_t`

源码：[src/ui/pages/ui_devices_page.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/pages/ui_devices_page.h:24)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `bool (*scan)(void *context);` | 25 |
| `bool (*connect)(ui_device_kind_t kind,void *context);` | 26 |
| `void *context;` | 27 |


## `ui_stm32_startup_t`

源码：[src/platform/stm32/ui_stm32_startup.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/platform/stm32/ui_stm32_startup.h:10)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `ui_runtime_port_t runtime;` | 11 |
| `bool (*resources_ready)(void *context);` | 12 |
| `void *resources_context;` | 13 |
| `ui_stm32_display_port_t display;` | 14 |
| `const ui_stm32_service_port_t *services;` | 15 |
| `const ui_ride_history_storage_t *history;` | 16 |
| `const ui_preferences_port_t *preferences;` | 17 |
| `const ui_nav_backend_t *navigation;` | 18 |
| `const ui_nav_map_backend_t *maps;` | 19 |
| `const ui_nav_search_backend_t *search;` | 20 |
| `uint32_t navigation_capabilities;` | 21 |
| `const ui_device_connection_port_t *devices;` | 22 |
| `const ui_background_port_t *background;` | 23 |
| `const lv_font_t *font20, *font24;` | 24 |
| `ui_sound_backend_cb_t sound;` | 25 |
| `void *sound_context;` | 26 |
| `uint16_t *raster_a, *raster_b;` | 27 |
| `size_t raster_pixel_count;` | 28 |


## `ui_stm32_diagnostics_t`

源码：[src/platform/stm32/ui_stm32_startup.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/platform/stm32/ui_stm32_startup.h:30)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `ui_runtime_diagnostics_t runtime;` | 31 |
| `uint32_t input_queue_drop, event_bridge_drop, event_forward_failure;` | 32 |
| `uint32_t command_drop, command_timeout, command_late_ack;` | 33 |


## `ui_stm32_display_port_t`

源码：[src/platform/stm32/ui_stm32_display.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/platform/stm32/ui_stm32_display.h:7)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint16_t width, height;` | 8 |
| `void *draw_buffer_a, *draw_buffer_b;` | 9 |
| `size_t draw_buffer_bytes;` | 10 |
| `ui_stm32_flush_status_t (*start_flush)(const lv_area_t *area, const uint8_t *pixels, uint32_t stride_bytes, void *context);` | 11 |
| `bool (*wait_flush_complete)(void *context);` | 16 |
| `void *context;` | 17 |


## `ui_runtime_port_t`

源码：[src/ui/core/ui_runtime.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/core/ui_runtime.h:10)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t (*now_ms)(void *context);` | 11 |
| `uint32_t (*now_us)(void *context);` | 12 |
| `bool (*is_ui_task)(void *context);` | 13 |
| `bool (*is_isr)(void *context);` | 14 |
| `void (*fault)(const char *reason, void *context);` | 15 |
| `void *context;` | 16 |


## `ui_runtime_diagnostics_t`

源码：[src/ui/core/ui_runtime.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/core/ui_runtime.h:18)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `uint32_t counters[UI_DIAG_COUNT];` | 19 |
| `uint32_t handler_calls, handler_last_us, handler_max_us;` | 20 |
| `bool microsecond_clock;` | 21 |


## `ui_nav_snapshot_t`

源码：[src/services/ui_nav_store.h](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_store.h:14)

| 实际成员或回调声明 | 源码行 |
| --- | --- |
| `ui_nav_session_t session;` | 15 |
| `nav_model_t guidance;` | 16 |
| `uint32_t revision,capabilities;` | 17 |
| `ui_nav_phase_t phase;` | 18 |
| `bool guidance_matches,busy;` | 19 |


## 任务入口与公开函数声明

以下只提取声明，不代表所有函数均可由 worker 调用；UiTask / worker 分工以接口文档为准。

| 头文件 | 公开声明 | 行号 |
| --- | --- | --- |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_register(const ui_stm32_service_port_t *port);` | 41 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_register_history_storage( const ui_ride_history_storage_t *storage);` | 46 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_publish_event(const ui_event_t *event);` | 50 |
| src/platform/stm32/ui_stm32_services.h | `void ui_stm32_services_submit_ride_data(const ui_ride_data_t *data);` | 51 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_submit_ride_patch(const ui_ride_data_t *patch, uint32_t fields);` | 52 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_submit_weather(const ui_weather_data_t *weather);` | 53 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_submit_preferences(const ui_preferences_t *preferences);` | 54 |
| src/platform/stm32/ui_stm32_services.h | `void ui_stm32_services_submit_navigation(const nav_model_t *model);` | 55 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_submit_state( const ui_state_adapter_snapshot_t *snapshot, ui_state_adapter_mask_t fields);` | 56 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_submit_input(const ui_input_message_t *message);` | 59 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_register_navigation(const ui_nav_backend_t *navigation, const ui_nav_map_backend_t *maps);` | 61 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_complete_navigation(const ui_nav_result_t *result);` | 63 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_submit_nav_session(const ui_nav_session_t *session);` | 64 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_submit_nav_catalog(const ui_nav_catalog_t *catalog, uint32_t revision);` | 65 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_submit_map_frame(const ui_nav_map_frame_t *frame);` | 66 |
| src/platform/stm32/ui_stm32_services.h | `void ui_stm32_services_sample_inputs(uint32_t pressed_mask, int32_t encoder_delta, uint32_t now_ms);` | 67 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_set_key_map(const ui_input_event_t short_events[UI_HW_KEY_COUNT], const ui_input_event_t long_events[UI_HW_KEY_COUNT]);` | 70 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_complete_command( uint32_t command_id, ui_command_status_t status, int32_t error_code, const ui_state_adapter_snapshot_t *state, ui_state_adapter_mask_t fields);` | 75 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_register_preferences(const ui_preferences_port_t *port);` | 84 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_register_search(const ui_nav_search_backend_t *backend);` | 85 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_complete_search(const ui_place_results_t *result);` | 86 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_submit_nav_capabilities(uint32_t capabilities);` | 87 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_submit_device_info(const ui_device_info_t *info);` | 88 |
| src/platform/stm32/ui_stm32_services.h | `ui_ride_history_result_t ui_stm32_services_commit_ride_worker(void);` | 89 |
| src/platform/stm32/ui_stm32_services.h | `ui_ride_history_result_t ui_stm32_services_history_page_worker(void);` | 90 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_preferences_worker(void);` | 91 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_submit_touch(int32_t x, int32_t y, bool pressed, bool cancelled);` | 94 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_get_touch(int32_t *x, int32_t *y, bool *pressed);` | 95 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_submit_device_status(const ui_stm32_device_status_t *status);` | 101 |
| src/platform/stm32/ui_stm32_services.h | `bool ui_stm32_services_take_device_status(ui_stm32_device_status_t *status);` | 102 |
| src/ui/models/ui_ride_data_model.h | `void ui_ride_data_model_init(void);` | 121 |
| src/ui/models/ui_ride_data_model.h | `int32_t ui_ride_data_model_subscribe(ui_ride_data_observer_cb_t callback, void *user_data);` | 122 |
| src/ui/models/ui_ride_data_model.h | `bool ui_ride_data_model_unsubscribe(int32_t subscription_id);` | 124 |
| src/ui/models/ui_ride_data_model.h | `void ui_ride_data_model_submit(const ui_ride_data_t *data);` | 127 |
| src/ui/models/ui_ride_data_model.h | `bool ui_ride_data_model_submit_patch(const ui_ride_data_t *patch, uint32_t fields);` | 131 |
| src/ui/models/ui_ride_data_model.h | `bool ui_ride_data_model_process(void);` | 134 |
| src/ui/models/ui_ride_data_model.h | `bool ui_ride_data_model_get(ui_ride_data_t *data, uint32_t *version);` | 135 |
| src/ui/models/ui_ride_data_model.h | `bool ui_ride_data_model_submit_checked(const ui_ride_data_t *data);` | 138 |
| src/services/ui_state_adapter.h | `void ui_state_adapter_init(void);` | 53 |
| src/services/ui_state_adapter.h | `bool ui_state_adapter_submit(const ui_state_adapter_snapshot_t *snapshot, ui_state_adapter_mask_t fields);` | 59 |
| src/services/ui_state_adapter.h | `ui_state_adapter_mask_t ui_state_adapter_process(void);` | 63 |
| src/services/ui_state_adapter.h | `ui_state_adapter_mask_t ui_state_adapter_pending(void);` | 64 |
| src/ui/models/ui_weather_model.h | `void ui_weather_model_init(void);` | 33 |
| src/ui/models/ui_weather_model.h | `bool ui_weather_model_get(ui_weather_data_t *weather);` | 34 |
| src/ui/models/ui_weather_model.h | `bool ui_weather_model_submit(const ui_weather_data_t *weather);` | 35 |
| src/ui/models/ui_weather_model.h | `bool ui_weather_model_process(uint32_t now_ms);` | 37 |
| src/ui/models/ui_weather_model.h | `int32_t ui_weather_model_subscribe(ui_weather_observer_cb_t callback, void *user_data);` | 38 |
| src/ui/models/ui_weather_model.h | `bool ui_weather_model_unsubscribe(int32_t subscription_id);` | 40 |
| src/services/ui_preferences.h | `bool ui_preferences_register(const ui_preferences_port_t *port);` | 22 |
| src/services/ui_preferences.h | `void ui_preferences_init(void);` | 24 |
| src/services/ui_preferences.h | `bool ui_preferences_get(ui_preferences_t *preferences);` | 25 |
| src/services/ui_preferences.h | `bool ui_preferences_submit(const ui_preferences_t *preferences);` | 27 |
| src/services/ui_preferences.h | `bool ui_preferences_process(uint32_t now_ms, ui_preferences_t *preferences);` | 29 |
| src/services/ui_preferences.h | `void ui_preferences_worker(void);` | 31 |
| src/services/ui_preferences.h | `bool ui_preferences_dirty(void);` | 32 |
| src/services/ui_preferences.h | `bool ui_preferences_last_save_ok(void);` | 33 |
| src/services/ui_preferences.h | `ui_preferences_storage_state_t ui_preferences_storage_state(void);` | 36 |
| src/services/ui_preferences.h | `bool ui_preferences_decode(const uint8_t blob[UI_PREFERENCES_BLOB_SIZE], ui_preferences_t *preferences);` | 37 |
| src/services/ui_device_info.h | `bool ui_device_info_submit(const ui_device_info_t *info);` | 11 |
| src/services/ui_device_info.h | `bool ui_device_info_process(ui_device_info_t *info);` | 12 |
| src/services/ui_command_service.h | `void ui_command_service_init(void);` | 81 |
| src/services/ui_command_service.h | `bool ui_command_service_register_backend(const ui_command_backend_t *backend);` | 82 |
| src/services/ui_command_service.h | `void ui_command_service_reset_backend(void);` | 83 |
| src/services/ui_command_service.h | `uint32_t ui_command_service_submit(ui_command_type_t type, int32_t value, uint32_t timeout_ms);` | 86 |
| src/services/ui_command_service.h | `bool ui_command_service_cancel_fix(uint32_t command_id);` | 89 |
| src/services/ui_command_service.h | `bool ui_command_service_complete(uint32_t command_id, ui_command_status_t status, int32_t error_code);` | 90 |
| src/services/ui_command_service.h | `bool ui_command_service_complete_with_state(uint32_t command_id, ui_command_status_t status, int32_t error_code, const ui_state_adapter_snapshot_t *state, ui_state_adapter_mask_t fields);` | 96 |
| src/services/ui_command_service.h | `void ui_command_service_process(uint32_t elapsed_ms);` | 101 |
| src/services/ui_command_service.h | `int32_t ui_command_service_subscribe(ui_command_observer_cb_t callback, void *user_data);` | 103 |
| src/services/ui_command_service.h | `bool ui_command_service_unsubscribe(int32_t subscription_id);` | 105 |
| src/services/ui_command_service.h | `uint8_t ui_command_service_pending(void);` | 107 |
| src/services/ui_command_service.h | `bool ui_command_service_active(ui_command_t *command);` | 108 |
| src/services/ui_command_service.h | `uint32_t ui_command_service_dropped(void);` | 109 |
| src/services/ui_command_service.h | `uint32_t ui_command_service_unexpected_results(void);` | 110 |
| src/services/ui_command_service.h | `uint32_t ui_command_service_timeouts(void);` | 111 |
| src/services/ui_navigation_service.h | `void ui_navigation_service_init(void);` | 96 |
| src/services/ui_navigation_service.h | `void ui_navigation_service_set_prompts_enabled(bool enabled);` | 98 |
| src/services/ui_navigation_service.h | `bool ui_navigation_service_prompts_enabled(void);` | 99 |
| src/services/ui_navigation_service.h | `void ui_navigation_service_set_demo_enabled(bool enabled);` | 100 |
| src/services/ui_navigation_service.h | `bool ui_navigation_service_demo_enabled(void);` | 101 |
| src/services/ui_navigation_service.h | `void ui_navigation_service_register_demo_step(void (*callback)(void));` | 102 |
| src/services/ui_navigation_service.h | `bool ui_navigation_service_demo_step(void);` | 103 |
| src/services/ui_navigation_service.h | `int32_t ui_navigation_service_subscribe(ui_navigation_observer_cb_t callback, void *user_data);` | 104 |
| src/services/ui_navigation_service.h | `bool ui_navigation_service_unsubscribe(int32_t subscription_id);` | 106 |
| src/services/ui_navigation_service.h | `void ui_navigation_service_submit(const nav_model_t *model);` | 109 |
| src/services/ui_navigation_service.h | `bool ui_navigation_service_process(void);` | 112 |
| src/services/ui_navigation_service.h | `bool ui_navigation_service_expire(uint32_t now_ms);` | 113 |
| src/services/ui_navigation_service.h | `bool ui_navigation_service_get(nav_model_t *model, uint32_t *version);` | 114 |
| src/services/ui_navigation_service.h | `bool ui_navigation_service_submit_checked(const nav_model_t *model);` | 117 |
| src/services/ui_nav_session.h | `void ui_nav_session_init(void);` | 88 |
| src/services/ui_nav_session.h | `bool ui_nav_session_register_backend(const ui_nav_backend_t *backend);` | 89 |
| src/services/ui_nav_session.h | `uint32_t ui_nav_session_request(const ui_nav_request_t *request, uint32_t now_ms);` | 91 |
| src/services/ui_nav_session.h | `void ui_nav_session_cancel(void);` | 92 |
| src/services/ui_nav_session.h | `bool ui_nav_session_discard_preview(void);` | 94 |
| src/services/ui_nav_session.h | `bool ui_nav_session_cancel_request(uint32_t request_id);` | 95 |
| src/services/ui_nav_session.h | `bool ui_nav_session_complete(const ui_nav_result_t *result);` | 97 |
| src/services/ui_nav_session.h | `bool ui_nav_session_submit(const ui_nav_session_t *session);` | 98 |
| src/services/ui_nav_session.h | `bool ui_nav_session_submit_catalog(const ui_nav_catalog_t *catalog,uint32_t revision);` | 100 |
| src/services/ui_nav_session.h | `void ui_nav_session_tick(uint32_t now_ms);` | 101 |
| src/services/ui_nav_session.h | `bool ui_nav_session_get(ui_nav_session_t *session);` | 102 |
| src/services/ui_nav_session.h | `bool ui_nav_session_get_catalog(ui_nav_catalog_t *catalog, uint32_t *version);` | 103 |
| src/services/ui_nav_session.h | `bool ui_nav_session_get_result(ui_nav_result_t *result, uint32_t *version);` | 104 |
| src/services/ui_nav_session.h | `bool ui_nav_session_busy(void);` | 105 |
| src/services/ui_nav_session.h | `bool ui_nav_session_active(void);` | 106 |
| src/services/ui_nav_session.h | `uint32_t ui_nav_session_now(void);` | 107 |
| src/services/ui_nav_session.h | `const char *ui_nav_result_text(ui_nav_result_code_t code);` | 108 |
| src/services/ui_nav_map_provider.h | `void ui_nav_map_provider_init(void);` | 47 |
| src/services/ui_nav_map_provider.h | `bool ui_nav_map_provider_register(const ui_nav_map_backend_t *backend);` | 48 |
| src/services/ui_nav_map_provider.h | `uint32_t ui_nav_map_provider_request(const ui_nav_map_request_t *request, uint32_t now_ms);` | 49 |
| src/services/ui_nav_map_provider.h | `void ui_nav_map_provider_cancel(void);` | 50 |
| src/services/ui_nav_map_provider.h | `bool ui_nav_map_provider_submit(const ui_nav_map_frame_t *frame);` | 52 |
| src/services/ui_nav_map_provider.h | `bool ui_nav_map_provider_process(uint32_t now_ms);` | 53 |
| src/services/ui_nav_map_provider.h | `bool ui_nav_map_provider_get(ui_nav_map_frame_t *frame, uint32_t *version);` | 54 |
| src/services/ui_nav_map_provider.h | `bool ui_nav_map_provider_busy(void);` | 55 |
| src/services/ui_nav_search.h | `bool ui_nav_search_register(const ui_nav_search_backend_t *backend);` | 6 |
| src/services/ui_nav_search.h | `uint32_t ui_nav_search_request(const char *query,ui_nav_point_t center,bool valid,uint32_t now);` | 7 |
| src/services/ui_nav_search.h | `void ui_nav_search_cancel(void);` | 8 |
| src/services/ui_nav_search.h | `bool ui_nav_search_submit(const ui_place_results_t *result);` | 9 |
| src/services/ui_nav_search.h | `void ui_nav_search_tick(uint32_t now);` | 11 |
| src/services/ui_nav_search.h | `void ui_nav_search_get(ui_place_results_t *result,uint32_t *version,bool *busy);` | 12 |
| src/services/ui_offline_places.h | `ui_nav_result_code_t ui_offline_places_open(ui_offline_places_t *places,const ui_offline_map_io_t *io);` | 12 |
| src/services/ui_offline_places.h | `ui_nav_result_code_t ui_offline_places_query(const ui_offline_places_t *places,const char *query,ui_nav_point_t center,bool center_valid,ui_place_results_t *results,bool (*cancelled)(void *),void *context,ui_place_stats_t *stats);` | 13 |
| src/services/ui_offline_map.h | `ui_nav_result_code_t ui_offline_map_open(ui_offline_map_t *map,const ui_offline_map_io_t *io);` | 16 |
| src/services/ui_offline_map.h | `ui_nav_result_code_t ui_offline_map_read(const ui_offline_map_t *map,const ui_nav_map_request_t *request,ui_nav_map_frame_t *frame,ui_offline_map_stats_t *stats);` | 19 |
| src/services/ui_offline_graph.h | `ui_nav_result_code_t ui_offline_graph_open(ui_offline_graph_t *graph,const ui_offline_map_io_t *io);` | 9 |
| src/services/ui_offline_graph.h | `size_t ui_offline_graph_workspace_size(const ui_offline_graph_t *graph);` | 10 |
| src/services/ui_offline_graph.h | `ui_nav_result_code_t ui_offline_graph_plan(const ui_offline_graph_t *graph, ui_nav_point_t start,ui_nav_point_t end,void *workspace,size_t bytes, ui_offline_route_t *route,bool (*cancelled)(void *context),void *context, ui_offline_graph_stats_t *stats);` | 14 |
| src/services/ui_offline_route.h | `ui_nav_result_code_t ui_offline_route_open(ui_offline_route_t *route,const ui_offline_map_io_t *io);` | 8 |
| src/services/ui_offline_route.h | `void ui_offline_route_replay(const ui_offline_route_t *route,float travelled_m,uint32_t session_id,uint32_t now_ms,nav_model_t *model);` | 11 |
| src/services/ui_offline_route.h | `void ui_offline_route_window(const ui_offline_route_t *route,ui_nav_map_frame_t *frame);` | 12 |
| src/services/ui_offline_tiles.h | `uint32_t ui_offline_crc32(uint32_t state,const void *data,size_t count);` | 26 |
| src/services/ui_offline_tiles.h | `ui_nav_result_code_t ui_offline_tiles_open(ui_offline_tiles_t *tiles,const ui_offline_map_io_t *io);` | 27 |
| src/services/ui_offline_tiles.h | `void ui_offline_tile_cache_init(ui_offline_tile_cache_t *cache,uint16_t *pixels,unsigned slots);` | 28 |
| src/services/ui_offline_tiles.h | `ui_nav_result_code_t ui_offline_tiles_window(const ui_offline_tiles_t *tiles, ui_offline_tile_cache_t *cache,const ui_nav_map_request_t *request, uint16_t *pixels,ui_nav_map_frame_t *frame,ui_offline_tiles_stats_t *stats);` | 31 |
| src/services/ui_ride_history_repository.h | `bool ui_ride_history_repository_register_page_worker(bool (*request)(void *), void *context);` | 66 |
| src/services/ui_ride_history_repository.h | `ui_ride_history_result_t ui_ride_history_repository_request_page(size_t newest_offset, ui_ride_record_t *records, size_t capacity, size_t *count);` | 68 |
| src/services/ui_ride_history_repository.h | `ui_ride_history_result_t ui_ride_history_repository_page_worker(void);` | 71 |
| src/services/ui_ride_history_repository.h | `bool ui_ride_history_repository_process_page(void);` | 72 |
| src/services/ui_ride_history_repository.h | `void ui_ride_history_repository_init(void);` | 73 |
| src/services/ui_ride_history_repository.h | `int64_t ui_ride_history_repository_now_utc(void);` | 75 |
| src/services/ui_ride_history_repository.h | `bool ui_ride_history_repository_register_storage( const ui_ride_history_storage_t *storage);` | 76 |
| src/services/ui_ride_history_repository.h | `ui_ride_history_result_t ui_ride_history_repository_load(void);` | 78 |
| src/services/ui_ride_history_repository.h | `ui_ride_history_result_t ui_ride_history_repository_prepare( const ui_ride_summary_t *summary);` | 79 |
| src/services/ui_ride_history_repository.h | `ui_ride_history_result_t ui_ride_history_repository_stage(const ui_ride_summary_t *summary);` | 82 |
| src/services/ui_ride_history_repository.h | `bool ui_ride_history_repository_begin_commit(void);` | 83 |
| src/services/ui_ride_history_repository.h | `bool ui_ride_history_repository_abort_commit(void);` | 84 |
| src/services/ui_ride_history_repository.h | `uint32_t ui_ride_history_repository_commits(void);` | 85 |
| src/services/ui_ride_history_repository.h | `ui_ride_history_result_t ui_ride_history_repository_commit_worker(void);` | 87 |
| src/services/ui_ride_history_repository.h | `bool ui_ride_history_repository_process(ui_ride_history_result_t *result);` | 89 |
| src/services/ui_ride_history_repository.h | `bool ui_ride_history_repository_busy(void);` | 90 |
| src/services/ui_ride_history_repository.h | `ui_ride_history_result_t ui_ride_history_repository_commit_pending(void);` | 92 |
| src/services/ui_ride_history_repository.h | `bool ui_ride_history_repository_has_pending(void);` | 93 |
| src/services/ui_ride_history_repository.h | `bool ui_ride_history_repository_get_pending(ui_ride_record_t *record);` | 94 |
| src/services/ui_ride_history_repository.h | `bool ui_ride_history_repository_recovery_supported(void);` | 95 |
| src/services/ui_ride_history_repository.h | `ui_ride_history_result_t ui_ride_history_repository_load_page( size_t newest_offset, ui_ride_record_t *records, size_t capacity, size_t *count);` | 96 |
| src/services/ui_ride_history_repository.h | `void ui_ride_history_repository_clear_pending(void);` | 99 |
| src/services/ui_ride_history_repository.h | `ui_ride_history_result_t ui_ride_history_repository_last_result(void);` | 100 |
| src/ui/models/ui_ride_summary_model.h | `void ui_ride_summary_model_init(void);` | 42 |
| src/ui/models/ui_ride_summary_model.h | `void ui_ride_summary_model_begin(const ui_ride_data_t *data);` | 43 |
| src/ui/models/ui_ride_summary_model.h | `void ui_ride_summary_model_update(const ui_ride_data_t *data);` | 44 |
| src/ui/models/ui_ride_summary_model.h | `void ui_ride_summary_model_finish(bool saved);` | 45 |
| src/ui/models/ui_ride_summary_model.h | `bool ui_ride_summary_model_restore(const ui_ride_summary_t *summary);` | 46 |
| src/ui/models/ui_ride_summary_model.h | `void ui_ride_summary_model_set_paused(bool paused);` | 47 |
| src/ui/models/ui_ride_summary_model.h | `void ui_ride_summary_model_get_zones(uint16_t heart_rate[4], uint16_t power[4]);` | 48 |
| src/ui/models/ui_ride_summary_model.h | `bool ui_ride_summary_model_set_zones(const uint16_t heart_rate[4], const uint16_t power[4]);` | 49 |
| src/ui/models/ui_ride_summary_model.h | `uint32_t ui_ride_summary_model_session_id(void);` | 51 |
| src/ui/models/ui_ride_summary_model.h | `uint32_t ui_ride_summary_model_reserve_session(void);` | 52 |
| src/ui/models/ui_ride_summary_model.h | `bool ui_ride_summary_model_get(ui_ride_summary_t *summary);` | 53 |
| src/ui/models/ui_ride_summary_model.h | `bool ui_ride_summary_model_active(void);` | 54 |
| src/ui/models/ui_ride_history_model.h | `bool ui_ride_record_is_valid(const ui_ride_record_t *record);` | 51 |
| src/ui/models/ui_ride_history_model.h | `void ui_ride_history_model_init(void);` | 52 |
| src/ui/models/ui_ride_history_model.h | `bool ui_ride_history_model_replace(const ui_ride_record_t *records, size_t count);` | 53 |
| src/ui/models/ui_ride_history_model.h | `bool ui_ride_history_model_append(const ui_ride_record_t *record);` | 55 |
| src/ui/models/ui_ride_history_model.h | `bool ui_ride_history_model_contains(uint64_t id);` | 56 |
| src/ui/models/ui_ride_history_model.h | `size_t ui_ride_history_model_count(void);` | 57 |
| src/ui/models/ui_ride_history_model.h | `bool ui_ride_history_model_get_newest(size_t index, ui_ride_record_t *record);` | 58 |
| src/ui/models/ui_ride_history_model.h | `bool ui_ride_history_model_copy_oldest_first(ui_ride_record_t *records, size_t capacity, size_t *count);` | 60 |
| src/ui/models/ui_ride_history_model.h | `void ui_ride_history_model_totals(ui_ride_history_totals_t *totals);` | 63 |
| src/ui/models/ui_ride_history_model.h | `bool ui_ride_history_model_set_totals(const ui_ride_history_totals_t *totals);` | 64 |
| src/ui/models/ui_ride_history_model.h | `uint32_t ui_ride_history_model_version(void);` | 65 |
| src/ui/core/ui_sound.h | `void ui_sound_init(void);` | 24 |
| src/ui/core/ui_sound.h | `void ui_sound_set_backend(ui_sound_backend_cb_t callback, void *user_data);` | 25 |
| src/ui/core/ui_sound.h | `void ui_sound_set_enabled(bool enabled);` | 26 |
| src/ui/core/ui_sound.h | `bool ui_sound_is_enabled(void);` | 27 |
| src/ui/core/ui_sound.h | `void ui_sound_set_volume(uint8_t volume);` | 28 |
| src/ui/core/ui_sound.h | `uint8_t ui_sound_get_volume(void);` | 29 |
| src/ui/core/ui_sound.h | `void ui_sound_play(ui_sound_event_t event);` | 30 |
| src/ui/core/ui_background.h | `bool ui_background_register_port(const ui_background_port_t *port);` | 25 |
| src/ui/core/ui_background.h | `void ui_background_init(void);` | 28 |
| src/ui/core/ui_background.h | `void ui_background_apply(lv_obj_t *screen);` | 31 |
| src/ui/core/ui_background.h | `void ui_background_set_opacity(lv_opa_t opacity);` | 34 |
| src/ui/core/ui_background.h | `void ui_background_set_visible(bool visible);` | 37 |
| src/ui/core/ui_background.h | `void ui_background_deinit(void);` | 40 |
| src/ui/pages/ui_devices_page.h | `void ui_devices_page_register_connection_port(const ui_device_connection_port_t *port);` | 29 |
| src/ui/pages/ui_devices_page.h | `bool ui_devices_page_request_scan(void);` | 30 |
| src/ui/pages/ui_devices_page.h | `bool ui_devices_page_request_selected(void);` | 31 |
| src/ui/pages/ui_devices_page.h | `lv_obj_t *ui_devices_page_create(ui_page_action_cb_t callback, void *user_data);` | 33 |
| src/ui/pages/ui_devices_page.h | `void ui_devices_page_set_active(bool active);` | 35 |
| src/ui/pages/ui_devices_page.h | `bool ui_devices_page_handle_input(ui_input_event_t input);` | 36 |
| src/ui/pages/ui_devices_page.h | `bool ui_devices_page_contains_scroll_point(int32_t x, int32_t y);` | 37 |
| src/ui/pages/ui_devices_page.h | `void ui_devices_page_set_status(ui_device_kind_t kind, const char *status, uint32_t color);` | 39 |
| src/ui/pages/ui_devices_page.h | `void ui_devices_page_set_feedback(const char *message);` | 41 |
| src/platform/stm32/ui_stm32_startup.h | `bool ui_stm32_startup(const ui_stm32_startup_t *startup);` | 39 |
| src/platform/stm32/ui_stm32_startup.h | `uint32_t ui_stm32_ui_task_handler(void);` | 41 |
| src/platform/stm32/ui_stm32_startup.h | `bool ui_stm32_services_get_diagnostics(ui_stm32_diagnostics_t *diagnostics);` | 42 |
| src/platform/stm32/ui_stm32_startup.h | `bool ui_stm32_datetime_set_offset_minutes(int minutes);` | 43 |
| src/platform/stm32/ui_stm32_display.h | `lv_display_t *ui_stm32_display_create(const ui_stm32_display_port_t *port);` | 19 |
| src/platform/stm32/ui_stm32_input.h | `lv_indev_t *ui_stm32_input_create_pointer(void);` | 5 |
| src/ui/core/ui_runtime.h | `bool ui_runtime_register(const ui_runtime_port_t *port);` | 25 |
| src/ui/core/ui_runtime.h | `bool ui_runtime_registered(void);` | 26 |
| src/ui/core/ui_runtime.h | `bool ui_runtime_is_isr(void);` | 27 |
| src/ui/core/ui_runtime.h | `bool ui_runtime_require_task(void);` | 28 |
| src/ui/core/ui_runtime.h | `bool ui_runtime_require_worker(void);` | 29 |
| src/ui/core/ui_runtime.h | `bool ui_runtime_require_ui(void);` | 30 |
| src/ui/core/ui_runtime.h | `uint32_t ui_runtime_now_ms(void);` | 31 |
| src/ui/core/ui_runtime.h | `uint32_t ui_runtime_now_us(void);` | 32 |
| src/ui/core/ui_runtime.h | `void ui_runtime_fault(const char *reason);` | 33 |
| src/ui/core/ui_runtime.h | `void ui_runtime_count(ui_diagnostic_counter_t counter);` | 34 |
| src/ui/core/ui_runtime.h | `void ui_runtime_record_handler(uint32_t duration_us);` | 35 |
| src/ui/core/ui_runtime.h | `void ui_runtime_get_diagnostics(ui_runtime_diagnostics_t *diagnostics);` | 36 |
| src/services/ui_nav_raster.h | `void ui_nav_raster_configure(uint16_t *first,uint16_t *second,size_t pixels);` | 8 |
| src/services/ui_nav_raster.h | `bool ui_nav_raster_begin(uint32_t request_id,unsigned *slot,uint16_t **pixels);` | 9 |
| src/services/ui_nav_raster.h | `bool ui_nav_raster_publish(unsigned slot,uint32_t request_id);` | 10 |
| src/services/ui_nav_raster.h | `void ui_nav_raster_abort(unsigned slot);` | 11 |
| src/services/ui_nav_raster.h | `const uint16_t *ui_nav_raster_take(uint32_t request_id);` | 12 |
| src/services/ui_nav_raster.h | `void ui_nav_raster_release(void);` | 13 |
| src/services/ui_nav_store.h | `void ui_nav_store_set_capabilities(uint32_t capabilities);` | 22 |
| src/services/ui_nav_store.h | `uint32_t ui_nav_store_capabilities(void);` | 23 |
| src/services/ui_nav_store.h | `bool ui_nav_store_get(ui_nav_snapshot_t *snapshot);` | 25 |
| src/services/ui_nav_store.h | `uint32_t ui_nav_store_command(const ui_nav_request_t *request,uint32_t now_ms);` | 26 |
