# Tracking Algorithm Comparison

This firmware now supports **three different tracking algorithms** for filament jam detection. You can switch between them via settings to find which works best for your setup.

## The Three Algorithms

### 1. Cumulative (Mode 0) - Legacy
**How it works**: Tracks total distance from baseline
- Expected: `current_total - baseline`
- Actual: Sum of all sensor pulses since baseline
- Deficit: `expected - actual`

**Pros:**
- ✅ Simple and fast
- ✅ Low memory usage (3 floats)
- ✅ Easy to understand

**Cons:**
- ❌ **Calibration drift**: If `movement_mm_per_pulse` is off by even 1%, deficit accumulates over long prints
- ❌ Example: 1000mm print with 1% error = 10mm accumulated error
- ❌ Can false-trigger on long prints or miss real jams

**Best for:**
- Short prints (< 100mm extrusion)
- Perfectly calibrated sensors
- Testing/debugging

---

### 2. Windowed (Mode 1) - Klipper-Style ⭐ **RECOMMENDED**
**How it works**: Tracks samples over a sliding time window (default 5 seconds)
- Stores last N telemetry updates with timestamps
- Prunes samples older than window
- Expected/Actual: Sum of samples in current window
- Deficit: `window_expected - window_actual`

**Pros:**
- ✅ **No calibration drift**: Old errors fall off the window
- ✅ Handles miscalibrated sensors
- ✅ Matches Klipper's proven approach
- ✅ Consistent behavior over long prints

**Cons:**
- ⚠️ More memory (20 samples × 12 bytes = 240 bytes)
- ⚠️ Slightly more CPU (prune old samples)
- ⚠️ Needs tuning of window size

**Best for:**
- Production use
- Long prints
- When sensor calibration is uncertain
- **Default recommendation**

**Settings:**
```json
{
  "tracking_mode": 1,
  "tracking_window_ms": 5000  // 5 seconds (tune: 3000-10000ms)
}
```

---

### 3. EWMA (Mode 2) - Exponentially Weighted Moving Average
**How it works**: Weighted average favoring recent samples
- Each new sample: `avg = alpha × new + (1 - alpha) × old_avg`
- Alpha = 0.3 means 30% weight on new, 70% on history
- No explicit window, decay is exponential

**Pros:**
- ✅ Handles calibration drift (old data decays)
- ✅ Very low memory (4 floats)
- ✅ Simple implementation
- ✅ Smooth response to changes

**Cons:**
- ⚠️ Less intuitive than windowed
- ⚠️ Alpha parameter needs tuning
- ⚠️ History never fully disappears (exponential decay)

**Best for:**
- Memory-constrained systems
- When you want smooth deficit tracking
- Alternative to windowed if memory is tight

**Settings:**
```json
{
  "tracking_mode": 2,
  "tracking_ewma_alpha": 0.3  // Tune: 0.1 (smooth) to 0.5 (responsive)
}
```

---

## Settings Reference

### Core Settings
```json
{
  "tracking_mode": 1,              // 0=Cumulative, 1=Windowed, 2=EWMA
  "tracking_window_ms": 5000,      // Window size for Mode 1 (milliseconds)
  "tracking_ewma_alpha": 0.3,      // Smoothing factor for Mode 2 (0.0-1.0)
  "detection_length_mm": 10.0,     // Jam threshold (mm of deficit)
  "detection_grace_period_ms": 500 // Grace period after move starts (ms)
}
```

### Mode 1 (Windowed) Tuning

**Window Size** (`tracking_window_ms`):

| Value | Drift Resistance | Detection Speed | Memory Use |
|-------|------------------|-----------------|------------|
| 3000ms | Low | Fast | Low |
| **5000ms** | **Medium** | **Balanced** | **Medium** |
| 7000ms | High | Slower | High |
| 10000ms | Very High | Slow | Very High |

**Formula**: At 250ms polling, window covers `window_ms / 250` samples

Examples:
- 3000ms = 12 samples
- 5000ms = 20 samples (default)
- 10000ms = 40 samples (need to increase MAX_SAMPLES in code!)

**Recommendation**: Start with 5000ms. If you see calibration drift on long prints, increase to 7000-10000ms.

---

### Mode 2 (EWMA) Tuning

**Alpha** (`tracking_ewma_alpha`):

| Value | Behavior | Drift Resistance | Responsiveness |
|-------|----------|------------------|----------------|
| 0.1 | Very smooth | Low | Slow |
| 0.2 | Smooth | Medium-Low | Medium-Slow |
| **0.3** | **Balanced** | **Medium** | **Medium** |
| 0.4 | Responsive | Medium-High | Fast |
| 0.5 | Very responsive | High | Very Fast |

**What it means**:
- Alpha = 0.3: Each new sample contributes 30%, history contributes 70%
- Alpha = 0.1: Each new sample contributes 10%, history contributes 90% (smoother, slower to adapt)
- Alpha = 0.5: Each new sample contributes 50%, history contributes 50% (more reactive)

**Half-life formula**: `half_life = ln(2) / ln(1 / (1 - alpha))`
- Alpha 0.3 → half-life ≈ 2 samples
- Alpha 0.2 → half-life ≈ 3 samples
- Alpha 0.1 → half-life ≈ 7 samples

**Recommendation**: Start with 0.3. If deficit jumps around too much, decrease to 0.2. If jams take too long to detect, increase to 0.4.

---

## Grace Period Setting

**`detection_grace_period_ms`**: Time to wait after expected position updates before checking for jams

**Why needed**: SDCP reports moves **before they execute** (look-ahead)
- 10mm move over 2 sec → TotalExtrusion jumps 10mm instantly
- Sensor pulses arrive gradually over 2 seconds
- Grace period prevents false jam detection during this lag

**Recommended values**:

| Print Speed | Grace Period | Rationale |
|-------------|--------------|-----------|
| 20-40mm/s | 500-750ms | Slower moves, longer to execute |
| 40-60mm/s | **500ms** | **Balanced (default)** |
| 60-100mm/s | 300-500ms | Faster moves, quicker execution |

**Too short**: False positives (pauses during normal printing)
**Too long**: Delayed jam detection

**Default 500ms**: Works for most speeds. Shorter than previous 1500ms because windowed tracking handles drift.

---

## Testing Each Algorithm

### Test 1: Short Print (Baseline)
**Goal**: All algorithms should work identically

1. Start with Mode 1 (Windowed)
2. Enable verbose logging
3. Run a small print (< 100mm extrusion)
4. Watch deficit - should stay near 0mm

**Expected**: All modes work fine on short prints

---

### Test 2: Long Print (Drift Test)
**Goal**: See if calibration error accumulates

**Setup**:
```json
{
  "verbose_logging": false,
  "flow_summary_logging": true,
  "detection_length_mm": 10.0
}
```

**Test A - Cumulative Mode**:
```json
{
  "tracking_mode": 0
}
```

Run a print with > 1000mm extrusion. Watch the Status page:
- Does deficit slowly grow over time?
- Does it eventually trigger false jam?

**Test B - Windowed Mode**:
```json
{
  "tracking_mode": 1,
  "tracking_window_ms": 5000
}
```

Run same print. Compare:
- Does deficit stay stable over time?
- No false triggers?

**Test C - EWMA Mode**:
```json
{
  "tracking_mode": 2,
  "tracking_ewma_alpha": 0.3
}
```

Run same print. Compare:
- Similar to windowed?
- Smoother or jumpier?

---

### Test 3: Actual Jam Detection
**Goal**: Verify all modes detect real jams

For each mode:
1. Start a test print
2. Wait for grace period to pass
3. **Manually pinch filament** before sensor
4. Observe time to detection

**Expected**: All modes should detect within ~10-15mm of requested extrusion after grace period.

---

### Test 4: Calibration Error Simulation
**Goal**: See how modes handle miscalibrated sensor

**Intentionally miscalibrate**:
```json
{
  "movement_mm_per_pulse": 2.95  // Wrong! Actual is 2.88mm
}
```

Run long print (1000mm+) with each mode:

**Cumulative (Mode 0)**:
- Error per pulse: +0.07mm
- After 350 pulses (1008mm actual): +24.5mm cumulative error!
- **Prediction**: False jam trigger

**Windowed (Mode 1)**:
- Error falls off after 5 seconds
- Deficit oscillates but doesn't accumulate
- **Prediction**: Stable, no false trigger

**EWMA (Mode 2)**:
- Error decays exponentially
- Deficit elevated but bounded
- **Prediction**: Stable, no false trigger

**This test proves why windowed/EWMA are superior for long prints!**

---

## Recommendations by Use Case

### ✅ General Use (Default)
```json
{
  "tracking_mode": 1,
  "tracking_window_ms": 5000,
  "detection_length_mm": 10.0,
  "detection_grace_period_ms": 500
}
```
**Why**: Proven Klipper approach, handles calibration drift, good balance

---

### ⚡ Fast Prints (> 60mm/s)
```json
{
  "tracking_mode": 1,
  "tracking_window_ms": 3000,
  "detection_length_mm": 7.0,
  "detection_grace_period_ms": 300
}
```
**Why**: Shorter window and grace period for faster response

---

### 🐢 Slow Prints (< 30mm/s)
```json
{
  "tracking_mode": 1,
  "tracking_window_ms": 7000,
  "detection_length_mm": 12.0,
  "detection_grace_period_ms": 750
}
```
**Why**: Longer window handles slower moves, higher threshold for tolerance

---

### 🔬 Short Test Prints
```json
{
  "tracking_mode": 0,
  "detection_length_mm": 10.0,
  "detection_grace_period_ms": 500
}
```
**Why**: Cumulative is fine for short prints, simpler for debugging

---

### 💾 Memory Constrained
```json
{
  "tracking_mode": 2,
  "tracking_ewma_alpha": 0.3,
  "detection_length_mm": 10.0,
  "detection_grace_period_ms": 500
}
```
**Why**: EWMA uses minimal memory (16 bytes vs 240 bytes for windowed)

---

## Troubleshooting

### Deficit keeps growing on long prints
**Cause**: Calibration drift (cumulative mode) or miscalibrated sensor

**Solutions**:
1. Switch to windowed mode (1) or EWMA mode (2)
2. Recalibrate `movement_mm_per_pulse` setting
3. Increase window size if in windowed mode

---

### False jam detections
**Causes**:
- Grace period too short
- Detection threshold too low
- Sensor skipping pulses
- Window too small (windowed mode)

**Solutions**:
1. Increase `detection_grace_period_ms` (try 750ms)
2. Increase `detection_length_mm` (try 12-15mm)
3. Check sensor mounting (mechanical issues)
4. Increase `tracking_window_ms` to 7000-10000ms

---

### Jams not detected fast enough
**Causes**:
- Detection threshold too high
- Grace period too long
- Window too large (windowed mode)

**Solutions**:
1. Decrease `detection_length_mm` (try 7-8mm)
2. Decrease `detection_grace_period_ms` (try 300ms)
3. Decrease `tracking_window_ms` to 3000ms
4. Use EWMA with higher alpha (0.4-0.5)

---

### Deficit jumps around erratically
**Causes**:
- Alpha too high (EWMA mode)
- Window too small (windowed mode)
- Bursty telemetry

**Solutions**:
1. **EWMA mode**: Decrease alpha to 0.2 or 0.1
2. **Windowed mode**: Increase window to 7000-10000ms
3. Increase `detection_grace_period_ms`

---

## Algorithm Comparison Table

| Feature | Cumulative | Windowed | EWMA |
|---------|------------|----------|------|
| **Memory** | 12 bytes | 240 bytes | 16 bytes |
| **CPU** | Very Low | Low | Very Low |
| **Drift Resistance** | ❌ None | ✅ Excellent | ✅ Good |
| **Long Print Stable** | ❌ No | ✅ Yes | ✅ Yes |
| **Calibration Tolerance** | ❌ Low | ✅ High | ✅ Medium |
| **Tuning Complexity** | Simple | Medium | Medium |
| **Klipper-Like** | ❌ No | ✅ Yes | ⚠️ Similar |
| **Best For** | Short prints, testing | **Production, long prints** | Memory-constrained |

---

## What to Report When Testing

When testing different modes, please report:

1. **Tracking mode used**: 0/1/2
2. **Settings**: window_ms, ewma_alpha, grace_period_ms
3. **Print characteristics**:
   - Total extrusion (mm)
   - Print speed (mm/s)
   - Duration
4. **Observations**:
   - Did deficit stay stable?
   - Any false positives?
   - Did real jams get detected?
   - How long to detect?
5. **Logs** (if verbose_logging enabled)

This data will help determine which algorithm works best for your specific setup!

---

## Summary

**TL;DR**:
- **Use Mode 1 (Windowed)** for most cases - it's the most robust
- **Use Mode 2 (EWMA)** if you need to save memory
- **Avoid Mode 0 (Cumulative)** for long prints unless sensor is perfectly calibrated

The windowed approach solves the calibration drift problem and matches Klipper's proven design. Test it first!
