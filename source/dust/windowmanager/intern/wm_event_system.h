

/* -------------------------------------------------------------------- */
/** \name Cursor Keymap Status
 *
 * Show cursor keys in the status bar.
 * This is done by detecting changes to the state - full key-map lookups are expensive
 * so only perform this on changing tools, space types, pressing different modifier keys... etc.
 * \{ */

/** State storage to detect changes between calls to refresh the information. */
struct CursorKeymapInfo_State {
  uint8_t modifier;
  short space_type;
  short region_type;
  /** Never use, just compare memory for changes. */
  bToolRef tref;
};

struct CursorKeymapInfo {
  /**
   * 0: Mouse button index.
   * 1: Event type (click/press, drag).
   * 2: Text.
   */
  char text[3][2][128];
  wmEvent state_event;
  CursorKeymapInfo_State state;
};

static void wm_event_cursor_store(CursorKeymapInfo_State *state,
                                  const wmEvent *event,
                                  short space_type,
                                  short region_type,
                                  const bToolRef *tref)
{
  state->modifier = event->modifier;
  state->space_type = space_type;
  state->region_type = region_type;
  state->tref = tref ? *tref : bToolRef{};
}

const char *WM_window_cursor_keymap_status_get(const wmWindow *win,
                                               int button_index,
                                               int type_index)
{
  if (win->cursor_keymap_status != nullptr) {
    CursorKeymapInfo *cd = static_cast<CursorKeymapInfo *>(win->cursor_keymap_status);
    const char *msg = cd->text[button_index][type_index];
    if (*msg) {
      return msg;
    }
  }
  return nullptr;
}

ScrArea *WM_window_status_area_find(wmWindow *win, bScreen *screen)
{
  if (screen->state == SCREENFULL) {
    return nullptr;
  }
  ScrArea *area_statusbar = nullptr;
  LISTBASE_FOREACH (ScrArea *, area, &win->global_areas.areabase) {
    if (area->spacetype == SPACE_STATUSBAR) {
      area_statusbar = area;
      break;
    }
  }
  return area_statusbar;
}

void WM_window_status_area_tag_redraw(wmWindow *win)
{
  bScreen *screen = WM_window_get_active_screen(win);
  ScrArea *area = WM_window_status_area_find(win, screen);
  if (area != nullptr) {
    ED_area_tag_redraw(area);
  }
}
