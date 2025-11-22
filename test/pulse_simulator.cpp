/**
 * Pulse Simulator - Unit tests for FilamentMotionSensor
 *
 * Tests various print conditions without hardware:
 * - Normal printing (healthy)
 * - Hard jams (complete blockage)
 * - Soft jams (partial clogs/underextrusion)
 * - Sparse infill (travel moves)
 * - Retractions
 * - Speed changes
 * - Transient spikes (false positive prevention)
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cmath>
#include <fstream>
#include <cstdint>

// Mock Arduino millis() function
unsigned long _mockMillis = 0;
unsigned long millis() { return _mockMillis; }

// Include actual sensor code
#include "../src/FilamentMotionSensor.h"
#include "../src/FilamentMotionSensor.cpp"

// ANSI color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"

// Test result tracking
struct TestResult {
    std::string name;
    bool passed;
    std::string details;
};
std::vector<TestResult> testResults;

// Simulation parameters
const float MM_PER_PULSE = 2.88f;
const int CHECK_INTERVAL_MS = 1000;
const float RATIO_THRESHOLD = 0.25f;
const float HARD_JAM_MM = 5.0f;
const int SOFT_JAM_TIME_MS = 10000;
const int HARD_JAM_TIME_MS = 5000;
const int GRACE_PERIOD_MS = 500;

// Logging for visualization
static std::ofstream gLogStream;
static bool gLogEnabled = false;
static std::string gCurrentTestName = "startup";

void initLogFile(const std::string& path) {
    gLogStream.open(path, std::ios::trunc);
    if (!gLogStream) {
        std::cerr << "WARNING: Unable to open log file '" << path << "'\n";
        gLogEnabled = false;
        return;
    }
    gLogEnabled = true;
    gLogStream << "test,label,timestamp,expected,actual,deficit,ratio,jammed\n";
}

void closeLogFile() {
    if (gLogStream.is_open()) {
        gLogStream.close();
    }
    gLogEnabled = false;
}

std::string csvEncode(const std::string& value) {
    std::string encoded = "\"";
    for (char ch : value) {
        if (ch == '"') {
            encoded += "\"\"";
        } else {
            encoded += ch;
        }
    }
    encoded += "\"";
    return encoded;
}

void logStateRow(const std::string& label, float expected, float actual, float deficit, float ratio, bool jammed) {
    if (!gLogEnabled) return;
    gLogStream << csvEncode(gCurrentTestName) << "," << csvEncode(label) << ","
               << _mockMillis << "," << expected << "," << actual << "," << deficit << ","
               << ratio << "," << (jammed ? 1 : 0) << "\n";
}

void logFrameState(const FilamentMotionSensor& sensor, const std::string& label, bool jammed) {
    if (!gLogEnabled) return;
    float expected = sensor.getExpectedDistance();
    float actual = sensor.getSensorDistance();
    float deficit = sensor.getDeficit();
    float ratio = sensor.getFlowRatio();
    logStateRow(label, expected, actual, deficit, ratio, jammed);
}

bool checkJam(const FilamentMotionSensor& sensor);

bool checkJamAndLog(const FilamentMotionSensor& sensor, const std::string& label) {
    bool jammed = checkJam(sensor);
    logFrameState(sensor, label, jammed);
    return jammed;
}

// Helper: Advance time
void advanceTime(int ms) {
    _mockMillis += ms;
}

// Helper: Simulate extrusion command from SDCP
void simulateExtrusion(FilamentMotionSensor& sensor, float deltaExtrusionMm, float currentTotalMm) {
    sensor.updateExpectedPosition(currentTotalMm);
}

// Helper: Simulate sensor pulses (multiple pulses for large movements)
void simulateSensorPulses(FilamentMotionSensor& sensor, float totalMm, float flowRate = 1.0f) {
    float actualMm = totalMm * flowRate;
    int pulseCount = static_cast<int>(actualMm / MM_PER_PULSE);
    for (int i = 0; i < pulseCount; i++) {
        sensor.addSensorPulse(MM_PER_PULSE);
    }
}

// Helper: Check jam detection
bool checkJam(const FilamentMotionSensor& sensor) {
    return sensor.isJammed(RATIO_THRESHOLD, HARD_JAM_MM,
                          SOFT_JAM_TIME_MS, HARD_JAM_TIME_MS,
                          CHECK_INTERVAL_MS, GRACE_PERIOD_MS);
}

// Helper: Print test header
void printTestHeader(const std::string& testName) {
    gCurrentTestName = testName;
    std::cout << "\n" << COLOR_CYAN << "=== " << testName << " ===" << COLOR_RESET << "\n";
}

// Helper: Record test result
void recordTest(const std::string& name, bool passed, const std::string& details = "") {
    testResults.push_back({name, passed, details});
    if (passed) {
        std::cout << COLOR_GREEN << "✓ PASS" << COLOR_RESET << ": " << name << "\n";
    } else {
        std::cout << COLOR_RED << "✗ FAIL" << COLOR_RESET << ": " << name;
        if (!details.empty()) {
            std::cout << " (" << details << ")";
        }
        std::cout << "\n";
    }
}

// Helper: Print sensor state (console only, logging handled elsewhere)
void printState(const FilamentMotionSensor& sensor, const std::string& label, bool jammed = false) {
    float expected = sensor.getExpectedDistance();
    float actual = sensor.getSensorDistance();
    float deficit = sensor.getDeficit();
    float ratio = sensor.getFlowRatio();

    std::cout << "  [" << std::setw(20) << std::left << label << "] "
              << "exp=" << std::fixed << std::setprecision(2) << expected << "mm "
              << "act=" << actual << "mm "
              << "deficit=" << deficit << "mm "
              << "ratio=" << ratio << " "
              << (jammed ? COLOR_RED "[JAM]" COLOR_RESET : COLOR_GREEN "[OK]" COLOR_RESET)
              << "\n";
}

//=============================================================================
// TEST 1: Normal Healthy Print
//=============================================================================
void testNormalPrinting() {
    printTestHeader("Test 1: Normal Healthy Print");

    FilamentMotionSensor sensor;
    sensor.setTrackingMode(TRACKING_MODE_WINDOWED, 5000, 0.3f);
    sensor.reset();
    _mockMillis = 0;

    float totalExtrusion = 0.0f;
    bool anyFalsePositive = false;

    // Simulate 30 seconds of normal printing at 50mm/s
    for (int sec = 0; sec < 30; sec++) {
        float deltaExtrusion = 50.0f;  // 50mm per second
        totalExtrusion += deltaExtrusion;

        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        simulateSensorPulses(sensor, deltaExtrusion, 1.0f);  // 100% flow rate

        advanceTime(CHECK_INTERVAL_MS);

        std::string label = "Normal print T+" + std::to_string(sec + 1) + "s";
        bool jammed = checkJamAndLog(sensor, label);
        if (jammed) {
            anyFalsePositive = true;
            printState(sensor, label, jammed);
        }
    }

    bool sampleJam = checkJamAndLog(sensor, "Normal print sample");
    printState(sensor, "Normal print sample", sampleJam);

    recordTest("Normal print no false positives", !anyFalsePositive);
}

//=============================================================================
// TEST 2: Hard Jam Detection (Complete Blockage)
//=============================================================================
void testHardJam() {
    printTestHeader("Test 2: Hard Jam Detection (Complete Blockage)");

    FilamentMotionSensor sensor;
    sensor.setTrackingMode(TRACKING_MODE_WINDOWED, 5000, 0.3f);
    sensor.reset();
    _mockMillis = 0;

    float totalExtrusion = 0.0f;

    // Normal printing for 5 seconds
    for (int sec = 0; sec < 5; sec++) {
        float deltaExtrusion = 20.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        simulateSensorPulses(sensor, deltaExtrusion, 1.0f);
        advanceTime(CHECK_INTERVAL_MS);
    }

    bool healthyJam = checkJamAndLog(sensor, "Before jam (healthy)");
    printState(sensor, "Before jam (healthy)", healthyJam);

    // Hard jam: extrusion commands but NO sensor pulses
    int jamDetectionSec = -1;
    bool jamDetectedTooEarly = false;

    for (int sec = 0; sec < 7; sec++) {
        float deltaExtrusion = 20.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        // NO sensor pulses - complete blockage
        advanceTime(CHECK_INTERVAL_MS);

        std::string label = "Hard jam T+" + std::to_string(sec + 1) + "s";
        bool jammed = checkJamAndLog(sensor, label);
        if (jammed && sec < 4) jamDetectedTooEarly = true;
        if (jammed && jamDetectionSec == -1) {
            jamDetectionSec = sec;
        }

        printState(sensor, label, jammed);
    }

    recordTest("Hard jam detected around the 5s mark", jamDetectionSec >= 4 && jamDetectionSec <= 6,
               jamDetectionSec >= 0 ? "Detected at T+" + std::to_string(jamDetectionSec+1) + "s" : "Not detected");
    recordTest("Hard jam not detected before 5 seconds", !jamDetectedTooEarly);
}

//=============================================================================
// TEST 3: Soft Jam Detection (Partial Clog/Underextrusion)
//=============================================================================
void testSoftJam() {
    printTestHeader("Test 3: Soft Jam Detection (Partial Clog)");

    FilamentMotionSensor sensor;
    sensor.setTrackingMode(TRACKING_MODE_WINDOWED, 5000, 0.3f);
    sensor.reset();
    _mockMillis = 0;

    float totalExtrusion = 0.0f;

    // Normal printing for 5 seconds
    for (int sec = 0; sec < 5; sec++) {
        float deltaExtrusion = 20.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        simulateSensorPulses(sensor, deltaExtrusion, 1.0f);
        advanceTime(CHECK_INTERVAL_MS);
    }

    bool beforeJamHealthy = checkJamAndLog(sensor, "Before jam (healthy)");
    printState(sensor, "Before jam (healthy)", beforeJamHealthy);

    // Soft jam: only 20% of filament passing (80% deficit)
    // With windowed tracking, it takes time for the window to fill with bad samples
    // Expected: Should detect within reasonable time (not instant, but before 10 seconds)
    bool jamDetected = false;
    int jamDetectionTime = -1;

    for (int sec = 0; sec < 20; sec++) {
        float deltaExtrusion = 20.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        simulateSensorPulses(sensor, deltaExtrusion, 0.20f);  // 20% flow rate (80% deficit)
        advanceTime(CHECK_INTERVAL_MS);

        std::string label = "Clog T+" + std::to_string(sec + 1) + "s";
        bool jammed = checkJamAndLog(sensor, label);
        if (jammed && !jamDetected) {
            jamDetected = true;
            jamDetectionTime = sec + 1;
        }

        if (sec < 10) {
            printState(sensor, label, jammed);
        }
    }

    recordTest("Soft jam detected within reasonable window", jamDetected && jamDetectionTime <= 18,
               jamDetected ? "Detected at T+" + std::to_string(jamDetectionTime) + "s" : "Not detected");
    recordTest("Soft jam detection waits for the window to fill", jamDetectionTime >= 9);
}

//=============================================================================
// TEST 4: Sparse Infill (No False Positives During Travel)
//=============================================================================
void testSparseInfill() {
    printTestHeader("Test 4: Sparse Infill (Travel Moves)");

    FilamentMotionSensor sensor;
    sensor.setTrackingMode(TRACKING_MODE_WINDOWED, 5000, 0.3f);
    sensor.reset();
    _mockMillis = 0;

    float totalExtrusion = 0.0f;
    bool falsePositive = false;

    // Normal printing
    for (int sec = 0; sec < 3; sec++) {
        float deltaExtrusion = 20.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        simulateSensorPulses(sensor, deltaExtrusion, 1.0f);
        advanceTime(CHECK_INTERVAL_MS);
    }

    bool beforeSparse = checkJamAndLog(sensor, "Before sparse infill");
    printState(sensor, "Before sparse infill", beforeSparse);

    // Sparse infill: 10 seconds of travel with minimal extrusion
    for (int sec = 0; sec < 10; sec++) {
        // No telemetry updates during travel
        advanceTime(CHECK_INTERVAL_MS);

        std::string label = "Travel T+" + std::to_string(sec + 1) + "s";
        bool jammed = checkJamAndLog(sensor, label);
        if (jammed) {
            falsePositive = true;
            printState(sensor, label, jammed);
        }
    }

    // Resume normal printing after telemetry gap
    for (int sec = 0; sec < 3; sec++) {
        float deltaExtrusion = 20.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);

        // Grace period: wait 500ms for sensor to catch up
        if (sec == 0) advanceTime(500);

        simulateSensorPulses(sensor, deltaExtrusion, 1.0f);
        advanceTime(CHECK_INTERVAL_MS - (sec == 0 ? 500 : 0));

        std::string label = "After gap T+" + std::to_string(sec + 1) + "s";
        bool jammed = checkJamAndLog(sensor, label);
        if (jammed) {
            falsePositive = true;
            printState(sensor, label, jammed);
        }
    }

    bool afterResumeJam = checkJamAndLog(sensor, "After resume");
    printState(sensor, "After resume", afterResumeJam);

    recordTest("No false positives during sparse infill", !falsePositive);
}

//=============================================================================
// TEST 5: Retraction Handling
//=============================================================================
void testRetractions() {
    printTestHeader("Test 5: Retraction Handling");

    FilamentMotionSensor sensor;
    sensor.setTrackingMode(TRACKING_MODE_WINDOWED, 5000, 0.3f);
    sensor.reset();
    _mockMillis = 0;

    float totalExtrusion = 0.0f;
    bool falsePositive = false;

    // Normal printing
    for (int sec = 0; sec < 3; sec++) {
        float deltaExtrusion = 20.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        simulateSensorPulses(sensor, deltaExtrusion, 1.0f);
        advanceTime(CHECK_INTERVAL_MS);
    }

    bool beforeRetractJam = checkJamAndLog(sensor, "Before retraction");
    printState(sensor, "Before retraction", beforeRetractJam);

    // Retraction (negative movement)
    totalExtrusion -= 5.0f;
    simulateExtrusion(sensor, -5.0f, totalExtrusion);
    advanceTime(CHECK_INTERVAL_MS);

    bool afterRetractJam = checkJamAndLog(sensor, "After retraction");
    printState(sensor, "After retraction", afterRetractJam);

    // Resume after retraction (grace period should apply)
    for (int sec = 0; sec < 3; sec++) {
        float deltaExtrusion = 20.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);

        // Grace period: wait 500ms
        if (sec == 0) advanceTime(500);

        simulateSensorPulses(sensor, deltaExtrusion, 1.0f);
        advanceTime(CHECK_INTERVAL_MS - (sec == 0 ? 500 : 0));

        std::string label = "After retract T+" + std::to_string(sec + 1) + "s";
        bool jammed = checkJamAndLog(sensor, label);
        if (jammed) {
            falsePositive = true;
        }

        printState(sensor, label, jammed);
    }

    recordTest("No false positives after retraction", !falsePositive);
}

//=============================================================================
// TEST 6: Ironing / Low-Flow Handling
//=============================================================================
void testIroningLowFlow() {
    printTestHeader("Test 6: Ironing / Low-Flow Handling");

    FilamentMotionSensor sensor;
    sensor.setTrackingMode(TRACKING_MODE_WINDOWED, 5000, 0.3f);
    sensor.reset();
    _mockMillis = 0;

    float totalExtrusion = 0.0f;
    bool falsePositive = false;

  // Simulate repeated low-flow micro-movements (iron-like passes)
  for (int sec = 0; sec < 20; sec++) {
      float deltaExtrusion = 0.2f;
      totalExtrusion += deltaExtrusion;
      simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);

      // Sensor reports matching micro-movement
      sensor.addSensorPulse(deltaExtrusion);

      advanceTime(CHECK_INTERVAL_MS);

      std::string label = "Ironing T+" + std::to_string(sec + 1) + "s";
      bool jammed = checkJamAndLog(sensor, label);
      if (jammed) {
          falsePositive = true;
      }
      printState(sensor, label, jammed);
    }

    bool afterIroningJam = checkJamAndLog(sensor, "After ironing pattern");
    printState(sensor, "After ironing pattern", afterIroningJam);
    recordTest("Ironing/low-flow pattern does not trigger jam", !falsePositive);
}

//=============================================================================
// TEST 7: Transient Spike Resistance (Hysteresis)
//=============================================================================
void testTransientSpikes() {
    printTestHeader("Test 7: Transient Spike Resistance");

    FilamentMotionSensor sensor;
    sensor.setTrackingMode(TRACKING_MODE_WINDOWED, 5000, 0.3f);
    sensor.reset();
    _mockMillis = 0;

    float totalExtrusion = 0.0f;
    bool falsePositive = false;

    // Normal printing
    for (int sec = 0; sec < 5; sec++) {
        float deltaExtrusion = 20.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        simulateSensorPulses(sensor, deltaExtrusion, 1.0f);
        advanceTime(CHECK_INTERVAL_MS);
    }

    // Single spike: bad ratio for 1 second
    float deltaExtrusion = 20.0f;
    totalExtrusion += deltaExtrusion;
    simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
    simulateSensorPulses(sensor, deltaExtrusion, 0.15f);  // 85% deficit (single spike)
    advanceTime(CHECK_INTERVAL_MS);

    bool spikeJam = checkJamAndLog(sensor, "Single spike T+1s");
    if (spikeJam) {
        falsePositive = true;
    }
    printState(sensor, "Single spike T+1s", spikeJam);

    // Ratio returns to normal (should reset counter)
    for (int sec = 0; sec < 3; sec++) {
        float deltaExtrusion = 20.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        simulateSensorPulses(sensor, deltaExtrusion, 1.0f);
        advanceTime(CHECK_INTERVAL_MS);
        std::string label = "After spike T+" + std::to_string(sec + 2) + "s";
        bool jammed = checkJamAndLog(sensor, label);
        if (jammed) {
            falsePositive = true;
        }
        printState(sensor, label, jammed);
    }

    recordTest("Transient spike did not trigger jam", !falsePositive);
}

//=============================================================================
// TEST 8: Edge Case - Minimum Movement Threshold
//=============================================================================
void testMinimumMovement() {
    printTestHeader("Test 8: Minimum Movement Threshold");

    FilamentMotionSensor sensor;
    sensor.setTrackingMode(TRACKING_MODE_WINDOWED, 5000, 0.3f);
    sensor.reset();
    _mockMillis = 0;

    float totalExtrusion = 0.0f;

    // Test 1: Very slow printing below detection threshold (<1mm expected)
    // Should NOT trigger jam - too little movement to judge
    bool falsePositiveSubThreshold = false;
    for (int sec = 0; sec < 5; sec++) {
        float deltaExtrusion = 0.1f;  // 0.1mm per second, stays below 1mm total in window
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        advanceTime(CHECK_INTERVAL_MS);

        std::string label = "Tiny move T+" + std::to_string(sec + 1) + "s";
        bool jammed = checkJamAndLog(sensor, label);
        if (jammed) {
            falsePositiveSubThreshold = true;
            printState(sensor, label, jammed);
        }
    }

    recordTest("No jam on sub-threshold movements (<1mm)", !falsePositiveSubThreshold);

    // Test 2: Slow printing with no sensor pulses (should trigger hard jam)
    // 0.5mm/sec × 10 sec = 5mm total (meets hard jam threshold)
    sensor.reset();
    _mockMillis = 0;
    totalExtrusion = 0.0f;
    bool hardJamDetected = false;

    for (int sec = 0; sec < 10; sec++) {
        float deltaExtrusion = 0.5f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        // No sensor pulses (0.5mm < 2.88mm pulse threshold)
        advanceTime(CHECK_INTERVAL_MS);

        std::string label = "Slow print T+" + std::to_string(sec + 1) + "s";
        bool jammed = checkJamAndLog(sensor, label);
        if (jammed) {
            hardJamDetected = true;
        }
    }

    bool slowPrintJam = checkJamAndLog(sensor, "Slow print, no pulses");
    printState(sensor, "Slow print, no pulses", slowPrintJam);

    recordTest("Hard jam detected on slow print without pulses", hardJamDetected);
}

//=============================================================================
// TEST 9: Grace Period Duration
//=============================================================================
void testGracePeriod() {
    printTestHeader("Test 9: Grace Period Duration");

    FilamentMotionSensor sensor;
    sensor.setTrackingMode(TRACKING_MODE_WINDOWED, 5000, 0.3f);
    sensor.reset();
    _mockMillis = 0;

    float totalExtrusion = 0.0f;

    // Normal printing to establish baseline
    for (int i = 0; i < 3; i++) {
        float deltaExtrusion = 20.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        simulateSensorPulses(sensor, deltaExtrusion, 1.0f);
        advanceTime(CHECK_INTERVAL_MS);
    }

    // Simulate telemetry gap (like sparse infill or pause)
    advanceTime(6000);  // 6 second gap - triggers telemetry gap detection and clears window

    // Resume with extrusion but no sensor pulses (jam scenario)
    float deltaExtrusion = 20.0f;
    totalExtrusion += deltaExtrusion;
    simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
    // No sensor pulses - creates jam condition

    // Check at 400ms (within 500ms grace period after gap)
    advanceTime(400);
    bool jamAt400 = checkJamAndLog(sensor, "At 400ms (in grace)");
    printState(sensor, "At 400ms (in grace)", jamAt400);

    // Grace period should still be active - no jam yet
    recordTest("Grace period protects at 400ms after gap", !jamAt400);

    // Advance past grace period and continue extrusion without pulses
    advanceTime(200);  // Now at 600ms - grace period expired

    bool jamAfterGrace = false;
    for (int i = 0; i < 10; i++) {
        deltaExtrusion = 20.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        advanceTime(CHECK_INTERVAL_MS);

        std::string label = "Jam after grace T+" + std::to_string(i + 1) + "s";
        bool jammed = checkJamAndLog(sensor, label);
        if (jammed) {
            jamAfterGrace = true;
            printState(sensor, label, jammed);
            break;
        }
    }

    if (!jamAfterGrace) {
        bool notDetectedJam =
            checkJamAndLog(sensor, "Jam after grace period (not detected)");
        printState(sensor, "Jam after grace period (not detected)", notDetectedJam);
    }

    recordTest("Detection active after grace period expires", jamAfterGrace);
}

//=============================================================================
// TEST 10: Normal Print with Hard Snag
//=============================================================================
void testHardSnagMidPrint() {
    printTestHeader("Test 10: Normal Print with Hard Snag");

    FilamentMotionSensor sensor;
    sensor.setTrackingMode(TRACKING_MODE_WINDOWED, 5000, 0.3f);
    sensor.reset();
    _mockMillis = 0;

    float totalExtrusion = 0.0f;
    for (int sec = 0; sec < 10; sec++) {
        float deltaExtrusion = 25.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        simulateSensorPulses(sensor, deltaExtrusion, 1.0f);
        advanceTime(CHECK_INTERVAL_MS);
    }

    bool beforeSnag = checkJamAndLog(sensor, "Before snag");
    printState(sensor, "Before snag", beforeSnag);

    bool jamDetected = false;
    int jamDetectionTime = -1;
    bool jamTooEarly = false;

    for (int sec = 0; sec < 10; sec++) {
        float deltaExtrusion = 20.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        advanceTime(CHECK_INTERVAL_MS);

        std::string label = "Hard snag T+" + std::to_string(sec + 1) + "s";
        bool jammed = checkJamAndLog(sensor, label);
        if (jammed && !jamDetected) {
            jamDetected = true;
            jamDetectionTime = sec + 1;
        }
        if (jammed && sec < 1) {
            jamTooEarly = true;
        }

        printState(sensor, label, jammed);
        if (jammed) {
            break;
        }
    }

    if (!jamDetected) {
        bool notDetectedJam = checkJamAndLog(sensor, "Hard snag (not detected)");
        printState(sensor, "Hard snag (not detected)", notDetectedJam);
    }

    recordTest("Hard jam detected after normal flow", jamDetected && jamDetectionTime <= 10,
               jamDetected ? "Detected at T+" + std::to_string(jamDetectionTime) + "s" : "Not detected");
    recordTest("Hard jam not detected too early", !jamTooEarly);
}

//=============================================================================
// TEST 11: Complex Flow Sequence (retractions, ironing, travel)
//=============================================================================
void testComplexFlowSequence() {
    printTestHeader("Test 11: Complex Flow Sequence");

    FilamentMotionSensor sensor;
    sensor.setTrackingMode(TRACKING_MODE_WINDOWED, 5000, 0.3f);
    sensor.reset();
    _mockMillis = 0;

    float totalExtrusion = 0.0f;
    bool falsePositive = false;

    // Steady extrusion
    for (int sec = 0; sec < 5; sec++) {
        float deltaExtrusion = 20.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        simulateSensorPulses(sensor, deltaExtrusion, 1.0f);
        advanceTime(CHECK_INTERVAL_MS);
    }
    bool steadyJam = checkJamAndLog(sensor, "Post steady section");
    printState(sensor, "Post steady section", steadyJam);

    // Retraction sequence
    totalExtrusion -= 5.0f;
    simulateExtrusion(sensor, -5.0f, totalExtrusion);
    advanceTime(CHECK_INTERVAL_MS);
    bool afterRetractJam = checkJamAndLog(sensor, "After retraction");
    printState(sensor, "After retraction", afterRetractJam);

    // Resume normal printing
    for (int sec = 0; sec < 4; sec++) {
        float deltaExtrusion = 15.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        simulateSensorPulses(sensor, deltaExtrusion, 1.0f);
        advanceTime(CHECK_INTERVAL_MS);
        std::string label = "Resumed T+" + std::to_string(sec + 1) + "s";
        bool jammed = checkJamAndLog(sensor, label);
        if (jammed) {
            falsePositive = true;
            printState(sensor, label, jammed);
        }
    }
    bool resumedJam = checkJamAndLog(sensor, "Resumed after retract");
    printState(sensor, "Resumed after retract", resumedJam);

    // Long travel gap
    for (int sec = 0; sec < 8; sec++) {
        advanceTime(CHECK_INTERVAL_MS);
        std::string label = "Travel gap T+" + std::to_string(sec + 1) + "s";
        bool jammed = checkJamAndLog(sensor, label);
        if (jammed) {
            falsePositive = true;
            printState(sensor, label, jammed);
        }
    }
    bool afterTravelJam = checkJamAndLog(sensor, "After travel gap");
    printState(sensor, "After travel gap", afterTravelJam);

    // Ironing / low-flow micro moves
    for (int sec = 0; sec < 15; sec++) {
        float deltaExtrusion = 0.3f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        sensor.addSensorPulse(deltaExtrusion);
        advanceTime(CHECK_INTERVAL_MS);

        std::string label = "Ironing spike T+" + std::to_string(sec + 1) + "s";
        bool jammed = checkJamAndLog(sensor, label);
        if (jammed) {
            falsePositive = true;
            printState(sensor, label, jammed);
        }
    }
    bool afterIroningJam = checkJamAndLog(sensor, "After ironing");
    printState(sensor, "After ironing", afterIroningJam);

    // Another travel with sparse expected movement
    for (int sec = 0; sec < 6; sec++) {
        advanceTime(CHECK_INTERVAL_MS);
        std::string label = "Extended travel T+" + std::to_string(sec + 1) + "s";
        checkJamAndLog(sensor, label);
    }
    bool extendedTravelJam = checkJamAndLog(sensor, "Extended travel");
    printState(sensor, "Extended travel", extendedTravelJam);

    bool postTravelJam = checkJamAndLog(sensor, "Post travel jam");
    if (postTravelJam) {
        falsePositive = true;
    }
    printState(sensor, "Post travel jam", postTravelJam);

    recordTest("Complex flow remains jam-free", !falsePositive);
}

//=============================================================================
// Main test runner
//=============================================================================
int main(int argc, char** argv) {
    const std::string DEFAULT_LOG_PATH = "render/filament_log.csv";
    std::string logPath;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--log") {
            logPath = DEFAULT_LOG_PATH;
        } else if (arg.rfind("--log=", 0) == 0) {
            logPath = arg.substr(6);
        } else if (arg == "--log-file" && i + 1 < argc) {
            logPath = argv[++i];
        }
    }
    std::cout << COLOR_BLUE << "\n"
              << "╔════════════════════════════════════════════════════════════╗\n"
              << "║        Filament Motion Sensor - Pulse Simulator           ║\n"
              << "║                     Unit Test Suite                        ║\n"
              << "╚════════════════════════════════════════════════════════════╝\n"
              << COLOR_RESET << "\n";

    std::cout << "Configuration:\n"
              << "  MM_PER_PULSE: " << MM_PER_PULSE << "mm\n"
              << "  CHECK_INTERVAL: " << CHECK_INTERVAL_MS << "ms\n"
              << "  RATIO_THRESHOLD: " << (RATIO_THRESHOLD * 100) << "% deficit\n"
              << "  HARD_JAM_MM: " << HARD_JAM_MM << "mm\n"
              << "  SOFT_JAM_TIME: " << SOFT_JAM_TIME_MS << "ms\n"
              << "  HARD_JAM_TIME: " << HARD_JAM_TIME_MS << "ms\n"
              << "  GRACE_PERIOD: " << GRACE_PERIOD_MS << "ms\n";

    if (!logPath.empty()) {
        initLogFile(logPath);
        std::cout << "Logging simulator state to: " << logPath << "\n";
    }

    // Run all tests
    testNormalPrinting();
    testHardJam();
    testSoftJam();
    testSparseInfill();
    testRetractions();
    testIroningLowFlow();
    testTransientSpikes();
    testMinimumMovement();
    testGracePeriod();
    testHardSnagMidPrint();
    testComplexFlowSequence();

    // Summary
    int passed = 0;
    int failed = 0;
    for (const auto& result : testResults) {
        if (result.passed) passed++;
        else failed++;
    }

    std::cout << "\n" << COLOR_BLUE << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                      TEST SUMMARY                          ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝" << COLOR_RESET << "\n";
    std::cout << "  Total: " << testResults.size() << " tests\n";
    std::cout << "  " << COLOR_GREEN << "Passed: " << passed << COLOR_RESET << "\n";
    std::cout << "  " << (failed > 0 ? COLOR_RED : COLOR_GREEN) << "Failed: " << failed << COLOR_RESET << "\n";

    if (failed > 0) {
        std::cout << "\n" << COLOR_RED << "Failed tests:" << COLOR_RESET << "\n";
        for (const auto& result : testResults) {
            if (!result.passed) {
                std::cout << "  - " << result.name;
                if (!result.details.empty()) {
                    std::cout << " (" << result.details << ")";
                }
                std::cout << "\n";
            }
        }
    }

    std::cout << "\n";
    closeLogFile();
    return (failed == 0) ? 0 : 1;
}
