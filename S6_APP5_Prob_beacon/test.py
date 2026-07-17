import websocket
import json
import time


# Adresse IP de ton ESP32
ESP32_IP = "10.158.122.227"

# Adresse WebSocket
WS_URL = f"ws://{ESP32_IP}/ws"


def on_message(ws, message):
    print("\nMessage reçu :")

    try:
        data = json.loads(message)

        print(json.dumps(
            data,
            indent=4
        ))

        # Exemple de traitement
        if data.get("event") == "found":
            print("✅ Beacon trouvé :", data["uuid"])

        elif data.get("event") == "lost":
            print("❌ Beacon perdu :", data["uuid"])

    except json.JSONDecodeError:
        print("Message non JSON :", message)



def on_error(ws, error):
    print("Erreur WebSocket :", error)



def on_close(ws, close_status_code, close_msg):
    print("Connexion fermée")



def on_open(ws):
    print("Connexion WebSocket établie")
    print("En attente des événements BLE...\n")



if __name__ == "__main__":

    print("Connexion à :", WS_URL)

    ws = websocket.WebSocketApp(
        WS_URL,
        on_open=on_open,
        on_message=on_message,
        on_error=on_error,
        on_close=on_close
    )


    ws.run_forever()