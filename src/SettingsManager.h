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
    float  detection_length_mm;          // Renamed from expected_deficit_mm (Klipper-style)
    int    detection_grace_period_ms;    // Grace period after move command before checking jams
    int    tracking_mode;                // 0=Cumulative, 1=Windowed, 2=EWMA
    int    tracking_window_ms;           // Window size for windowed mode (milliseconds)
    float  tracking_ewma_alpha;          // EWMA smoothing factor (0.0-1.0)
    int    sdcp_loss_behavior;
    int    flow_telemetry_stale_ms;
    int    ui_refresh_interval_ms;
    bool   dev_mode;
    bool   verbose_logging;
    bool   flow_summary_logging;
    float  movement_mm_per_pulse;

    // Deprecated settings (kept for backwards compatibility during migration)
    float  expected_deficit_mm;        // DEPRECATED: use detection_length_mm
    int    expected_flow_window_ms;    // DEPRECATED: distance-based detection only
    bool   zero_deficit_logging;       // DEPRECATED: simplified logging
    bool   use_total_extrusion_deficit;    // DEPRECATED: only total mode supported
    bool   total_vs_delta_logging;         // DEPRECATED: delta mode removed
    bool   packet_flow_logging;            // DEPRECATED: use verbose_logging
    bool   use_total_extrusion_backlog;    // DEPRECATED: always enabled now
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
    float  getDetectionLengthMM();          // New unified setting
    int    getDetectionGracePeriodMs();     // Grace period for look-ahead moves
    int    getTrackingMode();               // Tracking algorithm mode
    int    getTrackingWindowMs();           // Window size for windowed mode
    float  getTrackingEwmaAlpha();          // EWMA smoothing factor
    int    getSdcpLossBehavior();
    int    getFlowTelemetryStaleMs();
    int    getUiRefreshIntervalMs();
    bool   getDevMode();
    bool   getVerboseLogging();
    bool   getFlowSummaryLogging();
    float  getMovementMmPerPulse();

    // Deprecated getters (for backwards compatibility)
    float  getExpectedDeficitMM();     // DEPRECATED: use getDetectionLengthMM()
    int    getExpectedFlowWindowMs();  // DEPRECATED: returns 0

    void setSSID(const String &ssid);
    void setPassword(const String &password);
    void setAPMode(bool apMode);
    void setElegooIP(const String &ip);
    void setPauseOnRunout(bool pauseOnRunout);
    void setStartPrintTimeout(int timeoutMs);
    void setEnabled(bool enabled);
    void setHasConnected(bool hasConnected);
    void setDetectionLengthMM(float value);        // New unified setter
    void setDetectionGracePeriodMs(int periodMs);  // Grace period setter
    void setTrackingMode(int mode);                // Tracking algorithm setter
    void setTrackingWindowMs(int windowMs);        // Window size setter
    void setTrackingEwmaAlpha(float alpha);        // EWMA alpha setter
    void setSdcpLossBehavior(int behavior);
    void setFlowTelemetryStaleMs(int staleMs);
    void setUiRefreshIntervalMs(int intervalMs);
    void setDevMode(bool devMode);
    void setVerboseLogging(bool verbose);
    void setFlowSummaryLogging(bool enabled);
    void setMovementMmPerPulse(float mmPerPulse);

    // Deprecated setters (for backwards compatibility)
    void setExpectedDeficitMM(float value);    // DEPRECATED: use setDetectionLengthMM()

    String toJson(bool includePassword = true);
};

#define settingsManager SettingsManager::getInstance()

#endif
