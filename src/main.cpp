#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "ad9852.h"

#define AP_SSID "AD9852-DDS"
/* Leave AP_PASS empty for an open network, or set a WPA2 password (min 8 chars) */
#define AP_PASS ""

static ESP8266WebServer server(80);
static uint32_t current_freq = 10000; /* Hz, persists in RAM across requests */

/* ---- HTML page stored in flash (PROGMEM) to save heap ------------------- */
static constexpr char HTML[] PROGMEM = R"html(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AD9852 DDS</title>
<style>
  *{box-sizing:border-box}
  body{font-family:Arial,sans-serif;background:#f4f4f4;margin:0;padding:30px}
  .card{background:#fff;border-radius:8px;max-width:380px;margin:0 auto;padding:28px 32px;box-shadow:0 2px 8px rgba(0,0,0,.15)}
  h1{margin:0 0 6px;font-size:1.4em;color:#222}
  .sub{color:#888;font-size:.85em;margin:0 0 24px}
  .cur{font-size:1.1em;margin-bottom:20px;color:#333}
  .cur span{font-weight:bold;color:#0078d7}
  label{display:block;margin-bottom:6px;font-size:.9em;color:#555}
  .row{display:flex;gap:8px}
  input[type=number]{flex:1;padding:9px 12px;font-size:1em;border:1px solid #ccc;border-radius:4px;outline:none}
  input[type=number]:focus{border-color:#0078d7}
  button{padding:9px 20px;font-size:1em;background:#0078d7;color:#fff;border:none;border-radius:4px;cursor:pointer;white-space:nowrap}
  button:hover{background:#005fa3}
  .hint{font-size:.78em;color:#aaa;margin-top:10px}
</style>
</head>
<body>
<div class="card">
  <h1>AD9852 DDS</h1>
  <p class="sub">SYSCLK = 66.667 MHz &nbsp;&bull;&nbsp; max &asymp; 26.7 MHz</p>
  <div class="cur">Output: <span>%FREQ% Hz</span></div>
  <form action="/set" method="get">
    <label for="f">Frequency (Hz)</label>
    <div class="row">
      <input id="f" type="number" name="freq"
             min="1" max="26666800" step="1"
             value="%FREQ%" required>
      <button type="submit">Set</button>
    </div>
  </form>
  <p class="hint">Range: 1 Hz &ndash; 26 666 800 Hz</p>
</div>
</body>
</html>
)html";

/* ---- HTTP handlers ------------------------------------------------------- */

static void handle_root() {
    String page = FPSTR(HTML);
    page.replace("%FREQ%", String(current_freq));
    server.send(200, "text/html", page);
}

static void handle_set() {
    if (server.hasArg("freq")) {
        long v = server.arg("freq").toInt();
        if (v > 0) {
            current_freq = (uint32_t) v;
            ad9852_set_freq(current_freq);
            Serial.printf("Frequency set to %u Hz\n", current_freq);
        }
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

/* ---- Arduino entry points ------------------------------------------------ */

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\nAD9852 DDS + HTTP control");

    ad9852_init();
    ad9852_set_freq(current_freq);
    Serial.printf("DDS init OK — %u Hz\n", current_freq);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(
        IPAddress(10, 78, 0, 1), /* AP / gateway IP */
        IPAddress(10, 78, 0, 1), /* gateway */
        IPAddress(255, 255, 255, 0)
    );
    WiFi.softAP(AP_SSID, AP_PASS[0] ? AP_PASS : nullptr);
    Serial.printf("WiFi AP: SSID=\"%s\"  IP=%s\n",
                  AP_SSID, WiFi.softAPIP().toString().c_str());

    server.on("/", HTTP_GET, handle_root);
    server.on("/set", HTTP_GET, handle_set);
    server.onNotFound([]() {
        server.sendHeader("Location", "/");
        server.send(303);
    });
    server.begin();
    Serial.println("HTTP server ready on port 80");
}

void loop() {
    server.handleClient();
}
