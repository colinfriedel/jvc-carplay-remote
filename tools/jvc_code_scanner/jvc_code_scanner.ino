/*
  JVC wired-remote code scanner.

  Companion tool for jvc_carplay_remote. Sends one 7-bit command at a time on
  the steering-remote wire so every code from 0x00 to 0x7F can be tried
  against a head unit and its effect noted. Stepping by hand instead of on a
  timer leaves time to watch the screen after each code.

  Serial Monitor: 9600 baud, line ending set to "No line ending".

    n     send the current code, then advance to the next
    r     send the current code again
    gXX   jump to hex code XX (for example g1A)
    p     print the current code without sending

  Wiring matches the main sketch: D7 -> steering-remote wire, shared ground.
  D7 is only ever pulled low or released, never driven high.

  Frame format and timing are described in the main sketch.
*/

const uint16_t PULSE_US = 527;
const uint8_t  ADDRESS  = 0x47;
const uint8_t  REPS     = 3;
const uint8_t  MAX_CODE = 0x7F;

uint8_t currentCode = 0x00;

inline void lineLow()  { DDRD |=  _BV(PD7); }
inline void lineFree() { DDRD &= ~_BV(PD7); }

void sendBit(bool one) {
  lineLow();
  delayMicroseconds(PULSE_US);
  lineFree();
  delayMicroseconds(one ? 3 * PULSE_US : PULSE_US);
}

void sendBits7(uint8_t value) {
  for (uint8_t i = 0; i < 7; i++) {
    sendBit((value >> i) & 1);
  }
}

void sendFrame(uint8_t cmd) {
  lineLow();
  delayMicroseconds(16 * PULSE_US);
  lineFree();
  delayMicroseconds(8 * PULSE_US);
  sendBit(1);
  for (uint8_t r = 0; r < REPS; r++) {
    sendBits7(ADDRESS);
    sendBits7(cmd);
    sendBit(1);
    sendBit(1);
  }
  lineFree();
}

// Results observed on a KW-M560BT running CarPlay over USB. Codes not
// listed here produced no visible effect, or have not been identified.
const char *knownNote(uint8_t code) {
  switch (code) {
    case 0x04: return "volume up";
    case 0x05: return "volume down";
    case 0x08: return "source change";
    case 0x0E: return "mute";
    case 0x12: return "next track / seek up";
    case 0x13: return "previous track / seek down";
    case 0x1A: return "voice assistant";
    default:   return "";
  }
}

void printCode(const __FlashStringHelper *prefix) {
  Serial.print(prefix);
  Serial.print(F("0x"));
  if (currentCode < 0x10) Serial.print('0');
  Serial.print(currentCode, HEX);
  const char *note = knownNote(currentCode);
  if (note[0] != '\0') {
    Serial.print(F("  ("));
    Serial.print(note);
    Serial.print(')');
  }
  Serial.println();
}

void transmit() {
  printCode(F("Sending "));
  sendFrame(currentCode);
}

int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// Reads two hex digits from the serial buffer, giving up after one second
// so a stray 'g' cannot hang the sketch.
bool readHexByte(uint8_t &out) {
  char digits[2];
  const unsigned long start = millis();
  for (uint8_t i = 0; i < 2; i++) {
    while (!Serial.available()) {
      if (millis() - start > 1000) return false;
    }
    digits[i] = Serial.read();
  }
  const int hi = hexValue(digits[0]);
  const int lo = hexValue(digits[1]);
  if (hi < 0 || lo < 0) return false;
  out = (hi << 4) | lo;
  return out <= MAX_CODE;
}

void setup() {
  PORTD &= ~_BV(PD7);
  lineFree();
  Serial.begin(9600);
  Serial.println(F("JVC code scanner. n=next r=resend gXX=jump p=print"));
  printCode(F("Current code: "));
}

void loop() {
  if (!Serial.available()) return;

  switch (Serial.read()) {
    case 'n':
      transmit();
      currentCode = (currentCode + 1) & MAX_CODE;
      break;
    case 'r':
      transmit();
      break;
    case 'p':
      printCode(F("Current code: "));
      break;
    case 'g': {
      uint8_t code;
      if (readHexByte(code)) {
        currentCode = code;
        printCode(F("Current code: "));
      } else {
        Serial.println(F("Usage: gXX, where XX is 00 to 7F"));
      }
      break;
    }
  }
}
