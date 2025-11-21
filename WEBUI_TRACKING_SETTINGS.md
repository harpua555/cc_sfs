# WebUI Tracking Algorithm Settings

The Settings page has been updated to include all tracking algorithm configuration options.

## New Settings Added

### 1. **Tracking Mode** (dropdown)
- **Options:**
  - Mode 0: Cumulative (Legacy - has drift)
  - Mode 1: Windowed (Klipper-style - RECOMMENDED) ⭐
  - Mode 2: EWMA (Memory-efficient)
- **Default:** Mode 1 (Windowed)
- **Description:** Allows switching between the three tracking algorithms
- **Field:** `tracking_mode` (integer: 0, 1, or 2)

### 2. **Detection Grace Period** (number input)
- **Range:** 0-2000ms
- **Step:** 100ms
- **Default:** 500ms
- **Description:** Time to wait after move command before checking for jams
- **Tuning:**
  - Slower prints (< 30mm/s): 750-1000ms
  - Normal prints (40-60mm/s): 500ms (default)
  - Fast prints (> 60mm/s): 300ms
- **Field:** `detection_grace_period_ms`

### 3. **Windowed Mode: Window Size** (number input)
- **Range:** 1000-10000ms
- **Step:** 1000ms
- **Default:** 5000ms (5 seconds)
- **Description:** Sliding time window size for Mode 1 (Windowed)
- **Enabled:** Only when Tracking Mode = 1 (Windowed)
- **Tuning:**
  - Smaller window (3000ms): Faster response, less drift resistance
  - Default (5000ms): Balanced
  - Larger window (7000-10000ms): More drift resistance, slower response
- **Field:** `tracking_window_ms`

### 4. **EWMA Mode: Alpha** (number input)
- **Range:** 0.1-0.5
- **Step:** 0.05
- **Default:** 0.3
- **Description:** Smoothing factor for Mode 2 (EWMA)
- **Enabled:** Only when Tracking Mode = 2 (EWMA)
- **Tuning:**
  - Lower (0.1-0.2): Smoother, less responsive
  - Default (0.3): Balanced
  - Higher (0.4-0.5): More responsive, less smooth
- **Field:** `tracking_ewma_alpha`

## UI Organization

The settings page now has three main sections:

1. **WiFi Settings** (existing)
2. **Device Settings** (existing, includes Detection Length and Movement per Pulse)
3. **Tracking Algorithm Settings** (NEW)
   - Tracking Mode (dropdown with descriptions)
   - Detection Grace Period (always visible)
   - Window Size (disabled when not in Windowed mode)
   - EWMA Alpha (disabled when not in EWMA mode)
4. **Other Settings** (existing)

## Smart Field Disabling

The UI intelligently disables irrelevant settings based on the selected tracking mode:

- **Mode 0 (Cumulative):** Window Size and EWMA Alpha both disabled
- **Mode 1 (Windowed):** EWMA Alpha disabled, Window Size enabled
- **Mode 2 (EWMA):** Window Size disabled, EWMA Alpha enabled

This prevents user confusion and makes it clear which settings apply to which mode.

## Link to Documentation

The Tracking Mode field includes a link to `TRACKING_ALGORITHMS.md` for users who want detailed information about the algorithms, tuning recommendations, and testing procedures.

## API Changes

The settings are sent to the ESP32 via the existing `/update_settings` endpoint with these additional fields:

```json
{
  "detection_grace_period_ms": 500,
  "tracking_mode": 1,
  "tracking_window_ms": 5000,
  "tracking_ewma_alpha": 0.3
}
```

All settings are loaded from `/get_settings` on page load with appropriate defaults if the fields are missing (for backwards compatibility).

## Building the WebUI

After making changes to `webui/src/Settings.tsx`, rebuild the WebUI:

```bash
cd webui
npm install  # If dependencies aren't installed
npm run build
```

This will compile the SolidJS/TypeScript code and place the built files in `webui/dist`, which are served by the ESP32's web server.

## Testing the UI

1. Flash the updated firmware to the ESP32
2. Connect to the web interface
3. Navigate to Settings page
4. Verify all new fields are visible
5. Change Tracking Mode and verify Window Size/EWMA Alpha fields enable/disable appropriately
6. Save settings and verify they persist after page reload
7. Check that the tracking mode is applied (view Status page deficit values or enable verbose logging)
