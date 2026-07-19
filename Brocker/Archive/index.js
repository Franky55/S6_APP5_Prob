const mqtt = require("mqtt");
const fs = require("fs");
const path = require("path");

const MQTT_BROKER = "mqtt://localhost:1883";
const MQTT_TOPIC = "Archive/DB";
const RECONNECT_DELAY = 3000;
const CSV_PATH = path.join(__dirname, "db.csv");

function initCSV() {
  if (!fs.existsSync(CSV_PATH)) {
    fs.writeFileSync(CSV_PATH, "Date,In,UUID\n", "utf-8");
    console.log("Archive: Created db.csv with headers");
  }
}

function appendToCSV(date, isIn, uuid) {
  const row = `${date},${isIn},${uuid}\n`;
  fs.appendFileSync(CSV_PATH, row, "utf-8");
  console.log("Archive: Appended row ->", row.trim());
}

function connectMQTT() {
  const client = mqtt.connect(MQTT_BROKER);

  client.on("connect", () => {
    console.log("Archive: Connected to MQTT broker");
    client.subscribe(MQTT_TOPIC, (err) => {
      if (err) {
        console.error("Archive: MQTT subscribe error:", err.message);
      } else {
        console.log("Archive: Subscribed to", MQTT_TOPIC);
      }
    });
  });

  client.on("message", (topic, payload) => {
    try {
      const message = JSON.parse(payload.toString());
      const date = new Date().toISOString();
      const uuid = message.uuid;
      const isIn = message.in;

      if (!uuid) {
        console.error("Archive: No UUID in message, skipping");
        return;
      }

      appendToCSV(date, isIn, uuid);

      const ack = JSON.stringify({ status: "ok", uuid: uuid });
      client.publish(MQTT_TOPIC, ack, (err) => {
        if (err) {
          console.error("Archive: Ack publish error:", err.message);
        } else {
          console.log("Archive: Sent ack:", ack);
        }
      });
    } catch (err) {
      console.error("Archive: Failed to parse message:", err.message);
    }
  });

  client.on("error", (err) => {
    console.error("Archive: MQTT error:", err.message);
  });

  client.on("close", () => {
    console.log("Archive: MQTT disconnected. Reconnecting in " + RECONNECT_DELAY + "ms...");
    setTimeout(() => client.reconnect(), RECONNECT_DELAY);
  });
}

initCSV();
connectMQTT();
