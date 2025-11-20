#include "WebServer.h"

#include <AsyncJson.h>

#include "ElegooCC.h"
#include "Logger.h"

#define SPIFFS LittleFS

// External reference to firmware version from main.cpp
extern const char *firmwareVersion;
extern const char *chipFamily;

WebServer::WebServer(int port) : server(port) {}

void WebServer::begin()
{
    server.begin();

    // Get settings endpoint
    server.on("/get_settings", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  String jsonResponse = settingsManager.toJson(false);
                  request->send(200, "application/json", jsonResponse);
              });

    server.addHandler(new AsyncCallbackJsonWebHandler(
        "/update_settings",
        [this](AsyncWebServerRequest *request, JsonVariant &json)
        {
            JsonObject jsonObj = json.as<JsonObject>();
            settingsManager.setElegooIP(jsonObj["elegooip"].as<String>());
            settingsManager.setSSID(jsonObj["ssid"].as<String>());
            settingsManager.setElegooIP(jsonObj["elegooip"].as<String>());
            settingsManager.setSSID(jsonObj["ssid"].as<String>());
            if (jsonObj.containsKey("passwd") && jsonObj["passwd"].as<String>().length() > 0)
            {
                settingsManager.setPassword(jsonObj["passwd"].as<String>());
            }
            settingsManager.setAPMode(jsonObj["ap_mode"].as<bool>());
            settingsManager.setPauseOnRunout(jsonObj["pause_on_runout"].as<bool>());
            settingsManager.setEnabled(jsonObj["enabled"].as<bool>());
            settingsManager.setStartPrintTimeout(jsonObj["start_print_timeout"].as<int>());
            if (jsonObj.containsKey("expected_deficit_mm"))
            {
                settingsManager.setExpectedDeficitMM(jsonObj["expected_deficit_mm"].as<float>());
            }
            if (jsonObj.containsKey("expected_flow_window_ms"))
            {
                settingsManager.setExpectedFlowWindowMs(jsonObj["expected_flow_window_ms"].as<int>());
            }
            if (jsonObj.containsKey("sdcp_loss_behavior"))
            {
                settingsManager.setSdcpLossBehavior(jsonObj["sdcp_loss_behavior"].as<int>());
            }
            if (jsonObj.containsKey("dev_mode"))
            {
                settingsManager.setDevMode(jsonObj["dev_mode"].as<bool>());
            }
            if (jsonObj.containsKey("verbose_logging"))
            {
                settingsManager.setVerboseLogging(jsonObj["verbose_logging"].as<bool>());
            }
            if (jsonObj.containsKey("flow_summary_logging"))
            {
                settingsManager.setFlowSummaryLogging(
                    jsonObj["flow_summary_logging"].as<bool>());
            }
            if (jsonObj.containsKey("keep_expected_forever"))
            {
                settingsManager.setKeepExpectedForever(
                    jsonObj["keep_expected_forever"].as<bool>());
            }
            if (jsonObj.containsKey("movement_mm_per_pulse"))
            {
                settingsManager.setMovementMmPerPulse(
                    jsonObj["movement_mm_per_pulse"].as<float>());
            }
            settingsManager.save();
            jsonObj.clear();
            request->send(200, "text/plain", "ok");
        }));

    server.on("/discover_printer", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  String ip;
                  // Use a 3s timeout for discovery via the ElegooCC helper.
                  if (!elegooCC.discoverPrinterIP(ip, 3000))
                  {
                      DynamicJsonDocument jsonDoc(128);
                      jsonDoc["error"] = "No printer found";
                      String jsonResponse;
                      serializeJson(jsonDoc, jsonResponse);
                      request->send(504, "application/json", jsonResponse);
                      return;
                  }

                  settingsManager.setElegooIP(ip);
                  settingsManager.save(true);

                  DynamicJsonDocument jsonDoc(128);
                  jsonDoc["elegooip"] = ip;
                  String jsonResponse;
                  serializeJson(jsonDoc, jsonResponse);
                  request->send(200, "application/json", jsonResponse);
              });

    // Setup ElegantOTA
    ElegantOTA.begin(&server);

    // Sensor status endpoint
    server.on("/sensor_status", HTTP_GET,
              [this](AsyncWebServerRequest *request)
              {
                  // Add elegoo status information using singleton
                  printer_info_t elegooStatus = elegooCC.getCurrentInformation();

                  DynamicJsonDocument jsonDoc(512);
                  jsonDoc["stopped"]        = elegooStatus.filamentStopped;
                  jsonDoc["filamentRunout"] = elegooStatus.filamentRunout;

                  jsonDoc["elegoo"]["mainboardID"]          = elegooStatus.mainboardID;
                  jsonDoc["elegoo"]["printStatus"]          = (int) elegooStatus.printStatus;
                  jsonDoc["elegoo"]["isPrinting"]           = elegooStatus.isPrinting;
                  jsonDoc["elegoo"]["currentLayer"]         = elegooStatus.currentLayer;
                  jsonDoc["elegoo"]["totalLayer"]           = elegooStatus.totalLayer;
                  jsonDoc["elegoo"]["progress"]             = elegooStatus.progress;
                  jsonDoc["elegoo"]["currentTicks"]         = elegooStatus.currentTicks;
                  jsonDoc["elegoo"]["totalTicks"]           = elegooStatus.totalTicks;
                  jsonDoc["elegoo"]["PrintSpeedPct"]        = elegooStatus.PrintSpeedPct;
                  jsonDoc["elegoo"]["isWebsocketConnected"] = elegooStatus.isWebsocketConnected;
                  jsonDoc["elegoo"]["currentZ"]             = elegooStatus.currentZ;
                  jsonDoc["elegoo"]["expectedFilament"]     = elegooStatus.expectedFilamentMM;
                  jsonDoc["elegoo"]["actualFilament"]       = elegooStatus.actualFilamentMM;
                  jsonDoc["elegoo"]["expectedDelta"]        = elegooStatus.lastExpectedDeltaMM;
                  jsonDoc["elegoo"]["telemetryAvailable"]   = elegooStatus.telemetryAvailable;
                  jsonDoc["elegoo"]["currentDeficitMm"]     = elegooStatus.currentDeficitMm;
                  jsonDoc["elegoo"]["deficitThresholdMm"]   = elegooStatus.deficitThresholdMm;
                  jsonDoc["elegoo"]["deficitRatio"]         = elegooStatus.deficitRatio;
                  jsonDoc["elegoo"]["movementPulses"]       = (uint32_t) elegooStatus.movementPulseCount;

                  String jsonResponse;
                  serializeJson(jsonDoc, jsonResponse);
                  request->send(200, "application/json", jsonResponse);
              });

    // Logs endpoint
    server.on("/logs", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  String jsonResponse = logger.getLogsAsJson();
                  request->send(200, "application/json", jsonResponse);
              });

    // Raw text logs endpoint
    server.on("/logs_text", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  String textResponse = logger.getLogsAsText();
                  request->send(200, "text/plain", textResponse);
              });

    // Version endpoint
    server.on("/version", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  DynamicJsonDocument jsonDoc(256);
                  jsonDoc["firmware_version"] = firmwareVersion;
                  jsonDoc["chip_family"]      = chipFamily;
                  jsonDoc["build_date"]       = __DATE__;
                  jsonDoc["build_time"]       = __TIME__;

                  String jsonResponse;
                  serializeJson(jsonDoc, jsonResponse);
                  request->send(200, "application/json", jsonResponse);
              });

    // Serve static files from SPIFFS
    server.serveStatic("/assets/", SPIFFS, "/assets/");
    server.serveStatic("/", SPIFFS, "/");
}

void WebServer::loop()
{
    ElegantOTA.loop();
}
