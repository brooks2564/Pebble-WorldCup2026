// ── World Cup 2026 Live  ·  main.c ──────────────────────────────────────
#include <pebble.h>

#define KEY_AWAY_ABBR    1
#define KEY_HOME_ABBR    2
#define KEY_AWAY_SCORE   3
#define KEY_HOME_SCORE   4
#define KEY_MATCH_MIN    5
#define KEY_MATCH_PERIOD 6
#define KEY_STATUS       7
#define KEY_TEAM_IDX     8
#define KEY_START_TIME   9
#define KEY_GROUP_INFO   10
#define KEY_VIBRATE      11
#define KEY_NEXT_MATCH   12
#define KEY_BATTERY_BAR  13
#define KEY_TICKER       14
#define KEY_TV_NETWORK   15
#define KEY_TICKER_SPEED 16

#define NUM_TEAMS    48
#define PERSIST_TEAM 1
#define PERSIST_VIB  2
#define PERSIST_BAT  3
#define PERSIST_TICKER_SPEED 4

#ifdef PBL_PLATFORM_EMERY
#define TICKER_H 24
#else
#define TICKER_H 18
#endif

// 48 World Cup 2026 teams — sorted alphabetically by FIFA/ESPN code
static const char *TEAM_ABBR[NUM_TEAMS] = {
  "ALB","ARG","AUS","AUT","BEL","BRA","CMR","CAN",
  "CIV","COL","CRC","CRO","CZE","DEN","ECU","EGY",
  "ENG","ESP","FRA","GER","GHA","GRE","HON","HUN",
  "IRN","IRQ","JPN","JOR","KOR","MLI","MAR","MEX",
  "NED","NGA","NZL","PAN","PAR","POL","POR","SAU",
  "SCO","SEN","SRB","SUI","TUR","UKR","URU","USA"
};

static Window    *s_window;
static Layer     *s_canvas;
static Layer     *s_ticker_clip;
static TextLayer *s_ticker_cur;
static TextLayer *s_ticker_next;
static AppTimer  *s_ticker_timer;
static bool       s_anim_running = false;

// Ticker state
#define MAX_MATCHES 8
#define MATCH_LEN   22
static char s_ticker_raw[200];
static char s_matches[MAX_MATCHES][MATCH_LEN];
static int  s_match_count;
static int  s_match_idx;

// Match state
static char s_time_buf[6]    = "00:00";
static char s_date_buf[14]   = "";
static char s_away_abbr[5]   = "---";
static char s_home_abbr[5]   = "---";
static int  s_away_score     = 0;
static int  s_home_score     = 0;
static char s_match_min[12]  = "";
static int  s_match_period   = 0;
static char s_status[8]      = "off";
static char s_start_time[10] = "";
static char s_group_info[22] = "";
static bool s_vibrate        = true;
static char s_next_match[24] = "";
static bool s_battery_bar    = true;
static int  s_battery_pct    = 100;
static int  s_team_idx       = 47;   // USA
static int  s_prev_score     = -1;
static bool s_i_am_away      = false;
static int  s_ticker_speed   = 5000;
static char s_tv_network[24] = "";

static void request_match_data(void);

// ── Ticker ─────────────────────────────────────────────────────────────────
static void ticker_update_text(void) {
  if (!s_ticker_cur) return;
  if (s_match_count == 0) {
    text_layer_set_text(s_ticker_cur, s_date_buf);
  } else if (s_match_idx < s_match_count) {
    text_layer_set_text(s_ticker_cur, s_matches[s_match_idx]);
  }
}

static void ticker_animation_stopped(Animation *anim, bool finished, void *ctx) {
  int w = layer_get_bounds(s_ticker_clip).size.w;
  int h = TICKER_H;
  layer_set_frame(text_layer_get_layer(s_ticker_next), GRect(0, 0, w, h));
  layer_set_frame(text_layer_get_layer(s_ticker_cur),  GRect(0, h, w, h));
  TextLayer *tmp = s_ticker_cur;
  s_ticker_cur   = s_ticker_next;
  s_ticker_next  = tmp;
  s_anim_running = false;
  animation_destroy(anim);
}

static void ticker_advance(void *ctx) {
  if (s_match_count > 1 && s_ticker_cur && s_ticker_next && !s_anim_running) {
    int next_idx = (s_match_idx + 1) % s_match_count;
    const char *next_text = (s_match_count == 0) ? s_date_buf : s_matches[next_idx];
    text_layer_set_text(s_ticker_next, next_text);

    int w = layer_get_bounds(s_ticker_clip).size.w;
    int h = TICKER_H;
    layer_set_frame(text_layer_get_layer(s_ticker_next), GRect(0, h, w, h));

    GRect cur_from = GRect(0,  0, w, h);
    GRect cur_to   = GRect(0, -h, w, h);
    GRect nxt_from = GRect(0,  h, w, h);
    GRect nxt_to   = GRect(0,  0, w, h);

    Animation *anim_cur  = (Animation*)property_animation_create_layer_frame(
      text_layer_get_layer(s_ticker_cur),  &cur_from, &cur_to);
    Animation *anim_next = (Animation*)property_animation_create_layer_frame(
      text_layer_get_layer(s_ticker_next), &nxt_from, &nxt_to);

    animation_set_duration(anim_cur,  300);
    animation_set_duration(anim_next, 300);
    animation_set_curve(anim_cur,  AnimationCurveEaseInOut);
    animation_set_curve(anim_next, AnimationCurveEaseInOut);
    animation_set_handlers(anim_next, (AnimationHandlers){
      .stopped = ticker_animation_stopped
    }, NULL);

    Animation *spawn = animation_spawn_create(anim_cur, anim_next, NULL);
    s_anim_running = true;
    animation_schedule(spawn);
    s_match_idx = next_idx;
  }
  s_ticker_timer = app_timer_register((uint32_t)s_ticker_speed, ticker_advance, NULL);
}

static void ticker_parse_and_start(void) {
  s_match_count = 0;
  s_match_idx   = 0;
  int ri = 0, gi = 0, ci = 0;
  while (s_ticker_raw[ri] != '\0' && gi < MAX_MATCHES) {
    char ch = s_ticker_raw[ri++];
    if (ch == '|') {
      s_matches[gi][ci] = '\0';
      gi++;
      ci = 0;
    } else if (ci < MATCH_LEN - 1) {
      s_matches[gi][ci++] = ch;
    }
  }
  if (ci > 0 && gi < MAX_MATCHES) {
    s_matches[gi][ci] = '\0';
    gi++;
  }
  s_match_count = gi;

  if (s_ticker_timer) {
    app_timer_cancel(s_ticker_timer);
    s_ticker_timer = NULL;
  }
  s_anim_running = false;
  ticker_update_text();
  if (s_match_count > 1) {
    s_ticker_timer = app_timer_register((uint32_t)s_ticker_speed, ticker_advance, NULL);
  }
}

// ── Team colors (emery only) ───────────────────────────────────────────────
#ifdef PBL_PLATFORM_EMERY
static GColor team_color(const char *abbr) {
  if (!abbr) return GColorWhite;
  if (strcmp(abbr,"USA")==0) return GColorCobaltBlue;
  if (strcmp(abbr,"MEX")==0) return GColorIslamicGreen;
  if (strcmp(abbr,"CAN")==0) return GColorRed;
  if (strcmp(abbr,"BRA")==0) return GColorIslamicGreen;
  if (strcmp(abbr,"ARG")==0) return GColorCobaltBlue;
  if (strcmp(abbr,"FRA")==0) return GColorCobaltBlue;
  if (strcmp(abbr,"GER")==0) return GColorLightGray;
  if (strcmp(abbr,"ENG")==0) return GColorRed;
  if (strcmp(abbr,"ESP")==0) return GColorRed;
  if (strcmp(abbr,"POR")==0) return GColorRed;
  if (strcmp(abbr,"NED")==0) return GColorOrange;
  if (strcmp(abbr,"BEL")==0) return GColorRed;
  if (strcmp(abbr,"MAR")==0) return GColorRed;
  if (strcmp(abbr,"SEN")==0) return GColorIslamicGreen;
  if (strcmp(abbr,"GHA")==0) return GColorLightGray;
  if (strcmp(abbr,"NGA")==0) return GColorIslamicGreen;
  if (strcmp(abbr,"CMR")==0) return GColorIslamicGreen;
  if (strcmp(abbr,"EGY")==0) return GColorRed;
  if (strcmp(abbr,"CIV")==0) return GColorOrange;
  if (strcmp(abbr,"MLI")==0) return GColorYellow;
  if (strcmp(abbr,"JPN")==0) return GColorCobaltBlue;
  if (strcmp(abbr,"KOR")==0) return GColorRed;
  if (strcmp(abbr,"AUS")==0) return GColorYellow;
  if (strcmp(abbr,"IRN")==0) return GColorIslamicGreen;
  if (strcmp(abbr,"SAU")==0) return GColorIslamicGreen;
  if (strcmp(abbr,"JOR")==0) return GColorRed;
  if (strcmp(abbr,"IRQ")==0) return GColorIslamicGreen;
  if (strcmp(abbr,"CRO")==0) return GColorRed;
  if (strcmp(abbr,"SRB")==0) return GColorRed;
  if (strcmp(abbr,"SUI")==0) return GColorRed;
  if (strcmp(abbr,"DEN")==0) return GColorRed;
  if (strcmp(abbr,"AUT")==0) return GColorRed;
  if (strcmp(abbr,"TUR")==0) return GColorRed;
  if (strcmp(abbr,"POL")==0) return GColorRed;
  if (strcmp(abbr,"SCO")==0) return GColorCobaltBlue;
  if (strcmp(abbr,"GRE")==0) return GColorCobaltBlue;
  if (strcmp(abbr,"ALB")==0) return GColorRed;
  if (strcmp(abbr,"CZE")==0) return GColorRed;
  if (strcmp(abbr,"HUN")==0) return GColorRed;
  if (strcmp(abbr,"URU")==0) return GColorCobaltBlue;
  if (strcmp(abbr,"COL")==0) return GColorYellow;
  if (strcmp(abbr,"ECU")==0) return GColorYellow;
  if (strcmp(abbr,"PAR")==0) return GColorRed;
  if (strcmp(abbr,"HON")==0) return GColorCobaltBlue;
  if (strcmp(abbr,"CRC")==0) return GColorRed;
  if (strcmp(abbr,"PAN")==0) return GColorRed;
  if (strcmp(abbr,"NZL")==0) return GColorLightGray;
  return GColorWhite;
}

static void draw_team_text(GContext *ctx, const char *text, GFont font, GRect rect,
                           GTextOverflowMode overflow, GTextAlignment align, GColor color) {
  GRect r = rect;
  graphics_context_set_text_color(ctx, GColorBlack);
  r.origin.x -= 1; graphics_draw_text(ctx, text, font, r, overflow, align, NULL);
  r.origin.x += 2; graphics_draw_text(ctx, text, font, r, overflow, align, NULL);
  r.origin.x -= 1; r.origin.y -= 1; graphics_draw_text(ctx, text, font, r, overflow, align, NULL);
  r.origin.y += 2; graphics_draw_text(ctx, text, font, r, overflow, align, NULL);
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font, rect, overflow, align, NULL);
}
#endif

// ── Canvas ─────────────────────────────────────────────────────────────────
static void canvas_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int w   = b.size.w;
  int h   = b.size.h;
  int split = h * 3 / 10;
  int by    = split + 2;
#ifdef PBL_ROUND
  int hpad = 18;
#else
  int hpad = 2;
#endif

  // Background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // Battery bar
  if (s_battery_bar) {
    int bw = (w * s_battery_pct) / 100;
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_rect(ctx, GRect(0, h-3, w, 3), 0, GCornerNone);
    GColor bc = s_battery_pct > 50 ? GColorGreen :
                s_battery_pct > 20 ? GColorYellow : GColorRed;
    graphics_context_set_fill_color(ctx, bc);
    graphics_fill_rect(ctx, GRect(0, h-3, bw, 3), 0, GCornerNone);
  }

  // Divider
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_line(ctx, GPoint(0, split), GPoint(w, split));

#ifdef PBL_PLATFORM_EMERY
  GFont f28 = fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
  GFont f24 = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GFont f18 = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont f14 = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  int score_w=110, abbr_w=44;
  int score_y=by-8,  score_h=32;
  int group_y=by+16, status_y=by+36, status_h=26;
  int extra_y=by+64;
#else
  GFont f28 = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GFont f24 = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont f18 = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GFont f14 = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  int score_w=68, abbr_w=36;
  int score_y=by-4,  score_h=26;
  int group_y=by+22, status_y=by+26, status_h=20;
  int extra_y=by+46;
#endif
  GFont fsm = fonts_get_system_font(FONT_KEY_GOTHIC_14);

  // Time + date
  graphics_context_set_text_color(ctx, GColorWhite);
#ifdef PBL_PLATFORM_EMERY
  graphics_draw_text(ctx, s_time_buf, f24,
    GRect(hpad, 2, 72, 30), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, s_date_buf, fonts_get_system_font(FONT_KEY_GOTHIC_24),
    GRect(68, 2, w-68-hpad, 26), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
#else
  graphics_draw_text(ctx, s_time_buf, f24,
    GRect(hpad, 2, 60, 24), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, s_date_buf, fonts_get_system_font(FONT_KEY_GOTHIC_18),
    GRect(56, 2, w-56-hpad, 20), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
#endif

  // No match
  if (strcmp(s_status, "off") == 0) {
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "No Match Today", f24,
      GRect(0, by+2, w, 28), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    char tl[12];
    snprintf(tl, sizeof(tl), "~ %s ~", TEAM_ABBR[s_team_idx]);
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, tl, f14,
      GRect(0, by+32, w, 18), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    if (s_next_match[0]) {
      graphics_context_set_text_color(ctx, GColorLightGray);
      graphics_draw_text(ctx, "Next:", f14,
        GRect(hpad, by+54, 42, 18), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
      graphics_draw_text(ctx, s_next_match, f14,
        GRect(hpad+44, by+54, w-hpad-46, 18), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
    return;
  }

  // Score row: [AWAY]  score - score  [HOME]
#ifdef PBL_PLATFORM_EMERY
  draw_team_text(ctx, s_away_abbr, f24,
    GRect(hpad, score_y, abbr_w, score_h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft,
    team_color(s_away_abbr));
#else
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, s_away_abbr, f24,
    GRect(hpad, score_y, abbr_w, score_h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
#endif
  char sc[12];
  snprintf(sc, sizeof(sc), "%d - %d", s_away_score, s_home_score);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, sc, f28,
    GRect(w/2 - score_w/2, score_y, score_w, score_h), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
#ifdef PBL_PLATFORM_EMERY
  draw_team_text(ctx, s_home_abbr, f24,
    GRect(w-abbr_w-hpad, score_y, abbr_w, score_h), GTextOverflowModeTrailingEllipsis, GTextAlignmentRight,
    team_color(s_home_abbr));
#else
  graphics_draw_text(ctx, s_home_abbr, f24,
    GRect(w-abbr_w-hpad, score_y, abbr_w, score_h), GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
#endif

  // Group / stage info row
  if (s_group_info[0]) {
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, s_group_info, fsm,
      GRect(0, group_y, w, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  // Status row (yellow): match minute, kickoff time, or "Final"
  char stat[20];
  if (strcmp(s_status, "live") == 0) {
    if (s_match_min[0])
      snprintf(stat, sizeof(stat), "%s", s_match_min);
    else
      snprintf(stat, sizeof(stat), "Live");
  } else if (strcmp(s_status, "pre") == 0) {
    snprintf(stat, sizeof(stat), "%s", s_start_time[0] ? s_start_time : "Pre-Match");
  } else {
    snprintf(stat, sizeof(stat), "Final");
  }
  graphics_context_set_text_color(ctx, GColorYellow);
  graphics_draw_text(ctx, stat, f18,
    GRect(0, status_y, w, status_h), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

  // Pre-game: TV network
  if (strcmp(s_status, "pre") == 0 && s_tv_network[0]) {
    char tv[28];
    snprintf(tv, sizeof(tv), "TV: %s", s_tv_network);
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, tv, f14,
      GRect(0, extra_y, w, 18), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  // Final: next match
  if (strcmp(s_status, "final") == 0 && s_next_match[0]) {
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, "Next:", f14,
      GRect(hpad, extra_y, 42, 18), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, s_next_match, f14,
      GRect(hpad+44, extra_y, w-hpad-46, 18), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

// ── Clock ──────────────────────────────────────────────────────────────────
static void update_clock(struct tm *t) {
  clock_copy_time_string(s_time_buf, sizeof(s_time_buf));
  strftime(s_date_buf, sizeof(s_date_buf), "%a  %b %d", t);
  if (s_match_count == 0 && s_ticker_cur)
    text_layer_set_text(s_ticker_cur, s_date_buf);
}

static void tick_handler(struct tm *t, TimeUnits u) {
  update_clock(t);
  if (u & MINUTE_UNIT) request_match_data();
  layer_mark_dirty(s_canvas);
}

// ── Inbox ──────────────────────────────────────────────────────────────────
static void inbox_received(DictionaryIterator *iter, void *ctx) {
  Tuple *t;
  t = dict_find(iter, KEY_AWAY_ABBR);
  if (t) { strncpy(s_away_abbr, t->value->cstring, 4); s_away_abbr[4]=0; }
  t = dict_find(iter, KEY_HOME_ABBR);
  if (t) { strncpy(s_home_abbr, t->value->cstring, 4); s_home_abbr[4]=0; }
  s_i_am_away = strcmp(s_away_abbr, TEAM_ABBR[s_team_idx])==0;

  t = dict_find(iter, KEY_AWAY_SCORE);   if (t) s_away_score   = (int)t->value->int32;
  t = dict_find(iter, KEY_HOME_SCORE);   if (t) s_home_score   = (int)t->value->int32;
  t = dict_find(iter, KEY_MATCH_PERIOD); if (t) s_match_period = (int)t->value->int32;

  t = dict_find(iter, KEY_MATCH_MIN);
  if (t) { strncpy(s_match_min, t->value->cstring, 11); s_match_min[11]=0; }
  t = dict_find(iter, KEY_STATUS);
  if (t) { strncpy(s_status, t->value->cstring, 7); s_status[7]=0; }
  t = dict_find(iter, KEY_START_TIME);
  if (t) { strncpy(s_start_time, t->value->cstring, 9); s_start_time[9]=0; }
  t = dict_find(iter, KEY_GROUP_INFO);
  if (t) { strncpy(s_group_info, t->value->cstring, 21); s_group_info[21]=0; }
  t = dict_find(iter, KEY_NEXT_MATCH);
  if (t) { strncpy(s_next_match, t->value->cstring, 23); s_next_match[23]=0; }
  t = dict_find(iter, KEY_TV_NETWORK);
  if (t) { strncpy(s_tv_network, t->value->cstring, 23); s_tv_network[23]=0; }

  t = dict_find(iter, KEY_VIBRATE);
  if (t) { s_vibrate = (bool)t->value->int32; persist_write_bool(PERSIST_VIB, s_vibrate); }
  t = dict_find(iter, KEY_BATTERY_BAR);
  if (t) { s_battery_bar = (bool)t->value->int32; persist_write_bool(PERSIST_BAT, s_battery_bar); }

  t = dict_find(iter, KEY_TICKER);
  if (t) {
    strncpy(s_ticker_raw, t->value->cstring, 199);
    s_ticker_raw[199] = 0;
    ticker_parse_and_start();
  }

  t = dict_find(iter, KEY_TICKER_SPEED);
  if (t) {
    int spd = (int)t->value->int32;
    if (spd==5000||spd==10000||spd==30000||spd==60000) {
      s_ticker_speed = spd;
      persist_write_int(PERSIST_TICKER_SPEED, s_ticker_speed);
      if (s_ticker_timer) {
        app_timer_cancel(s_ticker_timer);
        s_ticker_timer = app_timer_register((uint32_t)s_ticker_speed, ticker_advance, NULL);
      }
    }
  }

  // Vibrate on goal scored by my team
  if (strcmp(s_status, "live")==0 && s_vibrate) {
    int my = s_i_am_away ? s_away_score : s_home_score;
    if (s_prev_score >= 0 && my > s_prev_score) vibes_double_pulse();
    s_prev_score = my;
  } else {
    s_prev_score = -1;
  }

  t = dict_find(iter, KEY_TEAM_IDX);
  if (t) {
    int idx = (int)t->value->int32;
    if (idx >= 0 && idx < NUM_TEAMS && idx != s_team_idx) {
      s_team_idx = idx;
      persist_write_int(PERSIST_TEAM, s_team_idx);
      strncpy(s_away_abbr, "---", 4); strncpy(s_home_abbr, "---", 4);
      s_away_score = s_home_score = 0;
      s_match_min[0] = s_start_time[0] = s_group_info[0] = 0;
      s_next_match[0] = s_tv_network[0] = 0;
      s_match_period = 0;
      s_prev_score = -1;
      strncpy(s_status, "off", 7);
      request_match_data();
    }
  }

  layer_mark_dirty(s_canvas);
}

static void inbox_dropped(AppMessageResult r, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Dropped: %d", (int)r);
}

static void request_match_data(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) return;
  dict_write_int(iter, KEY_TEAM_IDX, &s_team_idx, sizeof(int), true);
  app_message_outbox_send();
}

static void battery_handler(BatteryChargeState state) {
  s_battery_pct = state.charge_percent;
  if (s_canvas) layer_mark_dirty(s_canvas);
}

// ── Window ─────────────────────────────────────────────────────────────────
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  int w = bounds.size.w;

  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

#ifdef PBL_PLATFORM_EMERY
  s_ticker_clip = layer_create(GRect(0, 32, w, TICKER_H));
  GFont ticker_font = fonts_get_system_font(FONT_KEY_GOTHIC_24);
#else
  s_ticker_clip = layer_create(GRect(0, 28, w, TICKER_H));
  GFont ticker_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
#endif
  layer_add_child(root, s_ticker_clip);

  s_ticker_cur  = text_layer_create(GRect(0, 0,         w, TICKER_H));
  s_ticker_next = text_layer_create(GRect(0, TICKER_H,  w, TICKER_H));
  TextLayer *tls[2] = {s_ticker_cur, s_ticker_next};
  for (int i = 0; i < 2; i++) {
    text_layer_set_background_color(tls[i], GColorBlack);
    text_layer_set_text_color(tls[i], GColorWhite);
    text_layer_set_font(tls[i], ticker_font);
    text_layer_set_overflow_mode(tls[i], GTextOverflowModeTrailingEllipsis);
    text_layer_set_text(tls[i], "");
    layer_add_child(s_ticker_clip, text_layer_get_layer(tls[i]));
  }
}

static void window_unload(Window *window) {
  if (s_ticker_timer) { app_timer_cancel(s_ticker_timer); s_ticker_timer = NULL; }
  if (s_ticker_cur)   { text_layer_destroy(s_ticker_cur);  s_ticker_cur  = NULL; }
  if (s_ticker_next)  { text_layer_destroy(s_ticker_next); s_ticker_next = NULL; }
  if (s_ticker_clip)  { layer_destroy(s_ticker_clip);      s_ticker_clip = NULL; }
  if (s_canvas)       { layer_destroy(s_canvas);           s_canvas      = NULL; }
}

static void init(void) {
  s_match_count = 0; s_match_idx = 0;
  memset(s_ticker_raw, 0, sizeof(s_ticker_raw));

  if (persist_exists(PERSIST_TEAM))         s_team_idx    = persist_read_int(PERSIST_TEAM);
  if (persist_exists(PERSIST_VIB))          s_vibrate     = persist_read_bool(PERSIST_VIB);
  if (persist_exists(PERSIST_BAT))          s_battery_bar = persist_read_bool(PERSIST_BAT);
  if (persist_exists(PERSIST_TICKER_SPEED)) s_ticker_speed= persist_read_int(PERSIST_TICKER_SPEED);

  time_t now = time(NULL);
  update_clock(localtime(&now));

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load, .unload = window_unload
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
  s_battery_pct = battery_state_service_peek().charge_percent;

  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_open(512, 64);
  request_match_data();
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) { init(); app_event_loop(); deinit(); return 0; }
