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

// ===================== SETTINGS TIMING =====================
#define SETTINGS_HOLD_MS  3000
#define SETTINGS_HOLD_WIN  200

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
  &FreeSansBold24pt7b, &FreeSans24pt7b, &FreeSerifBold24pt7b
};
const char* clockFontNames[] = { "Sans Bold", "Sans", "SerifBold" };

// ===================== LANGUAGE =====================
enum {
  L_SYNC, L_SETUP, L_IP, L_SAVE, L_WIFI, L_PASS, L_CITY, L_LANG,
  L_HUM, L_RESTART, L_TIME, L_TZ, L_ASK_RST, L_ALARM, L_ENABLED,
  L_PATTERN, L_MUTE, L_CAL, L_NO_EVENT, L_TOMORROW,
  L_DAYS_ACTIVE, L_PERSONAL, L_CLOCK_FONT, L_DATE_STYLE, L_THEME,
  L_THEME_DARK, L_THEME_LIGHT, L_NM_SECTION, L_WEATHER_CAL,
  L_WIFI_SYS, L_ALARM_TIME, L_AUTO_CLOCK, L_TODAY, L_COUNT
};
const char* langNames[] = { "English","Italiano","Francais","Espanol","Deutsch" };
const char* labels[5][L_COUNT] = {
  {"Syncing...","SETUP MODE","Connect to:","SAVE","WiFi","Password","City","Language","RH: ","Restarting...","Time","UTC Offset","Restart required. Proceed?","Alarm","Enabled","Ringtone","Mute UI","Calendar","No Events","TOMORROW","Active days","Personalization","Clock Font","Date Style","Theme","Dark","Light","Night Mode","Weather & Calendar","WiFi & System","Alarm & Time","Auto clock return (min, 0=off)","TODAY"},
  {"Sincronizzazione...","MODALITA' SETUP","Connettiti a:","SALVA","WiFi","Password","Citta","Lingua","UM: ","Riavvio...","Ora","Offset UTC","Riavvio necessario. Procedere?","Sveglia","Attiva","Suoneria","Muta UI","Calendario","Nessun Evento","DOMANI","Giorni attivi","Personalizzazione","Font Orologio","Stile Data","Tema","Scuro","Chiaro","Modalita' Notte","Meteo e Calendario","WiFi e Sistema","Sveglia e Ora","Ritorno orologio (min, 0=off)","OGGI"},
  {"Synchro...","MODE CONFIG","Connecter a:","ENREGISTRER","WiFi","Mot de passe","Ville","Langue","RH: ","Redemarrage...","Heure","Offset UTC","Redemarrage requis. Continuer?","Reveil","Active","Sonnerie","Muet","Calendrier","Aucun evenement","DEMAIN","Jours actifs","Personnalisation","Police Horloge","Style Date","Theme","Sombre","Clair","Mode Nuit","Meteo et Calendrier","WiFi et Systeme","Reveil et Heure","Retour horloge (min, 0=off)","AUJOURD'HUI"},
  {"Sincronizando...","MODO SETUP","Conectar a:","GUARDAR","WiFi","Contrasena","Ciudad","Idioma","HR: ","Reiniciando...","Hora","Offset UTC","Reinicio requerido. Proceder?","Alarma","Activa","Tono","Silenciar","Calendario","Sin Eventos","MANANA","Dias activos","Personalizacion","Fuente Reloj","Estilo Fecha","Tema","Oscuro","Claro","Modo Noche","Clima y Calendario","WiFi y Sistema","Alarma y Hora","Retorno reloj (min, 0=off)","HOY"},
  {"Synchronisieren...","SETUP MODUS","Verbinden mit:","SPEICHERN","WiFi","Passwort","Stadt","Sprache","FF: ","Neustart...","Zeit","UTC-Offset","Neustart erforderlich. Fortfahren?","Wecker","Aktiv","Klingelton","Stumm","Kalender","Keine Termine","MORGEN","Aktive Tage","Personalisierung","Uhr-Schrift","Datumsstil","Design","Dunkel","Hell","Nachtmodus","Wetter und Kalender","WiFi und System","Wecker und Zeit","Auto Uhr (min, 0=aus)","HEUTE"}
};
const char* dayNames[5][7] = {
  {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"},
  {"Lun","Mar","Mer","Gio","Ven","Sab","Dom"},
  {"Lun","Mar","Mer","Jeu","Ven","Sam","Dim"},
  {"Lun","Mar","Mie","Jue","Vie","Sab","Dom"},
  {"Mo","Di","Mi","Do","Fr","Sa","So"}
};
const char* monthShort[5][12] = {
  {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"},
  {"Gen","Feb","Mar","Apr","Mag","Giu","Lug","Ago","Set","Ott","Nov","Dic"},
  {"Jan","Fev","Mar","Avr","Mai","Jun","Jul","Aou","Sep","Oct","Nov","Dec"},
  {"Ene","Feb","Mar","Abr","May","Jun","Jul","Ago","Sep","Oct","Nov","Dic"},
  {"Jan","Feb","Mar","Apr","Mai","Jun","Jul","Aug","Sep","Okt","Nov","Dez"}
};

// ===================== CSS — PROGMEM =====================
const char CSS_VARS[] PROGMEM =
  "<style>:root{--bg:#111318;--cbg:#1c1e26;--tbg:#141620;--txt:#dde1f0;--muted:#7b82a0;"
  "--acc:#a78bfa;--teal:#2dd4bf;--brd:#2e3148;--inp:#22253a;--ibox:#191c2e;--sep:#22253a;"
  "--hvr:#000;--sbtn:#4c1d95;--sbtn-h:#6d28d9;--ok:#065f46;--pill-on-bg:#2e1065;"
  "--pill-on-brd:#a78bfa;--pill-on-txt:#c4b5fd;--mute-brd:#f87171;--mute-txt:#f87171;"
  "--rssi-ok:#2dd4bf;--rssi-warn:#fbbf24;--rssi-bad:#f87171;}"
  "body.lm{--bg:#f0f2f9;--cbg:#fff;--tbg:#ede9fe;--txt:#1e1b2e;--muted:#6b7280;"
  "--acc:#7c3aed;--teal:#0d9488;--brd:#d1d5db;--inp:#f9fafb;--ibox:#f3f4f6;--sep:#e5e7eb;"
  "--hvr:#fff;--sbtn:#7c3aed;--sbtn-h:#6d28d9;--ok:#047857;--pill-on-bg:#ede9fe;"
  "--pill-on-brd:#7c3aed;--pill-on-txt:#6d28d9;--mute-brd:#ef4444;--mute-txt:#ef4444;"
  "--rssi-ok:#0d9488;--rssi-warn:#d97706;--rssi-bad:#dc2626;}"
  "*{box-sizing:border-box;transition:background-color .25s,color .2s,border-color .2s;}"
  "body{background:var(--bg);color:var(--txt);font-family:'Segoe UI',system-ui,sans-serif;margin:0;padding:0;}"
  ".page{padding:12px;max-width:600px;margin:0 auto;}"
  ".card{background:var(--cbg);padding:16px;border-radius:14px;margin-bottom:14px;"
  "box-shadow:0 2px 12px rgba(0,0,0,.18);border:1px solid var(--brd);}"
  ".topbar{background:var(--tbg);padding:9px 13px;border-radius:14px;margin-bottom:14px;"
  "box-shadow:0 4px 16px rgba(0,0,0,.25);display:flex;align-items:center;gap:7px;"
  "border:1px solid var(--brd);position:sticky;top:8px;z-index:99;flex-wrap:wrap;}"
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
  ".tbtn.exp-btn{border-color:var(--acc);color:var(--acc);padding:5px 11px;font-size:13px;"
  "font-weight:900;letter-spacing:1px;}"
  ".tbtn.exp-btn:hover,.tbtn.exp-btn.open{background:var(--acc);color:var(--hvr);}"
  ".tb-panel{flex-basis:100%;display:flex;align-items:center;gap:8px;padding-top:8px;"
  "margin-top:2px;border-top:1px solid var(--sep);}"
  ".tb-panel input[type=range]{flex:1;height:28px;padding:0;accent-color:var(--acc);}"
  ".tb-bv{font-size:12px;color:var(--txt);font-weight:700;min-width:32px;text-align:right;flex-shrink:0;}"
  "h2{color:var(--acc);font-size:14px;font-weight:800;margin:0 0 13px;border-bottom:1px solid var(--sep);"
  "padding-bottom:8px;display:flex;align-items:center;gap:7px;letter-spacing:.5px;text-transform:uppercase;}"
  "label{display:block;margin-top:10px;font-size:12px;color:var(--muted);font-weight:600;"
  "letter-spacing:.4px;text-transform:uppercase;}"
  ".lrow{display:flex;align-items:baseline;gap:8px;margin-top:10px;}"
  ".lrow span{font-size:12px;color:var(--muted);font-weight:600;letter-spacing:.4px;text-transform:uppercase;}"
  ".lrow a{font-size:11px;color:var(--teal);text-decoration:none;flex-shrink:0;}"
  ".lrow a:hover{text-decoration:underline;}"
  "select,input[type=text],input[type=number],input[type=password]{width:100%;padding:10px 12px;"
  "margin:4px 0;border-radius:9px;border:1.5px solid var(--brd);background:var(--inp);"
  "color:var(--txt);font-size:14px;outline:none;}"
  "select:focus,input:focus{border-color:var(--acc);box-shadow:0 0 0 3px rgba(167,139,250,.15);}"
  ".save-btn{width:100%;padding:13px;background:var(--sbtn);color:#fff;border:none;border-radius:9px;"
  "font-weight:800;cursor:pointer;margin-top:14px;font-size:13px;letter-spacing:.5px;"
  "text-transform:uppercase;transition:background .2s,transform .1s;display:block;text-align:center;}"
  ".save-btn:hover{background:var(--sbtn-h);}.save-btn:active{transform:scale(.98);}"
  ".save-btn.ok{background:var(--ok);}.save-btn.wait{background:#555;cursor:wait;}"
  ".row{display:flex;gap:10px;}.col{flex:1;}.wrow{display:flex;gap:8px;align-items:flex-end;}.wsel{flex:1;}"
  ".icon-btn{width:44px;height:44px;background:var(--inp);border:1.5px solid var(--brd);"
  "border-radius:9px;cursor:pointer;display:flex;justify-content:center;align-items:center;"
  "padding:0;flex-shrink:0;}"
  ".icon-btn:hover{background:var(--cbg);border-color:var(--acc);}"
  ".icon-btn svg{fill:var(--teal);width:22px;height:22px;}"
  ".spin{animation:spin 1s linear infinite;}@keyframes spin{100%{transform:rotate(360deg);}}"
  "hr.sep{border:0;border-top:1px solid var(--sep);margin:13px 0;}"
  ".ig{display:grid;grid-template-columns:auto 1fr;gap:5px 14px;font-size:12px;}"
  ".ig .k{color:var(--muted);white-space:nowrap;font-weight:600;}"
  ".ig .v{color:var(--txt);font-family:monospace;word-break:break-all;}"
  ".bar-wrap{background:var(--ibox);border-radius:4px;height:5px;margin:4px 0 10px;overflow:hidden;}"
  ".bar{height:100%;border-radius:4px;transition:width .4s;}"
  ".ibox{background:var(--ibox);border-radius:9px;padding:10px 13px;font-size:12px;"
  "color:var(--muted);margin:10px 0 4px;line-height:1.6;border:1px solid var(--sep);}"
  ".days-row{display:flex;gap:5px;flex-wrap:wrap;margin-top:8px;}"
  ".day-pill{display:inline-flex;align-items:center;justify-content:center;width:40px;height:34px;"
  "border-radius:8px;border:2px solid var(--brd);background:var(--inp);color:var(--muted);"
  "font-size:11px;font-weight:800;cursor:pointer;user-select:none;transition:all .15s;}"
  ".day-pill.on{border-color:var(--pill-on-brd);background:var(--pill-on-bg);color:var(--pill-on-txt);}"
  ".day-pill:hover{border-color:var(--acc);}"
  ".font-preview{display:flex;gap:8px;flex-wrap:wrap;margin-top:8px;}"
  ".fp{padding:6px 12px;border-radius:8px;border:2px solid var(--brd);background:var(--inp);"
  "color:var(--muted);font-size:13px;cursor:pointer;user-select:none;transition:all .15s;}"
  ".fp.on{border-color:var(--pill-on-brd);background:var(--pill-on-bg);color:var(--pill-on-txt);font-weight:700;}"
  ".fp:hover{border-color:var(--acc);}"
  "input[type=range]{padding:0;height:36px;accent-color:var(--acc);}"
  "</style>";

// ===================== AJAX JS — PROGMEM =====================
// NOTE: this string intentionally ends mid-function; handleRoot/handleAdvanced complete it.
const char AJAX_JS[] PROGMEM = R"js(
<script>
function toggleTheme(){document.body.classList.toggle('lm');const l=document.body.classList.contains('lm');document.getElementById('themebtn').textContent=l?'\u{1F319}':'\u2728';fetch('/toggleTheme');}
document.addEventListener('DOMContentLoaded',()=>{
  document.querySelectorAll('.day-pill').forEach(p=>{p.addEventListener('click',()=>{const i=document.getElementById('day_'+p.dataset.day);if(i.value==='1'){i.value='0';p.classList.remove('on');}else{i.value='1';p.classList.add('on');}});});
  document.querySelectorAll('.fp').forEach(f=>{f.addEventListener('click',()=>{document.querySelectorAll('.fp').forEach(x=>x.classList.remove('on'));f.classList.add('on');document.getElementById('cfont_inp').value=f.dataset.font;});});
  document.querySelectorAll('form.aform').forEach(f=>{
    const ir=f.dataset.restart!==undefined;
    f.addEventListener('submit',async e=>{
      e.preventDefault();const btn=f.querySelector('.save-btn');const orig=btn.textContent;
      btn.textContent='...';btn.classList.add('wait');btn.disabled=true;
      try{
        const res=await fetch(f.action,{method:'POST',body:new URLSearchParams(new FormData(f))});
        if(res.status===403){btn.textContent='Wrong password';btn.classList.remove('wait');setTimeout(()=>{btn.textContent=orig;btn.disabled=false;},2500);return;}
        if(ir){let s=8;const t=()=>{btn.textContent='Reboot '+s+'s';if(--s>0)setTimeout(t,1000);else location.reload();};btn.classList.remove('wait');t();}
        else{btn.textContent='Saved \u2713';btn.classList.remove('wait');btn.classList.add('ok');setTimeout(()=>{btn.textContent=orig;btn.classList.remove('ok');btn.disabled=false;},2000);}
      }catch(_){btn.textContent='Error';btn.classList.remove('wait');setTimeout(()=>{btn.textContent=orig;btn.disabled=false;},2000);}
    });
  });
});
function scanWifi(){
  const ic=document.getElementById('scIco');const sel=document.getElementById('ssid');
  if(!sel||!ic)return;ic.classList.add('spin');
  fetch('/scan').then(r=>r.json()).then(d=>{sel.innerHTML='';
)js";

// ===================== AUTO BRIGHTNESS SLOT EDITOR JS =====================
// FIX: uses addEventListener instead of inline handlers so IIFE-scoped vars (_s, ser, render)
// are accessible via closure — inline oninput/onclick run in global scope and would throw
// ReferenceError for variables declared inside the IIFE.
const char AB_JS[] PROGMEM = R"abjs(
<script>
(function(){
var _s=[];
var _raw=document.getElementById('_ab_slots');
if(_raw&&_raw.value){_raw.value.split(',').forEach(function(p){var k=p.split(':');if(k.length===2){var t=parseInt(k[0]),pct=parseInt(k[1]);if(!isNaN(t)&&!isNaN(pct))_s.push({t:t,p:pct});}});}
function fmt(t){return ('0'+Math.floor(t/60)).slice(-2)+':'+('0'+(t%60)).slice(-2);}
function ser(){_s.sort(function(a,b){return a.t-b.t;});document.getElementById('_ab_slots').value=_s.map(function(x){return x.t+':'+x.p;}).join(',');}
function render(){
  _s.sort(function(a,b){return a.t-b.t;});
  var wrap=document.getElementById('_ab_rows');
  if(!wrap)return;
  var h='';
  _s.forEach(function(s,i){
    h+='<div style="display:grid;grid-template-columns:90px 1fr 44px;gap:6px;align-items:center;padding:5px 0;border-bottom:1px solid var(--sep);">';
    h+='<input type="time" value="'+fmt(s.t)+'" data-i="'+i+'" class="_abt" style="padding:7px 6px;border-radius:8px;border:1.5px solid var(--brd);background:var(--inp);color:var(--txt);font-size:13px;width:100%;">';
    h+='<div style="display:flex;align-items:center;gap:6px;">';
    h+='<input type="range" min="1" max="100" value="'+s.p+'" data-i="'+i+'" class="_abr" style="flex:1;height:28px;accent-color:var(--acc);">';
    h+='<span style="min-width:38px;font-size:13px;font-weight:700;color:var(--txt);text-align:right;">'+s.p+'%</span>';
    h+='</div>';
    h+='<button type="button" data-i="'+i+'" class="_abd" style="height:36px;width:36px;border-radius:8px;border:1.5px solid var(--mute-brd);background:transparent;color:var(--mute-txt);cursor:pointer;font-size:18px;font-weight:700;display:flex;align-items:center;justify-content:center;">&#8722;</button>';
    h+='</div>';
  });
  wrap.innerHTML=h;
  wrap.querySelectorAll('._abt').forEach(function(el){
    el.addEventListener('change',function(){
      var v=this.value.split(':');
      _s[parseInt(this.dataset.i)].t=parseInt(v[0])*60+parseInt(v[1]);
      ser();
    });
  });
  wrap.querySelectorAll('._abr').forEach(function(el){
    el.addEventListener('input',function(){
      _s[parseInt(this.dataset.i)].p=parseInt(this.value);
      this.nextElementSibling.textContent=this.value+'%';
      ser();
    });
  });
  wrap.querySelectorAll('._abd').forEach(function(el){
    el.addEventListener('click',function(){
      _s.splice(parseInt(this.dataset.i),1);
      render();
      ser();
    });
  });
}
window.abAdd=function(){if(_s.length>=8)return;var t=(_s.length>0?_s[_s.length-1].t+60:720)%1440;_s.push({t:t,p:75});render();ser();};
render();
})();
</script>
)abjs";

// ===================== STATE =====================
int  hours,minutes,seconds,day,month,year,wdayBit=0;
int  dateFormat=0,currentLang=0,utcOffset=3600;
bool autoDST=true,isAPMode=false;
int  clockFont=0,autoClockReturnMin=5;
unsigned long modeChangedAt=0;
bool webThemeLight=false;
int  nmMode=0;
int  nmStartH=23,nmStartM=0,nmEndH=7,nmEndM=0,nmWakeTime=5;
bool isScreenOn=true,manualScreenOff=false;
unsigned long wakeScreenUntil=0;
int  pixelBrightness=100;

// ===================== AUTO BRIGHTNESS =====================
#define AB_MAX_SLOTS 8
struct BrightSlot { uint16_t minuteOfDay; uint8_t pct; };
bool autoBrightEnabled=false;
int  autoBrightSlotCount=7;
BrightSlot autoBrightSlots[AB_MAX_SLOTS]={
  {360,50},{420,75},{600,100},{1020,75},{1080,50},{1200,25},{1260,1}
};
int  lastAutoBrightMinute=-1;

bool inSettingsMode=false;
int  settingsIdx=0;

int  screenOffAnim=1,screenOffStepMs=30;
bool wakeAnim=false;
const char* screenOffAnimNames[]={"None","Checkerboard","Swipe Down","Swipe Up","Swipe Right","Swipe Left","Diagonal","Curtain","Dissolve","Implode"};
const char* screenOffAnimShort[]={"None","Checker","SwipeD","SwipeU","SwipeR","SwipeL","Diagonal","Curtain","Dissolve","Implode"};
const int SCREEN_OFF_ANIM_COUNT=10;

enum Mode { MODE_WEATHER=0,MODE_CLOCK=1,MODE_CALENDAR=2,MODE_FORECAST=3 };
extern Mode currentMode;

// ===================== BUFFER HELPERS =====================
static inline void u8ClearPx(uint8_t*b,int x,int y){b[(y>>3)*128+x]&=~(uint8_t)(1u<<(y&7));}
static inline void u8ClearRow(uint8_t*b,int y){uint8_t*p=b+(y>>3)*128;uint8_t m=~(uint8_t)(1u<<(y&7));for(int x=0;x<128;x++)p[x]&=m;}
static inline void u8ClearCol(uint8_t*b,int x){for(int pg=0;pg<8;pg++)b[pg*128+x]=0;}
static inline void u8RevealPx(uint8_t*b,const uint8_t*s,int x,int y){if(s[(y>>3)*128+x]&(uint8_t)(1u<<(y&7)))b[(y>>3)*128+x]|=(uint8_t)(1u<<(y&7));}
static inline void u8RevealRow(uint8_t*b,const uint8_t*s,int y){uint8_t*p=b+(y>>3)*128;const uint8_t*sp=s+(y>>3)*128;uint8_t m=(uint8_t)(1u<<(y&7));for(int x=0;x<128;x++)if(sp[x]&m)p[x]|=m;}
static inline void u8RevealCol(uint8_t*b,const uint8_t*s,int x){for(int pg=0;pg<8;pg++)b[pg*128+x]|=s[pg*128+x];}
static const uint8_t BAYER4[16] PROGMEM={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};

static void applyContrast(int pct){
  uint8_t v=(uint8_t)((constrain(pct,0,100)*255)/100);
  display.ssd1306_command(0x81); display.ssd1306_command(v);
}

// ===================== AUTO BRIGHTNESS HELPERS =====================
static String abSerialize(){
  String s="";
  for(int i=0;i<autoBrightSlotCount;i++){if(i>0)s+=',';s+=String(autoBrightSlots[i].minuteOfDay)+':'+String(autoBrightSlots[i].pct);}
  return s;
}
static void abParse(const String& s){
  autoBrightSlotCount=0;
  int pos=0;
  while(pos<(int)s.length()&&autoBrightSlotCount<AB_MAX_SLOTS){
    int colon=s.indexOf(':',pos); if(colon<0)break;
    int comma=s.indexOf(',',colon); if(comma<0)comma=s.length();
    int mins=constrain(s.substring(pos,colon).toInt(),0,1439);
    int pct=constrain(s.substring(colon+1,comma).toInt(),1,100);
    autoBrightSlots[autoBrightSlotCount++]={(uint16_t)mins,(uint8_t)pct};
    pos=comma+1;
  }
  for(int i=0;i<autoBrightSlotCount-1;i++)
    for(int j=i+1;j<autoBrightSlotCount;j++)
      if(autoBrightSlots[j].minuteOfDay<autoBrightSlots[i].minuteOfDay){
        BrightSlot tmp=autoBrightSlots[i];autoBrightSlots[i]=autoBrightSlots[j];autoBrightSlots[j]=tmp;
      }
}
static void applyAutoBrightness(){
  if(!autoBrightEnabled||autoBrightSlotCount==0)return;
  int curMin=hours*60+minutes;
  if(curMin==lastAutoBrightMinute)return;
  lastAutoBrightMinute=curMin;
  int best=autoBrightSlotCount-1;
  for(int i=0;i<autoBrightSlotCount;i++){if(autoBrightSlots[i].minuteOfDay<=curMin)best=i;}
  int newPct=autoBrightSlots[best].pct;
  if(newPct!=pixelBrightness){pixelBrightness=newPct;applyContrast(pixelBrightness);}
}

// ===================== FORWARD DECLARATIONS =====================
void refreshDisplay();
bool checkNightMode();
void updateDisplayClock();
void updateDisplayWeather();
void updateDisplayCalendar();
void fetchWeather();
void fetchCalendar();
void drawSettingsMenu();
// Forward-declared here so BuzzerClass::playUISound can call it;
// actual definition follows DATA STATE where muteUI is declared.
inline bool isSoundSuppressed();

// ===================== BUZZER =====================
// Declared before animateScreen so buzzer.update() is available there.
#if ENABLE_BUZZER
struct BuzzerStep{int freq;unsigned long dur;};
class BuzzerClass{
public:
  void begin(){pinMode(BUZZER_PIN,OUTPUT);playing=false;idx=0;loopPattern=false;}
  void stop(){noTone(BUZZER_PIN);playing=false;loopPattern=false;}
  void playUISound(int freq,unsigned long dur){if(isSoundSuppressed())return;static BuzzerStep p[2];p[0]={freq,dur};p[1]={0,1};playPattern(p,2,false);}
  void playPattern(const BuzzerStep*p,uint8_t len,bool loop=false){
    if(!p||len==0)return;if(len>MAX_STEPS)len=MAX_STEPS;
    noTone(BUZZER_PIN);
    for(uint8_t i=0;i<len;i++)steps[i]=p[i];
    total=len;idx=0;playing=true;loopPattern=loop;nextChange=millis();
    if(steps[0].freq>0)tone(BUZZER_PIN,steps[0].freq);
    nextChange+=steps[0].dur;
  }
  void update(){
    if(!playing)return;unsigned long now=millis();if(now<nextChange)return;
    noTone(BUZZER_PIN);
    if(++idx<total){if(steps[idx].freq>0)tone(BUZZER_PIN,steps[idx].freq);nextChange=now+steps[idx].dur;}
    else if(loopPattern){idx=0;if(steps[0].freq>0)tone(BUZZER_PIN,steps[0].freq);nextChange=now+steps[0].dur;}
    else playing=false;
  }
  bool isPlaying(){return playing;}
private:
  static const uint8_t MAX_STEPS=16;
  BuzzerStep steps[MAX_STEPS];uint8_t total=0,idx=0;
  bool playing=false,loopPattern=false;unsigned long nextChange=0;
};
BuzzerClass buzzer;
#else
struct BuzzerStep{int freq;unsigned long dur;};
class DummyBuzzer{public:void begin(){}void stop(){}void playUISound(int,unsigned long){}void playPattern(const BuzzerStep*,uint8_t,bool=false){}void update(){}bool isPlaying(){return false;}};
DummyBuzzer buzzer;
#endif

// ===================== UNIFIED SCREEN ANIMATION =====================
void animateScreen(bool reveal){
  if(screenOffAnim==0) return;
  if(reveal&&!wakeAnim) return;
  int sms=constrain(screenOffStepMs,1,500);
  uint8_t* buf=u8g2.getBufferPtr();
  static uint8_t saved[1024];

  if(reveal){
    refreshDisplay();
    if(currentMode==MODE_CLOCK) memcpy(buf,display.getBuffer(),1024);
    memcpy(saved,buf,1024);
    memset(buf,0,1024); u8g2.sendBuffer();
  } else {
    if(currentMode==MODE_CLOCK) memcpy(buf,display.getBuffer(),1024);
  }

  auto px =[&](int x,int y){if(reveal)u8RevealPx(buf,saved,x,y);else u8ClearPx(buf,x,y);};
  auto row=[&](int y)       {if(reveal)u8RevealRow(buf,saved,y); else u8ClearRow(buf,y);};
  auto col=[&](int x)       {if(reveal)u8RevealCol(buf,saved,x); else u8ClearCol(buf,x);};

  // animStep: pumps the buzzer on every animation frame so the alarm tone
  // advances correctly when the screen wakes while the alarm is already firing.
  auto animStep=[&](){buzzer.update();delay(sms);yield();};

  switch(screenOffAnim){
    case 1: for(int s=0;s<16;s++){for(int y=0;y<64;y++)for(int x=0;x<128;x++)if(pgm_read_byte(&BAYER4[(y&3)*4+(x&3)])==s)px(x,y);u8g2.sendBuffer();animStep();}break;
    case 2: for(int s=0;s<16;s++){int r=s*4;for(int y=r;y<r+4&&y<64;y++)row(y);u8g2.sendBuffer();animStep();}break;
    case 3: for(int s=0;s<16;s++){int r=63-s*4;for(int y=r;y>r-4&&y>=0;y--)row(y);u8g2.sendBuffer();animStep();}break;
    case 4: for(int s=0;s<16;s++){int c=s*8;for(int x=c;x<c+8&&x<128;x++)col(x);u8g2.sendBuffer();animStep();}break;
    case 5: for(int s=0;s<16;s++){int c=127-s*8;for(int x=c;x>c-8&&x>=0;x--)col(x);u8g2.sendBuffer();animStep();}break;
    case 6: for(int s=0;s<16;s++){int lo=s*16,hi=(s+1)*16;for(int y=0;y<64;y++)for(int x=0;x<128;x++){int v=x+y*2;if(v>=lo&&v<hi)px(x,y);}u8g2.sendBuffer();animStep();}break;
    case 7: for(int s=0;s<16;s++){int n=(s+1)*4;for(int x=0;x<n&&x<64;x++){col(x);col(127-x);}u8g2.sendBuffer();animStep();}break;
    case 8:{uint16_t st=(uint16_t)((millis()&0x1FFE)|1);for(int s=0;s<16;s++){for(int i=0;i<512;i++){st=(uint16_t)((st*6969u+1u)&0x1FFF);px(st&127,st>>7);}u8g2.sendBuffer();animStep();}break;}
    case 9: for(int s=0;s<16;s++){for(int k=s*2;k<=s*2+1&&k<32;k++){int x0=k,x1=127-k,y0=k,y1=63-k;if(x0>x1||y0>y1)break;for(int x=x0;x<=x1;x++){px(x,y0);px(x,y1);}for(int y=y0+1;y<y1;y++){px(x0,y);px(x1,y);}}u8g2.sendBuffer();animStep();}break;
    default:break;
  }
}
static inline void animateScreenOff(){animateScreen(false);}
static inline void animateScreenOn() {animateScreen(true);}

// ===================== CALENDAR TIME LOCALIZATION =====================
String localizeCalTime(const String& raw){
  static const char* tmrKw[]={"tomorrow","Tomorrow","TOMORROW","domani","Domani","DOMANI","demain","Demain","DEMAIN","manana","Manana","MANANA","morgen","Morgen","MORGEN",nullptr};
  static const char* todayKw[]={"today","Today","TODAY","oggi","Oggi","OGGI","hoy","Hoy","HOY","heute","Heute","HEUTE","aujourd","Aujourd",nullptr};
  for(int i=0;tmrKw[i];i++){String kw(tmrKw[i]);if(raw.length()>=kw.length()&&raw.substring(0,kw.length()).equalsIgnoreCase(kw))return String(labels[currentLang][L_TOMORROW])+raw.substring(kw.length());}
  for(int i=0;todayKw[i];i++){String kw(todayKw[i]);if(raw.length()>=kw.length()&&raw.substring(0,kw.length()).equalsIgnoreCase(kw)){String rest=raw.substring(kw.length());int sp=rest.indexOf(' ');return String(labels[currentLang][L_TODAY])+(sp>=0?rest.substring(sp):"");}}
  return raw;
}

// ===================== DATA STATE =====================
String weatherKey,weatherCity;
String w_temp="--",w_hum="--",w_desc="...",w_cityDisplay="",w_iconCode="";
String w_forecast_temp="--",w_forecast_desc="...",w_forecast_icon="",w_forecast_hum="--";
String w_forecast_min="--",w_forecast_max="--";
String googleScriptUrl="";
bool   hasCalendarEvent=false;
int    scrollX=0,textWidth=0;
unsigned long lastScrollUpdate=0;
#define CAL_MAX_EVENTS 5
String cal_events[CAL_MAX_EVENTS],cal_times[CAL_MAX_EVENTS];
int    cal_eventCount=0,cal_currentEventIdx=0;
const unsigned long DATA_REFRESH_INTERVAL=7200000UL;
unsigned long lastWeatherUpdate=0,lastCalendarUpdate=0;
Mode currentMode=MODE_CLOCK;
int  alarmHour=7,alarmMinute=0,alarmPatternIdx=0;
bool alarmEnabled=false,isAlarmRinging=false,muteUI=false;
int  alarmDays=0b1111111;
const char* alarmPatternNames[]={"Digital Beep","Nervous Cricket","Melodic Rise","Sci-Fi Siren"};
String otaPass="",otaUser="";

// ===================== NIGHT MODE =====================
bool checkNightMode(){
  if(nmMode==0)return false; if(nmMode==2)return true;
  int cur=hours*60+minutes,start=nmStartH*60+nmStartM,end=nmEndH*60+nmEndM;
  return(start<end)?(cur>=start&&cur<end):(cur>=start||cur<end);
}
inline bool isSoundSuppressed(){return muteUI||checkNightMode();}
bool isAlarmActiveToday(){time_t et=timeClient.getEpochTime();struct tm*p=gmtime(&et);int bit=(p->tm_wday==0)?6:p->tm_wday-1;return(alarmDays&(1<<bit))!=0;}
static void applyCpuFreq(bool on){setCpuFrequencyMhz(on?CPU_FREQ_ACTIVE:CPU_FREQ_IDLE);}
static void initPowerManagement(){esp_pm_config_t pm={.max_freq_mhz=CPU_FREQ_ACTIVE,.min_freq_mhz=CPU_FREQ_IDLE,.light_sleep_enable=true};esp_pm_configure(&pm);}
static void enableWifi6(){esp_wifi_set_protocol(WIFI_IF_STA,WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_11AX);}

// ===================== ALARM SOUNDS =====================
#if ENABLE_BUZZER
static const BuzzerStep p_classic[]={{2000,100},{0,100},{2000,100},{0,700}};
// Nervous Cricket: 3 rapid chirps then pause
static const BuzzerStep p_nervous[]={{2800,35},{0,25},{2800,35},{0,25},{2800,35},{0,600}};
// Melodic Rise: ascending arpeggio C5-E5-G5-C6 with resolution
static const BuzzerStep p_melodic[]={{523,160},{659,160},{784,160},{1047,220},{784,100},{1047,320},{0,380}};
// Sci-Fi Siren: smooth up-down frequency sweep
static const BuzzerStep p_siren[]  ={{900,70},{1100,70},{1300,70},{1500,70},{1700,70},{1500,70},{1300,70},{1100,70},{0,180}};
void startAlarmSound(){
  switch(alarmPatternIdx){
    case 1:buzzer.playPattern(p_nervous,6,true);break;
    case 2:buzzer.playPattern(p_melodic,7,true);break;
    case 3:buzzer.playPattern(p_siren,9,true);break;
    default:buzzer.playPattern(p_classic,4,true);break;
  }
}
void playSaveSound(){
  if(isSoundSuppressed())return;
  static const BuzzerStep ok[]={{880,80},{0,30},{1109,80},{0,30},{1319,200},{0,10}};
  buzzer.playPattern(ok,6,false);
}
static void playSaveSoundBlocking(){
  if(isSoundSuppressed()){delay(200);return;}
  static const BuzzerStep ok[]={{880,80},{0,30},{1109,80},{0,30},{1319,200},{0,10}};
  buzzer.playPattern(ok,6,false);
  unsigned long t=millis();while(millis()-t<450){buzzer.update();delay(1);}
}
#else
void startAlarmSound(){}
void playSaveSound(){}
static void playSaveSoundBlocking(){}
#endif

// ===================== DISPLAY DISPATCH =====================
void refreshDisplay(){
  if(inSettingsMode){drawSettingsMenu();return;}
  switch(currentMode){
    case MODE_WEATHER:case MODE_FORECAST:updateDisplayWeather();break;
    case MODE_CALENDAR:updateDisplayCalendar();break;
    default:updateDisplayClock();break;
  }
}

// ===================== HTTP HELPERS =====================
static void sendP(const char* pgm){
  char buf[129]; size_t len=strlen_P(pgm),off=0;
  while(off<len){size_t n=min(len-off,(size_t)128);memcpy_P(buf,pgm+off,n);buf[n]='\0';server.sendContent(buf);off+=n;}
}
static void sendStr(const __FlashStringHelper* s){ server.sendContent(s); }
static void sendStr(const char* s)               { if(s&&s[0]!='\0') server.sendContent(s); }
static void sendStr(const String& s)             { if(s.length()>0)  server.sendContent(s); }

// ===================== TOPBAR =====================
static void sendTopbar(const char* page){
  sendStr(F(
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
    "<meta charset='UTF-8'>"));
  sendP(CSS_VARS);
  sendStr(F("</head>"));
  sendStr(webThemeLight ? F("<body class='lm'>") : F("<body>"));
  sendStr(F("<div class='page'><div class='topbar'><span class='brand'>cuboid</span>"));
  sendStr(F("<a href='/' class='tbtn"));
  if(strcmp(page,"main")==0) sendStr(F(" active"));
  sendStr(F("'>&#9881; Config</a>"));
  sendStr(F("<a href='/advanced' class='tbtn"));
  if(strcmp(page,"advanced")==0) sendStr(F(" active"));
  sendStr(F("'>&#128296; Advanced</a>"));
  sendStr(F("<button class='tbtn theme-btn' id='themebtn' onclick='toggleTheme()'>"));
  sendStr(webThemeLight ? F("&#127769;") : F("&#10024;"));
  sendStr(F("</button>"));
#if ENABLE_BUZZER
  sendStr(F("<a href='/toggleMute' class='tbtn"));
  if(muteUI) sendStr(F(" mute-on"));
  sendStr(F("'>"));
  sendStr(muteUI ? F("&#128263;") : F("&#128266;"));
  sendStr(F("</a>"));
#endif
  sendStr(F("<button class='tbtn exp-btn' id='expBtn' onclick='togglePanel()'"
    " title='Brightness &amp; Screen'>&#9788; &#9660;</button>"));
  sendStr(F("<div id='tb-panel' class='tb-panel' style='display:none'>"));
  sendStr(F("<input type='range' id='bright-sl' min='1' max='100' step='1' value='"));
  sendStr(String(pixelBrightness));
  sendStr(F("'><span class='tb-bv' id='bv'>"));
  sendStr(String(pixelBrightness)); sendStr(F("%</span>"));
  sendStr(F("<button onclick='toggleScreenAjax(this)' class='tbtn"));
  if(!isScreenOn) sendStr(F(" mute-on"));
  sendStr(F("' id='screenBtn'>&#128421;</button></div></div>"));
  sendStr(F("<script>"
    "function togglePanel(){"
      "var p=document.getElementById('tb-panel');"
      "var b=document.getElementById('expBtn');"
      "var op=p.style.display==='none'||p.style.display==='';"
      "p.style.display=op?'flex':'none';"
      "b.innerHTML=op?'\\u2600 \\u25B2':'\\u2600 \\u25BC';"
      "b.classList.toggle('open',op);}"
    "function toggleScreenAjax(b){"
      "fetch('/toggleScreen')"
        ".then(r=>r.json())"
        ".then(d=>{d.on?b.classList.remove('mute-on'):b.classList.add('mute-on');})"
        ".catch(()=>{});}"
    "var _bt;"
    "document.addEventListener('DOMContentLoaded',function(){"
      "var sl=document.getElementById('bright-sl');"
      "if(sl)sl.addEventListener('input',function(){"
        "document.getElementById('bv').textContent=this.value+'%';"
        "clearTimeout(_bt);var v=this.value;"
        "_bt=setTimeout(function(){fetch('/setBrightness?v='+v);},350);});});"
    "</script>"));
}

// ===================== /scan =====================
void handleScan(){
  if(isAPMode){ server.send(200,"application/json","[]"); return; }
  WiFi.scanDelete();
  int n=WiFi.scanNetworks();
  String j="[";
  for(int i=0;i<n;++i){if(i>0)j+=",";j+="{\"s\":\""+WiFi.SSID(i)+"\",\"r\":"+WiFi.RSSI(i)+",\"b\":\""+WiFi.BSSIDstr(i)+"\"}";}
  j+="]";
  WiFi.scanDelete();
  server.send(200,"application/json",j);
}

// ===================== /sysinfo =====================
void handleSysInfo(){
  JsonDocument doc;
  doc["model"]=ESP.getChipModel();doc["rev"]=ESP.getChipRevision();doc["cpu_mhz"]=ESP.getCpuFreqMHz();
  doc["sdk"]=ESP.getSdkVersion();doc["mac"]=WiFi.macAddress();
  doc["flash_kb"]=(int)(ESP.getFlashChipSize()/1024);doc["sketch_kb"]=(int)(ESP.getSketchSize()/1024);
  doc["free_sketch_kb"]=(int)(ESP.getFreeSketchSpace()/1024);doc["heap_kb"]=(int)(ESP.getHeapSize()/1024);
  doc["heap_free_kb"]=(int)(ESP.getFreeHeap()/1024);doc["heap_min_kb"]=(int)(ESP.getMinFreeHeap()/1024);
  doc["psram_kb"]=(int)(ESP.getPsramSize()/1024);doc["temp_c"]=(int)temperatureRead();
  doc["ota_pass_set"]=(otaPass.length()>0);doc["ota_user_set"]=(otaUser.length()>0);
  unsigned long up=millis()/1000;char upbuf[32];
  snprintf(upbuf,sizeof(upbuf),"%lud %02lu:%02lu:%02lu",up/86400,(up%86400)/3600,(up%3600)/60,up%60);
  doc["uptime"]=upbuf;doc["wifi_ok"]=(WiFi.status()==WL_CONNECTED);
  if(WiFi.status()==WL_CONNECTED){
    doc["ssid"]=WiFi.SSID();doc["bssid"]=WiFi.BSSIDstr();doc["ip"]=WiFi.localIP().toString();
    doc["gw"]=WiFi.gatewayIP().toString();doc["mask"]=WiFi.subnetMask().toString();doc["dns"]=WiFi.dnsIP().toString();
    doc["ch"]=WiFi.channel();
    wifi_ap_record_t ap;
    if(esp_wifi_sta_get_ap_info(&ap)==ESP_OK){
      doc["rssi"]=ap.rssi;doc["auth"]=(int)ap.authmode;
      doc["b"]=(bool)ap.phy_11b;doc["g"]=(bool)ap.phy_11g;doc["n"]=(bool)ap.phy_11n;doc["ax"]=(bool)ap.phy_11ax;
      doc["pch"]=ap.primary;
      const char* bw="20 MHz";
      if(ap.second==WIFI_SECOND_CHAN_ABOVE)bw="40 MHz (+)";else if(ap.second==WIFI_SECOND_CHAN_BELOW)bw="40 MHz (-)";
      doc["bw"]=bw;doc["freq_mhz"]=(ap.primary<=14)?(2407+ap.primary*5):(5000+ap.primary*5);
    }
  }
  String out;serializeJson(doc,out);server.send(200,"application/json",out);
}

// ===================== /exportSettings =====================
void handleExportSettings(){
  preferences.begin("clock_cfg",true);
  JsonDocument doc;
  doc["ssid"]=preferences.getString("ssid","");doc["pass"]=preferences.getString("pass","");
  doc["lng"]=preferences.getInt("lng",0);doc["key"]=preferences.getString("key","");
  doc["city"]=preferences.getString("city","");doc["g_url"]=preferences.getString("g_url","");
  doc["offset"]=preferences.getInt("offset",3600);doc["dst"]=preferences.getInt("dst",1);
  doc["ah"]=preferences.getInt("ah",7);doc["am"]=preferences.getInt("am",0);
  doc["ae"]=preferences.getBool("ae",false);doc["ap"]=preferences.getInt("ap",0);
  doc["adays"]=preferences.getInt("adays",0b1111111);doc["mute"]=preferences.getBool("mute",false);
  doc["cfont"]=preferences.getInt("cfont",0);doc["df"]=preferences.getInt("df",0);
  doc["acr"]=preferences.getInt("acr",5);doc["so_anim"]=preferences.getInt("so_anim",1);
  doc["so_ms"]=preferences.getInt("so_ms",30);doc["wake_anim"]=preferences.getBool("wake_anim",false);
  doc["pct_bright"]=preferences.getInt("pct_bright",100);doc["wtheme"]=preferences.getBool("wtheme",false);
  doc["nm_mode"]=preferences.getInt("nm_mode",0);doc["nm_sh"]=preferences.getInt("nm_sh",23);
  doc["nm_sm"]=preferences.getInt("nm_sm",0);doc["nm_eh"]=preferences.getInt("nm_eh",7);
  doc["nm_em"]=preferences.getInt("nm_em",0);doc["nm_wt"]=preferences.getInt("nm_wt",5);
  doc["ab_en"]=preferences.getBool("ab_en",false);doc["ab_slots"]=preferences.getString("ab_slots","");
  preferences.end();
  doc["_version"]=1;doc["_device"]="cuboid";
  String out;serializeJsonPretty(doc,out);
  server.sendHeader("Content-Disposition","attachment; filename=\"cuboid-settings.json\"");
  server.sendHeader("Cache-Control","no-cache");
  server.send(200,"application/json",out);
}

// ===================== /importSettings =====================
void handleImportSettings(){
  String body=server.arg("plain");
  if(body.length()==0){server.send(400,"text/plain","Empty body");return;}
  JsonDocument doc;
  if(deserializeJson(doc,body)){server.send(400,"text/plain","JSON error");return;}
  if(doc["_device"].is<const char*>()&&String((const char*)doc["_device"])!="cuboid"){server.send(400,"text/plain","Wrong device");return;}
  preferences.begin("clock_cfg",false);
  if(doc["ssid"].is<const char*>())  preferences.putString("ssid",(const char*)doc["ssid"]);
  if(doc["pass"].is<const char*>())  preferences.putString("pass",(const char*)doc["pass"]);
  if(doc["lng"].is<int>())           preferences.putInt("lng",(int)doc["lng"]);
  if(doc["key"].is<const char*>())   preferences.putString("key",(const char*)doc["key"]);
  if(doc["city"].is<const char*>())  preferences.putString("city",(const char*)doc["city"]);
  if(doc["g_url"].is<const char*>()) preferences.putString("g_url",(const char*)doc["g_url"]);
  if(doc["offset"].is<int>())        preferences.putInt("offset",(int)doc["offset"]);
  if(doc["dst"].is<int>())           preferences.putInt("dst",(int)doc["dst"]);
  if(doc["ah"].is<int>())            preferences.putInt("ah",(int)doc["ah"]);
  if(doc["am"].is<int>())            preferences.putInt("am",(int)doc["am"]);
  if(doc["ae"].is<bool>())           preferences.putBool("ae",(bool)doc["ae"]);
  if(doc["ap"].is<int>())            preferences.putInt("ap",(int)doc["ap"]);
  if(doc["adays"].is<int>())         preferences.putInt("adays",(int)doc["adays"]);
  if(doc["mute"].is<bool>())         preferences.putBool("mute",(bool)doc["mute"]);
  if(doc["cfont"].is<int>())         preferences.putInt("cfont",(int)doc["cfont"]);
  if(doc["df"].is<int>())            preferences.putInt("df",(int)doc["df"]);
  if(doc["acr"].is<int>())           preferences.putInt("acr",(int)doc["acr"]);
  if(doc["so_anim"].is<int>())       preferences.putInt("so_anim",(int)doc["so_anim"]);
  if(doc["so_ms"].is<int>())         preferences.putInt("so_ms",(int)doc["so_ms"]);
  if(doc["wake_anim"].is<bool>())    preferences.putBool("wake_anim",(bool)doc["wake_anim"]);
  if(doc["pct_bright"].is<int>())    preferences.putInt("pct_bright",(int)doc["pct_bright"]);
  if(doc["wtheme"].is<bool>())       preferences.putBool("wtheme",(bool)doc["wtheme"]);
  if(doc["nm_mode"].is<int>())       preferences.putInt("nm_mode",(int)doc["nm_mode"]);
  if(doc["nm_sh"].is<int>())         preferences.putInt("nm_sh",(int)doc["nm_sh"]);
  if(doc["nm_sm"].is<int>())         preferences.putInt("nm_sm",(int)doc["nm_sm"]);
  if(doc["nm_eh"].is<int>())         preferences.putInt("nm_eh",(int)doc["nm_eh"]);
  if(doc["nm_em"].is<int>())         preferences.putInt("nm_em",(int)doc["nm_em"]);
  if(doc["nm_wt"].is<int>())         preferences.putInt("nm_wt",(int)doc["nm_wt"]);
  if(doc["ab_en"].is<bool>())        preferences.putBool("ab_en",(bool)doc["ab_en"]);
  if(doc["ab_slots"].is<const char*>()) preferences.putString("ab_slots",(const char*)doc["ab_slots"]);
  preferences.end();
  server.send(200,"text/plain","OK");
  delay(400);ESP.restart();
}

// ===================== / (main config page) =====================
void handleRoot(){
  if(WiFi.status()==WL_CONNECTED) esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200,"text/html","");
  sendTopbar("main");
  sendP(AJAX_JS);
  if(WiFi.status()==WL_CONNECTED){
    String cur=WiFi.SSID();
    if(cur.length()>0){
      sendStr(F("var o=document.createElement('option');o.value='"));
      sendStr(cur);
      sendStr(F("';o.text='"));sendStr(cur);
      sendStr(F(" (Connected)';o.selected=true;sel.add(o);"));
    }
  }
  sendStr(F("d.forEach(n=>{var o=document.createElement('option');"
    "o.value=n.s;o.text=n.s+' ('+n.r+'dBm) ['+n.b+']';sel.add(o);});"
    "ic.classList.remove('spin');}).catch(()=>ic.classList.remove('spin'));}"
    "document.addEventListener('DOMContentLoaded',scanWifi);</script>"));

  float utcHours=utcOffset/3600.0f;
  char dd_ex[3]; snprintf(dd_ex,3,"%02d",day);
  int safeWday  = constrain(wdayBit, 0, 6);
  int safeMon   = constrain(month, 1, 12) - 1;
  String fmt3 = String(dayNames[currentLang][safeWday])+" "+dd_ex+" "+monthShort[currentLang][safeMon]+" "+String(year);
  String fmt4 = String(dayNames[currentLang][safeWday])+" "+dd_ex+" "+monthShort[currentLang][safeMon];

  // ── Alarm & Time card ─────────────────────────────────────────────────
#if ENABLE_BUZZER
  sendStr(F("<div class='card'><h2>&#9200; "));
  sendStr(labels[currentLang][L_ALARM_TIME]);
  sendStr(F("</h2><form class='aform' action='/saveTimeAlarm' method='POST'>"));
  sendStr(F("<div class='row'><div class='col'><label>"));
  sendStr(labels[currentLang][L_ENABLED]);
  sendStr(F("</label><select name='ena'>"));
  sendStr(alarmEnabled?F("<option value='1' selected>ON</option><option value='0'>OFF</option>"):F("<option value='1'>ON</option><option value='0' selected>OFF</option>"));
  sendStr(F("</select></div><div class='col'><label>HH : MM</label>"
    "<div style='display:flex;gap:5px;'>"
    "<input type='number' name='h' min='0' max='23' value='"));
  sendStr(String(alarmHour));
  sendStr(F("'><input type='number' name='m' min='0' max='59' value='"));
  sendStr(String(alarmMinute));
  sendStr(F("'></div></div></div><label>"));
  sendStr(labels[currentLang][L_DAYS_ACTIVE]);
  sendStr(F("</label><div class='days-row'>"));
  for(int i=0;i<7;i++){
    bool a=(alarmDays&(1<<i))!=0;
    sendStr(F("<div class='day-pill"));if(a)sendStr(F(" on"));
    sendStr(F("' data-day='"));sendStr(String(i));sendStr(F("'>"));
    sendStr(dayNames[currentLang][i]);
    sendStr(F("</div><input type='hidden' id='day_"));sendStr(String(i));
    sendStr(F("' name='d"));sendStr(String(i));
    sendStr(a?F("' value='1'>"):F("' value='0'>"));
  }
  sendStr(F("</div><label>"));sendStr(labels[currentLang][L_PATTERN]);
  sendStr(F("</label><select name='pat'>"));
  for(int i=0;i<4;i++){
    sendStr(F("<option value='"));sendStr(String(i));
    sendStr(i==alarmPatternIdx?F("' selected>"):F("'>"));
    sendStr(alarmPatternNames[i]);sendStr(F("</option>"));
  }
  sendStr(F("</select><hr class='sep'>"));
#else
  sendStr(F("<div class='card'><h2>&#9200; "));
  sendStr(labels[currentLang][L_TIME]);
  sendStr(F("</h2><form class='aform' action='/saveTimeAlarm' method='POST'>"));
#endif
  sendStr(F("<div class='row'><div class='col'><label>"));
  sendStr(labels[currentLang][L_TZ]);
  sendStr(F(" (h)</label><input type='number' name='offset' step='0.5' min='-12' max='14' value='"));
  sendStr(String(utcHours,1));
  sendStr(F("'></div><div class='col'><label>Auto DST</label><select name='dst'>"));
  sendStr(autoDST?F("<option value='1' selected>ON</option><option value='0'>OFF</option>"):F("<option value='1'>ON</option><option value='0' selected>OFF</option>"));
  sendStr(F("</select></div></div><button class='save-btn' type='submit'>"));
  sendStr(labels[currentLang][L_SAVE]);
  sendStr(F("</button></form></div>"));

  // ── Personalization card ───────────────────────────────────────────────
  sendStr(F("<div class='card'><h2>&#127912; "));
  sendStr(labels[currentLang][L_PERSONAL]);
  sendStr(F("</h2><form class='aform' action='/savePersonalization' method='POST'><label>"));
  sendStr(labels[currentLang][L_CLOCK_FONT]);
  sendStr(F("</label><div class='font-preview'>"));
  for(int i=0;i<3;i++){
    sendStr(F("<div class='fp"));if(clockFont==i)sendStr(F(" on"));
    sendStr(F("' data-font='"));sendStr(String(i));sendStr(F("'>"));
    sendStr(clockFontNames[i]);sendStr(F("</div>"));
  }
  sendStr(F("</div><input type='hidden' id='cfont_inp' name='cfont' value='"));
  sendStr(String(clockFont));
  sendStr(F("'><label>"));sendStr(labels[currentLang][L_DATE_STYLE]);
  sendStr(F("</label><select name='df'>"));
  {
    const char* fmts[]={"DD/MM/YYYY","MM/DD/YYYY","YYYY/MM/DD",nullptr,nullptr};
    for(int i=0;i<5;i++){
      sendStr(F("<option value='"));sendStr(String(i));
      sendStr(dateFormat==i?F("' selected>"):F("'>"));
      if(i==3)sendStr(fmt3);else if(i==4)sendStr(fmt4);else sendStr(fmts[i]);
      sendStr(F("</option>"));
    }
  }
  sendStr(F("</select><label>"));sendStr(labels[currentLang][L_AUTO_CLOCK]);
  sendStr(F("</label><input type='number' name='acr' min='0' max='120' value='"));
  sendStr(String(autoClockReturnMin));
  sendStr(F("'><div class='ibox'>"));
  if(currentLang==1)      sendStr(F("Torna all'orologio dopo N minuti di inattivita'. 0=off."));
  else if(currentLang==2) sendStr(F("Retour horloge apres N minutes. 0=desactive."));
  else if(currentLang==3) sendStr(F("Vuelve al reloj tras N minutos. 0=desactivado."));
  else if(currentLang==4) sendStr(F("Kehrt nach N Minuten zur Uhr zuruck. 0=aus."));
  else                    sendStr(F("Returns to clock after N minutes of inactivity. 0=disabled."));
  sendStr(F("</div><hr class='sep'><label>Screen-off effect</label><select name='so_anim'>"));
  for(int i=0;i<SCREEN_OFF_ANIM_COUNT;i++){
    sendStr(F("<option value='"));sendStr(String(i));
    sendStr(screenOffAnim==i?F("' selected>"):F("'>"));
    sendStr(screenOffAnimNames[i]);sendStr(F("</option>"));
  }
  sendStr(F("</select><label>Step delay (ms)</label>"
    "<input type='number' name='so_ms' min='1' max='500' value='"));
  sendStr(String(screenOffStepMs));
  sendStr(F("'><div class='ibox'>12=fast &middot; 30=medium &middot; 55=slow</div>"
    "<hr class='sep'><label>Wake animation</label><select name='wake_anim'>"));
  sendStr(wakeAnim?
    F("<option value='1' selected>ON &ndash; mirror screen-off on wake</option><option value='0'>OFF &ndash; instant on</option>"):
    F("<option value='1'>ON &ndash; mirror screen-off on wake</option><option value='0' selected>OFF &ndash; instant on</option>"));
  sendStr(F("</select><button class='save-btn' type='submit'>"));
  sendStr(labels[currentLang][L_SAVE]);
  sendStr(F("</button></form></div>"));

  // ── Night Mode card ────────────────────────────────────────────────────
  sendStr(F("<div class='card'><h2>&#127769; "));
  sendStr(labels[currentLang][L_NM_SECTION]);
  sendStr(F("</h2><form class='aform' action='/saveNightMode' method='POST'>"
    "<label>Mode</label>"
    "<select name='nm_mode' onchange=\"var v=this.value;"
      "document.getElementById('nm_sched').style.display=(v==='1')?'block':'none';"
      "document.getElementById('nm_wake_row').style.display=(v!=='0')?'block':'none';\">"
    "<option value='0'"));if(nmMode==0)sendStr(F(" selected"));
  sendStr(F(">Off</option><option value='1'"));if(nmMode==1)sendStr(F(" selected"));
  sendStr(F(">Scheduled</option><option value='2'"));if(nmMode==2)sendStr(F(" selected"));
  sendStr(F(">Manual (always on)</option></select>"
    "<div id='nm_sched' style='display:"));
  sendStr(nmMode==1?F("block'"):F("none'"));
  sendStr(F("><div class='row'>"
    "<div class='col'><label>Start HH:MM</label><div style='display:flex;gap:5px;'>"
    "<input type='number' name='nm_sh' min='0' max='23' value='"));sendStr(String(nmStartH));
  sendStr(F("'><input type='number' name='nm_sm' min='0' max='59' value='"));sendStr(String(nmStartM));
  sendStr(F("'></div></div>"
    "<div class='col'><label>End HH:MM</label><div style='display:flex;gap:5px;'>"
    "<input type='number' name='nm_eh' min='0' max='23' value='"));sendStr(String(nmEndH));
  sendStr(F("'><input type='number' name='nm_em' min='0' max='59' value='"));sendStr(String(nmEndM));
  sendStr(F("'></div></div></div></div>"
    "<div id='nm_wake_row' style='display:"));
  sendStr(nmMode>0?F("block'"):F("none'"));
  sendStr(F("><label>Wake duration (s, 0=stay on)</label>"
    "<input type='number' name='nm_wt' min='0' max='3600' value='"));sendStr(String(nmWakeTime));
  sendStr(F("'></div><button class='save-btn' type='submit'>"));
  sendStr(labels[currentLang][L_SAVE]);
  sendStr(F("</button></form></div>"));

  // ── Auto Brightness card ──────────────────────────────────────────────
  sendStr(F("<div class='card'><h2>&#9728; Auto Brightness</h2>"
    "<form class='aform' action='/saveAutoBright' method='POST'>"));
  sendStr(F("<label>Mode</label><select name='ab_en'"
    " onchange=\"document.getElementById('_ab_sec').style.display="
    "this.value==='1'?'block':'none'\">"));
  sendStr(autoBrightEnabled
    ? F("<option value='0'>OFF &ndash; manual only</option>"
        "<option value='1' selected>ON &ndash; time schedule</option>")
    : F("<option value='0' selected>OFF &ndash; manual only</option>"
        "<option value='1'>ON &ndash; time schedule</option>"));
  sendStr(F("</select><div id='_ab_sec' style='display:"));
  sendStr(autoBrightEnabled ? F("block'>") : F("none'>"));
  sendStr(F("<div class='ibox' style='margin-top:10px;'>"
    "Each slot is active from that time until the next one (cyclic, 24h). "
    "Up to 8 slots. Manual slider overrides until the next minute tick."
    "</div>"));
  sendStr(F("<div style='display:grid;grid-template-columns:90px 1fr 44px;"
    "gap:6px;align-items:center;margin-top:10px;padding:4px 0;"
    "border-bottom:1px solid var(--sep);font-size:11px;color:var(--muted);"
    "font-weight:700;letter-spacing:.4px;text-transform:uppercase;'>"
    "<span>From</span><span>Brightness</span><span></span></div>"));
  sendStr(F("<div id='_ab_rows'></div>"));
  sendStr(F("<button type='button' onclick='abAdd()'"
    " style='width:100%;margin-top:8px;padding:10px;"
    "background:var(--teal);color:var(--hvr);border:none;"
    "border-radius:9px;font-weight:800;font-size:13px;"
    "cursor:pointer;letter-spacing:.4px;'>+ Add slot</button>"));
  sendStr(F("<input type='hidden' id='_ab_slots' name='ab_slots' value='"));
  sendStr(abSerialize());
  sendStr(F("'></div>"));
  sendStr(F("<button class='save-btn' type='submit'>SAVE</button></form></div>"));
  sendP(AB_JS);

  // ── Weather & Calendar card ────────────────────────────────────────────
  sendStr(F("<div class='card'><h2>&#9925; "));
  sendStr(labels[currentLang][L_WEATHER_CAL]);
  sendStr(F("</h2><form class='aform' action='/saveAPI' method='POST'>"
    "<div class='lrow'><span>OpenWeatherMap API Key</span>"
    "<a href='https://home.openweathermap.org/api_keys' target='_blank'>&#8599; Get key</a></div>"
    "<input type='text' name='w_key' value='"));sendStr(weatherKey);
  sendStr(F("'><label>"));sendStr(labels[currentLang][L_CITY]);
  sendStr(F("</label><input type='text' name='w_city' value='"));sendStr(weatherCity);
  sendStr(F("'><div class='lrow'><span>Google Script URL</span>"
    "<a href='https://script.google.com/home/' target='_blank'>&#8599; Apps Script</a></div>"
    "<input type='text' name='g_url' placeholder='https://script.google.com/macros/s/...' value='"));
  sendStr(googleScriptUrl);
  sendStr(F("'><button class='save-btn' type='submit'>"));
  sendStr(labels[currentLang][L_SAVE]);
  sendStr(F("</button></form></div>"));

  // ── WiFi & System card ─────────────────────────────────────────────────
  sendStr(F("<div class='card'><h2>&#128225; "));
  sendStr(labels[currentLang][L_WIFI_SYS]);
  sendStr(F("</h2><form class='aform' action='/saveSystem' method='POST' data-restart><label>"));
  sendStr(labels[currentLang][L_WIFI]);
  sendStr(F(" SSID</label><div class='wrow'><select name='ssid' id='ssid' class='wsel'>"));
  {String cur=WiFi.SSID();if(WiFi.status()==WL_CONNECTED&&cur.length()>0){sendStr(F("<option value='"));sendStr(cur);sendStr(F("' selected>"));sendStr(cur);sendStr(F(" (Connected)</option>"));}}
  sendStr(F("</select><button type='button' class='icon-btn' onclick='scanWifi()' title='Rescan'>"
    "<svg id='scIco' viewBox='0 0 24 24'><path d='M17.65 6.35C16.2 4.9 14.21 4 12 4c-4.42 0-7.99 3.58-7.99 8"
    "s3.57 8 7.99 8c3.73 0 6.84-2.55 7.73-6h-2.08c-.82 2.33-3.04 4-5.65 4-3.31 0-6-2.69-6-6s2.69-6 6-6"
    "c1.66 0 3.14.69 4.22 1.78L13 11h7V4l-2.35 2.35z'/></svg></button></div><label>"));
  sendStr(labels[currentLang][L_PASS]);
  sendStr(F("</label><input type='password' name='pass' placeholder='Leave empty to keep current'><label>"));
  sendStr(labels[currentLang][L_LANG]);
  sendStr(F("</label><select name='lng'>"));
  for(int i=0;i<5;i++){
    sendStr(F("<option value='"));sendStr(String(i));
    sendStr(currentLang==i?F("' selected>"):F("'>"));
    sendStr(langNames[i]);sendStr(F("</option>"));
  }
  sendStr(F("</select><button class='save-btn' type='submit'>"));
  sendStr(labels[currentLang][L_SAVE]);
  sendStr(F(" &amp; Reboot</button></form></div>"));
  sendStr(F("</div></body></html>"));
  server.sendContent("");
}

// ===================== /advanced =====================
void handleAdvanced(){
  if(WiFi.status()==WL_CONNECTED) esp_wifi_set_ps(WIFI_PS_NONE);

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200,"text/html","");
  sendTopbar("advanced");
  sendP(AJAX_JS);
  sendStr(F("}).catch(()=>{}); }</script>"));

  sendStr(F("<div class='card'><h2>&#128295; Firmware Update</h2>"
    "<p style='font-size:13px;color:var(--muted);margin:0 0 10px;'>Flash a .bin file directly from the browser:</p>"
    "<form method='POST' action='/update' enctype='multipart/form-data'>"
    "<input type='file' name='update' accept='.bin' style='background:transparent;padding:0;border:none;color:var(--txt);width:100%;'>"
    "<input type='submit' value='Flash Firmware' class='save-btn' style='margin-top:14px;cursor:pointer;'></form>"
    "<hr class='sep'>"
    "<p style='font-size:12px;color:var(--muted);margin:8px 0 2px;'>Arduino IDE OTA: device visible as <b>cuboid.local</b> on port 3232.</p>"));
  if(otaPass.length()==0&&otaUser.length()==0){
    sendStr(F("<form class='aform' action='/saveOTAPass' method='POST' data-restart>"
      "<label>OTA Username</label><input type='text' name='new_user' autocomplete='username'>"
      "<label>OTA Password</label><input type='password' name='new_pass' autocomplete='new-password'>"
      "<button class='save-btn' type='submit'>Set &amp; Reboot</button></form>"));
  } else {
    sendStr(F("<p style='font-size:12px;color:var(--teal);margin:6px 0 2px;'>&#128274; OTA credentials active.</p>"
      "<form class='aform' action='/saveOTAPass' method='POST' data-restart>"
      "<label>Current Password</label><input type='password' name='cur_pass' autocomplete='current-password'>"
      "<label>New Username</label><input type='text' name='new_user' autocomplete='username' value='"));
    sendStr(otaUser);
    sendStr(F("'><label>New Password <span style='color:var(--muted);font-size:10px;'>(empty = remove)</span></label>"
      "<input type='password' name='new_pass' autocomplete='new-password'>"
      "<button class='save-btn' type='submit'>Update &amp; Reboot</button></form>"));
  }
  sendStr(F("</div>"));

  sendStr(F("<div class='card'><h2>&#128190; Settings Backup &amp; Restore</h2>"
    "<p style='font-size:13px;color:var(--muted);margin:0 0 12px;'>"
    "Export all settings to <code style='color:var(--teal);'>cuboid-settings.json</code>, "
    "or import a previously saved file. The device reboots after import.</p>"
    "<a href='/exportSettings' class='save-btn' style='text-decoration:none;display:block;text-align:center;margin-bottom:10px;'>"
    "&#11123; Export cuboid-settings.json</a>"
    "<hr class='sep'>"
    "<label>Import settings file</label>"
    "<input type='file' id='impFile' accept='.json' style='background:transparent;padding:4px 0;border:none;color:var(--txt);width:100%;'>"
    "<button class='save-btn' id='impBtn' onclick='importSettings()' style='margin-top:10px;'>&#11121; Import &amp; Reboot</button>"
    "<div id='impMsg' style='font-size:12px;margin-top:8px;color:var(--muted);'></div></div>"));

  sendStr(F("<div class='card'><h2>&#128161; System Info "
    "<span id='rtag' style='font-size:10px;color:var(--muted);font-weight:normal;margin-left:6px;'>loading...</span></h2>"
    "<div id='si'><p style='color:var(--muted);font-size:12px;'>-</p></div></div>"
    "<div class='card'><h2>&#128246; WiFi Details</h2>"
    "<div id='wi'><p style='color:var(--muted);font-size:12px;'>-</p></div></div>"));

  sendStr(F(R"rawjs(
<script>
async function importSettings(){
  const f=document.getElementById('impFile').files[0];
  const msg=document.getElementById('impMsg');const btn=document.getElementById('impBtn');
  if(!f){msg.style.color='var(--rssi-bad)';msg.textContent='No file selected.';return;}
  const text=await f.text();let parsed;
  try{parsed=JSON.parse(text);}catch(e){msg.style.color='var(--rssi-bad)';msg.textContent='Invalid JSON: '+e.message;return;}
  btn.textContent='Uploading...';btn.disabled=true;
  try{
    const res=await fetch('/importSettings',{method:'POST',headers:{'Content-Type':'application/json'},body:text});
    if(res.ok){msg.style.color='var(--rssi-ok)';let s=6;const t=()=>{msg.textContent='Imported \u2713 \u2014 Rebooting in '+s+'s\u2026';if(--s>0)setTimeout(t,1000);else location.reload();};t();}
    else{const err=await res.text();msg.style.color='var(--rssi-bad)';msg.textContent='Error: '+err;btn.textContent='Import & Reboot';btn.disabled=false;}
  }catch(e){msg.style.color='var(--rssi-bad)';msg.textContent='Network error: '+e.message;btn.textContent='Import & Reboot';btn.disabled=false;}
}
const AUTH=['Open','WEP','WPA-PSK','WPA2-PSK','WPA/WPA2','WPA2-Ent','WPA3-PSK','WPA2/WPA3','WAPI','OWE','WPA3-Ent'];
function authName(n){return AUTH[n]||'Unknown ('+n+')';}
function kv(k,v){return '<div class="k">'+k+'</div><div class="v">'+v+'</div>';}
function yn(b){return b?'<span style="color:var(--teal)">Yes</span>':'<span style="color:var(--muted)">No</span>';}
function pct(f,t){return t>0?Math.round((1-f/t)*100):0;}
function bar(p){var c=p>80?'var(--rssi-bad)':p>55?'var(--rssi-warn)':'var(--rssi-ok)';return '<div class="bar-wrap"><div class="bar" style="width:'+p+'%;background:'+c+'"></div></div>';}
async function poll(){
  try{
    const d=await fetch('/sysinfo').then(r=>r.json());
    document.getElementById('rtag').textContent='live';
    var hp=pct(d.heap_free_kb,d.heap_kb);
    document.getElementById('si').innerHTML='<div class="ig">'+kv('Chip',d.model+' rev '+d.rev)+kv('CPU',d.cpu_mhz+' MHz')+kv('SDK',d.sdk)+kv('MAC',d.mac)+kv('Temp',d.temp_c+'\u00b0C')+kv('Uptime',d.uptime)+kv('OTA user',d.ota_user_set?'Set':'Not set')+kv('OTA pass',d.ota_pass_set?'Set':'Not set')+kv('Flash',d.flash_kb+' KB')+kv('Sketch',d.sketch_kb+' KB / free '+d.free_sketch_kb+' KB')+kv('Heap',d.heap_free_kb+' KB free / '+d.heap_kb+' KB ('+hp+'% used)')+(d.psram_kb>0?kv('PSRAM',d.psram_kb+' KB'):'')+kv('Heap min',d.heap_min_kb+' KB')+'</div>'+bar(hp);
    if(d.wifi_ok){document.getElementById('wi').innerHTML='<div class="ig">'+kv('SSID',d.ssid)+kv('BSSID',d.bssid)+kv('IP',d.ip)+kv('Gateway',d.gw)+kv('Subnet',d.mask)+kv('DNS',d.dns)+kv('Channel',d.pch+' ('+d.freq_mhz+' MHz)')+kv('Bandwidth',d.bw)+kv('RSSI',d.rssi+' dBm')+kv('Security',authName(d.auth))+kv('802.11b',yn(d.b))+kv('802.11g',yn(d.g))+kv('802.11n (HT)',yn(d.n))+kv('Wi-Fi 6 (ax)',yn(d.ax))+'</div>';}
    else document.getElementById('wi').innerHTML='<p style="color:var(--rssi-bad);font-size:12px;">Not connected</p>';
  }catch(e){document.getElementById('rtag').textContent='error';}
}
poll();setInterval(poll,4000);
</script>
)rawjs"));
  sendStr(F("</div></body></html>"));
  server.sendContent("");
}

// ===================== Form handlers =====================
void handleSetBrightness(){
  int pb=server.arg("v").toInt();if(pb<1||pb>100)pb=100;
  pixelBrightness=pb;applyContrast(pixelBrightness);
  preferences.begin("clock_cfg",false);preferences.putInt("pct_bright",pixelBrightness);preferences.end();
  server.send(200,"text/plain","OK");yield();
}

void handleToggleScreen(){
  if(isScreenOn){
    if(nmMode==2)wakeScreenUntil=0;
    else{animateScreenOff();display.ssd1306_command(SSD1306_DISPLAYOFF);u8g2.setPowerSave(1);isScreenOn=false;manualScreenOff=true;applyCpuFreq(false);}
  } else {
    wakeScreenUntil=(nmWakeTime>0)?millis()+(nmWakeTime*1000UL):ULONG_MAX;
    manualScreenOff=false;applyCpuFreq(true);
    display.ssd1306_command(SSD1306_DISPLAYON);u8g2.setPowerSave(0);isScreenOn=true;refreshDisplay();
  }
  server.send(200,"application/json",isScreenOn?"{\"on\":true}":"{\"on\":false}");
  yield();
}

void handleToggleMute(){
#if ENABLE_BUZZER
  muteUI=!muteUI;preferences.begin("clock_cfg",false);preferences.putBool("mute",muteUI);preferences.end();
#endif
  server.sendHeader("Location","/");server.send(303);yield();
}
void handleToggleTheme(){
  webThemeLight=!webThemeLight;preferences.begin("clock_cfg",false);preferences.putBool("wtheme",webThemeLight);preferences.end();
  server.send(200,"text/plain","OK");yield();
}
void handleSaveTimeAlarm(){
  preferences.begin("clock_cfg",false);
#if ENABLE_BUZZER
  alarmHour=server.arg("h").toInt();alarmMinute=server.arg("m").toInt();
  alarmEnabled=server.arg("ena").toInt();alarmPatternIdx=server.arg("pat").toInt();
  alarmDays=0;for(int i=0;i<7;i++)if(server.arg("d"+String(i))=="1")alarmDays|=(1<<i);
  preferences.putInt("ah",alarmHour);preferences.putInt("am",alarmMinute);
  preferences.putBool("ae",alarmEnabled);preferences.putInt("ap",alarmPatternIdx);preferences.putInt("adays",alarmDays);
#endif
  utcOffset=(int)(server.arg("offset").toFloat()*3600.0f);autoDST=server.arg("dst").toInt();
  preferences.putInt("offset",utcOffset);preferences.putInt("dst",autoDST);preferences.end();
  timeClient.setTimeOffset(utcOffset);
  server.send(200,"text/plain","OK");
  playSaveSound();
  yield();
}
void handleSavePersonalization(){
  clockFont=server.arg("cfont").toInt();dateFormat=server.arg("df").toInt();
  if(clockFont<0||clockFont>2)clockFont=0;if(dateFormat<0||dateFormat>4)dateFormat=0;
  int acr=server.arg("acr").toInt();if(acr<0)acr=0;
  autoClockReturnMin=acr;modeChangedAt=millis();
  int soAnim=server.arg("so_anim").toInt();int soMs=server.arg("so_ms").toInt();
  if(soAnim<0||soAnim>=SCREEN_OFF_ANIM_COUNT)soAnim=1;if(soMs<1||soMs>500)soMs=30;
  screenOffAnim=soAnim;screenOffStepMs=soMs;
  wakeAnim=(server.arg("wake_anim").toInt()==1);
  int pb=server.arg("pct_bright").toInt();if(pb<1||pb>100)pb=100;
  pixelBrightness=pb;applyContrast(pixelBrightness);
  preferences.begin("clock_cfg",false);
  preferences.putInt("cfont",clockFont);preferences.putInt("df",dateFormat);preferences.putInt("acr",autoClockReturnMin);
  preferences.putInt("so_anim",screenOffAnim);preferences.putInt("so_ms",screenOffStepMs);
  preferences.putBool("wake_anim",wakeAnim);preferences.putInt("pct_bright",pixelBrightness);
  preferences.end();
  server.send(200,"text/plain","OK");
  playSaveSound();
  refreshDisplay();yield();
}
void handleSaveAPI(){
  weatherKey=server.arg("w_key");weatherCity=server.arg("w_city");googleScriptUrl=server.arg("g_url");
  preferences.begin("clock_cfg",false);preferences.putString("key",weatherKey);preferences.putString("city",weatherCity);preferences.putString("g_url",googleScriptUrl);preferences.end();
  if(weatherKey!="")fetchWeather();if(googleScriptUrl!="")fetchCalendar();
  if((currentMode==MODE_WEATHER||currentMode==MODE_FORECAST)&&weatherKey=="")currentMode=MODE_CLOCK;
  if(currentMode==MODE_CALENDAR&&googleScriptUrl=="")currentMode=MODE_CLOCK;
  server.send(200,"text/plain","OK");
  playSaveSound();
  yield();
}
void handleSaveSystem(){
  String newSSID=server.arg("ssid"),newPass=server.arg("pass");
  preferences.begin("clock_cfg",false);
  if(newPass.length()>0){preferences.putString("ssid",newSSID);preferences.putString("pass",newPass);}
  preferences.putInt("lng",server.arg("lng").toInt());preferences.end();
  server.send(200,"text/plain","OK");
  playSaveSoundBlocking();
  ESP.restart();
}
void handleSaveNightMode(){
  preferences.begin("clock_cfg",false);
  nmMode=server.arg("nm_mode").toInt();if(nmMode<0||nmMode>2)nmMode=0;
  nmStartH=server.arg("nm_sh").toInt();nmStartM=server.arg("nm_sm").toInt();
  nmEndH=server.arg("nm_eh").toInt();nmEndM=server.arg("nm_em").toInt();
  nmWakeTime=server.arg("nm_wt").toInt();if(nmWakeTime<0)nmWakeTime=0;
  preferences.putInt("nm_mode",nmMode);preferences.putInt("nm_sh",nmStartH);preferences.putInt("nm_sm",nmStartM);
  preferences.putInt("nm_eh",nmEndH);preferences.putInt("nm_em",nmEndM);preferences.putInt("nm_wt",nmWakeTime);preferences.end();
  server.send(200,"text/plain","OK");
  playSaveSound();
  yield();
}
void handleSaveAutoBright(){
  autoBrightEnabled=(server.arg("ab_en").toInt()==1);
  String raw=server.arg("ab_slots");
  if(raw.length()>0) abParse(raw);
  else autoBrightSlotCount=0;
  if(autoBrightEnabled){lastAutoBrightMinute=-1;applyAutoBrightness();}
  preferences.begin("clock_cfg",false);
  preferences.putBool("ab_en",autoBrightEnabled);
  preferences.putString("ab_slots",abSerialize());
  preferences.end();
  server.send(200,"text/plain","OK");
  // FIX: use blocking variant — in AP/captive-portal mode server.handleClient() can monopolise
  // the loop for hundreds of ms, starving buzzer.update() and leaving tone() running forever.
  playSaveSoundBlocking();
  yield();
}
void handleSaveOTAPass(){
  if(otaPass.length()>0&&server.arg("cur_pass")!=otaPass){server.send(403,"text/plain","Wrong password");return;}
  String nu=server.arg("new_user"),np=server.arg("new_pass");
  if(nu.length()>0)otaUser=nu;otaPass=np;
  preferences.begin("clock_cfg",false);preferences.putString("ota_pass",otaPass);preferences.putString("ota_user",otaUser);preferences.end();
  server.send(200,"text/plain","OK");
  playSaveSoundBlocking();
  ESP.restart();
}

// ===================== DATA FETCH =====================
void fetchWeather(){
  if(weatherKey==""||WiFi.status()!=WL_CONNECTED)return;
  const char* codes[]={"en","it","fr","es","de"};
  WiFiClient client;
  HTTPClient http;
  String url="http://api.openweathermap.org/data/2.5/weather?q="+weatherCity+"&appid="+weatherKey+"&units=metric&lang="+codes[currentLang];
  if(http.begin(client,url)){
    if(http.GET()==200){
      JsonDocument doc;
      deserializeJson(doc,http.getStream());
      w_temp=String((float)doc["main"]["temp"],1)+"C";
      w_hum=String((int)doc["main"]["humidity"]);
      w_desc=(const char*)doc["weather"][0]["description"];
      w_cityDisplay=(const char*)doc["name"];
      w_iconCode=(const char*)doc["weather"][0]["icon"];
    }
    http.end();
  }
  yield();
  String urlF="http://api.openweathermap.org/data/2.5/forecast?q="+weatherCity+"&appid="+weatherKey+"&units=metric&cnt=9&lang="+codes[currentLang];
  if(http.begin(client,urlF)){
    if(http.GET()==200){
      JsonDocument doc;
      deserializeJson(doc,http.getStream());
      JsonObject slot=doc["list"][8];
      if(!slot.isNull()){
        w_forecast_temp=String((float)slot["main"]["temp"],1)+"C";
        w_forecast_desc=(const char*)slot["weather"][0]["description"];
        w_forecast_icon=(const char*)slot["weather"][0]["icon"];
        w_forecast_hum=String((int)slot["main"]["humidity"]);
      }
      float minT=100.f,maxT=-100.f;
      for(JsonVariant v:doc["list"].as<JsonArray>()){float t=v["main"]["temp"];if(t<minT)minT=t;if(t>maxT)maxT=t;}
      w_forecast_min=String(minT,0);w_forecast_max=String(maxT,0);
    }
    http.end();
  }
}
void fetchCalendar(){
  if(googleScriptUrl==""||WiFi.status()!=WL_CONNECTED)return;
  WiFiClientSecure client;client.setInsecure();
  HTTPClient http;http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if(http.begin(client,googleScriptUrl)){
    if(http.GET()==200){
      JsonDocument doc;
      if(!deserializeJson(doc,http.getStream())){
        cal_eventCount=0;cal_currentEventIdx=0;hasCalendarEvent=doc["hasEvent"];
        if(hasCalendarEvent){
          if(doc["events"].is<JsonArray>()){
            for(JsonVariant ev:doc["events"].as<JsonArray>()){
              if(cal_eventCount>=CAL_MAX_EVENTS)break;
              cal_events[cal_eventCount]=(const char*)ev["event"];
              cal_times[cal_eventCount]=(const char*)ev["time"];
              cal_eventCount++;
            }
          } else {
            cal_events[0]=(const char*)doc["event"];cal_times[0]=(const char*)doc["time"];cal_eventCount=1;
          }
          if(cal_eventCount==0)hasCalendarEvent=false;
          scrollX=0;textWidth=0;
        }
      }
    }
    http.end();
  }
  yield();
}

// ===================== DISPLAY =====================
void updateDisplayClock(){
  display.clearDisplay();display.setTextColor(WHITE);display.setFont(clockFontList[clockFont]);
  String t=(hours<10?"0":"")+String(hours)+":"+(minutes<10?"0":"")+String(minutes);
  int16_t x1,y1;uint16_t w,h;display.getTextBounds(t,0,0,&x1,&y1,&w,&h);
  int off=minutes%2;display.setCursor(((SCREEN_WIDTH-w)/2)+off,40+off);display.print(t);
  display.drawLine(0,48,128,48,WHITE);display.setFont();display.setTextSize(1);
  char dd[3],mm_[3];String yy=String(year);snprintf(dd,3,"%02d",day);snprintf(mm_,3,"%02d",month);
  String dateS;
  if(dateFormat==1)dateS=String(mm_)+"/"+dd+"/"+yy;
  else if(dateFormat==2)dateS=yy+"/"+mm_+"/"+dd;
  else if(dateFormat==3)dateS=String(dayNames[currentLang][wdayBit])+" "+dd+" "+monthShort[currentLang][month-1]+" "+yy;
  else if(dateFormat==4)dateS=String(dayNames[currentLang][wdayBit])+" "+dd+" "+monthShort[currentLang][month-1];
  else dateS=String(dd)+"/"+mm_+"/"+yy;
  display.getTextBounds(dateS,0,0,&x1,&y1,&w,&h);display.setCursor((SCREEN_WIDTH-w)/2,53);display.print(dateS);
#if ENABLE_BUZZER
  if(alarmEnabled){time_t t2=timeClient.getEpochTime()+86400UL;struct tm*tp=gmtime(&t2);int tb=(tp->tm_wday==0)?6:tp->tm_wday-1;if(alarmDays&(1<<tb))display.fillCircle(120,56,3,WHITE);else display.drawCircle(120,56,3,WHITE);}
#endif
  if(hasCalendarEvent){display.drawRect(5,52,8,8,WHITE);display.drawLine(5,54,13,54,WHITE);display.drawPixel(9,57,WHITE);}
  display.display();yield();
}
void drawWeatherIcon(const String& code,int x,int y,bool large){
  int g=64;if(code.length()>=2){String id=code.substring(0,2);if(id=="01")g=69;else if(id=="02"||id=="03")g=65;else if(id=="09"||id=="10")g=67;else if(id=="11")g=66;else if(id=="13")g=73;}
  u8g2.setFont(large?u8g2_font_open_iconic_weather_4x_t:u8g2_font_open_iconic_weather_2x_t);u8g2.drawGlyph(x,y,g);
}
void updateDisplayWeather(){
  bool fc=(currentMode==MODE_FORECAST);u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB14_tf);u8g2.setCursor(0,22);u8g2.print(fc?w_forecast_temp:w_temp);
  drawWeatherIcon(fc?w_forecast_icon:w_iconCode,90,48,true);
  u8g2.setFont(u8g2_font_6x10_tr);u8g2.setCursor(0,35);u8g2.print(labels[currentLang][L_HUM]);u8g2.print(fc?w_forecast_hum:w_hum);u8g2.print("%");
  if(fc){u8g2.setFont(u8g2_font_5x7_tr);int tw=u8g2.getStrWidth(labels[currentLang][L_TOMORROW]);u8g2.setCursor(128-tw,10);u8g2.print(labels[currentLang][L_TOMORROW]);u8g2.setFont(u8g2_font_6x10_tr);u8g2.setCursor(0,48);u8g2.print("v "+w_forecast_min+" ^ "+w_forecast_max);}
  else{u8g2.setFont(u8g2_font_6x10_tr);u8g2.setCursor(0,48);u8g2.print(w_cityDisplay);}
  u8g2.setFont(u8g2_font_5x7_tr);u8g2.setCursor(0,62);u8g2.print(fc?w_forecast_desc:w_desc);
  u8g2.sendBuffer();yield();
}
void updateDisplayCalendar(){
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_open_iconic_embedded_2x_t);u8g2.drawFrame(2,7,8,8);u8g2.drawLine(2,9,10,9);u8g2.drawPixel(6,12);
  u8g2.setFont(u8g2_font_6x12_tr);u8g2.setCursor(20,15);u8g2.print(labels[currentLang][L_CAL]);
  if(hasCalendarEvent&&cal_eventCount>0){char cb[8];snprintf(cb,sizeof(cb),"%d/%d",cal_currentEventIdx+1,cal_eventCount);u8g2.setFont(u8g2_font_5x7_tr);int cw=u8g2.getStrWidth(cb);u8g2.setCursor(127-cw,10);u8g2.print(cb);}
  u8g2.drawHLine(0,20,128);
  if(hasCalendarEvent&&cal_eventCount>0){
    String dt=localizeCalTime(cal_times[cal_currentEventIdx]);const String&ev=cal_events[cal_currentEventIdx];
    String dp="",to=dt;
    if(dt.length()>0&&!isDigit((unsigned char)dt[0])){int sp=dt.indexOf(' ');if(sp>0){dp=dt.substring(0,sp);to=dt.substring(sp+1);}}
    if(dp.length()>0){u8g2.setFont(u8g2_font_5x7_tr);u8g2.setCursor(0,31);u8g2.print(dp);u8g2.setFont(u8g2_font_helvB10_tf);u8g2.setCursor(0,44);u8g2.print(to);}
    else{u8g2.setFont(u8g2_font_helvB10_tf);u8g2.setCursor(0,40);u8g2.print(to);}
    u8g2.setFont(u8g2_font_6x10_tr);
    if(textWidth==0)textWidth=u8g2.getStrWidth(ev.c_str());
    u8g2.setCursor(textWidth>128?-scrollX:0,57);u8g2.print(ev);
  } else{u8g2.setFont(u8g2_font_6x10_tr);u8g2.setCursor(10,45);u8g2.print(labels[currentLang][L_NO_EVENT]);}
  u8g2.sendBuffer();yield();
}

// ===================== SETTINGS MENU =====================
#define SETTINGS_COUNT 12
static const char* settingNames[SETTINGS_COUNT]={"< EXIT","Screen","Alarm","WiFi","Font","Mute","NightMode","DateFmt","Language","Anim","AutoReturn","Bright"};
static const char* dateFmtShort[]={"DD/MM/YY","MM/DD/YY","YY/MM/DD","Long","Med"};

static bool isDefault(int i){
  switch(i){
    case 1:return isScreenOn;case 2:return!alarmEnabled;case 3:return!isAPMode;
    case 4:return clockFont==0;case 5:return!muteUI;case 6:return nmMode==0;
    case 7:return dateFormat==0;case 8:return currentLang==0;case 9:return screenOffAnim==1;
    case 10:return autoClockReturnMin==5;case 11:return pixelBrightness==100;default:return false;
  }
}
static String getSettingValue(int i){
  String v;
  switch(i){
    case 0:return"";
    case 1:v=isScreenOn?"ON":"OFF";break;
    case 2:v=alarmEnabled?"ON":"OFF";break;
    case 3:if(isAPMode)v="AP";else if(WiFi.status()==WL_CONNECTED)v=WiFi.localIP().toString();else v="STA";break;
    case 4:v=clockFontNames[clockFont];break;
    case 5:v=muteUI?"ON":"OFF";break;
    case 6:{static const char*nm[]={"Off","Sched","Manual"};v=nm[nmMode];break;}
    case 7:v=dateFmtShort[dateFormat];break;
    case 8:v=String(langNames[currentLang]).substring(0,3);break;
    case 9:v=screenOffAnimShort[screenOffAnim];break;
    case 10:v=autoClockReturnMin>0?String(autoClockReturnMin)+"m":"OFF";break;
    case 11:v=String(pixelBrightness)+"%";break;
    default:v="?";
  }
  if(isDefault(i))v+='*';return v;
}
static void applySettingChange(int i){
  if(i==0){inSettingsMode=false;settingsIdx=0;refreshDisplay();return;}
  preferences.begin("clock_cfg",false);
  switch(i){
    case 1:
      if(isScreenOn){animateScreenOff();display.ssd1306_command(SSD1306_DISPLAYOFF);u8g2.setPowerSave(1);isScreenOn=false;manualScreenOff=true;applyCpuFreq(false);inSettingsMode=false;}
      else{manualScreenOff=false;applyCpuFreq(true);display.ssd1306_command(SSD1306_DISPLAYON);u8g2.setPowerSave(0);isScreenOn=true;}
      break;
    case 2:alarmEnabled=!alarmEnabled;preferences.putBool("ae",alarmEnabled);break;
    case 3:{preferences.end();u8g2.clearBuffer();u8g2.setFont(u8g2_font_6x10_tr);u8g2.drawStr(15,35,labels[currentLang][L_RESTART]);u8g2.sendBuffer();if(!isAPMode){preferences.begin("clock_cfg",false);preferences.putBool("force_ap",true);preferences.end();}delay(800);ESP.restart();return;}
    case 4:clockFont=(clockFont+1)%3;preferences.putInt("cfont",clockFont);break;
    case 5:muteUI=!muteUI;preferences.putBool("mute",muteUI);break;
    case 6:nmMode=(nmMode+1)%3;preferences.putInt("nm_mode",nmMode);break;
    case 7:dateFormat=(dateFormat+1)%5;preferences.putInt("df",dateFormat);break;
    case 8:currentLang=(currentLang+1)%5;preferences.putInt("lng",currentLang);break;
    case 9:screenOffAnim=(screenOffAnim+1)%SCREEN_OFF_ANIM_COUNT;preferences.putInt("so_anim",screenOffAnim);break;
    case 10:{static const int steps[]={0,1,2,5,10,30};const int n=6;int cur=0;for(int k=0;k<n;k++)if(steps[k]==autoClockReturnMin){cur=k;break;}autoClockReturnMin=steps[(cur+1)%n];modeChangedAt=millis();preferences.putInt("acr",autoClockReturnMin);break;}
    case 11:{static const int bsteps[]={1,25,50,75,100};const int bn=5;int cur=bn-1;for(int k=0;k<bn;k++)if(bsteps[k]==pixelBrightness){cur=k;break;}pixelBrightness=bsteps[(cur+1)%bn];applyContrast(pixelBrightness);preferences.putInt("pct_bright",pixelBrightness);refreshDisplay();break;}
  }
  preferences.end();
}
void drawSettingsMenu(){
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);u8g2.drawStr(18,11,"-- SETTINGS --");u8g2.drawHLine(0,13,128);
  const int vis=4;
  int start=settingsIdx-1;if(start<0)start=0;if(start>SETTINGS_COUNT-vis)start=SETTINGS_COUNT-vis;
  u8g2.setFont(u8g2_font_6x10_tr);
  for(int i=0;i<vis&&(start+i)<SETTINGS_COUNT;i++){
    int idx=start+i;int y=24+i*12;bool sel=(idx==settingsIdx);
    if(sel){u8g2.setDrawColor(1);u8g2.drawRBox(0,y-9,125,11,2);u8g2.setDrawColor(0);}
    u8g2.drawStr(4,y,settingNames[idx]);
    String val=getSettingValue(idx);
    if(val.length()>0)u8g2.drawStr(124-u8g2.getStrWidth(val.c_str()),y,val.c_str());
    if(sel)u8g2.setDrawColor(1);
  }
  u8g2.drawVLine(127,14,50);
  int bh=max(4,(vis*50)/SETTINGS_COUNT);
  int by=14+(settingsIdx*(50-bh))/(SETTINGS_COUNT-1);
  u8g2.drawBox(126,by,2,bh);u8g2.sendBuffer();
}

// ===================== SETUP =====================
void setup(){
  nvs_flash_init();Wire.begin(OLED_SDA,OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC,0x3C);
  display.clearDisplay();display.display();
  u8g2.begin();
  pinMode(BOOT_BUTTON_PIN,INPUT_PULLUP);
#if ENABLE_BUZZER
  buzzer.begin();
#endif
  initPowerManagement();

  preferences.begin("clock_cfg",true);
  nmStartH=preferences.getInt("nm_sh",23);nmStartM=preferences.getInt("nm_sm",0);
  nmEndH=preferences.getInt("nm_eh",7);nmEndM=preferences.getInt("nm_em",0);nmWakeTime=preferences.getInt("nm_wt",5);
  nmMode=preferences.isKey("nm_mode")?preferences.getInt("nm_mode",0):(preferences.getBool("nm_en",false)?1:0);
  String savedSSID=preferences.getString("ssid","");String savedPass=preferences.getString("pass","");
  weatherKey=preferences.getString("key","");weatherCity=preferences.getString("city","");googleScriptUrl=preferences.getString("g_url","");
  currentLang=preferences.getInt("lng",0);utcOffset=preferences.getInt("offset",3600);autoDST=preferences.getInt("dst",1);
  dateFormat=preferences.getInt("df",0);clockFont=preferences.getInt("cfont",0);webThemeLight=preferences.getBool("wtheme",false);
  otaPass=preferences.getString("ota_pass","");otaUser=preferences.getString("ota_user","");
  autoClockReturnMin=preferences.getInt("acr",5);
  screenOffAnim=preferences.getInt("so_anim",1);screenOffStepMs=preferences.getInt("so_ms",30);
  wakeAnim=preferences.getBool("wake_anim",false);pixelBrightness=preferences.getInt("pct_bright",100);
  autoBrightEnabled=preferences.getBool("ab_en",false);
  {String abs=preferences.getString("ab_slots","");if(abs.length()>0)abParse(abs);}
  bool forceAP=preferences.getBool("force_ap",false);
#if ENABLE_BUZZER
  alarmHour=preferences.getInt("ah",7);alarmMinute=preferences.getInt("am",0);
  alarmEnabled=preferences.getBool("ae",false);alarmPatternIdx=preferences.getInt("ap",0);
  alarmDays=preferences.getInt("adays",0b1111111);muteUI=preferences.getBool("mute",false);
#endif
  preferences.end();

  applyContrast(pixelBrightness);
  if(forceAP){preferences.begin("clock_cfg",false);preferences.putBool("force_ap",false);preferences.end();}
  if(clockFont<0||clockFont>2)clockFont=0;
  timeClient.setTimeOffset(utcOffset);timeClient.setUpdateInterval(3600000);modeChangedAt=millis();

  if(!forceAP&&savedSSID!=""&&savedSSID!="NULL"){
    preferences.begin("wifi_boot",false);int wifiAttempt=preferences.getInt("attempt",0);preferences.end();
    WiFi.persistent(false);WiFi.setAutoReconnect(false);WiFi.mode(WIFI_STA);enableWifi6();
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    WiFi.begin(savedSSID.c_str(),savedPass.c_str());
    const unsigned long WTO=5000UL;unsigned long ws=millis();
    while(WiFi.status()!=WL_CONNECTED&&millis()-ws<WTO){
      int bw=(int)(((millis()-ws)*108UL)/WTO);
      u8g2.clearBuffer();u8g2.setFont(u8g2_font_6x10_tr);
      u8g2.drawStr(10,22,labels[currentLang][L_SYNC]);u8g2.drawStr(10,36,savedSSID.c_str());
      u8g2.drawFrame(10,44,108,7);if(bw>0)u8g2.drawBox(10,44,bw,7);
      char tb[16];snprintf(tb,sizeof(tb),"Try %d/3",wifiAttempt+1);u8g2.drawStr(10,62,tb);u8g2.sendBuffer();delay(200);
    }
    if(WiFi.status()==WL_CONNECTED){
      preferences.begin("wifi_boot",false);preferences.putInt("attempt",0);preferences.end();
      if(MDNS.begin(HOSTNAME)){
        MDNS.addService("http","tcp",80);
        MDNS.addService("arduino","tcp",3232);
        MDNS.addServiceTxt("arduino","tcp","board","esp32");
        MDNS.addServiceTxt("arduino","tcp","auth_upload",otaPass.length()>0?"yes":"no");
      }
    } else {
      if(++wifiAttempt<3){
        preferences.begin("wifi_boot",false);preferences.putInt("attempt",wifiAttempt);preferences.end();
        u8g2.clearBuffer();u8g2.setFont(u8g2_font_6x10_tr);u8g2.drawStr(10,35,"Retrying...");
        char rb[16];snprintf(rb,sizeof(rb),"(%d/3 failed)",wifiAttempt);u8g2.drawStr(10,50,rb);u8g2.sendBuffer();delay(1000);ESP.restart();
      } else {
        preferences.begin("wifi_boot",false);preferences.putInt("attempt",0);preferences.end();
        WiFi.disconnect();WiFi.mode(WIFI_AP);WiFi.softAP("ESP32-Cuboid-Fallback");isAPMode=true;
      }
    }
  } else {
    WiFi.mode(WIFI_AP);WiFi.softAP(forceAP?"ESP32-Cuboid-AP":"ESP32-Cuboid");isAPMode=true;
  }

  ArduinoOTA.setHostname(HOSTNAME);
  if(otaPass.length()>0) ArduinoOTA.setPassword(otaPass.c_str());
  ArduinoOTA.onStart([](){
    if(WiFi.status()==WL_CONNECTED) esp_wifi_set_ps(WIFI_PS_NONE);
  });
  ArduinoOTA.onEnd([](){
    if(WiFi.status()==WL_CONNECTED) esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  });
  ArduinoOTA.onError([](ota_error_t){
    if(WiFi.status()==WL_CONNECTED) esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  });
  ArduinoOTA.begin();

  if(isAPMode){
    dnsServer.start(53,"*",WiFi.softAPIP());
    u8g2.clearBuffer();u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(5,12,labels[currentLang][L_SETUP]);u8g2.drawStr(5,30,labels[currentLang][L_WIFI]);
    u8g2.drawStr(35,30,"ESP32-Cuboid");u8g2.drawStr(5,45,"IP:");u8g2.drawStr(25,45,"192.168.4.1");
    u8g2.drawStr(5,60,"http://cuboid.local");u8g2.sendBuffer();
  } else {
    timeClient.begin();timeClient.update();if(weatherKey!="")fetchWeather();if(googleScriptUrl!="")fetchCalendar();
  }

  server.on("/",handleRoot);
  server.on("/advanced",handleAdvanced);
  server.on("/sysinfo",handleSysInfo);
  server.on("/scan",handleScan);
  server.on("/toggleMute",handleToggleMute);
  server.on("/toggleScreen",handleToggleScreen);
  server.on("/setBrightness",handleSetBrightness);
  server.on("/toggleTheme",handleToggleTheme);
  server.on("/saveTimeAlarm",handleSaveTimeAlarm);
  server.on("/savePersonalization",handleSavePersonalization);
  server.on("/saveAPI",handleSaveAPI);
  server.on("/saveSystem",handleSaveSystem);
  server.on("/saveNightMode",handleSaveNightMode);
  server.on("/saveAutoBright",handleSaveAutoBright);
  server.on("/saveOTAPass",handleSaveOTAPass);
  server.on("/exportSettings",HTTP_GET,handleExportSettings);
  server.on("/importSettings",HTTP_POST,handleImportSettings);
  server.on("/update",HTTP_POST,
    [](){ server.sendHeader("Connection","close"); server.send(200,"text/plain",Update.hasError()?"FAIL":"OK"); ESP.restart(); },
    [](){ HTTPUpload&u=server.upload(); if(u.status==UPLOAD_FILE_START)Update.begin(UPDATE_SIZE_UNKNOWN); else if(u.status==UPLOAD_FILE_WRITE)Update.write(u.buf,u.currentSize); else if(u.status==UPLOAD_FILE_END)Update.end(true); }
  );
  const char* hdrs[]={"Referer"};server.collectHeaders(hdrs,1);
  server.begin();
}

// ===================== LOOP =====================
void loop(){
  ArduinoOTA.handle();

  if(digitalRead(BOOT_BUTTON_PIN)==LOW){
    if(!isScreenOn){
      manualScreenOff=false;
      wakeScreenUntil=(nmWakeTime>0)?millis()+(nmWakeTime*1000UL):ULONG_MAX;
      while(digitalRead(BOOT_BUTTON_PIN)==LOW){buzzer.update();yield();}
      goto end_button;
    }
    {
      if(nmMode>0)wakeScreenUntil=(nmWakeTime>0)?millis()+(unsigned long)nmWakeTime*1000UL:ULONG_MAX;
      unsigned long pressStart=millis();
      bool holdDone=false,settingsOpened=false;
      int lastCd=-1;

      while(digitalRead(BOOT_BUTTON_PIN)==LOW){
#if ENABLE_BUZZER
        buzzer.update();
#endif
        unsigned long held=millis()-pressStart;

        if(held>=7000&&held<10000){
          int cd=3-(int)((held-7000)/1000);
          if(cd!=lastCd){
            lastCd=cd;
            u8g2.clearBuffer();u8g2.setFont(u8g2_font_6x10_tr);
            u8g2.drawStr(5,12,"Release to cancel!");u8g2.drawStr(20,28,"FACTORY RESET in");
            u8g2.setFont(u8g2_font_helvB14_tf);char cb[4];snprintf(cb,4,"%d",cd);
            u8g2.drawStr((128-u8g2.getStrWidth(cb))/2,56,cb);u8g2.sendBuffer();
#if ENABLE_BUZZER
            buzzer.playUISound(2500,120);
#endif
          }
        }

        if(held>=10000){
          u8g2.clearBuffer();u8g2.setFont(u8g2_font_6x10_tr);
          u8g2.drawStr(10,35,"FACTORY RESET...");u8g2.sendBuffer();
#if ENABLE_BUZZER
          if(!isSoundSuppressed()){
            static const BuzzerStep rst[]={{2000,80},{0,70},{2000,80},{0,70},{2000,80},{0,70},{2000,80},{0,150},{3200,350},{0,1}};
            buzzer.playPattern(rst,10,false);
            unsigned long t=millis();while(millis()-t<2000){buzzer.update();delay(1);}
          } else {
            delay(400);
          }
#endif
          preferences.begin("clock_cfg",false);preferences.clear();preferences.end();ESP.restart();
        }

        if(!settingsOpened&&!inSettingsMode&&held>=SETTINGS_HOLD_MS&&held<SETTINGS_HOLD_MS+SETTINGS_HOLD_WIN){
          settingsOpened=true;holdDone=true;inSettingsMode=true;settingsIdx=0;
#if ENABLE_BUZZER
          buzzer.playUISound(1200,200);
#endif
          drawSettingsMenu();
        }

        if(!holdDone&&inSettingsMode&&held>=500&&held<700){
          holdDone=true;applySettingChange(settingsIdx);
#if ENABLE_BUZZER
          if(!isAlarmRinging)buzzer.playUISound(1400,80);
#endif
          if(inSettingsMode)drawSettingsMenu();
        }

        if(!holdDone&&!inSettingsMode&&held>=300&&held<400){
          if(currentMode==MODE_WEATHER){
            currentMode=MODE_FORECAST;holdDone=true;modeChangedAt=millis();
#if ENABLE_BUZZER
            buzzer.playUISound(1500,100);
#endif
            updateDisplayWeather();
          } else if(currentMode==MODE_CALENDAR&&hasCalendarEvent&&cal_eventCount>1){
            cal_currentEventIdx=(cal_currentEventIdx+1)%cal_eventCount;
            scrollX=0;textWidth=0;holdDone=true;modeChangedAt=millis();
#if ENABLE_BUZZER
            buzzer.playUISound(1500,100);
#endif
            updateDisplayCalendar();
          }
        }
        yield();
      }

      unsigned long held=millis()-pressStart;
      if(!holdDone&&held<300){
        if(inSettingsMode){
          settingsIdx=(settingsIdx+1)%SETTINGS_COUNT;
#if ENABLE_BUZZER
          buzzer.playUISound(1800,50);
#endif
          drawSettingsMenu();
        } else {
          if(isAlarmRinging){
#if ENABLE_BUZZER
            buzzer.stop();isAlarmRinging=false;
#endif
            if(currentMode==MODE_CLOCK)updateDisplayClock();
          } else {
            bool hw=(weatherKey!=""),hc=(googleScriptUrl!="");
            Mode nm=currentMode;
            if(currentMode==MODE_FORECAST)nm=MODE_WEATHER;
            else if(currentMode==MODE_CLOCK)nm=hc?MODE_CALENDAR:(hw?MODE_WEATHER:MODE_CLOCK);
            else if(currentMode==MODE_CALENDAR)nm=hw?MODE_WEATHER:MODE_CLOCK;
            else nm=MODE_CLOCK;
            bool ch=(nm!=currentMode);currentMode=nm;
            if(currentMode==MODE_CALENDAR){scrollX=0;textWidth=0;}
            if(ch){modeChangedAt=millis();
#if ENABLE_BUZZER
              buzzer.playUISound(2000,100);
#endif
            }
            refreshDisplay();
          }
        }
      }
    }
    end_button:;
  }

  server.handleClient();
#if ENABLE_BUZZER
  buzzer.update(); // must run even in AP mode — otherwise tone() plays forever
#endif
  if(isAPMode){dnsServer.processNextRequest();delay(10);yield();return;}

  // Declare 'now' after the AP-mode early return so it's available throughout the rest of loop()
  unsigned long now=millis();

  // FIX: acquire time values BEFORE the potentially-blocking timeClient.update()
  // so alarm firing is never delayed by an NTP sync (which can take up to ~1.5 s).
  hours=timeClient.getHours();minutes=timeClient.getMinutes();seconds=timeClient.getSeconds();
  {time_t et=timeClient.getEpochTime();struct tm*p=gmtime(&et);day=p->tm_mday;month=p->tm_mon+1;year=p->tm_year+1900;wdayBit=(p->tm_wday==0)?6:p->tm_wday-1;}

  if(isScreenOn&&!isAlarmRinging) applyAutoBrightness();

#if ENABLE_BUZZER
  if(alarmEnabled&&!isAlarmRinging&&hours==alarmHour&&minutes==alarmMinute&&seconds==0&&isAlarmActiveToday()){
    isAlarmRinging=true;manualScreenOff=false;currentMode=MODE_CLOCK;
    startAlarmSound();
    {unsigned long _at=millis();while(millis()-_at<220){buzzer.update();delay(1);}}
  }
#endif

  if(WiFi.status()==WL_CONNECTED){
    static unsigned long lastNtpCheck=0;
    if(now-lastNtpCheck>=60000){timeClient.update();lastNtpCheck=now;}
    if(weatherKey!=""&&now-lastWeatherUpdate>DATA_REFRESH_INTERVAL){fetchWeather();lastWeatherUpdate=now;}
    if(googleScriptUrl!=""&&now-lastCalendarUpdate>DATA_REFRESH_INTERVAL){fetchCalendar();lastCalendarUpdate=now;}
  }

  bool nmWantsOff=checkNightMode()&&!isAlarmRinging&&now>=wakeScreenUntil;
  bool sbo=!(nmWantsOff||(manualScreenOff&&!isAlarmRinging));

  if(sbo&&!isScreenOn){
    applyCpuFreq(true);display.ssd1306_command(SSD1306_DISPLAYON);u8g2.setPowerSave(0);
    isScreenOn=true;lastAutoBrightMinute=-1;
    if(wakeAnim)animateScreenOn();else refreshDisplay();
  } else if(!sbo&&isScreenOn&&!inSettingsMode){
    currentMode=MODE_CLOCK;modeChangedAt=millis();
    animateScreenOff();display.ssd1306_command(SSD1306_DISPLAYOFF);u8g2.setPowerSave(1);
    isScreenOn=false;applyCpuFreq(false);
  }

  if(isScreenOn&&!inSettingsMode&&autoClockReturnMin>0
     &&currentMode!=MODE_CLOCK
     &&(now-modeChangedAt)>=(unsigned long)autoClockReturnMin*60000UL){
    currentMode=MODE_CLOCK;modeChangedAt=now;updateDisplayClock();
  }

  static unsigned long lcu=0;
  if(isScreenOn&&!inSettingsMode&&currentMode==MODE_CLOCK&&now-lcu>=1000){
    updateDisplayClock();lcu=now;
  }

  if(isScreenOn&&!inSettingsMode&&currentMode==MODE_CALENDAR
     &&hasCalendarEvent&&textWidth>128&&now-lastScrollUpdate>=50){
    if(++scrollX>textWidth+20)scrollX=0;lastScrollUpdate=now;updateDisplayCalendar();
  }

  yield();
}
