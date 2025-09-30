//Recording for 5 seconds and downloading the audio
#include <Arduino.h>
#include <WiFi.h>
#include "SPIFFS.h"
#include "driver/i2s.h"
#include <vector>
#include <algorithm>

const char *ssid = "InnoFi";
const char *password = "Innovate@91761";

#define SAMPLE_RATE     16000
#define I2S_PORT        I2S_NUM_0
#define SAMPLE_BITS     16
#define CHANNEL_FORMAT  I2S_CHANNEL_FMT_ONLY_RIGHT
#define I2S_READ_LEN    1024
#define RECORD_TIME     5                // seconds
#define FILE_NAME       "/recording.wav"

#define I2S_BCK_IO      26
#define I2S_WS_IO       32
#define I2S_DATA_IN_IO  33

WiFiServer server(80);

// ---------------- I2S Setup ----------------
void i2s_install() {
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = CHANNEL_FORMAT,
      .communication_format = I2S_COMM_FORMAT_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 1024,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = -1
  };
  i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_BCK_IO,
      .ws_io_num = I2S_WS_IO,
      .data_out_num = -1,
      .data_in_num = I2S_DATA_IN_IO
  };
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
}

void i2s_uninstall() {
  i2s_driver_uninstall(I2S_PORT);
}

// ---------------- WAV Header ----------------
void writeWavHeader(File &file, uint32_t sampleRate, uint16_t bitsPerSample, uint16_t channels, uint32_t dataLength) {
  uint32_t fileSize = 36 + dataLength;
  uint16_t audioFormat = 1;

  file.seek(0);
  file.write((const uint8_t *)"RIFF", 4);
  file.write((uint8_t *)&fileSize, 4);
  file.write((const uint8_t *)"WAVE", 4);

  file.write((const uint8_t *)"fmt ", 4);
  uint32_t subChunk1Size = 16;
  file.write((uint8_t *)&subChunk1Size, 4);
  file.write((uint8_t *)&audioFormat, 2);
  file.write((uint8_t *)&channels, 2);
  file.write((uint8_t *)&sampleRate, 4);
  uint32_t byteRate = sampleRate * channels * bitsPerSample / 8;
  file.write((uint8_t *)&byteRate, 4);
  uint16_t blockAlign = channels * bitsPerSample / 8;
  file.write((uint8_t *)&blockAlign, 2);
  file.write((uint8_t *)&bitsPerSample, 2);

  file.write((const uint8_t *)"data", 4);
  file.write((uint8_t *)&dataLength, 4);
}

// ---------------- Record Function ----------------
bool recordWav(const char *path, int seconds) {
  File file = SPIFFS.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for recording");
    return false;
  }

  // Reserve header space (will fill later)
  for (int i = 0; i < 44; i++) file.write((uint8_t)0);

  uint32_t bytesWritten = 0;
  size_t bytesRead;
  int16_t i2sBuffer[I2S_READ_LEN];

  unsigned long start = millis();
  while ((millis() - start) < (seconds * 1000)) {
    i2s_read(I2S_PORT, (void *)i2sBuffer, sizeof(i2sBuffer), &bytesRead, portMAX_DELAY);
    if (bytesRead > 0) {
      file.write((uint8_t *)i2sBuffer, bytesRead);
      bytesWritten += bytesRead;
    }
  }

  // Now write proper WAV header
  writeWavHeader(file, SAMPLE_RATE, SAMPLE_BITS, 1, bytesWritten);
  file.close();
  Serial.printf("Recording saved: %s (%d bytes)\n", path, bytesWritten + 44);
  return true;
}

// ---------------- Web Server ----------------
void handleClient(WiFiClient client) {
  String req = client.readStringUntil('\r');
  client.flush();

  if (req.indexOf("GET / ") >= 0) {
    // Index page
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();
    client.println("<h2>ESP32 Recording</h2>");
    client.println("<a href=\"/download?file=recording.wav\">Download recording.wav</a>");
  }
  else if (req.indexOf("GET /download") >= 0) {
    String fname = FILE_NAME; // Always serve our recording
    File file = SPIFFS.open(fname, FILE_READ);
    if (!file) {
      client.println("HTTP/1.1 404 Not Found");
      client.println("Connection: close");
      client.println();
      return;
    }

    size_t fsize = file.size();
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/octet-stream");
    client.print("Content-Length: "); client.println(fsize);
    client.print("Content-Disposition: attachment; filename=\"");
    client.print("recording.wav");
    client.println("\"");
    client.println("Connection: close");
    client.println();

    uint8_t buf[1024];
    while (file.available()) {
      size_t len = file.read(buf, sizeof(buf));
      client.write(buf, len);
    }
    file.close();
  }
  else {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Connection: close");
    client.println();
  }
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);
  Serial.println("\n--- RECORD & SERVE WAV ---");

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
    return;
  }

  i2s_install();
  if (!recordWav(FILE_NAME, RECORD_TIME)) {
    Serial.println("Recording failed.");
  }
  i2s_uninstall();

  WiFi.begin(ssid, password);
  Serial.printf("Connecting to WiFi %s", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.printf("\nConnected. IP: %s\n", WiFi.localIP().toString().c_str());

  server.begin();
  Serial.println("HTTP server ready. Visit / in browser");
}

// ---------------- Loop ----------------
void loop() {
  WiFiClient client = server.available();
  if (client) {
    while (client.connected()) {
      if (client.available()) {
        handleClient(client);
        break;
      }
    }
    delay(1);
    client.stop();
  }
}