# STM32 接入结构成员与回调清单

> 2026-10-09 输入契约变更：实体按键消抖、单双击、长按与编码器解码现由 STM32 InputService 负责；旧 sample_inputs/set_key_map/submit_input 已删除。本文输入部分及原行号属于历史快照，新接入须使用 [实体输入架构与队列契约](../输入架构重构_20261009.md)，触摸与其余端口不变。


最终 LVGL 源码目录：`D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project`。本清单覆盖板端数据和回调边界；页面内部控件、动画状态和 LVGL 库内部变量不需要由 STM32 逐个赋值。

日期：2026-10-06。由当前本地源码提取。每一行保留真实 C 声明；逗号连接的变量共享同一类型，数组大小和回调参数保持原样。实现职责、单位、有效性和调用流程见配套接入指南。这里不列页面内部控件指针及动画状态。

## `ui_stm32_service_port_t`

源码：[ src/platform/stm32/ui_stm32_services.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/platform/stm32/ui_stm32_services.h:20)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `bool (*request_gnss_fix)(void *context);` | 23 |
| `bool (*save_ride)(void *context);` | 24 |
| `bool (*retry_save)(void *context);` | 25 |
| `bool (*dispatch_command)(const ui_command_t *command, void *context);` | 26 |
| `bool (*request_history_page_worker)(void *context);` | 27 |
| `void (*tick)(uint32_t elapsed_ms, void *context);` | 28 |
| `void (*lock)(void *context);` | 29 |
| `void (*unlock)(void *context);` | 30 |
| `void *context;` | 31 |

## `ui_ride_field_quality_t`

源码：[ src/ui/models/ui_ride_data_model.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/models/ui_ride_data_model.h:27)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `uint32_t sampled_at_ms;` | 28 |
| `uint32_t stale_after_ms;` | 29 |
| `ui_data_source_t source;` | 30 |

## `ui_ride_data_t`

源码：[ src/ui/models/ui_ride_data_model.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/models/ui_ride_data_model.h:35)

| 实际成员/回调声明 | 源码行 |
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

源码：[ src/services/ui_state_adapter.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_state_adapter.h:24)

| 实际成员/回调声明 | 源码行 |
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

源码：[ src/ui/models/ui_weather_model.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/models/ui_weather_model.h:17)

| 实际成员/回调声明 | 源码行 |
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

源码：[ src/services/ui_preferences.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_preferences.h:7)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `char user_name[32], tagline[48];` | 8 |
| `uint16_t heart_zone_bounds[4], power_zone_bounds[4];` | 9 |
| `uint32_t idle_sleep_ms;` | 10 |
| `uint8_t brightness, sound;` | 11 |
| `bool auto_pause, reduced_motion;` | 12 |
| `uint8_t dashboard_fields[UI_DASHBOARD_SLOT_COUNT], dashboard_preset;` | 13 |
| `uint32_t revision;` | 14 |

## `ui_preferences_port_t`

源码：[ src/services/ui_preferences.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_preferences.h:16)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `bool (*load)(uint8_t blob[UI_PREFERENCES_BLOB_SIZE], void *context);` | 17 |
| `bool (*store)(const uint8_t blob[UI_PREFERENCES_BLOB_SIZE], void *context);` | 18 |
| `bool (*request_worker)(void *context);` | 19 |
| `void *context;` | 20 |

## `ui_device_info_t`

源码：[ src/services/ui_device_info.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_device_info.h:5)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `char device_model[48], hardware_target[48], firmware_version[32], health[48];` | 6 |
| `uint64_t storage_free_bytes;` | 7 |
| `bool storage_ready;` | 8 |

## `ui_command_t`

源码：[ src/services/ui_command_service.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_command_service.h:51)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `uint32_t id;` | 53 |
| `ui_command_type_t type;` | 54 |
| `int32_t value;` | 55 |
| `uint32_t timeout_ms;` | 56 |

## `ui_command_result_t`

源码：[ src/services/ui_command_service.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_command_service.h:59)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `uint32_t id;` | 61 |
| `ui_command_type_t type;` | 62 |
| `ui_command_status_t status;` | 63 |
| `int32_t error_code;` | 64 |
| `int32_t value;` | 65 |

## `ui_command_backend_t`

源码：[ src/services/ui_command_service.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_command_service.h:68)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `bool (*dispatch)(const ui_command_t *command, void *context);` | 70 |
| `void (*tick)(uint32_t elapsed_ms, void *context);` | 71 |
| `void *context;` | 72 |
| `void (*cancel)(uint32_t command_id, void *context);` | 74 |

## `ui_input_message_t`

源码：[ src/ui/core/ui_input.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/core/ui_input.h:34)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `ui_input_event_t type;` | 36 |
| `int32_t x;` | 37 |
| `int32_t y;` | 38 |

## `ui_event_t`

源码：[ src/ui/core/ui_event.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/core/ui_event.h:79)

| 实际成员/回调声明 | 源码行 |
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

源码：[ src/services/ui_navigation_service.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_navigation_service.h:54)

| 实际成员/回调声明 | 源码行 |
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

源码：[ src/services/ui_nav_session.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_session.h:9)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `double latitude, longitude;` | 9 |
| `ui_nav_coord_t system;` | 9 |

## `ui_nav_item_t`

源码：[ src/services/ui_nav_session.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_session.h:31)

| 实际成员/回调声明 | 源码行 |
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

源码：[ src/services/ui_nav_session.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_session.h:46)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `ui_nav_list_t kind;` | 47 |
| `uint32_t request_id;` | 48 |
| `uint32_t offset, total;` | 49 |
| `uint8_t count;` | 50 |
| `ui_nav_item_t items[UI_NAV_CATALOG_MAX];` | 51 |
| `bool storage_valid;` | 52 |
| `uint32_t storage_total_kib,storage_free_kib;` | 53 |

## `ui_nav_request_t`

源码：[ src/services/ui_nav_session.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_session.h:55)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `uint32_t id, session_id, route_id, route_revision, offset;` | 56 |
| `ui_nav_request_kind_t kind;` | 57 |
| `ui_nav_source_t source;` | 58 |
| `ui_nav_list_t list;` | 59 |
| `ui_nav_item_t destination;` | 60 |
| `uint32_t timeout_ms;` | 61 |

## `ui_nav_result_t`

源码：[ src/services/ui_nav_session.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_session.h:63)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `uint32_t request_id, session_id;` | 64 |
| `ui_nav_result_code_t code;` | 65 |
| `ui_nav_item_t route;` | 66 |
| `ui_nav_catalog_t catalog;` | 67 |

## `ui_nav_session_t`

源码：[ src/services/ui_nav_session.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_session.h:69)

| 实际成员/回调声明 | 源码行 |
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

源码：[ src/services/ui_nav_session.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_session.h:80)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `bool (*dispatch)(const ui_nav_request_t *request, void *context);` | 81 |
| `void (*cancel)(uint32_t request_id, void *context);` | 82 |
| `void (*tick)(uint32_t elapsed_ms, void *context);` | 83 |
| `void *context;` | 84 |

## `ui_nav_local_point_t`

源码：[ src/services/ui_nav_map_provider.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_map_provider.h:10)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `float east_m,north_m;` | 10 |

## `ui_nav_area_t`

源码：[ src/services/ui_nav_map_provider.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_map_provider.h:12)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `uint8_t count;` | 12 |
| `ui_nav_area_kind_t kind;` | 12 |
| `ui_nav_local_point_t points[UI_NAV_MAP_AREA_POINT_MAX];` | 12 |

## `ui_nav_map_label_t`

源码：[ src/services/ui_nav_map_provider.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_map_provider.h:14)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `ui_nav_local_point_t point;` | 14 |
| `uint8_t kind;` | 14 |
| `char text[48];` | 14 |

## `ui_nav_road_t`

源码：[ src/services/ui_nav_map_provider.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_map_provider.h:15)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `ui_nav_point_t a, b;` | 15 |
| `uint8_t level;` | 15 |

## `ui_nav_map_request_t`

源码：[ src/services/ui_nav_map_provider.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_map_provider.h:17)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `uint32_t id, route_id, route_revision;` | 18 |
| `ui_nav_point_t center;` | 19 |
| `float meters_per_pixel, heading;` | 20 |
| `uint16_t width, height;` | 22 |
| `ui_nav_map_purpose_t purpose;` | 23 |

## `ui_nav_map_frame_t`

源码：[ src/services/ui_nav_map_provider.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_map_provider.h:25)

| 实际成员/回调声明 | 源码行 |
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

源码：[ src/services/ui_nav_map_provider.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_map_provider.h:42)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `bool (*request)(const ui_nav_map_request_t *request, void *context);` | 43 |
| `void (*cancel)(uint32_t request_id, void *context);` | 44 |
| `void *context;` | 45 |

## `ui_nav_search_request_t`

源码：[ src/services/ui_nav_search.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_search.h:4)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `uint32_t id;` | 4 |
| `char query[UI_PLACE_QUERY_MAX];` | 4 |
| `ui_nav_point_t center;` | 4 |
| `bool center_valid;` | 4 |

## `ui_nav_search_backend_t`

源码：[ src/services/ui_nav_search.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_nav_search.h:5)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `bool (*request)(const ui_nav_search_request_t *,void *);` | 5 |
| `void (*cancel)(uint32_t,void *);` | 5 |
| `void *context;` | 5 |

## `ui_place_t`

源码：[ src/services/ui_offline_places.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_places.h:6)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `uint32_t id;` | 6 |
| `ui_nav_point_t point;` | 6 |
| `char name[64],detail[64];` | 6 |
| `uint8_t kind;` | 6 |
| `bool entrance;` | 6 |
| `ui_nav_point_t navigation_point;` | 7 |
| `bool navigation_point_valid,online;` | 7 |

## `ui_place_results_t`

源码：[ src/services/ui_offline_places.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_places.h:8)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `uint32_t request_id;` | 8 |
| `ui_nav_result_code_t code;` | 8 |
| `uint8_t count;` | 8 |
| `bool limited;` | 8 |
| `uint8_t online_status;` | 8 |
| `ui_place_t items[UI_PLACE_RESULT_MAX];` | 8 |

## `ui_offline_places_t`

源码：[ src/services/ui_offline_places.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_places.h:9)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `ui_offline_map_io_t io;` | 9 |
| `uint32_t revision,records,keys,records_offset,keys_offset;` | 9 |

## `ui_place_stats_t`

源码：[ src/services/ui_offline_places.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_places.h:10)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `uint32_t bytes,scanned;` | 10 |
| `bool limited;` | 10 |

## `ui_offline_map_io_t`

源码：[ src/services/ui_offline_map.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_map.h:6)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `bool (*read_at)(void *context,uint32_t offset,void *bytes,size_t count);` | 7 |
| `uint32_t size;` | 8 |
| `void *context;` | 8 |

## `ui_offline_map_t`

源码：[ src/services/ui_offline_map.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_map.h:10)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `ui_offline_map_io_t io;` | 11 |
| `uint32_t revision,cell,cols,rows;` | 11 |
| `int32_t west,south,east,north;` | 12 |
| `char version_name[16],name[48];` | 13 |

## `ui_offline_map_stats_t`

源码：[ src/services/ui_offline_map.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_map.h:15)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `uint32_t tiles,bytes,roads_seen,areas_seen;` | 15 |
| `bool roads_limited,areas_limited;` | 15 |

## `ui_offline_graph_t`

源码：[ src/services/ui_offline_graph.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_graph.h:4)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `ui_offline_map_io_t io;` | 5 |
| `uint32_t revision,nodes,edges,names,node_offset,edge_offset,name_offset;` | 5 |
| `int32_t west,south,east,north;` | 6 |

## `ui_offline_graph_stats_t`

源码：[ src/services/ui_offline_graph.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_graph.h:8)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `uint32_t expanded,bytes;` | 8 |
| `float start_snap_m,end_snap_m;` | 8 |
| `bool cancelled;` | 8 |

## `ui_offline_route_point_t`

源码：[ src/services/ui_offline_route.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_route.h:6)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `ui_nav_point_t position;` | 6 |
| `float cumulative_m;` | 6 |
| `char road[48];` | 6 |

## `ui_offline_route_t`

源码：[ src/services/ui_offline_route.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_route.h:7)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `uint32_t id,revision,count;` | 7 |
| `float distance_m;` | 7 |
| `char name[48];` | 7 |
| `ui_offline_route_point_t points[UI_OFFLINE_ROUTE_POINT_MAX];` | 7 |

## `ui_offline_tile_level_t`

源码：[ src/services/ui_offline_tiles.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_tiles.h:8)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `float mpp;` | 8 |
| `uint32_t cols,rows,first;` | 8 |

## `ui_offline_tiles_t`

源码：[ src/services/ui_offline_tiles.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_tiles.h:9)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `ui_offline_map_io_t io;` | 10 |
| `uint32_t revision,directory,total,label_offset,label_bytes;` | 11 |
| `int32_t west,south,east,north;` | 12 |
| `float latitude_ref,meters_lon,meters_lat;` | 13 |
| `uint32_t level_count;` | 14 |
| `ui_offline_tile_level_t levels[UI_OFFLINE_TILE_LEVEL_MAX];` | 15 |

## `ui_offline_tile_label_t`

源码：[ src/services/ui_offline_tiles.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_tiles.h:17)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `float x,y;` | 17 |
| `uint8_t kind;` | 17 |
| `char text[48];` | 17 |

## `ui_offline_tile_cache_entry_t`

源码：[ src/services/ui_offline_tiles.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_tiles.h:18)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `uint32_t key,age;` | 18 |
| `bool valid;` | 18 |
| `uint8_t label_count;` | 18 |
| `ui_offline_tile_label_t labels[16];` | 18 |

## `ui_offline_tile_cache_t`

源码：[ src/services/ui_offline_tiles.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_tiles.h:19)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `uint16_t *pixels;` | 20 |
| `unsigned slots;` | 20 |
| `uint32_t clock;` | 20 |
| `ui_offline_tile_cache_entry_t entries[UI_OFFLINE_TILE_CACHE_MAX];` | 21 |

## `ui_offline_tiles_stats_t`

源码：[ src/services/ui_offline_tiles.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_offline_tiles.h:23)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `uint32_t bytes,hits,misses,tiles;` | 23 |
| `float mpp;` | 23 |

## `ui_ride_history_checkpoint_t`

源码：[ src/services/ui_ride_history_repository.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_ride_history_repository.h:30)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `ui_ride_history_totals_t totals;` | 31 |
| `uint64_t last_committed_id;` | 32 |
| `ui_ride_record_t pending;` | 33 |
| `bool pending_valid;` | 34 |
| `bool pending_is_new;` | 35 |

## `ui_ride_history_storage_t`

源码：[ src/services/ui_ride_history_repository.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/services/ui_ride_history_repository.h:46)

| 实际成员/回调声明 | 源码行 |
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

源码：[ src/ui/models/ui_ride_summary_model.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/models/ui_ride_summary_model.h:10)

| 实际成员/回调声明 | 源码行 |
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

源码：[ src/ui/models/ui_ride_history_model.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/models/ui_ride_history_model.h:13)

| 实际成员/回调声明 | 源码行 |
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

源码：[ src/ui/models/ui_ride_history_model.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/models/ui_ride_history_model.h:43)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `double total_distance_km;` | 45 |
| `uint64_t total_duration_seconds;` | 46 |
| `uint64_t total_ascent_m;` | 47 |
| `uint32_t ride_count;` | 48 |

## `ui_ride_report_trace_t`

源码：[ src/ui/models/ui_ride_report_data.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/models/ui_ride_report_data.h:21)

| 实际成员/回调声明 | 源码行 |
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

源码：[ src/ui/core/ui_background.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/core/ui_background.h:18)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `const void *(*load)(const char *resource_id, void *context);` | 19 |
| `void (*release)(const void *source, void *context);` | 20 |
| `void *context;` | 21 |

## `ui_device_connection_port_t`

源码：[ src/ui/pages/ui_devices_page.h ](D:/Postgraduate_Program/111AAALearn_Lab/111AAABikeComputer/Software/Lvgl_Project/src/ui/pages/ui_devices_page.h:24)

| 实际成员/回调声明 | 源码行 |
| --- | --- |
| `bool (*scan)(void *context);` | 25 |
| `bool (*connect)(ui_device_kind_t kind,void *context);` | 26 |
| `void *context;` | 27 |
