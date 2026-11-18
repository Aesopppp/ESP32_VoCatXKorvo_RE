/* 会话级双向静默。时间为单调毫秒低 32 位，差值支持时钟回绕。 */
#ifndef SESSION_POLICY_H
#define SESSION_POLICY_H
#include <stdbool.h>
#include <stdint.h>

#define SESSION_SILENCE_TIMEOUT_MS 60000u

static inline bool session_silence_expired(uint32_t now, uint32_t started,
                                         uint32_t input_voice, uint32_t output_voice)
{
    /* 零表示此方向尚无有效语音。旧会话的时间早于 started，不会延长本会话。 */
    uint32_t quiet_ms = now - started;
    if (input_voice && now - input_voice < quiet_ms) quiet_ms = now - input_voice;
    if (output_voice && now - output_voice < quiet_ms) quiet_ms = now - output_voice;
    return quiet_ms >= SESSION_SILENCE_TIMEOUT_MS;
}
#endif
