# Terry
ESP32-based LED matrix display

Terry uses an 8x32 matrix display to display helpful information such as the time and temperature. 

<img width="300" alt="Terry in action" src="https://github.com/user-attachments/assets/0847cd3d-cbce-43cd-8a81-87a1cbf4bf79" />

https://github.com/user-attachments/assets/c2c3426d-ec96-477e-add0-1d1e0bbbd54f

# Feautures

- Time: Terry tells the time extremely accurately using a WiFi connection to grab accurate NTP time. It updates every minute on the minute by sliding in the new time from the right. Time is in 12-hour format for now, 24 hour time will be optional.
- Weather: Terry displays the temperature as well, pulled every 15 minutes from [Visual Crossing](https://www.visualcrossing.com/)
- Precipitation: Terry displays an umbrella between the time and temperature if it is going to rain today. This will be expanded to other precipation types soon.

# Setup
- 3D print and assemble the case according to the instructions in the description (Makerworld link coming soon)
- Wire your ESP32 to the LED matrix per the wiring diagram below
  - VCC --> 5V
  - GRD --> GND
  - DIN --> P23
  - CS --> P17
  - CLK --> P18
- <img width="800" alt="480239091-ca9630f1-b777-48fb-a5fe-a29e09f07fe3" src="https://github.com/user-attachments/assets/6ee581bd-6d25-4c7c-be1b-cee6b6b83671" />
- Yours won't be inserted into the case yet but here is a picture of mine: 
  - ![Wiring Picture](https://github.com/user-attachments/assets/7c5ab319-5fd0-4ffd-a1fc-9304fc8639b6)
- Bend the pins on the LED matrix down 90 degrees towards the back of the screen so that it can fit into the case. You can use wires like this to get a good grip. 
  - ![bending](https://github.com/user-attachments/assets/dbfcd2de-6f96-4751-a893-ae388367d0b4)
  - Afterwards it should look like this and you can slide it into the case
  - ![into case](https://github.com/user-attachments/assets/c671e177-6ca5-4583-8d74-6b1f77775c2f)
- Now, plug in your usb cable and route it through the hole in the back of the case
  - ![plug in cable](https://github.com/user-attachments/assets/b59cb7b3-7dac-4e0d-85e6-932604f62717)
- You may want to flash the code bedfore sealing everything up. Press firmly on the back of the case to close it
  - ![sealed](https://github.com/user-attachments/assets/d23433fc-3080-449c-9b34-788be8b17271)

- Plug in the usb cable to your computer and flash the arduino file. Follow the instructions under # Arduino Setip
- Get your weather [API key](https://www.visualcrossing.com/sign-up/) and zip code ready
- Connect to Terry over a WiFi connection in order to enter your WiFi network information, API key, and zip code (you only need to do this once)
- Enjoy

# Arduino Setup

- Install Arduino IDE
- Add ESP32 to boards manager: https://dl.espressif.com/dl/package_esp32_index.json
- Add libraries: `ArduinoJson, MDParola, WiFiManager`
- For more assistance, follow the [ESP32 Software Installation Instructions](https://esp32io.com/tutorials/esp32-software-installation)


