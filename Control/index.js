const http = require("http");
const dgram = require("dgram");
const fs = require("fs");
const path = require("path");

const HTTP_PORT = 3001;
const ESP32_IP = "192.168.0.185";
const COAP_PORT = 5683;
const UI_PATH = path.join(__dirname, "UI/index.html");

const udpClient = dgram.createSocket("udp4");

function sendCoAP(command) {
  return new Promise((resolve, reject) => {
    const message = Buffer.from(command);
    udpClient.send(message, 0, message.length, COAP_PORT, ESP32_IP, (err) => {
      if (err) {
        reject(err);
        return;
      }
      console.log("Control: Sent CoAP command:", command);
    });

    const timeout = setTimeout(() => {
      udpClient.removeListener("message", onMessage);
      reject(new Error("CoAP timeout"));
    }, 3000);

    function onMessage(msg) {
      clearTimeout(timeout);
      udpClient.removeListener("message", onMessage);
      resolve(msg.toString());
    }

    udpClient.once("message", onMessage);
  });
}

function startHTTP() {
  const server = http.createServer((req, res) => {

    if (req.url === "/" && req.method === "GET") {
      fs.readFile(UI_PATH, (err, data) => {
        if (err) {
          res.writeHead(500, { "Content-Type": "text/plain" });
          res.end("Error loading UI");
          return;
        }
        res.writeHead(200, { "Content-Type": "text/html" });
        res.end(data);
      });
    } else if (req.url === "/led/on" && req.method === "POST") { // LED ON
      sendCoAP("on")
        .then((response) => {
          console.log("Control: ESP32 responded:", response);
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ status: "ok", led: "on", response: response }));
        })
        .catch((err) => {
          console.error("Control: CoAP error:", err.message);
          res.writeHead(500, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ status: "error", message: err.message }));
        });
    } else if (req.url === "/led/off" && req.method === "POST") { // LED OFF
      sendCoAP("off")
        .then((response) => {
          console.log("Control: ESP32 responded:", response);
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ status: "ok", led: "off", response: response }));
        })
        .catch((err) => {
          console.error("Control: CoAP error:", err.message);
          res.writeHead(500, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ status: "error", message: err.message }));
        });
    } else {
      res.writeHead(404, { "Content-Type": "text/plain" });
      res.end("Not found");
    }
  });

  server.listen(HTTP_PORT, () => {
    console.log("Control: HTTP server running on http://localhost:" + HTTP_PORT);
  });
}

startHTTP();
