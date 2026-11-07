#include <WiFi.h> 
#include <WiFiClientSecure.h> 
#include "certificados.h" 
#include <MQTT.h> 
#include <SPI.h> 
#include <MFRC522.h> 
#include <GFButton.h> 
#include <SQLiteManager.h> 
#include <LittleFS.h> 
#include <ArduinoJson.h> 
#include <uri/UriBraces.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include <NimBLEScan.h>
#include <NimBLEBeacon.h>


// Variáveis ----------------------------------------------------------------------------------------

// Banco de Dados
SQLiteManager banco; 

// RFID
MFRC522 rfid(46, 17); 
MFRC522::MIFARE_Key chaveA = 
{{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

// WiFi e MQTT
WiFiClientSecure conexaoSegura; 
MQTTClient mqtt(1000); 

// Sensor Movimento
GFButton sensor(21); // Atenção: Redefinir posição mais tarde no projeto

// LED
int pinoLED = 2; // Atenção: Redefinir posição mais tarde no projeto

// ESP Token
String token = "1274";

// Preferencias
Preferences preferencias; 
String titulo = "";
String descricao = "";
String url = "";

// Controle do E-paper
bool telaBranca = false;
bool atualizarTela = false;

// Controle do Beacon
unsigned long ultimaDeteccao = 0;
bool beaconProximo = false;

// Tempo sem detectar Beacon para deixar a tela preta
const unsigned long tempoEspera = 5000;

// E-paper
U8G2_FOR_ADAFRUIT_GFX fontes;
GxEPD2_290_T94_V2 modeloTela(10, 14, 15, 16);
GxEPD2_BW<GxEPD2_290_T94_V2, GxEPD2_290_T94_V2::HEIGHT> tela(modeloTela);
BarcodeGFX codigoBarras(tela);
QRCodeGFX qrcode(tela);

// Beacon
BLEScan* scannerBluetooth;
String idDoMeuBeacon = "MEU ID"; // Atenção: Alterar com nosso ID


// Funções ----------------------------------------------------------------------------------------

// Desenho no E-paper
void desenharObra() {
    tela.fillScreen(GxEPD_WHITE);

    // QR Code
    qrcode.setScale(3);
    qrcode.draw(url, 180, 0);

    // Título
    fontes.setFont(u8g2_font_helvR12_te);
    fontes.setFontMode(1);
    fontes.setCursor(5, 20);

    if (titulo.length() > 20) {
        int idx = titulo.lastIndexOf(" ", 20);
        if (idx < 0) {
            idx = 20;
        }
        fontes.print(titulo.substring(0, idx));
        fontes.setCursor(5, 35);
        fontes.print(titulo.substring(idx));

    } else {
        fontes.print(titulo);
    }

    // Descrição
    fontes.setFont(u8g2_font_helvR10_te);
    fontes.setFontMode(1);
    int j = 0;
    for (unsigned int i = 0; i < descricao.length(); i += 25) {
        fontes.setCursor(6, 60 + j);
        fontes.print(descricao.substring(i, i + 25));
        j += 15;
        // Evita ultrapassar a altura da tela
        if (60 + j > 120) {
            break;
        }
    }

    tela.display(false);
    telaBranca = true;

    Serial.println("E-paper atualizado!");
}

void telaPreta() {

    tela.fillScreen(GxEPD_BLACK);
    tela.display(false);

    telaBranca = false;

    Serial.println("E-paper: tela preta");
}

// Beacon 
float calcularDistancia(int potenciaSinal) {
  if (potenciaSinal == 0) { return -1.0; } // erro
    int referencia = -59; // dBm
    float razao = potenciaSinal * 1.0 / referencia;
    float distancia;
    if (razao < 1.0) { distancia = pow(razao, 10); }
    else {distancia = (0.89976) * pow(razao, 7.7095) + 0.111;}
  return (float)(int)(distancia * 10 + 0.5) / 10.0;
}

class MeuRastreador: public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* dispositivoBluetooth) override {
    std::string dadosFabricante = dispositivoBluetooth->getManufacturerData();
    if (dadosFabricante.length() != 25) { return; } // não é Beacon
      NimBLEBeacon oBeacon = NimBLEBeacon();
      oBeacon.setData((uint8_t*)dadosFabricante.data(), (uint8_t)dadosFabricante.length());
      String idDispositivo = oBeacon.getProximityUUID().toString().c_str();
      if (idDispositivo == idDoMeuBeacon) {
        scannerBluetooth->stop();
        int potencia = dispositivoBluetooth->getRSSI();
        float distancia = calcularDistancia(potencia);
        Serial.printf("Beacon a %.1f metros!\n", distancia);
        // Se pessoa perto da obra, E-paper acende com dados da obra
        if (distancia >= 0 && distancia < 3) {
          ultimaDeteccao = millis();
          beaconProximo = true;
        }
      }
    }
};


// Sensor de Movimento
void movimento (GFButton& sensor) { 
  Serial.println("Movimento detectado!"); 
  digitalWrite(pinoLED, LOW); 
} 

void inercia (GFButton& sensor) { 
  Serial.println("Inércia detectada!"); 
  digitalWrite(pinoLED, HIGH); 
} 

// RFID
String lerTextoDoBloco(byte bloco) { 
  byte tamanhoDados = 18; 
  char dados[tamanhoDados]; 
  MFRC522::StatusCode status = rfid.PCD_Authenticate( 
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, 
    bloco, &chaveA, &(rfid.uid) 
  ); 
  if (status != MFRC522::STATUS_OK) { return ""; } 
  status = rfid.MIFARE_Read(bloco, 
              (byte*)dados, &tamanhoDados); 
  if (status != MFRC522::STATUS_OK) { return ""; } 
  dados[tamanhoDados - 2] = '\0'; 
  return String(dados); 
}

// RFID
String lerRFID() { 
  String id = ""; 
  for (byte i = 0; i < rfid.uid.size; i++) { 
    if (i > 0) { 
      id += " "; 
    } 
    if (rfid.uid.uidByte[i] < 0x10) { 
      id += "0"; 
    } 
    id += String(rfid.uid.uidByte[i], HEX); 
  } 
  id.toUpperCase(); 
  return id; 
}


// WiFi e MQTT

void reconectarWiFi() { 
  if (WiFi.status() != WL_CONNECTED) { 
    WiFi.begin("LabIoT", "4n1m4l5@))!!"); 
    Serial.print("Conectando ao WiFi..."); 
    while (WiFi.status() != WL_CONNECTED) { 
      Serial.print("."); 
      delay(1000); 
    } 
    Serial.print("conectado!\nEndereço IP: "); 
    Serial.println(WiFi.localIP()); 
  } 
} 

void recebeuMensagem(String topico, String conteudo) { 
  Serial.println(topico + ": " + conteudo); 
  if (topico == "obra/"+token){
    JsonDocument dados_obras;
    deserializeJson(dados_obras,conteudo);

    // Armazena os dados recebidos da obra em preferencias
    preferencias.putString("titulo", String(dados_obras["titulo"]));
    preferencias.putString("descricao", String(dados_obras["descricao"]));
    preferencias.putString("url", String(dados_obras["url"]));

    titulo = preferencias.getString("titulo", "");
    descricao = preferencias.getString("descricao", "");
    url = preferencias.getString("url", "");

    atualizarTela = true;
  }
}

void reconectarMQTT() { 
  if (!mqtt.connected()) { 
    Serial.print("Conectando MQTT..."); 
    while(!mqtt.connected()) { 
      mqtt.connect("Brownie", "aula", "zowmad-tavQez"); 
      Serial.print("."); 
      delay(1000); 
    } 
    Serial.println(" conectado!"); 
    
    mqtt.subscribe("obra/"+token, 1);
  } 
}


// Setup ----------------------------------------------------------------------------------------

void setup() { 
  Serial.begin(115200); 
  delay(500); 

  // E-paper
  tela.init();
  tela.setRotation(3);
  fontes.begin(tela);
  fontes.setForegroundColor(GxEPD_BLACK);
  tela.fillScreen(GxEPD_BLACK);
  tela.display(false);
  telaBranca = false;

  // Preferences
  preferencias.begin("museu");

  // Carregar os dados salvos
  titulo = preferencias.getString("titulo", "");
  descricao = preferencias.getString("descricao", "");
  url = preferencias.getString("url", "");
  

  // Beacon
  NimBLEDevice::init("");
  scannerBluetooth = NimBLEDevice::getScan();
  scannerBluetooth->setScanCallbacks(new MeuRastreador());

  // Sensor Movimento
  sensor.setPressHandler(inercia); 
  sensor.setReleaseHandler(movimento); 

  // LED
  pinMode(pinoLED, OUTPUT);  
  digitalWrite(pinoLED, HIGH); 

  // RFID
  SPI.begin(); 
  rfid.PCD_Init();

  // WiFi e MQTT
  reconectarWiFi(); 
  conexaoSegura.setCACert(certificado1); 
  mqtt.begin("mqtt.janks.dev.br", 8883, conexaoSegura); 
  mqtt.onMessage(recebeuMensagem); 
  mqtt.setKeepAlive(10); 
  mqtt.setWill("tópico da desconexão", "conteúdo"); 
  reconectarMQTT(); 

  // Banco de Dados
  if (!LittleFS.begin(false)) { 
    Serial.println("Erro LittleFS!"); 
    while (true); 
  } 
  try { 
    banco.open("/littlefs/banco.db"); 
  } 
  catch (const std::exception &e) { 
    Serial.println(e.what()); 
    while (true); 
  }
} 


// loop ----------------------------------------------------------------------------------------

void loop() { 
  // Beacon
  scannerBluetooth->getResults(500, false);

  // Sensor Movimento
  sensor.process(); 

  // RFID
  if (rfid.PICC_IsNewCardPresent() && 
    rfid.PICC_ReadCardSerial()){ 
    String id = lerRFID(); 
    Serial.println("UID: " + id); 
    String texto = lerTextoDoBloco(6); 
    Serial.println("Bloco 6: " + texto); 
     
    rfid.PICC_HaltA(); 
    rfid.PCD_StopCrypto1(); 

    // MQTT
    mqtt.publish("saida/RFID", texto);
  }

  // Se passaram 5 segundos sem detectar o Beacon próximo
  if (beaconProximo &&
      millis() - ultimaDeteccao >= tempoEspera) {
      beaconProximo = false;
      Serial.println("Visitante se afastou!");
  }

  // Se o visitante estiver próximo
  if (beaconProximo) {
    // Desenha apenas se necessário
    if (!telaBranca || atualizarTela) {
        desenharObra();
        atualizarTela = false;
    }
  } 
  else {
    // Se não houver visitante, deixa a tela preta
    if (telaBranca) {
        telaPreta();
    }
  }

  // WiFi e MQTT
  reconectarWiFi(); 
  reconectarMQTT(); 
  mqtt.loop(); 
}
