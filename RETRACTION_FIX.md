# Retraction Handling Fix for Bursty Telemetry

> **Note**: This fix addresses part of the jumping deficit issue. A more fundamental issue related to SDCP look-ahead behavior is addressed in [SDCP_LOOKAHEAD_FIX.md](SDCP_LOOKAHEAD_FIX.md).

## Problem
The deficit percentage was jumping back and forth between high values and 0, despite expected and actual filament values being close. This was happening even during normal printing.

## Root Cause
Three issues were causing the erratic behavior:

### 1. **Retraction Handling**
The original code adjusted the baseline when retractions occurred, but **didn't reset the sensor distance**. This caused problems because:
- The sensor detects movement in **both directions** (it can't tell forward from backward)
- During retraction, the sensor keeps accumulating distance
- The expected position decreased (retraction), but sensor distance kept growing
- This created an artificial deficit that would suddenly appear/disappear

### 2. **Bursty Telemetry with Small Movements**
With 250ms polling (vs Klipper's ~100ms), telemetry arrives in larger chunks:
- Very small movements (< 1mm) had unreliable deficit ratios
- At the start of extrusion, one sensor pulse (2.88mm) could temporarily exceed expected
- Division by small numbers created wild percentage swings

### 3. **SDCP Look-Ahead Behavior (Most Critical!)**
SDCP reports **planned/commanded** extrusion immediately when moves start, not actual in-progress extrusion:
- A 10mm move over 2 seconds jumps `TotalExtrusion` by 10mm instantly
- Sensor pulses arrive gradually as filament actually moves
- Creates artificial deficit that gradually decreases as move executes
- **See [SDCP_LOOKAHEAD_FIX.md](SDCP_LOOKAHEAD_FIX.md) for the primary solution**

## Solution

### Change 1: Retraction Sync Point
**File**: `src/FilamentMotionSensor.cpp`

```cpp
// Handle retractions: if total extrusion decreased, treat as sync point
// Reset both baseline and sensor to resync tracking after retraction
if (totalExtrusionMm < expectedPositionMm)
{
    // Retraction detected - resync everything
    baselinePositionMm = totalExtrusionMm;
    sensorDistanceMm   = 0.0f;  // Reset sensor tracking after retraction
}
```

**Why this works**: Treats retraction as a synchronization point. Both expected and sensor tracking reset together, eliminating accumulated sensor movement during retractions.

### Change 2: Minimum Distance Before Jam Check
```cpp
// Don't check for jams until we've moved a meaningful distance
float expectedDistance = expectedPositionMm - baselinePositionMm;
float minDistanceBeforeCheck = detectionLengthMm * 0.5f;  // Half the detection threshold

if (expectedDistance < minDistanceBeforeCheck)
{
    return false;  // Not enough movement yet to reliably detect jams
}
```

**Why this works**:
- With `detection_length_mm = 10.0`, requires at least 5mm of movement before checking
- Prevents false positives from telemetry timing quirks at extrusion start
- Still allows fast jam detection (just not in the first 5mm)

### Change 3: Better Flow Ratio Calculation
```cpp
float expectedDistance = getExpectedDistance();
// Need at least 1mm of expected movement for meaningful ratio
if (expectedDistance < 1.0f)
{
    return 0.0f;  // Return 0 instead of wild percentages
}

float ratio = sensorDistanceMm / expectedDistance;
// Clamp to reasonable range [0, 1.5]
if (ratio > 1.5f) ratio = 1.5f;
if (ratio < 0.0f) ratio = 0.0f;
```

**Why this works**:
- Avoids division by very small numbers (< 1mm)
- Clamps ratio to physically realistic range
- Prevents UI from showing "300%" deficit during brief timing anomalies

## Expected Behavior After Fix

### Normal Printing
- **Deficit**: Should stay near 0mm most of the time
- **Deficit %**: Should stay 0-30% during normal operation
- **No more jumping**: Values should be stable and only gradually increase if a jam starts

### During Retractions
- **Automatic resync**: Deficit resets to 0 after each retraction
- **Clean slate**: Each extrusion segment after retraction starts fresh
- **No accumulated errors**: Sensor movement during retraction doesn't pollute tracking

### Jam Detection
- **Still fast**: Jams detected within `detection_length_mm` (default 10mm)
- **More stable**: Won't trigger from brief telemetry delays
- **No false positives**: Minimum movement threshold prevents spurious triggers

## Testing Recommendations

### 1. **Normal Print Test**
Watch the Status page during a normal print:
- Deficit should hover near 0mm
- Deficit % should stay below 40-50%
- Values should be relatively stable (minor fluctuations OK)

### 2. **Retraction Test**
Enable verbose logging and watch for retraction patterns:
```json
{
  "verbose_logging": true
}
```

Look for log lines showing TotalExtrusion decreasing, then increasing again. Verify deficit doesn't build up over multiple retraction cycles.

### 3. **Actual Jam Test**
Manually create a jam (pinch filament before sensor):
- Should detect within ~10-15mm of requested extrusion
- Log should show: "Filament jam detected! Expected X.Xmm, sensor Y.Ymm, deficit Z.Zmm"
- Deficit % should climb smoothly to 100% and trigger pause

### 4. **Start of Print**
Watch the first few seconds after print starts:
- First 5mm of extrusion: deficit shown but jam checking disabled
- After 5mm: jam detection becomes active
- No false positives from extrusion startup

## Settings That Affect Behavior

### `detection_length_mm` (default: 10.0)
- **Lower (7-8mm)**: Faster detection, slightly more sensitive
- **Higher (12-15mm)**: More tolerant, fewer false positives
- **Minimum check distance**: Always 50% of this value

### `movement_mm_per_pulse` (default: 2.88)
- Must match your sensor's actual spec
- Incorrect value = deficit will always be wrong
- **Test**: Watch sensor distance accumulate, compare to known extrusion amount

### `flow_telemetry_stale_ms` (default: 1000)
- How long without SDCP updates before telemetry considered stale
- 1000ms = 1 second is reasonable for 250ms polling
- Too low = false "telemetry lost" warnings

## Technical Details

### Why Not Use Klipper's Exact Algorithm?
Klipper has several advantages we don't have:
1. **Much faster polling** (~100ms vs our 250ms)
2. **Direct MCU integration** (no network delays)
3. **Deterministic timing** (real-time OS)

Our approach:
- **Distance-based** (like Klipper) ✓
- **Retraction sync** (adapted for our constraints) ✓
- **Minimum distance threshold** (compensates for slower polling) ✓

### Sensor Behavior During Retractions
The sensor is a **quadrature encoder** that detects rotation:
- Forwards extrusion: pulses accumulate
- Backwards retraction: pulses **still accumulate** (can't detect direction)
- This is why we **must** reset sensor distance after retraction

### Polling Frequency Impact
With 250ms polling, each telemetry update represents a larger chunk of movement:
- Klipper (100ms): ~1-2mm per update at normal speeds
- Our system (250ms): ~3-5mm per update at normal speeds

This means:
- **One sensor pulse** (2.88mm) might arrive before telemetry update
- **Temporarily** sensor > expected → negative deficit → clamped to 0
- **Next telemetry** catches up → deficit recalculates correctly

The minimum distance threshold prevents this from causing false jam detection.

## If Problems Persist

### Deficit still jumping around?
1. Check `verbose_logging` output - is TotalExtrusion changing smoothly?
2. Verify `movement_mm_per_pulse` is correct (2.88mm for SFS 2.0)
3. Increase `detection_length_mm` to 15mm for more tolerance

### False positives (pauses during normal printing)?
1. Increase `detection_length_mm` from 10 to 12-15mm
2. Check if sensor is skipping (mechanical issue)
3. Verify `flow_telemetry_stale_ms` isn't too low

### Jams not detected?
1. Decrease `detection_length_mm` from 10 to 7-8mm
2. Check if SDCP telemetry is actually arriving (Status page: "Telemetry Available")
3. Verify physical sensor is working (watch Movement Pulses counter)

### Deficit always shows high percentage?
1. Wrong `movement_mm_per_pulse` value - recalibrate
2. Sensor might be slipping - check mechanical mounting
3. Check if sensor wheel is clean (filament dust buildup)

## Summary

**Before**: Deficit jumping 0% → 80% → 0% even during normal printing
**After**: Stable deficit near 0-30% during normal printing, smooth climb during actual jams

The fix makes the system more tolerant of slower polling rates while maintaining reliable jam detection.
