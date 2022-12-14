/*
 * Container to manage painting in an offscreen context.
 */

#pragma once

struct bContext;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wmSurface {
  struct wmSurface *next, *prev;

  GHOST_ContextHandle ghost_ctx;
  struct GPUContext *gpu_ctx;

  void *customdata;

  void (*draw)(struct bContext *);
  /** Free customdata, not the surface itself (done by wm_surface API) */
  void (*free_data)(struct wmSurface *);

  /** Called  when surface is activated for drawing (made drawable). */
  void (*activate)(void);
  /** Called  when surface is deactivated for drawing (current drawable cleared). */
  void (*deactivate)(void);
} wmSurface;

/* Create/Free */
void wm_surface_add(wmSurface *surface);
void wm_surface_remove(wmSurface *surface);
void wm_surfaces_free(void);

/* Utils */
void wm_surfaces_iter(struct bContext *C, void (*cb)(struct bContext *, wmSurface *));

/* Drawing */
void wm_surface_make_drawable(wmSurface *surface);
void wm_surface_clear_drawable(void);
void wm_surface_set_drawable(wmSurface *surface, bool activate);
void wm_surface_reset_drawable(void);

#ifdef __cplusplus
}
#endif
