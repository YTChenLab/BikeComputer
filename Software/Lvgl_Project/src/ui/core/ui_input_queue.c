#include "ui_input_queue.h"

#include "ui_concurrency.h"

#include <string.h>

typedef struct
{
    ui_input_message_t messages[UI_INPUT_QUEUE_MAX];
    uint8_t count;
    uint32_t dropped;
} ui_input_queue_context_t;

static ui_input_queue_context_t s_input_queue;

static bool valid_message(const ui_input_message_t *message)
{
    return message != NULL && message->type < UI_INPUT_COUNT;
}

static bool release_class(ui_input_event_t type)
{
    return type == UI_INPUT_TOUCH_END || type == UI_INPUT_TOUCH_CANCEL ||
           type == UI_INPUT_BACK || type == UI_INPUT_BACK_DOUBLE ||
           type == UI_INPUT_BACK_LONG;
}

static bool remove_oldest_move(void)
{
    for(uint8_t index = 0; index < s_input_queue.count; index++) {
        if(s_input_queue.messages[index].type != UI_INPUT_TOUCH_MOVE) continue;
        for(uint8_t next = index + 1U; next < s_input_queue.count; next++)
            s_input_queue.messages[next - 1U] = s_input_queue.messages[next];
        s_input_queue.count--;
        return true;
    }
    return false;
}

static void remove_oldest(void)
{
    for(uint8_t index = 1U; index < s_input_queue.count; index++)
        s_input_queue.messages[index - 1U] = s_input_queue.messages[index];
    if(s_input_queue.count > 0U) s_input_queue.count--;
}

void ui_input_queue_init(void)
{
    ui_concurrency_lock();
    memset(&s_input_queue, 0, sizeof(s_input_queue));
    ui_concurrency_unlock();
}

bool ui_input_queue_post(const ui_input_message_t *message)
{
    if(!valid_message(message)) return false;
    ui_concurrency_lock();

    /* Pointer sampling can outpace the UI tick. Keep only the newest move. */
    if(message->type == UI_INPUT_TOUCH_MOVE && s_input_queue.count > 0U &&
       s_input_queue.messages[s_input_queue.count - 1U].type ==
           UI_INPUT_TOUCH_MOVE) {
        s_input_queue.messages[s_input_queue.count - 1U] = *message;
        ui_concurrency_unlock();
        return true;
    }

    if(s_input_queue.count >= UI_INPUT_QUEUE_MAX) {
        if(!release_class(message->type)) {
            s_input_queue.dropped++;
            ui_concurrency_unlock();
            return false;
        }
        if(!remove_oldest_move()) remove_oldest();
        s_input_queue.dropped++;
    }

    s_input_queue.messages[s_input_queue.count++] = *message;
    ui_concurrency_unlock();
    return true;
}

uint8_t ui_input_queue_drain(uint8_t max_messages,
                             ui_input_queue_callback_t callback,
                             void *user_data)
{
    if(max_messages == 0U || callback == NULL) return 0U;
    uint8_t processed = 0U;
    while(processed < max_messages) {
        ui_input_message_t message;
        ui_concurrency_lock();
        if(s_input_queue.count == 0U) {
            ui_concurrency_unlock();
            break;
        }
        message = s_input_queue.messages[0];
        for(uint8_t index = 1U; index < s_input_queue.count; index++)
            s_input_queue.messages[index - 1U] = s_input_queue.messages[index];
        s_input_queue.count--;
        ui_concurrency_unlock();
        callback(&message, user_data);
        processed++;
    }
    return processed;
}

uint8_t ui_input_queue_pending(void)
{
    ui_concurrency_lock();
    uint8_t count = s_input_queue.count;
    ui_concurrency_unlock();
    return count;
}

uint32_t ui_input_queue_dropped(void)
{
    ui_concurrency_lock();
    uint32_t dropped = s_input_queue.dropped;
    ui_concurrency_unlock();
    return dropped;
}
