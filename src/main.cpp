#define BLYNK_TEMPLATE_ID "TMPL2eYp6wwr_"
#define BLYNK_TEMPLATE_NAME "Midup"
#define BLYNK_AUTH_TOKEN "CqNfc1gSJKikV6zTHVBGNV_Att1HMb2W"

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <WebServer.h>

// LCD I2C no endereço 0x3F, com 16 colunas e 2 linhas
LiquidCrystal_I2C lcd(0x3F, 16, 2);

const int pinoEmissor = 14;
const int sensor1 = 13; // KY-022
const int sensor2 = 26; // Sensor reflexivo

const int leds[] = {15, 16, 17, 18, 19};
const int numLeds = 5;

unsigned long tempoSensor1 = 0;
unsigned long tempoSensor2 = 0;
bool esperandoSensor2 = false;
bool esperandoSensor1 = false;
unsigned long tempoLimite = 3000; // 3 segundos para timeout

char ssid[] = "nome da rede";
char pass[] = "senha";

int pessoas = 0; // contador de pessoas presentes
String historico = "";

WebServer server(80);

void atualizarLeds(int pessoas) {
  int n = pessoas;
  if (n > numLeds) n = numLeds;
  if (n < 0) n = 0;

  for (int i = 0; i < numLeds; i++) {
    digitalWrite(leds[i], i < n ? HIGH : LOW);
  }
}

void atualizarInterface(String evento) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(evento);
  lcd.setCursor(0, 1);
  lcd.print("Pessoas: ");
  lcd.print(pessoas);

  atualizarLeds(pessoas);
  delay(2000);
  lcd.clear();
}

void paginaWeb() {
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html lang="pt-br">
    <head>
      <meta charset="UTF-8">
      <title>Monitor de Ocupação</title>
      <style>
        body {
          font-family: Arial, sans-serif;
          background-color: #f2f2f2;
          color: #333;
          text-align: center;
          padding: 20px;
        }
        .container {
          background: white;
          max-width: 600px;
          margin: auto;
          padding: 20px;
          border-radius: 10px;
          box-shadow: 0 4px 8px rgba(0,0,0,0.1);
        }
        h1 {
          color: #4CAF50;
        }
        .pessoas {
          font-size: 2em;
          margin: 20px 0;
        }
        pre {
          text-align: left;
          background: #eee;
          padding: 10px;
          border-radius: 5px;
          overflow-x: auto;
          max-height: 200px;
          white-space: pre-wrap;
          word-wrap: break-word;
        }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>Monitor de Ocupação</h1>
        <div class="pessoas">👥 Pessoas no ambiente: <strong>%PESSOAS%</strong></div>
        <h2>Histórico</h2>
        <pre>%HISTORICO%</pre>
      </div>
    </body>
    </html>
  )rawliteral";

  html.replace("%PESSOAS%", String(pessoas));
  html.replace("%HISTORICO%", historico);
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);

  pinMode(sensor1, INPUT);
  pinMode(sensor2, INPUT);
  for (int i = 0; i < numLeds; i++) {
    pinMode(leds[i], OUTPUT);
    digitalWrite(leds[i], LOW);
  }

  ledcSetup(0, 38000, 8);
  ledcAttachPin(pinoEmissor, 0);
  ledcWrite(0, 128);

  Wire.begin(33, 32);
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Iniciando WiFi...");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  while (!Blynk.connected()) {
    delay(500);
    Serial.print(".");
  }

  lcd.clear();
  lcd.print("WiFi conectado!");
  delay(1000);
  lcd.clear();

  server.on("/", paginaWeb);
  server.begin();

  Serial.println("Servidor web iniciado.");
  Serial.print("IP do ESP32: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  Blynk.run();
  server.handleClient();

  int leitura1 = digitalRead(sensor1);
  int leitura2 = digitalRead(sensor2);
  unsigned long agora = millis();

  // Detecta sensor1 ativado primeiro
  if (leitura1 == LOW && !esperandoSensor2 && !esperandoSensor1) {
    tempoSensor1 = agora;
    esperandoSensor2 = true;
    Serial.println("Sensor 1 ativado, esperando sensor 2...");
  }

  // Detecta sensor2 ativado primeiro
  if (leitura2 == LOW && !esperandoSensor1 && !esperandoSensor2) {
    tempoSensor2 = agora;
    esperandoSensor1 = true;
    Serial.println("Sensor 2 ativado, esperando sensor 1...");
  }

  // Sequência entrada: sensor1 → sensor2
  if (esperandoSensor2 && leitura2 == LOW) {
    pessoas++;
    String evento = "Entrada detectada";
    Serial.println("📥 " + evento);
    historico += evento + " - " + String(pessoas) + " pessoas - " + String(agora / 1000) + "s\n";
    atualizarInterface(evento);
    Blynk.virtualWrite(V0, pessoas);
    esperandoSensor2 = false;
  }

  // Sequência saída: sensor2 → sensor1
  if (esperandoSensor1 && leitura1 == LOW) {
    if (pessoas > 0) pessoas--;
    String evento = "Saida detectada";
    Serial.println("📤 " + evento);
    historico += evento + " - " + String(pessoas) + " pessoas - " + String(agora / 1000) + "s\n";
    atualizarInterface(evento);
    Blynk.virtualWrite(V0, pessoas);
    esperandoSensor1 = false;
  }

  // Timeout para resetar espera
  if (esperandoSensor2 && (agora - tempoSensor1 > tempoLimite)) {
    esperandoSensor2 = false;
    Serial.println("⏱️ Timeout: sensor 2 não ativado a tempo");
  }
  if (esperandoSensor1 && (agora - tempoSensor2 > tempoLimite)) {
    esperandoSensor1 = false;
    Serial.println("⏱️ Timeout: sensor 1 não ativado a tempo");
  }

  delay(50);
}
