#ifndef ERIC_LIB_H
#define ERIC_LIB_H

struct Sensor {
  void Setup() {
      pinMode(pin1, INPUT_PULLUP);
    if (pin2 != 255) {
      pinMode(pin2, INPUT_PULLUP);
    }
  }

  // Must call every update.
  void Update() {
    bool reading_high = true;
    if (pin2 == 255) {
      reading_high = digitalRead(pin1) == HIGH;
    } else {
      reading_high = digitalRead(pin1) == HIGH && digitalRead(pin2) != HIGH;
    }
    if (reading_high && counter < Sensor::MAX_COUNT) {
      counter++;
    } else if (counter > 0) {
      counter--;
    }
  }

  bool Active() {
    return counter > 5;
  }

  int counter = 0;
  static const int MAX_COUNT = 100;

  Sensor(uint8_t pin1, uint8_t pin2)
    : pin1(pin1), pin2(pin2) {}

  static Sensor SingleSensor(uint8_t pin1) {
    return Sensor(pin1, 255);
  }
  static Sensor Pin1AndNotPin2(uint8_t pin1, uint8_t pin2) {
    return Sensor(pin1, pin2);
  }
  uint8_t pin1;
  uint8_t pin2;
};

struct Button {

  void Setup() {
    pinMode(button_pin, INPUT_PULLUP);
  }

  // Must call every update. Returns true when the button has been activated
  bool UpdateAndCheck() {
    if (digitalRead(button_pin) == LOW && button_counter < 200) {
      button_counter++;
      if (button_counter == 100) {
        return true;
      }
    } else if (button_counter > 0) {
      button_counter--;
    }
    return false;
  }

  Button(uint8_t button_pin)
    : button_pin(button_pin) {}

  int button_counter = 0;
  uint8_t button_pin;
};

#endif // ERIC_LIB_H
