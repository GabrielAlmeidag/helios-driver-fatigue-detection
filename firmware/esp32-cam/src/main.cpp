#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"

#include "../include/board_config.h"
#include "../include/config.h"

namespace {

constexpr char STREAM_CONTENT_TYPE[] = "multipart/x-mixed-replace;boundary=frame";
constexpr char STREAM_BOUNDARY[] = "\r\n--frame\r\n";

WebServer controlServer(HELIOS_CONTROL_PORT);
WiFiServer streamServer(HELIOS_STREAM_PORT);
WiFiClient streamClient;
uint8_t buzzerPattern = 0;
uint32_t framesSent = 0;
uint32_t streamStartedAt = 0;

bool initializeCamera() {
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
  config.frame_size = HELIOS_FRAME_SIZE;
  config.jpeg_quality = HELIOS_JPEG_QUALITY;
  config.fb_count = psramFound() ? 2 : 1;
  config.grab_mode = psramFound() ? CAMERA_GRAB_LATEST : CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;

  if (esp_camera_init(&config) != ESP_OK) return false;
  esp_camera_sensor_get()->set_framesize(esp_camera_sensor_get(), HELIOS_FRAME_SIZE);
  return true;
}

void setBuzzer(uint8_t pattern) {
  buzzerPattern = pattern;
  if (pattern == 0) {
    ledcWrite(HELIOS_BUZZER_CHANNEL, 0);
    return;
  }
  ledcWriteTone(HELIOS_BUZZER_CHANNEL, pattern == 2 ? 2000 : 880);
  ledcWrite(HELIOS_BUZZER_CHANNEL, pattern == 2 ? 220 : 150);
}

void handleRoot() {
  controlServer.send(200, "text/plain", "HELIOS-CAM online");
}

void handleStatus() {
  const uint32_t elapsed = millis() - streamStartedAt;
  const uint32_t fps = elapsed > 0 ? (framesSent * 1000UL) / elapsed : 0;
  String body = "{\"sistema\":\"HELIOS-CAM\",\"status\":\"online\",\"fps\":" +
                String(fps) + ",\"clientes\":" + String(WiFi.softAPgetStationNum()) +
                ",\"buzzer\":" + String(buzzerPattern) + "}";
  controlServer.send(200, "application/json", body);
}

void handleBuzzer() {
  if (!controlServer.hasArg("padrao")) {
    controlServer.send(400, "text/plain", "parametro padrao obrigatorio");
    return;
  }
  const int pattern = controlServer.arg("padrao").toInt();
  if (pattern < 0 || pattern > 2) {
    controlServer.send(400, "text/plain", "padrao deve ser 0, 1 ou 2");
    return;
  }
  setBuzzer(static_cast<uint8_t>(pattern));
  controlServer.send(204, "text/plain", "");
}

void acceptStreamClient() {
  if (streamClient && streamClient.connected()) return;
  if (streamClient) streamClient.stop();
  streamClient = streamServer.accept();
  if (!streamClient) return;
  streamClient.setNoDelay(true);
  streamClient.print("HTTP/1.1 200 OK\r\nContent-Type: ");
  streamClient.print(STREAM_CONTENT_TYPE);
  streamClient.print("\r\nCache-Control: no-cache\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Connection: close\r\n\r\n");
  streamStartedAt = millis();
  framesSent = 0;
}

void sendStreamFrame() {
  if (!streamClient || !streamClient.connected()) return;
  camera_fb_t* frame = esp_camera_fb_get();
  if (!frame) return;

  char header[128];
  const int headerLength = snprintf(
      header, sizeof(header),
      "%sContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
      STREAM_BOUNDARY, static_cast<unsigned>(frame->len));
  const bool sent =
      streamClient.write(reinterpret_cast<const uint8_t*>(header), headerLength) ==
          static_cast<size_t>(headerLength) &&
      streamClient.write(frame->buf, frame->len) == frame->len;
  esp_camera_fb_return(frame);
  if (sent) {
    ++framesSent;
  } else {
    streamClient.stop();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  pinMode(LED_GPIO_NUM, OUTPUT);
  digitalWrite(LED_GPIO_NUM, LOW);
  ledcSetup(HELIOS_BUZZER_CHANNEL, HELIOS_BUZZER_FREQUENCY_HZ, HELIOS_BUZZER_RESOLUTION);
  ledcAttachPin(HELIOS_BUZZER_GPIO, HELIOS_BUZZER_CHANNEL);
  setBuzzer(0);

  if (!initializeCamera()) {
    Serial.println("Falha ao inicializar a camera.");
    return;
  }

  IPAddress ip(HELIOS_IP);
  IPAddress gateway(HELIOS_GATEWAY);
  IPAddress netmask(HELIOS_NETMASK);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ip, gateway, netmask);
  WiFi.softAP(HELIOS_WIFI_SSID,
              strlen(HELIOS_WIFI_PASSWORD) == 0 ? nullptr : HELIOS_WIFI_PASSWORD,
              HELIOS_WIFI_CHANNEL, false, HELIOS_WIFI_MAX_CLIENTS);

  controlServer.on("/", HTTP_GET, handleRoot);
  controlServer.on("/status", HTTP_GET, handleStatus);
  controlServer.on("/buzzer", HTTP_POST, handleBuzzer);
  controlServer.begin();
  streamServer.begin();

  Serial.print("HELIOS-CAM pronta em http://");
  Serial.println(WiFi.softAPIP());
  Serial.println("Stream: http://192.168.4.1:81/stream");
}

void loop() {
  controlServer.handleClient();
  acceptStreamClient();
  sendStreamFrame();
  delay(1);
}

