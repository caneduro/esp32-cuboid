# 🕐 Cuboid — ESP32 Smart Clock

A compact smart clock based on **ESP32-C3** with OLED display, weather, Google Calendar integration, alarm, night mode and full web configuration — all in a 3D-printed enclosure.

> 📦 **3D Model on MakerWorld:** [Mini ESP32 Clock & Media](https://makerworld.com/it/models/2105366-mini-esp32-clock-media#profileId-2277121)

---

## ✨ Features

- **Clock** — large font display with date, 3 font styles selectable
- **Weather** — current conditions via OpenWeatherMap (temp, humidity, description, icon)
- **Forecast** — next-day forecast (min/max temp, description)
- **Google Calendar** — shows upcoming events with scrolling text, supports up to 5 events
- **Alarm** — 4 ringtone patterns, day-of-week selection, buzzer
- **Night Mode** — auto screen off at set times, temporary wake on button press
- **5 Languages** — English, Italiano, Français, Español, Deutsch
- **Web UI** — full configuration via browser (dark/light theme)
- **OTA Updates** — update firmware wirelessly from Arduino IDE or browser (.bin upload)
- **Auto Clock Return** — returns to clock after N minutes of inactivity on other screens
- **Wi-Fi 6 (802.11ax)** support
- **Power management** — CPU throttles to 80 MHz when screen is off
- **Factory Reset** — hold button 10 seconds

---

## 🔧 Hardware

| Component | Detail |
|-----------|--------|
| MCU | ESP32-C6 supermini (or compatible ESP32 that fits) |
| Display | SSD1306 128×64 OLED (I2C) |
| Button | BOOT button (GPIO 9) |
| Buzzer | Passive buzzer (GPIO 18) |
| SDA | GPIO 19 |
| SCL | GPIO 20 |

To disable the buzzer, set `#define ENABLE_BUZZER 0` at the top of the sketch.

---

## 📚 Required Libraries

Install all from the Arduino Library Manager (`Sketch → Include Library → Manage Libraries`):

| Library | Author |
|---------|--------|
| NTPClient | Fabrice Weinberg |
| Adafruit GFX Library | Adafruit |
| Adafruit SSD1306 | Adafruit |
| U8g2 | oliver |
| ArduinoJson | Benoit Blanchon |
| ArduinoOTA | (built-in with ESP32 core) |

**Board:** Install **ESP32 by Espressif** via `File → Preferences → Board Manager URL`:
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```
Then: `Tools → Board → esp32 → ESP32C3 Dev Module`

---

## 🚀 First Flash

1. Clone or download this repository
2. Open `cuboid/cuboid.ino` in Arduino IDE
3. Select your board (`ESP32C3 Dev Module`) and the correct COM port
4. Click **Upload**
5. On first boot, the device starts in **AP mode** → connect your phone/PC to WiFi `ESP32-Cuboid`
6. Open `http://192.168.4.1` or `http://cuboid.local` in your browser
7. Configure your WiFi credentials, timezone, and optionally weather/calendar API keys
8. Save & Reboot — the device connects to your network

---

## 🌐 Web Configuration

Once connected to your network, open `http://cuboid.local` (or the IP shown on the display).

### Config page sections:

**⏰ Alarm & Time**
- Enable/disable alarm, set time and active days
- Choose ringtone pattern
- Set UTC offset and Auto DST

**🎨 Personalization**
- Clock font (Sans Bold / Sans / Serif Bold)
- Date format (DD/MM/YYYY, MM/DD/YYYY, YYYY/MM/DD, long formats)
- Auto clock return timeout (minutes)

**🌙 Night Mode**
- Enable/disable, set start/end times
- Wake duration (seconds) when button is pressed during night

**🌤 Weather & Calendar**
- OpenWeatherMap API key and city name → [Get free key](https://home.openweathermap.org/api_keys)
- Google Apps Script URL for calendar integration (see below)

**📶 WiFi & System**
- Change WiFi network and password
- Change interface language

### Advanced page (`/advanced`):
- Flash firmware from browser (.bin file)
- Set OTA username/password for Arduino IDE OTA
- Live system info (chip, heap, temperature, uptime)
- Live WiFi details (RSSI, channel, security, 802.11 flags)

---

## 📅 Google Calendar Integration

The device can fetch upcoming events from Google Calendar via a Google Apps Script web app.

### Setup steps:

1. Go to [script.google.com](https://script.google.com) and create a new project
2. Paste the following code:

```javascript
function doGet() {
  var cal = CalendarApp.getDefaultCalendar();
  var now = new Date();
  var end = new Date(now.getTime() + 48 * 60 * 60 * 1000); // next 48 hours
  var events = cal.getEvents(now, end);

  if (events.length === 0) {
    return ContentService.createTextOutput(
      JSON.stringify({ hasEvent: false })
    ).setMimeType(ContentService.MimeType.JSON);
  }

  var result = [];
  for (var i = 0; i < Math.min(events.length, 5); i++) {
    var e = events[i];
    var start = e.getStartTime();
    var isToday = (start.toDateString() === now.toDateString());
    var isTomorrow = (start.toDateString() === new Date(now.getTime() + 86400000).toDateString());
    var prefix = isToday ? "Today " : (isTomorrow ? "Tomorrow " : "");
    var timeStr = prefix + Utilities.formatDate(start, Session.getScriptTimeZone(), "HH:mm");
    result.push({ event: e.getTitle(), time: timeStr });
  }

  return ContentService.createTextOutput(
    JSON.stringify({ hasEvent: true, events: result })
  ).setMimeType(ContentService.MimeType.JSON);
}
```

3. Click **Deploy → New deployment → Web App**
4. Set "Execute as: Me" and "Who has access: Anyone"
5. Copy the deployment URL and paste it in the device's web UI under "Google Script URL"

---

## 🎮 Button Controls

| Action | Result |
|--------|--------|
| Short press | Cycle through modes: Clock → Calendar → Weather → Clock |
| Long press (on Weather screen) | Switch to Forecast view |
| Long press (on Calendar, multiple events) | Next event |
| Short press during alarm | Dismiss alarm |
| Short press during Night Mode | Wake screen temporarily |
| Hold 10 seconds | Factory reset |

---

## 🏗 3D Printed Enclosure

The enclosure is designed for Bambu Lab printers and is available on MakerWorld:
(i will make a better one someday...)
👉 **[Mini ESP32 Clock & Media — MakerWorld](https://makerworld.com/it/models/2105366-mini-esp32-clock-media#profileId-2277121)**

### Print settings (recommended):
- **Material:** PLA or PETG
- **Layer height:** 0.2 mm
- **Infill:** 15–20%
- **Supports:** Only where needed (check the model)
- **Printer:** Bambu Lab A1 / P1 / X1 or compatible

### Assembly tips:
- The OLED display fits in the front cutout 
- ESP32-C3 board sits in the rear compartment
- Route the USB cable through the back opening for power
- The buzzer and button can be hot-glued or press-fit into their slots

> The .3mf file already includes slicer settings. Open it directly in Bambu Studio.

---

## 📁 Repository Structure

```
cuboid/
└── cuboid.ino   ← main sketch (open this in Arduino IDE)
README.md
.gitignore
LICENSE
```

> ⚠️ Arduino requires the sketch file to be inside a folder with the **same name** (`cuboid/cuboid.ino`). Do not rename them independently.

---

## 🛠 Customization

### Change hardware pins
Edit the defines at the top of `cuboid.ino`:
```cpp
#define OLED_SDA        19
#define OLED_SCL        20
#define BOOT_BUTTON_PIN 9
#define BUZZER_PIN      18
```

### Disable buzzer
```cpp
#define ENABLE_BUZZER   0
```

### Change device hostname
```cpp
#define HOSTNAME  "cuboid"   // accessible as cuboid.local
```

### Change data refresh interval
```cpp
const unsigned long DATA_REFRESH_INTERVAL = 7200000UL;  // 2 hours in ms
```

---

## 📜 License

MIT License — see [LICENSE](LICENSE) for details.  
You are free to use, modify and distribute this project, including for the 3D enclosure.

---

## 🤝 Contributing

Pull requests and issues are welcome! If you improve the enclosure design or add features, feel free to open a PR or share your remix on MakerWorld.
