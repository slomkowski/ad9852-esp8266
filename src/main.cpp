#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include "ad9852.hpp"

#include "secrets.hpp"


static ESP8266WebServer server(80);
static double currentFrequency = 100000; /* Hz, persists in RAM across requests */

/* ---- HTTP handlers ------------------------------------------------------- */

static void handle_root() {
    File f = LittleFS.open("/index.html", "r");
    if (!f) {
        server.send(500, "text/plain", "index.html missing from LittleFS");
        return;
    }
    String html = f.readString();
    f.close();
    server.send(200, "text/html", html);
}

static void handle_api_freq() {
    server.send(200, "text/plain", String(currentFrequency, 0));
}

static void handle_set_freq() {
    if (server.hasArg("freq")) {
        double v = server.arg("freq").toDouble();
        if (v > 0) {
            currentFrequency = v;
            ad9852::setFrequency(currentFrequency);
            Serial.printf("Frequency set to %f Hz\n", currentFrequency);
        }
    }
    server.send(200, "text/plain", "ok");
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\nAD9852 DDS + HTTP control");

    LittleFS.begin();

    ad9852::init();
    ad9852::setFrequency(currentFrequency);
    Serial.printf("DDS init OK — %f Hz\n", currentFrequency);

    Serial.print("1");
    WiFi.mode(WIFI_STA);

    Serial.print("2");
    WiFi.hostname("AD9852-DDS");

    Serial.print("3");
    WiFi.begin(secrets::wifiSsid, secrets::wifiPassword);

    Serial.print("Connecting to Wi-Fi ");
    Serial.print(WiFi.SSID());

    Serial.println(" ...");

    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(++i);
        Serial.print(' ');
    }

    Serial.println('\n');
    Serial.println("Connection established!");
    Serial.print("IP address:\t");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, handle_root);
    server.on("/api/freq", HTTP_GET, handle_api_freq);
    server.on("/api/freq", HTTP_POST, handle_set_freq);
    server.onNotFound([]() {
        server.sendHeader("Location", "/");
        server.send(303);
    });

    server.begin();
}

void loop() {
    server.handleClient();
}
