#include "EmonLib.h"
#include <Preferences.h>
#include <Wire.h>
#include <U8g2lib.h>     
#include <WiFi.h>
#include <WebServer.h>

// ========== CONFIGURAÇÃO WIFI (ACCESS POINT) ==========
const char* ssid = "LAB_MED_ELET_02";
const char* password = "12345678";
const int wifi_channel = 3;  //CANAL WIFI FIXO (1-3-6-8-11-13) 
WebServer server(80);


// ========== SEGURANÇA CONTRA RUÍDO DE BOOT ==========
// Bloqueia envios para o banco/display por 8 segundos até o hardware estabilizar
const unsigned long TEMPO_ESTABILIZACAO = 8000; 

// ========== PINOS ==========
#define I2C_SDA 13   
#define I2C_SCL 14   
#define BOTAO_EXTERNO 27
#define SCT_PIN 32             
#define ZMPT_PIN 34
#define LED_BUILTIN 2              

// ========== CONFIGURAÇÃO DISPLAY U8G2 (SH1106 1.3") ==========
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// ========== CALIBRACAO ==========
#define V_CAL 303
#define I_CAL 1.16
#define PHASECAL -4.

EnergyMonitor emon1;
Preferences preferences;

// Variáveis Globais de Medição
float v, i, p, s, q, fp;
int telaAtual = 0;
int telaAnterior = 0;
double consumoKWh = 0.0;
unsigned long tempoAnterior = 0;
unsigned long ultimoSalvamento = 0;
unsigned long ultimaMedicao = 0;

#define PULSOS_POR_KWH 1600  
double energiaAcumuladaParaPulso = 0.0;  
unsigned long ultimoPulso = 0;

volatile bool botaoFlag = false;

// ========== HTML ==========
const char HTML_INDEX[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Monitor ESP32</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:system-ui,-apple-system,sans-serif;background:#f5f5f5;display:flex;align-items:center;justify-content:center;min-height:100vh;color:#333}
.container{background:#fff;border-radius:16px;box-shadow:0 2px 20px rgba(0,0,0,.08);padding:32px;max-width:480px;width:90%}
h1{font-size:1.4em;font-weight:600;margin-bottom:28px;text-align:center;display:flex;align-items:center;justify-content:center;gap:10px}
.row{display:flex;justify-content:space-between;align-items:center;padding:14px 0;border-bottom:1px solid #eee}
.row:last-child{border:0}
.label{font-size:.95em;color:#666;font-weight:500}
.value{font-size:1.4em;font-weight:600;display:flex;align-items:baseline;gap:6px}
.unit{font-size:.75em;color:#999;font-weight:400}
</style>
</head>
<body>
<div class="container">
<h1><span>⚡</span>Monitor de Energia </h1>
<div class="row"><span class="label">Tensão RMS</span><div class="value"><span id="v">--</span><span class="unit">V</span></div></div>
<div class="row"><span class="label">Corrente RMS</span><div class="value"><span id="i">--</span><span class="unit">A</span></div></div>
<div class="row"><span class="label">Potência Ativa</span><div class="value"><span id="pa">--</span><span class="unit">W</span></div></div>
<div class="row"><span class="label">Potência Reativa</span><div class="value"><span id="pr">--</span><span class="unit">VAR</span></div></div>
<div class="row"><span class="label">Potência Aparente</span><div class="value"><span id="ps">--</span><span class="unit">VA</span></div></div>
<div class="row"><span class="label">Fator de Potência</span><div class="value"><span id="fp">--</span></div></div>
<div class="row"><span class="label">Energia</span><div class="value"><span id="e">--</span><span class="unit">kWh</span></div></div>
</div>
<script>
function update(){
  fetch('/data')
    .then(r => r.json())
    .then(d => {
      document.getElementById('v').textContent = d.vrms;
      document.getElementById('i').textContent = d.irms;
      document.getElementById('pa').textContent = d.pactive;
      document.getElementById('pr').textContent = d.preactive;
      document.getElementById('ps').textContent = d.paparent;
      document.getElementById('fp').textContent = d.fp;
      document.getElementById('e').textContent = d.kwh;
    })
    .catch(err => console.log('Erro:', err));
}
setInterval(update, 1200);
update();
</script>
</body>
</html>
)rawliteral";

// ========== HANDLERS SERVIDOR ==========
void handleRoot() { 
  server.send_P(200, "text/html", HTML_INDEX); 
}

void handleData() {
  char buffer[250];
  snprintf(buffer, sizeof(buffer), 
    "{\"vrms\":%.2f,\"irms\":%.2f,\"pactive\":%.1f,\"preactive\":%.1f,\"paparent\":%.1f,\"fp\":%.2f,\"kwh\":%.4f}", 
    v, i, p, q, s, fp, consumoKWh);
  server.send(200, "application/json", buffer);
}

void IRAM_ATTR handleBotao() {
  botaoFlag = true;
}

void setup() {
  Serial.begin(115200);
  
  pinMode(BOTAO_EXTERNO, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);  
  digitalWrite(LED_BUILTIN, LOW);
  
  attachInterrupt(digitalPinToInterrupt(BOTAO_EXTERNO), handleBotao, FALLING);

  Wire.begin(I2C_SDA, I2C_SCL); 
  u8g2.begin();

  analogReadResolution(12);             
  analogSetAttenuation(ADC_11db);       
  emon1.voltage(ZMPT_PIN, V_CAL, PHASECAL);
  emon1.current(SCT_PIN, I_CAL);
  
  preferences.begin("medidor", false); 
  consumoKWh = preferences.getDouble("consumo", 0.0);

  WiFi.softAP(ssid, password, wifi_channel);
  IPAddress IP = WiFi.softAPIP();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB12_tr);
  u8g2.drawStr(15, 30, " MEDIDAS");
  u8g2.drawStr(15, 52, "ELETRICAS");
  u8g2.sendBuffer();
  
  Serial.println("\n=== MEDIDOR MONOFÁSICO ===");
  Serial.printf("SSID: %s | Canal: %d\n", ssid, wifi_channel);
  Serial.printf("IP: %s\n", IP.toString().c_str());
  
  delay(2000);

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();

  tempoAnterior = millis();
  ultimoSalvamento = millis();
  ultimaMedicao = millis();
}

void loop() {
  server.handleClient();
  yield(); 
  
  if (botaoFlag) {
    botaoFlag = false;
    processarBotao();
  }

  unsigned long tempoAtual = millis();
  if (tempoAtual - ultimaMedicao >= 2000) {
    ultimaMedicao = tempoAtual;
    
    server.handleClient();
    
    emon1.calcVI(100, 2000);  
    
    // ==============================================================
    // TRAVA DE SEGURANÇA CONTRA RUÍDOS DE BOOT
    // ==============================================================
    if (millis() < TEMPO_ESTABILIZACAO) {
      // Nos primeiros 8 segundos, ignora os cálculos da EmonLib
      // Isso protege o banco de dados contra picos falsos de capacitor carregando
      v = 0.0;
      i = 0.0;
      p = 0.0;     
      s = 0.0; 
      fp = 0.0;
      q = 0.0;
    } else {
      // Passou o período de estabilização, assume os valores reais
      v = emon1.Vrms;
      i = emon1.Irms;
      p = emon1.realPower;     
      s = emon1.apparentPower; 
      fp = emon1.powerFactor;   
      
      // Filtro de ruído mínimo de linha (abaixo disso é considerado zero)
      if (v < 80.0 || i < 0.05) { 
        v=0; i=0; p=0; s=0; fp=0; 
      }
      
      q = (s > p) ? sqrt(pow(s, 2) - pow(p, 2)) : 0.0;
    }
    // ==============================================================

    double intervaloHoras = (double)(tempoAtual - tempoAnterior) / 3600000.0;
    if (p > 1.0) {
      double energiaWh = (p / 1000.0) * intervaloHoras * 1000.0;
      consumoKWh += energiaWh / 1000.0;
      energiaAcumuladaParaPulso += energiaWh;
      double whPorPulso = 1000.0 / PULSOS_POR_KWH;
      if (energiaAcumuladaParaPulso >= whPorPulso) {
        energiaAcumuladaParaPulso -= whPorPulso;
        gerarPulsoLED();
      }
    }
    tempoAnterior = tempoAtual;

    atualizarDisplay(v, i, p, s, q, fp);
    server.handleClient();
  }

  if (tempoAtual - ultimoSalvamento > 10000) {
    preferences.putDouble("consumo", consumoKWh);
    ultimoSalvamento = tempoAtual;
  }
  
  delay(5);
}

void processarBotao() {
  delay(50); 
  if (digitalRead(BOTAO_EXTERNO) == HIGH) return;
  
  telaAnterior = telaAtual;
  telaAtual++;
  if (telaAtual > 4) telaAtual = 0;
  
  unsigned long inicioPress = millis();
  int segAnterior = -1;
  
  while (digitalRead(BOTAO_EXTERNO) == LOW) {
    unsigned long duracao = millis() - inicioPress;
    if (duracao > 11000) break;
    server.handleClient();
    yield();
    
    if (duracao > 4000) {
      if (segAnterior == -1) telaAtual = telaAnterior;
      int seg = 10 - (duracao / 1000);
      if (seg != segAnterior && seg >= 0) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(10, 20, "MANTENHA PARA RESET");
        u8g2.setFont(u8g2_font_logisoso24_tr);
        u8g2.setCursor(50, 55); u8g2.print(seg);
        u8g2.sendBuffer();
        segAnterior = seg;
      }
      if (duracao >= 10000) {
        consumoKWh = 0.0;
        preferences.putDouble("consumo", 0.0);
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB14_tr);
        u8g2.drawStr(20, 40, "ZERADO!");
        u8g2.sendBuffer();
        delay(1500);
        return;
      }
    }
    delay(5);
  }
  atualizarDisplay(v, i, p, s, q, fp);
}

void atualizarDisplay(float v, float i, float p, float s, float q, float fp) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(10, 10, "Medidor Monofasico");
  u8g2.drawLine(0, 12, 128, 12);
  u8g2.setFont(u8g2_font_ncenB12_tr);

  switch (telaAtual) {
    case 0: // V + A
      u8g2.setCursor(0, 32); u8g2.print(v, 1); u8g2.print(" V");
      u8g2.setCursor(0, 54); u8g2.print(i, 2); u8g2.print(" A");
      break;
    case 1: // W + VA
      u8g2.setCursor(0, 32); u8g2.print(p, 1); u8g2.print(" W");
      u8g2.setCursor(0, 54); u8g2.print(s, 1); u8g2.print(" VA");
      break;
    case 2: // VAr + FP
      u8g2.setCursor(0, 32); u8g2.print(q, 1); u8g2.print(" VAr");
      u8g2.setCursor(0, 54); u8g2.print(fp, 2); u8g2.print(" fp");
      break;
    case 3: // kWh
      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.drawStr(0, 25, "Consumo Total:");
      u8g2.setFont(u8g2_font_ncenB10_tr);
      u8g2.setCursor(5, 45); u8g2.print(consumoKWh, 4); u8g2.print(" kWh");
      break;
    case 4: // WIFI
      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.setCursor(0, 28); u8g2.print("IP: "); u8g2.print(WiFi.softAPIP().toString());
      u8g2.setCursor(0, 43); u8g2.print("Rede: "); u8g2.print(ssid);
      u8g2.setCursor(0, 58); u8g2.print("Senha: "); u8g2.print(password);
      break;
  }
  
  if (telaAtual < 3) {
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.setCursor(0, 63);
    u8g2.print(consumoKWh, 5); u8g2.print(" kWh");
  }
  
  u8g2.sendBuffer();
}

void gerarPulsoLED() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(50);
  digitalWrite(LED_BUILTIN, LOW);
  ultimoPulso = millis();
}