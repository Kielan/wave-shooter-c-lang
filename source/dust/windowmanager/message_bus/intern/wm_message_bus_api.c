#include <stdio.h>

#include "CLG_log.h"
#include "MEM_guardedalloc.h"

#include "DNA_ID.h"

#include "LIB_ghash.h"
#include "LIB_listbase.h"
#include "LIB_utildefines.h"

#include "WM_message.h"
#include "WM_types.h"
#include "message_bus/intern/wm_message_bus_intern.h"

#include "API_access.h"

/* -------------------------------------------------------------------------- */

LIB_INLINE uint void_hash_uint(const void *key)
{
  size_t y = (size_t)key >> (sizeof(void *));
  return (unsigned int)y;
}

static uint wm_msg_api_gset_hash(const void *key_p)
{
  const wmMsgSubscribeKey_RNA *key = key_p;
  const wmMsgParams_API *params = &key->msg.params;
  //  printf("%s\n", API_struct_identifier(params->ptr.type));
  uint k = void_hash_uint(params->ptr.type);
  k ^= void_hash_uint(params->ptr.data);
  k ^= void_hash_uint(params->ptr.owner_id);
  k ^= void_hash_uint(params->prop);
  return k;
}
static bool wm_msg_api_gset_cmp(const void *key_a_p, const void *key_b_p)
{
  const wmMsgParams_API *params_a = &((const wmMsgSubscribeKey_API *)key_a_p)->msg.params;
  const wmMsgParams_API *params_b = &((const wmMsgSubscribeKey_API *)key_b_p)->msg.params;
  return !((params_a->ptr.type == params_b->ptr.type) &&
           (params_a->ptr.owner_id == params_b->ptr.owner_id) &&
           (params_a->ptr.data == params_b->ptr.data) && (params_a->prop == params_b->prop));
}
static void wm_msg_rna_gset_key_free(void *key_p)
{
  wmMsgSubscribeKey_API *key = key_p;
  wmMsgSubscribeValueLink *msg_lnk_next;
  for (wmMsgSubscribeValueLink *msg_lnk = key->head.values.first; msg_lnk;
       msg_lnk = msg_lnk_next) {
    msg_lnk_next = msg_lnk->next;
    wm_msg_subscribe_value_free(&key->head, msg_lnk);
  }
  if (key->msg.params.data_path != NULL) {
    MEM_freeN(key->msg.params.data_path);
  }
  MEM_freeN(key);
}

static void wm_msg_api_repr(FILE *stream, const wmMsgSubscribeKey *msg_key)
{
  const wmMsgSubscribeKey_API *m = (wmMsgSubscribeKey_API *)msg_key;
  const char *none = "<none>";
  fprintf(stream,
          "<wmMsg_API %p, "
          "id='%s', "
          "%s.%s values_len=%d\n",
          m,
          m->msg.head.id,
          m->msg.params.ptr.type ? API_struct_identifier(m->msg.params.ptr.type) : none,
          m->msg.params.prop ? API_property_identifier((PropertyAPI *)m->msg.params.prop) : none,
          BLI_listbase_count(&m->head.values));
}

static void wm_msg_api_update_by_id(struct wmMsgBus *mbus, ID *id_src, ID *id_dst)
{
  GSet *gs = mbus->messages_gset[WM_MSG_TYPE_API];
  GSetIterator gs_iter;
  LIB_gsetIterator_init(&gs_iter, gs);
  while (LIB_gsetIterator_done(&gs_iter) == false) {
    wmMsgSubscribeKey_API *key = LIB_gsetIterator_getKey(&gs_iter);
    LIB_gsetIterator_step(&gs_iter);
    if (key->msg.params.ptr.owner_id == id_src) {

      /* GSet always needs updating since the key changes. */
      LIB_gset_remove(gs, key, NULL);

      /* Remove any non-persistent values, so a single persistent
       * value doesn't modify behavior for the rest. */
      for (wmMsgSubscribeValueLink *msg_lnk = key->head.values.first, *msg_lnk_next; msg_lnk;
           msg_lnk = msg_lnk_next) {
        msg_lnk_next = msg_lnk->next;
        if (msg_lnk->params.is_persistent == false) {
          if (msg_lnk->params.tag) {
            mbus->messages_tag_count -= 1;
          }
          wm_msg_subscribe_value_free(&key->head, msg_lnk);
        }
      }

      bool remove = true;

      if (LIB_listbase_is_empty(&key->head.values)) {
        /* Remove, no reason to keep. */
      }
      else if (key->msg.params.ptr.data == key->msg.params.ptr.owner_id) {
        /* Simple, just update the ID. */
        key->msg.params.ptr.data = id_dst;
        key->msg.params.ptr.owner_id = id_dst;
        remove = false;
      }
      else {
        /* We need to resolve this from the new ID pointer. */
        PointerAPI idptr;
        API_id_pointer_create(id_dst, &idptr);
        PointerAPI ptr;
        PropertyAPI *prop = NULL;
        if (RNA_path_resolve(&idptr, key->msg.params.data_path, &ptr, &prop) &&
            (prop == NULL) == (key->msg.params.prop == NULL)) {
          key->msg.params.ptr = ptr;
          key->msg.params.prop = prop;
          remove = false;
        }
      }

      if (remove) {
        for (wmMsgSubscribeValueLink *msg_lnk = key->head.values.first, *msg_lnk_next; msg_lnk;
             msg_lnk = msg_lnk_next) {
          msg_lnk_next = msg_lnk->next;
          if (msg_lnk->params.is_persistent == false) {
            if (msg_lnk->params.tag) {
              mbus->messages_tag_count -= 1;
            }
            wm_msg_subscribe_value_free(&key->head, msg_lnk);
          }
        }
        /* Failed to persist, remove the key. */
        LIB_remlink(&mbus->messages, key);
        wm_msg_api_gset_key_free(key);
      }
      else {
        /* Note that it's not impossible this key exists, however it is very unlikely
         * since a subscriber would need to register in the middle of an undo for eg.
         * so assert for now. */
        LIB_assert(!LIB_gset_haskey(gs, key));
        LIB_gset_add(gs, key);
      }
    }
  }
}

static void wm_msg_api_remove_by_id(struct wmMsgBus *mbus, const ID *id)
{
  GSet *gs = mbus->messages_gset[WM_MSG_TYPE_API];
  GSetIterator gs_iter;
  LIB_gsetIterator_init(&gs_iter, gs);
  while (LIB_gsetIterator_done(&gs_iter) == false) {
    wmMsgSubscribeKey_API *key = LIB_gsetIterator_getKey(&gs_iter);
    BLI_gsetIterator_step(&gs_iter);
    if (key->msg.params.ptr.owner_id == id) {
      /* Clear here so we can decrement 'messages_tag_count'. */
      for (wmMsgSubscribeValueLink *msg_lnk = key->head.values.first, *msg_lnk_next; msg_lnk;
           msg_lnk = msg_lnk_next) {
        msg_lnk_next = msg_lnk->next;
        if (msg_lnk->params.tag) {
          mbus->messages_tag_count -= 1;
        }
        wm_msg_subscribe_value_free(&key->head, msg_lnk);
      }

      LIB_remlink(&mbus->messages, key);
      LIB_gset_remove(gs, key, NULL);
      wm_msg_api_gset_key_free(key);
    }
  }
}

void WM_msgtypeinfo_init_api(wmMsgTypeInfo *msgtype_info)
{
  msgtype_info->gset.hash_fn = wm_msg_rna_gset_hash;
  msgtype_info->gset.cmp_fn = wm_msg_rna_gset_cmp;
  msgtype_info->gset.key_free_fn = wm_msg_rna_gset_key_free;

  msgtype_info->repr = wm_msg_rna_repr;
  msgtype_info->update_by_id = wm_msg_rna_update_by_id;
  msgtype_info->remove_by_id = wm_msg_rna_remove_by_id;

  msgtype_info->msg_key_size = sizeof(wmMsgSubscribeKey_API);
}

/* -------------------------------------------------------------------------- */

wmMsgSubscribeKey_API *WM_msg_lookup_api(struct wmMsgBus *mbus,
                                         const wmMsgParams_API *msg_key_params)
{
  wmMsgSubscribeKey_API key_test;
  key_test.msg.params = *msg_key_params;
  return LIB_gset_lookup(mbus->messages_gset[WM_MSG_TYPE_API], &key_test);
}

void WM_msg_publish_rna_params(struct wmMsgBus *mbus, const wmMsgParams_API *msg_key_params)
{
  wmMsgSubscribeKey_API *key;

  const char *none = "<none>";
  CLOG_INFO(WM_LOG_MSGBUS_PUB,
            2,
            "rna(id='%s', %s.%s)",
            msg_key_params->ptr.owner_id ? ((ID *)msg_key_params->ptr.owner_id)->name : none,
            msg_key_params->ptr.type ? API_struct_identifier(msg_key_params->ptr.type) : none,
            msg_key_params->prop ? API_property_identifier((PropertyAPI *)msg_key_params->prop) :
                                   none);

  if ((key = WM_msg_lookup_api(mbus, msg_key_params))) {
    WM_msg_publish_with_key(mbus, &key->head);
  }

  /* Support anonymous subscribers, this may be some extra overhead
   * but we want to be able to be more ambiguous. */
  if (msg_key_params->ptr.owner_id || msg_key_params->ptr.data) {
    wmMsgParams_API msg_key_params_anon = *msg_key_params;

    /* We might want to enable this later? */
    if (msg_key_params_anon.prop != NULL) {
      /* All properties for this type. */
      msg_key_params_anon.prop = NULL;
      if ((key = WM_msg_lookup_api(mbus, &msg_key_params_anon))) {
        WM_msg_publish_with_key(mbus, &key->head);
      }
      msg_key_params_anon.prop = msg_key_params->prop;
    }

    msg_key_params_anon.ptr.owner_id = NULL;
    msg_key_params_anon.ptr.data = NULL;
    if ((key = WM_msg_lookup_rna(mbus, &msg_key_params_anon))) {
      WM_msg_publish_with_key(mbus, &key->head);
    }

    /* Support subscribers to a type. */
    if (msg_key_params->prop) {
      msg_key_params_anon.prop = NULL;
      if ((key = WM_msg_lookup_rna(mbus, &msg_key_params_anon))) {
        WM_msg_publish_with_key(mbus, &key->head);
      }
    }
  }
}

void WM_msg_publish_api(struct wmMsgBus *mbus, PointerAPI *ptr, PropertyRNA *prop)
{
  WM_msg_publish_rna_params(mbus,
                            &(wmMsgParams_API){
                                .ptr = *ptr,
                                .prop = prop,
                            });
}

void WM_msg_subscribe_api_params(struct wmMsgBus *mbus,
                                 const wmMsgParams_API *msg_key_params,
                                 const wmMsgSubscribeValue *msg_val_params,
                                 const char *id_repr)
{
  wmMsgSubscribeKey_API msg_key_test = {{NULL}};

  /* use when added */
  msg_key_test.msg.head.id = id_repr;
  msg_key_test.msg.head.type = WM_MSG_TYPE_RNA;
  /* for lookup */
  msg_key_test.msg.params = *msg_key_params;

  const char *none = "<none>";
  CLOG_INFO(WM_LOG_MSGBUS_SUB,
            3,
            "rna(id='%s', %s.%s, info='%s')",
            msg_key_params->ptr.owner_id ? ((ID *)msg_key_params->ptr.owner_id)->name : none,
            msg_key_params->ptr.type ? RNA_struct_identifier(msg_key_params->ptr.type) : none,
            msg_key_params->prop ? RNA_property_identifier((PropertyRNA *)msg_key_params->prop) :
                                   none,
            id_repr);

  wmMsgSubscribeKey_API *msg_key = (wmMsgSubscribeKey_API *)WM_msg_subscribe_with_key(
      mbus, &msg_key_test.head, msg_val_params);

  if (msg_val_params->is_persistent) {
    if (msg_key->msg.params.data_path == NULL) {
      if (msg_key->msg.params.ptr.data != msg_key->msg.params.ptr.owner_id) {
        /* We assume prop type can't change. */
        msg_key->msg.params.data_path = API_path_from_ID_to_struct(&msg_key->msg.params.ptr);
      }
    }
  }
}

void WM_msg_subscribe_api(struct wmMsgBus *mbus,
                          PointerAPI *ptr,
                          const PropertyAPI *prop,
                          const wmMsgSubscribeValue *msg_val_params,
                          const char *id_repr)
{
  WM_msg_subscribe_rna_params(mbus,
                              &(const wmMsgParams_API){
                                  .ptr = *ptr,
                                  .prop = prop,
                              },
                              msg_val_params,
                              id_repr);
}

/* -------------------------------------------------------------------------- */
/* ID variants of API
 *
 * note While we could have a separate type for ID's, use API since there is enough overlap.
 * note refactor this */

void WM_msg_subscribe_ID(struct wmMsgBus *mbus,
                         ID *id,
                         const wmMsgSubscribeValue *msg_val_params,
                         const char *id_repr)
{
  wmMsgParams_API msg_key_params = {{NULL}};
  API_id_pointer_create(id, &msg_key_params.ptr);
  WM_msg_subscribe_rna_params(mbus, &msg_key_params, msg_val_params, id_repr);
}

void WM_msg_publish_ID(struct wmMsgBus *mbus, ID *id)
{
  wmMsgParams_API msg_key_params = {{NULL}};
  API_id_pointer_create(id, &msg_key_params.ptr);
  WM_msg_publish_rna_params(mbus, &msg_key_params);
}
