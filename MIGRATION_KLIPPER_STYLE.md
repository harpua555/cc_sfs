# Filament Runout Sensor - Klipper-Style Migration

## Summary of Changes

This refactor implements a **Klipper-inspired filament motion sensor** that is significantly more stable and simpler than the previous multi-mode approach.

### Core Philosophy

**Before:** Complex chunk-based buffer system with multiple parallel tracking modes (delta, total, aggregated)
**After:** Simple cumulative distance tracking with direct comparison (Klipper's approach)

---

## What Changed

### 1. **New FilamentMotionSensor Class**
   - **Location:** `src/FilamentMotionSensor.h`, `src/FilamentMotionSensor.cpp`
   - **Purpose:** Simplified cumulative tracking (no chunks, no time windows)
   - **Key Methods:**
     - `updateExpectedPosition(float totalExtrusionMm)` - Updates expected extrusion from SDCP
     - `addSensorPulse(float mmPerPulse)` - Records sensor movement (2.88mm per pulse)
     - `isJammed(float detectionLengthMm)` - Simple check: `(expected - actual) > threshold`
     - `getDeficit()` - Returns current deficit in mm

### 2. **Simplified Settings**

   **New Settings:**
   - `detection_length_mm` - Distance threshold for jam detection (default: 10mm)
     - Replaces: `expected_deficit_mm` + `expected_flow_window_ms`
   - `movement_mm_per_pulse` - Now defaults to **2.88mm** (your actual sensor spec)

   **Removed Settings:**
   - `expected_flow_window_ms` - ❌ No more time-based hold windows
   - `zero_deficit_logging` - ❌ Simplified logging
   - `use_total_extrusion_deficit` - ❌ Delta mode removed
   - `total_vs_delta_logging` - ❌ Only one mode now
   - `packet_flow_logging` - ❌ Use `verbose_logging` instead
   - `use_total_extrusion_backlog` - ❌ Always enabled (total extrusion only)

### 3. **Code Simplification**

   **Removed:**
   - `FilamentFlowTracker` class (chunk-based buffer)
   - `updateExpectedFilament()` function (telemetry staleness now handled in processFilamentTelemetry)
   - `clearAggregatedBacklog()`, `resetTotalBacklog()`, `recalculateTotalBacklog()` - Aggregated tracking removed
   - `aggregatedDeficitSatisfied()` - No more time-based hold windows
   - All parallel tracking modes (delta accumulation, aggregated backlog, etc.)

   **Simplified:**
   - `processFilamentTelemetry()` - Only reads `TotalExtrusion` (no delta parsing)
   - `checkFilamentMovement()` - Direct distance-based detection (no chunk management)
   - `resetFilamentTracking()` - Just resets the motion sensor
   - State transitions (resume logic) - Simple sensor reset

---

## Migration Path

### For Existing Installations

Your existing `user_settings.json` will **automatically migrate**:

1. **Old setting detected:** `"expected_deficit_mm": 25.0`
   - **Auto-migrates to:** `"detection_length_mm": 25.0`
   - **Note:** 25mm is quite high - consider lowering to 10-15mm for faster detection

2. **Movement calibration:** Update `movement_mm_per_pulse`
   - **Old default:** `1.5mm` or `1.51mm`
   - **New default:** `2.88mm` (your actual sensor spec)
   - **Action:** Test and adjust based on real-world behavior

### Recommended Settings

```json
{
  "detection_length_mm": 10.0,        // 10mm deficit = jam detected
  "movement_mm_per_pulse": 2.88,      // Your sensor's spec
  "verbose_logging": false,            // Set true for debugging
  "flow_summary_logging": true,        // Recommended: 1/sec summary
  "dev_mode": false                    // Set true to prevent actual pauses during testing
}
```

---

## How It Works Now

### Detection Logic (Klipper-Style)

1. **Printer reports total extrusion** via SDCP (`TotalExtrusion` field)
   - On first telemetry: establish baseline
   - On subsequent: update expected position
   - Retractions handled automatically (baseline adjustment)

2. **Sensor reports pulses** (GPIO pin 13 changes state)
   - Each pulse = 2.88mm of actual filament movement
   - Accumulates into `sensorDistanceMm`

3. **Jam detection** (simple distance comparison)
   ```cpp
   deficit = (expectedPosition - baseline) - sensorDistance
   jammed = (deficit > detection_length_mm)
   ```

4. **No time-based logic**
   - No hold windows
   - No chunk expiration
   - Just pure distance-based comparison

### State Machine

- **IDLE** → No tracking
- **PRINTING** → Motion sensor active, deficit accumulates
- **JAM DETECTED** → `deficit > detection_length_mm`
- **PAUSE REQUESTED** → Websocket command sent
- **PAUSED** → Tracking frozen (state latched)
- **RESUME** → Motion sensor reset, fresh baseline

---

## Advantages Over Previous Implementation

| Aspect | Before | After |
|--------|--------|-------|
| **Complexity** | 3 parallel modes, chunk buffer | 1 mode, cumulative tracking |
| **Memory** | 16 chunks × 12 bytes = 192 bytes | 4 floats = 16 bytes |
| **Tuning** | 2 parameters (threshold + window) | 1 parameter (detection_length) |
| **Buffer overflow** | Possible (chunk coalescing) | Impossible (no buffer) |
| **Packet loss** | Delta mode vulnerable | Immune (total extrusion) |
| **Time dependency** | Hold window required | Distance-only |
| **False positives** | Possible with bursty telemetry | Reduced (no time windows) |
| **Debugging** | 16 chunks to inspect | 2 numbers (expected, actual) |
| **Code lines** | ~500 lines | ~150 lines |

---

## Testing & Calibration

### 1. **Verify Sensor Calibration**

Enable verbose logging and watch for sensor pulses:
```json
{
  "verbose_logging": true
}
```

**Expected log output:**
```
Sensor pulse: 0->1, total=2.88mm, pulses=1
Sensor pulse: 1->0, total=5.76mm, pulses=2
Flow: expected=10.0mm sensor=8.64mm deficit=1.36mm threshold=10.0mm ratio=0.14 pulses=3 jammed=0
```

**Check:**
- Does `pulses` increment with filament movement?
- Does `sensor` distance match `pulses × movement_mm_per_pulse`?
- If not, adjust `movement_mm_per_pulse`

### 2. **Tune Detection Threshold**

Start with `detection_length_mm: 10.0` and observe:

**Too sensitive?** (false positives during normal printing)
- **Increase:** Try 15mm or 20mm
- **Symptom:** Pauses when filament is actually moving

**Too lenient?** (jams not detected quickly enough)
- **Decrease:** Try 7mm or 8mm (Klipper default is 7mm)
- **Symptom:** Several cm of filament requested before pause

**Sweet spot:** Detect jams within first few mm of deficit, no false positives during normal extrusion

### 3. **Test Scenarios**

1. **Normal print** - No false positives
   - Watch `deficit` in logs - should stay near 0mm
   - Flow ratio should stay < 0.5 during normal operation

2. **Manual jam** - Pinch filament before sensor
   - Should detect within `detection_length_mm` of requested extrusion
   - Log should show: `"Filament jam detected! Expected X.Xmm, sensor Y.Ymm, deficit Z.Zmm"`

3. **Resume after jam** - Clear jam, resume print
   - Should reset tracking: `"Motion sensor reset (resume after pause)"`
   - Deficit should start from 0mm

4. **Physical runout** - Remove filament from spool
   - Physical sensor (pin 12) should trigger
   - Independent of motion sensor logic

---

## Logging Output

### Normal Operation (verbose_logging: true)
```
Telemetry: total=45.50mm, sensor=44.32mm, deficit=1.18mm, pulses=15
Flow: expected=45.50mm sensor=44.32mm deficit=1.18mm threshold=10.0mm ratio=0.12 pulses=15 jammed=0
```

### Jam Detected
```
Filament jam detected! Expected 55.30mm, sensor=44.32mm, deficit=10.98mm (threshold 10.0mm)
Pausing print, detected filament runout or stopped
```

### Summary Mode (flow_summary_logging: true)
```
Flow summary: expected=120.45mm sensor=118.08mm deficit=2.37mm threshold=10.0mm ratio=0.24 pulses=41
```

---

## Troubleshooting

### Issue: Sensor never pulses
**Check:**
- Is pin 13 correctly connected to your SFS sensor?
- Is sensor powered?
- Enable `verbose_logging` and manually rotate sensor - should see state changes

### Issue: Too many false positives
**Solutions:**
- Increase `detection_length_mm` (try 15mm → 20mm)
- Verify `movement_mm_per_pulse` is correct (should be 2.88mm)
- Check if sensor is skipping pulses (mechanical issue)

### Issue: Jams not detected
**Solutions:**
- Decrease `detection_length_mm` (try 10mm → 7mm)
- Check if SDCP telemetry is available: `Flow: expected=` should increment during printing
- Verify physical sensor is triggering on actual filament movement

### Issue: Settings not saving
**Check:**
- `user_settings.json` permissions
- Web UI shows new `detection_length_mm` field (not old fields)
- After save, restart ESP32 to reload settings

---

## Next Steps

1. ✅ **Flash firmware** with new code
2. ✅ **Verify settings migrate** automatically
3. ⚙️ **Test with dev_mode: true** first (prevents actual pauses)
4. 📊 **Watch logs** during test print to verify sensor pulses
5. 🔧 **Tune `detection_length_mm`** based on observed behavior
6. ✅ **Set dev_mode: false** once confident
7. 🎯 **Run real print** with jam detection active

---

## Key Files Modified

### Core Implementation
- `src/FilamentMotionSensor.h` - New sensor class (header)
- `src/FilamentMotionSensor.cpp` - New sensor class (implementation)
- `src/ElegooCC.h` - Updated to use FilamentMotionSensor
- `src/ElegooCC.cpp` - Simplified tracking logic
- `src/SettingsManager.h` - New unified settings schema
- `src/SettingsManager.cpp` - Migration logic from old settings

### Configuration
- `data/user_settings.json` - Updated default values

### Removed (can be deleted)
- `src/FilamentFlowTracker.h` - ❌ Old chunk-based tracker
- `src/FilamentFlowTracker.cpp` - ❌ Old chunk-based tracker
- `src/backup/FilamentFlowTracker_windowed.cpp.old` - ❌ Historical backup

---

## Philosophy & Design Decisions

This refactor follows **Klipper's filament_motion_sensor.py** design:

1. **Distance over time** - Filament physics are distance-based, not time-based
2. **Total over delta** - Absolute reference immune to packet loss
3. **Simplicity over features** - One reliable mode beats three unreliable ones
4. **Cumulative tracking** - No buffers, no expiration, just math
5. **Deterministic behavior** - Same input always produces same output

**Result:** More stable, easier to debug, and proven by Klipper's battle-tested implementation.

---

## Questions?

**Logs not making sense?** Enable `verbose_logging` and `flow_summary_logging`

**Want to understand the deficit calculation?**
```cpp
expected = TotalExtrusion - baseline  // From SDCP
actual = sensorPulses × 2.88mm        // From GPIO sensor
deficit = expected - actual            // Should be near zero
jammed = deficit > detection_length   // Simple threshold
```

**Why 2.88mm per pulse?** That's your sensor's spec - each mechanical pulse represents 2.88mm of filament movement through the sensor.

**Why 10mm threshold?** Balance between fast detection (lower = faster) and false positive avoidance (higher = more lenient). 10mm is 3-4 pulses of deficit before pausing.
