#ifndef UI_MEMORY_H
#define UI_MEMORY_H
#ifndef UI_LARGE_RAM
#if defined(UI_TARGET_STM32)
#if !defined(__clang__) && !defined(__GNUC__)
#error "STM32 UI requires Arm Compiler 6/C11 or a compatible compiler"
#endif
#define UI_LARGE_RAM __attribute__((section(".ui_sdram"), aligned(32)))
#else
#define UI_LARGE_RAM
#endif
#endif
#ifndef UI_DMA_RAM
#define UI_DMA_RAM UI_LARGE_RAM
#endif
/* .ui_sdram must be initialized after FMC is ready, before any UI API.
 * A NOINIT linker region requires explicit BSP zeroing; no physical address here. */
#endif
