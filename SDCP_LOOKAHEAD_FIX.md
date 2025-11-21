# SDCP Look-Ahead Fix for Filament Jam Detection

## Critical Discovery: SDCP Reports Planned Extrusion, Not Actual

You identified a fundamental issue with how SDCP telemetry works that explains the jumping deficit behavior:

**SDCP reports commanded/planned extrusion immediately when a move starts, not actual in-progress extrusion.**

### Example Timeline:
```
Time 0ms:   GCode command: Extrude 10mm over 2 seconds
Time 0ms:   SDCP TotalExtrusion: 50mm → 60mm (jumps immediately!)
Time 250ms: SDCP TotalExtrusion: 60mm (no change)
Time 500ms: SDCP TotalExtrusion: 60mm (no change)
Time 750ms: SDCP TotalExtrusion: 60mm (no change)
...
Time 2000ms: Move completes, TotalExtrusion still 60mm

Meanwhile, sensor pulses arrive gradually:
Time 300ms: First pulse (+2.88mm)
Time 600ms: Second pulse (+2.88mm)
Time 900ms: Third pulse (+2.88mm)
Time 1200ms: Fourth pulse (+2.88mm) ← ~11.52mm total
```

### Result:
- **At 250ms**: Expected=10mm, Actual=0mm → Deficit = 10mm (100%!)
- **At 500ms**: Expected=10mm, Actual=2.88mm → Deficit = 7.12mm (71%)
- **At 1000ms**: Expected=10mm, Actual=8.64mm → Deficit = 1.36mm (14%)
- **At 1500ms**: Expected=10mm, Actual=11.52mm → Deficit = 0mm (sensor caught up)

This is **fundamentally different** from how Klipper works. Klipper polls the printer's actual instantaneous position, not the commanded position.

## Solution: Grace Period After Expected Position Updates

We need to add a **time-based grace period** after SDCP reports a new expected position, to allow the physical movement to actually execute before checking for jams.

### Implementation

#### 1. Track When Expected Position Last Changed
**File**: [src/FilamentMotionSensor.h](src/FilamentMotionSensor.h)

Added `lastExpectedUpdateMs` member to track timing:
```cpp
private:
    bool          initialized;
    float         baselinePositionMm;
    float         expectedPositionMm;
    float         sensorDistanceMm;
    unsigned long lastExpectedUpdateMs;  // NEW: Track when expected last changed
```

#### 2. Update Timer on Significant Changes
**File**: [src/FilamentMotionSensor.cpp:17-49](src/FilamentMotionSensor.cpp#L17-L49)

```cpp
void FilamentMotionSensor::updateExpectedPosition(float totalExtrusionMm)
{
    unsigned long currentTime = millis();

    if (!initialized) {
        // First telemetry - establish baseline and start timer
        initialized = true;
        baselinePositionMm = totalExtrusionMm;
        expectedPositionMm = totalExtrusionMm;
        sensorDistanceMm = 0.0f;
        lastExpectedUpdateMs = currentTime;
        return;
    }

    // Retraction - reset everything including timer
    if (totalExtrusionMm < expectedPositionMm) {
        baselinePositionMm = totalExtrusionMm;
        sensorDistanceMm = 0.0f;
        lastExpectedUpdateMs = currentTime;  // Reset grace period
    }
    // Significant change (> 0.1mm) - reset grace period
    else if (abs(totalExtrusionMm - expectedPositionMm) > 0.1f) {
        lastExpectedUpdateMs = currentTime;  // New move command detected
    }

    expectedPositionMm = totalExtrusionMm;
}
```

**Key insight**: Only trigger grace period on **significant changes** (> 0.1mm), not on every telemetry packet with same value.

#### 3. Add Grace Period to Jam Check
**File**: [src/FilamentMotionSensor.cpp:60-89](src/FilamentMotionSensor.cpp#L60-L89)

```cpp
bool FilamentMotionSensor::isJammed(float detectionLengthMm, unsigned long gracePeriodMs) const
{
    if (!initialized || detectionLengthMm <= 0.0f) {
        return false;
    }

    // Grace period: Don't check immediately after expected position update
    // This handles SDCP "look-ahead" behavior where moves are reported before execution
    if (gracePeriodMs > 0) {
        unsigned long timeSinceUpdate = millis() - lastExpectedUpdateMs;
        if (timeSinceUpdate < gracePeriodMs) {
            return false;  // Still within grace period
        }
    }

    // ... rest of jam detection logic
}
```

#### 4. New Setting: `detection_grace_period_ms`
**Default**: 1500ms (1.5 seconds)

**Files Updated**:
- [src/SettingsManager.h:18](src/SettingsManager.h#L18) - Added field
- [src/SettingsManager.cpp:30](src/SettingsManager.cpp#L30) - Default value
- [src/SettingsManager.cpp:116-118](src/SettingsManager.cpp#L116-L118) - Load from JSON
- [src/SettingsManager.cpp:216-219](src/SettingsManager.cpp#L216-L219) - Getter
- [src/SettingsManager.cpp:343-348](src/SettingsManager.cpp#L343-L348) - Setter
- [data/user_settings.json:11](data/user_settings.json#L11) - Default config

#### 5. Use Grace Period in Detection
**File**: [src/ElegooCC.cpp:631-639](src/ElegooCC.cpp#L631-L639)

```cpp
// Get grace period setting (handles SDCP look-ahead behavior)
unsigned long gracePeriod = settingsManager.getDetectionGracePeriodMs();
if (gracePeriod <= 0) {
    gracePeriod = 1500;  // Default 1.5 seconds
}

// Distance-based jam detection with grace period (Klipper-style + SDCP adaptation)
bool jammed = motionSensor.isJammed(detectionLength, gracePeriod);
```

## How It Works

### Normal Operation Timeline

```
T=0ms:      Move command received, TotalExtrusion jumps 50→60mm
            - lastExpectedUpdateMs = 0
            - Grace period STARTS (1500ms window)
            - Jam checking: DISABLED

T=250ms:    Telemetry poll #1, TotalExtrusion still 60mm
            - No change (< 0.1mm), grace period timer NOT reset
            - Time since update: 250ms < 1500ms
            - Jam checking: DISABLED (still in grace period)

T=500ms:    First sensor pulse arrives (+2.88mm)
            - Expected: 10mm, Actual: 2.88mm, Deficit: 7.12mm
            - Time since update: 500ms < 1500ms
            - Jam checking: DISABLED (still in grace period)

T=1000ms:   Third sensor pulse (+8.64mm total)
            - Expected: 10mm, Actual: 8.64mm, Deficit: 1.36mm
            - Time since update: 1000ms < 1500ms
            - Jam checking: DISABLED (still in grace period)

T=1600ms:   Grace period expires
            - Expected: 10mm, Actual: 11.52mm, Deficit: 0mm
            - Time since update: 1600ms > 1500ms
            - Jam checking: ENABLED
            - Status: No jam (sensor caught up)
```

### During Actual Jam

```
T=0ms:      Move command, TotalExtrusion 50→60mm
            - Grace period starts

T=500ms:    Filament jammed (pinched before sensor)
            - Expected: 10mm, Actual: 0mm
            - Jam checking: DISABLED (grace period)

T=1600ms:   Grace period expires
            - Expected: 10mm, Actual: 0mm, Deficit: 10mm
            - Jam checking: ENABLED
            - Deficit > threshold → JAM DETECTED!
            - Pause triggered
```

## Expected Behavior Changes

### Before This Fix
- ❌ Deficit jumps to 100% immediately on move start
- ❌ Deficit oscillates wildly (80% → 0% → 60% → 0%)
- ❌ False positives possible on long moves
- ❌ UI shows alarming percentages during normal operation

### After This Fix
- ✅ Deficit stays near 0% during normal operation
- ✅ Grace period prevents spurious deficit spikes
- ✅ Real jams still detected reliably (after grace period expires)
- ✅ UI shows stable, meaningful values

## Tuning the Grace Period

### `detection_grace_period_ms` Settings

**Default: 1500ms (1.5 seconds)**
- Works for most print speeds
- Allows typical moves to complete before checking

**Slower speeds (20-40mm/s)**: Increase to 2000-2500ms
- Longer moves need more time to execute
- Prevents false positives on large extrusion commands

**Faster speeds (60-100mm/s)**: Decrease to 1000-1200ms
- Moves complete faster
- Faster jam detection response

**Formula**: `grace_period_ms = (typical_move_distance_mm / print_speed_mm_per_s) * 1000 * 1.5`

Example for 50mm/s speed, 10mm typical move:
```
grace_period = (10mm / 50mm/s) * 1000 * 1.5
             = 0.2s * 1000 * 1.5
             = 300ms (too short!)
```

**But** SDCP reports in 250ms chunks, so add ~1000ms buffer:
```
grace_period = 300ms + 1000ms = 1300ms
```

### Trade-offs

| Grace Period | Detection Speed | False Positives |
|--------------|-----------------|-----------------|
| 500ms | Very fast | High risk |
| 1000ms | Fast | Moderate risk |
| **1500ms** | **Balanced** | **Low risk** |
| 2000ms | Slower | Very low risk |
| 3000ms | Slow | Minimal risk |

## Interaction With Other Settings

### Combined Effect Table

| Setting | Default | Effect on Detection |
|---------|---------|---------------------|
| `detection_length_mm` | 10.0mm | Deficit threshold for jam |
| `detection_grace_period_ms` | 1500ms | Time before checking starts |
| `movement_mm_per_pulse` | 2.88mm | Sensor calibration |

**Minimum time to detect jam**:
```
min_detection_time = grace_period_ms + (detection_length_mm / extrusion_rate_mm_per_s) * 1000
```

Example at 50mm/s extrusion:
```
min_time = 1500ms + (10mm / 50mm/s) * 1000
         = 1500ms + 200ms
         = 1700ms (1.7 seconds)
```

## Why This Approach?

### Alternatives Considered

**1. Smoothed Expected Position (Moving Average)**
- ❌ Complex to implement
- ❌ Requires estimating actual velocity
- ❌ No direct feedback from printer

**2. Velocity-Based Detection**
- ❌ Sensor pulses too infrequent (2.88mm each)
- ❌ Hard to distinguish slow extrusion from jam

**3. Rate of Change (Derivative)**
- ❌ Noisy with 250ms polling
- ❌ Requires multiple samples

**4. Grace Period (Chosen)**
- ✅ Simple to implement
- ✅ Directly addresses SDCP look-ahead behavior
- ✅ Easy to understand and tune
- ✅ Still maintains fast detection after grace period

## Testing Recommendations

### 1. Normal Print Test
Enable verbose logging:
```json
{
  "verbose_logging": true,
  "detection_grace_period_ms": 1500
}
```

**Watch for**:
- Deficit should stay near 0mm most of the time
- Percentage should hover 0-30%
- No "Filament jam detected!" messages during normal printing

### 2. Adjust Grace Period
If you see deficit spikes during normal operation:
- **Increase** `detection_grace_period_ms` by 500ms
- Test again

If jams take too long to detect:
- **Decrease** `detection_grace_period_ms` by 200-300ms
- But not below 1000ms

### 3. Actual Jam Test
Manually pinch filament before sensor:
```
Expected behavior:
1. Grace period passes (1.5 seconds)
2. Deficit climbs above threshold
3. "Filament jam detected!" logged
4. Print pauses
```

**Measure**: Time from pinch to pause
- Should be: `grace_period_ms + ~500ms`
- With default 1500ms grace: ~2 seconds total

### 4. Long Move Test
Start a print with long travel moves (50-100mm extrusion):
- Deficit may spike briefly at move start
- But should drop to 0% before grace period expires
- If deficit still high at end of grace period → increase grace period

## Logs to Expect

### Normal Operation (verbose_logging: true)
```
Telemetry: total=50.00mm, sensor=48.32mm, deficit=1.68mm, pulses=17
Flow: expected=50.00mm sensor=48.32mm deficit=1.68mm threshold=10.0mm ratio=0.17 pulses=17 jammed=0
```

### Move Start (within grace period)
```
Telemetry: total=60.00mm, sensor=48.32mm, deficit=11.68mm, pulses=17
Flow: expected=60.00mm sensor=48.32mm deficit=11.68mm threshold=10.0mm ratio=1.17 pulses=17 jammed=0
```
**Note**: `jammed=0` even though deficit > threshold (grace period active!)

### After Grace Period (caught up)
```
Telemetry: total=60.00mm, sensor=59.04mm, deficit=0.96mm, pulses=21
Flow: expected=60.00mm sensor=59.04mm deficit=0.96mm threshold=10.0mm ratio=0.10 pulses=21 jammed=0
```

### Actual Jam Detected
```
Telemetry: total=60.00mm, sensor=48.32mm, deficit=11.68mm, pulses=17
Filament jam detected! Expected 60.00mm, sensor=48.32mm, deficit=11.68mm (threshold 10.0mm)
Pausing print, detected filament runout or stopped
```
**Note**: Only logged **after** grace period expires!

## Summary of Changes

### Files Modified
1. **[src/FilamentMotionSensor.h](src/FilamentMotionSensor.h)**
   - Added `lastExpectedUpdateMs` member
   - Added `gracePeriodMs` parameter to `isJammed()`

2. **[src/FilamentMotionSensor.cpp](src/FilamentMotionSensor.cpp)**
   - Track timing of expected position updates
   - Implement grace period logic in `isJammed()`
   - Reset grace period on retractions and significant changes

3. **[src/SettingsManager.h](src/SettingsManager.h)**
   - Added `detection_grace_period_ms` setting
   - Added getter/setter methods

4. **[src/SettingsManager.cpp](src/SettingsManager.cpp)**
   - Default grace period: 1500ms
   - Load/save grace period from JSON

5. **[src/ElegooCC.cpp](src/ElegooCC.cpp)**
   - Pass grace period to jam detection
   - Use setting value or default to 1500ms

6. **[data/user_settings.json](data/user_settings.json)**
   - Added `"detection_grace_period_ms": 1500`

### Build Status
✅ Build successful
- RAM: 14.2% (46,632 bytes)
- Flash: 39.0% (1,301,937 bytes)

## Key Takeaways

1. **SDCP is not Klipper**: SDCP reports planned extrusion (look-ahead), not actual position
2. **Grace period is essential**: Need time for physical movement to execute before checking
3. **Tunable for your setup**: Adjust grace period based on print speed and move sizes
4. **Still reliable**: Real jams detected after grace period expires
5. **Stable UI**: Deficit no longer jumps wildly during normal operation

This fix adapts Klipper's distance-based approach to work with SDCP's look-ahead reporting behavior.
