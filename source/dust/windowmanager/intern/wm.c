#include <stddef.h>
#include <string.h>

#include "KERNEL_context.h"

#include "WM_api.h"
#include "wm_draw.h"
#include "wm_event_system.h"

void WM_main(bContext *C)
{
  /* Single refresh before handling events.
   * This ensures we don't run operators before the depsgraph has been evaluated. */
  wm_event_do_refresh_wm_and_depsgraph(C);

  while (1) {

    /* Get events from ghost, handle window events, add to window queues. */
    wm_window_process_events(C);

    /* Per window, all events to the window, screen, area and region handlers. */
    wm_event_do_handlers(C);

    /* Events have left notes about changes, we handle and cache it. */
    wm_event_do_notifiers(C);

    /* Execute cached changes draw. */
    wm_draw_update(C);
  }
}
