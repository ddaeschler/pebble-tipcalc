/*
 * Pebble Tip Calculator
 * Copyright (C) 2014 David C. Daeschler
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <pebble.h>

static const int DISP_BUFFER_LEN = 32;

static Window *window;
static AppTimer* timer = 0;

static TextLayer* bill_amount_layer;
static TextLayer* bill_amount_label;

static TextLayer* tip_percentage_layer;
static TextLayer* tip_percentage_label;

static TextLayer* tip_amount_layer;
static TextLayer* tip_amount_label;

static TextLayer* total_layer;
static TextLayer* total_label;

//amounts are stored as an integer representing tenths of a cent
static const unsigned int TO_DOLLARS = 1000;
static const unsigned int TO_CENTS = 10;


static unsigned int bill_amount;
static unsigned int tip_percentage;
static unsigned int total_amount;
static unsigned int tip_amount;

static char* bill_amt_buffer;
static char* tip_percent_buffer;
static char* tip_amt_buffer;
static char* total_amt_buffer;

typedef void (*StateEntry)();
typedef void (*HandleTimer)();
typedef struct state
{
	ClickHandler up_single_click_handler;
	ClickHandler down_single_click_handler;
	ClickHandler select_click_handler;
	HandleTimer timer_handler;
	StateEntry state_entry;
	
} state;

typedef struct dollars_cents
{
	unsigned int dollars;
	unsigned int cents;
	
} dollars_cents;
	
static state* current_state;

typedef enum {NONE, DOLLARS, CENTS} blinktype;
blinktype blink = NONE;

/**
 * State prototypes and defs
 */
static void tipamt_select_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void tipamt_up_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void tipamt_down_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void tipamt_timer_handler(void* data);
static void tipamt_state_entry();

static state edit_tip_amount_state = 
	{ 	.up_single_click_handler = tipamt_up_single_click_handler, 
		.down_single_click_handler = tipamt_down_single_click_handler,
		.select_click_handler = tipamt_select_single_click_handler,		
		.state_entry = tipamt_state_entry, 								
		.timer_handler = tipamt_timer_handler									};

static void billamt_dollars_select_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void billamt_dollars_up_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void billamt_dollars_down_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void billamt_dollars_timer_handler(void* data);
static void billamt_dollars_state_entry();

static state edit_bill_amount_dollars_state = 
	{ 	.up_single_click_handler = billamt_dollars_up_single_click_handler, 
		.down_single_click_handler = billamt_dollars_down_single_click_handler,
		.select_click_handler = billamt_dollars_select_single_click_handler,
		.state_entry = billamt_dollars_state_entry,								
		.timer_handler = billamt_dollars_timer_handler							};

static void billamt_cents_select_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void billamt_cents_up_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void billamt_cents_down_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void billamt_cents_timer_handler(void* data);
static void billamt_cents_state_entry();

static state edit_bill_amount_cents_state = 
	{ 	.up_single_click_handler = billamt_cents_up_single_click_handler, 
		.down_single_click_handler = billamt_cents_down_single_click_handler,
		.select_click_handler = billamt_cents_select_single_click_handler,
		.state_entry = billamt_cents_state_entry,							
		.timer_handler = billamt_cents_timer_handler							};


/**
 * Core functions
 */
static void format_dollars_cents_blink(dollars_cents ds, char* out, blinktype mblink) {
	if (ds.cents >= 10) {
		if (mblink == NONE) {
			snprintf(out, DISP_BUFFER_LEN, "$%i.%i", ds.dollars, ds.cents);

		} else if (mblink == DOLLARS) {
			snprintf(out, DISP_BUFFER_LEN, ".%i", ds.cents);

		} else if (mblink == CENTS) {
			snprintf(out, DISP_BUFFER_LEN, "$%i.__", ds.dollars);
		}

	} else {
		if (mblink == NONE) {
			snprintf(out, DISP_BUFFER_LEN, "$%i.0%i", ds.dollars, ds.cents);

		} else if (mblink == DOLLARS) {
			snprintf(out, DISP_BUFFER_LEN, ".0%i", ds.cents);

		} else if (mblink == CENTS) {
			snprintf(out, DISP_BUFFER_LEN, "$%i.__", ds.dollars);
		}
	}
}

static void format_dollars_cents(dollars_cents ds, char* out) {
	format_dollars_cents_blink(ds, out, NONE); 
}

static dollars_cents to_dollars_cents(unsigned int num) {
	unsigned int dollars = num / TO_DOLLARS;
	unsigned int cents = (num - (dollars * TO_DOLLARS)) / TO_CENTS;
	
	return (dollars_cents) { .dollars = dollars, .cents = cents };
}

static int round_up(int numToRound, int multiple) { 
	if (multiple == 0) 
	{ 
		return numToRound; 
	} 

	int remainder = numToRound % multiple;
	if (remainder == 0) 
	{
		return numToRound;
	}
	
	return numToRound + multiple - remainder;
}

static void update_total_display() {
	dollars_cents ds = to_dollars_cents(total_amount);
	format_dollars_cents(ds, total_amt_buffer);
	
	text_layer_set_text(total_layer, total_amt_buffer);
}

static void recalc_total() {
	total_amount = bill_amount + tip_amount;
}

static void update_bill_amount_display() {
	dollars_cents ds = to_dollars_cents(bill_amount);
	format_dollars_cents_blink(ds, bill_amt_buffer, blink);
	
	text_layer_set_text(bill_amount_layer, bill_amt_buffer);
}

static void update_tip_percentage_display() {
	snprintf(tip_percent_buffer, DISP_BUFFER_LEN, "%i%%", tip_percentage);
	text_layer_set_text(tip_percentage_layer, tip_percent_buffer);
}

static void recalc_tip_amount() {
	//1000 = 1, 100 = .1, 10 = 0.01
	//multiply the tip amount * bill_amount then scale (divide) by 100 (percent)
	tip_amount = round_up((bill_amount * tip_percentage) / (TO_DOLLARS / 10), TO_CENTS);
}

static void update_tip_amount_display() {
	dollars_cents ds = to_dollars_cents(tip_amount);
	format_dollars_cents(ds, tip_amt_buffer);
	
	text_layer_set_text(tip_amount_layer, tip_amt_buffer);
}

static void calculate_and_display_new_amounts() {
	recalc_tip_amount();
	update_tip_amount_display();
	
	recalc_total();
	update_total_display();
}


/**
 * Timer functions
 */

const int TIMER_PERIOD = 500;
const int MAX_NUM_BLINKS = 10;
int num_blinks = 0;

static void handle_timer(void* data);

static void reschedule_timer(bool resetBlinkCounter) {
	if (timer != 0) 
	{
		app_timer_cancel(timer);
		timer = 0;
	}
	
	bool reschedule = false;
	if (resetBlinkCounter) 
	{
		num_blinks = 0;
		reschedule = true;
	}
	else
	{
		if (num_blinks < MAX_NUM_BLINKS)
		{
			num_blinks++;
			reschedule = true;
		}
	}
	
	if (reschedule) 
	{
		timer = app_timer_register(TIMER_PERIOD, &handle_timer, NULL);
	}
	else
	{
		blink = NONE;
		update_bill_amount_display();
	}
}

static void handle_timer(void* data) {
	timer = 0;
	current_state->timer_handler(data);

	//always reschedule the timer
	reschedule_timer(false);
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
	
	//no blink here
	blink = NONE;
	update_bill_amount_display();
}

static void tipamt_select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	//switch to the bill amount
	current_state = &edit_bill_amount_dollars_state;
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

static void tipamt_timer_handler(void* data) {
	if (blink != NONE) {
		blink = NONE;
		update_bill_amount_display();
	}
}
		

/**
 * State handlers for the billing amount dollars adjustment state
 */
static void billamt_dollars_state_entry() {
	//when we enter the billamt state, we need to update the colors to
	//highlight the billamt label, and de-highlight the tipamt label
	text_layer_set_background_color(bill_amount_label, GColorBlack);
	text_layer_set_text_color(bill_amount_label, GColorWhite);
	text_layer_set_background_color(tip_percentage_label, GColorWhite);
	text_layer_set_text_color(tip_percentage_label, GColorBlack);
	
	//initial blink type is none
	blink = NONE;
	update_bill_amount_display();
	
	reschedule_timer(true);
}

static void billamt_dollars_select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	//switch to cents
	current_state = &edit_bill_amount_cents_state;
	current_state->state_entry();
}

static void billamt_dollars_up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	blink = NONE;
	bill_amount += TO_DOLLARS;
	
	update_bill_amount_display();
	calculate_and_display_new_amounts();
	
	reschedule_timer(true);
}

static void billamt_dollars_down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	blink = NONE;
	if (bill_amount > TO_DOLLARS) bill_amount -= TO_DOLLARS;
	else bill_amount = 0;
	
	update_bill_amount_display();
	calculate_and_display_new_amounts();
	
	reschedule_timer(true);
}

static void billamt_dollars_timer_handler(void* data) {
	if (blink == NONE) blink = DOLLARS;
	else blink = NONE;
	
	update_bill_amount_display();
}


/**
 * State handlers for the billing amount cents adjustment state
 */
static void billamt_cents_state_entry() {
	//when we enter the billamt state, we need to update the colors to
	//highlight the billamt label, and de-highlight the tipamt label
	text_layer_set_background_color(bill_amount_label, GColorBlack);
	text_layer_set_text_color(bill_amount_label, GColorWhite);
	text_layer_set_background_color(tip_percentage_label, GColorWhite);
	text_layer_set_text_color(tip_percentage_label, GColorBlack);
	
	//initial blink type is NONE
	blink = NONE;
	update_bill_amount_display();
	
	reschedule_timer(true);
}

static void billamt_cents_select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	//switch to the tip percentage
	current_state = &edit_tip_amount_state;
	current_state->state_entry();
}

static void billamt_cents_up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	blink = NONE;
	bill_amount += TO_CENTS;
	
	update_bill_amount_display();
	calculate_and_display_new_amounts();
	
	reschedule_timer(true);
}

static void billamt_cents_down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
	blink = NONE;
	if (bill_amount > TO_CENTS) bill_amount -= TO_CENTS;
	else bill_amount = 0;
	
	update_bill_amount_display();
	calculate_and_display_new_amounts();
	
	reschedule_timer(true);
}

static void billamt_cents_timer_handler(void* data) {
	if (blink == NONE) blink = CENTS;
	else blink = NONE;
	
	update_bill_amount_display();
}

		
/**
 * Base click and timer handling 
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
  	bill_amount = 20 * TO_DOLLARS;
	tip_percentage = 15;
	
	bill_amt_buffer = (char*)malloc(DISP_BUFFER_LEN);
	tip_percent_buffer = (char*)malloc(DISP_BUFFER_LEN);
	tip_amt_buffer = (char*)malloc(DISP_BUFFER_LEN);
	total_amt_buffer = (char*)malloc(DISP_BUFFER_LEN);
	
	current_state = &edit_bill_amount_dollars_state;
	
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
	
	reschedule_timer(true);
	app_event_loop();
	
	deinit();
}
