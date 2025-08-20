# Terry
ESP32-based LED matrix display

Terry uses an 8x32 matrix display to display helpful information such as the time and temperature. 

<img width="300" alt="Terry in action" src="https://github.com/user-attachments/assets/0847cd3d-cbce-43cd-8a81-87a1cbf4bf79" />

# Feautures

- Time: Terry tells the time extremely accurately using a WiFi connection to grab accurate NTP time. It updates every minute on the minute by sliding in the new time from the right. Time is in 12-hour format for now, 24 hour time will be optional.
- Weather: Terry displays the temperature as well, pulled every 15 minutes from [Visual Crossing](https://www.visualcrossing.com/)
- Precipitation: Terry displays an umbrella between the time and temperature if it is going to rain today. This will be expanded to other precipation types soon.

# Setup

- Wire your ESP32 to the LED matrix per the wiring diagram below. We wired the CS_PIN to 17 and VCC to the 5V pin
- <img width="800" alt="image" src="https://github.com/user-attachments/assets/ca9630f1-b777-48fb-a5fe-a29e09f07fe3" />
- Bend the pins on the LED matrix down 90 degrees towards the back of the screen so that it can fit into the case. (picture coming soon)
- 3D print and assemble the case according to the instructions in the description (Makerworld link coming soon)
- Plug in the usb cable to your computer and flash the arduino file. Follow the instructions under # Arduino Setip
- Get your weather [API key](https://www.visualcrossing.com/sign-up/) and zip code ready
- Connect to Terry over a WiFi connection in order to enter your WiFi network information, API key, and zip code (you only need to do this once)
- Enjoy

# Arduino Setup

- Install Arduino IDE
- Add ESP32 to boards manager: https://dl.espressif.com/dl/package_esp32_index.json
- Add libraries: `ArduinoJson, MDParola, WiFiManager`
- For more assistance, follow the [ESP32 Software Installation Instructions](https://esp32io.com/tutorials/esp32-software-installation)

https://github.com/user-attachments/assets/c2c3426d-ec96-477e-add0-1d1e0bbbd54f
