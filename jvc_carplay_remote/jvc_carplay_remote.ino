/*
  JVC wired-remote controller for a two-way momentary switch.

  Gestures, per direction:
    tap                   volume step
    double tap            down: mute, up: voice assistant
    short hold + release  down: previous track, up: next track
    long hold             repeating volume change

  Target: Arduino Nano (ATmega328P, 16 MHz) driving the steering-remote
  input of a JVC KW-M560BT.

  Protocol: JVC's IR-style frame sent over the remote wire by pulling the
  line low and letting the head unit's pull-up release it. Timing and the
  base command set come from the "JVC stalk adapter - DIY?" thread on
  AVForums (https://www.avforums.com/threads/jvc-stalk-adapter-diy.248455/);
  the remaining codes were found by scanning this unit.

    pulse width T        527 us
    header               low 16T, idle 8T
    start bit            1
    body                 REPS x [ address (7 bits), command (7 bits), 1, 1 ]
    bit order            LSB first
    bit 0                low T, idle T
    bit 1                low T, idle 3T

  Wiring:
    switch common  -> GND
    switch down    -> D3
    switch up      -> D5
    D7             -> head unit steering-remote wire (shared ground required)

  D7 must never be driven high. It is only ever an output latched low or a
  floating input, so the head unit's pull-up sets the idle level.
*/

const uint8_t PIN_DOWN = 3;
const uint8_t PIN_UP   = 5;

const uint16_t PULSE_US = 527;
const uint8_t  ADDRESS  = 0x47;
const uint8_t  REPS     = 3;

const uint8_t CMD_VOL_UP     = 0x04;
const uint8_t CMD_VOL_DOWN   = 0x05;
const uint8_t CMD_MUTE       = 0x0E;
const uint8_t CMD_TRACK_NEXT = 0x12;
const uint8_t CMD_TRACK_PREV = 0x13;
const uint8_t CMD_ASSISTANT  = 0x1A;

const uint16_t TAP_MAX_MS             = 250;
const uint16_t SKIP_MAX_MS            = 500;
const uint16_t DOUBLE_TAP_WINDOW_MS   = 150;
const uint16_t DEBOUNCE_MS            = 40;
const uint16_t DOUBLE_TAP_LOCKOUT_MS  = 300;

struct Button {
  uint8_t pin;
  uint8_t tapCmd;
  uint8_t holdCmd;
  uint8_t doubleTapCmd;
  uint8_t doubleTapReps;
  uint16_t repeatDelayMs;
  const char *tapName;
  const char *holdName;
  const char *doubleTapName;
};

// Volume-up repeats need a slower pace for the head unit to keep
// accepting them past its stepped-volume range. Mute toggles, so it is
// sent once; repeated frames cancel it out.
const Button UP_BUTTON = {
  PIN_UP, CMD_VOL_UP, CMD_TRACK_NEXT, CMD_ASSISTANT, REPS, 125,
  "VOL UP", "NEXT TRACK", "ASSISTANT"
};

const Button DOWN_BUTTON = {
  PIN_DOWN, CMD_VOL_DOWN, CMD_TRACK_PREV, CMD_MUTE, 1, 20,
  "VOL DOWN", "PREV TRACK", "MUTE"
};

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

void sendFrame(uint8_t cmd, uint8_t reps) {
  lineLow();
  delayMicroseconds(16 * PULSE_US);
  lineFree();
  delayMicroseconds(8 * PULSE_US);
  sendBit(1);
  for (uint8_t r = 0; r < reps; r++) {
    sendBits7(ADDRESS);
    sendBits7(cmd);
    sendBit(1);
    sendBit(1);
  }
  lineFree();
}

void fire(const char *name, uint8_t cmd, uint8_t reps) {
  Serial.println(name);
  sendFrame(cmd, reps);
}

// True only if the pin has stayed released for DEBOUNCE_MS, which filters
// out contact bounce on both press and release.
bool confirmReleased(uint8_t pin) {
  const unsigned long start = millis();
  while (millis() - start < DEBOUNCE_MS) {
    if (digitalRead(pin) == LOW) return false;
  }
  return true;
}

bool secondTapFollows(uint8_t pin) {
  const unsigned long start = millis();
  while (millis() - start < DOUBLE_TAP_WINDOW_MS) {
    if (digitalRead(pin) == LOW) return true;
  }
  return false;
}

void waitForRelease(uint8_t pin) {
  while (digitalRead(pin) == LOW) { }
  confirmReleased(pin);
}

void handlePress(const Button &b) {
  const unsigned long pressedAt = millis();

  while (millis() - pressedAt < TAP_MAX_MS) {
    if (digitalRead(b.pin) != LOW && confirmReleased(b.pin)) {
      if (secondTapFollows(b.pin)) {
        waitForRelease(b.pin);
        fire(b.doubleTapName, b.doubleTapCmd, b.doubleTapReps);
        delay(DOUBLE_TAP_LOCKOUT_MS);
      } else {
        fire(b.tapName, b.tapCmd, REPS);
      }
      return;
    }
  }

  while (millis() - pressedAt < SKIP_MAX_MS) {
    if (digitalRead(b.pin) != LOW && confirmReleased(b.pin)) {
      fire(b.holdName, b.holdCmd, REPS);
      return;
    }
  }

  Serial.println(b.tapName);
  do {
    sendFrame(b.tapCmd, REPS);
    delay(b.repeatDelayMs);
  } while (digitalRead(b.pin) == LOW || !confirmReleased(b.pin));
}

void setup() {
  PORTD &= ~_BV(PD7);
  lineFree();
  pinMode(PIN_DOWN, INPUT_PULLUP);
  pinMode(PIN_UP, INPUT_PULLUP);
  Serial.begin(9600);
}

void loop() {
  const bool up   = digitalRead(PIN_UP)   == LOW;
  const bool down = digitalRead(PIN_DOWN) == LOW;

  if (up && !down) {
    handlePress(UP_BUTTON);
  } else if (down && !up) {
    handlePress(DOWN_BUTTON);
  }
}
