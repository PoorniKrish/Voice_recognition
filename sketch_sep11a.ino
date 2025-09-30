#define SOUND_SENSOR_PIN 12  // AO pin of KY-038
#define LED_PIN 22
#define THRESHOLD 3000       // Adjust after testing

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(SOUND_SENSOR_PIN,INPUT);
  Serial.begin(115200);
}

void loop() {
  int soundLevel = analogRead(SOUND_SENSOR_PIN); // 0-4095 on ESP32
  Serial.print("Sound level: ");
  Serial.println(soundLevel);

  if (soundLevel >= THRESHOLD) {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("LED ON");
  } else {
    digitalWrite(LED_PIN, LOW);
    Serial.println("LED OFF");
  }

  delay(100);
}
