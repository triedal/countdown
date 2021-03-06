#include <pebble.h>

#define KEY_TEMPERATURE 0

static Window *window;
static TextLayer *timeLayer;
//static TextLayer *batteryLayer;
static TextLayer *weatherLayer;

static Layer *time_ring_display_layer;

// Try 10 minute increments
// 68 = radius + fudge; 3 = 68*tan(2.5 degrees); 2.5 degrees per 10 minutes;

const GPathInfo TIME_RING_SEGMENT_PATH_POINTS = {
    3,
    (GPoint []) {
        {0, 0},
        {-18, -68}, // 68 = radius + fudge; 18 = 68*tan(15 degrees); 15 degrees per hour;
        {18,  -68},
    }
};

static GPath *time_ring_segment_path;

static void time_ring_display_layer_update_callback(Layer *layer, GContext *ctx) {
    // Get a tm structure
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    
    unsigned int max_angle = (tick_time->tm_hour) * 15;
    
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    
    graphics_context_set_fill_color(ctx, GColorClear);
    graphics_fill_circle(ctx, center, 65);
    
    graphics_context_set_fill_color(ctx, GColorBlack);

    for (; max_angle > 0; max_angle -= 15) {
        gpath_rotate_to(time_ring_segment_path, (TRIG_MAX_ANGLE / 360) * max_angle);
        gpath_draw_filled(ctx, time_ring_segment_path);
    }
    
    graphics_fill_circle(ctx, center, 60);
}
/*
static void handle_battery(BatteryChargeState charge_state) {
    static char battery_text[] = "100%";
    
    snprintf(battery_text, sizeof(battery_text), "%d%%", charge_state.charge_percent);
    
    text_layer_set_text(batteryLayer, battery_text);
}
*/

static void update_time() {
    // Get a tm structure
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    
    // Create a long-lived buffer
    static char timeText[] = "00:00";
        
    char *time_format;
    if (clock_is_24h_style()) {
        time_format = "%R";
    } else {
        time_format = "%I:%M";
    }
    strftime(timeText, sizeof(timeText), time_format, tick_time);
    
    // Handle lack of non-padded hour format string for twelve hour clock.
    if (!clock_is_24h_style() && (timeText[0] == '0')) {
        memmove(timeText, &timeText[1], sizeof(timeText) - 1);
    }
    text_layer_set_text(timeLayer, timeText);
}

static void handle_minute_tick (struct tm *tick_time, TimeUnits units_changed) {
    update_time();
    //handle_battery(battery_state_service_peek());
    layer_mark_dirty(time_ring_display_layer);
    
    // Get weather update every 30 minutes
    if(tick_time->tm_min % 30 == 0) {
        // Begin dictionary
        DictionaryIterator *iter;
        app_message_outbox_begin(&iter);
        
        // Add a key-value pair
        dict_write_uint8(iter, 0, 0);
        
        // Send the message!
        app_message_outbox_send();
    }
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
    // Store incoming information
    static char temperature_buffer[8];
    static char weather_layer_buffer[32];
    
    // Read first item
    Tuple *t = dict_read_first(iterator);
    
    // For all items
    while(t != NULL) {
        // Which key was received?
        switch(t->key) {
            case KEY_TEMPERATURE:
                snprintf(temperature_buffer, sizeof(temperature_buffer), "%d°", (int)t->value->int32);
                break;
            default:
                APP_LOG(APP_LOG_LEVEL_ERROR, "Key %d not recognized!", (int)t->key);
                break;
        }
        
        // Look for next item
        t = dict_read_next(iterator);
    }
    
    // Assemble full string and display
    snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s", temperature_buffer);
    text_layer_set_text(weatherLayer, weather_layer_buffer);
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_frame(window_layer);
    
    // Init layer for time display
    timeLayer = text_layer_create(GRect(0, 55, bounds.size.w, 50));
    text_layer_set_background_color(timeLayer, GColorClear);
    text_layer_set_text_color(timeLayer, GColorClear);
    text_layer_set_font(timeLayer, fonts_get_system_font(FONT_KEY_BITHAM_42_MEDIUM_NUMBERS));
    text_layer_set_text_alignment(timeLayer, GTextAlignmentCenter);
    
    /*
    // Init layer for battery level display
    batteryLayer = text_layer_create(GRect(0, 0, bounds.size.w, 24));
    text_layer_set_background_color(batteryLayer, GColorClear);
    text_layer_set_text_color(batteryLayer, GColorClear);
    text_layer_set_font(batteryLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_text_alignment(batteryLayer, GTextAlignmentRight);
    */
    
    // Init layer for weather display
    weatherLayer = text_layer_create(GRect(0, 100, 144, 30));
    text_layer_set_background_color(weatherLayer, GColorClear);
    text_layer_set_text_color(weatherLayer, GColorClear);
    text_layer_set_text_alignment(weatherLayer, GTextAlignmentCenter);
    text_layer_set_font(weatherLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text(weatherLayer, "");
    
    // Init the time left segment path
    time_ring_segment_path = gpath_create(&TIME_RING_SEGMENT_PATH_POINTS);
    gpath_move_to(time_ring_segment_path, grect_center_point(&bounds));
    
    // Init layer for time left display
    time_ring_display_layer = layer_create(bounds);
    layer_set_update_proc(time_ring_display_layer, time_ring_display_layer_update_callback);
    
    // Add child layers to window layer
    layer_add_child(window_layer, time_ring_display_layer);
    layer_add_child(window_layer, text_layer_get_layer(timeLayer));
    //layer_add_child(window_layer, text_layer_get_layer(batteryLayer));
    layer_add_child(window_layer, text_layer_get_layer(weatherLayer));
}

static void window_unload(Window *window) {
    text_layer_destroy(timeLayer);
    //text_layer_destroy(batteryLayer);
    text_layer_destroy(weatherLayer);
    layer_destroy(time_ring_display_layer);
    gpath_destroy(time_ring_segment_path);
}

static void init(void) {
    window = window_create();
    window_set_background_color(window, GColorBlack);
    window_set_window_handlers(window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
    });

    const bool animated = true;
    window_stack_push(window, animated);
    
    update_time();
    
    tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
    //battery_state_service_subscribe(handle_battery);
    
    // Register callbacks
    app_message_register_inbox_received(inbox_received_callback);
    app_message_register_inbox_dropped(inbox_dropped_callback);
    app_message_register_outbox_failed(outbox_failed_callback);
    app_message_register_outbox_sent(outbox_sent_callback);
    
    // Open AppMessage
    app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

static void deinit(void) {
    tick_timer_service_unsubscribe();
    //battery_state_service_unsubscribe();
    
    window_destroy(window);
}

int main(void) {
    init();

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

    app_event_loop();
    deinit();
}
