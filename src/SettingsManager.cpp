#include "SettingsManager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <stdlib.h>

#include "Logger.h"

SettingsManager &SettingsManager::getInstance()
{
    static SettingsManager instance;
    return instance;
}

SettingsManager::SettingsManager()
{
    isLoaded                     = false;
    requestWifiReconnect         = false;
    wifiChanged                  = false;
    settings.ap_mode             = false;
    settings.ssid                = "";
    settings.passwd              = "";
    settings.elegooip            = "";
    settings.pause_on_runout     = true;
    settings.start_print_timeout = 10000;
    settings.enabled             = true;
    settings.has_connected       = false;
    settings.expected_deficit_mm      = 8.4f;
    settings.expected_flow_window_ms  = 1500;
    settings.sdcp_loss_behavior       = 2;
    settings.flow_telemetry_stale_ms  = 1000;
    settings.ui_refresh_interval_ms   = 1000;
    settings.zero_deficit_logging           = false;
    settings.packet_flow_logging            = false;
    settings.dev_mode                       = false;
    settings.verbose_logging          = false;
    settings.flow_summary_logging     = false;
    settings.movement_mm_per_pulse    = 1.5f;
}

bool SettingsManager::load()
{
    File file = LittleFS.open("/user_settings.json", "r");
    if (!file)
    {
        logger.log("Settings file not found, using defaults");
        isLoaded = true;
        return false;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError     error = deserializeJson(doc, file);
    file.close();

    if (error)
    {
        logger.log("Settings JSON parsing error, using defaults");
        isLoaded = true;
        return false;
    }

    settings.ap_mode             = doc["ap_mode"] | false;
    settings.ssid                = doc["ssid"] | "";
    settings.passwd              = doc["passwd"] | "";
    settings.elegooip            = doc["elegooip"] | "";
    settings.pause_on_runout     = doc["pause_on_runout"] | true;
    settings.enabled             = doc["enabled"] | true;
    settings.start_print_timeout = doc["start_print_timeout"] | 10000;
    settings.has_connected       = doc["has_connected"] | false;
    settings.expected_deficit_mm =
        doc.containsKey("expected_deficit_mm") ? doc["expected_deficit_mm"].as<float>() : 8.4f;
    settings.expected_flow_window_ms =
        doc.containsKey("expected_flow_window_ms") ? doc["expected_flow_window_ms"].as<int>()
                                                   : 1500;
    settings.sdcp_loss_behavior =
        doc.containsKey("sdcp_loss_behavior") ? doc["sdcp_loss_behavior"].as<int>() : 2;
    settings.flow_telemetry_stale_ms =
        doc.containsKey("flow_telemetry_stale_ms")
            ? doc["flow_telemetry_stale_ms"].as<int>()
            : 1000;
    settings.ui_refresh_interval_ms =
        doc.containsKey("ui_refresh_interval_ms")
            ? doc["ui_refresh_interval_ms"].as<int>()
            : 1000;
    settings.zero_deficit_logging =
        doc.containsKey("zero_deficit_logging")
            ? doc["zero_deficit_logging"].as<bool>()
            : false;
    settings.packet_flow_logging =
        doc.containsKey("packet_flow_logging")
            ? doc["packet_flow_logging"].as<bool>()
            : false;
    settings.dev_mode =
        doc.containsKey("dev_mode") ? doc["dev_mode"].as<bool>() : false;
    settings.verbose_logging = doc.containsKey("verbose_logging")
                                   ? doc["verbose_logging"].as<bool>()
                                   : false;
    settings.flow_summary_logging = doc.containsKey("flow_summary_logging")
                                        ? doc["flow_summary_logging"].as<bool>()
                                        : false;
    settings.movement_mm_per_pulse = doc.containsKey("movement_mm_per_pulse")
                                         ? doc["movement_mm_per_pulse"].as<float>()
                                         : 1.5f;

    isLoaded = true;
    return true;
}

bool SettingsManager::save(bool skipWifiCheck)
{
    String output = toJson(true);

    File file = LittleFS.open("/user_settings.json", "w");
    if (!file)
    {
        logger.log("Failed to open settings file for writing");
        return false;
    }

    if (file.print(output) == 0)
    {
        logger.log("Failed to write settings to file");
        file.close();
        return false;
    }

    file.close();
    logger.log("Settings saved successfully");
    if (!skipWifiCheck && wifiChanged)
    {
        logger.log("WiFi changed, requesting reconnection");
        requestWifiReconnect = true;
        wifiChanged          = false;
    }
    return true;
}

const user_settings &SettingsManager::getSettings()
{
    if (!isLoaded)
    {
        load();
    }
    return settings;
}

String SettingsManager::getSSID()
{
    return getSettings().ssid;
}

String SettingsManager::getPassword()
{
    return getSettings().passwd;
}

bool SettingsManager::isAPMode()
{
    return getSettings().ap_mode;
}

String SettingsManager::getElegooIP()
{
    return getSettings().elegooip;
}

bool SettingsManager::getPauseOnRunout()
{
    return getSettings().pause_on_runout;
}

int SettingsManager::getStartPrintTimeout()
{
    return getSettings().start_print_timeout;
}

bool SettingsManager::getEnabled()
{
    return getSettings().enabled;
}

bool SettingsManager::getHasConnected()
{
    return getSettings().has_connected;
}

float SettingsManager::getExpectedDeficitMM()
{
    return getSettings().expected_deficit_mm;
}

int SettingsManager::getExpectedFlowWindowMs()
{
    return getSettings().expected_flow_window_ms;
}

int SettingsManager::getSdcpLossBehavior()
{
    return getSettings().sdcp_loss_behavior;
}

int SettingsManager::getFlowTelemetryStaleMs()
{
    return getSettings().flow_telemetry_stale_ms;
}

int SettingsManager::getUiRefreshIntervalMs()
{
    return getSettings().ui_refresh_interval_ms;
}

bool SettingsManager::getZeroDeficitLogging()
{
    return getSettings().zero_deficit_logging;
}

bool SettingsManager::getPacketFlowLogging()
{
    return getSettings().packet_flow_logging;
}
bool SettingsManager::getDevMode()
{
    return getSettings().dev_mode;
}

bool SettingsManager::getVerboseLogging()
{
    return getSettings().verbose_logging;
}

bool SettingsManager::getFlowSummaryLogging()
{
    return getSettings().flow_summary_logging;
}

float SettingsManager::getMovementMmPerPulse()
{
    return getSettings().movement_mm_per_pulse;
}

void SettingsManager::setSSID(const String &ssid)
{
    if (!isLoaded)
        load();
    if (settings.ssid != ssid)
    {
        settings.ssid = ssid;
        wifiChanged   = true;
    }
}

void SettingsManager::setPassword(const String &password)
{
    if (!isLoaded)
        load();
    if (settings.passwd != password)
    {
        settings.passwd = password;
        wifiChanged     = true;
    }
}

void SettingsManager::setAPMode(bool apMode)
{
    if (!isLoaded)
        load();
    if (settings.ap_mode != apMode)
    {
        settings.ap_mode = apMode;
        wifiChanged      = true;
    }
}

void SettingsManager::setElegooIP(const String &ip)
{
    if (!isLoaded)
        load();
    settings.elegooip = ip;
}

void SettingsManager::setPauseOnRunout(bool pauseOnRunout)
{
    if (!isLoaded)
        load();
    settings.pause_on_runout = pauseOnRunout;
}

void SettingsManager::setStartPrintTimeout(int timeoutMs)
{
    if (!isLoaded)
        load();
    settings.start_print_timeout = timeoutMs;
}

void SettingsManager::setEnabled(bool enabled)
{
    if (!isLoaded)
        load();
    settings.enabled = enabled;
}

void SettingsManager::setHasConnected(bool hasConnected)
{
    if (!isLoaded)
        load();
    settings.has_connected = hasConnected;
}

void SettingsManager::setExpectedDeficitMM(float value)
{
    if (!isLoaded)
        load();
    settings.expected_deficit_mm = value;
}

void SettingsManager::setExpectedFlowWindowMs(int windowMs)
{
    if (!isLoaded)
        load();
    settings.expected_flow_window_ms = windowMs;
}

void SettingsManager::setSdcpLossBehavior(int behavior)
{
    if (!isLoaded)
        load();
    settings.sdcp_loss_behavior = behavior;
}

void SettingsManager::setFlowTelemetryStaleMs(int staleMs)
{
    if (!isLoaded)
        load();
    settings.flow_telemetry_stale_ms = staleMs;
}

void SettingsManager::setUiRefreshIntervalMs(int intervalMs)
{
    if (!isLoaded)
        load();
    settings.ui_refresh_interval_ms = intervalMs;
}

void SettingsManager::setZeroDeficitLogging(bool enabled)
{
    if (!isLoaded)
        load();
    settings.zero_deficit_logging = enabled;
}

void SettingsManager::setPacketFlowLogging(bool enabled)
{
    if (!isLoaded)
        load();
    settings.packet_flow_logging = enabled;
}
void SettingsManager::setDevMode(bool devMode)
{
    if (!isLoaded)
        load();
    settings.dev_mode = devMode;
}

void SettingsManager::setVerboseLogging(bool verbose)
{
    if (!isLoaded)
        load();
    settings.verbose_logging = verbose;
}

void SettingsManager::setFlowSummaryLogging(bool enabled)
{
    if (!isLoaded)
        load();
    settings.flow_summary_logging = enabled;
}

void SettingsManager::setMovementMmPerPulse(float mmPerPulse)
{
    if (!isLoaded)
        load();
    settings.movement_mm_per_pulse = mmPerPulse;
}

String SettingsManager::toJson(bool includePassword)
{
    String                   output;
    StaticJsonDocument<1024> doc;

    doc["ap_mode"]             = settings.ap_mode;
    doc["ssid"]                = settings.ssid;
    doc["elegooip"]            = settings.elegooip;
    doc["pause_on_runout"]     = settings.pause_on_runout;
    doc["start_print_timeout"] = settings.start_print_timeout;
    doc["enabled"]             = settings.enabled;
    doc["has_connected"]       = settings.has_connected;
    doc["expected_deficit_mm"] = settings.expected_deficit_mm;
    doc["expected_flow_window_ms"] = settings.expected_flow_window_ms;
    doc["sdcp_loss_behavior"]  = settings.sdcp_loss_behavior;
    doc["flow_telemetry_stale_ms"] = settings.flow_telemetry_stale_ms;
    doc["ui_refresh_interval_ms"]  = settings.ui_refresh_interval_ms;
    doc["zero_deficit_logging"]    = settings.zero_deficit_logging;
    doc["packet_flow_logging"] = settings.packet_flow_logging;
    doc["dev_mode"]              = settings.dev_mode;
    doc["verbose_logging"]       = settings.verbose_logging;
    doc["flow_summary_logging"]  = settings.flow_summary_logging;
    doc["movement_mm_per_pulse"] = settings.movement_mm_per_pulse;

    if (includePassword)
    {
        doc["passwd"] = settings.passwd;
    }

    serializeJson(doc, output);
    return output;
}
