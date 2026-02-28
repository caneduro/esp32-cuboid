#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <U8g2lib.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <nvs_flash.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSerifBold24pt7b.h>
#include <esp_wifi.h>
#include <esp_chip_info.h>
#include <esp_pm.h>

// ===================== HARDWARE =====================
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_SDA        19
#define OLED_SCL        20
#define BOOT_BUTTON_PIN 9
#define BUZZER_PIN      18
#define ENABLE_BUZZER   1
#define HOSTNAME        "cuboid"

// ===================== POWER =====================
#define CPU_FREQ_ACTIVE  160
#define CPU_FREQ_IDLE     80

// ===================== OBJECTS =====================
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WiFiUDP     ntpUDP;
NTPClient   timeClient(ntpUDP, "pool.ntp.org");
WebServer   server(80);
DNSServer   dnsServer;
Preferences preferences;

// ===================== FONT TABLE =====================
const GFXfont* const clockFontList[] = {
  &FreeSansBold24pt7b,
  &FreeSans24pt7b,
  &FreeSerifBold24pt7b
};
const char* clockFontNames[] = { "Sans Bold", "Sans", "Serif Bold" };

// ===================== LANGUAGE =====================
enum {
  L_SYNC, L_SETUP, L_IP, L_SAVE, L_WIFI, L_PASS, L_CITY, L_LANG,
  L_HUM, L_RESTART, L_TIME, L_TZ, L_ASK_RST, L_ALARM, L_ENABLED,
  L_PATTERN, L_MUTE, L_CAL, L_NO_EVENT, L_TOMORROW,
  L_DAYS_ACTIVE, L_PERSONAL, L_CLOCK_FONT, L_DATE_STYLE, L_THEME,
  L_THEME_DARK, L_THEME_LIGHT, L_NM_SECTION, L_WEATHER_CAL,
  L_WIFI_SYS, L_ALARM_TIME,
  L_AUTO_CLOCK,
  L_TODAY,
  L_COUNT
};

const char* langNames[] = { "English", "Italiano", "Francais", "Espanol", "Deutsch" };

const char* labels[5][L_COUNT] = {
  { "Syncing...", "SETUP MODE", "Connect to:", "SAVE", "WiFi", "Password", "City", "Language",
    "RH: ", "Restarting...", "Time", "UTC Offset", "Restart required. Proceed?", "Alarm", "Enabled",
    "Ringtone", "Mute UI", "Calendar", "No Events", "TOMORROW",
    "Active days", "Personalization", "Clock Font", "Date Style", "Theme",
    "Dark", "Light", "Night Mode", "Weather & Calendar",
    "WiFi & System", "Alarm & Time",
    "Auto clock return (min, 0=off)",
    "TODAY" },
  { "Sincronizzazione...", "MODALITA' SETUP", "Connettiti a:", "SALVA", "WiFi", "Password", "Citta", "Lingua",
    "UM: ", "Riavvio...", "Ora", "Offset UTC", "Riavvio necessario. Procedere?", "Sveglia", "Attiva",
    "Suoneria", "Muta UI", "Calendario", "Nessun Evento", "DOMANI",
    "Giorni attivi", "Personalizzazione", "Font Orologio", "Stile Data", "Tema",
    "Scuro", "Chiaro", "Modalita' Notte", "Meteo e Calendario",
    "WiFi e Sistema", "Sveglia e Ora",
    "Ritorno orologio (min, 0=off)",
    "OGGI" },
  { "Synchro...", "MODE CONFIG", "Connecter a:", "ENREGISTRER", "WiFi", "Mot de passe", "Ville", "Langue",
    "RH: ", "Redemarrage...", "Heure", "Offset UTC", "Redemarrage requis. Continuer?", "Reveil", "Active",
    "Sonnerie", "Muet", "Calendrier", "Aucun evenement", "DEMAIN",
    "Jours actifs", "Personnalisation", "Police Horloge", "Style Date", "Theme",
    "Sombre", "Clair", "Mode Nuit", "Meteo et Calendrier",
    "WiFi et Systeme", "Reveil et Heure",
    "Retour horloge (min, 0=off)",
    "AUJOURD'HUI" },
  { "Sincronizando...", "MODO SETUP", "Conectar a:", "GUARDAR", "WiFi", "Contrasena", "Ciudad", "Idioma",
    "HR: ", "Reiniciando...", "Hora", "Offset UTC", "Reinicio requerido. Proceder?", "Alarma", "Activa",
    "Tono", "Silenciar", "Calendario", "Sin Eventos", "MANANA",
    "Dias activos", "Personalizacion", "Fuente Reloj", "Estilo Fecha", "Tema",
    "Oscuro", "Claro", "Modo Noche", "Clima y Calendario",
    "WiFi y Sistema", "Alarma y Hora",
    "Retorno reloj (min, 0=off)",
    "HOY" },
  { "Synchronisieren...", "SETUP MODUS", "Verbinden mit:", "SPEICHERN", "WiFi", "Passwort", "Stadt", "Sprache",
    "FF: ", "Neustart...", "Zeit", "UTC-Offset", "Neustart erforderlich. Fortfahren?", "Wecker", "Aktiv",
    "Klingelton", "Stumm", "Kalender", "Keine Termine", "MORGEN",
    "Aktive Tage", "Personalisierung", "Uhr-Schrift", "Datumsstil", "Design",
    "Dunkel", "Hell", "Nachtmodus", "Wetter und Kalender",
    "WiFi und System", "Wecker und Zeit",
    "Auto Uhr (min, 0=off)",
    "HEUTE" }
};

const char* dayNames[5][7] = {
  { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" },
  { "Lun", "Mar", "Mer", "Gio", "Ven", "Sab", "Dom" },
  { "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam", "Dim" },
  { "Lun", "Mar", "Mie", "Jue", "Vie", "Sab", "Dom" },
  { "Mo",  "Di",  "Mi",  "Do",  "Fr",  "Sa",  "So"  }
};

const char* monthShort[5][12] = {
  { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" },
  { "Gen","Feb","Mar","Apr","Mag","Giu","Lug","Ago","Set","Ott","Nov","Dic" },
  { "Jan","Fev","Mar","Avr","Mai","Jun","Jul","Aou","Sep","Oct","Nov","Dec" },
  { "Ene","Feb","Mar","Abr","May","Jun","Jul","Ago","Sep","Oct","Nov","Dic" },
  { "Jan","Feb","Mar","Apr","Mai","Jun","Jul","Aug","Sep","Okt","Nov","Dez" }
};

// ===================== STATE =====================
int  hours, minutes, seconds, day, month, year;
int  wdayBit = 0;
int  dateFormat = 0, currentLang = 0, utcOffset = 3600;
bool autoDST = true, isAPMode = false;
int  clockFont = 0;
int           autoClockReturnMin = 5;
unsigned long modeChangedAt      = 0;
bool webThemeLight = false;
bool          nmEnabled = false;
int           nmStartH = 23, nmStartM = 0, nmEndH = 7, nmEndM = 0;
int           nmWakeTime = 5;
bool          isScreenOn = true;
unsigned long wakeScreenUntil = 0;
String weatherKey, weatherCity;
String w_temp = "--", w_hum = "--", w_desc = "...", w_cityDisplay = "", w_iconCode = "";
String w_forecast_temp = "--", w_forecast_desc = "...", w_forecast_icon = "", w_forecast_hum = "--";
String w_forecast_min = "--", w_forecast_max = "--";
String googleScriptUrl = "";
bool   hasCalendarEvent = false;
int    scrollX = 0, textWidth = 0;
unsigned long lastScrollUpdate = 0;
#define CAL_MAX_EVENTS 5
String cal_events[CAL_MAX_EVENTS];
String cal_times[CAL_MAX_EVENTS];
int    cal_eventCount      = 0;
int    cal_currentEventIdx = 0;
const unsigned long DATA_REFRESH_INTERVAL = 7200000UL;
unsigned long lastWeatherUpdate = 0, lastCalendarUpdate = 0;
enum Mode { MODE_WEATHER = 0, MODE_CLOCK = 1, MODE_CALENDAR = 2, MODE_FORECAST = 3 };
Mode currentMode = MODE_CLOCK;
int  alarmHour = 7, alarmMinute = 0, alarmPatternIdx = 0;
bool alarmEnabled = false, isAlarmRinging = false, muteUI = false;
int  alarmDays = 0b1111111;
const char* alarmPatternNames[] = { "Digital Beep", "Nervous Cricket", "Melodic Rise", "Sci-Fi Siren" };
String otaPass = "";
String otaUser = "";

// ===================== FORWARD DECLARATIONS =====================
bool checkNightMode();
void updateDisplayClock();
void updateDisplayWeather();
void updateDisplayCalendar();
void fetchWeather();
void fetchCalendar();
void refreshDisplay();

// ===================== NIGHT MODE =====================
bool checkNightMode() {
  if (!nmEnabled) return false;
  int cur   = hours * 60 + minutes;
  int start = nmStartH * 60 + nmStartM;
  int end   = nmEndH   * 60 + nmEndM;
  return (start < end) ? (cur >= start && cur < end)
                       : (cur >= start || cur < end);
}

inline bool isSoundSuppressed() { return muteUI || checkNightMode(); }

bool isAlarmActiveToday() {
  time_t et = timeClient.getEpochTime();
  struct tm* p = gmtime(&et);
  int bit = (p->tm_wday == 0) ? 6 : p->tm_wday - 1;
  return (alarmDays & (1 << bit)) != 0;
}

static void applyCpuFreq(bool screenOn) {
  setCpuFrequencyMhz(screenOn ? CPU_FREQ_ACTIVE : CPU_FREQ_IDLE);
}

static void initPowerManagement() {
  esp_pm_config_t pm = {
    .max_freq_mhz       = CPU_FREQ_ACTIVE,
    .min_freq_mhz       = CPU_FREQ_IDLE,
    .light_sleep_enable = true
  };
  esp_pm_configure(&pm);
}

static void enableWifi6() {
  esp_wifi_set_protocol(
    WIFI_IF_STA,
    WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_11AX
  );
}

// ===================== BUZZER =====================
#if ENABLE_BUZZER

struct BuzzerStep { int freq; unsigned long dur; };

class BuzzerClass {
public:
  void begin() { pinMode(BUZZER_PIN, OUTPUT); playing = false; idx = 0; loopPattern = false; }
  void stop()  { noTone(BUZZER_PIN); playing = false; loopPattern = false; }

  void playUISound(int freq, unsigned long dur) {
    if (isSoundSuppressed()) return;
    BuzzerStep p[] = { {freq, dur}, {0, 1} };
    playPattern(p, 2, false);
  }

  void playPattern(const BuzzerStep* p, uint8_t len, bool loop = false) {
    if (!p || len == 0) return;
    if (len > MAX_STEPS) len = MAX_STEPS;
    for (uint8_t i = 0; i < len; i++) steps[i] = p[i];
    total = len; idx = 0; playing = true; loopPattern = loop;
    nextChange = millis();
    if (steps[0].freq > 0) tone(BUZZER_PIN, steps[0].freq);
  }

  void update() {
    if (!playing) return;
    unsigned long now = millis();
    if (now < nextChange) return;
    if (++idx < total) {
      noTone(BUZZER_PIN);
      if (steps[idx].freq > 0) tone(BUZZER_PIN, steps[idx].freq);
      nextChange = now + steps[idx].dur;
    } else if (loopPattern) {
      idx = 0; noTone(BUZZER_PIN);
      if (steps[0].freq > 0) tone(BUZZER_PIN, steps[0].freq);
      nextChange = now + steps[0].dur;
    } else {
      noTone(BUZZER_PIN); playing = false;
    }
  }

private:
  static const uint8_t MAX_STEPS = 16;
  BuzzerStep    steps[MAX_STEPS];
  uint8_t       total = 0, idx = 0;
  bool          playing = false, loopPattern = false;
  unsigned long nextChange = 0;
};

BuzzerClass buzzer;

BuzzerStep p_classic[] = { {2000,100},{0,100},{2000,100},{0,700} };
BuzzerStep p_nervous[] = { {3000, 50},{0, 50},{3000, 50},{0,500} };
BuzzerStep p_melodic[] = { {1046,150},{1318,150},{1568,150},{2093,300},{0,500} };
BuzzerStep p_siren[]   = { {1500,150},{2000,150},{2500,150},{2000,150},{0,100} };

void startAlarmSound() {
  switch (alarmPatternIdx) {
    case 1:  buzzer.playPattern(p_nervous, 4, true); break;
    case 2:  buzzer.playPattern(p_melodic, 5, true); break;
    case 3:  buzzer.playPattern(p_siren,   5, true); break;
    default: buzzer.playPattern(p_classic, 4, true); break;
  }
}

void playSaveSound() {
  if (isSoundSuppressed()) return;
  BuzzerStep ok[] = { {880,80},{0,30},{1109,80},{0,30},{1319,200},{0,10} };
  buzzer.playPattern(ok, 6, false);
  unsigned long t = millis();
  while (millis() - t < 500) { buzzer.update(); delay(1); }
}

#else
struct BuzzerStep { int freq; unsigned long dur; };
class DummyBuzzer {
public:
  void begin() {} void stop() {}
  void playUISound(int, unsigned long) {}
  void playPattern(const BuzzerStep*, uint8_t, bool = false) {}
  void update() {}
};
DummyBuzzer buzzer;
void startAlarmSound() {}
void playSaveSound()   {}
#endif

// ===================== CALENDAR TIME LOCALIZATION =====================
String localizeCalTime(const String& raw) {
  static const char* tmrKw[] = {
    "tomorrow", "Tomorrow", "TOMORROW",
    "domani",   "Domani",   "DOMANI",
    "demain",   "Demain",   "DEMAIN",
    "manana",   "Manana",   "MANANA",
    "morgen",   "Morgen",   "MORGEN",
    nullptr
  };
  static const char* todayKw[] = {
    "today",   "Today",   "TODAY",
    "oggi",    "Oggi",    "OGGI",
    "hoy",     "Hoy",     "HOY",
    "heute",   "Heute",   "HEUTE",
    "aujourd", "Aujourd",
    nullptr
  };
  for (int i = 0; tmrKw[i] != nullptr; i++) {
    String kw(tmrKw[i]);
    if (raw.length() >= kw.length() &&
        raw.substring(0, kw.length()).equalsIgnoreCase(kw))
      return String(labels[currentLang][L_TOMORROW]) + raw.substring(kw.length());
  }
  for (int i = 0; todayKw[i] != nullptr; i++) {
    String kw(todayKw[i]);
    if (raw.length() >= kw.length() &&
        raw.substring(0, kw.length()).equalsIgnoreCase(kw)) {
      String rest = raw.substring(kw.length());
      int sp = rest.indexOf(' ');
      return String(labels[currentLang][L_TODAY]) + (sp >= 0 ? rest.substring(sp) : "");
    }
  }
  return raw;
}

// ===================== DISPLAY HELPER =====================
void refreshDisplay() {
  switch (currentMode) {
    case MODE_WEATHER:
    case MODE_FORECAST: updateDisplayWeather();  break;
    case MODE_CALENDAR: updateDisplayCalendar(); break;
    default:            updateDisplayClock();    break;
  }
}

// ===================== HTML HEAD =====================
String htmlHead(const char* page) {
  String s = "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
    "<meta charset='UTF-8'>"
    "<style>"
    ":root{"
      "--bg:#111318;--cbg:#1c1e26;--tbg:#141620;"
      "--txt:#dde1f0;--muted:#7b82a0;--acc:#a78bfa;"
      "--teal:#2dd4bf;--brd:#2e3148;--inp:#22253a;"
      "--ibox:#191c2e;--sep:#22253a;--hvr:#000;"
      "--sbtn:#4c1d95;--sbtn-h:#6d28d9;"
      "--ok:#065f46;--pill-on-bg:#2e1065;--pill-on-brd:#a78bfa;--pill-on-txt:#c4b5fd;"
      "--mute-brd:#f87171;--mute-txt:#f87171;"
      "--tag-ok:#2dd4bf;--rssi-ok:#2dd4bf;--rssi-warn:#fbbf24;--rssi-bad:#f87171;"
    "}"
    "body.lm{"
      "--bg:#f0f2f9;--cbg:#ffffff;--tbg:#ede9fe;"
      "--txt:#1e1b2e;--muted:#6b7280;--acc:#7c3aed;"
      "--teal:#0d9488;--brd:#d1d5db;--inp:#f9fafb;"
      "--ibox:#f3f4f6;--sep:#e5e7eb;--hvr:#fff;"
      "--sbtn:#7c3aed;--sbtn-h:#6d28d9;"
      "--ok:#047857;--pill-on-bg:#ede9fe;--pill-on-brd:#7c3aed;--pill-on-txt:#6d28d9;"
      "--mute-brd:#ef4444;--mute-txt:#ef4444;"
      "--tag-ok:#0d9488;--rssi-ok:#0d9488;--rssi-warn:#d97706;--rssi-bad:#dc2626;"
    "}"
    "*{box-sizing:border-box;transition:background-color .25s,color .2s,border-color .2s;}"
    "body{background:var(--bg);color:var(--txt);font-family:'Segoe UI',system-ui,sans-serif;margin:0;padding:0;}"
    ".page{padding:12px;max-width:600px;margin:0 auto;}"
    ".card{background:var(--cbg);padding:16px;border-radius:14px;margin-bottom:14px;"
      "box-shadow:0 2px 12px rgba(0,0,0,.18);border:1px solid var(--brd);}"
    ".topbar{background:var(--tbg);padding:9px 13px;border-radius:14px;margin-bottom:14px;"
      "box-shadow:0 4px 16px rgba(0,0,0,.25);display:flex;align-items:center;gap:7px;"
      "border:1px solid var(--brd);position:sticky;top:8px;z-index:99;}"
    ".topbar .brand{flex:1;font-size:15px;font-weight:900;color:var(--acc);letter-spacing:2px;"
      "white-space:nowrap;overflow:hidden;text-overflow:ellipsis;font-variant:small-caps;}"
    ".tbtn{color:var(--teal);text-decoration:none;font-size:12px;font-weight:700;padding:5px 10px;"
      "border-radius:12px;border:2px solid var(--teal);transition:background .2s,color .2s;"
      "white-space:nowrap;flex-shrink:0;background:transparent;cursor:pointer;}"
    ".tbtn:hover,.tbtn.active{background:var(--teal);color:var(--hvr);}"
    ".tbtn.mute-on{border-color:var(--mute-brd);color:var(--mute-txt);}"
    ".tbtn.mute-on:hover{background:var(--mute-brd);color:#fff;}"
    ".tbtn.theme-btn{border-color:var(--acc);color:var(--acc);font-size:15px;padding:4px 9px;}"
    ".tbtn.theme-btn:hover{background:var(--acc);color:var(--hvr);}"
    "h2{color:var(--acc);font-size:14px;font-weight:800;margin:0 0 13px;"
      "border-bottom:1px solid var(--sep);padding-bottom:8px;"
      "display:flex;align-items:center;gap:7px;letter-spacing:.5px;text-transform:uppercase;}"
    "h2 .ico{font-size:16px;}"
    "label{display:block;margin-top:10px;font-size:12px;color:var(--muted);"
      "font-weight:600;letter-spacing:.4px;text-transform:uppercase;}"
    ".lrow{display:flex;align-items:baseline;gap:8px;margin-top:10px;}"
    ".lrow span{font-size:12px;color:var(--muted);font-weight:600;letter-spacing:.4px;text-transform:uppercase;}"
    ".lrow a{font-size:11px;color:var(--teal);text-decoration:none;flex-shrink:0;}"
    ".lrow a:hover{text-decoration:underline;}"
    "select,input[type=text],input[type=number],input[type=password]{"
      "width:100%;padding:10px 12px;margin:4px 0;border-radius:9px;"
      "border:1.5px solid var(--brd);background:var(--inp);color:var(--txt);"
      "font-size:14px;outline:none;}"
    "select:focus,input:focus{border-color:var(--acc);box-shadow:0 0 0 3px rgba(167,139,250,.15);}"
    ".save-btn{width:100%;padding:13px;background:var(--sbtn);color:#fff;border:none;"
      "border-radius:9px;font-weight:800;cursor:pointer;margin-top:14px;font-size:13px;"
      "letter-spacing:.5px;text-transform:uppercase;"
      "transition:background .2s,transform .1s;display:block;text-align:center;}"
    ".save-btn:hover{background:var(--sbtn-h);}"
    ".save-btn:active{transform:scale(.98);}"
    ".save-btn.ok{background:var(--ok);}"
    ".save-btn.wait{background:#555;cursor:wait;}"
    ".row{display:flex;gap:10px;} .col{flex:1;}"
    ".wrow{display:flex;gap:8px;align-items:flex-end;} .wsel{flex:1;}"
    ".icon-btn{width:44px;height:44px;background:var(--inp);border:1.5px solid var(--brd);"
      "border-radius:9px;cursor:pointer;display:flex;justify-content:center;"
      "align-items:center;padding:0;flex-shrink:0;}"
    ".icon-btn:hover{background:var(--cbg);border-color:var(--acc);}"
    ".icon-btn svg{fill:var(--teal);width:22px;height:22px;transition:transform .5s;}"
    ".spin{animation:spin 1s linear infinite;}"
    "@keyframes spin{100%{transform:rotate(360deg);}}"
    "hr.sep{border:0;border-top:1px solid var(--sep);margin:13px 0;}"
    ".ig{display:grid;grid-template-columns:auto 1fr;gap:5px 14px;font-size:12px;}"
    ".ig .k{color:var(--muted);white-space:nowrap;font-weight:600;}"
    ".ig .v{color:var(--txt);font-family:monospace;word-break:break-all;}"
    ".bar-wrap{background:var(--ibox);border-radius:4px;height:5px;margin:4px 0 10px;overflow:hidden;}"
    ".bar{height:100%;border-radius:4px;transition:width .4s;}"
    ".ibox{background:var(--ibox);border-radius:9px;padding:10px 13px;font-size:12px;"
      "color:var(--muted);margin:10px 0 4px;line-height:1.6;border:1px solid var(--sep);}"
    ".days-row{display:flex;gap:5px;flex-wrap:wrap;margin-top:8px;}"
    ".day-pill{display:inline-flex;align-items:center;justify-content:center;"
      "width:40px;height:34px;border-radius:8px;border:2px solid var(--brd);"
      "background:var(--inp);color:var(--muted);font-size:11px;font-weight:800;"
      "cursor:pointer;user-select:none;transition:all .15s;letter-spacing:.3px;}"
    ".day-pill.on{border-color:var(--pill-on-brd);background:var(--pill-on-bg);color:var(--pill-on-txt);}"
    ".day-pill:hover{border-color:var(--acc);}"
    ".font-preview{display:flex;gap:8px;flex-wrap:wrap;margin-top:8px;}"
    ".fp{padding:6px 12px;border-radius:8px;border:2px solid var(--brd);"
      "background:var(--inp);color:var(--muted);font-size:13px;cursor:pointer;"
      "user-select:none;transition:all .15s;}"
    ".fp.on{border-color:var(--pill-on-brd);background:var(--pill-on-bg);color:var(--pill-on-txt);font-weight:700;}"
    ".fp:hover{border-color:var(--acc);}"
    "</style></head>";

  s += webThemeLight ? "<body class='lm'>" : "<body>";
  s += "<div class='page'>";
  s += "<div class='topbar'><span class='brand'>cuboid</span>";
  s += String("<a href='/' class='tbtn") + (strcmp(page,"main")==0    ? " active" : "") + "'> &#9881; Config</a>";
  s += String("<a href='/advanced' class='tbtn") + (strcmp(page,"advanced")==0 ? " active" : "") + "'> &#128296; Advanced</a>";
  s += String("<button class='tbtn theme-btn' onclick='toggleTheme()'>")
       + (webThemeLight ? "&#127769;" : "&#9728;") + "</button>";
#if ENABLE_BUZZER
  s += String("<a href='/toggleMute' class='tbtn") + (muteUI ? " mute-on" : "") + "'>";
  s += muteUI ? "&#128263;" : "&#128266;";
  s += "</a>";
#endif
  s += "</div>";
  return s;
}

// ===================== AJAX JS =====================
// NOTE: scanWifi() function is split: the opening part lives here in PROGMEM,
// the connected-SSID injection + closing brace is appended inline in handleRoot().
// The function also contains an early guard (if(!sel)return) so it is safe to
// call on the Advanced page where the #ssid element does not exist.
const char AJAX_JS[] PROGMEM = R"js(
<script>
function toggleTheme(){
  document.body.classList.toggle('lm');
  const isLight = document.body.classList.contains('lm');
  document.querySelector('.theme-btn').textContent = isLight ? '\u{1F319}' : '\u2728';
  fetch('/toggleTheme');
}

document.addEventListener('DOMContentLoaded',()=>{
  document.querySelectorAll('.day-pill').forEach(pill=>{
    pill.addEventListener('click',()=>{
      const inp = document.getElementById('day_'+pill.dataset.day);
      if(inp.value==='1'){ inp.value='0'; pill.classList.remove('on'); }
      else               { inp.value='1'; pill.classList.add('on'); }
    });
  });

  document.querySelectorAll('.fp').forEach(fp=>{
    fp.addEventListener('click',()=>{
      document.querySelectorAll('.fp').forEach(x=>x.classList.remove('on'));
      fp.classList.add('on');
      document.getElementById('cfont_inp').value = fp.dataset.font;
    });
  });

  document.querySelectorAll('form.aform').forEach(f=>{
    const isRestart = f.dataset.restart !== undefined;
    f.addEventListener('submit', async e=>{
      e.preventDefault();
      const btn = f.querySelector('.save-btn');
      const orig = btn.textContent;
      btn.textContent='...'; btn.classList.add('wait'); btn.disabled=true;
      try {
        const res = await fetch(f.action,{method:'POST',body:new URLSearchParams(new FormData(f))});
        if(res.status===403){
          btn.textContent='Wrong password'; btn.classList.remove('wait');
          setTimeout(()=>{btn.textContent=orig;btn.disabled=false;},2500); return;
        }
        if(isRestart){
          let s=8;
          const t=()=>{btn.textContent='Reboot '+s+'s';if(--s>0)setTimeout(t,1000);else location.reload();};
          btn.classList.remove('wait'); t();
        } else {
          btn.textContent='Saved \u2713'; btn.classList.remove('wait'); btn.classList.add('ok');
          setTimeout(()=>{btn.textContent=orig;btn.classList.remove('ok');btn.disabled=false;},2000);
        }
      } catch(_){
        btn.textContent='Error'; btn.classList.remove('wait');
        setTimeout(()=>{btn.textContent=orig;btn.disabled=false;},2000);
      }
    });
  });
});
function scanWifi(){
  const ic=document.getElementById('scIco');
  const sel=document.getElementById('ssid');
  if(!sel||!ic) return;
  ic.classList.add('spin');
  fetch('/scan').then(r=>r.json()).then(d=>{
    sel.innerHTML='';
)js";

// ===================== /scan =====================
void handleScan() {
  int n = WiFi.scanNetworks();
  String j = "[";
  for (int i = 0; i < n; ++i) {
    if (i > 0) j += ",";
    j += "{\"s\":\"" + WiFi.SSID(i) + "\",\"r\":" + WiFi.RSSI(i) + ",\"b\":\"" + WiFi.BSSIDstr(i) + "\"}";
  }
  j += "]";
  server.send(200, "application/json", j);
}

// ===================== /sysinfo =====================
void handleSysInfo() {
  JsonDocument doc;
  doc["model"]         = ESP.getChipModel();
  doc["rev"]           = ESP.getChipRevision();
  doc["cpu_mhz"]       = ESP.getCpuFreqMHz();
  doc["sdk"]           = ESP.getSdkVersion();
  doc["mac"]           = WiFi.macAddress();
  doc["flash_kb"]      = (int)(ESP.getFlashChipSize() / 1024);
  doc["sketch_kb"]     = (int)(ESP.getSketchSize() / 1024);
  doc["free_sketch_kb"]= (int)(ESP.getFreeSketchSpace() / 1024);
  doc["heap_kb"]       = (int)(ESP.getHeapSize() / 1024);
  doc["heap_free_kb"]  = (int)(ESP.getFreeHeap() / 1024);
  doc["heap_min_kb"]   = (int)(ESP.getMinFreeHeap() / 1024);
  doc["psram_kb"]      = (int)(ESP.getPsramSize() / 1024);
  doc["temp_c"]        = (int)temperatureRead();
  doc["ota_pass_set"]  = (otaPass.length() > 0);
  doc["ota_user_set"]  = (otaUser.length() > 0);
  unsigned long up = millis() / 1000;
  char upbuf[32];
  snprintf(upbuf, sizeof(upbuf), "%lud %02lu:%02lu:%02lu",
    up/86400, (up%86400)/3600, (up%3600)/60, up%60);
  doc["uptime"] = upbuf;
  doc["wifi_ok"] = (WiFi.status() == WL_CONNECTED);
  if (WiFi.status() == WL_CONNECTED) {
    doc["ssid"]  = WiFi.SSID();
    doc["bssid"] = WiFi.BSSIDstr();
    doc["ip"]    = WiFi.localIP().toString();
    doc["gw"]    = WiFi.gatewayIP().toString();
    doc["mask"]  = WiFi.subnetMask().toString();
    doc["dns"]   = WiFi.dnsIP().toString();
    doc["ch"]    = WiFi.channel();
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
      doc["rssi"] = ap.rssi;
      doc["auth"] = (int)ap.authmode;
      doc["b"]    = (bool)ap.phy_11b;
      doc["g"]    = (bool)ap.phy_11g;
      doc["n"]    = (bool)ap.phy_11n;
      doc["ax"]   = (bool)ap.phy_11ax;
      doc["pch"]  = ap.primary;
      const char* bw = "20 MHz";
      if      (ap.second == WIFI_SECOND_CHAN_ABOVE) bw = "40 MHz (+)";
      else if (ap.second == WIFI_SECOND_CHAN_BELOW) bw = "40 MHz (-)";
      doc["bw"]       = bw;
      int fmhz = (ap.primary <= 14) ? (2407 + ap.primary * 5) : (5000 + ap.primary * 5);
      doc["freq_mhz"] = fmhz;
    }
  }
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// ===================== / (main config page) =====================
void handleRoot() {
  String s = htmlHead("main");
  s += FPSTR(AJAX_JS);

  // ---- Complete the scanWifi() function (started in PROGMEM above) ----
  // Inject the currently connected SSID as first option after clearing the list,
  // then close the then()-callback and the function itself.
  // FIX BUG 2: there is NO blocking WiFi.scanNetworks() call here anymore.
  // The full network list is fetched asynchronously via fetch('/scan').
  // We also auto-trigger scanWifi() on DOMContentLoaded so the list populates
  // automatically without the user having to click the Rescan button.
  if (WiFi.status() == WL_CONNECTED) {
    String cur = WiFi.SSID();
    if (cur.length() > 0)
      s += "var o=document.createElement('option');o.value='" + cur
           + "';o.text='" + cur + " (Connected)';o.selected=true;sel.add(o);";
  }
  s += "d.forEach(n=>{var o=document.createElement('option');"
       "o.value=n.s;o.text=n.s+' ('+n.r+'dBm) ['+n.b+']';sel.add(o);});"
       "ic.classList.remove('spin');"
       "}).catch(()=>ic.classList.remove('spin'));}"          // closes scanWifi()
       "document.addEventListener('DOMContentLoaded',scanWifi);"  // auto-scan on load
       "</script>";

  float utcHours = utcOffset / 3600.0f;
  char dd_ex[3]; snprintf(dd_ex, 3, "%02d", day);
  String fmt3label = String(dayNames[currentLang][wdayBit]) + " " + dd_ex
                   + " " + monthShort[currentLang][month-1] + " " + String(year);
  String fmt4label = String(dayNames[currentLang][wdayBit]) + " " + dd_ex
                   + " " + monthShort[currentLang][month-1];

  // ---- 1. Alarm & Time ----
  // FIX BONUS: when ENABLE_BUZZER==0 there is no alarm UI, so call the card just "Time"
#if ENABLE_BUZZER
  const char* card1Title = labels[currentLang][L_ALARM_TIME];
#else
  const char* card1Title = labels[currentLang][L_TIME];
#endif

  s += "<div class='card'>"
       "<h2><span class='ico'>&#9200;</span> " + String(card1Title) + "</h2>"
       "<form class='aform' action='/saveTimeAlarm' method='POST'>";

#if ENABLE_BUZZER
  s += "<div class='row'>"
       "<div class='col'><label>" + String(labels[currentLang][L_ENABLED]) + "</label>"
       "<select name='ena'>"
       "<option value='1'" + String(alarmEnabled  ? " selected" : "") + ">ON</option>"
       "<option value='0'" + String(!alarmEnabled ? " selected" : "") + ">OFF</option>"
       "</select></div>"
       "<div class='col'><label>HH : MM</label>"
       "<div style='display:flex;gap:5px;'>"
       "<input type='number' name='h' min='0' max='23' value='" + String(alarmHour)   + "'>"
       "<input type='number' name='m' min='0' max='59' value='" + String(alarmMinute) + "'>"
       "</div></div></div>";

  s += "<label>" + String(labels[currentLang][L_DAYS_ACTIVE]) + "</label>"
       "<div class='days-row'>";
  for (int i = 0; i < 7; i++) {
    bool active = (alarmDays & (1 << i)) != 0;
    s += "<div class='day-pill" + String(active ? " on" : "")
         + "' data-day='" + String(i) + "'>"
         + String(dayNames[currentLang][i]) + "</div>"
         + "<input type='hidden' id='day_" + String(i)
         + "' name='d" + String(i) + "' value='" + String(active ? "1" : "0") + "'>";
  }
  s += "</div>";

  s += "<label>" + String(labels[currentLang][L_PATTERN]) + "</label>"
       "<select name='pat'>";
  for (int i = 0; i < 4; i++)
    s += "<option value='" + String(i) + "'" + (alarmPatternIdx==i?" selected":"") + ">"
         + alarmPatternNames[i] + "</option>";
  s += "</select><hr class='sep'>";
#endif

  s += "<div class='row'>"
       "<div class='col'><label>" + String(labels[currentLang][L_TZ]) + " (h)</label>"
       "<input type='number' name='offset' step='0.5' min='-12' max='14' value='"
       + String(utcHours, 1) + "'></div>"
       "<div class='col'><label>Auto DST</label>"
       "<select name='dst'>"
       "<option value='1'" + String(autoDST  ? " selected" : "") + ">ON</option>"
       "<option value='0'" + String(!autoDST ? " selected" : "") + ">OFF</option>"
       "</select></div></div>"
       "<button class='save-btn' type='submit'>" + labels[currentLang][L_SAVE] + "</button>"
       "</form></div>";   // <-- card 1 properly closed

  // ---- 2. Personalizzazione ----
  s += "<div class='card'>"
       "<h2><span class='ico'>&#127912;</span> " + String(labels[currentLang][L_PERSONAL]) + "</h2>"
       "<form class='aform' action='/savePersonalization' method='POST'>";

  s += "<label>" + String(labels[currentLang][L_CLOCK_FONT]) + "</label>"
       "<div class='font-preview'>";
  for (int i = 0; i < 3; i++) {
    s += "<div class='fp" + String(clockFont==i?" on":"")
         + "' data-font='" + String(i) + "'>" + clockFontNames[i] + "</div>";
  }
  s += "</div>"
       "<input type='hidden' id='cfont_inp' name='cfont' value='" + String(clockFont) + "'>";

  s += "<label>" + String(labels[currentLang][L_DATE_STYLE]) + "</label>"
       "<select name='df'>"
       "<option value='0'" + String(dateFormat==0?" selected":"") + ">DD/MM/YYYY</option>"
       "<option value='1'" + String(dateFormat==1?" selected":"") + ">MM/DD/YYYY</option>"
       "<option value='2'" + String(dateFormat==2?" selected":"") + ">YYYY/MM/DD</option>"
       "<option value='3'" + String(dateFormat==3?" selected":"") + ">" + fmt3label + "</option>"
       "<option value='4'" + String(dateFormat==4?" selected":"") + ">" + fmt4label + "</option>"
       "</select>";

  s += "<label>" + String(labels[currentLang][L_AUTO_CLOCK]) + "</label>"
       "<input type='number' name='acr' min='0' max='120' value='" + String(autoClockReturnMin) + "'>"
       "<div class='ibox'>";
  if      (currentLang == 1) s += "Quando il display e' su Meteo o Calendario, torna automaticamente all'orologio dopo questo numero di minuti di inattivita'. Impostare 0 per disabilitare.";
  else if (currentLang == 2) s += "Quand l'ecran est sur Meteo ou Calendrier, il revient automatiquement a l'horloge apres ce nombre de minutes d'inactivite. 0 = desactive.";
  else if (currentLang == 3) s += "Cuando la pantalla esta en Clima o Calendario, vuelve automaticamente al reloj tras este numero de minutos. 0 = desactivado.";
  else if (currentLang == 4) s += "Wenn die Anzeige auf Wetter oder Kalender steht, kehrt sie nach dieser Anzahl Minuten zur Uhr zuruck. 0 = deaktiviert.";
  else                       s += "When the display is on Weather or Calendar, it automatically returns to the clock screen after this many minutes of inactivity. Set to 0 to disable.";
  s += "</div>";

  // FIX BUG 1: original code had  String("<button...") + ...  which is a discarded
  // expression — the button, </form> and </div> were never appended to s, leaving
  // the card open and swallowing all subsequent cards as nested content.
  s += "<button class='save-btn' type='submit'>" + String(labels[currentLang][L_SAVE]) + "</button>"
       "</form></div>";   // <-- card 2 properly closed

  // ---- 3. Night Mode ----
  s += "<div class='card'>"
       "<h2><span class='ico'>&#127769;</span> " + String(labels[currentLang][L_NM_SECTION]) + "</h2>"
       "<form class='aform' action='/saveNightMode' method='POST'>"
       "<label>" + String(labels[currentLang][L_ENABLED]) + "</label>"
       "<select name='nm_en'>"
       "<option value='1'" + String(nmEnabled  ? " selected" : "") + ">ON</option>"
       "<option value='0'" + String(!nmEnabled ? " selected" : "") + ">OFF</option>"
       "</select>"
       "<div class='row'>"
       "<div class='col'><label>Start HH:MM</label>"
       "<div style='display:flex;gap:5px;'>"
       "<input type='number' name='nm_sh' min='0' max='23' value='" + String(nmStartH) + "'>"
       "<input type='number' name='nm_sm' min='0' max='59' value='" + String(nmStartM) + "'>"
       "</div></div>"
       "<div class='col'><label>End HH:MM</label>"
       "<div style='display:flex;gap:5px;'>"
       "<input type='number' name='nm_eh' min='0' max='23' value='" + String(nmEndH) + "'>"
       "<input type='number' name='nm_em' min='0' max='59' value='" + String(nmEndM) + "'>"
       "</div></div></div>"
       "<label>Wake (s)</label>"
       "<input type='number' name='nm_wt' min='1' max='60' value='" + String(nmWakeTime) + "'>"
       "<button class='save-btn' type='submit'>" + labels[currentLang][L_SAVE] + "</button>"
       "</form></div>";

  // ---- 4. Weather & Calendar ----
  s += "<div class='card'>"
       "<h2><span class='ico'>&#9925;</span> " + String(labels[currentLang][L_WEATHER_CAL]) + "</h2>"
       "<form class='aform' action='/saveAPI' method='POST'>"
       "<div class='lrow'><span>OpenWeatherMap API Key</span>"
       "<a href='https://home.openweathermap.org/api_keys' target='_blank'>&#8599; Get key</a></div>"
       "<input type='text' name='w_key' value='" + weatherKey + "'>"
       "<label>" + String(labels[currentLang][L_CITY]) + "</label>"
       "<input type='text' name='w_city' value='" + weatherCity + "'>"
       "<div class='lrow'><span>Google Script URL</span>"
       "<a href='https://script.google.com/home/' target='_blank'>&#8599; Apps Script</a></div>"
       "<input type='text' name='g_url' placeholder='https://script.google.com/macros/s/...' value='" + googleScriptUrl + "'>"
       "<button class='save-btn' type='submit'>" + labels[currentLang][L_SAVE] + "</button>"
       "</form></div>";

  // ---- 5. WiFi & System ----
  // FIX BUG 2: no blocking WiFi.scanNetworks() — select starts with current SSID only;
  // the async scanWifi() call (auto-triggered via DOMContentLoaded above) fills the rest.
  s += "<div class='card'>"
       "<h2><span class='ico'>&#128225;</span> " + String(labels[currentLang][L_WIFI_SYS]) + "</h2>"
       "<form class='aform' action='/saveSystem' method='POST' data-restart>"
       "<label>" + String(labels[currentLang][L_WIFI]) + " SSID</label>"
       "<div class='wrow'>"
       "<select name='ssid' id='ssid' class='wsel'>";
  {
    String cur = WiFi.SSID();
    if (WiFi.status() == WL_CONNECTED && cur.length() > 0)
      s += "<option value='" + cur + "' selected>" + cur + " (Connected)</option>";
    // No blocking scan here — scanWifi() will populate the list asynchronously
  }
  s += "</select>"
       "<button type='button' class='icon-btn' onclick='scanWifi()' title='Rescan'>"
       "<svg id='scIco' viewBox='0 0 24 24'>"
       "<path d='M17.65 6.35C16.2 4.9 14.21 4 12 4c-4.42 0-7.99 3.58-7.99 8s3.57 8 7.99 8"
       "c3.73 0 6.84-2.55 7.73-6h-2.08c-.82 2.33-3.04 4-5.65 4-3.31 0-6-2.69-6-6s2.69-6 6-6"
       "c1.66 0 3.14.69 4.22 1.78L13 11h7V4l-2.35 2.35z'/>"
       "</svg></button></div>"
       "<label>" + String(labels[currentLang][L_PASS]) + "</label>"
       "<input type='password' name='pass' placeholder='Leave empty to keep current'>"
       "<label>" + String(labels[currentLang][L_LANG]) + "</label>"
       "<select name='lng'>";
  for (int i = 0; i < 5; i++)
    s += "<option value='" + String(i) + "'" + (currentLang==i?" selected":"") + ">"
         + langNames[i] + "</option>";
  s += "</select>"
       "<button class='save-btn' type='submit'>" + String(labels[currentLang][L_SAVE]) + " &amp; Reboot</button>"
       "</form></div>";

  s += "</div></body></html>";
  server.send(200, "text/html", s);
}

// ===================== /advanced =====================
void handleAdvanced() {
  String s = htmlHead("advanced");
  s += FPSTR(AJAX_JS);
  s += "}).catch(()=>{}); }</script>";

  s += "<div class='card'>"
       "<h2><span class='ico'>&#128295;</span> Firmware Update</h2>"
       "<p style='font-size:13px;color:var(--muted);margin:0 0 10px;'>Flash a .bin file directly from the browser:</p>"
       "<form method='POST' action='/update' enctype='multipart/form-data'>"
       "<input type='file' name='update' accept='.bin' "
       "style='background:transparent;padding:0;border:none;color:var(--txt);width:100%;'>"
       "<input type='submit' value='Flash Firmware' class='save-btn' style='margin-top:14px;cursor:pointer;'>"
       "</form>"
       "<hr class='sep'>"
       "<p style='font-size:12px;color:var(--muted);margin:8px 0 2px;'>"
       "Arduino IDE OTA: device visible as <b>cuboid.local</b> on port 3232.</p>";

  if (otaPass.length() == 0 && otaUser.length() == 0) {
    s += "<form class='aform' action='/saveOTAPass' method='POST' data-restart>"
         "<label>OTA Username</label>"
         "<input type='text' name='new_user' autocomplete='username'>"
         "<label>OTA Password</label>"
         "<input type='password' name='new_pass' autocomplete='new-password'>"
         "<button class='save-btn' type='submit'>Set &amp; Reboot</button>"
         "</form>";
  } else {
    s += "<p style='font-size:12px;color:var(--teal);margin:6px 0 2px;'>&#128274; OTA credentials active.</p>"
         "<form class='aform' action='/saveOTAPass' method='POST' data-restart>"
         "<label>Current Password</label>"
         "<input type='password' name='cur_pass' autocomplete='current-password'>"
         "<label>New Username</label>"
         "<input type='text' name='new_user' autocomplete='username' value='" + otaUser + "'>"
         "<label>New Password <span style='color:var(--muted);font-size:10px;'>(empty = remove)</span></label>"
         "<input type='password' name='new_pass' autocomplete='new-password'>"
         "<button class='save-btn' type='submit'>Update &amp; Reboot</button>"
         "</form>";
  }
  s += "</div>";

  s += "<div class='card'>"
       "<h2><span class='ico'>&#128161;</span> System Info "
       "<span id='rtag' style='font-size:10px;color:var(--muted);font-weight:normal;margin-left:6px;'>loading...</span></h2>"
       "<div id='si'><p style='color:var(--muted);font-size:12px;'>-</p></div></div>";

  s += "<div class='card'>"
       "<h2><span class='ico'>&#128246;</span> WiFi Details</h2>"
       "<div id='wi'><p style='color:var(--muted);font-size:12px;'>-</p></div></div>";

  s += R"rawjs(
<script>
const AUTH=['Open','WEP','WPA-PSK','WPA2-PSK','WPA/WPA2','WPA2-Ent','WPA3-PSK','WPA2/WPA3','WAPI','OWE','WPA3-Ent'];
function authName(n){return AUTH[n]||'Unknown ('+n+')';}
function kv(k,v){return '<div class="k">'+k+'</div><div class="v">'+v+'</div>';}
function yn(b){return b?'<span style="color:var(--teal)">Yes</span>':'<span style="color:var(--muted)">No</span>';}
function pct(f,t){return t>0?Math.round((1-f/t)*100):0;}
function bar(p){var c=p>80?'var(--rssi-bad)':p>55?'var(--rssi-warn)':'var(--rssi-ok)';
  return '<div class="bar-wrap"><div class="bar" style="width:'+p+'%;background:'+c+'"></div></div>';}
async function poll(){
  try{
    const d=await fetch('/sysinfo').then(r=>r.json());
    document.getElementById('rtag').textContent='live';
    var hp=pct(d.heap_free_kb,d.heap_kb);
    document.getElementById('si').innerHTML=
      '<div class="ig">'+
      kv('Chip',d.model+' rev '+d.rev)+kv('CPU',d.cpu_mhz+' MHz')+kv('SDK',d.sdk)+
      kv('MAC',d.mac)+kv('Temp',d.temp_c+'\u00b0C')+kv('Uptime',d.uptime)+
      kv('OTA user',d.ota_user_set?'Set':'Not set')+
      kv('OTA pass',d.ota_pass_set?'Set':'Not set')+
      kv('Flash',d.flash_kb+' KB')+kv('Sketch',d.sketch_kb+' KB / free '+d.free_sketch_kb+' KB')+
      kv('Heap',d.heap_free_kb+' KB free / '+d.heap_kb+' KB ('+hp+'% used)')+
      (d.psram_kb>0?kv('PSRAM',d.psram_kb+' KB'):'')+
      kv('Heap min',d.heap_min_kb+' KB')+
      '</div>'+bar(hp);
    if(d.wifi_ok){
      document.getElementById('wi').innerHTML=
        '<div class="ig">'+
        kv('SSID',d.ssid)+kv('BSSID',d.bssid)+kv('IP',d.ip)+
        kv('Gateway',d.gw)+kv('Subnet',d.mask)+kv('DNS',d.dns)+
        kv('Channel',d.pch+' ('+d.freq_mhz+' MHz)')+kv('Bandwidth',d.bw)+
        kv('RSSI',d.rssi+' dBm')+kv('Security',authName(d.auth))+
        kv('802.11b',yn(d.b))+kv('802.11g',yn(d.g))+kv('802.11n (HT)',yn(d.n))+
        kv('Wi-Fi 6 (ax)',yn(d.ax))+'</div>';
    } else {
      document.getElementById('wi').innerHTML='<p style="color:var(--rssi-bad);font-size:12px;">Not connected</p>';
    }
  } catch(e){ document.getElementById('rtag').textContent='error'; }
}
poll(); setInterval(poll,4000);
</script>
)rawjs";

  s += "</div></body></html>";
  server.send(200, "text/html", s);
}

// ===================== Form handlers =====================
void handleToggleMute() {
#if ENABLE_BUZZER
  muteUI = !muteUI;
  preferences.begin("clock_cfg", false);
  preferences.putBool("mute", muteUI);
  preferences.end();
#endif
  server.sendHeader("Location", "/");
  server.send(303);
  yield();
}

void handleToggleTheme() {
  webThemeLight = !webThemeLight;
  preferences.begin("clock_cfg", false);
  preferences.putBool("wtheme", webThemeLight);
  preferences.end();
  server.send(200, "text/plain", "OK");
  yield();
}

void handleSaveTimeAlarm() {
  preferences.begin("clock_cfg", false);
#if ENABLE_BUZZER
  alarmHour       = server.arg("h").toInt();
  alarmMinute     = server.arg("m").toInt();
  alarmEnabled    = server.arg("ena").toInt();
  alarmPatternIdx = server.arg("pat").toInt();
  alarmDays = 0;
  for (int i = 0; i < 7; i++) {
    if (server.arg("d" + String(i)) == "1") alarmDays |= (1 << i);
  }
  preferences.putInt("ah",    alarmHour);
  preferences.putInt("am",    alarmMinute);
  preferences.putBool("ae",   alarmEnabled);
  preferences.putInt("ap",    alarmPatternIdx);
  preferences.putInt("adays", alarmDays);
#endif
  utcOffset = (int)(server.arg("offset").toFloat() * 3600.0f);
  autoDST   = server.arg("dst").toInt();
  preferences.putInt("offset", utcOffset);
  preferences.putInt("dst",    autoDST);
  preferences.end();
  timeClient.setTimeOffset(utcOffset);
  server.send(200, "text/plain", "OK");
  playSaveSound();
  yield();
}

void handleSavePersonalization() {
  clockFont  = server.arg("cfont").toInt();
  dateFormat = server.arg("df").toInt();
  if (clockFont  < 0 || clockFont  > 2) clockFont  = 0;
  if (dateFormat < 0 || dateFormat > 4) dateFormat = 0;
  int acr = server.arg("acr").toInt();
  if (acr < 0) acr = 0;
  autoClockReturnMin = acr;
  modeChangedAt = millis();
  preferences.begin("clock_cfg", false);
  preferences.putInt("cfont", clockFont);
  preferences.putInt("df",    dateFormat);
  preferences.putInt("acr",   autoClockReturnMin);
  preferences.end();
  server.send(200, "text/plain", "OK");
  playSaveSound();
  refreshDisplay();
  yield();
}

void handleSaveAPI() {
  weatherKey      = server.arg("w_key");
  weatherCity     = server.arg("w_city");
  googleScriptUrl = server.arg("g_url");
  preferences.begin("clock_cfg", false);
  preferences.putString("key",   weatherKey);
  preferences.putString("city",  weatherCity);
  preferences.putString("g_url", googleScriptUrl);
  preferences.end();
  if (weatherKey      != "") fetchWeather();
  if (googleScriptUrl != "") fetchCalendar();
  if ((currentMode == MODE_WEATHER || currentMode == MODE_FORECAST) && weatherKey == "")
    currentMode = MODE_CLOCK;
  if (currentMode == MODE_CALENDAR && googleScriptUrl == "")
    currentMode = MODE_CLOCK;
  server.send(200, "text/plain", "OK");
  playSaveSound();
  yield();
}

void handleSaveSystem() {
  String newSSID = server.arg("ssid");
  String newPass = server.arg("pass");
  preferences.begin("clock_cfg", false);
  if (newPass.length() > 0) {
    preferences.putString("ssid", newSSID);
    preferences.putString("pass", newPass);
  }
  preferences.putInt("lng", server.arg("lng").toInt());
  preferences.end();
  server.send(200, "text/plain", "OK");
  playSaveSound();
  delay(500);
  ESP.restart();
}

void handleSaveNightMode() {
  preferences.begin("clock_cfg", false);
  nmEnabled  = server.arg("nm_en").toInt();
  nmStartH   = server.arg("nm_sh").toInt(); nmStartM = server.arg("nm_sm").toInt();
  nmEndH     = server.arg("nm_eh").toInt(); nmEndM   = server.arg("nm_em").toInt();
  nmWakeTime = server.arg("nm_wt").toInt();
  preferences.putBool("nm_en", nmEnabled);
  preferences.putInt("nm_sh", nmStartH); preferences.putInt("nm_sm", nmStartM);
  preferences.putInt("nm_eh", nmEndH);   preferences.putInt("nm_em", nmEndM);
  preferences.putInt("nm_wt", nmWakeTime);
  preferences.end();
  server.send(200, "text/plain", "OK");
#if ENABLE_BUZZER
  playSaveSound();
#endif
  yield();
}

void handleSaveOTAPass() {
  if (otaPass.length() > 0) {
    if (server.arg("cur_pass") != otaPass) {
      server.send(403, "text/plain", "Wrong password");
      return;
    }
  }
  String newUser = server.arg("new_user");
  String newPass = server.arg("new_pass");
  if (newUser.length() > 0) otaUser = newUser;
  otaPass = newPass;
  preferences.begin("clock_cfg", false);
  preferences.putString("ota_pass", otaPass);
  preferences.putString("ota_user", otaUser);
  preferences.end();
  server.send(200, "text/plain", "OK");
  delay(500);
  ESP.restart();
}

// ===================== DATA FETCH =====================
void fetchWeather() {
  if (weatherKey == "" || WiFi.status() != WL_CONNECTED) return;
  const char* codes[] = { "en","it","fr","es","de" };
  WiFiClient client; HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" + weatherCity
             + "&appid=" + weatherKey + "&units=metric&lang=" + codes[currentLang];
  if (http.begin(client, url)) {
    if (http.GET() == 200) {
      JsonDocument doc; deserializeJson(doc, http.getString());
      w_temp        = String((float)doc["main"]["temp"], 1) + "C";
      w_hum         = String((int)doc["main"]["humidity"]);
      w_desc        = (const char*)doc["weather"][0]["description"];
      w_cityDisplay = (const char*)doc["name"];
      w_iconCode    = (const char*)doc["weather"][0]["icon"];
    }
    http.end();
  }
  yield();
  String urlF = "http://api.openweathermap.org/data/2.5/forecast?q=" + weatherCity
              + "&appid=" + weatherKey + "&units=metric&cnt=9&lang=" + codes[currentLang];
  if (http.begin(client, urlF)) {
    if (http.GET() == 200) {
      JsonDocument doc; deserializeJson(doc, http.getString());
      JsonObject   slot = doc["list"][8];
      if (!slot.isNull()) {
        w_forecast_temp = String((float)slot["main"]["temp"], 1) + "C";
        w_forecast_desc = (const char*)slot["weather"][0]["description"];
        w_forecast_icon = (const char*)slot["weather"][0]["icon"];
        w_forecast_hum  = String((int)slot["main"]["humidity"]);
      }
      float minT = 100.f, maxT = -100.f;
      for (JsonVariant v : doc["list"].as<JsonArray>()) {
        float t = v["main"]["temp"];
        if (t < minT) minT = t;
        if (t > maxT) maxT = t;
      }
      w_forecast_min = String(minT, 0);
      w_forecast_max = String(maxT, 0);
    }
    http.end();
  }
}

void fetchCalendar() {
  if (googleScriptUrl == "" || WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (http.begin(client, googleScriptUrl)) {
    if (http.GET() == 200) {
      JsonDocument doc;
      if (!deserializeJson(doc, http.getString())) {
        cal_eventCount      = 0;
        cal_currentEventIdx = 0;
        hasCalendarEvent    = doc["hasEvent"];
        if (hasCalendarEvent) {
          if (doc["events"].is<JsonArray>()) {
            for (JsonVariant ev : doc["events"].as<JsonArray>()) {
              if (cal_eventCount >= CAL_MAX_EVENTS) break;
              cal_events[cal_eventCount] = (const char*)ev["event"];
              cal_times[cal_eventCount]  = (const char*)ev["time"];
              cal_eventCount++;
            }
          } else {
            cal_events[0] = (const char*)doc["event"];
            cal_times[0]  = (const char*)doc["time"];
            cal_eventCount = 1;
          }
          if (cal_eventCount == 0) hasCalendarEvent = false;
          scrollX = 0; textWidth = 0;
        }
      }
    }
    http.end();
  }
  yield();
}

// ===================== DISPLAY =====================
void updateDisplayClock() {
  display.clearDisplay(); display.setTextColor(WHITE);
  display.setFont(clockFontList[clockFont]);
  String t = (hours < 10 ? "0" : "") + String(hours)
           + ":" + (minutes < 10 ? "0" : "") + String(minutes);
  int16_t x1,y1; uint16_t w,h;
  display.getTextBounds(t, 0, 0, &x1, &y1, &w, &h);
  int off = minutes % 2;
  display.setCursor(((SCREEN_WIDTH - w) / 2) + off, 40 + off);
  display.print(t);
  display.drawLine(0, 48, 128, 48, WHITE);
  display.setFont(); display.setTextSize(1);
  char dd[3], mm_[3]; String yy = String(year);
  snprintf(dd, 3, "%02d", day); snprintf(mm_, 3, "%02d", month);
  String dateS;
  if      (dateFormat == 1) dateS = String(mm_) + "/" + dd + "/" + yy;
  else if (dateFormat == 2) dateS = yy + "/" + mm_ + "/" + dd;
  else if (dateFormat == 3) {
    dateS = String(dayNames[currentLang][wdayBit]) + " "
           + dd + " " + monthShort[currentLang][month-1] + " " + yy;
  } else if (dateFormat == 4) {
    dateS = String(dayNames[currentLang][wdayBit]) + " "
           + dd + " " + monthShort[currentLang][month-1];
  } else {
    dateS = String(dd) + "/" + mm_ + "/" + yy;
  }
  display.getTextBounds(dateS, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 53);
  display.print(dateS);
#if ENABLE_BUZZER
  if (alarmEnabled) {
    time_t tomorrow = timeClient.getEpochTime() + 86400UL;
    struct tm* tp = gmtime(&tomorrow);
    int tomorrowBit = (tp->tm_wday == 0) ? 6 : tp->tm_wday - 1;
    bool ringsTomorrow = (alarmDays & (1 << tomorrowBit)) != 0;
    if (ringsTomorrow) display.fillCircle(120, 56, 3, WHITE);
    else               display.drawCircle(120, 56, 3, WHITE);
  }
#endif
  if (hasCalendarEvent) {
    display.drawRect(5, 52, 8, 8, WHITE);
    display.drawLine(5, 54, 13, 54, WHITE);
    display.drawPixel(9, 57, WHITE);
  }
  display.display(); yield();
}

void drawWeatherIcon(const String& code, int x, int y, bool large) {
  int g = 64;
  if (code.length() >= 2) {
    String id = code.substring(0, 2);
    if      (id=="01")           g = 69;
    else if (id=="02"||id=="03") g = 65;
    else if (id=="09"||id=="10") g = 67;
    else if (id=="11")           g = 66;
    else if (id=="13")           g = 73;
  }
  u8g2.setFont(large ? u8g2_font_open_iconic_weather_4x_t
                     : u8g2_font_open_iconic_weather_2x_t);
  u8g2.drawGlyph(x, y, g);
}

void updateDisplayWeather() {
  bool fc = (currentMode == MODE_FORECAST);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB14_tf);
  u8g2.setCursor(0, 22); u8g2.print(fc ? w_forecast_temp : w_temp);
  drawWeatherIcon(fc ? w_forecast_icon : w_iconCode, 90, 48, true);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 35);
  u8g2.print(labels[currentLang][L_HUM]);
  u8g2.print(fc ? w_forecast_hum : w_hum); u8g2.print("%");
  if (fc) {
    u8g2.setFont(u8g2_font_5x7_tr);
    int tw = u8g2.getStrWidth(labels[currentLang][L_TOMORROW]);
    u8g2.setCursor(128 - tw, 10); u8g2.print(labels[currentLang][L_TOMORROW]);
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(0, 48); u8g2.print("v " + w_forecast_min + " ^ " + w_forecast_max);
  } else {
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(0, 48); u8g2.print(w_cityDisplay);
  }
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(0, 62); u8g2.print(fc ? w_forecast_desc : w_desc);
  u8g2.sendBuffer(); yield();
}

void updateDisplayCalendar() {
  u8g2.clearBuffer();
  // Header row
  u8g2.setFont(u8g2_font_open_iconic_embedded_2x_t);
  u8g2.drawFrame(2,7,8,8); u8g2.drawLine(2,9,10,9); u8g2.drawPixel(6,12);
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.setCursor(20, 15); u8g2.print(labels[currentLang][L_CAL]);

  if (hasCalendarEvent && cal_eventCount > 0) {
    char cntBuf[8];
    snprintf(cntBuf, sizeof(cntBuf), "%d/%d", cal_currentEventIdx + 1, cal_eventCount);
    u8g2.setFont(u8g2_font_5x7_tr);
    int cw = u8g2.getStrWidth(cntBuf);
    u8g2.setCursor(127 - cw, 10);
    u8g2.print(cntBuf);
  }

  u8g2.drawHLine(0, 20, 128);

  if (hasCalendarEvent && cal_eventCount > 0) {
    String displayTime = localizeCalTime(cal_times[cal_currentEventIdx]);
    const String& curEvent = cal_events[cal_currentEventIdx];

    // FIX BUG 3: "TOMORROW 15:00" printed entirely in helvB10 bold was too wide.
    // We now split the string at the first space: if the first character is NOT a
    // digit the leading word is a day-label (TOMORROW / TODAY / DOMANI / etc.) and
    // gets a small 5x7 font; the time portion (or the whole string when there is no
    // prefix) is printed in medium bold helvB10.
    //
    // Display layout (y=0 top, y=63 bottom, separator at y=20):
    //   y=31  day-label in 5x7   (only when a prefix exists)
    //   y=44  time in helvB10    (or y=40 when no prefix, same as before)
    //   y=57  event title in 6x10 (scrolling if wider than 128px)

    String dayPrefix = "";
    String timeOnly  = displayTime;
    if (displayTime.length() > 0 && !isDigit((unsigned char)displayTime[0])) {
      int sp = displayTime.indexOf(' ');
      if (sp > 0) {
        dayPrefix = displayTime.substring(0, sp);
        timeOnly  = displayTime.substring(sp + 1);
      }
    }

    if (dayPrefix.length() > 0) {
      u8g2.setFont(u8g2_font_5x7_tr);
      u8g2.setCursor(0, 31);
      u8g2.print(dayPrefix);
      u8g2.setFont(u8g2_font_helvB10_tf);
      u8g2.setCursor(0, 44);
      u8g2.print(timeOnly);
    } else {
      u8g2.setFont(u8g2_font_helvB10_tf);
      u8g2.setCursor(0, 40);
      u8g2.print(timeOnly);
    }

    u8g2.setFont(u8g2_font_6x10_tr);
    if (textWidth == 0) textWidth = u8g2.getStrWidth(curEvent.c_str());
    u8g2.setCursor(textWidth > 128 ? -scrollX : 0, 57);
    u8g2.print(curEvent);
  } else {
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(10, 45); u8g2.print(labels[currentLang][L_NO_EVENT]);
  }
  u8g2.sendBuffer(); yield();
}

// ===================== SETUP =====================
void setup() {
  nvs_flash_init();
  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  u8g2.begin();
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
#if ENABLE_BUZZER
  buzzer.begin();
#endif
  initPowerManagement();

  preferences.begin("clock_cfg", true);
  nmEnabled  = preferences.getBool("nm_en", false);
  nmStartH   = preferences.getInt("nm_sh", 23); nmStartM = preferences.getInt("nm_sm", 0);
  nmEndH     = preferences.getInt("nm_eh", 7);  nmEndM   = preferences.getInt("nm_em", 0);
  nmWakeTime = preferences.getInt("nm_wt", 5);
  String savedSSID = preferences.getString("ssid", "");
  String savedPass = preferences.getString("pass", "");
  weatherKey      = preferences.getString("key",   "");
  weatherCity     = preferences.getString("city",  "");
  googleScriptUrl = preferences.getString("g_url", "");
  currentLang    = preferences.getInt("lng",    0);
  utcOffset      = preferences.getInt("offset", 3600);
  autoDST        = preferences.getInt("dst",    1);
  dateFormat     = preferences.getInt("df",     0);
  clockFont      = preferences.getInt("cfont",  0);
  webThemeLight  = preferences.getBool("wtheme", false);
  otaPass        = preferences.getString("ota_pass", "");
  otaUser        = preferences.getString("ota_user", "");
  autoClockReturnMin = preferences.getInt("acr", 5);
#if ENABLE_BUZZER
  alarmHour       = preferences.getInt("ah",    7);
  alarmMinute     = preferences.getInt("am",    0);
  alarmEnabled    = preferences.getBool("ae",   false);
  alarmPatternIdx = preferences.getInt("ap",    0);
  alarmDays       = preferences.getInt("adays", 0b1111111);
  muteUI          = preferences.getBool("mute", false);
#endif
  preferences.end();

  if (clockFont < 0 || clockFont > 3) clockFont = 0;
  timeClient.setTimeOffset(utcOffset);
  timeClient.setUpdateInterval(3600000);
  modeChangedAt = millis();

  if (savedSSID != "" && savedSSID != "NULL") {
    preferences.begin("wifi_boot", false);
    int wifiAttempt = preferences.getInt("attempt", 0);
    preferences.end();

    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_STA);
    enableWifi6();
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());

    const unsigned long WIFI_TIMEOUT_MS = 5000UL;
    unsigned long wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < WIFI_TIMEOUT_MS) {
      unsigned long elapsed = millis() - wifiStart;
      int barW = (int)((elapsed * 108UL) / WIFI_TIMEOUT_MS);
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_6x10_tr);
      u8g2.drawStr(10, 22, labels[currentLang][L_SYNC]);
      u8g2.drawStr(10, 36, savedSSID.c_str());
      u8g2.drawFrame(10, 44, 108, 7);
      if (barW > 0) u8g2.drawBox(10, 44, barW, 7);
      char tbuf[16]; snprintf(tbuf, sizeof(tbuf), "Try %d/3", wifiAttempt + 1);
      u8g2.drawStr(10, 62, tbuf);
      u8g2.sendBuffer();
      delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
      preferences.begin("wifi_boot", false);
      preferences.putInt("attempt", 0);
      preferences.end();
      if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
    } else {
      wifiAttempt++;
      if (wifiAttempt < 3) {
        preferences.begin("wifi_boot", false);
        preferences.putInt("attempt", wifiAttempt);
        preferences.end();
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.drawStr(10, 35, "Retrying...");
        char rbuf[16]; snprintf(rbuf, sizeof(rbuf), "(%d/3 failed)", wifiAttempt);
        u8g2.drawStr(10, 50, rbuf);
        u8g2.sendBuffer();
        delay(1000);
        ESP.restart();
      } else {
        preferences.begin("wifi_boot", false);
        preferences.putInt("attempt", 0);
        preferences.end();
        WiFi.disconnect();
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ESP32-Cuboid-Fallback");
        isAPMode = true;
      }
    }
  } else {
    WiFi.mode(WIFI_AP); WiFi.softAP("ESP32-Cuboid");
    isAPMode = true;
  }

  ArduinoOTA.setHostname(HOSTNAME);
  if (otaUser.length() > 0) ArduinoOTA.setPasswordHash(otaUser.c_str());
  if (otaPass.length() > 0) ArduinoOTA.setPassword(otaPass.c_str());
  ArduinoOTA.onStart([]() {});
  ArduinoOTA.onError([](ota_error_t) {});
  ArduinoOTA.begin();

  if (isAPMode) {
    dnsServer.start(53, "*", WiFi.softAPIP());
    u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(5, 12, labels[currentLang][L_SETUP]);
    u8g2.drawStr(5, 30, labels[currentLang][L_WIFI]);
    u8g2.drawStr(35, 30, "ESP32-Cuboid");
    u8g2.drawStr(5, 45, "IP:");  u8g2.drawStr(25, 45, "192.168.4.1");
    u8g2.drawStr(5, 60, "http://cuboid.local");
    u8g2.sendBuffer();
  } else {
    timeClient.begin();
    if (weatherKey      != "") fetchWeather();
    if (googleScriptUrl != "") fetchCalendar();
  }

  server.on("/",                    handleRoot);
  server.on("/advanced",            handleAdvanced);
  server.on("/sysinfo",             handleSysInfo);
  server.on("/scan",                handleScan);
  server.on("/toggleMute",          handleToggleMute);
  server.on("/toggleTheme",         handleToggleTheme);
  server.on("/saveTimeAlarm",       handleSaveTimeAlarm);
  server.on("/savePersonalization", handleSavePersonalization);
  server.on("/saveAPI",             handleSaveAPI);
  server.on("/saveSystem",          handleSaveSystem);
  server.on("/saveNightMode",       handleSaveNightMode);
  server.on("/saveOTAPass",         handleSaveOTAPass);
  server.on("/update", HTTP_POST,
    []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
      ESP.restart();
    },
    []() {
      HTTPUpload& u = server.upload();
      if      (u.status == UPLOAD_FILE_START) { Update.begin(UPDATE_SIZE_UNKNOWN); }
      else if (u.status == UPLOAD_FILE_WRITE) { Update.write(u.buf, u.currentSize); }
      else if (u.status == UPLOAD_FILE_END)   { Update.end(true); }
    }
  );
  server.begin();
}

// ===================== LOOP =====================
void loop() {
  ArduinoOTA.handle();

  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    unsigned long pressStart  = millis();
    bool          longHandled = false;

    while (digitalRead(BOOT_BUTTON_PIN) == LOW) {
#if ENABLE_BUZZER
      buzzer.update();
#endif
      unsigned long held = millis() - pressStart;

      if (!longHandled && held > 300 && currentMode == MODE_WEATHER) {
        currentMode = MODE_FORECAST; longHandled = true;
        modeChangedAt = millis();
        buzzer.playUISound(1500, 100);
        updateDisplayWeather();
      }

      if (!longHandled && held > 300 && currentMode == MODE_CALENDAR
          && hasCalendarEvent && cal_eventCount > 1) {
        cal_currentEventIdx = (cal_currentEventIdx + 1) % cal_eventCount;
        scrollX = 0; textWidth = 0;
        longHandled = true;
        modeChangedAt = millis();
        buzzer.playUISound(1500, 100);
        updateDisplayCalendar();
      }

      if (held > 10000) {
        u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.drawStr(10, 35, "FACTORY RESET..."); u8g2.sendBuffer();
#if ENABLE_BUZZER
        BuzzerStep rst[] = { {2000,80},{0,70},{2000,80},{0,70},{2000,80},{0,70},{2000,80},{0,150},{3200,350},{0,1} };
        buzzer.playPattern(rst, 10, false);
        unsigned long t = millis();
        while (millis() - t < 2000) { buzzer.update(); delay(1); }
#endif
        preferences.begin("clock_cfg", false); preferences.clear(); preferences.end();
        ESP.restart();
      }
      yield();
    }

    if (!longHandled && (millis() - pressStart < 300)) {
      if (isAlarmRinging) {
#if ENABLE_BUZZER
        buzzer.stop(); isAlarmRinging = false;
#endif
        if (currentMode == MODE_CLOCK) updateDisplayClock();
      } else {
        bool inNight = checkNightMode();
        if (inNight) wakeScreenUntil = millis() + (nmWakeTime * 1000UL);
        bool shouldCycle = !inNight || isScreenOn;
        if (shouldCycle) {
          bool hasWeather = (weatherKey      != "");
          bool hasCal     = (googleScriptUrl != "");
          Mode newMode = currentMode;
          if      (currentMode == MODE_FORECAST)  newMode = MODE_WEATHER;
          else if (currentMode == MODE_CLOCK)     newMode = hasCal ? MODE_CALENDAR : (hasWeather ? MODE_WEATHER : MODE_CLOCK);
          else if (currentMode == MODE_CALENDAR)  newMode = hasWeather ? MODE_WEATHER : MODE_CLOCK;
          else                                    newMode = MODE_CLOCK;
          bool changed = (newMode != currentMode);
          currentMode = newMode;
          if (currentMode == MODE_CALENDAR) { scrollX = 0; textWidth = 0; }
          if (changed) {
            modeChangedAt = millis();
            buzzer.playUISound(2000, 100);
          }
        }
      }
    }
    refreshDisplay();
  }

  server.handleClient();
  if (isAPMode) { dnsServer.processNextRequest(); return; }

  unsigned long now = millis();
#if ENABLE_BUZZER
  buzzer.update();
#endif

  if (WiFi.status() == WL_CONNECTED) {
    timeClient.update();
    if (weatherKey != "" && now - lastWeatherUpdate > DATA_REFRESH_INTERVAL) {
      fetchWeather(); lastWeatherUpdate = now;
    }
    if (googleScriptUrl != "" && now - lastCalendarUpdate > DATA_REFRESH_INTERVAL) {
      fetchCalendar(); lastCalendarUpdate = now;
    }
  }

  hours   = timeClient.getHours();
  minutes = timeClient.getMinutes();
  seconds = timeClient.getSeconds();
  {
    time_t et = timeClient.getEpochTime(); struct tm* p = gmtime(&et);
    day    = p->tm_mday;
    month  = p->tm_mon + 1;
    year   = p->tm_year + 1900;
    wdayBit = (p->tm_wday == 0) ? 6 : p->tm_wday - 1;
  }

#if ENABLE_BUZZER
  if (alarmEnabled && !isAlarmRinging
      && hours == alarmHour && minutes == alarmMinute && seconds == 0
      && isAlarmActiveToday()) {
    isAlarmRinging = true;
    currentMode    = MODE_CLOCK;
    startAlarmSound();
  }
#endif

  bool shouldBeOn = !(checkNightMode() && !isAlarmRinging && now >= wakeScreenUntil);
  if (shouldBeOn && !isScreenOn) {
    applyCpuFreq(true);
    display.ssd1306_command(SSD1306_DISPLAYON); u8g2.setPowerSave(0);
    isScreenOn = true; updateDisplayClock();
  } else if (!shouldBeOn && isScreenOn) {
    display.ssd1306_command(SSD1306_DISPLAYOFF); u8g2.setPowerSave(1);
    isScreenOn = false; applyCpuFreq(false);
  }

  if (isScreenOn
      && autoClockReturnMin > 0
      && currentMode != MODE_CLOCK
      && (now - modeChangedAt) >= (unsigned long)autoClockReturnMin * 60000UL) {
    currentMode   = MODE_CLOCK;
    modeChangedAt = now;
    updateDisplayClock();
  }

  static unsigned long lastClockUpdate = 0;
  if (isScreenOn && currentMode == MODE_CLOCK && now - lastClockUpdate >= 1000) {
    updateDisplayClock(); lastClockUpdate = now;
  }

  if (isScreenOn && currentMode == MODE_CALENDAR
      && hasCalendarEvent && textWidth > 128
      && now - lastScrollUpdate >= 50) {
    if (++scrollX > textWidth + 20) scrollX = 0;
    lastScrollUpdate = now;
    updateDisplayCalendar();
  }

  yield();
}
