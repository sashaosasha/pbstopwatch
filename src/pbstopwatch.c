#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"


#define MY_UUID { 0xE4, 0x81, 0xE4, 0x34, 0xF5, 0xC0, 0x40, 0xED, 0xAB, 0xFD, 0x47, 0xCC, 0xDE, 0xF5, 0x8F, 0xF8 }
PBL_APP_INFO(MY_UUID,
             "Stopwatch", "Sasha Ognev",
             1, 0, /* App version */
             RESOURCE_ID_STOPWATCH_ICON,
             APP_INFO_STANDARD_APP);


#define MAX_TIMERS  5
#define TEXT_BUF_SIZE 16
static const int SCREEN_WIDTH = 144;
static const int SCREEN_HEIGHT = 168;
static const int TOOLBAR_WIDTH = 24;
static const int TIMER_COOKIE = 1;
static const int STOPWATCH_TOP = 16;

BmpContainer buttons[4];
enum ENUM_BUTTONS
{
   BTN_GO = 0,
   BTN_STOP = 1,
   BTN_LAP = 2,
   BTN_CLEAR = 3
};

typedef struct StopwatchTimer
{
    PblTm     time;
    bool      is_running;
    TextLayer layer;
    char      text[TEXT_BUF_SIZE];
} StopwatchTimer;

typedef struct Stopwatch
{
    StopwatchTimer  timers[MAX_TIMERS];
    Layer           buttonsLayer;
    int             used_timers;
} Stopwatch;

Stopwatch stopwatch;
Window window;
AppTimerHandle timer_handle;
TextLayer debug_layer;

void init_debug()
{
    text_layer_init(&debug_layer, GRect(0, SCREEN_HEIGHT - 30, SCREEN_WIDTH, 30));
    text_layer_set_text_color(&debug_layer, GColorWhite);
    text_layer_set_background_color(&debug_layer, GColorClear);
    text_layer_set_font(&debug_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_overflow_mode(&debug_layer, GTextOverflowModeWordWrap);
    layer_add_child(&window.layer, &debug_layer.layer);
}

void log_msg(const char* msg)
{
    text_layer_set_text(&debug_layer, msg);
}

void draw_timer(int i)
{
    string_format_time(stopwatch.timers[i].text, TEXT_BUF_SIZE, "%T", &stopwatch.timers[i].time);
    text_layer_set_text(&stopwatch.timers[i].layer, stopwatch.timers[i].text);
}

// Up button click -> start/stop main stopwatch
//
void up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
    (void)recognizer;
    (void)window;

    stopwatch.timers[0].is_running = !stopwatch.timers[0].is_running;
    layer_mark_dirty(&stopwatch.buttonsLayer);
}

// Up button long click -> stop all watches
//
void up_long_click_handler(ClickRecognizerRef recognizer, Window *window) {
    (void)recognizer;
    (void)window;
    for (int i = 0; i < MAX_TIMERS; ++i)
    {
        stopwatch.timers[i].is_running = false;
    }
    layer_mark_dirty(&stopwatch.buttonsLayer);
}

void clear_timer(int i)
{
    StopwatchTimer* tmr = &stopwatch.timers[i];
    tmr->is_running = false;
    memset(&tmr->time, 0, sizeof(PblTm));
}

// One click on down -> clear main
//
void down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
    (void)recognizer;
    (void)window;
    clear_timer(0);
    draw_timer(0);
}

// long click on down -> clear all
//
void down_long_click_handler(ClickRecognizerRef recognizer, Window *window) {
    (void)recognizer;
    (void)window;
    for (int i = 0; i < MAX_TIMERS; ++i)
    {
        clear_timer(i);
        if (i > 0)
        {
            text_layer_set_text(&stopwatch.timers[i].layer, "");
        }
    }
    draw_timer(0);
}

// Shift all timers down preserving timer to layer association but moving time value and running state
//
void shift_timers(bool reset_main)
{
    if (stopwatch.used_timers < MAX_TIMERS)
    {
        ++stopwatch.used_timers;
    }
    for (int i = stopwatch.used_timers-1; i >= 1; --i)
    {
        memcpy(&stopwatch.timers[i].time, &stopwatch.timers[i-1].time, sizeof(PblTm));
        stopwatch.timers[i].is_running = stopwatch.timers[i-1].is_running;
    }
    if (reset_main)
    {
        memset(&stopwatch.timers[0].time, 0, sizeof(PblTm));
    }
    for (int i = 0; i < stopwatch.used_timers; ++i)
    {
        draw_timer(i);
    }
}

// middle button click -> lap and freeze lap time + reset main timer
//
void select_single_click_handler(ClickRecognizerRef recognizer, Window *window)
{
  (void)recognizer;
  (void)window;

  stopwatch.timers[0].is_running = false;
  shift_timers(true);
  stopwatch.timers[0].is_running = true;
}

// middle button long click -> lap and continue lap timer + reset main
//
void select_long_click_handler(ClickRecognizerRef recognizer, Window *window)
{
  (void)recognizer;
  (void)window;

  shift_timers(true);
  stopwatch.timers[0].is_running = true;
}

// middle button dbl click -> lap and freeze + continue main timer
//
void select_dbl_click_handler(ClickRecognizerRef recognizer, Window *window)
{
  (void)recognizer;
  (void)window;

  stopwatch.timers[0].is_running = false;
  shift_timers(false);
  stopwatch.timers[0].is_running = true;
}

// Configure button click handlers
//
void click_config_provider(ClickConfig **config, Window *window) {
  (void)window;

  config[BUTTON_ID_UP]->click.handler = (ClickHandler) up_single_click_handler;
  config[BUTTON_ID_UP]->long_click.handler = (ClickHandler) up_long_click_handler;
  config[BUTTON_ID_UP]->long_click.delay_ms = 700;

  config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) select_single_click_handler;
  config[BUTTON_ID_SELECT]->long_click.handler = (ClickHandler) select_long_click_handler;
  config[BUTTON_ID_SELECT]->long_click.delay_ms = 500;
  config[BUTTON_ID_SELECT]->multi_click.handler = (ClickHandler) select_dbl_click_handler;
  config[BUTTON_ID_SELECT]->multi_click.min = 2;
  config[BUTTON_ID_SELECT]->multi_click.max = 2;
  config[BUTTON_ID_SELECT]->multi_click.last_click_only = true;
  config[BUTTON_ID_SELECT]->multi_click.timeout = 1200;

  config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) down_single_click_handler;
  config[BUTTON_ID_DOWN]->long_click.delay_ms = 700;
  config[BUTTON_ID_DOWN]->long_click.handler = (ClickHandler) down_long_click_handler;
}


void draw_play(GContext* ctx, GPoint origin)
{
    for (int i = 0; i < 6; ++i)
    {
        graphics_draw_line(ctx, GPoint(origin.x + 5 + i, origin.y + 2 + i),
                                GPoint(origin.x + 5 + i, origin.y + 12 - i));
    }
}

void draw_stop(GContext* ctx, GPoint origin)
{
    for (int i = 0; i < 3; ++i)
    {
        graphics_draw_line(ctx, GPoint(origin.x + 4+i, origin.y + 2),
                                GPoint(origin.x + 4+i, origin.y + 12));
        graphics_draw_line(ctx, GPoint(origin.x + 10+i, origin.y + 2),
                                GPoint(origin.x + 10+i, origin.y + 12));
    }
}

void draw_lap(GContext* ctx, GPoint origin)
{
    graphics_draw_line(ctx, GPoint(origin.x + 7, origin.y + 2), GPoint(origin.x + 7, origin.y + 12));
    graphics_draw_line(ctx, GPoint(origin.x + 8, origin.y + 2), GPoint(origin.x + 8, origin.y + 12));
    graphics_draw_line(ctx, GPoint(origin.x + 5, origin.y + 10), GPoint(origin.x + 10, origin.y + 10));
    graphics_draw_line(ctx, GPoint(origin.x + 6, origin.y + 11), GPoint(origin.x + 9, origin.y + 11));
}

void draw_reset(GContext* ctx, GPoint origin)
{
    graphics_draw_line(ctx, GPoint(origin.x + 4, origin.y + 2), GPoint(origin.x + 10, origin.y + 12));
    graphics_draw_line(ctx, GPoint(origin.x + 10, origin.y + 2), GPoint(origin.x + 4, origin.y + 12));
}

void draw_bitmap(GContext* ctx, GBitmap* bmp, GPoint pt)
{
    graphics_draw_bitmap_in_rect(ctx, bmp, GRect(pt.x, pt.y, bmp->bounds.size.w, bmp->bounds.size.h));
}

// Update function for toolbar layer.
// Draw buttons
void toolbar_update_proc(Layer *me, GContext* ctx)
{
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_fill_color(ctx, GColorBlack);

    int sz = 16;
    int top_button_y = 8;
    int middle_button_y = SCREEN_HEIGHT/2 - sz;
    int bottom_button_dy = 20;

/*
    int left = me->bounds.origin.x;
    if (stopwatch.timers[0].is_running )
    {
        draw_stop(ctx, GPoint(left, top_button_y));
    }
    else
    {
        draw_play(ctx, GPoint(left, top_button_y));
    }
    draw_lap(ctx, GPoint(left, middle_button_y));
    draw_reset(ctx,  GPoint(left, SCREEN_HEIGHT - bottom_button_dy - sz));
*/
    int left = 2;
    int top = stopwatch.timers[0].is_running ? BTN_STOP : BTN_GO;
    draw_bitmap(ctx, &buttons[top].bmp, GPoint(left, top_button_y));
    draw_bitmap(ctx, &buttons[BTN_LAP].bmp, GPoint(left, middle_button_y));
    draw_bitmap(ctx, &buttons[BTN_CLEAR].bmp, GPoint(left, SCREEN_HEIGHT - bottom_button_dy - sz));


/*
    graphics_draw_circle(ctx, GPoint(left + 8, 20), 3);
    graphics_draw_circle(ctx, GPoint(left + 8, SCREEN_HEIGHT - 20), 3);
    graphics_draw_circle(ctx, GPoint(left + 8, SCREEN_HEIGHT / 2), 3);
*/

}


// Initializes text layer for a timer given it's index
// Timer at 0 comes in bigger font
void init_timer_layer(StopwatchTimer* timer, int timer_index)
{
    text_layer_init(&timer->layer, GRect(4, STOPWATCH_TOP + timer_index * 20 + (timer_index > 0 ? 10 : 0) ,
                                         140 - TOOLBAR_WIDTH, timer_index == 0 ? 30 : 20));
    text_layer_set_text_color(&timer->layer, GColorWhite);
    text_layer_set_background_color(&timer->layer, GColorClear);
    text_layer_set_font(&timer->layer,
                        fonts_get_system_font(timer_index == 0 ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_18_BOLD));
    layer_add_child(&window.layer, &timer->layer.layer);
}

// Initializes layer with graphics on buttons
void init_button_layer(Layer* layer)
{
    layer_init(layer, GRect(SCREEN_WIDTH-TOOLBAR_WIDTH, 0, TOOLBAR_WIDTH, SCREEN_HEIGHT));
    layer->update_proc = toolbar_update_proc;
    layer_add_child(&window.layer, layer);
}

// Pebble initialization routine
//
void handle_init(AppContextRef ctx)
{
  (void)ctx;

  window_init(&window, "Stopwatch");
  window_stack_push(&window, true);
  window_set_background_color(&window, GColorBlack);

  window_set_click_config_provider(&window, (ClickConfigProvider) click_config_provider);

  for (int i = 0; i < MAX_TIMERS; ++i)
  {
      init_timer_layer(&stopwatch.timers[i], i);
      memset(&stopwatch.timers[i].time, 0, sizeof(PblTm));
      stopwatch.timers[i].is_running = false;
  }
  text_layer_set_text(&stopwatch.timers[0].layer, "00:00:00");
  stopwatch.used_timers = 1;

  init_button_layer(&stopwatch.buttonsLayer);
  init_debug();

  resource_init_current_app(&VERSION);
  bmp_init_container(RESOURCE_ID_GO_BUTTON, &buttons[BTN_GO]);
  bmp_init_container(RESOURCE_ID_STOP_BUTTON, &buttons[BTN_STOP]);
  bmp_init_container(RESOURCE_ID_LAP_BUTTON, &buttons[BTN_LAP]);
  bmp_init_container(RESOURCE_ID_CLEAR_BUTTON, &buttons[BTN_CLEAR]);
}

// Adds 1 second to a time
//
void increment_time(PblTm* time)
{
    time->tm_sec += 1;
    time->tm_min += (time->tm_sec / 60);
    time->tm_sec = time->tm_sec % 60;
    time->tm_hour += (time->tm_min / 60);
    time->tm_min = time->tm_min % 60;
}

// Tick every second
//
void handle_second_tick(AppContextRef ctx, PebbleTickEvent *t)
{
    for (int i = 0; i < MAX_TIMERS; ++i)
    {
        if (stopwatch.timers[i].is_running)
        {
            increment_time(&stopwatch.timers[i].time);
            draw_timer(i);
        }
    }

}

void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .tick_info = {
      .tick_handler = &handle_second_tick,
      .tick_units = SECOND_UNIT
    }
  };
  app_event_loop(params, &handlers);
}
