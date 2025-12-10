/*
 * GROWTRON - Sistema de Automação de Cultivo
 * Versão Corrigida para ESP32 Core 3.0+
 */

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <Update.h>
#include <FS.h>
#include <LittleFS.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --------------------------------------------------------------------------
// 1. CONFIGURAÇÃO DE HARDWARE E PINOS
// --------------------------------------------------------------------------

// I2C Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32 
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Sensores
#define PIN_DHT 4
#define PIN_LDR 32
// Sensores de Solo (ADC1)
const int PIN_SOLO[4] = {36, 39, 34, 35}; 

// Relés Luzes
// Grow A: Normal(0), Emerson(1) | Grow B: Normal(2), Emerson(3)
const int PIN_RELE_LUZ[4] = {23, 5, 18, 19}; 

// Relés Rega (Bombas)
// Vaso 1(0), Vaso 2(1), Vaso 3(2), Vaso 4(3)
const int PIN_RELE_REGA[4] = {25, 26, 27, 14};

// Botão Reset (GPIO 0 é o botão BOOT na maioria das placas)
#define PIN_FACTORY_RESET 0

// --------------------------------------------------------------------------
// 2. ESTRUTURAS DE DADOS
// --------------------------------------------------------------------------

struct ConfigVaso {
  bool ativo;
  char nome[32];
  int limiteUmidadeBaixo;
  int limiteUmidadeAlto;
  // Rega Auto
  bool regaAutomatica;
  int horaRega1, minutoRega1;
  int horaRega2, minutoRega2;
  bool usarRega2;
  // Modo
  bool regaPulsativa;
  int pulsosRega;
  int tempoPulsoOn;
  int tempoPulsoOff;
  int tempoRegaContinua;
  // Partição (0=A, 1=B)
  int particao; 
};

struct EstadoVaso {
  int umidadeADC;
  int umidadePercent;
  bool regando;
  unsigned long inicioRega; // millis
  int pulsoAtual;
  bool pulsoLigado;
  unsigned long ultimoPulsoChange; // millis
  bool regouHoje1;
  bool regouHoje2;
  int diaUltimaRega;
};

struct ConfigLuzGrow {
  int horaLigar, minutoLigar;
  int horaDesligar, minutoDesligar;
  int deslocamentoEmerson; // minutos
};

struct ConfigGlobal {
  char userPass[32];
  int numParticoes; // 1 ou 2
  int dhtType; // 11 ou 22
  bool thingSpeakAtivo;
  char thingSpeakApiKey[20];
  char thingSpeakChannel[15];
  char camIP[20];
  char camToken[20];
  char wifiSSID[32];
  char wifiPass[32];
};

// Calibração de Solo
int soloSecoADC[4] = {3000, 3000, 3000, 3000};    
int soloMolhadoADC[4] = {1200, 1200, 1200, 1200}; 

// Instâncias Globais
ConfigVaso cfVaso[4];
EstadoVaso stVaso[4];
ConfigLuzGrow cfLuz[2]; // 0=A, 1=B
ConfigGlobal cfGlobal;

// Variáveis de Runtime
float tempAtual = 0.0;
float humAtual = 0.0;
int luxADC = 0;
bool luzNormalState[2] = {false, false}; // A, B
bool luzEmersonState[2] = {false, false}; // A, B
bool apMode = false; // Indica se estamos em modo Access Point

// Timers
unsigned long lastSensorRead = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastThingSpeak = 0;
unsigned long lastRegaCheck = 0;
int displayPage = 0;

// Objetos
AsyncWebServer server(80);
Preferences preferences;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "a.st1.ntp.br", -3 * 3600, 60000); // GMT-3 Brasil
DHT* dht = nullptr; 

// --------------------------------------------------------------------------
// 3. FUNÇÕES AUXILIARES
// --------------------------------------------------------------------------

void logMessage(String tipo, String msg) {
  File file = LittleFS.open("/logs.txt", "a");
  if (file) {
    if (file.size() > 50000) { 
      file.close();
      LittleFS.remove("/logs.txt");
      file = LittleFS.open("/logs.txt", "w");
    }
    String timeStr = timeClient.getFormattedTime();
    file.printf("[%s] [%s] %s\n", timeStr.c_str(), tipo.c_str(), msg.c_str());
    file.close();
  }
  Serial.printf("[%s] %s\n", tipo.c_str(), msg.c_str());
}

void loadSettings() {
  preferences.begin("growtron", true); // Read-only

  // Global
  String pass = preferences.getString("pass", "1234");
  strlcpy(cfGlobal.userPass, pass.c_str(), sizeof(cfGlobal.userPass));
  cfGlobal.numParticoes = preferences.getInt("nPart", 2);
  cfGlobal.dhtType = preferences.getInt("dhtType", 11); 
  cfGlobal.thingSpeakAtivo = preferences.getBool("tsActive", false);
  
  String key = preferences.getString("tsKey", "");
  strlcpy(cfGlobal.thingSpeakApiKey, key.c_str(), sizeof(cfGlobal.thingSpeakApiKey));
  String chan = preferences.getString("tsChan", "");
  strlcpy(cfGlobal.thingSpeakChannel, chan.c_str(), sizeof(cfGlobal.thingSpeakChannel));
  String cip = preferences.getString("camIP", "");
  strlcpy(cfGlobal.camIP, cip.c_str(), sizeof(cfGlobal.camIP));
  String ctok = preferences.getString("camTok", "");
  strlcpy(cfGlobal.camToken, ctok.c_str(), sizeof(cfGlobal.camToken));
  
  // WiFi Salvo
  String wssid = preferences.getString("ssid", "");
  strlcpy(cfGlobal.wifiSSID, wssid.c_str(), sizeof(cfGlobal.wifiSSID));
  String wpass = preferences.getString("wpass", "");
  strlcpy(cfGlobal.wifiPass, wpass.c_str(), sizeof(cfGlobal.wifiPass));

  // Luzes
  for(int i=0; i<2; i++) {
    char k[10];
    sprintf(k, "l%d_hOn", i); cfLuz[i].horaLigar = preferences.getInt(k, 6);
    sprintf(k, "l%d_mOn", i); cfLuz[i].minutoLigar = preferences.getInt(k, 0);
    sprintf(k, "l%d_hOff", i); cfLuz[i].horaDesligar = preferences.getInt(k, 22);
    sprintf(k, "l%d_mOff", i); cfLuz[i].minutoDesligar = preferences.getInt(k, 0);
    sprintf(k, "l%d_emer", i); cfLuz[i].deslocamentoEmerson = preferences.getInt(k, 15);
  }

  // Vasos
  for(int i=0; i<4; i++) {
    char prefix[10]; sprintf(prefix, "v%d", i);
    char k[16];
    
    sprintf(k, "%s_atv", prefix); cfVaso[i].ativo = preferences.getBool(k, true);
    sprintf(k, "%s_nom", prefix); 
    String n = preferences.getString(k, ("Vaso " + String(i+1)).c_str());
    strlcpy(cfVaso[i].nome, n.c_str(), sizeof(cfVaso[i].nome));
    
    sprintf(k, "%s_limL", prefix); cfVaso[i].limiteUmidadeBaixo = preferences.getInt(k, 30);
    sprintf(k, "%s_limH", prefix); cfVaso[i].limiteUmidadeAlto = preferences.getInt(k, 80);
    
    sprintf(k, "%s_auto", prefix); cfVaso[i].regaAutomatica = preferences.getBool(k, false);
    sprintf(k, "%s_h1", prefix); cfVaso[i].horaRega1 = preferences.getInt(k, 8);
    sprintf(k, "%s_m1", prefix); cfVaso[i].minutoRega1 = preferences.getInt(k, 0);
    sprintf(k, "%s_h2", prefix); cfVaso[i].horaRega2 = preferences.getInt(k, 18);
    sprintf(k, "%s_m2", prefix); cfVaso[i].minutoRega2 = preferences.getInt(k, 0);
    sprintf(k, "%s_u2", prefix); cfVaso[i].usarRega2 = preferences.getBool(k, false);
    
    sprintf(k, "%s_puls", prefix); cfVaso[i].regaPulsativa = preferences.getBool(k, false);
    sprintf(k, "%s_npul", prefix); cfVaso[i].pulsosRega = preferences.getInt(k, 3);
    sprintf(k, "%s_ton", prefix); cfVaso[i].tempoPulsoOn = preferences.getInt(k, 5);
    sprintf(k, "%s_toff", prefix); cfVaso[i].tempoPulsoOff = preferences.getInt(k, 10);
    sprintf(k, "%s_tcont", prefix); cfVaso[i].tempoRegaContinua = preferences.getInt(k, 10);
    
    sprintf(k, "%s_part", prefix); cfVaso[i].particao = preferences.getInt(k, (i < 2 ? 0 : 1));

    // Calibração
    sprintf(k, "cal_s%d", i); soloSecoADC[i] = preferences.getInt(k, 3000);
    sprintf(k, "cal_m%d", i); soloMolhadoADC[i] = preferences.getInt(k, 1200);
  }

  preferences.end();
}

void saveVasoConfig(int i) {
  preferences.begin("growtron", false);
  char prefix[10]; sprintf(prefix, "v%d", i);
  char k[16];
  sprintf(k, "%s_atv", prefix); preferences.putBool(k, cfVaso[i].ativo);
  sprintf(k, "%s_nom", prefix); preferences.putString(k, cfVaso[i].nome);
  sprintf(k, "%s_limL", prefix); preferences.putInt(k, cfVaso[i].limiteUmidadeBaixo);
  sprintf(k, "%s_limH", prefix); preferences.putInt(k, cfVaso[i].limiteUmidadeAlto);
  sprintf(k, "%s_auto", prefix); preferences.putBool(k, cfVaso[i].regaAutomatica);
  sprintf(k, "%s_h1", prefix); preferences.putInt(k, cfVaso[i].horaRega1);
  sprintf(k, "%s_m1", prefix); preferences.putInt(k, cfVaso[i].minutoRega1);
  sprintf(k, "%s_h2", prefix); preferences.putInt(k, cfVaso[i].horaRega2);
  sprintf(k, "%s_m2", prefix); preferences.putInt(k, cfVaso[i].minutoRega2);
  sprintf(k, "%s_u2", prefix); preferences.putBool(k, cfVaso[i].usarRega2);
  sprintf(k, "%s_puls", prefix); preferences.putBool(k, cfVaso[i].regaPulsativa);
  sprintf(k, "%s_npul", prefix); preferences.putInt(k, cfVaso[i].pulsosRega);
  sprintf(k, "%s_ton", prefix); preferences.putInt(k, cfVaso[i].tempoPulsoOn);
  sprintf(k, "%s_toff", prefix); preferences.putInt(k, cfVaso[i].tempoPulsoOff);
  sprintf(k, "%s_tcont", prefix); preferences.putInt(k, cfVaso[i].tempoRegaContinua);
  sprintf(k, "%s_part", prefix); preferences.putInt(k, cfVaso[i].particao);
  preferences.end();
}

void saveGlobalConfig() {
  preferences.begin("growtron", false);
  preferences.putString("pass", cfGlobal.userPass);
  preferences.putInt("nPart", cfGlobal.numParticoes);
  preferences.putInt("dhtType", cfGlobal.dhtType);
  preferences.putBool("tsActive", cfGlobal.thingSpeakAtivo);
  preferences.putString("tsKey", cfGlobal.thingSpeakApiKey);
  preferences.putString("tsChan", cfGlobal.thingSpeakChannel);
  preferences.putString("camIP", cfGlobal.camIP);
  preferences.putString("camTok", cfGlobal.camToken);
  
  // Salvar WiFi se foi alterado
  if(strlen(cfGlobal.wifiSSID) > 0) {
      preferences.putString("ssid", cfGlobal.wifiSSID);
      preferences.putString("wpass", cfGlobal.wifiPass);
  }
  
  for(int i=0; i<2; i++) {
    char k[10];
    sprintf(k, "l%d_hOn", i); preferences.putInt(k, cfLuz[i].horaLigar);
    sprintf(k, "l%d_mOn", i); preferences.putInt(k, cfLuz[i].minutoLigar);
    sprintf(k, "l%d_hOff", i); preferences.putInt(k, cfLuz[i].horaDesligar);
    sprintf(k, "l%d_mOff", i); preferences.putInt(k, cfLuz[i].minutoDesligar);
    sprintf(k, "l%d_emer", i); preferences.putInt(k, cfLuz[i].deslocamentoEmerson);
  }
  preferences.end();
}

void saveCalib(int i) {
  preferences.begin("growtron", false);
  char k[10];
  sprintf(k, "cal_s%d", i); preferences.putInt(k, soloSecoADC[i]);
  sprintf(k, "cal_m%d", i); preferences.putInt(k, soloMolhadoADC[i]);
  preferences.end();
}

// --------------------------------------------------------------------------
// 4. LÓGICA DE CONTROLE
// --------------------------------------------------------------------------

bool isTimeInWindow(int nowMins, int startMins, int endMins) {
  if (startMins < endMins) {
    return (nowMins >= startMins && nowMins < endMins);
  } else {
    return (nowMins >= startMins || nowMins < endMins);
  }
}

void checkLights() {
  int currentMins = timeClient.getHours() * 60 + timeClient.getMinutes();

  for (int i = 0; i < 2; i++) { 
    int startNorm = cfLuz[i].horaLigar * 60 + cfLuz[i].minutoLigar;
    int endNorm = cfLuz[i].horaDesligar * 60 + cfLuz[i].minutoDesligar;
    
    bool normalOn = isTimeInWindow(currentMins, startNorm, endNorm);
    
    int startEmer = startNorm - cfLuz[i].deslocamentoEmerson;
    int endEmer = endNorm + cfLuz[i].deslocamentoEmerson;
    
    if (startEmer < 0) startEmer += 1440;
    if (endEmer >= 1440) endEmer -= 1440;
    
    bool emersonOn = isTimeInWindow(currentMins, startEmer, endEmer);
    
    luzNormalState[i] = normalOn;
    luzEmersonState[i] = emersonOn;

    digitalWrite(PIN_RELE_LUZ[i*2], normalOn ? LOW : HIGH); 
    digitalWrite(PIN_RELE_LUZ[i*2 + 1], emersonOn ? LOW : HIGH);
  }
}

void checkSensors() {
  if (dht) {
    float t = dht->readTemperature();
    float h = dht->readHumidity();
    if (!isnan(t)) tempAtual = t;
    if (!isnan(h)) humAtual = h;
  }
  
  luxADC = analogRead(PIN_LDR);

  for(int i=0; i<4; i++) {
    int raw = analogRead(PIN_SOLO[i]);
    stVaso[i].umidadeADC = raw;
    int pct = map(raw, soloSecoADC[i], soloMolhadoADC[i], 0, 100);
    stVaso[i].umidadePercent = constrain(pct, 0, 100);
  }
}

void startRega(int i) {
  if (!cfVaso[i].ativo) return;
  if (stVaso[i].regando) return; 

  stVaso[i].regando = true;
  stVaso[i].inicioRega = millis();
  stVaso[i].pulsoAtual = 0;
  stVaso[i].pulsoLigado = true; 
  stVaso[i].ultimoPulsoChange = millis();
  
  digitalWrite(PIN_RELE_REGA[i], LOW); 
  logMessage("REGA", String("Iniciada Vaso ") + String(i+1));
}

void stopRega(int i) {
  stVaso[i].regando = false;
  digitalWrite(PIN_RELE_REGA[i], HIGH); 
  logMessage("REGA", String("Parada Vaso ") + String(i+1));
}

void controlWatering() {
  unsigned long now = millis();

  int hoje = timeClient.getDay();
  for(int i=0; i<4; i++) {
    if (stVaso[i].diaUltimaRega != hoje) {
      stVaso[i].regouHoje1 = false;
      stVaso[i].regouHoje2 = false;
      stVaso[i].diaUltimaRega = hoje;
    }
  }

  for(int i=0; i<4; i++) {
    if (stVaso[i].regando) {
      if (!cfVaso[i].regaPulsativa) {
        if (now - stVaso[i].inicioRega >= cfVaso[i].tempoRegaContinua * 1000UL) {
          stopRega(i);
        }
      } else {
        unsigned long duracaoFase = stVaso[i].pulsoLigado ? (cfVaso[i].tempoPulsoOn * 1000UL) : (cfVaso[i].tempoPulsoOff * 1000UL);
        
        if (now - stVaso[i].ultimoPulsoChange >= duracaoFase) {
          if (stVaso[i].pulsoLigado) {
             digitalWrite(PIN_RELE_REGA[i], HIGH);
             stVaso[i].pulsoLigado = false;
             stVaso[i].pulsoAtual++; 
          } else {
             if (stVaso[i].pulsoAtual >= cfVaso[i].pulsosRega) {
               stopRega(i); 
               continue;
             }
             digitalWrite(PIN_RELE_REGA[i], LOW);
             stVaso[i].pulsoLigado = true;
          }
          stVaso[i].ultimoPulsoChange = now;
        }
      }
    }

    if (!stVaso[i].regando && cfVaso[i].regaAutomatica && cfVaso[i].ativo) {
      int h = timeClient.getHours();
      int m = timeClient.getMinutes();
      
      bool trigger1 = (h == cfVaso[i].horaRega1 && m == cfVaso[i].minutoRega1 && !stVaso[i].regouHoje1);
      bool trigger2 = (cfVaso[i].usarRega2 && h == cfVaso[i].horaRega2 && m == cfVaso[i].minutoRega2 && !stVaso[i].regouHoje2);

      if (trigger1 || trigger2) {
        if (stVaso[i].umidadePercent < cfVaso[i].limiteUmidadeBaixo) {
          startRega(i);
          if (trigger1) stVaso[i].regouHoje1 = true;
          if (trigger2) stVaso[i].regouHoje2 = true;
        } else {
           if (trigger1) stVaso[i].regouHoje1 = true;
           if (trigger2) stVaso[i].regouHoje2 = true;
        }
      }
    }
  }
}

void sendThingSpeak() {
  if (!cfGlobal.thingSpeakAtivo || apMode) return;
  
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.thingspeak.com/update?api_key=" + String(cfGlobal.thingSpeakApiKey);
    url += "&field1=" + String(tempAtual);
    url += "&field2=" + String(humAtual);
    url += "&field3=" + String(luxADC);
    url += "&field4=" + String(stVaso[0].umidadePercent);
    url += "&field5=" + String(stVaso[1].umidadePercent);
    url += "&field6=" + String(stVaso[2].umidadePercent);
    url += "&field7=" + String(stVaso[3].umidadePercent);
    int statusLuz = (luzNormalState[0]?1:0) + (luzEmersonState[0]?2:0) + (luzNormalState[1]?4:0) + (luzEmersonState[1]?8:0);
    url += "&field8=" + String(statusLuz);

    http.begin(url);
    int httpCode = http.GET();
    http.end();
  }
}

// --------------------------------------------------------------------------
// 5. INTERFACE DO DISPLAY
// --------------------------------------------------------------------------

void updateOLED() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);

  if(apMode) {
      display.println("AP MODE: GrowtronAP");
      display.printf("IP: %s", WiFi.softAPIP().toString().c_str());
      display.display();
      return;
  }

  display.printf("%02d:%02d WiFi:%s\n", timeClient.getHours(), timeClient.getMinutes(), (WiFi.status()==WL_CONNECTED?"OK":"X"));
  display.drawLine(0, 8, 128, 8, SSD1306_WHITE);

  switch(displayPage) {
    case 0: 
      display.setCursor(0, 10);
      display.println("GROWTRON V1.1");
      display.printf("IP: %s", WiFi.localIP().toString().c_str());
      break;
    case 1: 
      display.setCursor(0, 10);
      display.printf("T: %.1fC  H: %.1f%%\n", tempAtual, humAtual);
      display.printf("Lux ADC: %d", luxADC);
      break;
    case 2: 
      display.setCursor(0, 10);
      display.printf("A N:%d E:%d\n", luzNormalState[0], luzEmersonState[0]);
      display.printf("B N:%d E:%d", luzNormalState[1], luzEmersonState[1]);
      break;
    case 3: 
      display.setCursor(0, 10);
      display.printf("V1:%d V2:%d\n", stVaso[0].regando, stVaso[1].regando);
      display.printf("V3:%d V4:%d", stVaso[2].regando, stVaso[3].regando);
      break;
    case 4: 
      display.setCursor(0, 10);
      display.printf("V1: %d%% (%d)\n", stVaso[0].umidadePercent, stVaso[0].umidadeADC);
      display.printf("V2: %d%% (%d)", stVaso[1].umidadePercent, stVaso[1].umidadeADC);
      break;
    case 5: 
      display.setCursor(0, 10);
      display.printf("V3: %d%% (%d)\n", stVaso[2].umidadePercent, stVaso[2].umidadeADC);
      display.printf("V4: %d%% (%d)", stVaso[3].umidadePercent, stVaso[3].umidadeADC);
      break;
  }
  
  display.display();
  displayPage++;
  if (displayPage > 5) displayPage = 0;
}

// --------------------------------------------------------------------------
// 6. SERVIDOR WEB E HTML
// --------------------------------------------------------------------------

bool checkAuth(AsyncWebServerRequest *request) {
  if (request->hasHeader("Cookie")) {
    String cookie = request->header("Cookie");
    if (cookie.indexOf("ESPSESSIONID=1") != -1) return true;
  }
  return false;
}

String getHead() {
  return R"raw(
<!DOCTYPE html>
<html lang="pt-br">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Growtron Monitor</title>
  <style>
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: linear-gradient(135deg, #0f0f1a, #1a1a2e); color: #e0e0e0; margin: 0; padding: 0; }
    h1, h2, h3 { color: #00d4aa; margin-top: 0; }
    .navbar { background-color: #0b0b12; padding: 15px; display: flex; justify-content: space-between; align-items: center; border-bottom: 2px solid #00d4aa; }
    .navbar a { color: #fff; text-decoration: none; margin: 0 10px; font-weight: bold; }
    .container { padding: 20px; max-width: 1000px; margin: auto; }
    .card { background: #232336; border-radius: 10px; padding: 20px; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 20px; }
    .btn { background: #00d4aa; color: #000; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; text-decoration: none; display: inline-block; font-weight: bold; margin-top: 5px;}
    .btn:hover { background: #00a885; }
    .btn-red { background: #ff4d4d; color: white; }
    input, select { padding: 8px; border-radius: 4px; border: 1px solid #444; background: #161625; color: white; width: 100%; box-sizing: border-box; margin-bottom: 10px;}
    .status-on { color: #00ff00; font-weight: bold; }
    .status-off { color: #ff0000; font-weight: bold; }
    .log-box { background: #000; color: #0f0; font-family: monospace; padding: 10px; height: 300px; overflow-y: scroll; border: 1px solid #444; }
    img.cam { width: 100%; border-radius: 5px; border: 2px solid #444; }
  </style>
</head>
<body>
<div class="navbar">
  <div style="font-size: 1.2rem;">🌱 Growtron</div>
  <div>
    <a href="/">Home</a>
    <a href="/config">Cfg</a>
    <a href="/logs">Logs</a>
    <a href="/logout" style="color:#ff6b6b;">Sair</a>
  </div>
</div>
<div class="container">
)raw";
}

String getFooter() {
  return R"raw(
</div>
<script>
  function toggleRega(vaso) {
    if(confirm('Alterar estado da rega do Vaso ' + (vaso+1) + '?')) {
      fetch('/rega', { method: 'POST', headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: 'vaso='+vaso })
      .then(() => setTimeout(()=>location.reload(), 1000));
    }
  }
</script>
</body></html>
)raw";
}

void handleRoot(AsyncWebServerRequest *request) {
  if (!checkAuth(request)) return request->redirect("/login");
  
  String html = getHead();
  
  html += "<div class='card'><h2>Status do Sistema</h2>";
  if(apMode) html += "<p style='color:orange'>MODO AP - Configure o WiFi nas Configurações</p>";
  html += "<div class='grid'>";
  html += "<div><b>Hora:</b> " + timeClient.getFormattedTime() + "</div>";
  html += "<div><b>Temp:</b> " + String(tempAtual, 1) + " C</div>";
  html += "<div><b>Umid:</b> " + String(humAtual, 1) + " %</div>";
  html += "<div><b>Lux:</b> " + String(luxADC) + "</div>";
  html += "</div></div>";

  if (strlen(cfGlobal.camIP) > 5 && !apMode) {
      html += "<div class='card'><h2>Monitoramento Visual</h2>";
      html += "<img class='cam' src='http://" + String(cfGlobal.camIP) + "/capture?token=" + String(cfGlobal.camToken) + "' alt='Camera'>";
      html += "</div>";
  }

  html += "<div class='card'><h2>Iluminação</h2><div class='grid'>";
  for(int i=0; i<cfGlobal.numParticoes; i++) {
    html += "<div><h3>Grow " + String(i==0?"A":"B") + "</h3>";
    html += "Normal: <span class='" + String(luzNormalState[i]?"status-on":"status-off") + "'>" + (luzNormalState[i]?"ON":"OFF") + "</span><br>";
    html += "Emerson: <span class='" + String(luzEmersonState[i]?"status-on":"status-off") + "'>" + (luzEmersonState[i]?"ON":"OFF") + "</span>";
    html += "</div>";
  }
  html += "</div></div>";

  html += "<div class='grid'>";
  for(int i=0; i<4; i++) {
    if(!cfVaso[i].ativo) continue;
    html += "<div class='card'>";
    html += "<h3>" + String(cfVaso[i].nome) + " (G" + String(cfVaso[i].particao==0?"A":"B") + ")</h3>";
    html += "<p>Umidade: <b>" + String(stVaso[i].umidadePercent) + "%</b> (" + String(stVaso[i].umidadeADC) + ")</p>";
    html += "<p>Bomba: <span class='" + String(stVaso[i].regando?"status-on":"status-off") + "'>" + (stVaso[i].regando?"LIGADO":"DESLIGADO") + "</span></p>";
    html += "<button class='btn' onclick='toggleRega(" + String(i) + ")'>" + (stVaso[i].regando?"Parar Rega":"Regar Agora") + "</button>";
    html += "</div>";
  }
  html += "</div>";

  html += getFooter();
  request->send(200, "text/html", html);
}

void handleConfig(AsyncWebServerRequest *request) {
  if (!checkAuth(request)) return request->redirect("/login");
  
  String html = getHead();
  html += "<h2>Configurações</h2><form method='POST' action='/config'>";
  
  html += "<div class='card'><h3>WiFi e Geral</h3>";
  html += "SSID WiFi: <input type='text' name='ssid' value='" + String(cfGlobal.wifiSSID) + "'><br>";
  html += "Senha WiFi: <input type='password' name='wpass' value='" + String(cfGlobal.wifiPass) + "'><br>";
  html += "Senha Admin: <input type='text' name='pass' value='" + String(cfGlobal.userPass) + "'><br>";
  html += "Partições (1 ou 2): <input type='number' name='nPart' min='1' max='2' value='" + String(cfGlobal.numParticoes) + "'><br>";
  html += "Tipo DHT (11 ou 22): <input type='number' name='dhtType' value='" + String(cfGlobal.dhtType) + "'>";
  html += "</div>";

  for(int i=0; i<2; i++) {
    String growName = (i==0) ? "Grow A" : "Grow B";
    String p = String(i);
    html += "<div class='card'><h3>Iluminação - " + growName + "</h3>";
    html += "Ligar (H:M): <input type='number' name='l"+p+"_hOn' style='width:60px' value='"+String(cfLuz[i].horaLigar)+"'> : ";
    html += "<input type='number' name='l"+p+"_mOn' style='width:60px' value='"+String(cfLuz[i].minutoLigar)+"'><br>";
    html += "Desligar (H:M): <input type='number' name='l"+p+"_hOff' style='width:60px' value='"+String(cfLuz[i].horaDesligar)+"'> : ";
    html += "<input type='number' name='l"+p+"_mOff' style='width:60px' value='"+String(cfLuz[i].minutoDesligar)+"'><br>";
    html += "Deslocamento Emerson (min): <input type='number' name='l"+p+"_emer' value='"+String(cfLuz[i].deslocamentoEmerson)+"'>";
    html += "</div>";
  }

  html += "<div class='card'><h3>Integrações</h3>";
  html += "Câmera IP: <input type='text' name='camIP' value='" + String(cfGlobal.camIP) + "'><br>";
  html += "Câmera Token: <input type='text' name='camTok' value='" + String(cfGlobal.camToken) + "'><br>";
  html += "<hr>ThingSpeak Ativo? <input type='checkbox' name='tsActive' " + String(cfGlobal.thingSpeakAtivo?"checked":"") + "><br>";
  html += "API Key: <input type='text' name='tsKey' value='" + String(cfGlobal.thingSpeakApiKey) + "'><br>";
  html += "Channel ID: <input type='text' name='tsChan' value='" + String(cfGlobal.thingSpeakChannel) + "'>";
  html += "</div>";

  html += "<button type='submit' class='btn'>Salvar Tudo e Reiniciar</button></form><br>";

  // Config Vasos - CORREÇÃO DE STRINGS AQUI
  html += "<h3>Configuração de Vasos</h3>";
  html += "<div class='grid'>";
  for(int i=0; i<4; i++) {
    html += "<div class='card'><form method='POST' action='/configVaso?id=" + String(i) + "'>";
    html += "<h4>Vaso " + String(i+1) + " (" + String(cfVaso[i].nome) + ")</h4>";
    html += "Nome: <input type='text' name='nome' value='" + String(cfVaso[i].nome) + "'><br>";
    html += "Ativo: <input type='checkbox' name='ativo' " + String(cfVaso[i].ativo?"checked":"") + "><br>";
    html += "Partição (0=A, 1=B): <input type='number' name='part' value='" + String(cfVaso[i].particao) + "'><br>";
    html += "Limites (%): Min <input type='number' name='limL' style='width:50px' value='"+String(cfVaso[i].limiteUmidadeBaixo)+"'> ";
    html += "Max <input type='number' name='limH' style='width:50px' value='"+String(cfVaso[i].limiteUmidadeAlto)+"'><br>";
    html += "<hr>Auto Rega: <input type='checkbox' name='auto' "+String(cfVaso[i].regaAutomatica?"checked":"")+"><br>";
    // String Corrections Applied Below:
    html += "Horário 1: <input type='number' name='h1' style='width:40px' value='"+String(cfVaso[i].horaRega1)+"'>:<input type='number' name='m1' style='width:40px' value='"+String(cfVaso[i].minutoRega1)+"'><br>";
    html += "Usar Horário 2? <input type='checkbox' name='u2' "+String(cfVaso[i].usarRega2?"checked":"")+"> ";
    html += "<input type='number' name='h2' style='width:40px' value='"+String(cfVaso[i].horaRega2)+"'>:<input type='number' name='m2' style='width:40px' value='"+String(cfVaso[i].minutoRega2)+"'><br>";
    html += "<hr>Modo Pulsativo? <input type='checkbox' name='puls' "+String(cfVaso[i].regaPulsativa?"checked":"")+"><br>";
    html += "Tempo Contínuo (s): <input type='number' name='tcont' value='"+String(cfVaso[i].tempoRegaContinua)+"'><br>";
    html += "Pulsos: <input type='number' name='npul' value='"+String(cfVaso[i].pulsosRega)+"'> (On: <input type='number' name='ton' style='width:40px' value='"+String(cfVaso[i].tempoPulsoOn)+"'>s Off: <input type='number' name='toff' style='width:40px' value='"+String(cfVaso[i].tempoPulsoOff)+"'>s)<br>";
    
    html += "<button type='submit' class='btn'>Salvar Vaso</button>";
    html += "</form>";
    
    html += "<hr><b>Calibração ("+String(stVaso[i].umidadeADC)+")</b><br>";
    html += "<form action='/calib' method='POST' style='display:inline;'><input type='hidden' name='v' value='"+String(i)+"'><input type='hidden' name='t' value='seco'>";
    html += "<button class='btn btn-warn' style='font-size:0.8rem'>Definir SECO ("+String(soloSecoADC[i])+")</button></form> ";
    html += "<form action='/calib' method='POST' style='display:inline;'><input type='hidden' name='v' value='"+String(i)+"'><input type='hidden' name='t' value='molhado'>";
    html += "<button class='btn btn-warn' style='font-size:0.8rem'>Definir MOLHADO ("+String(soloMolhadoADC[i])+")</button></form>";
    html += "</div>";
  }
  html += "</div>";
  
  html += "<div class='card'><h3>OTA Firmware</h3>";
  html += "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Atualizar' class='btn btn-red'></form></div>";

  html += getFooter();
  request->send(200, "text/html", html);
}

void handleSaveConfig(AsyncWebServerRequest *request) {
  if (!checkAuth(request)) return request->redirect("/login");
  
  if(request->hasArg("ssid")) strlcpy(cfGlobal.wifiSSID, request->arg("ssid").c_str(), 32);
  if(request->hasArg("wpass")) strlcpy(cfGlobal.wifiPass, request->arg("wpass").c_str(), 32);
  if(request->hasArg("pass")) strlcpy(cfGlobal.userPass, request->arg("pass").c_str(), 32);
  if(request->hasArg("nPart")) cfGlobal.numParticoes = request->arg("nPart").toInt();
  if(request->hasArg("dhtType")) cfGlobal.dhtType = request->arg("dhtType").toInt();
  if(request->hasArg("camIP")) strlcpy(cfGlobal.camIP, request->arg("camIP").c_str(), 20);
  if(request->hasArg("camTok")) strlcpy(cfGlobal.camToken, request->arg("camTok").c_str(), 20);
  
  cfGlobal.thingSpeakAtivo = request->hasArg("tsActive");
  if(request->hasArg("tsKey")) strlcpy(cfGlobal.thingSpeakApiKey, request->arg("tsKey").c_str(), 20);
  if(request->hasArg("tsChan")) strlcpy(cfGlobal.thingSpeakChannel, request->arg("tsChan").c_str(), 15);

  for(int i=0; i<2; i++) {
    String p = String(i);
    if(request->hasArg("l"+p+"_hOn")) cfLuz[i].horaLigar = request->arg("l"+p+"_hOn").toInt();
    if(request->hasArg("l"+p+"_mOn")) cfLuz[i].minutoLigar = request->arg("l"+p+"_mOn").toInt();
    if(request->hasArg("l"+p+"_hOff")) cfLuz[i].horaDesligar = request->arg("l"+p+"_hOff").toInt();
    if(request->hasArg("l"+p+"_mOff")) cfLuz[i].minutoDesligar = request->arg("l"+p+"_mOff").toInt();
    if(request->hasArg("l"+p+"_emer")) cfLuz[i].deslocamentoEmerson = request->arg("l"+p+"_emer").toInt();
  }

  saveGlobalConfig();
  request->send(200, "text/html", "<h1>Salvo! Reiniciando...</h1><script>setTimeout(function(){window.location.href='/';}, 5000);</script>");
  delay(1000);
  ESP.restart();
}

void handleSaveVaso(AsyncWebServerRequest *request) {
  if (!checkAuth(request)) return request->redirect("/login");
  if (request->hasArg("id")) {
    int id = request->arg("id").toInt();
    if (id >= 0 && id < 4) {
      if(request->hasArg("nome")) strlcpy(cfVaso[id].nome, request->arg("nome").c_str(), 32);
      cfVaso[id].ativo = request->hasArg("ativo");
      if(request->hasArg("part")) cfVaso[id].particao = request->arg("part").toInt();
      if(request->hasArg("limL")) cfVaso[id].limiteUmidadeBaixo = request->arg("limL").toInt();
      if(request->hasArg("limH")) cfVaso[id].limiteUmidadeAlto = request->arg("limH").toInt();
      
      cfVaso[id].regaAutomatica = request->hasArg("auto");
      if(request->hasArg("h1")) cfVaso[id].horaRega1 = request->arg("h1").toInt();
      if(request->hasArg("m1")) cfVaso[id].minutoRega1 = request->arg("m1").toInt();
      cfVaso[id].usarRega2 = request->hasArg("u2");
      if(request->hasArg("h2")) cfVaso[id].horaRega2 = request->arg("h2").toInt();
      if(request->hasArg("m2")) cfVaso[id].minutoRega2 = request->arg("m2").toInt();
      
      cfVaso[id].regaPulsativa = request->hasArg("puls");
      if(request->hasArg("tcont")) cfVaso[id].tempoRegaContinua = request->arg("tcont").toInt();
      if(request->hasArg("npul")) cfVaso[id].pulsosRega = request->arg("npul").toInt();
      if(request->hasArg("ton")) cfVaso[id].tempoPulsoOn = request->arg("ton").toInt();
      if(request->hasArg("toff")) cfVaso[id].tempoPulsoOff = request->arg("toff").toInt();

      saveVasoConfig(id);
    }
  }
  request->redirect("/config");
}

void setupServer() {
  server.on("/", HTTP_GET, handleRoot);
  
  server.on("/login", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = getHead();
    html += "<div class='card'><h2>Login</h2><form method='POST' action='/login'>";
    html += "Usuário: admin<br>Senha: <input type='password' name='password'><br>";
    html += "<button type='submit' class='btn'>Entrar</button></form></div>";
    html += getFooter();
    request->send(200, "text/html", html);
  });
  
  server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasArg("password")) {
       if(request->arg("password") == String(cfGlobal.userPass)) {
         AsyncWebServerResponse *response = request->beginResponse(303);
         response->addHeader("Location", "/");
         response->addHeader("Set-Cookie", "ESPSESSIONID=1");
         request->send(response);
         return;
       }
    }
    request->redirect("/login");
  });

  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(303);
    response->addHeader("Location", "/login");
    response->addHeader("Set-Cookie", "ESPSESSIONID=0; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
    request->send(response);
  });

  server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return request->redirect("/login");
    String html = getHead();
    html += "<div class='card'><h2>Logs do Sistema</h2><div class='log-box'>";
    File file = LittleFS.open("/logs.txt", "r");
    if(file){
      while(file.available()){
        html += (char)file.read();
      }
      html.replace("\n", "<br>");
      file.close();
    } else {
      html += "Sem logs.";
    }
    html += "</div><br><a href='/resetLogs' class='btn btn-red'>Limpar Logs</a></div>";
    html += getFooter();
    request->send(200, "text/html", html);
  });

  server.on("/resetLogs", HTTP_GET, [](AsyncWebServerRequest *request){
    if(checkAuth(request)) { LittleFS.remove("/logs.txt"); }
    request->redirect("/logs");
  });

  server.on("/config", HTTP_GET, handleConfig);
  server.on("/config", HTTP_POST, handleSaveConfig);
  server.on("/configVaso", HTTP_POST, handleSaveVaso);

  server.on("/calib", HTTP_POST, [](AsyncWebServerRequest *request){
    if(checkAuth(request) && request->hasArg("v") && request->hasArg("t")) {
      int v = request->arg("v").toInt();
      String t = request->arg("t");
      int val = analogRead(PIN_SOLO[v]);
      if(t == "seco") soloSecoADC[v] = val;
      if(t == "molhado") soloMolhadoADC[v] = val;
      saveCalib(v);
    }
    request->redirect("/config");
  });

  server.on("/rega", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!checkAuth(request)) return request->send(403);
    if(request->hasArg("vaso")) {
      int v = request->arg("vaso").toInt();
      if(stVaso[v].regando) stopRega(v);
      else startRega(v);
    }
    request->send(200);
  });

  // OTA
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
      bool shouldReboot = !Update.hasError();
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot?"OK":"FAIL");
      response->addHeader("Connection", "close");
      request->send(response);
      if(shouldReboot) ESP.restart();
    },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
      if(!checkAuth(request)) return;
      if(!index){
        if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)){
          Update.printError(Serial);
        }
      }
      if(Update.write(data, len) != len){
        Update.printError(Serial);
      }
      if(final){
        if(Update.end(true)){
          Serial.printf("OTA Sucesso: %u B\n", index+len);
        } else {
          Update.printError(Serial);
        }
      }
  });

  server.begin();
}

// --------------------------------------------------------------------------
// 7. SETUP E LOOP
// --------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_FACTORY_RESET, INPUT_PULLUP);
  for(int i=0; i<4; i++) { pinMode(PIN_RELE_LUZ[i], OUTPUT); digitalWrite(PIN_RELE_LUZ[i], HIGH); }
  for(int i=0; i<4; i++) { pinMode(PIN_RELE_REGA[i], OUTPUT); digitalWrite(PIN_RELE_REGA[i], HIGH); }
  
  if(!LittleFS.begin(true)){ Serial.println("LittleFS Error"); }

  loadSettings();
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("GROWTRON BOOT...");
  display.display();

  dht = new DHT(PIN_DHT, (cfGlobal.dhtType == 22 ? DHT22 : DHT11));
  dht->begin();

  // Reset de Fábrica via Botão
  if(digitalRead(PIN_FACTORY_RESET) == LOW) {
    display.println("Resetando WiFi...");
    display.display();
    preferences.begin("growtron", false);
    preferences.putString("ssid", "");
    preferences.putString("wpass", "");
    preferences.end();
    delay(2000);
  }
  
  // Conexão WiFi Manual (Substitui WiFiManager)
  WiFi.mode(WIFI_STA);
  if(strlen(cfGlobal.wifiSSID) > 0) {
    display.println("Conectando WiFi...");
    display.printf("%s\n", cfGlobal.wifiSSID);
    display.display();
    WiFi.begin(cfGlobal.wifiSSID, cfGlobal.wifiPass);
    
    int retries = 0;
    while(WiFi.status() != WL_CONNECTED && retries < 20) {
      delay(500);
      Serial.print(".");
      retries++;
    }
  }

  if(WiFi.status() != WL_CONNECTED) {
    // Modo AP se falhar ou não tiver config
    apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("GrowtronAP", "growtron123");
    Serial.println("WiFi falhou. Criando AP GrowtronAP");
    display.println("FALHA WIFI");
    display.println("CRIANDO AP...");
    display.display();
  } else {
    apMode = false;
    Serial.println("WiFi Conectado!");
    display.println("WIFI OK");
    display.println(WiFi.localIP());
    display.display();
  }

  timeClient.begin();
  timeClient.update();

  setupServer();
  
  logMessage("SYSTEM", "Boot Completo.");
}

void loop() {
  unsigned long now = millis();
  
  if(!apMode) timeClient.update();

  if (now - lastSensorRead > 5000) {
    checkSensors();
    checkLights();
    lastSensorRead = now;
  }

  if (now - lastRegaCheck > 100) { 
    controlWatering();
    lastRegaCheck = now;
  }

  if (now - lastDisplayUpdate > 2000) {
    updateOLED();
    lastDisplayUpdate = now;
  }

  if (now - lastThingSpeak > 60000) {
    sendThingSpeak();
    lastThingSpeak = now;
  }

  if (digitalRead(PIN_FACTORY_RESET) == LOW) {
    unsigned long t = millis();
    while(digitalRead(PIN_FACTORY_RESET) == LOW) {
      if(millis() - t > 5000) {
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("FACTORY RESET...");
        display.display();
        preferences.begin("growtron", false);
        preferences.clear();
        preferences.end();
        LittleFS.remove("/logs.txt");
        delay(2000);
        ESP.restart();
      }
    }
  }
}
