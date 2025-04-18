#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// WiFi credentials
const char* ssid = "iP_Salva";
const char* password = "kamubaik";

// Motor pins
#define IN1 2
#define IN2 14
#define IN3 15
#define IN4 13

// Camera pins for AI-Thinker
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Streaming definitions
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;

void testMotors() {
  // Maju
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  delay(2000);
  
  // Stop
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  delay(1000);
  
  // Mundur
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  delay(2000);
  
  // Stop
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  
  // Initialize motor pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  
  // testMotors ();

  // // Stop motors initially
  // digitalWrite(IN1, LOW);
  // digitalWrite(IN2, LOW);
  // digitalWrite(IN3, LOW);
  // digitalWrite(IN4, LOW);

  // Camera configuration
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count = 2;

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("Stream URL: http://");
  Serial.println(WiFi.localIP());

  startCameraServer();
}

void loop() {
  delay(1);
}

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char *part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if (fb->format != PIXFORMAT_JPEG) {
        bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
        esp_camera_fb_return(fb);
        fb = NULL;
        if (!jpeg_converted) {
          Serial.println("JPEG compression failed");
          res = ESP_FAIL;
        }
      } else {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
      }
    }

    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }

    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }

    if (res != ESP_OK) {
      break;
    }
  }

  return res;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port = 80;
  config.max_open_sockets = 3;

  httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = [](httpd_req_t *req) {
      httpd_resp_set_type(req, "text/html");
      const char* html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32-CAM Robot</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin: 0 auto; padding: 20px; }
    button { padding: 12px 24px; margin: 5px; font-size: 18px; border: none; border-radius: 5px; }
    #forward { background-color: #4CAF50; color: white; }
    #left, #right { background-color: #2196F3; color: white; }
    #stop { background-color: #f44336; color: white; }
    #back { background-color: #ff9800; color: white; }
    #stream { max-width: 100%; height: auto; border: 1px solid #ddd; border-radius: 4px; padding: 5px; }
  </style>
</head>
<body>
  <h1>ESP32-CAM Robot Control</h1>
  <img id="stream" src="/stream">
  <div>
    <button id="forward" ontouchstart="fetch('/control?cmd=forward')" ontouchend="fetch('/control?cmd=stop')" onclick="fetch('/control?cmd=forward')">Maju</button>
  </div>
  <div>
    <button id="left" ontouchstart="fetch('/control?cmd=left')" ontouchend="fetch('/control?cmd=stop')" onclick="fetch('/control?cmd=left')">Kiri</button>
    <button id="stop" onclick="fetch('/control?cmd=stop')">Stop</button>
    <button id="right" ontouchstart="fetch('/control?cmd=right')" ontouchend="fetch('/control?cmd=stop')" onclick="fetch('/control?cmd=right')">Kanan</button>
  </div>
  <div>
    <button id="back" ontouchstart="fetch('/control?cmd=back')" ontouchend="fetch('/control?cmd=stop')" onclick="fetch('/control?cmd=back')">Mundur</button>

  </div>
  <script>
    // Auto-reconnect stream if disconnected
    const img = document.getElementById('stream');
    function refreshStream() {
      img.src = '/stream?' + Date.now();
      img.onerror = function() { setTimeout(refreshStream, 1000); }
    }
    img.onerror = function() { setTimeout(refreshStream, 1000); }
  </script>
</body>
</html>
)rawliteral";
      return httpd_resp_send(req, html, strlen(html));
    },
    .user_ctx = NULL
  };

  httpd_uri_t cmd_uri = {
    .uri = "/control",
    .method = HTTP_GET,
    .handler = [](httpd_req_t *req) {
      char* buf;
      size_t buf_len;
      char variable[32] = {0,};
      
      buf_len = httpd_req_get_url_query_len(req) + 1;
      if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if (!buf) {
          httpd_resp_send_500(req);
          return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
          if (httpd_query_key_value(buf, "cmd", variable, sizeof(variable)) == ESP_OK) {
          } else {
            free(buf);
            httpd_resp_send_404(req);
            return ESP_FAIL;
          }
        } else {
          free(buf);
          httpd_resp_send_404(req);
          return ESP_FAIL;
        }
        free(buf);
      } else {
        httpd_resp_send_404(req);
        return ESP_FAIL;
      }

      // Handle motor control
      String cmd = String(variable);
      Serial.println("Command: " + cmd);

      analogWrite (ENA, 200);
      analogWrite (ENB, 200);

      if (cmd == "forward") {
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        digitalWrite(IN3, HIGH);
        digitalWrite(IN4, LOW);
      } else if (cmd == "back") {
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, HIGH);
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, HIGH);
      } else if (cmd == "left") {
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, HIGH);
        digitalWrite(IN3, HIGH);
        digitalWrite(IN4, LOW);
      } else if (cmd == "right") {
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, HIGH);
      } else if (cmd == "stop") {
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, LOW);
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, LOW);
      }
      
      httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
      return ESP_OK;
    },
    .user_ctx = NULL
  };

  httpd_uri_t stream_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
  };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &index_uri);
    httpd_register_uri_handler(stream_httpd, &cmd_uri);
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

  }
}