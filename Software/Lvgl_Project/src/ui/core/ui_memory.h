#ifndef UI_MEMORY_H
#define UI_MEMORY_H
/* Boards can define UI_LARGE_RAM=__attribute__((section(".ui_sdram")))
 * and initialize that region before ui_app_init(). Empty on the simulator. */
#ifndef UI_LARGE_RAM
#define UI_LARGE_RAM
#endif
#endif
