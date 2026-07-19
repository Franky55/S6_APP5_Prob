const WebSocket = require("ws");
const mqtt = require("mqtt");

const ESP32_ADDRESS = "ws://192.168.0.185/ws";
const MQTT_BROKER = "mqtt://localhost:1883";
const MQTT_TOPIC = "Archive/DB";
const RECONNECT_DELAY = 3000;
const DEDUP_WINDOW = 30000;

let ws;
let mqttClient;
const published = new Map();

function isDuplicate(uuid, event) {
  const key = uuid + "|" + event;
  const now = Date.now();
  if (published.has(key)) {
    if (now - published.get(key) < DEDUP_WINDOW) {
      return true;
    }
  }
  published.set(key, now);
  return false;
}

function connectMQTT() {
  mqttClient = mqtt.connect(MQTT_BROKER);

  mqttClient.on("connect", () => {
    console.log("Relai: Connected to MQTT broker");
  });

  mqttClient.on("error", (err) => {
    console.error("Relai: MQTT error:", err.message);
  });

  mqttClient.on("close", () => {
    console.log("Relai: MQTT disconnected. Reconnecting in " + RECONNECT_DELAY + "ms...");
    setTimeout(() => mqttClient.reconnect(), RECONNECT_DELAY);
  });
}

function connectWS() {
  ws = new WebSocket(ESP32_ADDRESS);

  ws.on("open", () => {
    console.log("Relai: Connected to ESP32");
  });

  ws.on("message", (data) => {
    try {
      const message = JSON.parse(data.toString());
      console.log("Relai: UUID received:", message.uuid, "| Event:", message.event);

      if (isDuplicate(message.uuid, message.event)) {
        console.log("Relai: Duplicate skipped:", message.uuid, message.event);
        return;
      }

      if (mqttClient && mqttClient.connected) {
        const payload = JSON.stringify({ uuid: message.uuid, event: message.event });
        mqttClient.publish(MQTT_TOPIC, payload, (err) => {
          if (err) {
            console.error("Relai: MQTT publish error:", err.message);
          } else {
            console.log("Relai: Published to MQTT:", payload);
          }
        });
      }
    } catch (err) {
      console.error("Relai: Failed to parse message:", err.message);
    }
  });

  ws.on("error", (err) => {
    console.error("Relai: WebSocket error:", err.message);
  });

  ws.on("close", () => {
    console.log("Relai: Disconnected from ESP32. Reconnecting in " + RECONNECT_DELAY + "ms...");
    setTimeout(connectWS, RECONNECT_DELAY);
  });
}

connectMQTT();
connectWS();
