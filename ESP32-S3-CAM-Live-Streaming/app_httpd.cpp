#include <Arduino.h>
#include "esp_http_server.h"
#include "esp_camera.h"

// ============================================================
// SERVER
// ============================================================

httpd_handle_t camera_httpd = NULL;

// ============================================================
// MJPEG STREAM
// ============================================================

#define PART_BOUNDARY "123456789000000000000987654321"

static const char *STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;

static const char *STREAM_BOUNDARY =
    "\r\n--" PART_BOUNDARY "\r\n";

static const char *STREAM_PART =
    "Content-Type: image/jpeg\r\n"
    "Content-Length: %u\r\n"
    "\r\n";

// ============================================================
// SIMPLE HTML PAGE
// ============================================================

static const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>

<meta charset="UTF-8">

<meta name="viewport"
      content="width=device-width, initial-scale=1.0">

<title>ESP32-S3-EYE Camera</title>

<style>

html, body {
    margin: 0;
    padding: 0;
    width: 100%;
    height: 100%;
    background: #000;
    font-family: Arial, sans-serif;
}

body {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
}

h1 {
    color: white;
    font-size: 22px;
    margin: 0 0 15px 0;
}

.camera-container {
    width: 95%;
    max-width: 800px;
}

#camera {
    display: block;
    width: 100%;
    height: auto;
    border-radius: 8px;
    background: #111;
}

.live {
    color: #00ff66;
    margin-top: 10px;
    font-size: 14px;
}

</style>

</head>

<body>

<h1>ESP32-S3-EYE LIVE CAMERA</h1>

<div class="camera-container">

    <img
        id="camera"
        src="/stream"
        alt="Live Camera"
    >

</div>

<div class="live">
    ● LIVE
</div>

</body>
</html>
)rawliteral";

// ============================================================
// HOME PAGE
// ============================================================

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(
        req,
        "text/html"
    );

    httpd_resp_set_hdr(
        req,
        "Cache-Control",
        "no-cache"
    );

    httpd_resp_set_hdr(
        req,
        "Access-Control-Allow-Origin",
        "*"
    );

    return httpd_resp_send(
        req,
        INDEX_HTML,
        strlen(INDEX_HTML)
    );
}

// ============================================================
// STREAM HANDLER
// ============================================================

static esp_err_t stream_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;

    esp_err_t res = ESP_OK;

    size_t jpg_len = 0;

    uint8_t *jpg_buf = NULL;

    // --------------------------------------------------------
    // Tell browser this is an MJPEG stream
    // --------------------------------------------------------

    res = httpd_resp_set_type(
        req,
        STREAM_CONTENT_TYPE
    );

    if (res != ESP_OK)
    {
        return res;
    }

    httpd_resp_set_hdr(
        req,
        "Access-Control-Allow-Origin",
        "*"
    );

    httpd_resp_set_hdr(
        req,
        "Cache-Control",
        "no-cache"
    );

    httpd_resp_set_hdr(
        req,
        "Pragma",
        "no-cache"
    );

    // --------------------------------------------------------
    // CONTINUOUS STREAM
    // --------------------------------------------------------

    while (true)
    {
        // Get newest camera frame
        fb = esp_camera_fb_get();

        if (!fb)
        {
            Serial.println(
                "Camera capture failed"
            );

            res = ESP_FAIL;
            break;
        }

        // ----------------------------------------------------
        // Camera should already produce JPEG
        // ----------------------------------------------------

        if (fb->format != PIXFORMAT_JPEG)
        {
            Serial.println(
                "Camera frame is not JPEG"
            );

            esp_camera_fb_return(fb);

            fb = NULL;

            res = ESP_FAIL;

            break;
        }

        jpg_buf = fb->buf;
        jpg_len = fb->len;

        // ----------------------------------------------------
        // SEND BOUNDARY
        // ----------------------------------------------------

        res = httpd_resp_send_chunk(
            req,
            STREAM_BOUNDARY,
            strlen(STREAM_BOUNDARY)
        );

        if (res != ESP_OK)
        {
            esp_camera_fb_return(fb);
            fb = NULL;
            break;
        }

        // ----------------------------------------------------
        // SEND JPEG HEADER
        // ----------------------------------------------------

        char header[64];

        int header_len = snprintf(
            header,
            sizeof(header),
            STREAM_PART,
            (unsigned int)jpg_len
        );

        res = httpd_resp_send_chunk(
            req,
            header,
            header_len
        );

        if (res != ESP_OK)
        {
            esp_camera_fb_return(fb);
            fb = NULL;
            break;
        }

        // ----------------------------------------------------
        // SEND JPEG IMAGE
        // ----------------------------------------------------

        res = httpd_resp_send_chunk(
            req,
            (const char *)jpg_buf,
            jpg_len
        );

        // ----------------------------------------------------
        // RETURN CAMERA BUFFER
        // ----------------------------------------------------

        esp_camera_fb_return(fb);

        fb = NULL;
        jpg_buf = NULL;

        // ----------------------------------------------------
        // CLIENT DISCONNECTED
        // ----------------------------------------------------

        if (res != ESP_OK)
        {
            Serial.println(
                "Stream client disconnected"
            );

            break;
        }

        // Small delay
        vTaskDelay(
            5 / portTICK_PERIOD_MS
        );
    }

    return res;
}

// ============================================================
// START CAMERA SERVER
// ============================================================

void startCameraServer()
{
    httpd_config_t config =
        HTTPD_DEFAULT_CONFIG();

    // ========================================================
    // PORT 80
    // ========================================================

    config.server_port = 80;

    config.ctrl_port = 32768;

    // ========================================================
    // CONNECTION SETTINGS
    // ========================================================

    config.max_open_sockets = 4;

    config.max_uri_handlers = 4;

    config.stack_size = 8192;

    // ========================================================
    // HOME PAGE
    // ========================================================

    httpd_uri_t index_uri =
    {
        .uri = "/",

        .method = HTTP_GET,

        .handler = index_handler,

        .user_ctx = NULL
    };

    // ========================================================
    // STREAM
    // ========================================================

    httpd_uri_t stream_uri =
    {
        .uri = "/stream",

        .method = HTTP_GET,

        .handler = stream_handler,

        .user_ctx = NULL
    };

    // ========================================================
    // START SERVER
    // ========================================================

    Serial.println(
        "Starting camera server on port 80..."
    );

    esp_err_t err = httpd_start(
        &camera_httpd,
        &config
    );

    if (err == ESP_OK)
    {
        httpd_register_uri_handler(
            camera_httpd,
            &index_uri
        );

        httpd_register_uri_handler(
            camera_httpd,
            &stream_uri
        );

        Serial.println(
            "Camera server started!"
        );
    }
    else
    {
        Serial.printf(
            "Camera server failed: 0x%x\n",
            err
        );
    }
}
