#include <Arduino.h>
#include <ArduinoJson.h>

#ifndef SETTINGS_DATA_H
#define SETTINGS_DATA_H

struct user_settings
{
    String ssid;
    String passwd;
    bool   ap_mode;
    String elegooip;
    bool   pause_on_runout;
    int    start_print_timeout;
    bool   enabled;
    bool   has_connected;
    float  expected_deficit_mm;
    int    expected_flow_window_ms;
    int    sdcp_loss_behavior;
    int    flow_telemetry_stale_ms;
    int    ui_refresh_interval_ms;
    bool   zero_deficit_logging;
    bool   dev_mode;
    bool   verbose_logging;
    bool   flow_summary_logging;
    float  movement_mm_per_pulse;
};

class SettingsManager
{
   private:
    user_settings settings;
    bool          isLoaded;
    bool          wifiChanged;

    SettingsManager();

    SettingsManager(const SettingsManager &)            = delete;
    SettingsManager &operator=(const SettingsManager &) = delete;

   public:
    static SettingsManager &getInstance();

    // Flag to request WiFi reconnection with new credentials
    bool requestWifiReconnect;

    bool load();
    bool save(bool skipWifiCheck = false);

    //  (loads if not already loaded)
    const user_settings &getSettings();

    String getSSID();
    String getPassword();
    bool   isAPMode();
    String getElegooIP();
    bool   getPauseOnRunout();
    int    getStartPrintTimeout();
    bool   getEnabled();
    bool   getHasConnected();
    float  getExpectedDeficitMM();
    int    getExpectedFlowWindowMs();
    int    getSdcpLossBehavior();
    int    getFlowTelemetryStaleMs();
    int    getUiRefreshIntervalMs();
    bool   getZeroDeficitLogging();
    bool   getDevMode();
    bool   getVerboseLogging();
    bool   getFlowSummaryLogging();
    float  getMovementMmPerPulse();

    void setSSID(const String &ssid);
    void setPassword(const String &password);
    void setAPMode(bool apMode);
    void setElegooIP(const String &ip);
    void setPauseOnRunout(bool pauseOnRunout);
    void setStartPrintTimeout(int timeoutMs);
    void setEnabled(bool enabled);
    void setHasConnected(bool hasConnected);
    void setExpectedDeficitMM(float value);
    void setExpectedFlowWindowMs(int windowMs);
    void setSdcpLossBehavior(int behavior);
    void setFlowTelemetryStaleMs(int staleMs);
    void setUiRefreshIntervalMs(int intervalMs);
    void setZeroDeficitLogging(bool enabled);
    void setDevMode(bool devMode);
    void setVerboseLogging(bool verbose);
    void setFlowSummaryLogging(bool enabled);
    void setMovementMmPerPulse(float mmPerPulse);

    String toJson(bool includePassword = true);
};

#define settingsManager SettingsManager::getInstance()

#endif
