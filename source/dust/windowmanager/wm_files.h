#pragma once

struct Main;
struct wmGenericCallback;
struct wmOperatorType;

#ifdef __cplusplus
extern "C" {
#endif

/* wm_files.c */
void wm_history_file_read(void);
void wm_homefile_read(struct bContext *C,
                      struct ReportList *reports,
                      bool use_factory_settings,
                      bool use_empty_data,
                      bool use_data,
                      bool use_userdef,
                      const char *filepath_startup_override,
                      const char *app_template_override,
                      bool *r_is_factory_startup);
void wm_file_read_report(bContext *C, struct Main *bmain);

void wm_close_file_dialog(bContext *C, struct wmGenericCallback *post_action);
bool wm_file_or_image_is_modified(const Main *bmain, const wmWindowManager *wm);

void WM_OT_save_homefile(struct wmOperatorType *ot);
void WM_OT_save_userpref(struct wmOperatorType *ot);
void WM_OT_read_userpref(struct wmOperatorType *ot);
void WM_OT_read_factory_userpref(struct wmOperatorType *ot);
void WM_OT_read_history(struct wmOperatorType *ot);
void WM_OT_read_homefile(struct wmOperatorType *ot);
void WM_OT_read_factory_settings(struct wmOperatorType *ot);

void WM_OT_open_mainfile(struct wmOperatorType *ot);

void WM_OT_revert_mainfile(struct wmOperatorType *ot);
void WM_OT_recover_last_session(struct wmOperatorType *ot);
void WM_OT_recover_auto_save(struct wmOperatorType *ot);

void WM_OT_save_as_mainfile(struct wmOperatorType *ot);
void WM_OT_save_mainfile(struct wmOperatorType *ot);

/* wm_files_link.c */
void WM_OT_link(struct wmOperatorType *ot);
void WM_OT_append(struct wmOperatorType *ot);

void WM_OT_lib_relocate(struct wmOperatorType *ot);
void WM_OT_lib_reload(struct wmOperatorType *ot);

#ifdef __cplusplus
}
#endif
