#include <WiFi.h>
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
#include <esp_sntp.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

// ===================== RTOS =====================
QueueHandle_t eventQueue;
SemaphoreHandle_t displayMutex;
SemaphoreHandle_t wifiMutex;

typedef enum {
    EVENT_BUTTON_TAP,
    EVENT_BUTTON_HOLD,
    EVENT_BUTTON_MED,
    EVENT_WEB_ACTIVITY,
    EVENT_ALARM_TRIGGER,
    EVENT_DATA_REFRESH,
    EVENT_WAKE_UP
} SystemEvent;

// ===================== DEEP SLEEP RTC =====================
RTC_DATA_ATTR time_t rtc_last_sync = 0;
RTC_DATA_ATTR uint64_t rtc_sleep_us = 0;
RTC_DATA_ATTR bool rtc_initialized = false;
RTC_DATA_ATTR bool rtc_alarm_pending = false;
RTC_DATA_ATTR int  rtc_lastAlarmFiredDay = -1;

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
#define WEB_ACTIVE_TIMEOUT  30000UL
enum PowerMode : uint8_t {
  PWR_DEEP,
  PWR_DISPLAY,
  PWR_WIFI_IDLE,
  PWR_WIFI_ACTIVE,
  PWR_FETCH,
  PWR_OTA
};
static PowerMode _pwrCur = (PowerMode)255;

enum PowerState {
    STATE_ACTIVE,
    STATE_DEEP_SLEEP
};
PowerState currentPowerState = STATE_ACTIVE;

// ===================== SETTINGS TIMING =====================
#define SETTINGS_HOLD_MS  2000
#define SETTINGS_HOLD_WIN  200

// ===================== OBJECTS =====================
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
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
  L_WIFI_SYS, L_ALARM_TIME, L_AUTO_CLOCK, L_TODAY, L_ROTATION, L_COUNT
};
const char* langNames[] = { "English","Italiano","Francais","Espanol","Deutsch" };
const char* labels[5][L_COUNT] = {
  {"Syncing...","SETUP MODE","Connect to:","SAVE","WiFi","Password","City","Language","RH: ","Restarting...","Time","UTC Offset","Restart required. Proceed?","Alarm","Enabled","Ringtone","Mute UI","Calendar","No Events","TOMORROW","Active days","Personalization","Clock Font","Date Style","Theme","Dark","Light","Night Mode","Weather & Calendar","WiFi & System","Alarm & Time","Auto clock return (min, 0=off)","TODAY","Rotation"},
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

// ===================== CSS — PROGMEM (identico all'originale) =====================
const char CSS_VARS[] PROGMEM = "<style>:root{--bg:#111318;--cbg:#1c1e26;--tbg:#141620;--txt:#dde1f0;--muted:#7b82a0;--acc:#a78bfa;--teal:#2dd4bf;--brd:#2e3148;--inp:#22253a;--ibox:#191c2e;--sep:#22253a;--hvr:#000;--sbtn:#4c1d95;--sbtn-h:#6d28d9;--ok:#065f46;--pill-on-bg:#2e1065;--pill-on-brd:#a78bfa;--pill-on-txt:#c4b5fd;--mute-brd:#f87171;--mute-txt:#f87171;--rssi-ok:#2dd4bf;--rssi-warn:#fbbf24;--rssi-bad:#f87171;}body.lm{--bg:#f0f2f9;--cbg:#fff;--tbg:#ede9fe;--txt:#1e1b2e;--muted:#6b7280;--acc:#7c3aed;--teal:#0d9488;--brd:#d1d5db;--inp:#f9fafb;--ibox:#f3f4f6;--sep:#e5e7eb;--hvr:#fff;--sbtn:#7c3aed;--sbtn-h:#6d28d9;--ok:#047857;--pill-on-bg:#ede9fe;--pill-on-brd:#7c3aed;--pill-on-txt:#6d28d9;--mute-brd:#ef4444;--mute-txt:#ef4444;--rssi-ok:#0d9488;--rssi-warn:#d97706;--rssi-bad:#dc2626;}*{box-sizing:border-box;transition:background-color .25s,color .2s,border-color .2s;}body{background:var(--bg);color:var(--txt);font-family:'Segoe UI',system-ui,sans-serif;margin:0;padding:0;}.page{padding:12px;max-width:600px;margin:0 auto;}.card{background:var(--cbg);padding:16px;border-radius:14px;margin-bottom:14px;box-shadow:0 2px 12px rgba(0,0,0,.18);border:1px solid var(--brd);}.topbar{background:var(--tbg);padding:9px 13px;border-radius:14px;margin-bottom:14px;box-shadow:0 4px 16px rgba(0,0,0,.25);display:flex;align-items:center;gap:7px;border:1px solid var(--brd);position:sticky;top:8px;z-index:99;flex-wrap:wrap;}.topbar .brand{flex:1;font-size:15px;font-weight:900;color:var(--acc);letter-spacing:2px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;font-variant:small-caps;}.tbtn{color:var(--teal);text-decoration:none;font-size:12px;font-weight:700;padding:5px 10px;border-radius:12px;border:2px solid var(--teal);transition:background .2s,color .2s;white-space:nowrap;flex-shrink:0;background:transparent;cursor:pointer;}.tbtn:hover,.tbtn.active{background:var(--teal);color:var(--hvr);}.tbtn.mute-on{border-color:var(--mute-brd);color:var(--mute-txt);}.tbtn.mute-on:hover{background:var(--mute-brd);color:#fff;}.tbtn.theme-btn{border-color:var(--acc);color:var(--acc);font-size:15px;padding:4px 9px;}.tbtn.theme-btn:hover{background:var(--acc);color:var(--hvr);}.tbtn.exp-btn{border-color:var(--acc);color:var(--acc);padding:5px 11px;font-size:13px;font-weight:900;letter-spacing:1px;}.tbtn.exp-btn:hover,.tbtn.exp-btn.open{background:var(--acc);color:var(--hvr);}.tb-panel{flex-basis:100%;display:flex;align-items:center;gap:8px;padding-top:8px;margin-top:2px;border-top:1px solid var(--sep);}.tb-panel input[type=range]{flex:1;height:28px;padding:0;accent-color:var(--acc);}.tb-bv{font-size:12px;color:var(--txt);font-weight:700;min-width:32px;text-align:right;flex-shrink:0;}h2{color:var(--acc);font-size:14px;font-weight:800;margin:0 0 13px;border-bottom:1px solid var(--sep);padding-bottom:8px;display:flex;align-items:center;gap:7px;letter-spacing:.5px;text-transform:uppercase;}label{display:block;margin-top:10px;font-size:12px;color:var(--muted);font-weight:600;letter-spacing:.4px;text-transform:uppercase;}.lrow{display:flex;align-items:baseline;gap:8px;margin-top:10px;}.lrow span{font-size:12px;color:var(--muted);font-weight:600;letter-spacing:.4px;text-transform:uppercase;}.lrow a{font-size:11px;color:var(--teal);text-decoration:none;flex-shrink:0;}.lrow a:hover{text-decoration:underline;}select,input[type=text],input[type=number],input[type=password]{width:100%;padding:10px 12px;margin:4px 0;border-radius:9px;border:1.5px solid var(--brd);background:var(--inp);color:var(--txt);font-size:14px;outline:none;}select:focus,input:focus{border-color:var(--acc);box-shadow:0 0 0 3px rgba(167,139,250,.15);}.save-btn{width:100%;padding:13px;background:var(--sbtn);color:#fff;border:none;border-radius:9px;font-weight:800;cursor:pointer;margin-top:14px;font-size:13px;letter-spacing:.5px;text-transform:uppercase;transition:background .2s,transform .1s;display:block;text-align:center;}.save-btn:hover{background:var(--sbtn-h);}.save-btn:active{transform:scale(.98);}.save-btn.ok{background:var(--ok);}.save-btn.wait{background:#555;cursor:wait;}.row{display:flex;gap:10px;}.col{flex:1;}.wrow{display:flex;gap:8px;align-items:flex-end;}.wsel{flex:1;}.icon-btn{width:44px;height:44px;background:var(--inp);border:1.5px solid var(--brd);border-radius:9px;cursor:pointer;display:flex;justify-content:center;align-items:center;padding:0;flex-shrink:0;}.icon-btn:hover{background:var(--cbg);border-color:var(--acc);}.icon-btn svg{fill:var(--teal);width:22px;height:22px;}.spin{animation:spin 1s linear infinite;}@keyframes spin{100%{transform:rotate(360deg);}}hr.sep{border:0;border-top:1px solid var(--sep);margin:13px 0;}.ig{display:grid;grid-template-columns:auto 1fr;gap:5px 14px;font-size:12px;}.ig .k{color:var(--muted);white-space:nowrap;font-weight:600;}.ig .v{color:var(--txt);font-family:monospace;word-break:break-all;}.bar-wrap{background:var(--ibox);border-radius:4px;height:5px;margin:4px 0 10px;overflow:hidden;}.bar{height:100%;border-radius:4px;transition:width .4s;}.ibox{background:var(--ibox);border-radius:9px;padding:10px 13px;font-size:12px;color:var(--muted);margin:10px 0 4px;line-height:1.6;border:1px solid var(--sep);}.days-row{display:flex;gap:5px;flex-wrap:wrap;margin-top:8px;}.day-pill{display:inline-flex;align-items:center;justify-content:center;width:40px;height:34px;border-radius:8px;border:2px solid var(--brd);background:var(--inp);color:var(--muted);font-size:11px;font-weight:800;cursor:pointer;user-select:none;transition:all .15s;}.day-pill.on{border-color:var(--pill-on-brd);background:var(--pill-on-bg);color:var(--pill-on-txt);}.day-pill:hover{border-color:var(--acc);}.font-preview{display:flex;gap:8px;flex-wrap:wrap;margin-top:8px;}.fp{padding:6px 12px;border-radius:8px;border:2px solid var(--brd);background:var(--inp);color:var(--muted);font-size:13px;cursor:pointer;user-select:none;transition:all .15s;}.fp.on{border-color:var(--pill-on-brd);background:var(--pill-on-bg);color:var(--pill-on-txt);font-weight:700;}.fp:hover{border-color:var(--acc);}input[type=range]{padding:0;height:36px;accent-color:var(--acc);}</style>";

// ===================== AJAX JS — PROGMEM =====================
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

const char PREVIEW_JS[] PROGMEM = R"prevjs(
<script>
(function(){
  var SC=3, CW=384, CH=192;
  var _open=false, _tick=null, _lang=0;

  var _L=[
    {off:'Screen off',    cal:'Calendar',   noEvt:'No Events',        tmr:'TOMORROW', alm:'\u26a0 ALARM \u26a0',
     modes:['Weather','Clock','Calendar','Forecast'],   set:'Settings L'},
    {off:'Schermo spento',cal:'Calendario', noEvt:'Nessun Evento',    tmr:'DOMANI',   alm:'\u26a0 SVEGLIA \u26a0',
     modes:['Meteo','Orologio','Calendario','Previsioni'], set:'Impost. L'},
    {off:'\u00c9cran \u00e9teint',cal:'Calendrier',noEvt:'Aucun \u00e9v\u00e9nement',tmr:'DEMAIN',alm:'\u26a0 R\u00c9VEIL \u26a0',
     modes:['M\u00e9t\u00e9o','Horloge','Calendrier','Pr\u00e9visions'],set:'R\u00e9glages L'},
    {off:'Pantalla apagada',cal:'Calendario',noEvt:'Sin Eventos',     tmr:'MA\u00d1ANA',alm:'\u26a0 ALARMA \u26a0',
     modes:['Clima','Reloj','Calendario','Pron\u00f3stico'],set:'Config. L'},
    {off:'Bildschirm aus', cal:'Kalender',  noEvt:'Keine Termine',    tmr:'MORGEN',   alm:'\u26a0 WECKER \u26a0',
     modes:['Wetter','Uhr','Kalender','Vorhersage'],     set:'Einst. L'}
  ];
  function t(key){ return (_L[_lang]||_L[0])[key]; }

  window.togglePreview=function(){
    _open=!_open;
    var p=document.getElementById('_pp');
    if(!p)return;
    p.style.display=_open?'block':'none';
    if(_open&&!_tick){_tick=setInterval(_poll,1000);_poll();}
    else if(!_open){clearInterval(_tick);_tick=null;}
  };

  async function _poll(){
    try{
      var s=await fetch('/displayState').then(r=>r.json());
      _lang=s.lang||0;
      _render(s);
      var lbl=t('modes')[s.mode]||'';
      var rotLbl=['','  [90\u00b0R]','  [180\u00b0]','  [90\u00b0L]'][s.rotation||0];
      var extra=s.inSettings?(t('set')+s.settingsLevel):lbl;
      var screenLbl=s.screenOn?'':' ['+t('off')+']';
      document.getElementById('_pst').textContent=extra+screenLbl+rotLbl;
    }catch(e){}
  }

  window.simBtn=async function(type){
    try{await fetch('/btn'+type,{method:'POST'});}catch(e){}
    setTimeout(_poll,350);
  };

  function p(v){return v*SC;}
  function textCentered(ctx,txt,y){var w=ctx.measureText(txt).width;ctx.fillText(txt,Math.round((CW-w)/2),y);}

  function _resizeCanvas(cv,portrait){
    var nW=portrait?p(64):p(128);
    var nH=portrait?p(128):p(64);
    if(cv.width!==nW||cv.height!==nH){cv.width=nW;cv.height=nH;}
    CW=nW;CH=nH;
    if(portrait){cv.style.width='auto';cv.style.height='280px';cv.style.margin='0 auto';}
    else{cv.style.width='100%';cv.style.height='auto';cv.style.margin='';}
  }

  function _render(s){
    var cv=document.getElementById('_oled');
    if(!cv)return;
    var rot=s.rotation||0;
    var portrait=(rot===1||rot===3);
    _resizeCanvas(cv,portrait);
    var ctx=cv.getContext('2d');
    ctx.imageSmoothingEnabled=false;
    ctx.fillStyle='#000';ctx.fillRect(0,0,CW,CH);
    if(!s.screenOn){
      ctx.fillStyle='#333';ctx.font=p(6)+'px "Courier New",monospace';
      var ot='[ '+t('off')+' ]';var tw=ctx.measureText(ot).width;
      ctx.fillText(ot,(CW-tw)/2,CH/2+p(3));return;
    }
    if(s.inSettings){portrait?_drawSettingsP(ctx,s):_drawSettings(ctx,s);return;}
    switch(s.mode){
      case 0:portrait?_drawWeatherP(ctx,s,false):_drawWeather(ctx,s,false);break;
      case 2:portrait?_drawCalendarP(ctx,s):_drawCalendar(ctx,s);break;
      case 3:portrait?_drawWeatherP(ctx,s,true):_drawWeather(ctx,s,true);break;
      default:portrait?_drawClockP(ctx,s):_drawClock(ctx,s);break;
    }
  }

  var _DN=[['Mon','Tue','Wed','Thu','Fri','Sat','Sun'],['Lun','Mar','Mer','Gio','Ven','Sab','Dom'],
           ['Lun','Mar','Mer','Jeu','Ven','Sam','Dim'],['Lun','Mar','Mie','Jue','Vie','Sab','Dom'],
           ['Mo','Di','Mi','Do','Fr','Sa','So']];
  var _MN=[['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'],
           ['Gen','Feb','Mar','Apr','Mag','Giu','Lug','Ago','Set','Ott','Nov','Dic'],
           ['Jan','F\u00e9v','Mar','Avr','Mai','Jun','Jul','Ao\u00fb','Sep','Oct','Nov','D\u00e9c'],
           ['Ene','Feb','Mar','Abr','May','Jun','Jul','Ago','Sep','Oct','Nov','Dic'],
           ['Jan','Feb','Mar','Apr','Mai','Jun','Jul','Aug','Sep','Okt','Nov','Dez']];
  function _days(l){return _DN[l]||_DN[0];}
  function _mons(l){return _MN[l]||_MN[0];}
  function _dateStr(s){
    var days=_days(s.lang),mons=_mons(s.lang);
    var dd=String(s.day).padStart(2,'0'),mm=String(s.month).padStart(2,'0'),yy=s.year;
    switch(s.dateFormat){
      case 1:return mm+'/'+dd+'/'+yy;
      case 2:return yy+'/'+mm+'/'+dd;
      case 3:return days[s.wday]+' '+dd+' '+mons[s.month-1]+' '+yy;
      case 4:return days[s.wday]+' '+dd+' '+mons[s.month-1];
      default:return dd+'/'+mm+'/'+yy;
    }
  }

  function _drawClock(ctx,s){
    var tm=(s.h<10?'0':'')+s.h+':'+(s.m<10?'0':'')+s.m;
    ctx.fillStyle='#fff';
    ctx.font='bold '+p(26)+'px "Arial Narrow",Arial,sans-serif';
    var tw=ctx.measureText(tm).width,off=s.m%2;
    ctx.fillText(tm,Math.round((CW-tw)/2)+off*SC,p(40)+off*SC);
    ctx.fillStyle='#fff';ctx.fillRect(0,p(48),CW,SC);
    var ds=_dateStr(s);
    ctx.font=p(6)+'px "Courier New",monospace';
    tw=ctx.measureText(ds).width;ctx.fillText(ds,(CW-tw)/2,p(55));
    if(s.alarmEnabled){
      var tb=(s.wday+1)%7;var tmrA=(s.alarmDays===undefined)?1:((s.alarmDays>>tb)&1);
      ctx.beginPath();ctx.arc(p(120),p(56),p(3),0,2*Math.PI);
      if(tmrA){ctx.fillStyle='#fff';ctx.fill();}else{ctx.strokeStyle='#fff';ctx.lineWidth=SC;ctx.stroke();}
    }
    if(s.hasCal){ctx.strokeStyle='#fff';ctx.lineWidth=SC;ctx.strokeRect(p(5)+.5,p(52)+.5,p(8),p(8));ctx.fillStyle='#fff';ctx.fillRect(p(5),p(54),p(8),SC);ctx.fillRect(p(9),p(57),SC,SC);}
    if(s.alarmRinging){ctx.fillStyle='#ff3030';ctx.font='bold '+p(7)+'px monospace';textCentered(ctx,t('alm'),p(10));}
    if(s.ecoSyncing){ctx.fillStyle='#fff';ctx.font=p(5)+'px monospace';ctx.fillText((Date.now()/400)%2<1?'~*':'*~',p(114),p(7));}
  }

  function _drawClockP(ctx,s){
    var W=CW,H=CH;
    ctx.fillStyle='#fff';
    ctx.font='bold '+p(26)+'px "Arial Narrow",Arial,sans-serif';
    var hh=(s.h<10?'0':'')+s.h, mm=(s.m<10?'0':'')+s.m;
    var tw=ctx.measureText(hh).width;
    ctx.fillText(hh,(W-tw)/2,p(44));
    ctx.fillRect(p(8),p(50),W-p(16),SC);
    tw=ctx.measureText(mm).width;
    ctx.fillText(mm,(W-tw)/2,p(86));
    ctx.font=p(6)+'px "Courier New",monospace';
    var ds=_dateStr(s); tw=ctx.measureText(ds).width;
    ctx.fillText(ds,(W-tw)/2,p(100));
    if(s.dateFormat<3){
      ctx.font=p(5)+'px "Courier New",monospace';
      var dn=_days(s.lang)[s.wday]; tw=ctx.measureText(dn).width;
      ctx.fillText(dn,(W-tw)/2,p(112));
    }
    if(s.alarmEnabled){
      var tb=(s.wday+1)%7;var tmrA=(s.alarmDays===undefined)?1:((s.alarmDays>>tb)&1);
      ctx.beginPath();ctx.arc(p(6),p(122),p(3),0,2*Math.PI);
      if(tmrA){ctx.fillStyle='#fff';ctx.fill();}else{ctx.strokeStyle='#fff';ctx.lineWidth=SC;ctx.stroke();}
    }
    if(s.hasCal){ctx.strokeStyle='#fff';ctx.lineWidth=SC;ctx.strokeRect(W-p(14)+.5,p(116)+.5,p(8),p(8));ctx.fillStyle='#fff';ctx.fillRect(W-p(14),p(118),p(8),SC);ctx.fillRect(W-p(10),p(121),SC,SC);}
    if(s.ecoSyncing){ctx.font=p(5)+'px monospace';ctx.fillText((Date.now()/400)%2<1?'~*':'*~',p(2),p(8));}
  }

  function _drawWeather(ctx,s,fc){
    var tmp=fc?s.w_fc_temp:s.w_temp,hum=fc?s.w_fc_hum:s.w_hum,desc=fc?s.w_fc_desc:s.w_desc;
    var icon=fc?s.w_fc_icon:s.w_icon,bot=fc?('v '+s.w_fc_min+' ^ '+s.w_fc_max):s.w_city;
    ctx.fillStyle='#fff';ctx.font='bold '+p(14)+'px Arial,sans-serif';
    ctx.fillText(tmp||'--',0,p(22));
    ctx.fillStyle='#aaa';ctx.font=p(6)+'px "Courier New",monospace';
    ctx.fillText('RH: '+(hum||'--')+'%',0,p(35));
    ctx.fillStyle='#fff';ctx.font=p(6)+'px "Courier New",monospace';
    ctx.fillText(bot||'',0,p(48));
    ctx.fillStyle='#999';ctx.font=p(5)+'px "Courier New",monospace';
    ctx.fillText(desc||'',0,p(62));
    if(fc){ctx.fillStyle='#fff';ctx.font='bold '+p(5)+'px monospace';var lbl=t('tmr');var lw=ctx.measureText(lbl).width;ctx.fillText(lbl,CW-lw,p(10));}
    _drawIcon(ctx,icon,p(90),p(8),p(36));
  }

  function _drawWeatherP(ctx,s,fc){
    var tmp=fc?s.w_fc_temp:s.w_temp,hum=fc?s.w_fc_hum:s.w_hum,desc=fc?s.w_fc_desc:s.w_desc;
    var icon=fc?s.w_fc_icon:s.w_icon,bot=fc?('v '+s.w_fc_min+' ^'+s.w_fc_max):s.w_city;
    ctx.fillStyle='#fff';
    if(fc){ctx.font='bold '+p(5)+'px monospace';var lbl=t('tmr');var lw=ctx.measureText(lbl).width;ctx.fillText(lbl,(CW-lw)/2,p(8));}
    ctx.font='bold '+p(14)+'px Arial,sans-serif';
    var tw=ctx.measureText(tmp||'--').width;ctx.fillText(tmp||'--',(CW-tw)/2,p(22));
    _drawIcon(ctx,icon,(CW-p(32))/2,p(26),p(32));
    ctx.fillStyle='#aaa';ctx.font=p(6)+'px "Courier New",monospace';
    var hs='RH: '+(hum||'--')+'%';tw=ctx.measureText(hs).width;ctx.fillText(hs,(CW-tw)/2,p(73));
    ctx.fillStyle='#fff';ctx.font=p(6)+'px "Courier New",monospace';
    tw=ctx.measureText(bot||'').width;ctx.fillText(bot||'',(CW-tw)/2,p(85));
    ctx.fillStyle='#999';ctx.font=p(5)+'px "Courier New",monospace';
    tw=ctx.measureText(desc||'').width;ctx.fillText(desc||'',(CW-tw)/2,p(98));
  }

  function _drawCalendar(ctx,s){
    ctx.fillStyle='#fff';ctx.font='bold '+p(8)+'px sans-serif';
    ctx.fillText(t('cal'),p(16),p(14));ctx.fillStyle='#fff';ctx.fillRect(0,p(20),CW,SC);
    if(s.cal_count>0){
      var cnt=(s.cal_idx+1)+'/'+s.cal_count;
      ctx.fillStyle='#aaa';ctx.font=p(5)+'px monospace';var cw=ctx.measureText(cnt).width;ctx.fillText(cnt,CW-cw,p(10));
      ctx.fillStyle='#fff';ctx.font='bold '+p(8)+'px sans-serif';ctx.fillText(s.cal_time||'',0,p(40));
      ctx.fillStyle='#eee';ctx.font=p(6)+'px "Courier New",monospace';ctx.fillText(s.cal_event||'',0,p(56));
    }else{ctx.fillStyle='#888';ctx.font=p(7)+'px monospace';ctx.fillText(t('noEvt'),p(10),p(45));}
  }

  function _drawCalendarP(ctx,s){
    ctx.fillStyle='#fff';ctx.font='bold '+p(8)+'px sans-serif';
    var lc=t('cal');var tw=ctx.measureText(lc).width;ctx.fillText(lc,(CW-tw)/2,p(14));
    ctx.fillRect(0,p(18),CW,SC);
    if(s.cal_count>0){
      var cnt=(s.cal_idx+1)+'/'+s.cal_count;
      ctx.fillStyle='#aaa';ctx.font=p(5)+'px monospace';var cw=ctx.measureText(cnt).width;ctx.fillText(cnt,CW-cw,p(10));
      ctx.fillStyle='#fff';ctx.font='bold '+p(8)+'px sans-serif';
      tw=ctx.measureText(s.cal_time||'').width;ctx.fillText(s.cal_time||'',(CW-tw)/2,p(40));
      ctx.fillStyle='#eee';ctx.font=p(6)+'px "Courier New",monospace';
      tw=ctx.measureText(s.cal_event||'').width;ctx.fillText(s.cal_event||'',(CW-tw)/2,p(60));
    }else{
      ctx.fillStyle='#888';ctx.font=p(7)+'px monospace';
      var nv=ctx.measureText(t('noEvt')).width;ctx.fillText(t('noEvt'),(CW-nv)/2,p(65));
    }
  }

  function _drawSettings(ctx,s){
    var titles=['-- SETTINGS --','- Controls -','-- WiFi --','-- Display --','-- System --','--- Info ---'];
    ctx.fillStyle='#fff';ctx.font='bold '+p(7)+'px "Courier New",monospace';
    textCentered(ctx,titles[s.settingsLevel]||titles[0],p(11));
    ctx.fillStyle='#fff';ctx.fillRect(0,p(13),CW,SC);
    var items=s.menuItems||[],vals=s.menuVals||[],start=s.menuStart||0;
    for(var i=0;i<items.length;i++){
      var y=p(24+i*12),sel=(s.settingsIdx===start+i);
      if(sel){ctx.fillStyle='#fff';ctx.fillRect(0,y-p(9),p(125),p(11));ctx.fillStyle='#000';}
      else ctx.fillStyle='#ccc';
      ctx.font=p(6)+'px "Courier New",monospace';
      ctx.fillText(items[i],p(4),y);
      if(vals[i]){var vw=ctx.measureText(vals[i]).width;ctx.fillText(vals[i],p(124)-vw,y);}
      if(sel)ctx.fillStyle='#fff';
    }
    ctx.fillStyle='#fff';ctx.fillRect(p(127),p(14),p(1),p(50));
    var total=s.menuTotal||items.length||1;
    var bh=Math.max(p(4),(4*p(50))/total);
    var by=p(14)+(s.settingsIdx*(p(50)-bh))/Math.max(total-1,1);
    ctx.fillRect(p(126),by,p(2),bh);
  }

  function _drawSettingsP(ctx,s){
    var titles=['-- SETTINGS --','- Controls -','-- WiFi --','-- Display --','-- System --','--- Info ---'];
    ctx.fillStyle='#fff';ctx.font='bold '+p(6)+'px "Courier New",monospace';
    var tt=titles[s.settingsLevel]||titles[0];var tw=ctx.measureText(tt).width;
    ctx.fillText(tt,(CW-tw)/2,p(9));
    ctx.fillRect(0,p(11),CW,SC);
    var items=s.menuItems||[],vals=s.menuVals||[],start=s.menuStart||0;
    for(var i=0;i<items.length;i++){
      var y=p(21+i*14),sel=(s.settingsIdx===start+i);
      if(sel){ctx.fillStyle='#fff';ctx.fillRect(0,y-p(9),p(60),p(11));ctx.fillStyle='#000';}
      else ctx.fillStyle='#ccc';
      ctx.font=p(6)+'px "Courier New",monospace';
      ctx.fillText(items[i],p(3),y);
      if(vals[i]){var vw=ctx.measureText(vals[i]).width;ctx.fillText(vals[i],CW-p(3)-vw,y);}
      if(sel)ctx.fillStyle='#fff';
    }
    ctx.fillStyle='#fff';ctx.fillRect(CW-p(2),p(12),SC,CH-p(14));
    var total=s.menuTotal||items.length||1;
    var bh=Math.max(p(4),(items.length*(CH-p(14)))/total);
    var by=p(12)+(s.settingsIdx*(CH-p(14)-bh))/Math.max(total-1,1);
    ctx.fillRect(CW-p(2),by,p(2),bh);
  }

  function _fillCloud(ctx,px,py,cr,col){
    ctx.fillStyle=col;
    ctx.beginPath();ctx.arc(px-cr*0.32,py-cr*0.06,cr*0.42,0,Math.PI*2);ctx.fill();
    ctx.beginPath();ctx.arc(px,         py-cr*0.22,cr*0.50,0,Math.PI*2);ctx.fill();
    ctx.beginPath();ctx.arc(px+cr*0.36, py-cr*0.02,cr*0.34,0,Math.PI*2);ctx.fill();
    ctx.fillRect(px-cr*0.70,py-cr*0.06,cr*1.40,cr*0.54);
  }
  function _sun(ctx,sx,sy,sr,col){
    ctx.strokeStyle=col;ctx.fillStyle=col;ctx.lineWidth=Math.max(2,sr*0.18);
    ctx.beginPath();ctx.arc(sx,sy,sr*0.52,0,Math.PI*2);ctx.fill();
    ctx.lineWidth=Math.max(1.5,sr*0.13);
    for(var a=0;a<8;a++){var ra=a*Math.PI/4;ctx.beginPath();ctx.moveTo(sx+Math.cos(ra)*sr*0.72,sy+Math.sin(ra)*sr*0.72);ctx.lineTo(sx+Math.cos(ra)*sr,sy+Math.sin(ra)*sr);ctx.stroke();}
  }
  function _moon(ctx,mx,my,mr){
    ctx.fillStyle='#ddd';ctx.beginPath();ctx.arc(mx,my,mr*0.62,0,Math.PI*2);ctx.fill();
    ctx.fillStyle='#000';ctx.beginPath();ctx.arc(mx+mr*0.28,my-mr*0.12,mr*0.52,0,Math.PI*2);ctx.fill();
  }
  function _drawIcon(ctx,code,x,y,sz){
    if(!code||sz<4)return;
    var id=(code||'').substring(0,2);var night=code.length>2&&code[2]==='n';
    ctx.save();ctx.lineCap='round';ctx.lineJoin='round';
    var cx=x+sz/2,cy=y+sz/2,r=sz*0.40;
    if(id==='01'){if(night)_moon(ctx,cx,cy,r);else _sun(ctx,cx,cy,r,'#ffdd44');}
    else if(id==='02'){if(night)_moon(ctx,cx-r*0.18,cy-r*0.28,r*0.52);else _sun(ctx,cx-r*0.16,cy-r*0.28,r*0.50,'#ffdd44');_fillCloud(ctx,cx+r*0.10,cy+r*0.30,r*0.60,'#ccc');}
    else if(id==='03'){_fillCloud(ctx,cx,cy,r*0.82,'#bbb');}
    else if(id==='04'){_fillCloud(ctx,cx+r*0.12,cy+r*0.22,r*0.62,'#888');_fillCloud(ctx,cx-r*0.10,cy-r*0.06,r*0.58,'#aaa');}
    else if(id==='09'){_fillCloud(ctx,cx,cy-r*0.10,r*0.68,'#999');ctx.strokeStyle='#66aaff';ctx.lineWidth=Math.max(1.5,sz*0.045);for(var i=0;i<4;i++){var rx=cx-r*0.40+i*r*0.28;ctx.beginPath();ctx.moveTo(rx,cy+r*0.32);ctx.lineTo(rx-r*0.12,cy+r*0.64);ctx.stroke();}}
    else if(id==='10'){if(!night)_sun(ctx,cx-r*0.24,cy-r*0.40,r*0.38,'#ffdd44');_fillCloud(ctx,cx+r*0.04,cy+r*0.04,r*0.60,'#aaa');ctx.strokeStyle='#66aaff';ctx.lineWidth=Math.max(1.5,sz*0.045);for(var i=0;i<3;i++){var rx=cx-r*0.24+i*r*0.26;ctx.beginPath();ctx.moveTo(rx,cy+r*0.26);ctx.lineTo(rx-r*0.10,cy+r*0.56);ctx.stroke();}}
    else if(id==='11'){_fillCloud(ctx,cx,cy-r*0.12,r*0.72,'#777');ctx.strokeStyle='#ffd700';ctx.lineWidth=Math.max(2.5,sz*0.07);ctx.beginPath();ctx.moveTo(cx+r*0.06,cy+r*0.20);ctx.lineTo(cx-r*0.16,cy+r*0.50);ctx.lineTo(cx+r*0.02,cy+r*0.50);ctx.lineTo(cx-r*0.20,cy+r*0.84);ctx.stroke();}
    else if(id==='13'){ctx.strokeStyle='#aaddff';ctx.fillStyle='#aaddff';ctx.lineWidth=Math.max(1.5,sz*0.045);for(var i=0;i<6;i++){var a=i*Math.PI/3;ctx.beginPath();ctx.moveTo(cx,cy);ctx.lineTo(cx+Math.cos(a)*r*0.72,cy+Math.sin(a)*r*0.72);ctx.stroke();var bx=cx+Math.cos(a)*r*0.42,by=cy+Math.sin(a)*r*0.42,perp=a+Math.PI/2;ctx.beginPath();ctx.moveTo(bx+Math.cos(perp)*r*0.20,by+Math.sin(perp)*r*0.20);ctx.lineTo(bx-Math.cos(perp)*r*0.20,by-Math.sin(perp)*r*0.20);ctx.stroke();}ctx.beginPath();ctx.arc(cx,cy,r*0.12,0,Math.PI*2);ctx.fill();for(var i=0;i<3;i++){ctx.beginPath();ctx.arc(cx-r*0.28+i*r*0.28,cy+r*0.82,r*0.08,0,Math.PI*2);ctx.fill();}}
    else if(id==='50'){ctx.strokeStyle='#aaa';for(var i=0;i<4;i++){ctx.globalAlpha=1.0-i*0.20;ctx.lineWidth=Math.max(2,sz*0.05);var fy=y+sz*(0.22+i*0.20),fw=sz*(0.55+(i%2)*0.20),fx=x+(sz-fw)/2;ctx.beginPath();ctx.moveTo(fx,fy);ctx.quadraticCurveTo(fx+fw*0.5,fy+r*(i%2?0.06:-0.06),fx+fw,fy);ctx.stroke();}ctx.globalAlpha=1;}
    else{ctx.strokeStyle='#555';ctx.lineWidth=Math.max(1.5,sz*0.04);ctx.strokeRect(x+sz*0.1,y+sz*0.1,sz*0.8,sz*0.8);ctx.fillStyle='#888';ctx.font=Math.max(8,sz*0.20)+'px monospace';ctx.fillText((code||'?').substring(0,3),x+sz*0.18,y+sz*0.62);}
    ctx.restore();
  }

})();
</script>
)prevjs";

// ===================== STATE VARIABLES =====================
#define ECO_SCREEN_TIMEOUT  20000UL   // 20s schermo acceso dopo wake in eco mode
#define ECO_SLEEP_TIMEOUT   30000UL   // 30s totali prima di tornare in deep sleep
unsigned long lastUserInput = 0;      // aggiornato da pulsante E da noteWebActivity
int  hours,minutes,seconds,day,month,year,wdayBit=0;
int  dateFormat=0,currentLang=0;
bool isAPMode=false;
String posixTz="CET-1CEST,M3.5.0,M10.5.0/3";
int  clockFont=0,autoClockReturnMin=5;
unsigned long modeChangedAt=0;
bool webThemeLight=false;
int  nmMode=0;
int  nmStartH=23,nmStartM=0,nmEndH=7,nmEndM=0,nmWakeTime=5;
bool isScreenOn=true,manualScreenOff=false;
unsigned long wakeScreenUntil=0;
int  pixelBrightness=100;

// Auto brightness
#define AB_MAX_SLOTS 8
struct BrightSlot { uint16_t minuteOfDay; uint8_t pct; };
bool autoBrightEnabled=false;
int  autoBrightSlotCount=7;
BrightSlot autoBrightSlots[AB_MAX_SLOTS]={{360,50},{420,75},{600,100},{1020,75},{1080,50},{1200,25},{1260,1}};
int  lastAutoBrightMinute=-1;

bool inSettingsMode=false;
int  settingsIdx=0,settingsLevel=0;

int  screenOffAnim=1,screenOffStepMs=30;
bool wakeAnim=false;
const char* screenOffAnimNames[]={"None","Checkerboard","Swipe Down","Swipe Up","Swipe Right","Swipe Left","Diagonal","Curtain","Dissolve","Implode"};
const char* screenOffAnimShort[]={"None","Checker","SwipeD","SwipeU","SwipeR","SwipeL","Diagonal","Curtain","Dissolve","Implode"};
const char* rotNameShort[] = {"0\xc2\xb0", "90R", "180\xc2\xb0", "90L"};
const int SCREEN_OFF_ANIM_COUNT=10;

enum Mode { MODE_WEATHER=0,MODE_CLOCK=1,MODE_CALENDAR=2,MODE_FORECAST=3 };
Mode currentMode=MODE_CLOCK;

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
unsigned long lastWeatherUpdate=0,lastCalendarUpdate=0,lastNtpCheck=0;
int  alarmHour=7,alarmMinute=0,alarmPatternIdx=0;
bool alarmEnabled=false,isAlarmRinging=false,muteUI=false;
int  alarmDays=0b1111111;
const char* alarmPatternNames[]={"Digital Beep","Nervous Cricket","Melodic Rise","Sci-Fi Siren"};
String otaPass="",otaUser="";
String savedSSID="",savedPass="";
bool ecoMode=false;
int  lastAlarmFiredDay=-1;
bool ecoSyncing=false;
unsigned long ecoSyncStart=0;
bool ecoRadioStopped=false;
bool simBtnTap  = false;
bool simBtnHold = false;
bool simBtnMed  = false;
int displayRotation = 0;
unsigned long lastWebActivity = 0;

// ===================== FORWARD DECLARATIONS =====================
void refreshDisplay();
bool checkNightMode();
void updateDisplayClock();
void updateDisplayWeather();
void updateDisplayCalendar();
void fetchWeather();
void fetchCalendar();
void drawSettingsMenu();
inline bool isSoundSuppressed();
static int          smenuCount();
static const char*  smenuName(int i);
static String       getSettingValue(int level, int i);
static void applySettingChange(int i);
static void noteWebActivity();
static void applyPower(PowerMode mode);
void drawWeatherIcon(const String& code,int x,int y,bool large);
String localizeCalTime(const String& raw);
void updateDisplayClock_Portrait();
void updateDisplayWeather_Portrait();
void updateDisplayCalendar_Portrait();
void drawSettingsMenu_Portrait();
bool isPortrait();
void applyDisplayRotation();
static inline void animateScreenOff();
static inline void animateScreenOn();
void startAlarmSound();
void playSaveSound();
static void playSaveSoundBlocking();
void handleButtonTap();
void handleButtonHold();
void handleButtonMed();
void startAlarmRoutine();

static TaskHandle_t buttonTaskHandle = NULL;

void IRAM_ATTR buttonISR() {
    BaseType_t hp = pdFALSE;
    vTaskNotifyGiveFromISR(buttonTaskHandle, &hp);
    portYIELD_FROM_ISR(hp);
}

// ===================== BUFFER HELPERS (identici) =====================
static inline void u8ClearPx(uint8_t*b,int x,int y){b[(y>>3)*128+x]&=~(uint8_t)(1u<<(y&7));}
static inline void u8ClearRow(uint8_t*b,int y){uint8_t*p=b+(y>>3)*128;uint8_t m=~(uint8_t)(1u<<(y&7));for(int x=0;x<128;x++)p[x]&=m;}
static inline void u8ClearCol(uint8_t*b,int x){for(int pg=0;pg<8;pg++)b[pg*128+x]=0;}
static inline void u8RevealPx(uint8_t*b,const uint8_t*s,int x,int y){if(s[(y>>3)*128+x]&(uint8_t)(1u<<(y&7)))b[(y>>3)*128+x]|=(uint8_t)(1u<<(y&7));}
static inline void u8RevealRow(uint8_t*b,const uint8_t*s,int y){uint8_t*p=b+(y>>3)*128;const uint8_t*sp=s+(y>>3)*128;uint8_t m=(uint8_t)(1u<<(y&7));for(int x=0;x<128;x++)if(sp[x]&m)p[x]|=m;}
static inline void u8RevealCol(uint8_t*b,const uint8_t*s,int x){for(int pg=0;pg<8;pg++)b[pg*128+x]|=s[pg*128+x];}
static const uint8_t BAYER4[16] PROGMEM={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
static void applyContrast(int pct){uint8_t v=(uint8_t)((constrain(pct,0,100)*255)/100);display.ssd1306_command(0x81); display.ssd1306_command(v);}

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

// ===================== RTC TIME MANAGEMENT =====================
void updateTimeFromRTC() {
    if (rtc_sleep_us > 0) {
        time_t now = rtc_last_sync + (rtc_sleep_us / 1000000LL);
        struct timeval tv = { .tv_sec = now, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        rtc_sleep_us = 0;
    }
}

void saveCurrentTimeToRTC() {
    time(&rtc_last_sync);
}

void goToDeepSleep() {
    if (currentPowerState != STATE_ACTIVE) return;
    if (isAlarmRinging) return;
    if (inSettingsMode) return;

    // In eco mode: dormi appena scaduto il timeout inattività
    // In modalità normale: dormi solo se schermo spento da un po'
    bool canSleep;
    if (ecoMode) {
        canSleep = (millis() - lastUserInput) >= ECO_SLEEP_TIMEOUT;
    } else {
        canSleep = !isScreenOn
            && (millis() - lastWebActivity) >= WEB_ACTIVE_TIMEOUT;
    }
    if (!canSleep) return;
    if (uxQueueMessagesWaiting(eventQueue) > 0) return;

    // Forza schermo off se ancora acceso (eco mode con timeout schermo)
    if (isScreenOn) {
        if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            display.ssd1306_command(SSD1306_DISPLAYOFF);
            u8g2.setPowerSave(1);
            xSemaphoreGive(displayMutex);
        }
        isScreenOn = false;
    }

    saveCurrentTimeToRTC();
    WiFi.disconnect(true);
    esp_wifi_stop();

    // Timer: 60s base, oppure esattamente all'ora della sveglia se più vicina
    uint64_t sleep_us = 60ULL * 1000000ULL;
    if (alarmEnabled && isAlarmActiveToday()) {
        time_t now; time(&now);
        struct tm tm_alarm = *localtime(&now);
        tm_alarm.tm_hour = alarmHour;
        tm_alarm.tm_min  = alarmMinute;
        tm_alarm.tm_sec  = 0;
        time_t alarm_ts  = mktime(&tm_alarm);
        if (alarm_ts <= now) alarm_ts += 86400;
        uint64_t to_alarm = (alarm_ts - now) * 1000000ULL;
        if (to_alarm < sleep_us) sleep_us = to_alarm;
    }

    esp_sleep_enable_timer_wakeup(sleep_us);
    esp_sleep_enable_ext1_wakeup(1ULL << GPIO_NUM_9, ESP_EXT1_WAKEUP_ANY_LOW);

    currentPowerState = STATE_DEEP_SLEEP;
    esp_deep_sleep_start();
}

// ===================== BUZZER (identico all'originale) =====================
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

#if ENABLE_BUZZER
static const BuzzerStep p_classic[]={{2000,100},{0,100},{2000,100},{0,700}};
static const BuzzerStep p_nervous[]={{2800,35},{0,25},{2800,35},{0,25},{2800,35},{0,600}};
static const BuzzerStep p_melodic[]={{523,160},{659,160},{784,160},{1047,220},{784,100},{1047,320},{0,380}};
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

// ===================== UNIFIED SCREEN ANIMATION (identico) =====================
void animateScreen(bool reveal){
  if(screenOffAnim==0) return;
  if(reveal&&!wakeAnim) return;
  if(isPortrait()){
    if(!reveal){
      if(xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memset(u8g2.getBufferPtr(),0,1024); u8g2.sendBuffer();
        display.clearDisplay(); display.display();
        xSemaphoreGive(displayMutex);
      }
    } else { refreshDisplay(); }
    return;
  }
  int sms=constrain(screenOffStepMs,1,500);
  uint8_t* buf;
  if(xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
  buf = u8g2.getBufferPtr();
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
  auto animStep=[&](){buzzer.update();xSemaphoreGive(displayMutex);vTaskDelay(pdMS_TO_TICKS(sms));xSemaphoreTake(displayMutex, portMAX_DELAY);};
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
  xSemaphoreGive(displayMutex);
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

// ===================== POWER MANAGEMENT =====================
static void applyPower(PowerMode mode){
  if(mode==_pwrCur) return;
  _pwrCur=mode;
  switch(mode){
    case PWR_DEEP:        setCpuFrequencyMhz(10);  break;
    case PWR_DISPLAY:     setCpuFrequencyMhz(20);  break;
    case PWR_WIFI_IDLE:   setCpuFrequencyMhz(80);  esp_wifi_set_ps(WIFI_PS_MAX_MODEM); break;
    case PWR_WIFI_ACTIVE: setCpuFrequencyMhz(80);  esp_wifi_set_ps(WIFI_PS_MIN_MODEM); break;
    case PWR_FETCH:       setCpuFrequencyMhz(160); esp_wifi_set_ps(WIFI_PS_MIN_MODEM); break;
    case PWR_OTA:         setCpuFrequencyMhz(160); esp_wifi_set_ps(WIFI_PS_NONE);      break;
  }
}
static inline PowerMode idleMode(){
  return (WiFi.status()==WL_CONNECTED&&!isAPMode) ? PWR_WIFI_IDLE : (isScreenOn ? PWR_DISPLAY : PWR_DEEP);
}
static void initPowerManagement(){
  esp_pm_config_t pm={.max_freq_mhz=160,.min_freq_mhz=10,.light_sleep_enable=true};
  esp_pm_configure(&pm);
}

static void noteWebActivity() {
    lastWebActivity = millis();
    lastUserInput   = millis();          // <-- aggiunto
    applyPower(PWR_WIFI_ACTIVE);
    if (eventQueue) {
        SystemEvent ev = EVENT_WEB_ACTIVITY;
        xQueueSend(eventQueue, &ev, 0);
    }
}

// ===================== DISPLAY ROTATION =====================
bool isPortrait() { return displayRotation == 1 || displayRotation == 3; }
void applyDisplayRotation() {
  static const u8g2_cb_t* u8rot[] = {U8G2_R0, U8G2_R1, U8G2_R2, U8G2_R3};
  u8g2.setDisplayRotation(u8rot[displayRotation]);
  display.setRotation(displayRotation);
}

// ===================== NIGHT MODE =====================
bool checkNightMode(){
  if(nmMode==0)return false; if(nmMode==2)return true;
  int cur=hours*60+minutes,start=nmStartH*60+nmStartM,end=nmEndH*60+nmEndM;
  return(start<end)?(cur>=start&&cur<end):(cur>=start||cur<end);
}
inline bool isSoundSuppressed(){return muteUI||checkNightMode();}
bool isAlarmActiveToday(){time_t et=time(nullptr);struct tm ti;localtime_r(&et,&ti);int bit=(ti.tm_wday==0)?6:ti.tm_wday-1;return(alarmDays&(1<<bit))!=0;}
void readLocalTime(){time_t et=time(nullptr);struct tm ti;localtime_r(&et,&ti);hours=ti.tm_hour;minutes=ti.tm_min;seconds=ti.tm_sec;day=ti.tm_mday;month=ti.tm_mon+1;year=ti.tm_year+1900;wdayBit=(ti.tm_wday==0)?6:ti.tm_wday-1;}

// ===================== DISPLAY FUNCTIONS (adattate con mutex) =====================
void refreshDisplay(){
  if(xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
  if(inSettingsMode){drawSettingsMenu();}
  else{
    switch(currentMode){
      case MODE_WEATHER:case MODE_FORECAST:
        isPortrait()?updateDisplayWeather_Portrait():updateDisplayWeather(); break;
      case MODE_CALENDAR:
        isPortrait()?updateDisplayCalendar_Portrait():updateDisplayCalendar(); break;
      default:
        isPortrait()?updateDisplayClock_Portrait():updateDisplayClock(); break;
    }
  }
  xSemaphoreGive(displayMutex);
}

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
  if(alarmEnabled){time_t t2=time(nullptr)+86400;struct tm tp;localtime_r(&t2,&tp);int tb=(tp.tm_wday==0)?6:tp.tm_wday-1;if(alarmDays&(1<<tb))display.fillCircle(120,56,3,WHITE);else display.drawCircle(120,56,3,WHITE);}
#endif
  if(hasCalendarEvent){display.drawRect(5,52,8,8,WHITE);display.drawLine(5,54,13,54,WHITE);display.drawPixel(9,57,WHITE);}
  if(ecoSyncing){
    display.setFont(NULL);display.setTextSize(1);
    display.setCursor(114,0);
    display.print((millis()/400)%2?"~*":"*~");
  }
  display.display();
}

void updateDisplayClock_Portrait() {
  display.clearDisplay(); display.setTextColor(WHITE);
  display.setFont(clockFontList[clockFont]);
  char hh[3], mm2[3];
  snprintf(hh, 3, "%02d", hours); snprintf(mm2, 3, "%02d", minutes);
  int16_t bx,by; uint16_t bw,bh;
  int W = display.width();
  display.getTextBounds(hh,0,0,&bx,&by,&bw,&bh);
  display.setCursor((W-(int)bw)/2-bx, 44);
  display.print(hh);
  display.drawLine(5,92,W-5,92,WHITE);
  display.getTextBounds(mm2,0,0,&bx,&by,&bw,&bh);
  display.setCursor((W-(int)bw)/2-bx, 86);
  display.print(mm2);
  display.setFont(NULL); display.setTextSize(1);
  char dd2[3], mn2[3];
  snprintf(dd2,3,"%02d",day); snprintf(mn2,3,"%02d",month);
  String dateS;
  int dfmt = (dateFormat>=3)?4:dateFormat;
  if(dfmt==1)      dateS=String(mn2)+"/"+dd2+"/"+year;
  else if(dfmt==2) dateS=String(year)+"/"+mn2+"/"+dd2;
  else if(dfmt==4) dateS=String(dayNames[currentLang][wdayBit])+" "+dd2+" "+monthShort[currentLang][month-1];
  else             dateS=String(dd2)+"/"+mn2+"/"+year;
  display.getTextBounds(dateS.c_str(),0,0,&bx,&by,&bw,&bh);
  display.setCursor(max(0,(W-(int)bw)/2),100);
  display.print(dateS);
  if(dateFormat<3){
    const char* dn=dayNames[currentLang][wdayBit];
    display.getTextBounds(dn,0,0,&bx,&by,&bw,&bh);
    display.setCursor((W-(int)bw)/2,112); display.print(dn);
  }
#if ENABLE_BUZZER
  if(alarmEnabled){
    time_t t2=time(nullptr)+86400; struct tm tp; localtime_r(&t2,&tp);
    int tb=(tp.tm_wday==0)?6:tp.tm_wday-1;
    if(alarmDays&(1<<tb)) display.fillCircle(6,122,3,WHITE);
    else                   display.drawCircle(6,122,3,WHITE);
  }
#endif
  if(hasCalendarEvent){
    display.drawRect(W-14,116,8,8,WHITE);
    display.drawLine(W-14,118,W-6,118,WHITE);
    display.drawPixel(W-10,121,WHITE);
  }
  if(ecoSyncing){
    display.setFont(NULL);display.setTextSize(1);
    display.setCursor(W-12,0);
    display.print((millis()/400)%2?"~*":"*~");
  }
  display.display();
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
  u8g2.sendBuffer();
}
void updateDisplayWeather_Portrait() {
  bool fc=(currentMode==MODE_FORECAST);
  u8g2.clearBuffer();
  if(fc){
    u8g2.setFont(u8g2_font_5x7_tr);
    int tw=u8g2.getStrWidth(labels[currentLang][L_TOMORROW]);
    u8g2.setCursor((64-tw)/2,8); u8g2.print(labels[currentLang][L_TOMORROW]);
  }
  u8g2.setFont(u8g2_font_helvB14_tf);
  const char* tmp=fc?w_forecast_temp.c_str():w_temp.c_str();
  int tw=u8g2.getStrWidth(tmp);
  u8g2.setCursor((64-tw)/2,22); u8g2.print(tmp);
  drawWeatherIcon(fc?w_forecast_icon:w_iconCode, 16, 62, true);
  u8g2.setFont(u8g2_font_6x10_tr);
  String hum=String(labels[currentLang][L_HUM])+(fc?w_forecast_hum:w_hum)+"%";
  tw=u8g2.getStrWidth(hum.c_str());
  u8g2.setCursor((64-tw)/2,76); u8g2.print(hum);
  u8g2.setFont(u8g2_font_5x7_tr);
  String bot=fc?("v "+w_forecast_min+" ^"+w_forecast_max):w_cityDisplay;
  tw=u8g2.getStrWidth(bot.c_str());
  u8g2.setCursor((64-tw)/2,88); u8g2.print(bot);
  const char* desc=fc?w_forecast_desc.c_str():w_desc.c_str();
  String d(desc);
  int dw=u8g2.getStrWidth(desc);
  if(dw<=64){
    u8g2.setCursor((64-dw)/2,100); u8g2.print(desc);
  } else {
    int brk=d.indexOf(' ');
    if(brk>0){
      String l1=d.substring(0,brk), l2=d.substring(brk+1);
      int w1=u8g2.getStrWidth(l1.c_str()), w2=u8g2.getStrWidth(l2.c_str());
      u8g2.setCursor((64-w1)/2,98); u8g2.print(l1.c_str());
      u8g2.setCursor((64-w2)/2,108); u8g2.print(l2.c_str());
    } else { u8g2.setCursor(0,100); u8g2.print(desc); }
  }
  u8g2.sendBuffer();
}
void updateDisplayCalendar(){
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_open_iconic_embedded_2x_t);u8g2.drawFrame(2,7,8,8);u8g2.drawLine(2,9,10,9);u8g2.drawPixel(6,12);
  u8g2.setFont(u8g2_font_6x12_tr);u8g2.setCursor(20,15);u8g2.print(labels[currentLang][L_CAL]);
  if(hasCalendarEvent&&cal_eventCount>0){char cb[24];snprintf(cb,sizeof(cb),"%d/%d",cal_currentEventIdx+1,cal_eventCount);u8g2.setFont(u8g2_font_5x7_tr);int cw=u8g2.getStrWidth(cb);u8g2.setCursor(127-cw,10);u8g2.print(cb);}
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
  u8g2.sendBuffer();
}
void updateDisplayCalendar_Portrait() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);
  int tw=u8g2.getStrWidth(labels[currentLang][L_CAL]);
  u8g2.setCursor((64-tw)/2,14); u8g2.print(labels[currentLang][L_CAL]);
  u8g2.drawHLine(0,17,64);
  if(hasCalendarEvent&&cal_eventCount>0){
    char cb[12]; snprintf(cb,sizeof(cb),"%d/%d",cal_currentEventIdx+1,cal_eventCount);
    u8g2.setFont(u8g2_font_5x7_tr);
    tw=u8g2.getStrWidth(cb); u8g2.setCursor(64-tw,8); u8g2.print(cb);
    String dt=localizeCalTime(cal_times[cal_currentEventIdx]);
    const String&ev=cal_events[cal_currentEventIdx];
    String dp="",to=dt;
    if(dt.length()>0&&!isDigit((unsigned char)dt[0])){
      int sp=dt.indexOf(' ');
      if(sp>0){dp=dt.substring(0,sp);to=dt.substring(sp+1);}
    }
    int yp=30;
    if(dp.length()>0){
      u8g2.setFont(u8g2_font_5x7_tr);
      tw=u8g2.getStrWidth(dp.c_str());
      u8g2.setCursor((64-tw)/2,yp); yp+=10; u8g2.print(dp.c_str());
    }
    u8g2.setFont(u8g2_font_helvB10_tf);
    tw=u8g2.getStrWidth(to.c_str());
    u8g2.setCursor((64-tw)/2,yp+12); yp+=24; u8g2.print(to.c_str());
    u8g2.setFont(u8g2_font_6x10_tr);
    if(textWidth==0) textWidth=u8g2.getStrWidth(ev.c_str());
    int evY=max(yp+4,68);
    if(textWidth<=64){ u8g2.setCursor((64-textWidth)/2,evY); }
    else             { u8g2.setCursor(-scrollX,evY); }
    u8g2.print(ev.c_str());
  } else {
    u8g2.setFont(u8g2_font_6x10_tr);
    tw=u8g2.getStrWidth(labels[currentLang][L_NO_EVENT]);
    u8g2.setCursor((64-tw)/2,65); u8g2.print(labels[currentLang][L_NO_EVENT]);
  }
  u8g2.sendBuffer();
}

// ===================== SETTINGS MENU (identico) =====================
#define SMENU_TOP_N  5
static const char* sTopNames[SMENU_TOP_N]  ={"< EXIT","> Controls","> WiFi","> Display","> System"};
#define SMENU_CTRL_N 3
static const char* sCtrlNames[SMENU_CTRL_N]={"< Back","Mute","Alarm"};
#define SMENU_WIFI_N 3
static const char* sWifiNames[SMENU_WIFI_N]={"< Back","Network","EcoMode"};
#define SMENU_DISP_N 9
static const char* sDispNames[SMENU_DISP_N]={"< Back","Screen","Bright","Font","NightMode","DateFmt","Anim","AutoRet.","Rotation"};
#define SMENU_SYS_N  4
static const char* sSysNames[SMENU_SYS_N]  ={"< Back","Language","Restart","Info"};
#define SMENU_INFO_N 5
static const char* sInfoNames[SMENU_INFO_N]={"< Back","CPU","Temp","WiFi","Uptime"};
static const char* dateFmtShort[]={"DD/MM/YY","MM/DD/YY","YY/MM/DD","Long","Med"};

static String fmtUptime(){
  unsigned long s=millis()/1000;
  unsigned long d=s/86400,h=(s%86400)/3600,m=(s%3600)/60;
  char b[12];
  if(d>0) snprintf(b,sizeof(b),"%lud%luh",(unsigned long)d,(unsigned long)h);
  else    snprintf(b,sizeof(b),"%luh%lum",(unsigned long)h,(unsigned long)m);
  return String(b);
}
static String fmtRSSI(){
  if(WiFi.status()!=WL_CONNECTED||ecoRadioStopped) return "--";
  return String(WiFi.RSSI())+"dBm";
}

static int smenuCount(){
  switch(settingsLevel){
    case 1: return SMENU_CTRL_N;
    case 2: return SMENU_WIFI_N;
    case 3: return SMENU_DISP_N;
    case 4: return SMENU_SYS_N;
    case 5: return SMENU_INFO_N;
    default:return SMENU_TOP_N;
  }
}
static const char* smenuName(int i){
  switch(settingsLevel){
    case 1: return sCtrlNames[i];
    case 2: return sWifiNames[i];
    case 3: return sDispNames[i];
    case 4: return sSysNames[i];
    case 5: return sInfoNames[i];
    default:return sTopNames[i];
  }
}
static const char* smenuTitle(){
  switch(settingsLevel){
    case 1: return "- Controls -";
    case 2: return "-- WiFi --";
    case 3: return "-- Display --";
    case 4: return "-- System --";
    case 5: return "--- Info ---";
    default:return "-- SETTINGS --";
  }
}
static String getSettingValue(int level,int i){
  if(level==1){
    switch(i){
      case 1:{String v=muteUI?"ON*":"OFF";return v;}
      case 2:{String v=alarmEnabled?"ON*":"OFF";return v;}
      default:return"";
    }
  }
  if(level==2){
    switch(i){
      case 1:{if(isAPMode)return"AP";if(WiFi.status()==WL_CONNECTED)return WiFi.localIP().toString();if(ecoMode)return"eco";return"off";}
      case 2:{String v=ecoMode?"ON":"OFF*";return v;}
      default:return"";
    }
  }
  if(level==3){
    switch(i){
      case 1:{String v=isScreenOn?"ON":"OFF";return v;}
      case 2:{String v=String(pixelBrightness)+"%";if(pixelBrightness==100)v+='*';return v;}
      case 3:{String v=clockFontNames[clockFont];if(clockFont==0)v+='*';return v;}
      case 4:{static const char*nm[]={"Off","Sched","Man"};String v=nm[nmMode];if(nmMode==0)v+='*';return v;}
      case 5:{String v=dateFmtShort[dateFormat];if(dateFormat==0)v+='*';return v;}
      case 6:{String v=screenOffAnimShort[screenOffAnim];if(screenOffAnim==1)v+='*';return v;}
      case 7:{String v=autoClockReturnMin>0?String(autoClockReturnMin)+"m":"OFF";if(autoClockReturnMin==5)v+='*';return v;}
      case 8:{String v=rotNameShort[displayRotation];if(displayRotation==0)v+='*';return v;}
      default:return"";
    }
  }
  if(level==4){
    switch(i){
      case 1:{String v=String(langNames[currentLang]).substring(0,3);if(currentLang==0)v+='*';return v;}
      case 3: return ">";
      default:return"";
    }
  }
  if(level==5){
    switch(i){
      case 1: return String(ESP.getCpuFreqMHz())+"MHz";
      case 2: return String((int)temperatureRead())+"C";
      case 3: return fmtRSSI();
      case 4: return fmtUptime();
      default:return"";
    }
  }
  return"";
}

static void applySettingChange(int i){
  if(i==0){
    if(settingsLevel==0){inSettingsMode=false;settingsIdx=0;settingsLevel=0;refreshDisplay();}
    else if(settingsLevel==5){settingsLevel=4;settingsIdx=0;}
    else{settingsLevel=0;settingsIdx=0;}
    return;
  }
  if(settingsLevel==0&&i>=1&&i<=4){settingsLevel=i;settingsIdx=0;return;}

  preferences.begin("clock_cfg",false);

  if(settingsLevel==1){
    switch(i){
      case 1: muteUI=!muteUI;preferences.putBool("mute",muteUI);break;
      case 2: alarmEnabled=!alarmEnabled;preferences.putBool("ae",alarmEnabled);break;
    }
  }
  else if(settingsLevel==2){
    switch(i){
      case 1:{
        preferences.end();
        u8g2.clearBuffer();u8g2.setFont(u8g2_font_6x10_tr);u8g2.drawStr(15,35,labels[currentLang][L_RESTART]);u8g2.sendBuffer();
        if(!isAPMode){preferences.begin("clock_cfg",false);preferences.putBool("force_ap",true);preferences.end();}
        delay(800);ESP.restart();return;
      }
      case 2:{
        ecoMode=!ecoMode;
        preferences.putBool("eco_mode",ecoMode);
        if(ecoMode){
          ecoSyncing=false;
        } else {
          ecoSyncing=false;
          applyPower(PWR_FETCH);
          if(ecoRadioStopped){esp_wifi_start();ecoRadioStopped=false;}
          WiFi.begin(savedSSID.c_str(),savedPass.c_str());
        }
        break;
      }
    }
  }
  else if(settingsLevel==3){
    switch(i){
      case 1:
        if(isScreenOn){
          animateScreenOff();display.ssd1306_command(SSD1306_DISPLAYOFF);u8g2.setPowerSave(1);
          isScreenOn=false;manualScreenOff=true;inSettingsMode=false;settingsLevel=0;
          applyPower((WiFi.status()==WL_CONNECTED&&!ecoMode)?PWR_WIFI_IDLE:PWR_DEEP);
        } else {
          manualScreenOff=false;
          applyPower((WiFi.status()==WL_CONNECTED&&!ecoMode)?PWR_WIFI_IDLE:PWR_DISPLAY);
          display.ssd1306_command(SSD1306_DISPLAYON);u8g2.setPowerSave(0);isScreenOn=true;
        }
        break;
      case 2:
        {static const int bs[]={1,25,50,75,100};const int bn=5;int cur=bn-1;for(int k=0;k<bn;k++)if(bs[k]==pixelBrightness){cur=k;break;}pixelBrightness=bs[(cur+1)%bn];applyContrast(pixelBrightness);preferences.putInt("pct_bright",pixelBrightness);refreshDisplay();break;}
      case 3:clockFont=(clockFont+1)%3;preferences.putInt("cfont",clockFont);break;
      case 4:nmMode=(nmMode+1)%3;preferences.putInt("nm_mode",nmMode);break;
      case 5:dateFormat=(dateFormat+1)%5;preferences.putInt("df",dateFormat);break;
      case 6:screenOffAnim=(screenOffAnim+1)%SCREEN_OFF_ANIM_COUNT;preferences.putInt("so_anim",screenOffAnim);break;
      case 7:{static const int st[]={0,1,2,5,10,30};const int n=6;int cur=0;for(int k=0;k<n;k++)if(st[k]==autoClockReturnMin){cur=k;break;}autoClockReturnMin=st[(cur+1)%n];modeChangedAt=millis();preferences.putInt("acr",autoClockReturnMin);break;}
      case 8:displayRotation=(displayRotation+1)%4;applyDisplayRotation();preferences.putInt("rot",displayRotation);break;    
    }
  }
  else if(settingsLevel==4){
    switch(i){
      case 1:currentLang=(currentLang+1)%5;preferences.putInt("lng",currentLang);break;
      case 2:{
        preferences.end();
        u8g2.clearBuffer();u8g2.setFont(u8g2_font_6x10_tr);u8g2.drawStr(10,35,labels[currentLang][L_RESTART]);u8g2.sendBuffer();
        delay(800);ESP.restart();return;
      }
      case 3:preferences.end();settingsLevel=5;settingsIdx=0;return;
    }
  }
  else if(settingsLevel==5){
    if(i>=1) buzzer.playUISound(600,80);
    preferences.end();
    return;
  }
  preferences.end();
}

void drawSettingsMenu(){
  if(isPortrait()){drawSettingsMenu_Portrait();return;}
  int total=smenuCount();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);
  const char* title=smenuTitle();
  u8g2.drawStr((128-u8g2.getStrWidth(title))/2,11,title);
  u8g2.drawHLine(0,13,128);
  const int vis=4;
  int start=settingsIdx-1;if(start<0)start=0;if(start>total-vis)start=total-vis;if(start<0)start=0;
  u8g2.setFont(u8g2_font_6x10_tr);
  for(int i=0;i<vis&&(start+i)<total;i++){
    int idx=start+i;int y=24+i*12;bool sel=(idx==settingsIdx);
    if(sel){u8g2.setDrawColor(1);u8g2.drawRBox(0,y-9,125,11,2);u8g2.setDrawColor(0);}
    u8g2.drawStr(4,y,smenuName(idx));
    String val=getSettingValue(settingsLevel,idx);
    if(val.length()>0)u8g2.drawStr(124-u8g2.getStrWidth(val.c_str()),y,val.c_str());
    if(sel)u8g2.setDrawColor(1);
  }
  u8g2.drawVLine(127,14,50);
  int bh=max(4,(vis*50)/total);
  int by=14+(settingsIdx*(50-bh))/(total>1?total-1:1);
  u8g2.drawBox(126,by,2,bh);u8g2.sendBuffer();
}

void drawSettingsMenu_Portrait() {
  int total=smenuCount();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  const char* title=smenuTitle();
  int tw=u8g2.getStrWidth(title);
  u8g2.setCursor((64-tw)/2,8); u8g2.print(title);
  u8g2.drawHLine(0,10,64);
  const int vis=8;
  int start=settingsIdx-3;
  if(start<0)start=0;
  if(start>total-vis)start=total-vis;
  if(start<0)start=0;
  for(int i=0;i<vis&&(start+i)<total;i++){
    int idx=start+i; int y=20+i*14; bool sel=(idx==settingsIdx);
    if(sel){u8g2.setDrawColor(1);u8g2.drawRBox(0,y-8,61,11,2);u8g2.setDrawColor(0);}
    u8g2.drawStr(3,y,smenuName(idx));
    String val=getSettingValue(settingsLevel,idx);
    if(val.length()>0){int vw=u8g2.getStrWidth(val.c_str());u8g2.drawStr(61-vw,y,val.c_str());}
    if(sel)u8g2.setDrawColor(1);
  }
  u8g2.drawVLine(63,11,117);
  int bh=max(4,(vis*117)/max(total,1));
  int by=11+(settingsIdx*(117-bh))/max(total-1,1);
  u8g2.drawBox(62,by,2,bh);
  u8g2.sendBuffer();
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

// ===================== BUTTON HANDLERS (usate nei task) =====================
void handleButtonTap(){
  if(isAlarmRinging){
#if ENABLE_BUZZER
    buzzer.stop();isAlarmRinging=false;
#endif
    if(currentMode==MODE_CLOCK) refreshDisplay();
  } else if(inSettingsMode){
    settingsIdx=(settingsIdx+1)%smenuCount();
#if ENABLE_BUZZER
    buzzer.playUISound(1800,50);
#endif
    drawSettingsMenu();
  } else {
    bool hw=(weatherKey!=""),hc=(googleScriptUrl!="");
    Mode nm=currentMode;
    if(currentMode==MODE_FORECAST) nm=MODE_WEATHER;
    else if(currentMode==MODE_CLOCK) nm=hc?MODE_CALENDAR:(hw?MODE_WEATHER:MODE_CLOCK);
    else if(currentMode==MODE_CALENDAR) nm=hw?MODE_WEATHER:MODE_CLOCK;
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
void handleButtonHold(){
  if(inSettingsMode){
    applySettingChange(settingsIdx);
#if ENABLE_BUZZER
    if(!isAlarmRinging) buzzer.playUISound(1400,80);
#endif
    if(inSettingsMode) drawSettingsMenu();
  } else {
    inSettingsMode=true;settingsIdx=0;settingsLevel=0;
#if ENABLE_BUZZER
    buzzer.playUISound(1200,200);
#endif
    drawSettingsMenu();
  }
}
void handleButtonMed(){
  if(!inSettingsMode){
    if(currentMode==MODE_WEATHER){
      currentMode=MODE_FORECAST;modeChangedAt=millis();
#if ENABLE_BUZZER
      buzzer.playUISound(1500,100);
#endif
      refreshDisplay();
    } else if(currentMode==MODE_CALENDAR&&hasCalendarEvent&&cal_eventCount>1){
      cal_currentEventIdx=(cal_currentEventIdx+1)%cal_eventCount;
      scrollX=0;textWidth=0;modeChangedAt=millis();
#if ENABLE_BUZZER
      buzzer.playUISound(1500,100);
#endif
      refreshDisplay();
    }
  }
}
void startAlarmRoutine(){
  lastAlarmFiredDay=day;
  isAlarmRinging=true;manualScreenOff=false;currentMode=MODE_CLOCK;
  wakeScreenUntil=ULONG_MAX;
  applyPower(PWR_DISPLAY);
  startAlarmSound();
}

// ===================== WEB SERVER HANDLERS (con noteWebActivity) =====================
static void sendP(const char* pgm){
  char buf[129]; size_t len=strlen_P(pgm),off=0;
  while(off<len){size_t n=min(len-off,(size_t)128);memcpy_P(buf,pgm+off,n);buf[n]='\0';server.sendContent(buf);off+=n;}
}
static void sendStr(const __FlashStringHelper* s){ server.sendContent(s); }
static void sendStr(const char* s)               { if(s&&s[0]!='\0') server.sendContent(s); }
static void sendStr(const String& s)             { if(s.length()>0)  server.sendContent(s); }

static void sendTopbar(const char* page){
  sendStr(F(
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
    "<meta charset='UTF-8'>"));
  sendP(CSS_VARS);
  sendStr(F("</head>"));
  sendStr(webThemeLight ? F("<body class='lm'>") : F("<body>"));
  sendStr(F("<div class='page'>"));
  sendStr(F("<div class='topbar'>"));
  sendStr(F("<span class='brand'>cuboid</span>"));
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
  sendStr(F("<button class='tbtn' onclick='togglePreview()'"
    " title='Anteprima display OLED'"
    " style='border-color:var(--teal);color:var(--teal);font-size:14px;padding:4px 9px;'"
    ">&#128250;</button>"));
  sendStr(F("<button class='tbtn exp-btn' id='expBtn' onclick='togglePanel()'"
    " title='Luminosita &amp; Schermo'>&#9788; &#9660;</button>"));
  sendStr(F("<div id='tb-panel' class='tb-panel' style='display:none'>"));
  sendStr(F("<input type='range' id='bright-sl' min='1' max='100' step='1' value='"));
  sendStr(String(pixelBrightness));
  sendStr(F("'><span class='tb-bv' id='bv'>"));
  sendStr(String(pixelBrightness));
  sendStr(F("%</span>"));
  sendStr(F("<button onclick='toggleScreenAjax(this)' class='tbtn"));
  if(!isScreenOn) sendStr(F(" mute-on"));
  sendStr(F("' id='screenBtn'>&#128421;</button>"));
  sendStr(F("</div>"));
  sendStr(F("</div>"));
  sendStr(F("<div id='_pp' style='display:none;position:fixed;bottom:16px;right:16px;"
    "background:var(--cbg);border:2px solid var(--teal);border-radius:14px;padding:14px;"
    "z-index:9999;box-shadow:0 8px 40px rgba(0,0,0,.65);"
    "width:min(420px,calc(100vw - 20px));backdrop-filter:blur(8px);'>"));
  sendStr(F("<div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;'>"));
  sendStr(F("<span style='font-size:11px;font-weight:900;color:var(--teal);letter-spacing:2px;'>"
    "&#128250; DISPLAY PREVIEW</span>"));
  sendStr(F("<button onclick='togglePreview()'"
    " style='background:none;border:none;color:var(--muted);cursor:pointer;"
    "font-size:20px;line-height:1;padding:0;'>&#10005;</button>"));
  sendStr(F("</div>"));
  sendStr(F("<canvas id='_oled' width='384' height='192'"
    " style='border-radius:8px;display:block;width:100%;"
    "image-rendering:pixelated;image-rendering:crisp-edges;"
    "border:2px solid #0a1a0a;"
    "box-shadow:0 0 18px rgba(0,200,60,0.18) inset,0 0 8px rgba(0,200,60,0.12);"
    "background:#000;'></canvas>"));
  sendStr(F("<div style='display:flex;gap:8px;margin-top:10px;'>"));
  sendStr(F("<button onclick=\"simBtn('Tap')\" class='tbtn'"
    " style='flex:1;font-size:12px;padding:8px 4px;'>"
    "&#8630; Tap <span style='font-size:10px;color:var(--muted);'>(modo)</span></button>"));
  sendStr(F("<button onclick=\"simBtn('Med')\" class='tbtn'"
    " style='flex:1;font-size:12px;padding:8px 4px;"
    "border-color:var(--teal);color:var(--teal);'>"
    "&#9654; Med <span style='font-size:10px;color:var(--muted);'>(fc / cal+)</span></button>"));
  sendStr(F("<button onclick=\"simBtn('Hold')\" class='tbtn'"
    " style='flex:1;font-size:12px;padding:8px 4px;"
    "border-color:var(--acc);color:var(--acc);'>"
    "&#9881; Hold <span style='font-size:10px;color:var(--muted);'>(settings)</span></button>"));
  sendStr(F("</div>"));
  sendStr(F("<div id='_pst'"
    " style='font-size:10px;color:var(--muted);text-align:center;"
    "margin-top:6px;font-family:monospace;letter-spacing:.5px;'></div>"));
  sendStr(F("<div style='font-size:9px;color:var(--brd);text-align:right;margin-top:4px;'>"
    "aggiornamento ogni 1s</div>"));
  sendStr(F("</div>"));
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

void handleScan(){
  noteWebActivity();
  if(isAPMode){ server.send(200,"application/json","[]"); return; }
  WiFi.scanDelete();
  int n=WiFi.scanNetworks();
  String j="[";
  for(int i=0;i<n;++i){if(i>0)j+=",";j+="{\"s\":\""+WiFi.SSID(i)+"\",\"r\":"+WiFi.RSSI(i)+",\"b\":\""+WiFi.BSSIDstr(i)+"\"}";}
  j+="]";
  WiFi.scanDelete();
  server.send(200,"application/json",j);
}

void handleSysInfo(){
  noteWebActivity();
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

void handleExportSettings(){
  noteWebActivity();
  preferences.begin("clock_cfg",true);
  JsonDocument doc;
  doc["ssid"]=preferences.getString("ssid","");doc["pass"]=preferences.getString("pass","");
  doc["lng"]=preferences.getInt("lng",0);doc["key"]=preferences.getString("key","");
  doc["city"]=preferences.getString("city","");doc["g_url"]=preferences.getString("g_url","");
  doc["tz_posix"]=preferences.getString("tz_posix","CET-1CEST,M3.5.0,M10.5.0/3");
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
  doc["eco_mode"]=preferences.getBool("eco_mode",false);
  doc["rot"]=preferences.getInt("rot",0);
  if(doc["rot"].is<int>()) preferences.putInt("rot",(int)doc["rot"]);
  preferences.end();
  doc["_version"]=1;doc["_device"]="cuboid";
  String out;serializeJsonPretty(doc,out);
  server.sendHeader("Content-Disposition","attachment; filename=\"cuboid-settings.json\"");
  server.sendHeader("Cache-Control","no-cache");
  server.send(200,"application/json",out);
}

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
  if(doc["tz_posix"].is<const char*>()) preferences.putString("tz_posix",(const char*)doc["tz_posix"]);
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
  if(doc["eco_mode"].is<bool>()) preferences.putBool("eco_mode",(bool)doc["eco_mode"]);
  preferences.end();
  server.send(200,"text/plain","OK");
  delay(400);ESP.restart();
}

void handleRoot(){
  noteWebActivity();
  applyPower(PWR_FETCH);
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
  char dd_ex[3]; snprintf(dd_ex,3,"%02d",day);
  int safeWday  = constrain(wdayBit, 0, 6);
  int safeMon   = constrain(month, 1, 12) - 1;
  String fmt3 = String(dayNames[currentLang][safeWday])+" "+dd_ex+" "+monthShort[currentLang][safeMon]+" "+String(year);
  String fmt4 = String(dayNames[currentLang][safeWday])+" "+dd_ex+" "+monthShort[currentLang][safeMon];
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
    sendStr(F("</div><input type='hidden' id='day_"));
    sendStr(String(i));
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
  sendStr(F("<label>Timezone</label><select name='tz_posix'>"));
  const char* tzOpts[][2]={
    {"NZST-12NZDT,M9.5.0,M4.1.0/3",    "UTC+12  Auckland · Wellington"},
    {"<+11>-11",                          "UTC+11  Solomon Is. · Noumea · Magadan"},
    {"AEST-10AEDT,M10.1.0,M4.1.0/3",    "UTC+10  Sydney · Melbourne  (Brisbane: no DST)"},
    {"ACST-9:30ACDT,M10.1.0,M4.1.0/3",  "UTC+9:30  Adelaide  (Darwin: no DST)"},
    {"JST-9",                             "UTC+9   Tokyo · Seoul · Yakutsk"},
    {"CST-8",                             "UTC+8   Beijing · Singapore · Perth · Manila"},
    {"<+07>-7",                           "UTC+7   Bangkok · Jakarta · Hanoi"},
    {"<+06>-6",                           "UTC+6   Dhaka · Almaty"},
    {"IST-5:30",                          "UTC+5:30  New Delhi · Mumbai · Colombo"},
    {"<+05>-5",                           "UTC+5   Karachi · Tashkent · Islamabad"},
    {"<+04>-4",                           "UTC+4   Dubai · Baku · Muscat"},
    {"<+0330>-3:30",                      "UTC+3:30  Tehran"},
    {"MSK-3",                             "UTC+3   Moscow · Istanbul · Nairobi · Riyadh"},
    {"EET-2EEST,M3.5.0/3,M10.5.0/4",    "UTC+2   Atene · Helsinki · Cairo · Bucarest"},
    {"CET-1CEST,M3.5.0,M10.5.0/3",      "UTC+1   Roma · Berlino · Parigi · Madrid"},
    {"GMT0BST,M3.5.0/1,M10.5.0",        "UTC+0 (DST)    Londra · Dublino"},
    {"UTC0",                              "UTC+0 (no DST) UTC · Reykjavik · Lisbona"},
    {"<-01>1",                            "UTC-1   Azzorre · Capo Verde"},
    {"<-02>2",                            "UTC-2   Georgia del Sud · Fernando de Noronha"},
    {"<-03>3",                            "UTC-3   Buenos Aires · Sao Paulo · Montevideo"},
    {"<-04>4",                            "UTC-4   Caracas · Halifax · La Paz · Manaus"},
    {"EST5EDT,M3.2.0,M11.1.0",           "UTC-5   New York · Toronto · Lima · Bogota"},
    {"CST6CDT,M3.2.0,M11.1.0",           "UTC-6   Chicago · Mexico City · Guatemala"},
    {"MST7MDT,M3.2.0,M11.1.0",           "UTC-7   Denver · Calgary  (Phoenix: no DST)"},
    {"PST8PDT,M3.2.0,M11.1.0",           "UTC-8   Los Angeles · Vancouver · Seattle"},
    {"AKST9AKDT,M3.2.0,M11.1.0",         "UTC-9   Alaska · Anchorage"},
    {"HST10",                             "UTC-10  Hawaii · Honolulu"},
    {"<-11>11",                           "UTC-11  Samoa · Midway"},
  };
  const int tzCount=(int)(sizeof(tzOpts)/sizeof(tzOpts[0]));
  bool tzFound=false;
  for(int i=0;i<tzCount;i++){
    sendStr(F("<option value='"));sendStr(tzOpts[i][0]);
    bool sel=(posixTz==tzOpts[i][0]);if(sel)tzFound=true;
    sendStr(sel?F("' selected>"):F("'>"));
    sendStr(tzOpts[i][1]);sendStr(F("</option>"));
  }
  if(!tzFound){
    sendStr(F("<option value='"));sendStr(posixTz);sendStr(F("' selected>Custom: "));sendStr(posixTz);sendStr(F("</option>"));
  }
  sendStr(F("<hr class='sep'><label>"));
  sendStr(labels[currentLang][L_ROTATION]);
  sendStr(F("</label><select name='rot'>"));
  {
    const char* rl[]={"Normal (0\xc2\xb0)","Right (90\xc2\xb0 CW)","Upside Down (180\xc2\xb0)","Left (270\xc2\xb0 CW)"};
    for(int i=0;i<4;i++){
      sendStr(F("<option value='"));sendStr(String(i));
      sendStr(displayRotation==i?F("' selected>"):F("'>"));
      sendStr(rl[i]);sendStr(F("</option>"));
    }
  }
  sendStr(F("</select>"));
  sendStr(F("</select><button class='save-btn' type='submit'>"));
  sendStr(labels[currentLang][L_SAVE]);
  sendStr(F("</button></form></div>"));
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
  sendP(PREVIEW_JS);
  sendStr(F("</div></body></html>"));
  server.sendContent("");
}

void handleAdvanced(){
  noteWebActivity();
  applyPower(PWR_FETCH);
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
  sendStr(F("<div class='card'><h2 style='color:var(--mute-txt)'>&#9888; Factory Reset</h2>"
    "<p style='font-size:13px;color:var(--muted);margin:0 0 12px;'>"
    "Erases <b>all</b> saved settings (WiFi, alarm, API keys, preferences) and reboots into setup mode. "
    "This cannot be undone.</p>"
    "<button id='frBtn1' class='save-btn' style='background:transparent;border:2px solid var(--mute-brd);color:var(--mute-txt);'"
    " onclick='frStep1()'>&#128465; Factory Reset&hellip;</button>"
    "<div id='frConfirm' style='display:none;margin-top:12px;padding:12px;background:var(--ibox);"
    "border-radius:9px;border:1.5px solid var(--mute-brd);'>"
    "<p style='font-size:13px;color:var(--mute-txt);font-weight:700;margin:0 0 10px;'>"
    "&#9888; Are you sure? All settings will be lost.</p>"
    "<div style='display:flex;gap:8px;'>"
    "<button class='save-btn' style='background:var(--mute-brd);flex:1;margin-top:0;' onclick='frConfirm()'>Yes, wipe everything</button>"
    "<button class='save-btn' style='background:var(--cbg);border:1.5px solid var(--brd);color:var(--txt);flex:1;margin-top:0;' onclick='frCancel()'>Cancel</button>"
    "</div></div></div>"));
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
function frStep1(){document.getElementById('frConfirm').style.display='block';document.getElementById('frBtn1').style.display='none';}
function frCancel(){document.getElementById('frConfirm').style.display='none';document.getElementById('frBtn1').style.display='block';}
async function frConfirm(){
  const btn=document.querySelector('#frConfirm button');btn.textContent='Wiping...';btn.disabled=true;
  try{await fetch('/factoryReset',{method:'POST'});}catch(_){}
  let s=5;const t=()=>{btn.textContent='Rebooting in '+s+'s\u2026';if(--s>0)setTimeout(t,1000);else location.reload();};t();
}
</script>
)rawjs"));
  sendP(PREVIEW_JS);
  sendStr(F("</div></body></html>"));
  server.sendContent("");
}

void handleFactoryReset(){
  noteWebActivity();
  server.send(200,"text/plain","OK");
  yield();
  preferences.begin("clock_cfg",false);preferences.clear();preferences.end();
  preferences.begin("wifi_boot",false);preferences.clear();preferences.end();
  delay(300);
  ESP.restart();
}
void handleSetBrightness(){
  noteWebActivity();
  int pb=server.arg("v").toInt();if(pb<1||pb>100)pb=100;
  pixelBrightness=pb;applyContrast(pixelBrightness);
  preferences.begin("clock_cfg",false);preferences.putInt("pct_bright",pixelBrightness);preferences.end();
  server.send(200,"text/plain","OK");
}
void handleToggleScreen(){
  noteWebActivity();
  if(isScreenOn){
    if(nmMode==2)wakeScreenUntil=0;
    else{
      animateScreenOff();display.ssd1306_command(SSD1306_DISPLAYOFF);u8g2.setPowerSave(1);
      isScreenOn=false;manualScreenOff=true;
    }
  } else {
    wakeScreenUntil=(nmWakeTime>0)?millis()+(nmWakeTime*1000UL):ULONG_MAX;
    manualScreenOff=false;
    display.ssd1306_command(SSD1306_DISPLAYON);u8g2.setPowerSave(0);isScreenOn=true;refreshDisplay();
  }
  server.send(200,"application/json",isScreenOn?"{\"on\":true}":"{\"on\":false}");
}
void handleToggleMute(){
  noteWebActivity();
#if ENABLE_BUZZER
  muteUI=!muteUI;preferences.begin("clock_cfg",false);preferences.putBool("mute",muteUI);preferences.end();
#endif
  server.sendHeader("Location","/");server.send(303);
}
void handleToggleTheme(){
  noteWebActivity();
  webThemeLight=!webThemeLight;preferences.begin("clock_cfg",false);preferences.putBool("wtheme",webThemeLight);preferences.end();
  server.send(200,"text/plain","OK");
}
void handleSaveTimeAlarm(){
  noteWebActivity();
  preferences.begin("clock_cfg",false);
#if ENABLE_BUZZER
  alarmHour=server.arg("h").toInt();alarmMinute=server.arg("m").toInt();
  alarmEnabled=server.arg("ena").toInt();alarmPatternIdx=server.arg("pat").toInt();
  alarmDays=0;for(int i=0;i<7;i++)if(server.arg("d"+String(i))=="1")alarmDays|=(1<<i);
  preferences.putInt("ah",alarmHour);preferences.putInt("am",alarmMinute);
  preferences.putBool("ae",alarmEnabled);preferences.putInt("ap",alarmPatternIdx);preferences.putInt("adays",alarmDays);
#endif
  posixTz=server.arg("tz_posix");
  preferences.putString("tz_posix",posixTz);preferences.end();
  configTzTime(posixTz.c_str(),"pool.ntp.org","time.nist.gov");
  server.send(200,"text/plain","OK");
  playSaveSound();
}
void handleSavePersonalization(){
  noteWebActivity();
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
  displayRotation=constrain(server.arg("rot").toInt(),0,3);
  applyDisplayRotation();
  preferences.putInt("rot",displayRotation);
  preferences.end();
  server.send(200,"text/plain","OK");
  playSaveSound();
  refreshDisplay();
}
void handleSaveAPI(){
  noteWebActivity();
  weatherKey=server.arg("w_key");weatherCity=server.arg("w_city");googleScriptUrl=server.arg("g_url");
  preferences.begin("clock_cfg",false);preferences.putString("key",weatherKey);preferences.putString("city",weatherCity);preferences.putString("g_url",googleScriptUrl);preferences.end();
  applyPower(PWR_FETCH);
  if(weatherKey!="")fetchWeather();
  if(googleScriptUrl!="")fetchCalendar();
  applyPower(PWR_WIFI_ACTIVE);
  if((currentMode==MODE_WEATHER||currentMode==MODE_FORECAST)&&weatherKey=="")currentMode=MODE_CLOCK;
  if(currentMode==MODE_CALENDAR&&googleScriptUrl=="")currentMode=MODE_CLOCK;
  server.send(200,"text/plain","OK");
  playSaveSound();
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
  noteWebActivity();
  preferences.begin("clock_cfg",false);
  nmMode=server.arg("nm_mode").toInt();if(nmMode<0||nmMode>2)nmMode=0;
  nmStartH=server.arg("nm_sh").toInt();nmStartM=server.arg("nm_sm").toInt();
  nmEndH=server.arg("nm_eh").toInt();nmEndM=server.arg("nm_em").toInt();
  nmWakeTime=server.arg("nm_wt").toInt();if(nmWakeTime<0)nmWakeTime=0;
  preferences.putInt("nm_mode",nmMode);preferences.putInt("nm_sh",nmStartH);preferences.putInt("nm_sm",nmStartM);
  preferences.putInt("nm_eh",nmEndH);preferences.putInt("nm_em",nmEndM);preferences.putInt("nm_wt",nmWakeTime);preferences.end();
  server.send(200,"text/plain","OK");
  playSaveSound();
}
void handleSaveAutoBright(){
  noteWebActivity();
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
  playSaveSoundBlocking();
}
void handleDisplayState(){
  noteWebActivity();
  JsonDocument doc;
  doc["mode"]          = (int)currentMode;
  doc["screenOn"]      = isScreenOn;
  doc["inSettings"]    = inSettingsMode;
  doc["settingsLevel"] = settingsLevel;
  doc["settingsIdx"]   = settingsIdx;
  doc["rotation"]      = displayRotation;
  doc["h"]          = hours;
  doc["m"]          = minutes;
  doc["s"]          = seconds;
  doc["day"]        = day;
  doc["month"]      = month;
  doc["year"]       = year;
  doc["wday"]       = wdayBit;
  doc["dateFormat"] = dateFormat;
  doc["lang"]       = currentLang;
  doc["alarmEnabled"] = alarmEnabled;
  doc["alarmH"]       = alarmHour;
  doc["alarmM"]       = alarmMinute;
  doc["alarmRinging"] = isAlarmRinging;
  doc["alarmDays"]    = alarmDays;
  doc["hasCal"]    = hasCalendarEvent;
  doc["cal_count"] = cal_eventCount;
  doc["cal_idx"]   = cal_currentEventIdx;
  if(cal_eventCount > 0){
    doc["cal_event"] = cal_events[cal_currentEventIdx];
    doc["cal_time"]  = localizeCalTime(cal_times[cal_currentEventIdx]);
  }
  doc["w_temp"] = w_temp;
  doc["w_hum"]  = w_hum;
  doc["w_desc"] = w_desc;
  doc["w_city"] = w_cityDisplay;
  doc["w_icon"] = w_iconCode;
  doc["w_fc_temp"] = w_forecast_temp;
  doc["w_fc_hum"]  = w_forecast_hum;
  doc["w_fc_desc"] = w_forecast_desc;
  doc["w_fc_icon"] = w_forecast_icon;
  doc["w_fc_min"]  = w_forecast_min;
  doc["w_fc_max"]  = w_forecast_max;
  doc["ecoSyncing"] = ecoSyncing;
  if(inSettingsMode){
    int total = smenuCount();
    int vis   = isPortrait() ? 8 : 4;
    int start = settingsIdx - (isPortrait() ? 3 : 1);
    if(start < 0)          start = 0;
    if(start > total - vis) start = total - vis;
    if(start < 0)          start = 0;
    doc["menuStart"] = start;
    doc["menuTotal"] = total;
    JsonArray items = doc["menuItems"].to<JsonArray>();
    JsonArray vals  = doc["menuVals"].to<JsonArray>();
    for(int i = 0; i < vis && (start + i) < total; i++){
      items.add(smenuName(start + i));
      vals.add(getSettingValue(settingsLevel, start + i));
    }
  }
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}
void handleBtnTap(){
  noteWebActivity();
  simBtnTap = true;
  server.send(200, "text/plain", "OK");
}
void handleBtnHold(){
  noteWebActivity();
  simBtnHold = true;
  server.send(200, "text/plain", "OK");
}
void handleBtnMed(){
  noteWebActivity();
  simBtnMed = true;
  server.send(200, "text/plain", "OK");
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

// ===================== RTOS TASKS =====================
void systemManagerTask(void *pvParameters) {
    SystemEvent ev;
    for (;;) {
        if (xQueueReceive(eventQueue, &ev, portMAX_DELAY) == pdTRUE) {
            switch (ev) {
                case EVENT_BUTTON_TAP:
                    handleButtonTap();
                    break;
                case EVENT_BUTTON_HOLD:
                    handleButtonHold();
                    break;
                case EVENT_BUTTON_MED:
                    handleButtonMed();
                    break;
                case EVENT_ALARM_TRIGGER:
                    startAlarmRoutine();
                    break;
                case EVENT_WEB_ACTIVITY:
                    // already handled by noteWebActivity, just keep power active
                    break;
                case EVENT_DATA_REFRESH:
                    if (WiFi.status() == WL_CONNECTED) {
                        if (weatherKey != "") fetchWeather();
                        if (googleScriptUrl != "") fetchCalendar();
                    }
                    break;
                case EVENT_WAKE_UP:
                    // risveglio da deep sleep: niente da fare qui
                    break;
                default:
                    break;
            }
        }
        // Ogni ciclo controlla se è il momento di entrare in deep sleep
        static unsigned long lastDeepSleepCheck = 0;
        if (millis() - lastDeepSleepCheck > 5000) {
            lastDeepSleepCheck = millis();
            goToDeepSleep();
        }
    }
}

void webServerTask(void *pvParameters) {
    for (;;) {
        server.handleClient();
        if(isAPMode) dnsServer.processNextRequest();
        ArduinoOTA.handle();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void displayTask(void *pvParameters) {
    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(1000));
        if (isAPMode) continue;
        if (currentPowerState != STATE_ACTIVE) continue;

        readLocalTime();

        // In eco mode con schermo spento: non fare nulla, lascia che
        // goToDeepSleep() nel systemManagerTask faccia il suo lavoro
        if (!isScreenOn) continue;

        applyAutoBrightness();
        refreshDisplay();

        // auto-clock return
        if (!inSettingsMode && autoClockReturnMin > 0
            && currentMode != MODE_CLOCK
            && (millis() - modeChangedAt) >= (unsigned long)autoClockReturnMin * 60000UL) {
            currentMode   = MODE_CLOCK;
            modeChangedAt = millis();
        }

        // scroll calendario
        if (!inSettingsMode && currentMode == MODE_CALENDAR
            && hasCalendarEvent && textWidth > 128) {
            if (++scrollX > textWidth + 20) scrollX = 0;
        }

        // Eco mode: spegni schermo dopo ECO_SCREEN_TIMEOUT
        if (ecoMode && isScreenOn
            && (millis() - lastUserInput) >= ECO_SCREEN_TIMEOUT) {
            if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                animateScreenOff();
                display.ssd1306_command(SSD1306_DISPLAYOFF);
                u8g2.setPowerSave(1);
                xSemaphoreGive(displayMutex);
            }
            isScreenOn = false;
            inSettingsMode = false;
            applyPower(PWR_DEEP);
        }
    }
}

void buttonTask(void *pvParameters) {
    buttonTaskHandle = xTaskGetCurrentTaskHandle();
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BOOT_BUTTON_PIN), buttonISR, FALLING);

    for (;;) {
        // blocca senza consumare CPU finché l'ISR non notifica
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // debounce HW
        vTaskDelay(pdMS_TO_TICKS(15));
        if (digitalRead(BOOT_BUTTON_PIN) != LOW) continue;

        unsigned long pressStart = millis();
        // aggiorna lastUserInput ad ogni pressione rilevata
        lastUserInput = millis();
        bool actionTaken = false;

        while (digitalRead(BOOT_BUTTON_PIN) == LOW) {
            unsigned long held = millis() - pressStart;

            if (!actionTaken && inSettingsMode && held >= 500 && held < 700) {
                actionTaken = true;
                SystemEvent ev = EVENT_BUTTON_HOLD;
                xQueueSend(eventQueue, &ev, 0);
            }
            if (!actionTaken && !inSettingsMode
                && held >= SETTINGS_HOLD_MS && held < SETTINGS_HOLD_MS + 400) {
                actionTaken = true;
                SystemEvent ev = EVENT_BUTTON_HOLD;
                xQueueSend(eventQueue, &ev, 0);
            }
            if (!actionTaken && !inSettingsMode && held >= 300 && held < 700) {
                bool medApplies = (currentMode == MODE_WEATHER)
                    || (currentMode == MODE_CALENDAR && hasCalendarEvent && cal_eventCount > 1);
                if (medApplies) {
                    actionTaken = true;
                    SystemEvent ev = EVENT_BUTTON_MED;
                    xQueueSend(eventQueue, &ev, 0);
                }
            }
            if (!actionTaken && held >= 10000) {
                actionTaken = true;
                preferences.begin("clock_cfg", false);
                preferences.clear();
                preferences.end();
                ESP.restart();
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        if (!actionTaken && (millis() - pressStart) < 300) {
            // al rilascio da deep sleep: pulsante fisico ha già svegliato la CPU,
            // il tap è implicito — accendiamo schermo se spento
            if (!isScreenOn) {
                isScreenOn = true;
                manualScreenOff = false;
                display.ssd1306_command(SSD1306_DISPLAYON);
                u8g2.setPowerSave(0);
                lastWebActivity = millis(); // riusa come "last user input"
                refreshDisplay();
            } else {
                SystemEvent ev = EVENT_BUTTON_TAP;
                xQueueSend(eventQueue, &ev, 0);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void dataFetchTask(void *pvParameters) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(DATA_REFRESH_INTERVAL));
        SystemEvent ev = EVENT_DATA_REFRESH;
        xQueueSend(eventQueue, &ev, 0);
    }
}

void alarmMonitorTask(void *pvParameters) {
    // Controlla subito se c'era un allarme pendente dal wake-up
    if (rtc_alarm_pending) {
        rtc_alarm_pending = false;
        rtc_lastAlarmFiredDay = day;
        SystemEvent ev = EVENT_ALARM_TRIGGER;
        xQueueSend(eventQueue, &ev, 0);
    }

    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(1000));
        if (!alarmEnabled || isAlarmRinging) continue;
        if (rtc_lastAlarmFiredDay == day) continue;
        if (hours == alarmHour && minutes == alarmMinute && isAlarmActiveToday()) {
            rtc_lastAlarmFiredDay = day;  // aggiorna il flag RTC, non solo lastAlarmFiredDay
            lastAlarmFiredDay = day;
            SystemEvent ev = EVENT_ALARM_TRIGGER;
            xQueueSend(eventQueue, &ev, 0);
        }
    }
}

// ===================== SETUP =====================
void setup(){
  eventQueue   = xQueueCreate(20, sizeof(SystemEvent));
  displayMutex = xSemaphoreCreateMutex();
  wifiMutex    = xSemaphoreCreateMutex();
  nvs_flash_init();Wire.begin(OLED_SDA,OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC,0x3C);
  display.clearDisplay();display.display();
  u8g2.begin();
  pinMode(BOOT_BUTTON_PIN,INPUT_PULLUP);
#if ENABLE_BUZZER
  buzzer.begin();
#endif
  initPowerManagement();

  esp_sleep_wakeup_cause_t wakeReason = esp_sleep_get_wakeup_cause();
  bool isDeepSleepWake = (wakeReason != ESP_SLEEP_WAKEUP_UNDEFINED);

  if (isDeepSleepWake) {
    updateTimeFromRTC();
    readLocalTime(); // popola hours/minutes/day/ecc. subito

    bool wakeFromButton = (wakeReason == ESP_SLEEP_WAKEUP_EXT1);
    bool wakeFromTimer  = (wakeReason == ESP_SLEEP_WAKEUP_TIMER);

    // Carica il minimo indispensabile per decidere cosa fare
    preferences.begin("clock_cfg", true);
    alarmEnabled  = preferences.getBool("ae", false);
    alarmHour     = preferences.getInt("ah", 7);
    alarmMinute   = preferences.getInt("am", 0);
    alarmDays     = preferences.getInt("adays", 0b1111111);
    posixTz       = preferences.getString("tz_posix", "CET-1CEST,M3.5.0,M10.5.0/3");
    ecoMode       = preferences.getBool("eco_mode", false);
    preferences.end();

    setenv("TZ", posixTz.c_str(), 1);
    tzset();
    readLocalTime(); // rileggi con TZ corretta

    bool alarmDue = alarmEnabled
                    && isAlarmActiveToday()
                    && (hours == alarmHour)
                    && (minutes == alarmMinute)
                    && (rtc_lastAlarmFiredDay != day);

    if (wakeFromTimer && !alarmDue) {
        // Solo tick RTC: aggiorna l'ora e torna in sleep senza accendere niente
        saveCurrentTimeToRTC();

        if (ecoMode) {
            uint64_t sleep_us = 60ULL * 1000000ULL;
            if (alarmEnabled) {
                time_t now; time(&now);
                struct tm tm_alarm = *localtime(&now);
                tm_alarm.tm_hour = alarmHour;
                tm_alarm.tm_min  = alarmMinute;
                tm_alarm.tm_sec  = 0;
                time_t alarm_ts = mktime(&tm_alarm);
                if (alarm_ts <= now) alarm_ts += 86400;
                uint64_t to_alarm = (alarm_ts - now) * 1000000ULL;
                if (to_alarm < sleep_us) sleep_us = to_alarm;
            }
            esp_sleep_enable_timer_wakeup(sleep_us);
            esp_sleep_enable_ext1_wakeup(1ULL << GPIO_NUM_9, ESP_EXT1_WAKEUP_ANY_LOW);
            esp_deep_sleep_start(); // non ritorna
        }
        // non eco mode: schermo spento, boot normale senza WiFi
        isScreenOn = false;

      }   else if (alarmDue) {
        // Segna l'allarme come pendente PRIMA del boot completo
        // così non dipende dal timing dell'alarmMonitorTask
            rtc_alarm_pending = true;
        }

    // Wake da pulsante o da sveglia: accendi display
    if (wakeFromButton || alarmDue) {
        display.ssd1306_command(SSD1306_DISPLAYON);
        u8g2.setPowerSave(0);
        isScreenOn = true;
        lastUserInput = millis();
      }
  }

  currentPowerState = STATE_ACTIVE;

  preferences.begin("clock_cfg",true);
  displayRotation = preferences.getInt("rot", 0);
  nmStartH=preferences.getInt("nm_sh",23);nmStartM=preferences.getInt("nm_sm",0);
  nmEndH=preferences.getInt("nm_eh",7);nmEndM=preferences.getInt("nm_em",0);nmWakeTime=preferences.getInt("nm_wt",5);
  nmMode=preferences.isKey("nm_mode")?preferences.getInt("nm_mode",0):(preferences.getBool("nm_en",false)?1:0);
  savedSSID=preferences.getString("ssid","");savedPass=preferences.getString("pass","");
  weatherKey=preferences.getString("key","");weatherCity=preferences.getString("city","");googleScriptUrl=preferences.getString("g_url","");
  currentLang=preferences.getInt("lng",0);posixTz=preferences.getString("tz_posix","CET-1CEST,M3.5.0,M10.5.0/3");
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
  ecoMode=preferences.getBool("eco_mode",false);
  preferences.end();
  applyDisplayRotation();
  applyContrast(pixelBrightness);
  if(forceAP){preferences.begin("clock_cfg",false);preferences.putBool("force_ap",false);preferences.end();}
  if(clockFont<0||clockFont>2)clockFont=0;
  modeChangedAt=millis();
  if(!forceAP&&savedSSID!=""&&savedSSID!="NULL"){
    preferences.begin("wifi_boot",false);int wifiAttempt=preferences.getInt("attempt",0);preferences.end();
    WiFi.persistent(false);WiFi.setAutoReconnect(false);WiFi.mode(WIFI_STA);
    applyPower(PWR_FETCH);
    WiFi.begin(savedSSID.c_str(),savedPass.c_str());
    const unsigned long WTO=5000UL;unsigned long ws=millis();
    while(WiFi.status()!=WL_CONNECTED&&millis()-ws<WTO){
      int bw=(int)(((millis()-ws)*108UL)/WTO);
      u8g2.clearBuffer();u8g2.setFont(u8g2_font_6x10_tr);
      u8g2.drawStr(10,22,labels[currentLang][L_SYNC]);u8g2.drawStr(10,36,savedSSID.c_str());
      u8g2.drawFrame(10,44,108,7);if(bw>0)u8g2.drawBox(10,44,bw,7);
      char tb[20];snprintf(tb,sizeof(tb),"Try %d/3",wifiAttempt+1);u8g2.drawStr(10,62,tb);u8g2.sendBuffer();delay(200);
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
        char rb[24];snprintf(rb,sizeof(rb),"(%d/3 failed)",wifiAttempt);u8g2.drawStr(10,50,rb);u8g2.sendBuffer();delay(1000);ESP.restart();
      } else {
        preferences.begin("wifi_boot",false);preferences.putInt("attempt",0);preferences.end();
        WiFi.disconnect();WiFi.mode(WIFI_AP);WiFi.softAP("ESP32-Cuboid-Fallback");isAPMode=true;
        esp_wifi_set_ps(WIFI_PS_NONE);
      }
    }
  } else {
    WiFi.mode(WIFI_AP);WiFi.softAP(forceAP?"ESP32-Cuboid-AP":"ESP32-Cuboid");isAPMode=true;
    esp_wifi_set_ps(WIFI_PS_NONE);
  }

  ArduinoOTA.setHostname(HOSTNAME);
  if(otaPass.length()>0) ArduinoOTA.setPassword(otaPass.c_str());
  ArduinoOTA.onStart([](){ applyPower(PWR_OTA); });
  ArduinoOTA.onEnd([](){ _pwrCur=(PowerMode)255; });
  ArduinoOTA.onError([](ota_error_t){ _pwrCur=(PowerMode)255; });
  ArduinoOTA.begin();

  if(isAPMode){
    dnsServer.start(53,"*",WiFi.softAPIP());
    u8g2.clearBuffer();u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(5,12,labels[currentLang][L_SETUP]);u8g2.drawStr(5,30,labels[currentLang][L_WIFI]);
    u8g2.drawStr(35,30,"ESP32-Cuboid");u8g2.drawStr(5,45,"IP:");u8g2.drawStr(25,45,"192.168.4.1");
    u8g2.drawStr(5,60,"http://cuboid.local");u8g2.sendBuffer();
  } else {
    configTzTime(posixTz.c_str(),"pool.ntp.org","time.nist.gov");
    applyPower(PWR_FETCH);
    delay(1500); lastNtpCheck=millis();
    if(weatherKey!=""){fetchWeather();lastWeatherUpdate=millis();}
    if(googleScriptUrl!=""){fetchCalendar();lastCalendarUpdate=millis();}
    if(ecoMode){
      WiFi.disconnect(false);
      applyPower(PWR_DISPLAY);
    } else {
      applyPower(PWR_WIFI_IDLE);
    }
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
  server.on("/factoryReset",HTTP_POST,handleFactoryReset);
  server.on("/exportSettings",HTTP_GET,handleExportSettings);
  server.on("/importSettings",HTTP_POST,handleImportSettings);
  server.on("/displayState", HTTP_GET,  handleDisplayState);
  server.on("/btnTap",       HTTP_POST, handleBtnTap);
  server.on("/btnHold",      HTTP_POST, handleBtnHold);
  server.on("/btnMed",       HTTP_POST, handleBtnMed);
  server.on("/update",HTTP_POST,
    [](){ server.sendHeader("Connection","close"); server.send(200,"text/plain",Update.hasError()?"FAIL":"OK"); ESP.restart(); },
    [](){ HTTPUpload&u=server.upload(); if(u.status==UPLOAD_FILE_START)Update.begin(UPDATE_SIZE_UNKNOWN); else if(u.status==UPLOAD_FILE_WRITE)Update.write(u.buf,u.currentSize); else if(u.status==UPLOAD_FILE_END)Update.end(true); }
  );
  const char* hdrs[]={"Referer"};server.collectHeaders(hdrs,1);
  server.begin();

  // Crea i task (tutti sul core 0, HP)
  xTaskCreatePinnedToCore(systemManagerTask, "SysMgr", 8192, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(webServerTask,    "WebSrv", 8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(displayTask,      "Display", 4096, NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(buttonTask,       "Button",  2048, NULL, 4, NULL, 0);
  xTaskCreatePinnedToCore(dataFetchTask,    "DataFetch", 8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(alarmMonitorTask, "Alarm",   2048, NULL, 2, NULL, 0);

  // Elimina il task loop() se esiste
  vTaskDelete(NULL);
}

void loop() {
  // Non usato: tutto è gestito dai task RTOS
}