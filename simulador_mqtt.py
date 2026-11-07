
import json
import os
import ssl
import paho.mqtt.client as mqtt

# Mesmo broker utilizado pelo Flask
BROKER = "mqtt.janks.dev.br"
PORTA = 8883
USUARIO = "aula"
SENHA = "zowmad-tavQez"

# Simulando o ESP32 da obra 1274
TOKEN_ESP = "1274"

TOPICO = f"obra/{TOKEN_ESP}"


def ao_conectar(client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        print("Conectado ao broker MQTT!")
        client.subscribe(TOPICO, qos=1)
        print(f"Aguardando mensagens em: {TOPICO}")
    else:
        print(f"Falha na conexão: {reason_code}")


def ao_receber(client, userdata, mensagem):
    print("\nNova mensagem recebida!")

    dados = json.loads(mensagem.payload.decode("utf-8"))

    print(json.dumps(dados, indent=4, ensure_ascii=False))

    if dados.get("status") == "sem_obra":
        print("E-paper: nenhuma obra cadastrada.")
    else:
        print("E-paper atualizado!")
        print("Título:", dados.get("titulo"))
        print("Artista:", dados.get("artista"))
        print("Ano:", dados.get("ano"))
        print("QR Code:", dados.get("url"))


cliente = mqtt.Client(
    mqtt.CallbackAPIVersion.VERSION2,
    client_id="simulador-esp32-1274"
)

cliente.username_pw_set(USUARIO, SENHA)
cliente.tls_set(cert_reqs=ssl.CERT_REQUIRED)

cliente.on_connect = ao_conectar
cliente.on_message = ao_receber

cliente.connect(BROKER, PORTA, 60)

print("Iniciando simulador ESP32...")
cliente.loop_forever()
