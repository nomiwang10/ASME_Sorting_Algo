#include <Wire.h>
#include <Adafruit_TCS34725.h>

/* * Speed/Accuracy Setup:
 * Integration Time: 24ms (0xF6) - fast enough for moving objects[cite: 613].
 * Gain: 16X - better for distinguishing blue/green shades[cite: 143, 677].
 */
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_25MS, TCS34725_GAIN_16X);

// ---------- TUNING KNOBS ----------
const uint16_t PRESENT_CLEAR = 1800; // Presence gate: Only process if item is close[cite: 355].
const float SAT_RECYCLE_MIN  = 0.15f; // Minimum "colorfulness" to be considered recycling.

// Hue ranges for recycling [cite: 943, 944]
const float GREEN_H_MIN = 80.0f;
const float GREEN_H_MAX = 170.0f;
const float BLUE_H_MIN  = 180.0f;
const float BLUE_H_MAX  = 260.0f;

// Hardware helps us now, so we only need minimal software averaging
const int SAMPLES = 2; 

void setup() {
  Serial.begin(9600);
  if (!tcs.begin()) {
    Serial.println("Sensor not found!");
    while (1);
  }

  /* * HARDWARE PERSISTENCE FILTER (Register 0x0C)
   * We set this to 0x03. This means 3 consecutive integration cycles 
   * must agree before an interrupt/status change is "real".
   * This drastically improves accuracy without needing slow software delays.
   */
  tcs.write8(0x0C, 0x03); 

  tcs.setInterrupt(false); // Turn on onboard LED [cite: 957]
}

void loop() {
  uint16_t r, g, b, c;
  readFastHardwareRaw(r, g, b, c);

  // If nothing is in front of the sensor, do nothing [cite: 961]
  if (c < PRESENT_CLEAR) {
    return;
  }

  // Normalize RGB by Clear channel to handle varying distances [cite: 962]
  float rn = (float)r / (float)c;
  float gn = (float)g / (float)c;
  float bn = (float)b / (float)c;

  float h, s, v;
  rgbToHsv(rn, gn, bn, h, s, v);

  // ---------- CLASSIFICATION (GARBAGE BY DEFAULT) ----------
  // Start with the assumption that it is garbage.
  String binType = "GARBAGE"; 

  // Check if it qualifies as "Colorful" enough to be a recycling block
  if (s >= SAT_RECYCLE_MIN) {
    // Check if the color falls into Green or Blue recycling ranges
    if ((h >= GREEN_H_MIN && h <= GREEN_H_MAX) || 
        (h >= BLUE_H_MIN && h <= BLUE_H_MAX)) {
      binType = "RECYCLING";
    }
  }

  // Output results
  Serial.print(binType);
  Serial.print(" | H: "); Serial.print(h, 1);
  Serial.print(" S: "); Serial.print(s, 2);
  Serial.print(" C: "); Serial.println(c);
}

/**
 * readFastHardwareRaw:
 * Uses the AVALID bit in the Status Register (0x13) to wait for data.
 * This is much faster than the old delay(10) method.
 */
static void readFastHardwareRaw(uint16_t &r, uint16_t &g, uint16_t &b, uint16_t &c) {
  uint32_t rs=0, gs=0, bs=0, cs=0;
  
  for (int i = 0; i < SAMPLES; i++) {
    // Poll hardware: AVALID (bit 0) is 1 when a cycle is finished.
    while (!(tcs.read8(0x13) & 0x01)); 
    
    uint16_t r1, g1, b1, c1;
    tcs.getRawData(&r1, &g1, &b1, &c1);
    rs += r1; gs += g1; bs += b1; cs += c1;
  }
  r = rs / SAMPLES; g = gs / SAMPLES; b = bs / SAMPLES; c = cs / SAMPLES;
}

// Standard HSV Conversion logic [cite: 945-952]
static void rgbToHsv(float r, float g, float b, float &h, float &s, float &v) {
  float mx = max(r, max(g, b));
  float mn = min(r, min(g, b));
  float d  = mx - mn;
  v = mx;
  s = (mx <= 1e-6f) ? 0.0f : (d / mx);
  if (d <= 1e-6f) { h = 0.0f; return; }
  if (mx == r) h = 60.0f * fmod(((g - b) / d), 6.0f);
  else if (mx == g) h = 60.0f * (((b - r) / d) + 2.0f);
  else h = 60.0f * (((r - g) / d) + 4.0f);
  if (h < 0.0f) h += 360.0f;
}