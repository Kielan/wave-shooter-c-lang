#pragma once

#include "../wm_message_bus.h"

struct wmMsgBus {
  struct GSet *messages_gset[WM_MSG_TYPE_NUM];
  /** Messages in order of being added. */
  ListBase messages;
  /** Avoid checking messages when no tags exist. */
  uint messages_tag_count;
};

void wm_msg_subscribe_value_free(struct wmMsgSubscribeKey *msg_key,
                                 struct wmMsgSubscribeValueLink *msg_lnk);

typedef struct wmMsgSubscribeKey_Generic {
  wmMsgSubscribeKey head;
  wmMsg msg;
} wmMsgSubscribeKey_Generic;

LIB_INLINE const wmMsg *wm_msg_subscribe_value_msg_cast(const wmMsgSubscribeKey *key)
{
  return &((wmMsgSubscribeKey_Generic *)key)->msg;
}
LIB_INLINE wmMsg *wm_msg_subscribe_value_msg_cast_mut(wmMsgSubscribeKey *key)
{
  return &((wmMsgSubscribeKey_Generic *)key)->msg;
}
