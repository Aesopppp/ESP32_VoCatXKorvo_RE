/**
 * ws_service.h -- Authenticated full-duplex WebSocket audio client.
 *
 *   Responsibilities:
 *     - Open / close WebSocket; main_task owns retry policy.
 *     - WebSocket Ping/Pong keepalive (handled by esp_websocket_client).
 *     - Auth handshake: send `auth` JSON on connect, wait for `__proxy_connected__`.
 *     - Upload: Opus frames -> Base64 -> `input_audio_buffer.append` text frames.
 *     - Download: raw Opus binary frames -> decode -> audio_out_play_opus().
 *     - Server control: authentication and connection errors only.
 *     - Continuous audio: optional speech/response events never close playback.
 *
 *   Thread-safety:
 *     Lifecycle/configuration/auth APIs are serialized by main_task.
 *     Audio upload APIs may run in the existing audio task.
 *     WebSocket events are dispatched via esp_websocket event handler
 *     (runs inside esp_websocket_client internal task) and forwarded to
 *     app_event_queue / event_bus.
 */

#ifndef WS_SERVICE_H
#define WS_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_websocket_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- WebSocket connection state ---------------------------------------- */
typedef enum {
    WS_STATE_DISCONNECTED  = 0,
    WS_STATE_CONNECTING    = 1,  /* TCP/TLS handshaking */
    WS_STATE_AUTHING       = 2,  /* WS opened, auth sent, awaiting __proxy_connected__ */
    WS_STATE_CONNECTED     = 3,  /* auth ok, ready to send/receive audio */
    WS_STATE_ERROR         = 4,
} ws_conn_state_t;

/* ---- Public API -------------------------------------------------------- */
esp_err_t ws_service_init(void);
/* 主任务先 disconnect，再设置本次连接的 URL/token，最后 connect。
 * 非 NULL 输入被整组复制，最长长度见 proto_service.h；不借用 HTTP 快照。
 * 两参数均 NULL 才读取旧 NVS 配置，调用者仅在握手显式关闭时允许这样做。
 * 生命周期调用之间不会自动回退到旧配置，不写入 NVS。 */
esp_err_t ws_service_set_credentials(const char *url, const char *token);
esp_err_t ws_service_connect(void);
esp_err_t ws_service_disconnect(void);
esp_err_t ws_service_deinit(void);

/**
 * @brief Send one Opus frame as a Base64-encoded JSON text frame.
 *
 *   Internally: Opus -> mbedtls_base64_encode -> JSON -> WS text frame.
 *
 * @param opus_data  Opus payload (raw bytes from opus_enc_encode)
 * @param opus_len   payload length in bytes
 * 仅 audio_in 单生产者调用，内部复用固定发送缓冲；并发误用返回 INVALID_STATE。
 * 同步发送，最多等待 1 秒；不持播放锁/采样缓存锁调用。
 * @return ESP_OK on successful send; ESP_ERR_INVALID_STATE if unavailable.
 */
#define WS_OPUS_FRAME_MAX_BYTES 256u /* 与 16 kHz/20 ms 编码输出缓冲一致 */
esp_err_t ws_service_send_opus_frame(const uint8_t *opus_data, size_t opus_len);
/** Main-task-only lifecycle APIs. RX callbacks never wait for audio tasks. */
esp_err_t ws_service_authenticate(void);
uint32_t ws_service_generation(void);

/** 主任务在断连时复制任务上下文，首条 auth 携带 task_ids。
 * 仅影响鉴权字段；连接后的收发和普通会话一致，disconnect 清理上下文。 */
esp_err_t ws_service_prepare_reminder(const char *device_id,
                                      const char *const *task_ids, size_t task_count);

/** Generic JSON text send (for future protocol extensions). */
esp_err_t ws_service_send_json(const char *json_str);

ws_conn_state_t ws_service_get_state(void);
esp_websocket_client_handle_t ws_service_get_client(void);

#ifdef __cplusplus
}
#endif

#endif /* WS_SERVICE_H */
