// --- LIBRARIES ---
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include "esp_camera.h"

// --- CAMERA CONFIGURATION ---
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

// --- WIFI ---
const char* ssid = "iP_Salva";
const char* password = "kamubaik";

WebServer server(80);

// --- SERVO ---
Servo servoPan;
Servo servoTilt;
int posPan = 90;
int posTilt = 90;

// --- L298N MOTOR DRIVER ---
#define IN1 14
#define IN2 15
#define IN3 13
#define IN4 12

// --- FLASH LED ---
#define FLASH_LED_PIN 4

// --- HTML PAGE ---
String htmlPage = "";

// --- CAMERA SETUP ---
void startCamera() {
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if(psramFound()){
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
}

// --- STREAMING ---
void handleJPGStream() {
  WiFiClient client = server.client();
  String response = "";
  response += "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  while (1) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    response = "--frame\r\n";
    response += "Content-Type: image/jpeg\r\n\r\n";
    server.sendContent(response);
    server.sendContent((const char *)fb->buf, fb->len);
    server.sendContent("\r\n");
    esp_camera_fb_return(fb);

    if (!client.connected()) break;
    delay(100);
  }
}

// --- HANDLE CONTROL ---
void handleControl() {
  String action = server.arg("action");

  if (action == "forward") {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  } else if (action == "backward") {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
  } else if (action == "left") {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  } else if (action == "right") {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
  } else if (action == "stop") {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
  } else if (action == "flash_on") {
    digitalWrite(FLASH_LED_PIN, HIGH);
  } else if (action == "flash_off") {
    digitalWrite(FLASH_LED_PIN, LOW);
  } else if (action == "set_pan") {
    int value = server.arg("value").toInt();
    posPan = constrain(value, 0, 180);
    servoPan.write(posPan);
  } else if (action == "set_tilt") {
    int value = server.arg("value").toInt();
    posTilt = constrain(value, 0, 180);
    servoTilt.write(posTilt);
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

// --- HANDLE HTML ---
void handleRoot() {
  htmlPage = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <title>ESP32-CAM Robot</title>
    <style>
      body {
        font-family: Arial, sans-serif;
        text-align: center;
        background-color: #f0f0f0;
      }
      h1 {
        margin-top: 10px;
      }
      .controls, .sliders {
        margin: 10px auto;
        display: flex;
        flex-wrap: wrap;
        justify-content: center;
        gap: 10px;
      }
      button {
        padding: 10px 20px;
        font-size: 16px;
        background-color: #4CAF50;
        color: white;
        border: none;
        border-radius: 8px;
        cursor: pointer;
      }
      button:hover {
        background-color: #45a049;
      }
      input[type=range] {
        width: 200px;
      }
    </style>
  </head>
  <body>
    <h1>ESP32-CAM Robot</h1>
    <img src='/stream' style='width:320px;'><br><br>

    <div class="controls">
      <form action='/control'><button name='action' value='forward'>↑ Forward</button></form>
      <form action='/control'><button name='action' value='left'>← Left</button></form>
      <form action='/control'><button name='action' value='stop'>⏹ Stop</button></form>
      <form action='/control'><button name='action' value='right'>→ Right</button></form>
      <form action='/control'><button name='action' value='backward'>↓ Backward</button></form>
    </div>

    <div class="controls">
      <form action='/control'><button name='action' value='flash_on'>💡 Flash ON</button></form>
      <form action='/control'><button name='action' value='flash_off'>💡 Flash OFF</button></form>
    </div>

    <div class="sliders">
      <div>
        <label for='pan'>Pan</label><br>
        <input type='range' min='0' max='180' value='90' id='pan' onchange='updateServo("pan", this.value)'>
      </div>
      <div>
        <label for='tilt'>Tilt</label><br>
        <input type='range' min='0' max='180' value='90' id='tilt' onchange='updateServo("tilt", this.value)'>
      </div>
    </div>

    <script>
      function updateServo(axis, value) {
        fetch(`/control?action=set_` + axis + `&value=` + value);
      }
    </script>
  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", htmlPage);
}


// --- SETUP ---
void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(FLASH_LED_PIN, OUTPUT);

  servoPan.attach(2);
  servoTilt.attach(0);
  servoPan.write(posPan);
  servoTilt.write(posTilt);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());

  startCamera();

  server.on("/", handleRoot);
  server.on("/stream", HTTP_GET, handleJPGStream);
  server.on("/control", handleControl);
  server.begin();
}

// --- LOOP ---
void loop() {
  server.handleClient();
}
