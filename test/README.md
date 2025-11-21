# Pulse Simulator - Unit Tests

Comprehensive unit testing for the FilamentMotionSensor without requiring hardware.

## Features

- **Test 1: Normal Healthy Print** - Verifies no false positives during 30 seconds of continuous printing
- **Test 2: Hard Jam Detection** - Complete blockage (no sensor pulses) detected in ~2 seconds
- **Test 3: Soft Jam Detection** - Partial clog (20% flow rate) detected in ~3 seconds
- **Test 4: Sparse Infill** - Travel moves with minimal extrusion don't cause false positives
- **Test 5: Retraction Handling** - Grace period applies correctly after retractions
- **Test 6: Transient Spike Resistance** - Single bad ratio spike doesn't trigger jam (hysteresis)
- **Test 7: Minimum Movement Threshold** - Tiny movements below 1mm threshold don't cause issues
- **Test 8: Grace Period Duration** - 500ms grace period protects against SDCP look-ahead

## Building and Running

### Windows

```batch
cd test
build_tests.bat
```

### Linux/Mac

```bash
cd test
chmod +x build_tests.sh
./build_tests.sh
```

### Manual Compilation

```bash
g++ -std=c++11 -o pulse_simulator pulse_simulator.cpp -I..
./pulse_simulator
```

## Configuration

Test parameters are defined at the top of `pulse_simulator.cpp`:

```cpp
const float MM_PER_PULSE = 2.88f;           // Sensor calibration
const int CHECK_INTERVAL_MS = 1000;          // Jam check frequency
const float RATIO_THRESHOLD = 0.70f;         // 70% deficit = soft jam
const float HARD_JAM_MM = 5.0f;              // Hard jam threshold
const int SOFT_JAM_TIME_MS = 3000;           // Soft jam duration
const int HARD_JAM_TIME_MS = 2000;           // Hard jam duration
const int GRACE_PERIOD_MS = 500;             // Grace period duration
```

## Understanding Test Output

Each test shows detailed sensor state:

```
[Test Name] exp=24.60mm act=24.60mm deficit=0.00mm ratio=1.00 [OK]
```

- **exp**: Expected extrusion (from SDCP telemetry)
- **act**: Actual sensor measurement
- **deficit**: How much expected exceeds actual
- **ratio**: Flow ratio (0.0 = 0% passing, 1.0 = 100% passing)
- **[JAM]** or **[OK]**: Current jam detection state

## Adding New Tests

To add a new test scenario:

1. Create a new test function following the pattern:
```cpp
void testMyScenario() {
    printTestHeader("Test X: My Scenario");

    FilamentMotionSensor sensor;
    sensor.setTrackingMode(TRACKING_MODE_WINDOWED, 5000, 0.3f);
    sensor.reset();
    _mockMillis = 0;

    // Your test logic...

    recordTest("Test description", passed);
}
```

2. Call it from `main()`:
```cpp
testMyScenario();
```

## Test Scenarios Simulated

### Normal Printing
- Continuous extrusion at 50mm/s
- 100% sensor flow rate
- Expected: No false positives

### Hard Jam
- Extrusion commands continue
- Sensor pulses stop completely
- Expected: Jam detected in 2 seconds

### Soft Jam
- Extrusion commands continue
- Sensor shows only 20% flow rate
- Expected: Jam detected in 3 seconds

### Sparse Infill
- 3 seconds normal printing
- 10 seconds travel (no telemetry updates)
- 3 seconds resume
- Expected: No false positives, grace period applies on resume

### Retractions
- Normal printing → retraction → resume
- Expected: Grace period applies after retraction

### Transient Spikes
- Single 1-second bad ratio spike
- Returns to normal
- Expected: Hysteresis prevents false positive

## Exit Codes

- `0`: All tests passed
- `1`: One or more tests failed
