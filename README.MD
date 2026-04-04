# 🕐 Cuboid — ESP32 Smart Clock

A compact smart clock built on the **ESP32-C6 Super Mini** — shows the time, weather, Google Calendar events, and lets you set an alarm. Everything configurable from a browser, no app needed.

> 📦 **3D Model on MakerWorld:** [Mini ESP32 Clock & Media](https://makerworld.com/it/models/2105366-mini-esp32-clock-media#profileId-2277121)

> [!IMPORTANT]
> **Hardware & Assembly Guide:** For now, refer to [**esp32-clock-media**](https://github.com/caneduro/esp32-clock-media#hardware--assembly) for hardware requirements and step-by-step assembly photos. The enclosure is 100% compatible with this firmware.

---

## ✨ What it does

| | |
|---|---|
| 🕐 **Clock** | Large font, 3 styles, date below |
| 🌤 **Weather** | Current conditions + next-day forecast via OpenWeatherMap |
| 📅 **Calendar** | Up to 5 upcoming events from Google Calendar, scrolling text |
| ⏰ **Alarm** | 4 ringtones, per-day scheduling |
| 🌙 **Night Mode** | Auto screen-off on schedule, wake with button press |
| ☀️ **Auto Brightness** | Up to 8 time-based brightness slots |
| 💤 **Eco Mode** | Full WiFi radio shutdown between syncs — minimal power draw |
| 🌐 **Web UI** | Full config from any browser, dark/light theme |
| 💾 **Backup/Restore** | Export and import all settings as JSON |
| 🛜 **Wi-Fi 6** | 802.11ax support |
| 🌍 **5 Languages** | English, Italiano, Français, Español, Deutsch |

---

## 🔧 Hardware

| Component | Detail |
|---|---|
| MCU | ESP32-C6 Super Mini |
| Display | SSD1306 128×64 OLED (I2C) |
| Button | BOOT button — GPIO 9 |
| Buzzer | Passive buzzer — GPIO 18 |
| SDA | GPIO 19 |
| SCL | GPIO 20 |

> No buzzer? Set `#define ENABLE_BUZZER 0` at the top of `cuboid.ino`.

---

## 🚀 Getting Started

### 1. Flash

1. Install **ESP32 by Espressif** in Arduino IDE (board manager URL below)
2. Select `ESP32C6 Dev Module` and your COM port
3. Open `cuboid/cuboid.ino` and click **Upload**

```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

### 2. First boot

On first boot the device creates a WiFi hotspot called `ESP32-Cuboid`.

1. Connect your phone or PC to it
2. Open `http://192.168.4.1` (or `http://cuboid.local`)
3. Enter your WiFi credentials, timezone, and optionally your API keys
4. Save & Reboot — done

After connecting to your network the clock is reachable at **`http://cuboid.local`** or the IP shown on the display.

---

## 🎮 Button Controls

| Action | Result |
|---|---|
| Short press | Cycle modes: Clock → Calendar → Weather → Clock |
| Long press on Weather | Switch to Forecast view |
| Long press on Calendar (multiple events) | Next event |
| Short press during alarm | Dismiss |
| Short press during Night Mode | Wake screen temporarily |
| Hold 2 seconds | Open on-device Settings menu |
| Hold 10 seconds | Factory reset |

---

## 🌐 Web Configuration

Open `http://cuboid.local` from any browser on your network.

### ⏰ Alarm & Time
- Enable/disable alarm, set time and active days
- Choose ringtone (*Digital Beep*, *Nervous Cricket*, *Melodic Rise*, *Sci-Fi Siren*)
- UTC offset and Auto DST

### 🎨 Personalization
- Clock font (Sans Bold / Sans / Serif Bold)
- Date format (5 styles including day/month names)
- Screen-off animation and speed
- Wake animation (mirror the screen-off effect on wake)
- Auto clock return timeout
- **Auto-brightness** — define up to 8 time/brightness slots; the display dims and brightens automatically throughout the day

### 🌙 Night Mode
- Off / Scheduled / Always on
- Set start and end times
- Wake duration when button is pressed at night

### 🌤 Weather & Calendar
- OpenWeatherMap API key + city → [Get a free key](https://home.openweathermap.org/api_keys)
- Google Apps Script URL for calendar (see setup below)

### 📶 WiFi & System
- Change network and password
- Change language

### ⚙️ Advanced (`/advanced`)
- Flash firmware from browser (`.bin` upload)
- Set OTA username/password for Arduino IDE OTA
- Export/import all settings as `cuboid-settings.json`
- Live system info: chip, heap, temperature, uptime
- Live WiFi info: RSSI, channel, security, 802.11 flags
- **Factory Reset** — two-step confirmation wipes all settings and reboots into setup mode

---

## 💤 Eco Mode

Eco Mode cuts power consumption by shutting down the WiFi radio completely between syncs. Enable it from the on-device settings menu (WiFi → EcoMode) or from the hardware button.

When enabled:
- The radio is stopped (`esp_wifi_stop`) after every sync cycle
- Weather, calendar and NTP are refreshed every 2 hours; the radio powers up only for the duration of the sync
- **Pre-alarm sync** — the radio wakes automatically 1–10 minutes before the scheduled alarm, so the time is always accurate when it fires
- A small animated indicator (`~*` / `*~`) appears in the top-right corner of the clock face while connecting
- If the alarm fires mid-sync, the connection is aborted immediately so the buzzer is not delayed

Eco Mode is most useful on battery-powered builds or whenever the web UI is not needed during the day.

---

## 📅 Google Calendar Setup

The clock fetches events via a Google Apps Script web app that you deploy once.

1. Go to [script.google.com](https://script.google.com) and create a new project
2. Paste this code:

```javascript
function doGet() {
  var cal = CalendarApp.getDefaultCalendar();
  var now = new Date();
  var end = new Date(now.getTime() + 48 * 60 * 60 * 1000);
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

3. **Deploy → New deployment → Web App**
4. Set *Execute as: Me* and *Who has access: Anyone*
5. Copy the URL and paste it in the Web UI under **Google Script URL**

---

## 🛠 Customization

```cpp
#define OLED_SDA        19    // I2C pins
#define OLED_SCL        20
#define BOOT_BUTTON_PIN 9
#define BUZZER_PIN      18
#define ENABLE_BUZZER   1     // set to 0 to disable buzzer entirely
#define HOSTNAME        "cuboid"  // accessible as cuboid.local
```

```cpp
const unsigned long DATA_REFRESH_INTERVAL = 7200000UL;  // weather/calendar/NTP refresh in normal mode (ms)
                                                         // also used as eco sync interval
```

---

## 📚 Required Libraries

Install from Arduino Library Manager (`Sketch → Include Library → Manage Libraries`):

| Library | Author |
|---|---|
| NTPClient | Fabrice Weinberg |
| Adafruit GFX Library | Adafruit |
| Adafruit SSD1306 | Adafruit |
| U8g2 | oliver |
| ArduinoJson | Benoit Blanchon |
| ArduinoOTA | built-in with ESP32 core |

---

## 📁 Repository Structure

```
cuboid/
└── cuboid.ino   ← open this in Arduino IDE
README.md
CHANGELOG.md
LICENSE
```

> ⚠️ Arduino requires the sketch file to be inside a folder with the **same name**. Don't rename them independently.

---

## 🏗 3D Enclosure

Print files and assembly photos are in the older repo for now:
🔗 **[Assembly Photos & Hardware Guide](https://github.com/caneduro/esp32-clock-media#hardware--assembly)**

👉 **[MakerWorld — ready-to-print profiles](https://makerworld.com/it/models/2105366-mini-esp32-clock-media#profileId-2277121)**

**Quick print settings:** PLA or PETG · 0.2 mm layers · 15–20% infill · supports where needed

---

## 🗺️ Roadmap

- [ ] **Auto Timezone via IP** — detect timezone automatically on first boot
- [x] **System Info Screen** — show IP, WiFi status and metrics directly on the OLED
- [x] **Web Live Preview** — show what's going onto the display remotely
- [ ] **Improved 3D Enclosure** — new optimized case on MakerWorld
- [ ] **Display Preview on website** — Realtime monitoring/preview of what is going on on the integrated display so you can change settings without looking at it
- [ ] **Full Offline Mode** — set time and date fully offline in cases where wifi is not an option (maybe wifi ap used only to set things and then they are all calculated)
- [ ] **Multi Wifi** — set multiple wifi network and connect to them flawlessly (maybe 5 network or idk)


---

## 📜 Changelog

See [CHANGELOG.md](CHANGELOG.md) for the full version history.

---

## 📜 License

MIT — see [LICENSE](LICENSE). Free to use, modify and distribute, including the 3D enclosure.

## 🤝 Contributing

PRs and issues are welcome. If you improve the enclosure or add features, feel free to open a PR or share your remix on MakerWorld.
