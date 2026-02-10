#include <Wire.h>
#include <Adafruit_TCS34725.h>

/* * Speed/Accuracy Setup:
 * Integration Time: 24ms (0xF6) - fast enough for moving objects.
 * Gain: 16X - better for distinguishing blue/green shades.
 */
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_24MS, TCS34725_GAIN_16X);

// ---------- TUNING KNOBS ----------
const uint16_t PRESENT_CLEAR = 600;   // Lowered from 1800 to detect objects further away.
const uint16_t MAX_SAFE_CLEAR = 20000; // If C is higher than this, the sensor is likely oversaturated.
const float SAT_RECYCLE_MIN  = 0.15f; // Minimum "colorfulness" to be considered recycling.
const float VALUE_BLACK_MAX  = 0.2f;  // If brightness (Value) is below this, it's "Black/Dark".
// Hue ranges for recycling
const float GREEN_H_MIN = 80.0f;
const float GREEN_H_MAX = 170.0f;
const float BLUE_H_MIN  = 180.0f;
const float BLUE_H_MAX  = 260.0f;

// Hardware helps us now, so we only need minimal software averaging
const int SAMPLES = 2; 

void setup() {
  // 1. INCREASE BAUD RATE: 9600 is often the bottleneck that causes slowdowns.
  Serial.begin(115200); 
  
  // Tell the ESP32 to use specific pins for I2C
  // Wire.begin(SDA_PIN, SCL_PIN);
  Wire.begin(21, 22);

  // Explicitly telling the code to look for the sensor at address 0x29
  if (!tcs.begin(0x29)) { 
    Serial.println("Sensor not found at 0x29!");
   while (1);
  }

  // HARDWARE PERSISTENCE FILTER: 3 cycles must agree.
  tcs.write8(0x0C, 0x03); 
  tcs.setInterrupt(false);
}

void loop() {
  uint16_t r, g, b, c;
  
  // SCAN: Grab current data 
  readFastHardwareRaw(r, g, b, c);

  // GATE: Ignore if nothing is there 
  if (c < PRESENT_CLEAR) {
    return; 
  }
  // Add a check to catch "Too Close" errors
  if (c > MAX_SAFE_CLEAR) {
    Serial.println(">>> ERROR: OBJECT TOO CLOSE (OVERSATURATED)");
    return; 
  }

  // NORMALIZE & CONVERT: Turn raw data into Hue/Sat 
  float h, s, v;
  rgbToHsv((float)r/c, (float)g/c, (float)b/c, h, s, v);

  // DECIDE: Categorize the object. Default to GARBBAGE
  String binType = "GARBAGE"; 

  // EXCEPTION FOR BLACK/DARKNESS
  // If the light reflected is too dim, we don't even check color.
  if (v < VALUE_BLACK_MAX) {
    binType = "GARBAGE"; 
  } 
  else if (s >= SAT_RECYCLE_MIN) {
    if ((h >= GREEN_H_MIN && h <= GREEN_H_MAX) || 
        (h >= BLUE_H_MIN && h <= BLUE_H_MAX)) {
      binType = "RECYCLING";
    }
  }

  // --- LEAVE SPACE FOR ACTUATOR CODE ---
  // place "Kick" logic later.
  if (binType == "RECYCLING") {
    // TODO: Trigger Recycling Actuator
    Serial.print(">>> ACTION: RECYCLING BIN | ");
  } else {
    // TODO: Trigger Garbage Actuator
    Serial.print(">>> ACTION: GARBAGE BIN   | ");
  }
  // ----------------------

  // CLEAR: Print info and end loop. 
  // All local variables (h, s, v, binType) are deleted here.
  Serial.print("H: "); Serial.print(h, 1);
  Serial.print(" S: "); Serial.println(s, 2);
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
