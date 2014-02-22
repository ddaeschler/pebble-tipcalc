#include <pebble.h>

static const int DISP_BUFFER_LEN = 32;

static Window *window;

static TextLayer* bill_amount_layer;
static TextLayer* bill_amount_label;

static TextLayer* tip_percentage_layer;
static TextLayer* tip_percentage_label;

static TextLayer* tip_amount_layer;
static TextLayer* tip_amount_label;

static TextLayer* total_layer;
static TextLayer* total_label;

static unsigned int bill_amount;
static unsigned int tip_percentage;
static float total_amt;
static float tip_amt;

static char* bill_amt_buffer;
static char* tip_percent_buffer;
static char* tip_amt_buffer;
static char* total_amt_buffer;

typedef void (*StateEntry)();
typedef struct state
{
	ClickHandler up_single_click_handler;
	ClickHandler down_single_click_handler;
	ClickHandler select_click_handler;
	StateEntry state_entry;
	
} state;
	
static state* current_state;

/**
 * State prototypes and defs
 */
static void tipamt_select_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void tipamt_up_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void tipamt_down_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void tipamt_state_entry();

static state edit_tip_amount_state = 
	{ 	.up_single_click_handler = tipamt_up_single_click_handler, 
		.down_single_click_handler = tipamt_down_single_click_handler,
		.select_click_handler = tipamt_select_single_click_handler,		
		.state_entry = tipamt_state_entry 								};

static void billamt_select_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void billamt_up_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void billamt_down_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void billamt_state_entry();

static state edit_bill_amount_state = 
	{ 	.up_single_click_handler = billamt_up_single_click_handler, 
		.down_single_click_handler = billamt_down_single_click_handler,
		.select_click_handler = billamt_select_single_click_handler,
		.state_entry = billamt_state_entry								};
		

/**
 * Core functions
 */
void float_to_string(char* buffer, int bufferSize, double number)
{
	char decimalBuffer[5];

	snprintf(buffer, bufferSize, "%d", (int)number);
	strcat(buffer, ".");

	snprintf(decimalBuffer, 5, "%02d", (int)((double)(number - (int)number) * (double)100));
	strcat(buffer, decimalBuffer);
}

static void update_total_display() {
	float_to_string(total_amt_buffer + 1, DISP_BUFFER_LEN, total_amt);
	total_amt_buffer[0] = '$';
	
	text_layer_set_text(total_layer, total_amt_buffer);
}

static void recalc_total() {
	total_amt = bill_amount * (1 + (tip_percentage / 100.0f));
}

static void update_bill_amount_display() {
	snprintf(bill_amt_buffer, DISP_BUFFER_LEN, "$%i.00", bill_amount);
	text_layer_set_text(bill_amount_layer, bill_amt_buffer);
}

static void update_tip_percentage_display() {
	snprintf(tip_percent_buffer, DISP_BUFFER_LEN, "%i%%", tip_percentage);
	text_layer_set_text(tip_percentage_layer, tip_percent_buffer);
}

static void recalc_tip_amount() {
	tip_amt = bill_amount * (tip_percentage / 100.0f);
}

static void update_tip_amount_display() {
	float_to_string(tip_amt_buffer + 1, DISP_BUFFER_LEN, tip_amt);
	tip_amt_buffer[0] = '$';
	
	text_layer_set_text(tip_amount_layer, tip_amt_buffer);
}

static void calculate_and_display_new_amounts() {
	recalc_tip_amount();
	update_tip_amount_display();
	
	recalc_total();
	update_total_display();
}



/**
 * State handlers for the tip amount adjustment state
 */
static void tipamt_state_entry() {
	//when we enter the tipamt state, we need to update the colors to
	//highlight the tipamt label, and de-highlight the billamt label
	text_layer_set_background_color(tip_percentage_label, GColorBlack);
	text_layer_set_text_color(tip_percentage_label, GColorWhite);
	text_layer_set_background_color(bill_amount_label, GColorWhite);
	text_layer_set_text_color(bill_amount_label, GColorBlack);
}

static void tipamt_select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	//switch to the bill amount
	current_state = &edit_bill_amount_state;
	current_state->state_entry();
}

static void tipamt_up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	++tip_percentage;
	
	update_tip_percentage_display();
	
	calculate_and_display_new_amounts();
}

static void tipamt_down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (tip_percentage > 0) --tip_percentage;
	
	update_tip_percentage_display();
	
	calculate_and_display_new_amounts();
}

		

/**
 * State handlers for the billing amount adjustment state
 */
static void billamt_state_entry() {
	//when we enter the billamt state, we need to update the colors to
	//highlight the billamt label, and de-highlight the tipamt label
	text_layer_set_background_color(bill_amount_label, GColorBlack);
	text_layer_set_text_color(bill_amount_label, GColorWhite);
	text_layer_set_background_color(tip_percentage_label, GColorWhite);
	text_layer_set_text_color(tip_percentage_label, GColorBlack);
}

static void billamt_select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	//switch to the tip percentage
	current_state = &edit_tip_amount_state;
	current_state->state_entry();
}

static void billamt_up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	++bill_amount;
	
	update_bill_amount_display();
	calculate_and_display_new_amounts();
}

static void billamt_down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	if (bill_amount > 0) --bill_amount;
	
	update_bill_amount_display();
	calculate_and_display_new_amounts();
}

		
/**
 * Base click handling 
 */
static void do_select_single_click(ClickRecognizerRef recognizer, void *context) {
	current_state->select_click_handler(recognizer, context);
}
static void do_up_single_click(ClickRecognizerRef recognizer, void *context) {
	current_state->up_single_click_handler(recognizer, context);
}
static void do_down_single_click(ClickRecognizerRef recognizer, void *context) {
	current_state->down_single_click_handler(recognizer, context);
}

static void click_config_provider(void *context) {
	window_single_click_subscribe(BUTTON_ID_SELECT, do_select_single_click);
	window_single_repeating_click_subscribe(BUTTON_ID_UP, 30, do_up_single_click);
	window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 30, do_down_single_click);
}


/**
 * Startup/init/shutdown Routines
 */
static void window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	
	int AMT_WIDTH = (int)(bounds.size.w * 0.60f);
	const int TEXT_LAYER_HEIGHT = 30;
	
	//BILL AMOUNT
	bill_amount_layer = text_layer_create((GRect) { .origin = { 0, 0 }, .size = { AMT_WIDTH, TEXT_LAYER_HEIGHT } });
	text_layer_set_font(bill_amount_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
	text_layer_set_text(bill_amount_layer, "$0.00");
	text_layer_set_text_alignment(bill_amount_layer, GTextAlignmentRight);
	layer_add_child(window_layer, text_layer_get_layer(bill_amount_layer));
	
	bill_amount_label = text_layer_create((GRect) { .origin = { AMT_WIDTH , 0 }, .size = { bounds.size.w - AMT_WIDTH, TEXT_LAYER_HEIGHT } });
	text_layer_set_font(bill_amount_label, fonts_get_system_font(FONT_KEY_GOTHIC_24));
	text_layer_set_text(bill_amount_label, "Bill Amt");
	text_layer_set_text_alignment(bill_amount_label, GTextAlignmentRight);
	layer_add_child(window_layer, text_layer_get_layer(bill_amount_label));
	
	
	//TIP PERCENTAGE
	tip_percentage_layer = text_layer_create((GRect) { .origin = { 0, TEXT_LAYER_HEIGHT }, .size = { AMT_WIDTH, TEXT_LAYER_HEIGHT } });
	text_layer_set_font(tip_percentage_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
	text_layer_set_text(tip_percentage_layer, "0%");
	text_layer_set_text_alignment(tip_percentage_layer, GTextAlignmentRight);
	layer_add_child(window_layer, text_layer_get_layer(tip_percentage_layer));
	
	tip_percentage_label = text_layer_create((GRect) { .origin = { AMT_WIDTH , TEXT_LAYER_HEIGHT }, .size = { bounds.size.w - AMT_WIDTH, TEXT_LAYER_HEIGHT } });
	text_layer_set_font(tip_percentage_label, fonts_get_system_font(FONT_KEY_GOTHIC_24));
	text_layer_set_text(tip_percentage_label, "Tip %");
	text_layer_set_text_alignment(tip_percentage_label, GTextAlignmentRight);
	layer_add_child(window_layer, text_layer_get_layer(tip_percentage_label));
	
	//TIP AMT
	tip_amount_label = text_layer_create((GRect) { .origin = { 0, TEXT_LAYER_HEIGHT * 3 }, .size = { bounds.size.w - AMT_WIDTH, TEXT_LAYER_HEIGHT } });
	text_layer_set_font(tip_amount_label, fonts_get_system_font(FONT_KEY_GOTHIC_24));
	text_layer_set_text(tip_amount_label, "Tip Amt: ");
	text_layer_set_text_alignment(tip_amount_label, GTextAlignmentLeft);
	layer_add_child(window_layer, text_layer_get_layer(tip_amount_label));
	
	tip_amount_layer = text_layer_create((GRect) { .origin = { bounds.size.w - AMT_WIDTH , TEXT_LAYER_HEIGHT * 3 }, .size = { AMT_WIDTH - 1, TEXT_LAYER_HEIGHT } });
	text_layer_set_font(tip_amount_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
	text_layer_set_text(tip_amount_layer, "$0.00");
	text_layer_set_text_alignment(tip_amount_layer, GTextAlignmentRight);
	layer_add_child(window_layer, text_layer_get_layer(tip_amount_layer));
	
	//TOTAL
	total_label = text_layer_create((GRect) { .origin = { 0, TEXT_LAYER_HEIGHT * 4 }, .size = { bounds.size.w - AMT_WIDTH, TEXT_LAYER_HEIGHT } });
	text_layer_set_font(total_label, fonts_get_system_font(FONT_KEY_GOTHIC_24));
	text_layer_set_text(total_label, "Total: ");
	text_layer_set_text_alignment(total_label, GTextAlignmentLeft);
	layer_add_child(window_layer, text_layer_get_layer(total_label));
	
	total_layer = text_layer_create((GRect) { .origin = { bounds.size.w - AMT_WIDTH , TEXT_LAYER_HEIGHT * 4 }, .size = { AMT_WIDTH - 1, TEXT_LAYER_HEIGHT } });
	text_layer_set_font(total_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
	text_layer_set_text(total_layer, "$0.00");
	text_layer_set_text_alignment(total_layer, GTextAlignmentRight);
	layer_add_child(window_layer, text_layer_get_layer(total_layer));
	
	update_bill_amount_display();
	update_tip_percentage_display();
	calculate_and_display_new_amounts();
	
	current_state->state_entry();
}

static void window_unload(Window *window) {
	text_layer_destroy(bill_amount_layer);
	text_layer_destroy(bill_amount_label);
	text_layer_destroy(tip_percentage_layer);
	text_layer_destroy(tip_percentage_label);
	text_layer_destroy(total_layer);
	text_layer_destroy(total_label);
}

static void init(void) {
  	bill_amount = 20;
	tip_percentage = 15;
	
	bill_amt_buffer = (char*)malloc(DISP_BUFFER_LEN);
	tip_percent_buffer = (char*)malloc(DISP_BUFFER_LEN);
	tip_amt_buffer = (char*)malloc(DISP_BUFFER_LEN);
	total_amt_buffer = (char*)malloc(DISP_BUFFER_LEN);
	
	current_state = &edit_bill_amount_state;
	
	window = window_create();
	window_set_click_config_provider(window, click_config_provider);
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload,
	});
	
	const bool animated = true;
	window_stack_push(window, animated);
}

static void deinit(void) {
	window_destroy(window);
	free(bill_amt_buffer);
	free(tip_percent_buffer);
	free(tip_amt_buffer);
	free(total_amt_buffer);
}

int main(void) {
	init();

	APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

	app_event_loop();
	deinit();
}
