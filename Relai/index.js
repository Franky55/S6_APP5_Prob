const WebSocket = require("ws");

const ESP32_ADDRESS = "ws://192.168.1.100:81";
const RECONNECT_DELAY = 3000;

let ws;

function connect() {
  ws = new WebSocket(ESP32_ADDRESS);

  ws.on("open", () => {
    console.log("Relai: Connected to ESP32");
  });

  ws.on("message", (data) => {
    try {
      const message = JSON.parse(data.toString());
      console.log("Relai: UUID received:", message.uuid);
    } catch (err) {
      console.error("Relai: Failed to parse message:", err.message);
    }
  });

  ws.on("error", (err) => {
    console.error("Relai: WebSocket error:", err.message);
  });

  ws.on("close", () => {
    console.log("Relai: Disconnected from ESP32. Reconnecting in " + RECONNECT_DELAY + "ms...");
    setTimeout(connect, RECONNECT_DELAY);
  });
}

connect();
