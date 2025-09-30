# ESP32 Audio Recorder with SPIFFS and Web Download
This project records 5 seconds of audio from an I2S microphone connected to the ESP32, saves the audio as a WAV file in SPIFFS filesystem, and serves the recording via a simple web server for download.

## Features
Records 16 kHz, 16-bit, mono audio using the I2S interface
Stores audio file in SPIFFS with correct WAV header
Connects to WiFi and hosts an HTTP server
Provides a link to download the recorded audio file
