#include <pebble.h>

static Window *window;
static TextLayer *timeLayer;
static TextLayer *batteryLayer;

static void handle_battery(BatteryChargeState charge_state) {
    static char battery_text[] = "100%";
    
    snprintf(battery_text, sizeof(battery_text), "%d%%", charge_state.charge_percent);
    
    text_layer_set_text(batteryLayer, battery_text);
}

static void update_time() {
    // Get a tm structure
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    
    // Create a long-lived buffer
    static char hourText[] = "00:00";
    
    // Write the current hours and minutes into the buffer
    if(clock_is_24h_style() == true) {
        // Use 24 hour format
        strftime(hourText, sizeof("00:00"), "%H:%M", tick_time);
    } else {
        // Use 12 hour format
        strftime(hourText, sizeof("0:00"), "%I:%M", tick_time);
    }
    
    // Display this time on the TextLayer
    text_layer_set_text(timeLayer, hourText);
}

static void handle_minute_tick (struct tm *tick_time, TimeUnits units_changed) {
    update_time();
    handle_battery(battery_state_service_peek());
}

static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_frame(window_layer);
    
    timeLayer = text_layer_create(GRect(0, 55, bounds.size.w, 50));
    text_layer_set_background_color(timeLayer, GColorClear);
    text_layer_set_text_color(timeLayer, GColorBlack);
    text_layer_set_font(timeLayer, fonts_get_system_font(FONT_KEY_BITHAM_42_MEDIUM_NUMBERS));
    text_layer_set_text_alignment(timeLayer, GTextAlignmentCenter);
    
    batteryLayer = text_layer_create(GRect(0, 0, bounds.size.w, 24));
    text_layer_set_background_color(batteryLayer, GColorClear);
    text_layer_set_text_color(batteryLayer, GColorBlack);
    text_layer_set_font(batteryLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_text_alignment(batteryLayer, GTextAlignmentRight);
    
    layer_add_child(window_layer, text_layer_get_layer(timeLayer));
    layer_add_child(window_layer, text_layer_get_layer(batteryLayer));
}

static void window_unload(Window *window) {
    text_layer_destroy(timeLayer);
    text_layer_destroy(batteryLayer);
}

static void init(void) {
    window = window_create();
    window_set_window_handlers(window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
    });

    const bool animated = true;
    window_stack_push(window, animated);
    
    update_time();
    
    tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
    battery_state_service_subscribe(handle_battery);
}

static void deinit(void) {
    tick_timer_service_unsubscribe();
    battery_state_service_unsubscribe();
    
    window_destroy(window);
}

int main(void) {
    init();

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

    app_event_loop();
    deinit();
}
