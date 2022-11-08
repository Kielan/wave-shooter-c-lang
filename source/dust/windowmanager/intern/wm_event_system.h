#ifdef WITH_INPUT_NDOF
/* Adds custom-data to event. */
static void attach_ndof_data(wmEvent *event, const GHOST_TEventNDOFMotionData *ghost)
{
  wmNDOFMotionData *data = MEM_cnew<wmNDOFMotionData>("Custom-data NDOF");

  const float ts = U.ndof_sensitivity;
  const float rs = U.ndof_orbit_sensitivity;

  mul_v3_v3fl(data->tvec, &ghost->tx, ts);
  mul_v3_v3fl(data->rvec, &ghost->rx, rs);

  if (U.ndof_flag & NDOF_PAN_YZ_SWAP_AXIS) {
    float t;
    t = data->tvec[1];
    data->tvec[1] = -data->tvec[2];
    data->tvec[2] = t;
  }

  data->dt = ghost->dt;

  data->progress = (wmProgress)ghost->progress;

  event->custom = EVT_DATA_NDOF_MOTION;
  event->customdata = data;
  event->customdata_free = true;
}
#endif /* WITH_INPUT_NDOF */

/* Imperfect but probably usable... draw/enable drags to other windows. */
static wmWindow *wm_event_cursor_other_windows(wmWindowManager *wm, wmWindow *win, wmEvent *event)
{
  /* If GHOST doesn't support window positioning, don't use this feature at all. */
  const static int8_t supports_window_position = GHOST_SupportsWindowPosition();
  if (!supports_window_position) {
    return nullptr;
  }

  if (wm->windows.first == wm->windows.last) {
    return nullptr;
  }

  /* In order to use window size and mouse position (pixels), we have to use a WM function. */

  /* Check if outside, include top window bar. */
  int event_xy[2] = {UNPACK2(event->xy)};
  if (event_xy[0] < 0 || event_xy[1] < 0 || event_xy[0] > WM_window_pixels_x(win) ||
      event_xy[1] > WM_window_pixels_y(win) + 30) {
    /* Let's skip windows having modal handlers now. */
    /* Potential XXX ugly... I wouldn't have added a `modalhandlers` list
     * (introduced in rev 23331, ton). */
    LISTBASE_FOREACH (wmEventHandler *, handler, &win->modalhandlers) {
      if (ELEM(handler->type, WM_HANDLER_TYPE_UI, WM_HANDLER_TYPE_OP)) {
        return nullptr;
      }
    }

    wmWindow *win_other = WM_window_find_under_cursor(win, event_xy, event_xy);
    if (win_other && win_other != win) {
      copy_v2_v2_int(event->xy, event_xy);
      return win_other;
    }
  }
  return nullptr;
}

static bool wm_event_is_double_click(const wmEvent *event)
{
  if ((event->type == event->prev_type) && (event->prev_val == KM_RELEASE) &&
      (event->val == KM_PRESS)) {
    if (ISMOUSE_BUTTON(event->type) && WM_event_drag_test(event, event->prev_press_xy)) {
      /* Pass. */
    }
    else {
      if ((PIL_check_seconds_timer() - event->prev_press_time) * 1000 < U.dbl_click_time) {
        return true;
      }
    }
  }

  return false;
}

/**
 * Copy the current state to the previous event state.
 */
static void wm_event_prev_values_set(wmEvent *event, wmEvent *event_state)
{
  event->prev_val = event_state->prev_val = event_state->val;
  event->prev_type = event_state->prev_type = event_state->type;
}

static void wm_event_prev_click_set(wmEvent *event_state)
{
  event_state->prev_press_time = PIL_check_seconds_timer();
  event_state->prev_press_type = event_state->type;
  event_state->prev_press_modifier = event_state->modifier;
  event_state->prev_press_keymodifier = event_state->keymodifier;
  event_state->prev_press_xy[0] = event_state->xy[0];
  event_state->prev_press_xy[1] = event_state->xy[1];
}

static wmEvent *wm_event_add_mousemove(wmWindow *win, const wmEvent *event)
{
  wmEvent *event_last = static_cast<wmEvent *>(win->event_queue.last);

  /* Some painting operators want accurate mouse events, they can
   * handle in between mouse move moves, others can happily ignore
   * them for better performance. */
  if (event_last && event_last->type == MOUSEMOVE) {
    event_last->type = INBETWEEN_MOUSEMOVE;
    event_last->flag = (eWM_EventFlag)0;
  }

  wmEvent *event_new = wm_event_add(win, event);
  if (event_last == nullptr) {
    event_last = win->eventstate;
  }

  copy_v2_v2_int(event_new->prev_xy, event_last->xy);
  return event_new;
}


static wmEvent *wm_event_add_mousemove_to_head(wmWindow *win)
{
  /* Use the last handled event instead of `win->eventstate` because the state of the modifiers
   * and previous values should be set based on the last state, not using values from the future.
   * So this gives an accurate simulation of mouse motion before the next event is handled. */
  const wmEvent *event_last = win->event_last_handled;

  wmEvent tevent;
  if (event_last) {
    tevent = *event_last;

    tevent.flag = (eWM_EventFlag)0;
    tevent.utf8_buf[0] = '\0';

    wm_event_custom_clear(&tevent);
  }
  else {
    memset(&tevent, 0x0, sizeof(tevent));
  }

  tevent.type = MOUSEMOVE;
  tevent.val = KM_NOTHING;
  copy_v2_v2_int(tevent.prev_xy, tevent.xy);

  wmEvent *event_new = wm_event_add(win, &tevent);
  BLI_remlink(&win->event_queue, event_new);
  BLI_addhead(&win->event_queue, event_new);

  copy_v2_v2_int(event_new->prev_xy, event_last->xy);
  return event_new;
}

static wmEvent *wm_event_add_trackpad(wmWindow *win, const wmEvent *event, int deltax, int deltay)
{
  /* Ignore in between track-pad events for performance, we only need high accuracy
   * for painting with mouse moves, for navigation using the accumulated value is ok. */
  wmEvent *event_last = static_cast<wmEvent *>(win->event_queue.last);
  if (event_last && event_last->type == event->type) {
    deltax += event_last->xy[0] - event_last->prev_xy[0];
    deltay += event_last->xy[1] - event_last->prev_xy[1];

    wm_event_free_last(win);
  }

  /* Set prev_xy, the delta is computed from this in operators. */
  wmEvent *event_new = wm_event_add(win, event);
  event_new->prev_xy[0] = event_new->xy[0] - deltax;
  event_new->prev_xy[1] = event_new->xy[1] - deltay;

  return event_new;
}

/**
 * Update the event-state for any kind of event that supports #KM_PRESS / #KM_RELEASE.
 *
 * \param check_double_click: Optionally skip checking for double-click events.
 * Needed for event simulation where the time of click events is not so predictable.
 */
static void wm_event_state_update_and_click_set_ex(wmEvent *event,
                                                   wmEvent *event_state,
                                                   const bool is_keyboard,
                                                   const bool check_double_click)
{
  BLI_assert(ISKEYBOARD_OR_BUTTON(event->type));
  BLI_assert(ELEM(event->val, KM_PRESS, KM_RELEASE));

  /* Only copy these flags into the `event_state`. */
  const eWM_EventFlag event_state_flag_mask = WM_EVENT_IS_REPEAT;

  wm_event_prev_values_set(event, event_state);

  /* Copy to event state. */
  event_state->val = event->val;
  event_state->type = event->type;
  /* It's important only to write into the `event_state` modifier for keyboard
   * events because emulate MMB clears one of the modifiers in `event->modifier`,
   * making the second press not behave as if the modifier is pressed, see T96279. */
  if (is_keyboard) {
    event_state->modifier = event->modifier;
  }
  event_state->flag = (event->flag & event_state_flag_mask);
  /* NOTE: It's important that `keymodifier` is handled in the keyboard event handling logic
   * since the `event_state` and the `event` are not kept in sync. */

  /* Double click test. */
  if (check_double_click && wm_event_is_double_click(event)) {
    CLOG_INFO(WM_LOG_HANDLERS, 1, "DBL_CLICK: detected");
    event->val = KM_DBL_CLICK;
  }
  else if (event->val == KM_PRESS) {
    if ((event->flag & WM_EVENT_IS_REPEAT) == 0) {
      wm_event_prev_click_set(event_state);
    }
  }
}

static void wm_event_state_update_and_click_set(wmEvent *event,
                                                wmEvent *event_state,
                                                const GHOST_TEventType type)
{
  const bool is_keyboard = ELEM(type, GHOST_kEventKeyDown, GHOST_kEventKeyUp);
  const bool check_double_click = true;
  wm_event_state_update_and_click_set_ex(event, event_state, is_keyboard, check_double_click);
}

/* Returns true when the two events corresponds to a press of the same key with the same modifiers.
 */
static bool wm_event_is_same_key_press(const wmEvent &event_a, const wmEvent &event_b)
{
  if (event_a.val != KM_PRESS || event_b.val != KM_PRESS) {
    return false;
  }

  if (event_a.modifier != event_b.modifier || event_a.type != event_b.type) {
    return false;
  }

  return true;
}

/**
 * Returns true if the event is a key press event which is to be ignored and not added to the event
 * queue.
 *
 * A key press event will be ignored if there is already matched key press in the queue.
 * This avoids the event queue "clogging" in the situations when there is an operator bound to a
 * key press event and the execution time of the operator is longer than the key repeat.
 */
static bool wm_event_is_ignorable_key_press(const wmWindow *win, const wmEvent &event)
{
  if (BLI_listbase_is_empty(&win->event_queue)) {
    /* If the queue is empty never ignore the event.
     * Empty queue at this point means that the events are handled fast enough, and there is no
     * reason to ignore anything. */
    return false;
  }

  if ((event.flag & WM_EVENT_IS_REPEAT) == 0) {
    /* Only ignore repeat events from the keyboard, and allow accumulation of non-repeat events.
     *
     * The goal of this check is to allow events coming from a keyboard macro software, which can
     * generate events quicker than the main loop handles them. In this case we want all events to
     * be handled (unless the keyboard macro software tags them as repeat) because otherwise it
     * will become impossible to get reliable results of automated events testing. */
    return false;
  }

  const wmEvent &last_event = *static_cast<const wmEvent *>(win->event_queue.last);

  return wm_event_is_same_key_press(last_event, event);
}

void wm_event_add_ghostevent(wmWindowManager *wm, wmWindow *win, int type, void *customdata)
{
  if (UNLIKELY(G.f & G_FLAG_EVENT_SIMULATE)) {
    return;
  }

  /**
   * Having both, \a event and \a event_state, can be highly confusing to work with,
   * but is necessary for our current event system, so let's clear things up a bit:
   *
   * - Data added to event only will be handled immediately,
   *   but will not be copied to the next event.
   * - Data added to \a event_state only stays,
   *   but is handled with the next event -> execution delay.
   * - Data added to event and \a event_state stays and is handled immediately.
   */
  wmEvent event, *event_state = win->eventstate;

  /* Initialize and copy state (only mouse x y and modifiers). */
  event = *event_state;
  event.flag = (eWM_EventFlag)0;

  /**
   * Always support accessing the last key press/release. This is set from `win->eventstate`,
   * so it will always be a valid event type to store in the previous state.
   *
   * Note that these values are intentionally _not_ set in the `win->eventstate`,
   * as copying these values only makes sense when `win->eventstate->{val/type}` would be
   * written to (which only happens for some kinds of events).
   * If this was done it could leave `win->eventstate` previous and current value
   * set to the same key press/release state which doesn't make sense.
   */
  event.prev_type = event.type;
  event.prev_val = event.val;

  /* Always use modifiers from the active window since
   * changes to modifiers aren't sent to inactive windows, see: T66088. */
  if ((wm->winactive != win) && (wm->winactive && wm->winactive->eventstate)) {
    event.modifier = wm->winactive->eventstate->modifier;
    event.keymodifier = wm->winactive->eventstate->keymodifier;
  }

  /* Ensure the event state is correct, any deviation from this may cause bugs.
   *
   * NOTE: #EVENT_NONE is set when unknown keys are pressed,
   * while not common, avoid a false alarm. */
#ifndef NDEBUG
  if ((event_state->type || event_state->val) && /* Ignore cleared event state. */
      !(ISKEYBOARD_OR_BUTTON(event_state->type) || (event_state->type == EVENT_NONE))) {
    CLOG_WARN(WM_LOG_HANDLERS,
              "Non-keyboard/mouse button found in 'win->eventstate->type = %d'",
              event_state->type);
  }
  if ((event_state->prev_type || event_state->prev_val) && /* Ignore cleared event state. */
      !(ISKEYBOARD_OR_BUTTON(event_state->prev_type) || (event_state->type == EVENT_NONE))) {
    CLOG_WARN(WM_LOG_HANDLERS,
              "Non-keyboard/mouse button found in 'win->eventstate->prev_type = %d'",
              event_state->prev_type);
  }
#endif

  switch (type) {
    /* Mouse move, also to inactive window (X11 does this). */
    case GHOST_kEventCursorMove: {
      GHOST_TEventCursorData *cd = static_cast<GHOST_TEventCursorData *>(customdata);

      copy_v2_v2_int(event.xy, &cd->x);
      wm_stereo3d_mouse_offset_apply(win, event.xy);
      wm_tablet_data_from_ghost(&cd->tablet, &event.tablet);

      event.type = MOUSEMOVE;
      event.val = KM_NOTHING;
      {
        wmEvent *event_new = wm_event_add_mousemove(win, &event);
        copy_v2_v2_int(event_state->xy, event_new->xy);
        event_state->tablet.is_motion_absolute = event_new->tablet.is_motion_absolute;
      }

      /* Also add to other window if event is there, this makes overdraws disappear nicely. */
      /* It remaps mouse-coord to other window in event. */
      wmWindow *win_other = wm_event_cursor_other_windows(wm, win, &event);
      if (win_other) {
        wmEvent event_other = *win_other->eventstate;

        /* Use the modifier state of this window. */
        event_other.modifier = event.modifier;
        event_other.keymodifier = event.keymodifier;

        /* See comment for this operation on `event` for details. */
        event_other.prev_type = event_other.type;
        event_other.prev_val = event_other.val;

        copy_v2_v2_int(event_other.xy, event.xy);
        event_other.type = MOUSEMOVE;
        event_other.val = KM_NOTHING;
        {
          wmEvent *event_new = wm_event_add_mousemove(win_other, &event_other);
          copy_v2_v2_int(win_other->eventstate->xy, event_new->xy);
          win_other->eventstate->tablet.is_motion_absolute = event_new->tablet.is_motion_absolute;
        }
      }

      break;
    }
    case GHOST_kEventTrackpad: {
      GHOST_TEventTrackpadData *pd = static_cast<GHOST_TEventTrackpadData *>(customdata);
      switch (pd->subtype) {
        case GHOST_kTrackpadEventMagnify:
          event.type = MOUSEZOOM;
          pd->deltaX = -pd->deltaX;
          pd->deltaY = -pd->deltaY;
          break;
        case GHOST_kTrackpadEventSmartMagnify:
          event.type = MOUSESMARTZOOM;
          break;
        case GHOST_kTrackpadEventRotate:
          event.type = MOUSEROTATE;
          break;
        case GHOST_kTrackpadEventScroll:
        default:
          event.type = MOUSEPAN;
          break;
      }

      event.xy[0] = event_state->xy[0] = pd->x;
      event.xy[1] = event_state->xy[1] = pd->y;
      event.val = KM_NOTHING;

      /* The direction is inverted from the device due to system preferences. */
      if (pd->isDirectionInverted) {
        event.flag |= WM_EVENT_SCROLL_INVERT;
      }

      wm_event_add_trackpad(win, &event, pd->deltaX, -pd->deltaY);
      break;
    }
    /* Mouse button. */
    case GHOST_kEventButtonDown:
    case GHOST_kEventButtonUp: {
      GHOST_TEventButtonData *bd = static_cast<GHOST_TEventButtonData *>(customdata);

      /* Get value and type from Ghost. */
      event.val = (type == GHOST_kEventButtonDown) ? KM_PRESS : KM_RELEASE;

      if (bd->button == GHOST_kButtonMaskLeft) {
        event.type = LEFTMOUSE;
      }
      else if (bd->button == GHOST_kButtonMaskRight) {
        event.type = RIGHTMOUSE;
      }
      else if (bd->button == GHOST_kButtonMaskButton4) {
        event.type = BUTTON4MOUSE;
      }
      else if (bd->button == GHOST_kButtonMaskButton5) {
        event.type = BUTTON5MOUSE;
      }
      else if (bd->button == GHOST_kButtonMaskButton6) {
        event.type = BUTTON6MOUSE;
      }
      else if (bd->button == GHOST_kButtonMaskButton7) {
        event.type = BUTTON7MOUSE;
      }
      else {
        event.type = MIDDLEMOUSE;
      }

      /* Get tablet data. */
      wm_tablet_data_from_ghost(&bd->tablet, &event.tablet);

      wm_eventemulation(&event, false);
      wm_event_state_update_and_click_set(&event, event_state, (GHOST_TEventType)type);

      /* Add to other window if event is there (not to both!). */
      wmWindow *win_other = wm_event_cursor_other_windows(wm, win, &event);
      if (win_other) {
        wmEvent event_other = *win_other->eventstate;

        /* Use the modifier state of this window. */
        event_other.modifier = event.modifier;
        event_other.keymodifier = event.keymodifier;

        /* See comment for this operation on `event` for details. */
        event_other.prev_type = event_other.type;
        event_other.prev_val = event_other.val;

        copy_v2_v2_int(event_other.xy, event.xy);

        event_other.type = event.type;
        event_other.val = event.val;
        event_other.tablet = event.tablet;

        wm_event_add(win_other, &event_other);
      }
      else {
        wm_event_add(win, &event);
      }

      break;
    }
    /* Keyboard. */
    case GHOST_kEventKeyDown:
    case GHOST_kEventKeyUp: {
      GHOST_TEventKeyData *kd = static_cast<GHOST_TEventKeyData *>(customdata);
      event.type = convert_key(kd->key);
      if (UNLIKELY(event.type == EVENT_NONE)) {
        break;
      }

      /* Might be not null terminated. */
      memcpy(event.utf8_buf, kd->utf8_buf, sizeof(event.utf8_buf));
      if (kd->is_repeat) {
        event.flag |= WM_EVENT_IS_REPEAT;
      }
      event.val = (type == GHOST_kEventKeyDown) ? KM_PRESS : KM_RELEASE;

      wm_eventemulation(&event, false);

      /* Exclude arrow keys, escape, etc from text input. */
      if (type == GHOST_kEventKeyUp) {
        /* Ghost should do this already for key up. */
        if (event.utf8_buf[0]) {
          CLOG_ERROR(WM_LOG_EVENTS,
                     "ghost on your platform is misbehaving, utf8 events on key up!");
        }
        event.utf8_buf[0] = '\0';
      }
      else {
        if (event.utf8_buf[0] < 32 && event.utf8_buf[0] > 0) {
          event.utf8_buf[0] = '\0';
        }
      }

      if (event.utf8_buf[0]) {
        /* NOTE(@campbellbarton): Detect non-ASCII characters stored in `utf8_buf`,
         * ideally this would never happen but it can't be ruled out for X11 which has
         * special handling of Latin1 when building without UTF8 support.
         * Avoid regressions by adding this conversions, it should eventually be removed. */
        if ((event.utf8_buf[0] >= 0x80) && (event.utf8_buf[1] == '\0')) {
          const uint c = uint(event.utf8_buf[0]);
          int utf8_buf_len = BLI_str_utf8_from_unicode(c, event.utf8_buf, sizeof(event.utf8_buf));
          CLOG_ERROR(WM_LOG_EVENTS,
                     "ghost detected non-ASCII single byte character '%u', converting to utf8 "
                     "('%.*s', length=%d)",
                     c,
                     utf8_buf_len,
                     event.utf8_buf,
                     utf8_buf_len);
        }

        if (BLI_str_utf8_size(event.utf8_buf) == -1) {
          CLOG_ERROR(WM_LOG_EVENTS,
                     "ghost detected an invalid unicode character '%d'",
                     int(uchar(event.utf8_buf[0])));
          event.utf8_buf[0] = '\0';
        }
      }

      /* NOTE(@campbellbarton): Setting the modifier state based on press/release
       * is technically incorrect.
       *
       * - The user might hold both left/right modifier keys, then only release one.
       *
       *   This could be solved by storing a separate flag for the left/right modifiers,
       *   and combine them into `event.modifiers`.
       *
       * - The user might have multiple keyboards (or keyboard + NDOF device)
       *   where it's possible to press the same modifier key multiple times.
       *
       *   This could be solved by tracking the number of held modifier keys,
       *   (this is in fact what LIBXKB does), however doing this relies on all GHOST
       *   back-ends properly reporting every press/release as any mismatch could result
       *   in modifier keys being stuck (which is very bad!).
       *
       * To my knowledge users never reported a bug relating to these limitations so
       * it seems reasonable to keep the current logic. */

      switch (event.type) {
        case EVT_LEFTSHIFTKEY:
        case EVT_RIGHTSHIFTKEY: {
          SET_FLAG_FROM_TEST(event.modifier, (event.val == KM_PRESS), KM_SHIFT);
          break;
        }
        case EVT_LEFTCTRLKEY:
        case EVT_RIGHTCTRLKEY: {
          SET_FLAG_FROM_TEST(event.modifier, (event.val == KM_PRESS), KM_CTRL);
          break;
        }
        case EVT_LEFTALTKEY:
        case EVT_RIGHTALTKEY: {
          SET_FLAG_FROM_TEST(event.modifier, (event.val == KM_PRESS), KM_ALT);
          break;
        }
        case EVT_OSKEY: {
          SET_FLAG_FROM_TEST(event.modifier, (event.val == KM_PRESS), KM_OSKEY);
          break;
        }
        default: {
          if (event.val == KM_PRESS) {
            if (event.keymodifier == 0) {
              /* Only set in `eventstate`, for next event. */
              event_state->keymodifier = event.type;
            }
          }
          else {
            BLI_assert(event.val == KM_RELEASE);
            if (event.keymodifier == event.type) {
              event.keymodifier = event_state->keymodifier = 0;
            }
          }

          /* This case happens on holding a key pressed,
           * it should not generate press events with the same key as modifier. */
          if (event.keymodifier == event.type) {
            event.keymodifier = 0;
          }
          else if (event.keymodifier == EVT_UNKNOWNKEY) {
            /* This case happens with an external number-pad, and also when using 'dead keys'
             * (to compose complex latin characters e.g.), it's not really clear why.
             * Since it's impossible to map a key modifier to an unknown key,
             * it shouldn't harm to clear it. */
            event_state->keymodifier = event.keymodifier = 0;
          }
          break;
        }
      }

      /* It's important `event.modifier` has been initialized first. */
      wm_event_state_update_and_click_set(&event, event_state, (GHOST_TEventType)type);

      /* If test_break set, it catches this. Do not set with modifier presses.
       * Exclude modifiers because MS-Windows uses these to bring up the task manager.
       *
       * NOTE: in general handling events here isn't great design as
       * event handling should be managed by the event handling loop.
       * Make an exception for `G.is_break` as it ensures we can always cancel operations
       * such as rendering or baking no matter which operation is currently handling events. */
      if ((event.type == EVT_ESCKEY) && (event.val == KM_PRESS) && (event.modifier == 0)) {
        G.is_break = true;
      }

      if (!wm_event_is_ignorable_key_press(win, event)) {
        wm_event_add(win, &event);
      }

      break;
    }

    case GHOST_kEventWheel: {
      GHOST_TEventWheelData *wheelData = static_cast<GHOST_TEventWheelData *>(customdata);

      if (wheelData->z > 0) {
        event.type = WHEELUPMOUSE;
      }
      else {
        event.type = WHEELDOWNMOUSE;
      }

      event.val = KM_PRESS;
      wm_event_add(win, &event);

      break;
    }
    case GHOST_kEventTimer: {
      event.type = TIMER;
      event.custom = EVT_DATA_TIMER;
      event.customdata = customdata;
      event.val = KM_NOTHING;
      event.keymodifier = 0;
      wm_event_add(win, &event);

      break;
    }

#ifdef WITH_INPUT_NDOF
    case GHOST_kEventNDOFMotion: {
      event.type = NDOF_MOTION;
      event.val = KM_NOTHING;
      attach_ndof_data(&event, static_cast<const GHOST_TEventNDOFMotionData *>(customdata));
      wm_event_add(win, &event);

      CLOG_INFO(WM_LOG_HANDLERS, 1, "sending NDOF_MOTION, prev = %d %d", event.xy[0], event.xy[1]);
      break;
    }

    case GHOST_kEventNDOFButton: {
      GHOST_TEventNDOFButtonData *e = static_cast<GHOST_TEventNDOFButtonData *>(customdata);

      event.type = NDOF_BUTTON_INDEX_AS_EVENT(e->button);

      switch (e->action) {
        case GHOST_kPress:
          event.val = KM_PRESS;
          break;
        case GHOST_kRelease:
          event.val = KM_RELEASE;
          break;
        default:
          BLI_assert_unreachable();
      }

      event.custom = 0;
      event.customdata = nullptr;

      wm_event_state_update_and_click_set(&event, event_state, (GHOST_TEventType)type);

      wm_event_add(win, &event);

      break;
    }
#endif /* WITH_INPUT_NDOF */

    case GHOST_kEventUnknown:
    case GHOST_kNumEventTypes:
      break;

    case GHOST_kEventWindowDeactivate: {
      event.type = WINDEACTIVATE;
      wm_event_add(win, &event);

      break;
    }

#ifdef WITH_INPUT_IME
    case GHOST_kEventImeCompositionStart: {
      event.val = KM_PRESS;
      win->ime_data = static_cast<wmIMEData *>(customdata);
      win->ime_data->is_ime_composing = true;
      event.type = WM_IME_COMPOSITE_START;
      wm_event_add(win, &event);
      break;
    }
    case GHOST_kEventImeComposition: {
      event.val = KM_PRESS;
      event.type = WM_IME_COMPOSITE_EVENT;
      wm_event_add(win, &event);
      break;
    }
    case GHOST_kEventImeCompositionEnd: {
      event.val = KM_PRESS;
      if (win->ime_data) {
        win->ime_data->is_ime_composing = false;
      }
      event.type = WM_IME_COMPOSITE_END;
      wm_event_add(win, &event);
      break;
    }
#endif /* WITH_INPUT_IME */
  }

#if 0
  WM_event_print(&event);
#endif
}

#ifdef WITH_XR_OPENXR
void wm_event_add_xrevent(wmWindow *win, wmXrActionData *actiondata, short val)
{
  BLI_assert(ELEM(val, KM_PRESS, KM_RELEASE));

  wmEvent event{};
  event.type = EVT_XR_ACTION;
  event.val = val;
  event.flag = (eWM_EventFlag)0;
  event.custom = EVT_DATA_XR;
  event.customdata = actiondata;
  event.customdata_free = true;

  wm_event_add(win, &event);
}
#endif /* WITH_XR_OPENXR */

/** \name WM Interface Locking
 * \{ */

/**
 * Check whether operator is allowed to run in case interface is locked,
 * If interface is unlocked, will always return truth.
 */
static bool wm_operator_check_locked_interface(bContext *C, wmOperatorType *ot)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  if (wm->is_interface_locked) {
    if ((ot->flag & OPTYPE_LOCK_BYPASS) == 0) {
      return false;
    }
  }

  return true;
}

void WM_set_locked_interface(wmWindowManager *wm, bool lock)
{

  /* This will prevent events from being handled while interface is locked
   *
   * Use a "local" flag for now, because currently no other areas could
   * benefit of locked interface anyway (aka using G.is_interface_locked
   * wouldn't be useful anywhere outside of window manager, so let's not
   * pollute global context with such an information for now).
   */
  wm->is_interface_locked = lock ? 1 : 0;

  /* This will prevent drawing regions which uses non-thread-safe data.
   * Currently it'll be just a 3D viewport.
   *
   * TODO(sergey): Make it different locked states, so different jobs
   *               could lock different areas of blender and allow
   *               interaction with others?
   */
  BKE_spacedata_draw_locks(lock);
}

void WM_event_get_keymaps_from_handler(wmWindowManager *wm,
                                       wmWindow *win,
                                       wmEventHandler_Keymap *handler,
                                       wmEventHandler_KeymapResult *km_result)
{
  if (handler->dynamic.keymap_fn != nullptr) {
    handler->dynamic.keymap_fn(wm, win, handler, km_result);
    BLI_assert(handler->keymap == nullptr);
  }
  else {
    memset(km_result, 0x0, sizeof(*km_result));
    wmKeyMap *keymap = WM_keymap_active(wm, handler->keymap);
    BLI_assert(keymap != nullptr);
    if (keymap != nullptr) {
      km_result->keymaps[km_result->keymaps_len++] = keymap;
    }
  }
}

wmKeyMapItem *WM_event_match_keymap_item(bContext *C, wmKeyMap *keymap, const wmEvent *event)
{
  LISTBASE_FOREACH (wmKeyMapItem *, kmi, &keymap->items) {
    if (wm_eventmatch(event, kmi)) {
      wmOperatorType *ot = WM_operatortype_find(kmi->idname, false);
      if (WM_operator_poll_context(C, ot, WM_OP_INVOKE_DEFAULT)) {
        return kmi;
      }
    }
  }
  return nullptr;
}

wmKeyMapItem *WM_event_match_keymap_item_from_handlers(
    bContext *C, wmWindowManager *wm, wmWindow *win, ListBase *handlers, const wmEvent *event)
{
  LISTBASE_FOREACH (wmEventHandler *, handler_base, handlers) {
    /* During this loop, UI handlers for nested menus can tag multiple handlers free. */
    if (handler_base->flag & WM_HANDLER_DO_FREE) {
      /* Pass. */
    }
    else if (handler_base->poll == nullptr || handler_base->poll(CTX_wm_region(C), event)) {
      if (handler_base->type == WM_HANDLER_TYPE_KEYMAP) {
        wmEventHandler_Keymap *handler = (wmEventHandler_Keymap *)handler_base;
        wmEventHandler_KeymapResult km_result;
        WM_event_get_keymaps_from_handler(wm, win, handler, &km_result);
        for (int km_index = 0; km_index < km_result.keymaps_len; km_index++) {
          wmKeyMap *keymap = km_result.keymaps[km_index];
          if (WM_keymap_poll(C, keymap)) {
            wmKeyMapItem *kmi = WM_event_match_keymap_item(C, keymap, event);
            if (kmi != nullptr) {
              return kmi;
            }
          }
        }
      }
    }
  }
  return nullptr;
}

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
