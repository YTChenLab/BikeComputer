#ifndef UI_INPUT_QUEUE_H
#define UI_INPUT_QUEUE_H

#include "ui_input.h"

#include <stdbool.h>
#include <stdint.h>

#define UI_INPUT_QUEUE_MAX 24

typedef void (*ui_input_queue_callback_t)(const ui_input_message_t *message,
                                          void *user_data);

void ui_input_queue_init(void);
bool ui_input_queue_post(const ui_input_message_t *message);
uint8_t ui_input_queue_drain(uint8_t max_messages,
                             ui_input_queue_callback_t callback,
                             void *user_data);
uint8_t ui_input_queue_pending(void);
uint32_t ui_input_queue_dropped(void);

#endif /* UI_INPUT_QUEUE_H */
