/*
 * This registers and AccelerometerService handler and increments a counter each time
 * the handler is called.  Pressing the down button displays the current count.
 * After a fairly short time (typically < 3 minutes) it stops updating.
 */

#include <pebble.h>

#define TICK_TIME 2000

static Window *window;
static TextLayer *text_layer;
static uint16_t acc_count = 0;
static uint64_t start_time = 0;
static uint64_t last_time = 0;
static uint64_t min_time = UINT64_MAX;
static uint64_t max_time = 0;
static uint64_t avg_accum = 0;
static unsigned int avg_count = 0;
static uint64_t last_elapsed = UINT64_MAX;


static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Select");
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {

	static char percentText[11];
	BatteryChargeState state = battery_state_service_peek();
	snprintf(percentText,sizeof(percentText), "Bat: %hd%%",state.charge_percent);

	text_layer_set_text(text_layer, percentText);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
	static char msg[30];
	snprintf(msg, sizeof(msg), "Acc count %u", acc_count);
	text_layer_set_text(text_layer, msg);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  text_layer = text_layer_create((GRect) { .origin = { 0, 40 }, .size = { bounds.size.w, bounds.size.h-40 } });
  text_layer_set_text(text_layer, "Running");
  text_layer_set_text_alignment(text_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(text_layer);
}

static void accelHandle(AccelData *data, uint32_t num_samples) {
	acc_count++;
	if (start_time == 0) {
		if (num_samples > 0) {
			start_time = data[0].timestamp;
		}
	}
	if (num_samples > 0){
		last_time = data[num_samples-1].timestamp;
	}
//	APP_LOG(APP_LOG_LEVEL_DEBUG, "acc triggered");
}

static void acc_start()
{
	accel_data_service_subscribe(25, accelHandle);
	accel_service_set_sampling_rate(ACCEL_SAMPLING_100HZ);
}

static void acc_stop()
{
	accel_data_service_unsubscribe();
}


static void timer_handler(void* data);

static void restart_timer() {
	app_timer_register(TICK_TIME, timer_handler, NULL);
}

static unsigned getHour(unsigned sec) {
	return sec/(60*60);
}

static unsigned getMin(unsigned sec) {
	return (sec/60) % 60;
}

static unsigned getSec(unsigned sec) {
	return sec % 60;
}

static unsigned getAvg() {
	if (avg_count == 0) return 0;
	return avg_accum / avg_count;
}

static void timer_handler(void* data) {
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "Timer handler triggered");
	static char msg[80];
	uint64_t elapsed = (last_time - start_time) / 1000;

	if (last_elapsed == elapsed) { // Acc stuck
		if (elapsed > max_time) {
			max_time = elapsed;
		}
		if (elapsed < min_time && elapsed > 0) {
			min_time = elapsed;
		}
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Acceleration stuck at=%llu, acc called %u", elapsed, acc_count);
		avg_accum += elapsed;
		avg_count++;

		start_time = 0;
		acc_stop();
		acc_start();
	} else {

	}
	last_elapsed = elapsed;



	snprintf(msg, sizeof(msg), "Cur: %u:%u:%u\nMax: %u:%u:%u\nMin: %u:%u:%u\nAvg: %u:%u:%u\nCnt: %u",
			getHour(elapsed), getMin(elapsed), getSec(elapsed),
			getHour(max_time), getMin(max_time), getSec(max_time),
			getHour(min_time), getMin(min_time), getSec(min_time),
			getHour(getAvg()), getMin(getAvg()), getSec(getAvg()),
			avg_count
			);
	text_layer_set_text(text_layer, msg);
	restart_timer();
}

static void init(void) {
  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  acc_start();

  const bool animated = true;
  window_stack_push(window, animated);
  restart_timer();
}

static void deinit(void) {
  window_destroy(window);
  acc_stop();
}

int main(void) {
  init();

  app_event_loop();
  deinit();
}
