// RP2350-Zero + ST7789 240x320 SPI — bring-up / wiring test
//
// Library: Adafruit ST7789 (+ Adafruit GFX) — chosen for a first, foolproof
// hardware check. Performance driver (LovyanGFX/DMA) comes later for Doom.
//
// Wiring (SPI0):
//   ST7789  -> RP2350-Zero
//   GND     -> GND
//   VCC     -> 3V3 (OUT)
//   SCL     -> GP2  (SPI0 SCK)
//   SDA     -> GP3  (SPI0 TX / MOSI)
//   DC      -> GP4
//   RES     -> GP5
//   CS      -> GP6
//   BL/BLK  -> GP7

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

#define PIN_SCLK  2
#define PIN_MOSI  3
#define PIN_DC    4
#define PIN_RST   5
#define PIN_CS    6
#define PIN_BL    7

Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, PIN_CS, PIN_DC, PIN_RST);

void drawColorBars() {
  const uint16_t bars[] = {
    ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE,
    ST77XX_YELLOW, ST77XX_CYAN, ST77XX_MAGENTA, ST77XX_WHITE
  };
  const int n = sizeof(bars) / sizeof(bars[0]);
  int w = tft.width() / n;
  for (int i = 0; i < n; i++) {
    tft.fillRect(i * w, 0, w, tft.height(), bars[i]);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nRP2350-Zero ST7789 bring-up test");

  // Backlight on
  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);

  // Route SPI0 to the chosen pins, then start the display
  SPI.setSCK(PIN_SCLK);
  SPI.setTX(PIN_MOSI);
  SPI.begin();

  tft.init(240, 320);          // native panel resolution (portrait)
  tft.setRotation(3);          // landscape -> 320x240
  tft.invertDisplay(true);     // ST7789 modules usually need INVON; flip if colors look wrong
  tft.setSPISpeed(40000000);   // 40 MHz; bump later once stable

  Serial.printf("Resolution after rotation: %dx%d\n", tft.width(), tft.height());

  // --- Visual self-test ---
  tft.fillScreen(ST77XX_BLACK);
  drawColorBars();
  delay(1500);

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(3);
  tft.setCursor(10, 20);
  tft.println("RP2350-Zero");
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 70);
  tft.println("ST7789 OK");
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(10, 100);
  tft.printf("%d x %d", tft.width(), tft.height());

  // Border to confirm full-panel addressing (no offset/garbage edges)
  tft.drawRect(0, 0, tft.width(), tft.height(), ST77XX_RED);
}

uint32_t frame = 0;
void loop() {
  // Live counter proves the board keeps running and SPI stays healthy
  tft.fillRect(10, 150, 300, 30, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(10, 150);
  tft.printf("frame %lu", (unsigned long)frame++);
  Serial.printf("frame %lu\n", (unsigned long)frame);
  delay(500);
}
