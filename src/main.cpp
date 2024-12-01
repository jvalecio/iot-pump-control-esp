#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <qrcode_espi.h>
#include <ArduinoJson.h>

const char* ssid = "ZTE_DFA0 OI 2.4G";
const char* password = "20204040";


#define BT3_PIN 5
#define BT2_PIN 33
#define BT1_PIN 32

#define PUMP1_PIN 2
#define PUMP2_PIN 25
//Your Domain name with URL path or IP address with path
String  serverName = "https://52.200.71.202:5000/";  // Server URL

// Definições
#define TEMPO_PARA_EXPIRAR_QR_CODE 120000 // 2 minutos
#define TICK_DELAY 100 
#define PUMP_ON_TIME  180000

unsigned long pump_time_left = PUMP_ON_TIME;

// Variáveis globais
unsigned long lastTime = 0;
unsigned long qrCodeInitTimestamp = 0;
unsigned long pumpOnStartTime = 0;
unsigned long now = 0;
bool approved = false;
bool paused = false;
int activePump = 0;

enum State { IDLE, SEND_REQUEST, WAIT_FOR_APPROVAL, HANDLE_APPROVAL, PUMP_ON, PUMP_SELECTION, PAUSED, ERROR };
State currentState = IDLE;

TFT_eSPI display = TFT_eSPI();
QRcode_eSPI qrcode (&display);

bool bt1 = false;
bool bt2 = false;
bool bt3 = false;

// Funções auxiliares
void handleError(const String &message) {
  Serial.println("ERRO");
  Serial.println(message);
  currentState = ERROR;
}

void IRAM_ATTR BT1_ISR() {
    //Serial.println("BOTAO 1");
    bt1 = !digitalRead(BT1_PIN);
}

void IRAM_ATTR BT2_ISR() {
  //Serial.println("BOTAO 2");
    bt2 = !digitalRead(BT2_PIN);
}

void IRAM_ATTR BT3_ISR() {
  //Serial.println("BOTAO 3");
    bt3 = !digitalRead(BT3_PIN);
}


bool read_bt1(){
  bool tmp = bt1;
  bt1 = false;

  return tmp;
}

bool read_bt2(){
  bool tmp = bt2;
  bt2 = false;

  return tmp;
}

bool read_bt_pause(){
  bool tmp = bt3;
  bt3 = false;

  return tmp;
}

void reset_bt(){
  bt1 = false;
  bt2 = false;
}

void turn_on_pump(int pump){
  if(pump == 1){
    digitalWrite(PUMP1_PIN, LOW);
  }else if(pump == 2){
    digitalWrite(PUMP2_PIN, LOW);
  }
  
}
void turn_off_pumps(){
  digitalWrite(PUMP1_PIN, HIGH);
  digitalWrite(PUMP2_PIN, HIGH);
}



void setup() {
  pinMode(BT1_PIN, INPUT_PULLUP);
  pinMode(BT2_PIN, INPUT_PULLUP);
  pinMode(BT3_PIN, INPUT_PULLUP);
  pinMode(PUMP1_PIN, OUTPUT);
  pinMode(PUMP2_PIN, OUTPUT);
  digitalWrite(PUMP1_PIN, HIGH);
  digitalWrite(PUMP2_PIN, HIGH);
  reset_bt();
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("Connecting");

  display.init();
  qrcode.init();
  // Initialize QRcode display using library

  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  //display.print("Connected to WiFi network with IP Address: ");
  //display.println(WiFi.localIP());
  display.fillScreen(TFT_WHITE);
  Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 seconds before publishing the first reading.");
  display.setTextColor(TFT_BLACK,TFT_WHITE);  
  display.setTextSize(3);
  display.setRotation(3);
  
  Serial.println("Starting...");
  
  attachInterrupt(digitalPinToInterrupt(BT1_PIN), BT1_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BT2_PIN), BT2_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BT3_PIN), BT3_ISR, CHANGE);
  delay(3000);
}



void loop() {
  unsigned long current_time = millis();
  switch (currentState) {
    case IDLE:
      //display.fillScreen(TFT_WHITE);
      display.setCursor(0,0);
      //display.println("\n Pressione \n o botao para \n iniciar");
      display.print(" \n\nPressione o botao \n   para iniciar");
      if ((current_time - lastTime) > TICK_DELAY) {
        lastTime = current_time;
        if (WiFi.status() == WL_CONNECTED) {
          
          if(read_bt1() || read_bt2()){
            currentState = SEND_REQUEST;
            Serial.println("BOTAO 2 on");
            display.fillScreen(TFT_WHITE);
            display.setCursor(0,0);
            display.print("\n\n Realize o \n pagamento via \n PIX para a \n liberacao \n do dispositivo");
            delay(5000);
            display.fillScreen(TFT_WHITE);
            display.setCursor(0,0);
          }else{
          }
        } else {
          Serial.println("WiFi Disconnected");
          currentState = ERROR;
        }
      }
      break;
    
    case SEND_REQUEST: {
      Serial.println("SEND REQUEST");
      HTTPClient http;
      String serverPath = serverName + "criar-pix";
      http.begin(serverPath.c_str());
      int httpResponseCode = http.GET();

      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);

        String payload = http.getString();
        Serial.println(payload);

        JsonDocument doc;
        // Deserialize the JSON document
        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
            handleError("Error parsing JSON: " + String(error.c_str()));
            break;
          }

          String qr = doc["qrcode"];
          if (!qr.isEmpty()) {
            qrCodeInitTimestamp = millis();
            display.setRotation(0);
            display.fillScreen(TFT_WHITE);
            display.setCursor(0,0);
            qrcode.create(qr);  // Função de criação de QR Code
            display.setRotation(3);
            approved = false;
            currentState = WAIT_FOR_APPROVAL;
          }else{
            handleError("Error parsing JSON: " + String(error.c_str()));
            break;
          }
      } else {
        handleError("Error in SEND_REQUEST: " + String(httpResponseCode));
      }
      http.end();
    } break;

    case WAIT_FOR_APPROVAL: {
      if ((millis() - qrCodeInitTimestamp) < TEMPO_PARA_EXPIRAR_QR_CODE) {
        HTTPClient http;
        String serverPath = serverName + "checar-pagamento";
        delay(5000);
        http.begin(serverPath.c_str());
        int httpResponseCode = http.GET();

        if (httpResponseCode > 0) {
          Serial.print("HTTP Response code: ");
          Serial.println(httpResponseCode);

          String payload = http.getString();

          JsonDocument doc;
            // Deserialize the JSON document
          DeserializationError error = deserializeJson(doc, payload);

          if (error) {
            handleError("Error parsing JSON: " + String(error.c_str()));
            break;
          }

          String status = doc["status"];
          if (status.equals("approved")) {
            //if(1){
            approved = true;
            currentState = HANDLE_APPROVAL;
            break;
          }else{
            approved = false;
          }
        } else {
          Serial.print("Error code: ");
          Serial.println(httpResponseCode);
        }
        delay(1000);  // Aguarda 1 segundo antes de verificar novamente
      } else {
        currentState = HANDLE_APPROVAL;
      }
    } break;

    case HANDLE_APPROVAL:
      if (approved) {
        display.fillScreen(TFT_GREEN);
        display.setCursor(0,0);
        delay(1000);
        display.fillScreen(TFT_WHITE);
        display.println("\n PAGAMENTO \n CONFIRMADO");
        delay(1000);
        display.fillScreen(TFT_WHITE);
        display.setCursor(0,0);
        display.println("\n SELECIONE \n A BOMBA\n PARA INICIAR");
        currentState = PUMP_SELECTION;
      } else {
        display.fillScreen(TFT_RED);
        display.setCursor(0,0);
        delay(1000);
        display.fillScreen(TFT_WHITE);
        display.println("\n PAGAMENTO \n NAO \n CONFIRMADO");
        delay(1000);
        display.fillScreen(TFT_WHITE);
        display.setCursor(0,0);
        currentState = IDLE;
      }
      delay(500);
      //display.fillScreen(TFT_WHITE);
      break;
    
    case PUMP_SELECTION:
        if (read_bt1()) {
          display.fillScreen(TFT_WHITE);
          display.setCursor(0,0);
          display.println("\n BOMBA 1 \n SELECIONADA");
          activePump = 1;
          delay(2000);

          display.fillScreen(TFT_WHITE);
          display.setCursor(0,0);
          display.print("\n DISPOSITIVO \n LIBERADO");

          pumpOnStartTime = millis();
          pump_time_left = PUMP_ON_TIME;
          currentState = PUMP_ON;
        }else if (read_bt2()){
          display.fillScreen(TFT_WHITE);
          display.setCursor(0,0);
          display.println("\n BOMBA 2 \n SELECIONADA");
          activePump = 2;
          delay(2000);

          display.fillScreen(TFT_WHITE);
          display.setCursor(0,0);
          display.print("\n DISPOSITIVO \n LIBERADO");

          pumpOnStartTime = millis();
          pump_time_left = PUMP_ON_TIME;
          currentState = PUMP_ON;
        }
    break;
    case PAUSED:
      if (read_bt_pause()) {
        paused = !paused;
        display.fillScreen(TFT_WHITE);
        display.setCursor(0,0);
        display.println("\n RETOMANDO");
        delay(1000);
        pumpOnStartTime = millis();
        display.fillScreen(TFT_WHITE);
        display.setCursor(0,0);
        display.print("\n DISPOSITIVO \n LIBERADO");
        currentState = PUMP_ON;
      }
      if (read_bt1()) {
        display.fillScreen(TFT_WHITE);
        display.setCursor(0,0);
        display.println("\n BOMBA 1 \n SELECIONADA");
        delay(2000);
        currentState = PUMP_ON;
        activePump = 1;

        delay(2000);
        display.fillScreen(TFT_WHITE);
        display.setCursor(0,0);
        display.print("\n DISPOSITIVO \n LIBERADO");
        pumpOnStartTime = millis();

      }else if (read_bt2()){
        display.fillScreen(TFT_WHITE);
        display.setCursor(0,0);
        display.println("\n BOMBA 2 \n SELECIONADA");
        delay(2000);
        display.fillScreen(TFT_WHITE);
        display.setCursor(0,0);
        display.print("\n DISPOSITIVO \n LIBERADO");
        currentState = PUMP_ON;
        activePump = 2;

        delay(2000);
        pumpOnStartTime = millis();
      }
    
    break;
    case PUMP_ON:
      
      if (read_bt_pause()) {
        paused = !paused;
        pump_time_left = pump_time_left - (millis() - pumpOnStartTime);

        turn_off_pumps();
        delay(200);
        Serial.print("time left: ");
        Serial.println(pump_time_left);
        display.fillScreen(TFT_WHITE);
        display.setCursor(0,0);
        display.println("\n\n PAUSADO");
        delay(1000);
        currentState = PAUSED;
      }else{
        now = millis();
        if ((now - pumpOnStartTime) <= pump_time_left) {
          
          turn_on_pump(activePump);
          
          display.setCursor(135, 149);
          display.print( (pump_time_left - (now - pumpOnStartTime))/ 1000);
          display.print("     ");
        } else {
          turn_off_pumps();
          display.fillScreen(TFT_WHITE);
          display.setCursor(0,0);
          display.print("\n FINALIZADO");
          delay(2000);
          display.fillScreen(TFT_WHITE);
          currentState = IDLE;
        }
      }
      break;

    case ERROR:
      display.fillScreen(TFT_RED);
      delay(1000);
      display.fillScreen(TFT_WHITE);
      display.println("\n\n ERRO DE CONEXAO");
      delay(3000);
      display.fillScreen(TFT_WHITE);
      display.setCursor(0,0);
      lastTime = millis();
      currentState = IDLE;
      reset_bt();
      break;



  }
}
