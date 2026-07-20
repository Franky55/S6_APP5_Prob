const mqtt = require("mqtt");
const http = require("http");
const fs = require("fs");
const path = require("path");

const MQTT_BROKER = "mqtt://localhost:1883";
const MQTT_TOPIC = "Archive/DB";
const RECONNECT_DELAY = 3000;
const HTTP_PORT = 3000;
const CONTROL_ADDRESS = "http://localhost:3001";
const CSV_PATH = path.join(__dirname, "db.csv");
const UI_PATH = path.join(__dirname, "UI/index.html");

function initCSV() {
  if (!fs.existsSync(CSV_PATH)) {
    fs.writeFileSync(CSV_PATH, "Date,Event,UUID\n", "utf-8");
    console.log("Archive: Created db.csv with headers");
  }
}

function appendToCSV(date, event, uuid) {
  const row = `${date},${event},${uuid}\n`;
  fs.appendFileSync(CSV_PATH, row, "utf-8");
  console.log("Archive: Appended row ->", row.trim());
}

function readCSV() {
  if (!fs.existsSync(CSV_PATH)) return [];
  const content = fs.readFileSync(CSV_PATH, "utf-8").trim();
  if (!content) return [];
  const lines = content.split("\n");
  if (lines.length <= 1) return [];
  return lines.slice(1).map((line) => {
    const [date, event, uuid] = line.split(",");
    return { date, event, uuid };
  });
}

function proxyToControl(command, res) {
  const url = CONTROL_ADDRESS + "/led/" + command;
  const req = http.request(url, { method: "POST" }, (proxyRes) => {
    let body = "";
    proxyRes.on("data", (chunk) => { body += chunk; });
    proxyRes.on("end", () => {
      res.writeHead(proxyRes.statusCode, { "Content-Type": "application/json" });
      res.end(body);
    });
  });

  req.on("error", (err) => {
    console.error("Archive: Proxy error:", err.message);
    res.writeHead(500, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ status: "error", message: "Control unreachable: " + err.message }));
  });

  req.end();
}

function startHTTP() {
  const server = http.createServer((req, res) => {
    if (req.url === "/") {
      fs.readFile(UI_PATH, (err, data) => {
        if (err) {
          res.writeHead(500, { "Content-Type": "text/plain" });
          res.end("Error loading UI");
          return;
        }
        res.writeHead(200, { "Content-Type": "text/html" });
        res.end(data);
      });
    } else if (req.url === "/api/entries") {
      const entries = readCSV();
      res.writeHead(200, { "Content-Type": "application/json" });
      res.end(JSON.stringify(entries));
    } else if (req.url === "/led/on" && req.method === "POST") {
      proxyToControl("on", res);
    } else if (req.url === "/led/off" && req.method === "POST") {
      proxyToControl("off", res);
    } else {
      res.writeHead(404, { "Content-Type": "text/plain" });
      res.end("Not found");
    }
  });

  server.listen(HTTP_PORT, () => {
    console.log("Archive: HTTP server running on http://localhost:" + HTTP_PORT);
  });
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
      const event = message.event;

      if (!uuid) {
        console.error("Archive: No UUID in message, skipping");
        return;
      }

      appendToCSV(date, event, uuid);
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
startHTTP();
connectMQTT();
