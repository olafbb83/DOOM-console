// ESP32-S3 as a USB<->UART bridge to read the RP2350's serial debug output.
//
// The RP2350 (running Doom) prints logs + panic messages on its UART1 TX = GP8
// at 115200 baud. This sketch listens on an ESP32 GPIO and forwards every byte
// to the USB serial port, so it shows up in the Arduino Serial Monitor / any
// terminal on the ESP32's COM port.
//
// Wiring:
//   RP2350 GP8 (TX)   ->  ESP32-S3 GPIO 2  (BRIDGE_RX below)
//   RP2350 GND        ->  ESP32-S3 GND
//   (RP2350 powered by its own USB; ESP32 powered by its own USB to the PC)
//
// Both are 3.3V logic -> direct connection, no level shifter.
//
// Open the ESP32's COM port at 115200 baud to watch the RP2350 output.

#define BRIDGE_RX 2     // ESP32-S3 input pin; connect RP2350 GP20 here
#define BRIDGE_TX 3     // not used (we only receive), but Serial1 needs a TX pin
#define LINK_BAUD 115200

void setup() {
  Serial.begin(115200);                                   // USB -> PC
  Serial1.begin(LINK_BAUD, SERIAL_8N1, BRIDGE_RX, BRIDGE_TX); // <- from RP2350 GP20
  delay(300);
  Serial.println("\n[uart_bridge] listening on GPIO 2 @115200 - feed RP2350 GP20 here");
}

void loop() {
  // forward RP2350 -> USB
  while (Serial1.available()) Serial.write(Serial1.read());
  // (optional) forward USB -> RP2350, in case we ever want to send keys
  while (Serial.available())  Serial1.write(Serial.read());
}
