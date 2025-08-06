# Terry
ESP32-based LED matrix display

Terry uses an 8x32 matrix display to display helpful information such as the time and temperature. 

<img width="1307" height="1000" alt="image" src="https://github.com/user-attachments/assets/0847cd3d-cbce-43cd-8a81-87a1cbf4bf79" />

# Feautures

- Time: Terry tells the time extremely accurately using a WiFi connection to grab accurate NTP time. It updates every minute on the minute by sliding in the new time from the right. Time is in 12-hour format for now, 24 hour time will be optional.
- Weather: Terry displays the temperature as well, pulled every 15 minutes from [Visual Crossing](https://www.visualcrossing.com/)
- Precipitation: Terry displays an umbrella between the time and temperature if it is going to rain today. This will be expanded to other precipation types soon. 

# Contributing

- Install Arduino IDE
- Add ESP32 to boards manager: https://dl.espressif.com/dl/package_esp32_index.json
- Add libraries: `ArduinoJson, MDParola, WiFiManager`
- Get your weather [API key](https://www.visualcrossing.com/sign-up/) and zip code ready 
