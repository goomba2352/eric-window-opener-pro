#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include "eric_lib.h"
#include "eric_window_opener_web.h"

// const int STATE_CLOSED = 0;
// const int STATE_OPENING = 1;
// const int STATE_PAUSED_OPEN = 2;
// const int STATE_SECURING_OPEN = 3;
// const int STATE_OPENED = 4;
// const int STATE_CLOSING = 5;
// const int STATE_PAUSED_CLOSE = 6;
// const int STATE_SECURING_CLOSE = 7;
// USB: D5 Purple, D6 Green, D4 Blue

const int MOTOR_STOP = 0;
const int MOTOR_FORWARD = 1;
const int MOTOR_BACK = 2;

struct SettingsV1 {
  uint32_t open_timeout_millis = 60000;
  uint32_t close_timeout_millis = 60000;
  uint32_t secure_open_millis = 3000;
  uint32_t secure_close_millis = 3000;
  uint32_t secure_close_speed = 200;
  uint32_t secure_open_speed = 200;
  uint32_t normal_motor_speed = 255;

  static SettingsV1 &Get() {
    // Gib defaults here. Constructor will attempt to load from EEPROM if available.
    static SettingsV1 settings;
    return settings;
  }

  static void Save() {
    SettingsV1 &save = Get();
    save.magic_string = SettingsV1::V1MagicString;
    EEPROM.begin(sizeof(save));
    EEPROM.put(0, save);
    EEPROM.end();
    Serial.println("Wrote settings!");
    Serial.println(Get().magic_string);
  }

  static void Load() {
    SettingsV1 load_attempt;
    EEPROM.begin(sizeof(load_attempt));
    EEPROM.get(0, load_attempt);
    EEPROM.end();
    if (load_attempt.magic_string == SettingsV1::V1MagicString) {
      // copy fields over.
      Get().close_timeout_millis = load_attempt.close_timeout_millis;
      Get().open_timeout_millis = load_attempt.open_timeout_millis;
      Get().secure_close_millis = load_attempt.secure_close_millis;
      Get().secure_open_millis = load_attempt.secure_open_millis;
      Get().secure_close_speed = load_attempt.secure_close_speed;
      Get().secure_open_speed = load_attempt.secure_open_speed;
      Serial.println("Data verified, magic string '" + String(load_attempt.magic_string) + "' found.");
      Serial.println("Restored settings");
    } else {
      Serial.println("Settings not found.");
      Serial.println("magic string '" + String(load_attempt.magic_string) + "' found, expected: '" + String(SettingsV1::V1MagicString) + "'.");
    }
  }


  static const uint32_t MIN_TIMEOUT = 200;            // 200 ms
  static const uint32_t MAX_TIMEOUT = 5 * 60 * 1000;  // 5 minutes
  static bool SetOrInvalidate(uint32_t value, uint32_t min, uint32_t max, uint32_t *set) {
    if (value < min) {
      return false;
    } else if (value > max) {
      return false;
    }
    *set = value;
    return true;
  }

  // V1 Magic String: 1163020611 (ERIC). If magic string isn't there, do not load the struct.
  // Purposefully at the end.
  uint32_t magic_string = 0;
  static const uint32_t V1MagicString = 1163020611;

private:

  SettingsV1() {}

  // no can has copy
  SettingsV1(SettingsV1 const &);
  void operator=(SettingsV1 const &);

  // Static members
};
SettingsV1 &settings = SettingsV1::Get();

struct State {
public:
  State(bool terminal, uint32_t *max_millis_in_state, const String &name, State *expire_open, String press_button_to_text, int motor_direction, const uint32_t *motor_speed)
    : terminal(terminal),
      max_millis_in_state(max_millis_in_state),
      name(name),
      expire_option(expire_open),
      button_action(nullptr),
      press_button_to_text(press_button_to_text),
      motor_direction(motor_direction),
      motor_speed(motor_speed) {}

  inline static State *OfTerminal(const String &name, String press_button_to_text) {
    return new State(true, nullptr, name, nullptr, press_button_to_text, MOTOR_STOP, 0);
  }

  inline static State *OfExpires(const String &name, uint32_t *max_millis, State *next, String press_button_to_text, int motor_direction, const uint32_t *motor_speed) {
    return new State(false, max_millis, name, next, press_button_to_text, motor_direction, motor_speed);
  }

  bool terminal;
  uint32_t *max_millis_in_state;
  String name;
  State *expire_option = nullptr;
  State *button_action = nullptr;
  State *close_sensor_action = nullptr;
  State *open_sensor_action = nullptr;

  String press_button_to_text;
  int motor_direction = MOTOR_STOP;
  const uint32_t *motor_speed = nullptr;
};

static long int action_start_millis = 0;

const int OPEN_DIRECTION = MOTOR_FORWARD;
const int CLOSE_DIRECTION = MOTOR_BACK;
static State *STATE_CLOSED = State::OfTerminal("closed", "Open Window");
//STATE_CLOSED->ActionStart = &MotorStop;
static State *STATE_SECURE_CLOSE = State::OfExpires("almost fully closed", &settings.secure_close_millis, STATE_CLOSED, "pls wait...", CLOSE_DIRECTION, &settings.secure_close_speed);
static State *STATE_PAUSED_OPENING = State::OfTerminal("pause open", "Close Window");
static State *STATE_OPENING = State::OfExpires("opening", &settings.open_timeout_millis, STATE_PAUSED_OPENING, "Stop Opening", OPEN_DIRECTION, &settings.normal_motor_speed);
static State *STATE_OPENED = State::OfTerminal("opened", "Close Window");
static State *STATE_SECURE_OPEN = State::OfExpires("almost fully open", &settings.secure_open_millis, STATE_OPENED, "pls wait...", OPEN_DIRECTION, &settings.secure_open_speed);
static State *STATE_PAUSED_CLOSING = State::OfTerminal("paused while closing", "Open Window");
static State *STATE_CLOSING = State::OfExpires("closing", &settings.close_timeout_millis, STATE_PAUSED_CLOSING, "Stop Closing", CLOSE_DIRECTION, &settings.normal_motor_speed);

State *state = STATE_CLOSED;

ESP8266WebServer server(80);

// There is a hardware bug where open is active when OPEN and CLOSE are activated
// for close, but not open. Nasty hack, but lets see if this works.
Sensor OpenSensor = Sensor::Pin1AndNotPin2(12, 14); // D6, D5
Sensor CloseSensor = Sensor::SingleSensor(14); //  D5
Button HardwareButton(2);   // D4
const u_int8_t PIN_MOTOR_1 = 5;       // D1
const u_int8_t PIN_MOTOR_2 = 4;       // D2
const u_int8_t PIN_MOTOR_SPEED = 13;  // D7 


void setup() {
  Serial.begin(9600);
  Serial.println("setting up buttons");
  STATE_CLOSED->button_action = STATE_OPENING;
  STATE_OPENING->button_action = STATE_PAUSED_OPENING;
  STATE_OPENING->open_sensor_action = STATE_SECURE_OPEN;
  STATE_PAUSED_OPENING->button_action = STATE_CLOSING;
  STATE_OPENED->button_action = STATE_CLOSING;
  STATE_CLOSING->button_action = STATE_PAUSED_CLOSING;
  STATE_CLOSING->close_sensor_action = STATE_SECURE_CLOSE;
  STATE_PAUSED_CLOSING->button_action = STATE_OPENING;

  // Starting EEPROM
  SettingsV1::Load();

  Serial.println("setting up pins");
  OpenSensor.Setup();
  CloseSensor.Setup();
  HardwareButton.Setup();
  
  pinMode(PIN_MOTOR_1, OUTPUT);
  pinMode(PIN_MOTOR_2, OUTPUT);
  pinMode(PIN_MOTOR_SPEED, OUTPUT);

  Serial.println("Hello world");
  // TODO: Replace with EEPROM setting?
  WiFi.begin("UR SSID", "UR PASSWORD");
  Serial.print("Attempting wifi connection.");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("  Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // Actual pages you can visit
  server.on("/", ServeRootPage);
  server.on("/settings", ServeSettingsPage);

  // Updates state:
  server.on("/button", ButtonHandler);

  // Get info:
  server.on("/raw", RawSensor);
  server.on("/get_settings", GetSettings);
  server.begin();
  Serial.println("Started http server");

  // Try to determine initial state from pins. Do 200 reads, and
  // then determine state.
  for (int i = 0; i < 200; i++) {
    OpenSensor.Update();
    CloseSensor.Update();
  }
  if (CloseSensor.Active()) {
    ChangeState(STATE_CLOSED);
  } else if (OpenSensor.Active()) {
    ChangeState(STATE_OPENED);
  } else {
    // Unknown state, next button press closes the window.
    // This avoids jamming the window open in the worst
    // case scenario.
    ChangeState(STATE_PAUSED_OPENING);
  }
}

void loop() {
  server.handleClient();
  OpenSensor.Update();
  CloseSensor.Update();

  long int now = millis();

  if (HardwareButton.UpdateAndCheck()) {
    ButtonHandler();
  }

  if (state->terminal) {
    return;
  }
  if (state->max_millis_in_state != nullptr && (now - action_start_millis > *(state->max_millis_in_state))) {
    ChangeState(state->expire_option);
  }
  if (state->close_sensor_action != nullptr && CloseSensor.Active()) {
    ChangeState(state->close_sensor_action);
  } else if (state->open_sensor_action != nullptr && OpenSensor.Active()) {
    ChangeState(state->open_sensor_action);
  }
}


void ServeRootPage() {
  ServePage(server, "Eric Window Opener Pro", home);
}

void ServeSettingsPage() {
  UpdateSettings();
  ServePage(server, "Window Opener Pro Settings", settings_page);
}

void MotorForward(uint32_t speed) {
  analogWrite(PIN_MOTOR_SPEED, speed);
  digitalWrite(PIN_MOTOR_1, HIGH);
  digitalWrite(PIN_MOTOR_2, LOW);
}

void MotorBackward(uint32_t speed) {
  analogWrite(PIN_MOTOR_SPEED, speed);
  digitalWrite(PIN_MOTOR_1, LOW);
  digitalWrite(PIN_MOTOR_2, HIGH);
}

void StopMotor() {
  analogWrite(PIN_MOTOR_SPEED, 0);
  digitalWrite(PIN_MOTOR_1, LOW);
  digitalWrite(PIN_MOTOR_2, LOW);
}

void Button() {
  ChangeState(state->button_action);
}

void ChangeState(State *new_state) {
  if (state == new_state || new_state == nullptr) {
    Serial.println("state null or same, exiting.");
    return;
  }
  Serial.println("changing state to: " + new_state->name + " from " + (state == nullptr ? "unset" : state->name));
  state = new_state;
  action_start_millis = millis();
  if (state->motor_direction == MOTOR_STOP) {
    StopMotor();
  } else if (state->motor_speed != nullptr) {
    if (state->motor_direction == MOTOR_FORWARD) {
      MotorForward(*state->motor_speed);
    } else if (state->motor_direction == MOTOR_BACK) {
      MotorBackward(*state->motor_speed);
    }
  }
}

void ButtonHandler() {
  Button();
  server.send(200, "text/json", "");
}

void RawSensor() {
  String close = CloseSensor.Active() ? "true" : "false";
  String open = OpenSensor.Active() ? "true" : "false";
  String button_sensor = digitalRead(HardwareButton.button_pin) != HIGH ? "true" : "false";
  String state_name = state->name;
  String button_action = state->press_button_to_text;
  String val = "{ \"close_sensor\": "
               + close + ", \"open_sensor\":"
               + open + ", \"button_sensor\":"
               + button_sensor + ", \"state_name\":\""
               + state_name + "\", \"button_action\":\""
               + button_action + "\" }";
  server.send(200, "text/json", val);
}

void GetSettings() {
  String val = "{ \"close_timeout\": " + String(settings.close_timeout_millis)
               + ", \"open_timeout\":" + String(settings.open_timeout_millis)
               + ", \"close_secure\":" + String(settings.secure_close_millis)
               + ", \"open_secure\":" + String(settings.secure_open_millis)
               + ", \"close_secure_speed\":" + String(settings.secure_close_speed)
               + ", \"open_secure_speed\":" + String(settings.secure_open_speed)
               + ", \"default_motor_speed\":" + String(settings.normal_motor_speed) + " }";
  server.send(200, "text/json", val);
}

bool TrySetIntSetting(const String &setting_name, uint32_t min, uint32_t max, uint32_t *destination) {
  if (server.hasArg(setting_name) && server.arg(setting_name) != nullptr) {
    uint32_t before_value = *destination;
    if (SettingsV1::SetOrInvalidate(server.arg(setting_name).toInt(), min, max, destination)) {
      uint32_t after_value = *destination;
      if (before_value == after_value) {
        Serial.println("Not changed '" + setting_name + "'. ");
        return false;
      } else {
        Serial.println("Updated '" + setting_name + "'. ");
        return true;
      }
    } else {
      Serial.println("invalid range for '" + setting_name + "', accepted: [" + String(min) + ", " + String(max) + "]. ");
      return false;
    }
  }
  Serial.println("Arg not set: '" + setting_name + "'.");
  return false;
}

void UpdateSettings() {
  bool updated = false;
  updated |= TrySetIntSetting("open_timeout", SettingsV1::MIN_TIMEOUT, SettingsV1::MAX_TIMEOUT, &settings.open_timeout_millis);
  updated |= TrySetIntSetting("open_secure", SettingsV1::MIN_TIMEOUT, SettingsV1::MAX_TIMEOUT, &settings.secure_open_millis);
  updated |= TrySetIntSetting("open_secure_speed", 0, 255, &settings.secure_open_speed);
  updated |= TrySetIntSetting("close_timeout", SettingsV1::MIN_TIMEOUT, SettingsV1::MAX_TIMEOUT, &settings.close_timeout_millis);
  updated |= TrySetIntSetting("close_secure", SettingsV1::MIN_TIMEOUT, SettingsV1::MAX_TIMEOUT, &settings.secure_close_millis);
  updated |= TrySetIntSetting("close_secure_speed", 0, 255, &settings.secure_close_speed);
  if (updated) {
    SettingsV1::Save();
  }
}
