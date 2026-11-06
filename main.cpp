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
#include <WebServer.h>
#include <uri/UriBraces.h>


// Variáveis ----------------------------------------------------------------------------------------

// WebServer
WebServer servidor(80);

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


// Funções ----------------------------------------------------------------------------------------

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

// Web Server

// Site de Cadastro de Visitantes
void tratarDados() {

  String nome = servidor.arg("nome");
  String email = servidor.arg("email");
  String rfid = servidor.arg("rfid");
  String consentimento = servidor.arg("consentimento");

  Serial.println("Nome: " + nome);
  Serial.println("Email: " + email);
  Serial.println("RFID: " + rfid);
  Serial.println("Consentimento: " + consentimento);

  if (nome.isEmpty() || rfid.isEmpty() ||
      consentimento != "true") {

      servidor.send(
          400,
          "text/plain; charset=utf-8",
          "Dados inválidos"
      );
      return;
  }   

  JsonDocument visitante;

  visitante["nome"] = nome;
  visitante["email"] = email;
  visitante["rfid"] = rfid;
  visitante["consentimento"] = consentimento;

  String payload;
  serializeJson(visitante, payload);

  mqtt.publish("visitante", payload);

  servidor.sendHeader("Location", "/cadastro");
  servidor.send(303);
}

void paginaCadastro() {

    File arquivo = LittleFS.open("/cadastro-visitante.html", "r");

    if (!arquivo) {
        servidor.send(404, "text/plain", "Página não encontrada");
        return;
    }

    servidor.streamFile(arquivo, "text/html");
    arquivo.close();
}


// Site de Cadastro de Obras


void tratarDadosObra() {

  String titulo = servidor.arg("titulo");
  String artista = servidor.arg("artista");
  String ano = servidor.arg("ano");
  String imagem_url = servidor.arg("imagem_url");
  String descricao = servidor.arg("descricao");
  String curiosidades = servidor.arg("curiosidades");
  String token = servidor.arg("token");
  String workId = servidor.arg("workId");

  // Validar autorização antes de alterar dados
  // if (token != TOKEN_ADMIN) {
  //     servidor.send(
  //         403,
  //         "text/plain; charset=utf-8",
  //         "Acesso não autorizado"
  //     );
  //     return;
  // }

  if (titulo.isEmpty()) {
      servidor.send(
          400,
          "text/plain; charset=utf-8",
          "Título obrigatório"
      );
      return;
  }

  Serial.println("=== CADASTRO DE OBRA ===");

  Serial.println("Título: " + titulo);
  Serial.println("Artista: " + artista);
  Serial.println("Ano: " + ano);
  Serial.println("Imagem: " + imagem_url);
  Serial.println("Descrição: " + descricao);
  Serial.println("Curiosidades: " + curiosidades);
  Serial.println("token: " + token);

  // Adiciona no Banco de Dados
  JsonDocument obra;

  obra["titulo"] = titulo;
  obra["artista"] = artista;
  obra["ano"] = ano;
  obra["imagem_url"] = imagem_url;
  obra["descricao"] = descricao;
  obra["curiosidades"] = curiosidades;
  obra["token"] = token;

  String payload;
  serializeJson(obra, payload);

  mqtt.publish("obra", payload);

  // Envia MQTT para o ESP específico da obra
  mqtt.publish("obra/"+token, payload); // Atenção: Adicionar no ESP para quando receber MQTT atualizar os dados

  servidor.send(
      200,
      "text/plain; charset=utf-8",
      "Dados da obra recebidos!"
  );
}


void paginaAdmin() {

    File arquivo = LittleFS.open(
        "/cadastro-obra.html", "r"
    );

    if (!arquivo) {
        servidor.send(
            404,
            "text/plain",
            "Página não encontrada"
        );
        return;
    }

    servidor.streamFile(arquivo, "text/html");
    arquivo.close();
}

// Site com a Obra


void buscarObra() {

    String token = servidor.pathArg(0);

    if (token.isEmpty()) {
        servidor.send(
            400,
            "text/plain",
            "Token inválido"
        );
        return;
    }

    for (char c : token) {
        if (c < '0' || c > '9') {
            servidor.send(
                400,
                "text/plain",
                "Token inválido"
            );
            return;
        }
    }

    try {

        JsonDocument resultado = banco.execute(
            "SELECT titulo, artista, ano, imagem_url, "
            "descricao, curiosidades "
            "FROM cadastros_obras "
            "WHERE token = ? "
            "ORDER BY id DESC LIMIT 1",
            token
        );

        if (!resultado.is<JsonArray>() ||
             resultado.as<JsonArray>().size() == 0) {

            servidor.send(
                404,
                "application/json",
                "{\"erro\":\"Obra não encontrada\"}"
            );

            return;
        }

        String resposta;
        serializeJson(resultado[0], resposta);

        servidor.send(
            200,
            "application/json",
            resposta
        );

    } catch (const std::exception &e) {

        Serial.println(e.what());

        servidor.send(
            500,
            "application/json",
            "{\"erro\":\"Erro ao consultar banco\"}"
        );
    }
}


void paginaObra() {

    File arquivo = LittleFS.open(
        "/obra.html", "r"
    );

    if (!arquivo) {
        servidor.send(
            404,
            "text/plain",
            "Página não encontrada"
        );
        return;
    }

    servidor.streamFile(arquivo, "text/html");
    arquivo.close();
}

void paginaCSS() {
    File arquivo = LittleFS.open("/style.css", "r");

    if (!arquivo) {
        servidor.send(404, "text/plain", "CSS não encontrado");
        return;
    }

    servidor.streamFile(arquivo, "text/css");
    arquivo.close();
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
    
    //mqtt.subscribe("topico1");                // qos = 0  
    //mqtt.subscribe("topico2/+/parametro", 1); // qos = 1  
  } 
}


// Setup ----------------------------------------------------------------------------------------

void setup() { 
  Serial.begin(115200); 
  delay(500); 

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

  // Servidor Web

  //CSS
  servidor.on("/style.css", HTTP_GET, paginaCSS);

  // Site de Cadastro de Visitantes
  servidor.on("/cadastro", HTTP_GET, paginaCadastro);
  servidor.on("/cadastro", HTTP_POST, tratarDados);


  // Site de Cadastro de Obras
  servidor.on("/admin", HTTP_GET, paginaAdmin);
  servidor.on("/admin", HTTP_POST, tratarDadosObra);
  
  // Site com a Obra
  servidor.on("/obra", HTTP_GET, paginaObra);
  servidor.on(UriBraces("/api/obras/posicao/{}"),HTTP_GET,buscarObra);
 
  servidor.begin();

} 


// loop ----------------------------------------------------------------------------------------

void loop() { 
  
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

    // MQTT e Banco de Dados
    try { 
      JsonDocument result = banco.execute(                    // Atenção: Criar banco de dados e ajustar se necessário
      "INSERT INTO saida_RFID (rfid) " 
      "VALUES(?)", texto
      ); 
    } 
    catch (const std::exception &e) { 
      Serial.println(e.what()); 
    }
    
    mqtt.publish("saida/RFID", texto);
  } 
  
  // WiFi e MQTT
  reconectarWiFi(); 
  reconectarMQTT(); 
  mqtt.loop(); 

  servidor.handleClient();
}
