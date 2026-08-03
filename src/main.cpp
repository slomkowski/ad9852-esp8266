#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ElegantOTA.h>
#include <LittleFS.h>
#include "ad9852.hpp"

#include "secrets.hpp"


static ESP8266WebServer server(80);
static double currentFrequency = 1500000; /* Hz, persists in RAM across requests */

static void handleRoot() {
    auto f = LittleFS.open("/index.html", "r");
    if (!f) {
        server.send(500, "text/plain", "index.html missing from LittleFS");
        return;
    }
    const auto html = f.readString();
    f.close();
    server.send(200, "text/html", html);
}

static void handleApiFreq() {
    server.send(200, "text/plain", String(currentFrequency, 0));
}

static void handleGetMultiplier() {
    server.send(200, "text/plain", String(ad9852::getMultiplier()));
}

static void handleSetMultiplier() {
    if (server.hasArg("mult")) {
        const int m = constrain(server.arg("mult").toInt(), 4, 15);
        ad9852::setMultiplier(m);
        Serial.printf("Multiplier set to %d\n", ad9852::getMultiplier());
    }
    server.send(200, "text/plain", "ok");
}

static void handleSetFreq() {
    if (server.hasArg("freq")) {
        if (const double v = server.arg("freq").toDouble(); v > 0) {
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
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);

    Serial.println('\n');
    Serial.println("Connection established!");
    Serial.print("IP address:\t");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/freq", HTTP_GET, handleApiFreq);
    server.on("/api/freq", HTTP_POST, handleSetFreq);
    server.on("/api/multiplier", HTTP_GET, handleGetMultiplier);
    server.on("/api/multiplier", HTTP_POST, handleSetMultiplier);
    server.onNotFound([]() {
        server.sendHeader("Location", "/");
        server.send(303);
    });

    MDNS.begin("ad9852");

    // Browser-based OTA at http://ad9852.local/update, guarded by the OTA
    // password injected from platformio.ini [ota] section. Pass the credentials
    // to begin() directly — it calls setAuth() internally, so a separate
    // setAuth() before begin() would be overwritten with empty defaults.
    ElegantOTA.begin(&server, "admin", OTA_PASSWORD);

    MDNS.addService("http", "tcp", 80);
    server.begin();
    Serial.println("Ready — http://ad9852.local  (OTA: /update)");
}

void loop() {
    MDNS.update();
    server.handleClient();
    ElegantOTA.loop();
}
