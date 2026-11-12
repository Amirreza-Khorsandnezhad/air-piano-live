// Air Piano LIVE = AirPiano.ino + a serial link to the web page.
// Serial out (115200): "@READY", "@N <noteIndex> <cm>", "@OFF", plus the normal "E4 450ms" lines.
// Serial in: "D<cm>" distance override, "R" toggle record, "P" play.
const uint8_t TRIG = 7, ECHO = 6, BUZ = 8;
const uint8_t PIN_R = 9, PIN_G = 10, PIN_B = 5;
const uint8_t REC_BTN = 2, PLAY_BTN = 4;
const char* NAMES[8] = {"C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5"};
const uint16_t FREQ[8] = {262, 294, 330, 349, 392, 440, 494, 523};
const uint8_t COLORS[8][3] = {
  {255, 0, 0}, {255, 70, 0}, {255, 200, 0}, {0, 255, 0},
  {0, 255, 200}, {0, 0, 255}, {75, 0, 130}, {160, 0, 255}
};
struct NoteRec { int8_t idx; uint16_t dur; uint16_t gap; };
NoteRec song[32];
uint8_t songLen = 0;
bool wasRec = false, haveEnd = false;
int recCur = -1;
unsigned long segStart = 0, lastEnd = 0;
uint16_t pendingGap = 0;
float remoteD = 0; unsigned long remoteT = 0;
bool recReq = false, playReq = false;
char cmd[16]; uint8_t cl = 0;

float readDistanceCm() {
  digitalWrite(TRIG, LOW); delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  unsigned long t = pulseIn(ECHO, HIGH, 30000UL);
  if (t == 0) return -1;
  return t * 0.0343 / 2.0;
}
int noteFromDistance(float d) {
  if (d < 5.0 || d > 45.0) return -1;
  int b = (int)((d - 5.0) / 5.0);
  return b > 7 ? 7 : b;
}
void setLed(int idx, float br) {
  if (idx < 0) { analogWrite(PIN_R, 0); analogWrite(PIN_G, 0); analogWrite(PIN_B, 0); return; }
  analogWrite(PIN_R, COLORS[idx][0] * br);
  analogWrite(PIN_G, COLORS[idx][1] * br);
  analogWrite(PIN_B, COLORS[idx][2] * br);
}
float tuningBrightness(float d) {
  float pos = fmod(d - 5.0, 5.0) / 5.0;
  return 0.2 + 0.8 * (1.0 - fabs(pos - 0.5) * 2.0);
}
void emitOn(int i, float d) { Serial.print("@N "); Serial.print(i); Serial.print(' '); Serial.println(d, 1); }
void emitOff() { Serial.println("@OFF"); }
void printNote(int idx, unsigned long dur) {
  Serial.print(NAMES[idx]); Serial.print(' '); Serial.print(dur); Serial.println("ms");
}
void closeSegment(unsigned long now) {
  if (recCur >= 0 && songLen < 32) {
    unsigned long dur = now - segStart;
    song[songLen++] = { (int8_t)recCur, (uint16_t)min(dur, 65535UL), pendingGap };
    printNote(recCur, dur);
  }
  lastEnd = now; haveEnd = true; recCur = -1;
}
void handleRecording(bool rec, int note) {
  unsigned long now = millis();
  if (rec && !wasRec) { songLen = 0; recCur = -1; haveEnd = false; Serial.println("--- REC ---"); }
  if (rec && note != recCur) {
    if (recCur >= 0) closeSegment(now);
    if (note >= 0) {
      recCur = note; segStart = now;
      pendingGap = haveEnd ? (uint16_t)min(now - lastEnd, 65535UL) : 0;
    }
  }
  if (!rec && wasRec) { if (recCur >= 0) closeSegment(now); Serial.println("--- REC stop ---"); }
  wasRec = rec;
}
void readCmd() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      cmd[cl] = 0;
      if (cl) {
        if (cmd[0] == 'D') { remoteD = atof(cmd + 1); remoteT = millis(); }
        else if (cmd[0] == 'R') recReq = true;
        else if (cmd[0] == 'P') playReq = true;
      }
      cl = 0;
    } else if (cl < 15) cmd[cl++] = c;
  }
}
void playSong() {
  noTone(BUZ); setLed(-1, 0); emitOff();
  Serial.println("--- PLAY ---");
  for (uint8_t i = 0; i < songLen; i++) {
    delay(song[i].gap);
    setLed(song[i].idx, 1.0);
    tone(BUZ, FREQ[song[i].idx]);
    emitOn(song[i].idx, 5 + song[i].idx * 5 + 2.5);
    printNote(song[i].idx, song[i].dur);
    delay(song[i].dur);
    noTone(BUZ); setLed(-1, 0); emitOff();
  }
  Serial.println("--- END ---");
  while (digitalRead(PLAY_BTN) == LOW) {}
  while (Serial.available()) Serial.read();
}
void setup() {
  Serial.begin(115200);
  pinMode(TRIG, OUTPUT); pinMode(ECHO, INPUT); pinMode(BUZ, OUTPUT);
  pinMode(PIN_R, OUTPUT); pinMode(PIN_G, OUTPUT); pinMode(PIN_B, OUTPUT);
  pinMode(REC_BTN, INPUT_PULLUP); pinMode(PLAY_BTN, INPUT_PULLUP);
  Serial.println("@READY");
}
void loop() {
  static int note = -1, cand = -2, lastTone = -1;
  static bool recOn = false, lastBtn = HIGH;
  static float lastEmit = -100;
  readCmd();
  bool b = digitalRead(REC_BTN);
  if (lastBtn == HIGH && b == LOW) { recOn = !recOn; delay(30); }
  lastBtn = b;
  if (recReq) { recOn = !recOn; recReq = false; }
  if (!recOn && (digitalRead(PLAY_BTN) == LOW || playReq)) { playReq = false; playSong(); return; }

  float d = (remoteT && millis() - remoteT < 400) ? remoteD : readDistanceCm();
  int raw = noteFromDistance(d);
  if (raw == cand) note = raw;
  cand = raw;
  if (note != lastTone) {
    if (note >= 0) tone(BUZ, FREQ[note]); else { noTone(BUZ); emitOff(); }
    lastTone = note; lastEmit = -100;
  }
  if (note >= 0 && fabs(d - lastEmit) >= 0.5) { emitOn(note, d); lastEmit = d; }
  setLed(note, note >= 0 ? tuningBrightness(d) : 0);
  handleRecording(recOn, note);
  delay(20);
}
