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
const float RATIO_THRESHOLD = 0.70f;
const float HARD_JAM_MM = 5.0f;
const int SOFT_JAM_TIME_MS = 3000;
const int HARD_JAM_TIME_MS = 2000;
const int GRACE_PERIOD_MS = 500;

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

// Helper: Print sensor state
void printState(const FilamentMotionSensor& sensor, const std::string& label) {
    float expected = sensor.getExpectedDistance();
    float actual = sensor.getSensorDistance();
    float deficit = sensor.getDeficit();
    float ratio = sensor.getFlowRatio();
    bool jammed = checkJam(sensor);

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

        if (checkJam(sensor)) {
            anyFalsePositive = true;
            printState(sensor, "FALSE POSITIVE!");
        }
    }

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

    printState(sensor, "Before jam (healthy)");

    // Hard jam: extrusion commands but NO sensor pulses
    bool jamDetectedAt2Sec = false;
    bool jamDetectedBefore2Sec = false;

    for (int sec = 0; sec < 5; sec++) {
        float deltaExtrusion = 20.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        // NO sensor pulses - complete blockage
        advanceTime(CHECK_INTERVAL_MS);

        bool jammed = checkJam(sensor);
        if (jammed && sec < 2) jamDetectedBefore2Sec = true;
        if (jammed && sec == 2) jamDetectedAt2Sec = true;

        printState(sensor, "Jam T+" + std::to_string(sec+1) + "s");
    }

    recordTest("Hard jam detected at 2 seconds", jamDetectedAt2Sec);
    recordTest("Hard jam not detected too early", !jamDetectedBefore2Sec);
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

    printState(sensor, "Before jam (healthy)");

    // Soft jam: only 20% of filament passing (80% deficit)
    // With windowed tracking, it takes time for the window to fill with bad samples
    // Expected: Should detect within reasonable time (not instant, but before 10 seconds)
    bool jamDetected = false;
    int jamDetectionTime = -1;

    for (int sec = 0; sec < 10; sec++) {
        float deltaExtrusion = 20.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        simulateSensorPulses(sensor, deltaExtrusion, 0.20f);  // 20% flow rate (80% deficit)
        advanceTime(CHECK_INTERVAL_MS);

        bool jammed = checkJam(sensor);
        if (jammed && !jamDetected) {
            jamDetected = true;
            jamDetectionTime = sec + 1;
        }

        if (sec < 8) {
            printState(sensor, "Clog T+" + std::to_string(sec+1) + "s");
        }
    }

    recordTest("Soft jam detected within reasonable time", jamDetected && jamDetectionTime <= 7,
               jamDetected ? "Detected at T+" + std::to_string(jamDetectionTime) + "s" : "Not detected");
    recordTest("Soft jam not instant (allows window to fill)", jamDetectionTime >= 3);
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

    printState(sensor, "Before sparse infill");

    // Sparse infill: 10 seconds of travel with minimal extrusion
    for (int sec = 0; sec < 10; sec++) {
        // No telemetry updates during travel
        advanceTime(CHECK_INTERVAL_MS);

        if (checkJam(sensor)) {
            falsePositive = true;
            printState(sensor, "Travel T+" + std::to_string(sec+1) + "s");
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

        if (checkJam(sensor)) {
            falsePositive = true;
            printState(sensor, "After gap T+" + std::to_string(sec+1) + "s");
        }
    }

    printState(sensor, "After resume");

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

    printState(sensor, "Before retraction");

    // Retraction (negative movement)
    totalExtrusion -= 5.0f;
    simulateExtrusion(sensor, -5.0f, totalExtrusion);
    advanceTime(CHECK_INTERVAL_MS);

    printState(sensor, "After retraction");

    // Resume after retraction (grace period should apply)
    for (int sec = 0; sec < 3; sec++) {
        float deltaExtrusion = 20.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);

        // Grace period: wait 500ms
        if (sec == 0) advanceTime(500);

        simulateSensorPulses(sensor, deltaExtrusion, 1.0f);
        advanceTime(CHECK_INTERVAL_MS - (sec == 0 ? 500 : 0));

        if (checkJam(sensor)) {
            falsePositive = true;
        }

        printState(sensor, "After retract T+" + std::to_string(sec+1) + "s");
    }

    recordTest("No false positives after retraction", !falsePositive);
}

//=============================================================================
// TEST 6: Transient Spike Resistance (Hysteresis)
//=============================================================================
void testTransientSpikes() {
    printTestHeader("Test 6: Transient Spike Resistance");

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

    if (checkJam(sensor)) {
        falsePositive = true;
    }
    printState(sensor, "Single spike T+1s");

    // Ratio returns to normal (should reset counter)
    for (int sec = 0; sec < 3; sec++) {
        float deltaExtrusion = 20.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        simulateSensorPulses(sensor, deltaExtrusion, 1.0f);
        advanceTime(CHECK_INTERVAL_MS);

        if (checkJam(sensor)) {
            falsePositive = true;
        }

        printState(sensor, "After spike T+" + std::to_string(sec+2) + "s");
    }

    recordTest("Transient spike did not trigger jam", !falsePositive);
}

//=============================================================================
// TEST 7: Edge Case - Minimum Movement Threshold
//=============================================================================
void testMinimumMovement() {
    printTestHeader("Test 7: Minimum Movement Threshold");

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

        if (checkJam(sensor)) {
            falsePositiveSubThreshold = true;
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

        if (checkJam(sensor)) {
            hardJamDetected = true;
        }
    }

    printState(sensor, "Slow print, no pulses");

    recordTest("Hard jam detected on slow print without pulses", hardJamDetected);
}

//=============================================================================
// TEST 8: Grace Period Duration
//=============================================================================
void testGracePeriod() {
    printTestHeader("Test 8: Grace Period Duration");

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
    bool jamAt400 = checkJam(sensor);
    printState(sensor, "At 400ms (in grace)");

    // Grace period should still be active - no jam yet
    recordTest("Grace period protects at 400ms after gap", !jamAt400);

    // Advance past grace period and continue extrusion without pulses
    advanceTime(200);  // Now at 600ms - grace period expired

    // Add more extrusion without pulses to ensure hard jam triggers
    for (int i = 0; i < 3; i++) {
        deltaExtrusion = 20.0f;
        totalExtrusion += deltaExtrusion;
        simulateExtrusion(sensor, deltaExtrusion, totalExtrusion);
        advanceTime(CHECK_INTERVAL_MS);
    }

    bool jamAfterGrace = checkJam(sensor);
    printState(sensor, "After grace period");

    recordTest("Detection active after grace period expires", jamAfterGrace);
}

//=============================================================================
// Main test runner
//=============================================================================
int main() {
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

    // Run all tests
    testNormalPrinting();
    testHardJam();
    testSoftJam();
    testSparseInfill();
    testRetractions();
    testTransientSpikes();
    testMinimumMovement();
    testGracePeriod();

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
    return (failed == 0) ? 0 : 1;
}
