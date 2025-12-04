# ESP32 Closed-Loop Temperature Controller With Motorized Knob

This project is a fully self-contained temperature control system built on the ESP32.  
It uses a MAX6675 thermocouple for temperature feedback, a 28BYJ-48 stepper motor with ULN2003 driver to physically rotate a control knob, and a local web interface for setting the target temperature.  

The system operates entirely offline using ESP32 Access Point mode and includes an OLED display for real-time status. Target temperature is stored in non-volatile memory and persists across power cycles.

This project demonstrates practical embedded control, sensor integration, motion control, web-based configuration, and persistent storage.

---

## Core Features

- Offline temperature control using ESP32 Access Point mode  
- Web-based UI for setting target temperature (25–200 °C)  
- MAX6675 thermocouple temperature sensing (250 ms updates)  
- Stepper-driven mechanical knob control  
- Automatic homing using a limit switch  
- Closed-loop temperature stabilization  
- OLED status display (SSD1306)  
- Non-volatile storage using ESP32 Preferences  
- High-torque half-step motor driving  
- Mechanical belt reduction support  
- Safety hysteresis to prevent oscillation  

---

## System Operation

1. On boot, the ESP32:
   - Loads the last saved target temperature from non-volatile memory
   - Starts a Wi-Fi Access Point
   - Homes the stepper using the limit switch
   - Initializes the OLED display

2. The user connects to the ESP32 Wi-Fi network and opens the control page.

3. The target temperature is set through the web UI.

4. Every 250 ms:
   - The MAX6675 is read
   - Temperature is updated on the OLED

5. Every 5 seconds:
   - The controller compares actual temperature to the target
   - The stepper motor turns the knob up or down in 1-degree mechanical increments
   - Position is tracked in degrees

6. Target temperature changes are saved immediately to flash memory.

---

## Hardware Used

- ESP32 Dev Module
- MAX6675 K-type thermocouple amplifier
- K-type thermocouple
- 28BYJ-48 stepper motor
- ULN2003 stepper driver board
- SSD1306 OLED (128×64)
- Mechanical limit switch (active LOW)
- 5V external power supply (2–3A recommended)
- Optional belt reduction (3:1 used in this build)

---<img width="849" height="605" alt="pcb" src="https://github.com/user-attachments/assets/8dbca9e4-234a-4759-92a1-3bf2fa8ac0a7" />


## Pin Configuration

### Stepper (ULN2003)

- IN1 → GPIO 14  
- IN2 → GPIO 12  
- IN3 → GPIO 13  
- IN4 → GPIO 15  

### MAX6675

- SO  → GPIO 19  
- CS  → GPIO 5  
- SCK → GPIO 18  

### OLED (I²C)

- SDA → GPIO 21  
- SCL → GPIO 22  
- Address → 0x3C  

### Limit Switch

- LIMIT_PIN → GPIO 32 (INPUT_PULLUP)

---

## Mechanical Configuration

- Stepper is belt-driven with a 3:1 reduction
- Effective resolution:
  - 11 motor steps per degree
  - 33 steps per knob degree with belt reduction
- Homing direction is reversed:
  - Clockwise movement reduces angle
  - Home position is treated as 270 degrees
- After contacting the limit switch, the system backs off 3 degrees for mechanical safety

---

## Web Interface

- ESP32 operates in Access Point mode
- Local web server on port 80
- Responsive HTML UI
- User sets target temperature between 25–200 °C
- Target is stored using Preferences (flash memory)
- Page reloads automatically after saving

### Default Wi-Fi Credentials

SSID:  
ESP32-TEMP-CONTROL  

Password:  
12345678  

---

## OLED Display Output

- Current temperature (large font)
- Target temperature
- Mechanical knob position in degrees

Display refresh is throttled to 500 ms to reduce flicker.

---

## Control Logic

- Temperature read every 250 ms
- Movement decision every 5 seconds
- Hysteresis: ±0.1 °C
- If temperature is below target → knob increases heat
- If temperature is above target → knob decreases heat
- Motor coils remain energized for holding torque

---

## Software Dependencies

Install the following libraries using Arduino Library Manager:

- WiFi
- WebServer
- Adafruit GFX
- Adafruit SSD1306
- MAX6675
- Preferences

---

## Build Notes and Power Requirements

- Use a dedicated 5V 2–3A supply for the stepper motor
- Add a 470–1000 µF capacitor across the motor supply
- Do not power the stepper directly from the ESP32 5V pin under load
- Keep thermocouple wiring away from motor wiring to reduce noise

---

## Safety Notes

- This system mechanically turns a live temperature control knob
- Do not operate unattended without proper thermal and electrical protection
- Limit switch is mandatory for safe homing
- Always verify full mechanical travel before enabling automatic movement

---

## Future Improvements

- PID temperature control instead of step-based control
- Encoder-based position tracking
- Dual temperature sensors for redundancy
- Remote monitoring via MQTT
- Encrypted web interface
- Automatic power-loss recovery with mechanical re-alignment

---

## Author

Kevin Kearns  
Embedded and IoT Developer  
Self-taught MCU and electronics developer  
Web Development Bootcamp Graduate  
Creator of Intro to Arduino Course  

---

## License

MIT License  
Free to use, modify, and distribute.


