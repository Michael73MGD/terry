# Terry
ESP32-based LED matrix display

Terry uses an 8x32 matrix display to display helpful information such as the time and temperature. 

<img width="300" alt="Terry in action" src="https://github.com/user-attachments/assets/0847cd3d-cbce-43cd-8a81-87a1cbf4bf79" />

https://github.com/user-attachments/assets/c2c3426d-ec96-477e-add0-1d1e0bbbd54f

# Features

- Time: Terry synchronizes its clock over WiFi using NTP and slides in the new time each minute. Choose 12-hour or 24-hour time during setup; the choice is saved across restarts.
- Weather: Terry displays the temperature as well, pulled every 15 minutes from [Visual Crossing](https://www.visualcrossing.com/)
- Precipitation: Terry shows the tested 5x8 snowflake when any hour today has precipitation probability **above 50%** and includes snow in its expected precipitation types. Otherwise, the existing umbrella appears when today's overall precipitation probability exceeds 50%. Snow takes priority on mixed rain/snow days; exactly 50% does not qualify. Icons are hidden once their forecast date has passed or weather is stale.
- Snow detection uses hourly probability and type together, so a high rain chance and a low snow chance do not incorrectly select the snowflake. The [weather API](https://www.visualcrossing.com/resources/documentation/weather-data/weather-data-documentation/) reports daily precipitation chance as the maximum hourly chance. It does not provide a separate snow-only probability in these fields; Terry uses precipitation chance for hours forecast to include snow. It does not infer snowfall from cold temperatures or snow already on the ground.
- Recovery: weather outages leave the clock running. Failed requests retry after 1, 2, 4, 8, then 15 minutes; rate limits wait at least 15 minutes. Last good weather is retained for up to one hour before being hidden.
- Time zones: select a supported US time zone or UTC during setup. US daylight saving transitions are handled automatically.

# Setup
- 3D print and assemble the case according to the instructions in the description (Makerworld link coming soon)
- Wire your ESP32 to the LED matrix per the wiring diagram below
  - VCC --> 5V
  - GND --> GND
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
- You may want to flash the code before sealing everything up. Press firmly on the back of the case to close it
  - ![sealed](https://github.com/user-attachments/assets/d23433fc-3080-449c-9b34-788be8b17271)

- Plug in the usb cable to your computer and flash the arduino file. Follow the instructions under # Arduino Setup
- Get your weather [API key](https://www.visualcrossing.com/sign-up/) and zip code ready
- Join the open **Terry** WiFi network when prompted. The display repeats the connection instructions while setup is open. If the setup page doesn't open automatically, visit **http://192.168.4.1**. The setup network closes after five minutes; restart Terry to try again if setup is incomplete.
- First open **Setup**, enter your API key and US ZIP code, choose your **Time zone** and **Clock format** from the dropdowns, and save. Then open **Configure WiFi** and save your home network credentials. Both network and weather settings must be complete before setup closes. WiFi-first also works: if your phone switches networks, rejoin Terry and visit **http://192.168.4.1** to finish Setup.
- ZIP codes can contain five digits (including leading zeroes) or ZIP+4, such as `01234` or `12345-6789`. Keys must be alphanumeric and at most 49 characters long.
- The time zone dropdown offers Eastern (New York), Central (Chicago), Mountain (Denver), Pacific (Los Angeles), Arizona, Alaska, Hawaii, and UTC. Eastern is the default. Choose the zone matching your weather location so the umbrella refers to the same calendar day.
- Clock format defaults to **12-hour**, including when upgrading from older settings. Choose **24-hour** for times such as `00:05`, `13:05`, and `23:59`. Reopen setup to change either selection; an erase is not required.
- Enjoy

# Arduino Setup

- Install Arduino IDE and open `terry.ino` from the `terry` folder. All sketch and header files must stay together, including `build_opt.h`, which disables unused Parola effects to save flash space.
- Add the ESP32 board index in Preferences: https://espressif.github.io/arduino-esp32/package_esp32_index.json
- Install **esp32 by Espressif Systems 3.3.12** through Boards Manager. This version supplies the certificate bundle used for verified HTTPS connections.
- Install the following Library Manager versions: **ArduinoJson 7.4.3**, **MD_Parola 3.7.7**, **MD_MAX72XX 3.5.1**, and **WiFiManager 2.0.17**.
- The original board's markings indicate **NodeMCU-32S** (`esp32:esp32:nodemcu-32s`). Select that board in Arduino IDE. The generic **ESP32 Dev Module** build profile is also retained. The documented wiring uses classic ESP32 hardware SPI pins (MOSI 23 and SCK 18), with chip select on GPIO 17.
- For more assistance, follow the [ESP32 Software Installation Instructions](https://esp32io.com/tutorials/esp32-software-installation)

With Arduino CLI, the pinned dependency profile is in `sketch.yaml`:

```sh
arduino-cli compile --profile esp32-dev-module .
```

The [Arduino CLI build profile](https://docs.arduino.cc/arduino-cli/sketch-project-file) downloads its specified dependencies into a managed cache on first use. When those exact versions are already installed globally, a local build can also use:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all .
```

## Troubleshooting and changing settings

- If the display says weather setup failed, **restart Terry and join its WiFi network again** to correct the key or ZIP code. The device remembers that setup is needed on its next boot; it does not repeatedly reboot itself.
- If saved WiFi cannot connect within 15 seconds at startup, Terry opens setup so you can enter a new network. After the five-minute setup window closes, it retries saved WiFi in the background.
- You can also open setup by sending `setup` followed by a newline in Serial Monitor at **115200 baud**. No accessible button is required.
- After setup or a restart, Terry requests a time sync as soon as WiFi is ready, then immediately fetches weather once the clock is valid. A stationary **WiFi** indicator appears while connecting, followed by **Sync** during time and weather fetching. It then paints the first time and temperature together without waiting for the next minute. Setup instructions continue to repeat while the portal is open.
- HTTPS weather requests wait for a valid system clock, rather than disabling certificate verification. If weather fails, Terry shows the clock and retries automatically. A startup weather request gets up to 20 seconds before the clock is shown without weather; a later successful response adds the temperature.
- Temporary network/service failures and rate limits retry automatically. They do not require changing settings. A newly configured device may reopen setup on its next boot until it has completed one successful weather request.
- The setup network is intentionally open. It runs during setup/recovery and closes on successful configuration or after five minutes. API keys are masked in the form and omitted from application logs.
- Settings are stored in NVS. A valid legacy `/1st_config.json` is imported from SPIFFS automatically on upgrade; the migration does not format SPIFFS. Keep the existing partition layout when upgrading so legacy data remains accessible. Avoid **Erase All Flash** if you want to preserve settings.

## Development checks

Run the native regression tests with a C++17 compiler and the pinned ArduinoJson library installed:

```sh
sh tests/run.sh
```

By default the runner looks for ArduinoJson in `~/Documents/Arduino/libraries/ArduinoJson/src`; set `ARDUINOJSON_DIR` if your library directory differs. Tests use address and undefined-behavior sanitizers. Hardware checks still require an ESP32, LED matrix, WiFi network, and weather account.

