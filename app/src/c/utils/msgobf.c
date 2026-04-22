#include "msgobf.h"

#include "keykey.h"
#include "utils_common.h"

static void fill_keystream(uint8_t *buffer, size_t length) {
  uint32_t state = MSGOBF_SEED;

  for (size_t i = 0; i < length; i++) {
    state = (state * MSGOBF_MULT) + MSGOBF_INC;
    buffer[i] = (uint8_t)((state >> 24) & 0xFF);
  }
}

void msgobf_send_keystream(void) {
  uint8_t keystream[MSGOBF_KEYSTREAM_LEN];
  fill_keystream(keystream, MSGOBF_KEYSTREAM_LEN);

  DictionaryIterator *iter = NULL;
  AppMessageResult begin_result = app_message_outbox_begin(&iter);
  if (begin_result != APP_MSG_OK || !iter) {
    UTIL_LOG(APP_LOG_LEVEL_ERROR, "Outbox begin failed: %d", (int)begin_result);
    return;
  }

  DictionaryResult write_result = dict_write_data(
      iter, MESSAGE_KEY_REQUEST_DATA, keystream, MSGOBF_KEYSTREAM_LEN);
  if (write_result != DICT_OK) {
    UTIL_LOG(APP_LOG_LEVEL_ERROR, "Keystream write failed: %d", (int)write_result);
    return;
  }

  AppMessageResult send_result = app_message_outbox_send();
  if (send_result != APP_MSG_OK) {
    UTIL_LOG(APP_LOG_LEVEL_ERROR, "Keystream send failed: %d", (int)send_result);
  }
}
