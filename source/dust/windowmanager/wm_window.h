#pragma once

struct wmOperator;

#ifdef __cplusplus
extern "C" {
#endif

/* *************** internal api ************** */
void wm_ghost_init(bContext *C);
void wm_ghost_exit(void);

void wm_get_screensize(int *r_width, int *r_height);
void wm_get_desktopsize(int *r_width, int *r_height);

wmWindow *wm_window_new(const struct Main *bmain,
                        wmWindowManager *wm,
                        wmWindow *parent,
                        bool dialog);
wmWindow *wm_window_copy(struct Main *bmain,
                         wmWindowManager *wm,
                         wmWindow *win_src,
                         const bool duplicate_layout,
                         const bool child);
wmWindow *wm_window_copy_test(bContext *C,
                              wmWindow *win_src,
                              const bool duplicate_layout,
                              const bool child);
void wm_window_free(bContext *C, wmWindowManager *wm, wmWindow *win);
void wm_window_close(bContext *C, wmWindowManager *wm, wmWindow *win);

void wm_window_title(wmWindowManager *wm, wmWindow *win);
void wm_window_ghostwindows_ensure(wmWindowManager *wm);
void wm_window_ghostwindows_remove_invalid(bContext *C, wmWindowManager *wm);
void wm_window_process_events(const bContext *C);

void wm_window_clear_drawable(wmWindowManager *wm);
void wm_window_make_drawable(wmWindowManager *wm, wmWindow *win);
void wm_window_reset_drawable(void);

void wm_window_raise(wmWindow *win);
void wm_window_lower(wmWindow *win);
void wm_window_set_size(wmWindow *win, int width, int height);
void wm_window_get_position(wmWindow *win, int *r_pos_x, int *r_pos_y);
void wm_window_swap_buffers(wmWindow *win);
void wm_window_set_swap_interval(wmWindow *win, int interval);
bool wm_window_get_swap_interval(wmWindow *win, int *intervalOut);

void wm_get_cursor_position(wmWindow *win, int *x, int *y);
void wm_cursor_position_from_ghost(wmWindow *win, int *x, int *y);
void wm_cursor_position_to_ghost(wmWindow *win, int *x, int *y);

#ifdef WITH_INPUT_IME
void wm_window_IME_begin(wmWindow *win, int x, int y, int w, int h, bool complete);
void wm_window_IME_end(wmWindow *win);
#endif

/* *************** window operators ************** */
int wm_window_close_exec(bContext *C, struct wmOperator *op);
int wm_window_fullscreen_toggle_exec(bContext *C, struct wmOperator *op);
void wm_quit_with_optional_confirmation_prompt(bContext *C, wmWindow *win) ATTR_NONNULL();

int wm_window_new_exec(bContext *C, struct wmOperator *op);
int wm_window_new_main_exec(bContext *C, struct wmOperator *op);

void wm_test_autorun_warning(bContext *C);

#ifdef __cplusplus
}
#endif
