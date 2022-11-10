#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct AviCodecData;
struct Collection;
struct Depsgraph;
struct GHash;
struct Main;
struct Object;
struct RenderData;
struct Scene;
struct TransformOrientation;
struct UnitSettings;
struct View3DCursor;
struct ViewLayer;

typedef enum eSceneCopyMethod {
  SCE_COPY_NEW = 0,
  SCE_COPY_EMPTY = 1,
  SCE_COPY_LINK_COLLECTION = 2,
  SCE_COPY_FULL = 3,
} eSceneCopyMethod;

/* Use as the contents of a 'for' loop: for (SETLOOPER(...)) { ... */
#define SETLOOPER(_sce_basis, _sce_iter, _base) \
  _sce_iter = _sce_basis, \
  _base = _setlooper_base_step( \
      &_sce_iter, KERNEL_view_layer_context_active_PLACEHOLDER(_sce_basis), NULL); \
  _base; \
  _base = _setlooper_base_step(&_sce_iter, NULL, _base)

#define SETLOOPER_VIEW_LAYER(_sce_basis, _view_layer, _sce_iter, _base) \
  _sce_iter = _sce_basis, _base = _setlooper_base_step(&_sce_iter, _view_layer, NULL); \
  _base; \
  _base = _setlooper_base_step(&_sce_iter, NULL, _base)

#define SETLOOPER_SET_ONLY(_sce_basis, _sce_iter, _base) \
  _sce_iter = _sce_basis, _base = _setlooper_base_step(&_sce_iter, NULL, NULL); \
  _base; \
  _base = _setlooper_base_step(&_sce_iter, NULL, _base)

struct Base *_setlooper_base_step(struct Scene **sce_iter,
                                  struct ViewLayer *view_layer,
                                  struct Base *base);

void free_avicodecdata(struct AviCodecData *acd);

struct Scene *KERNEL_scene_add(struct Main *bmain, const char *name);

void KERNEL_scene_remove_rigidbody_object(struct Main *bmain,
                                       struct Scene *scene,
                                       struct Object *ob,
                                       const bool free_us);

bool KERNEL_scene_object_find(struct Scene *scene, struct Object *ob);
struct Object *KERNEL_scene_object_find_by_name(const struct Scene *scene, const char *name);

/* Scene base iteration function.
 * Define struct here, so no need to bother with alloc/free it.
 */
typedef struct SceneBaseIter {
  struct ListBase *duplilist;
  struct DupliObject *dupob;
  float omat[4][4];
  struct Object *dupli_refob;
  int phase;
} SceneBaseIter;

int KERNEL_scene_base_iter_next(struct Depsgraph *depsgraph,
                             struct SceneBaseIter *iter,
                             struct Scene **scene,
                             int val,
                             struct Base **base,
                             struct Object **ob);

void KERNEL_scene_base_flag_to_objects(struct ViewLayer *view_layer);
void KERNEL_scene_object_base_flag_sync_from_base(struct Base *base);

void KERNEL_scene_set_background(struct Main *bmain, struct Scene *sce);
struct Scene *KERNEL_scene_set_name(struct Main *bmain, const char *name);

struct ToolSettings *KERNEL_toolsettings_copy(struct ToolSettings *toolsettings, const int flag);
void KERNEL_toolsettings_free(struct ToolSettings *toolsettings);

struct Scene *KERNEL_scene_duplicate(struct Main *bmain, struct Scene *sce, eSceneCopyMethod type);
void KERNEL_scene_groups_relink(struct Scene *sce);

bool KERNEL_scene_has_view_layer(const struct Scene *scene, const struct ViewLayer *layer);
struct Scene *KERNEL_scene_find_from_collection(const struct Main *bmain,
                                             const struct Collection *collection);

#ifdef DURIAN_CAMERA_SWITCH
struct Object *KERNEL_scene_camera_switch_find(struct Scene *scene); /* DURIAN_CAMERA_SWITCH */
#endif
bool KERNEL_scene_camera_switch_update(struct Scene *scene);

const char *KERNEL_scene_find_marker_name(const struct Scene *scene, int frame);
const char *KERNEL_scene_find_last_marker_name(const struct Scene *scene, int frame);

int KERNEL_scene_frame_snap_by_seconds(struct Scene *scene, double interval_in_seconds, int cfra);

/* checks for cycle, returns 1 if it's all OK */
bool KERNEL_scene_validate_setscene(struct Main *bmain, struct Scene *sce);

float KERNEL_scene_frame_get(const struct Scene *scene);
float KERNEL_scene_frame_to_ctime(const struct Scene *scene, const float frame);
void KERNEL_scene_frame_set(struct Scene *scene, double cfra);

struct TransformOrientationSlot *BKE_scene_orientation_slot_get_from_flag(struct Scene *scene,
                                                                          int flag);
struct TransformOrientationSlot *BKE_scene_orientation_slot_get(struct Scene *scene,
                                                                int slot_index);
void KERNEL_scene_orientation_slot_set_index(struct TransformOrientationSlot *orient_slot,
                                          int orientation);
int KERNEL_scene_orientation_slot_get_index(const struct TransformOrientationSlot *orient_slot);

/* **  Scene evaluation ** */

void KERNEL_scene_update_sound(struct Depsgraph *depsgraph, struct Main *bmain);
void KERNEL_scene_update_tag_audio_volume(struct Depsgraph *, struct Scene *scene);

void KERNEL_scene_graph_update_tagged(struct Depsgraph *depsgraph, struct Main *bmain);
void KERNEL_scene_graph_evaluated_ensure(struct Depsgraph *depsgraph, struct Main *bmain);

void KERNEL_scene_graph_update_for_newframe(struct Depsgraph *depsgraph);

void KERNEL_scene_view_layer_graph_evaluated_ensure(struct Main *bmain,
                                                 struct Scene *scene,
                                                 struct ViewLayer *view_layer);

struct SceneRenderView *KERNEL_scene_add_render_view(struct Scene *sce, const char *name);
bool KERNEL_scene_remove_render_view(struct Scene *scene, struct SceneRenderView *srv);

/* render profile */
int get_render_subsurf_level(const struct RenderData *r, int lvl, bool for_render);
int get_render_child_particle_number(const struct RenderData *r, int num, bool for_render);

bool KERNEL_scene_use_shading_nodes_custom(struct Scene *scene);
bool KERNEL_scene_use_spherical_stereo(struct Scene *scene);

bool KERNEL_scene_uses_blender_eevee(const struct Scene *scene);
bool KERNEL_scene_uses_blender_workbench(const struct Scene *scene);
bool KERNEL_scene_uses_cycles(const struct Scene *scene);

void KERNEL_scene_copy_data_eevee(struct Scene *sce_dst, const struct Scene *sce_src);

void KERNEL_scene_disable_color_management(struct Scene *scene);
bool KERNEL_scene_check_color_management_enabled(const struct Scene *scene);
bool KERNEL_scene_check_rigidbody_active(const struct Scene *scene);

int KERNEL_scene_num_threads(const struct Scene *scene);
int KERNEL_render_num_threads(const struct RenderData *r);

int KERNEL_render_preview_pixel_size(const struct RenderData *r);

/**********************************/

double KERNEL_scene_unit_scale(const struct UnitSettings *unit, const int unit_type, double value);

/* multiview */
bool KERNEL_scene_multiview_is_stereo3d(const struct RenderData *rd);
bool KERNEL_scene_multiview_is_render_view_active(const struct RenderData *rd,
                                               const struct SceneRenderView *srv);
bool KERNEL_scene_multiview_is_render_view_first(const struct RenderData *rd, const char *viewname);
bool KERNEL_scene_multiview_is_render_view_last(const struct RenderData *rd, const char *viewname);
int KERNEL_scene_multiview_num_views_get(const struct RenderData *rd);
struct SceneRenderView *KERNEL_scene_multiview_render_view_findindex(const struct RenderData *rd,
                                                                  const int view_id);
const char *KERNEL_scene_multiview_render_view_name_get(const struct RenderData *rd,
                                                     const int view_id);
int KERNEL_scene_multiview_view_id_get(const struct RenderData *rd, const char *viewname);
void KERNEL_scene_multiview_filepath_get(struct SceneRenderView *srv,
                                      const char *filepath,
                                      char *r_filepath);
void KERNEL_scene_multiview_view_filepath_get(const struct RenderData *rd,
                                           const char *filepath,
                                           const char *view,
                                           char *r_filepath);
const char *KERNEL_scene_multiview_view_suffix_get(const struct RenderData *rd, const char *viewname);
const char *KERNEL_scene_multiview_view_id_suffix_get(const struct RenderData *rd, const int view_id);
void KERNEL_scene_multiview_view_prefix_get(struct Scene *scene,
                                         const char *name,
                                         char *r_prefix,
                                         const char **r_ext);
void KERNEL_scene_multiview_videos_dimensions_get(const struct RenderData *rd,
                                               const size_t width,
                                               const size_t height,
                                               size_t *r_width,
                                               size_t *r_height);
int KERNEL_scene_multiview_num_videos_get(const struct RenderData *rd);

/* depsgraph */
void KERNEL_scene_allocate_depsgraph_hash(struct Scene *scene);
void KERNEL_scene_ensure_depsgraph_hash(struct Scene *scene);
void KERNEL_scene_free_depsgraph_hash(struct Scene *scene);
void KERNEL_scene_free_view_layer_depsgraph(struct Scene *scene, struct ViewLayer *view_layer);

/* Do not allocate new depsgraph. */
struct Depsgraph *KERNEL_scene_get_depsgraph(struct Scene *scene, struct ViewLayer *view_layer);
/* Allocate new depsgraph if necessary. */
struct Depsgraph *KERNEL_scene_ensure_depsgraph(struct Main *bmain,
                                             struct Scene *scene,
                                             struct ViewLayer *view_layer);

struct GHash *KERNEL_scene_undo_depsgraphs_extract(struct Main *bmain);
void KERNEL_scene_undo_depsgraphs_restore(struct Main *bmain, struct GHash *depsgraph_extract);

void KERNEL_scene_transform_orientation_remove(struct Scene *scene,
                                            struct TransformOrientation *orientation);
struct TransformOrientation *KE_scene_transform_orientation_find(const struct Scene *scene,
                                                                  const int index);
int KERNEL_scene_transform_orientation_get_index(const struct Scene *scene,
                                              const struct TransformOrientation *orientation);

void KERNEL_scene_cursor_rot_to_mat3(const struct View3DCursor *cursor, float mat[3][3]);
void KERNEL_scene_cursor_mat3_to_rot(struct View3DCursor *cursor,
                                  const float mat[3][3],
                                  bool use_compat);

void KERNEL_scene_cursor_rot_to_quat(const struct View3DCursor *cursor, float quat[4]);
void KERNEL_scene_cursor_quat_to_rot(struct View3DCursor *cursor,
                                  const float quat[4],
                                  bool use_compat);

void KERNEL_scene_cursor_to_mat4(const struct View3DCursor *cursor, float mat[4][4]);
void KERNEL_scene_cursor_from_mat4(struct View3DCursor *cursor,
                                const float mat[4][4],
                                bool use_compat);

/* Dependency graph evaluation. */

/* Evaluate parts of sequences which needs to be done as a part of a dependency graph evaluation.
 * This does NOT include actual rendering of the strips, but rather makes them up-to-date for
 * animation playback and makes them ready for the sequencer's rendering pipeline to render them.
 */
void KERNEL_scene_eval_sequencer_sequences(struct Depsgraph *depsgraph, struct Scene *scene);

#ifdef __cplusplus
}
#endif
