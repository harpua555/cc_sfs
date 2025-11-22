#include "FilamentMotionSensor.h"

static const unsigned long INVALID_SAMPLE_TIMESTAMP = ~0UL;

FilamentMotionSensor::FilamentMotionSensor()
{
    trackingMode = TRACKING_MODE_WINDOWED;  // Default to windowed mode
    windowSizeMs = 5000;  // 5 second window
    ewmaAlpha = 0.3f;     // 30% weight on new samples
    reset();
}

void FilamentMotionSensor::reset()
{
    initialized           = false;
    lastExpectedUpdateMs  = millis();

    // Reset cumulative state
    baselinePositionMm    = 0.0f;
    expectedPositionMm    = 0.0f;
    sensorDistanceMm      = 0.0f;

    // Reset windowed state
    sampleCount           = 0;
    nextSampleIndex       = 0;
    for (int i = 0; i < MAX_SAMPLES; i++)
    {
        samples[i].timestampMs = 0;
        samples[i].expectedMm  = 0.0f;
        samples[i].actualMm    = 0.0f;
    }

    // Reset EWMA state
    ewmaExpectedMm        = 0.0f;
    ewmaActualMm          = 0.0f;
    ewmaLastExpectedMm    = 0.0f;
    ewmaLastActualMm      = 0.0f;

    // Reset jam tracking
    lastWindowDeficitMm     = 0.0f;
    lastDeficitTimestampMs  = 0;
    hardJamStartMs          = 0;
    softJamStartMs          = 0;
    hardJamAccumExpectedMm  = 0.0f;
    hardJamAccumActualMm    = 0.0f;
    hardJamLastSampleMs     = INVALID_SAMPLE_TIMESTAMP;
    softJamAccumExpectedMm  = 0.0f;
    softJamAccumActualMm    = 0.0f;
    softJamLastSampleMs     = INVALID_SAMPLE_TIMESTAMP;
}

void FilamentMotionSensor::setTrackingMode(FilamentTrackingMode mode, unsigned long windowMs,
                                           float alpha)
{
    trackingMode = mode;
    windowSizeMs = windowMs;
    ewmaAlpha    = alpha;

    // Clamp EWMA alpha to valid range
    if (ewmaAlpha < 0.01f) ewmaAlpha = 0.01f;
    if (ewmaAlpha > 1.0f) ewmaAlpha = 1.0f;
}

void FilamentMotionSensor::updateExpectedPosition(float totalExtrusionMm)
{
    unsigned long currentTime = millis();

    if (!initialized)
    {
        // First telemetry received - establish baseline for all modes
        initialized           = true;
        lastExpectedUpdateMs  = currentTime;

        // Cumulative mode init
        baselinePositionMm    = totalExtrusionMm;
        expectedPositionMm    = totalExtrusionMm;
        sensorDistanceMm      = 0.0f;

        // EWMA mode init
        ewmaLastExpectedMm    = totalExtrusionMm;
        ewmaLastActualMm      = 0.0f;
        ewmaExpectedMm        = 0.0f;
        ewmaActualMm          = 0.0f;

        return;
    }

    // Handle retractions: reset tracking for all modes
    if (totalExtrusionMm < expectedPositionMm)
    {
        // Retraction detected - resync everything
        lastExpectedUpdateMs  = currentTime;

        // Reset cumulative tracking
        baselinePositionMm   = totalExtrusionMm;
        sensorDistanceMm     = 0.0f;

        // Reset windowed tracking
        sampleCount          = 0;
        nextSampleIndex      = 0;

        // Reset EWMA tracking
        ewmaLastExpectedMm   = totalExtrusionMm;
        ewmaLastActualMm     = sensorDistanceMm;
        ewmaExpectedMm       = 0.0f;
        ewmaActualMm         = 0.0f;
    }

    // Calculate deltas for windowed/EWMA modes
    float expectedDelta = totalExtrusionMm - expectedPositionMm;

    // Telemetry gap detection: If no updates for >2 seconds, reset grace period
    // This handles sparse infill, travel moves, print pauses, speed changes
    // Grace period applies after: (1) initialization, (2) retractions, (3) telemetry gaps
    unsigned long timeSinceLastUpdate = currentTime - lastExpectedUpdateMs;
    if (timeSinceLastUpdate > 2000 && expectedDelta > 0.01f)
    {
        // Telemetry gap detected - reset grace period timer when extrusion resumes
        lastExpectedUpdateMs = currentTime;
    }

    if (trackingMode == TRACKING_MODE_WINDOWED && expectedDelta > 0.01f)
    {
        // Add sample with zero actual (will be updated by sensor pulses)
        addSample(expectedDelta, 0.0f);
    }
    else if (trackingMode == TRACKING_MODE_EWMA && expectedDelta > 0.01f)
    {
        // Update EWMA with new expected distance (actual updated by sensor)
        float newExpected = totalExtrusionMm - ewmaLastExpectedMm;
        ewmaExpectedMm = ewmaAlpha * newExpected + (1.0f - ewmaAlpha) * ewmaExpectedMm;
        ewmaLastExpectedMm = totalExtrusionMm;
    }

    // Update cumulative tracking
    expectedPositionMm = totalExtrusionMm;
}

void FilamentMotionSensor::addSensorPulse(float mmPerPulse)
{
    if (mmPerPulse <= 0.0f || !initialized)
    {
        return;
    }

    unsigned long currentTime = millis();

    // Update cumulative tracking
    sensorDistanceMm += mmPerPulse;

    // Update windowed tracking - add actual distance to most recent sample
    if (trackingMode == TRACKING_MODE_WINDOWED && sampleCount > 0)
    {
        // Find most recent sample and add actual distance
        int mostRecentIndex = (nextSampleIndex - 1 + MAX_SAMPLES) % MAX_SAMPLES;
        if (mostRecentIndex >= 0 && mostRecentIndex < sampleCount)
        {
            samples[mostRecentIndex].actualMm += mmPerPulse;
        }
    }

    // Update EWMA tracking
    if (trackingMode == TRACKING_MODE_EWMA)
    {
        float actualDelta = mmPerPulse;
        ewmaActualMm = ewmaAlpha * actualDelta + (1.0f - ewmaAlpha) * ewmaActualMm;
    }
}

void FilamentMotionSensor::addSample(float expectedDeltaMm, float actualDeltaMm)
{
    unsigned long currentTime = millis();

    // Prune old samples first
    pruneOldSamples();

    // Add new sample
    samples[nextSampleIndex].timestampMs = currentTime;
    samples[nextSampleIndex].expectedMm  = expectedDeltaMm;
    samples[nextSampleIndex].actualMm    = actualDeltaMm;

    nextSampleIndex = (nextSampleIndex + 1) % MAX_SAMPLES;
    if (sampleCount < MAX_SAMPLES)
    {
        sampleCount++;
    }
}

void FilamentMotionSensor::pruneOldSamples()
{
    unsigned long currentTime = millis();
    unsigned long cutoffTime  = currentTime - windowSizeMs;

    // Remove samples older than window
    int newCount = 0;
    for (int i = 0; i < sampleCount; i++)
    {
        int idx = (nextSampleIndex - sampleCount + i + MAX_SAMPLES) % MAX_SAMPLES;
        if (samples[idx].timestampMs >= cutoffTime)
        {
            // Keep this sample
            if (newCount != i)
            {
                // Compact array
                int newIdx = (nextSampleIndex - sampleCount + newCount + MAX_SAMPLES) % MAX_SAMPLES;
                samples[newIdx] = samples[idx];
            }
            newCount++;
        }
    }

    sampleCount = newCount;
    // Update nextSampleIndex to point after the last valid sample
    // This ensures mostRecentIndex calculation works correctly after pruning
    if (sampleCount > 0)
    {
        nextSampleIndex = sampleCount;
    }
    else
    {
        nextSampleIndex = 0;
    }
}

void FilamentMotionSensor::getWindowedDistances(float &expectedMm, float &actualMm) const
{
    expectedMm = 0.0f;
    actualMm   = 0.0f;

    // Sum all samples in window
    for (int i = 0; i < sampleCount; i++)
    {
        int idx = (nextSampleIndex - sampleCount + i + MAX_SAMPLES) % MAX_SAMPLES;
        expectedMm += samples[idx].expectedMm;
        actualMm   += samples[idx].actualMm;
    }
}

bool FilamentMotionSensor::isJammed(float ratioThreshold, float hardJamThresholdMm,
                                    int softJamTimeMs, int hardJamTimeMs, int checkIntervalMs,
                                    unsigned long gracePeriodMs) const
{
    if (!initialized || checkIntervalMs <= 0)
    {
        hardJamStartMs = 0;
        softJamStartMs = 0;
        return false;
    }

    if (ratioThreshold <= 0.0f)
    {
        ratioThreshold = 0.25f;
    }
    if (ratioThreshold > 1.0f)
    {
        ratioThreshold = 1.0f;
    }
    if (softJamTimeMs <= 0)
    {
        softJamTimeMs = 10000;
    }
    if (hardJamTimeMs <= 0)
    {
        hardJamTimeMs = 5000;
    }

    unsigned long currentTime = millis();
    if (gracePeriodMs > 0)
    {
        unsigned long timeSinceUpdate = currentTime - lastExpectedUpdateMs;
        if (timeSinceUpdate < gracePeriodMs)
        {
            hardJamStartMs = 0;
            softJamStartMs = 0;
            return false;
        }
    }

    float expectedDistance = getExpectedDistance();
    float actualDistance   = getSensorDistance();
    float windowDeficit    = expectedDistance - actualDistance;
    if (windowDeficit < 0.0f)
    {
        windowDeficit = 0.0f;
    }

    unsigned long deltaMs = (lastDeficitTimestampMs == 0) ? 0 : (currentTime - lastDeficitTimestampMs);
    float deficitGrowthRateMmPerSec = 0.0f;
    if (deltaMs > 0)
    {
        float deltaDeficit = windowDeficit - lastWindowDeficitMm;
        deficitGrowthRateMmPerSec = (deltaDeficit * 1000.0f) / deltaMs;
        if (deficitGrowthRateMmPerSec < 0.0f)
        {
            deficitGrowthRateMmPerSec = 0.0f;
        }
    }
    lastWindowDeficitMm    = windowDeficit;
    lastDeficitTimestampMs = currentTime;

    float passingRatio = (expectedDistance > 0.0f) ? (actualDistance / expectedDistance) : 1.0f;
    if (passingRatio < 0.0f)
    {
        passingRatio = 0.0f;
    }

    float latestExpected = 0.0f;
    int latestSampleIndex = -1;
    if (sampleCount > 0)
    {
        latestSampleIndex = (nextSampleIndex - 1 + MAX_SAMPLES) % MAX_SAMPLES;
        if (latestSampleIndex >= 0 && latestSampleIndex < MAX_SAMPLES)
        {
            latestExpected = samples[latestSampleIndex].expectedMm;
        }
    }

    const float MIN_EXPECTED_DELTA = 0.05f;
    const float HARD_PASS_RATIO_THRESHOLD = 0.10f;
    const float MIN_HARD_EXPECTED_MM    = hardJamThresholdMm;
    const float MIN_SOFT_EXPECTED_MM    = hardJamThresholdMm / 2.0f;
    const float MIN_SOFT_DEFICIT_MM     = 0.5f;

    bool expectedAdvancing = latestExpected >= MIN_EXPECTED_DELTA;

    if (expectedAdvancing)
    {
        if (latestSampleIndex >= 0 && latestSampleIndex < MAX_SAMPLES)
        {
            unsigned long sampleMs = samples[latestSampleIndex].timestampMs;
            if (sampleMs != INVALID_SAMPLE_TIMESTAMP && sampleMs != hardJamLastSampleMs)
            {
                hardJamAccumExpectedMm += samples[latestSampleIndex].expectedMm;
                hardJamAccumActualMm += samples[latestSampleIndex].actualMm;
                hardJamLastSampleMs = sampleMs;
            }
        }

        float hardAccumRatio = (hardJamAccumExpectedMm > 0.0f)
                                   ? (hardJamAccumActualMm / hardJamAccumExpectedMm)
                                   : 1.0f;

        if (hardAccumRatio < HARD_PASS_RATIO_THRESHOLD)
        {
            if (hardJamStartMs == 0)
            {
                hardJamStartMs = currentTime;
            }

            if (currentTime - hardJamStartMs >= (unsigned long)hardJamTimeMs)
            {
                return true;
            }
        }
        else
        {
            hardJamStartMs = 0;
            hardJamAccumExpectedMm = 0.0f;
            hardJamAccumActualMm = 0.0f;
            hardJamLastSampleMs = INVALID_SAMPLE_TIMESTAMP;
        }

            if (latestSampleIndex >= 0 && latestSampleIndex < MAX_SAMPLES)
            {
                unsigned long sampleMs = samples[latestSampleIndex].timestampMs;
                if (sampleMs != INVALID_SAMPLE_TIMESTAMP && sampleMs != softJamLastSampleMs)
                {
                    softJamAccumExpectedMm += samples[latestSampleIndex].expectedMm;
                    softJamAccumActualMm += samples[latestSampleIndex].actualMm;
                    softJamLastSampleMs = sampleMs;
                }
            }

        float softAccumRatio = (softJamAccumExpectedMm > 0.0f)
                                   ? (softJamAccumActualMm / softJamAccumExpectedMm)
                                   : 1.0f;
        float softAccumDeficit = softJamAccumExpectedMm - softJamAccumActualMm;
        if (softAccumDeficit < 0.0f)
        {
            softAccumDeficit = 0.0f;
        }

        if (softAccumRatio < ratioThreshold)
        {
            if (softJamStartMs == 0)
            {
                softJamStartMs = currentTime;
            }

            if (softAccumDeficit >= MIN_SOFT_DEFICIT_MM &&
                currentTime - softJamStartMs >= (unsigned long)softJamTimeMs)
            {
                return true;
            }
        }
        else
        {
            softJamStartMs = 0;
            softJamAccumExpectedMm = 0.0f;
            softJamAccumActualMm = 0.0f;
            softJamLastSampleMs = INVALID_SAMPLE_TIMESTAMP;
        }
    }
    else
    {
        hardJamStartMs = 0;
        hardJamAccumExpectedMm = 0.0f;
        hardJamAccumActualMm = 0.0f;
        hardJamLastSampleMs = INVALID_SAMPLE_TIMESTAMP;
        softJamStartMs = 0;
        softJamAccumExpectedMm = 0.0f;
        softJamAccumActualMm = 0.0f;
        softJamLastSampleMs = INVALID_SAMPLE_TIMESTAMP;
    }

    return false;
}

float FilamentMotionSensor::getDeficit() const
{
    if (!initialized)
    {
        return 0.0f;
    }

    float expectedDistance = getExpectedDistance();
    float actualDistance   = getSensorDistance();
    float deficit          = expectedDistance - actualDistance;

    return deficit > 0.0f ? deficit : 0.0f;
}

float FilamentMotionSensor::getExpectedDistance() const
{
    if (!initialized)
    {
        return 0.0f;
    }

    switch (trackingMode)
    {
        case TRACKING_MODE_CUMULATIVE:
            return expectedPositionMm - baselinePositionMm;

        case TRACKING_MODE_WINDOWED:
        {
            float expectedMm, actualMm;
            getWindowedDistances(expectedMm, actualMm);
            return expectedMm;
        }

        case TRACKING_MODE_EWMA:
            return ewmaExpectedMm;

        default:
            return 0.0f;
    }
}

float FilamentMotionSensor::getSensorDistance() const
{
    if (!initialized)
    {
        return 0.0f;
    }

    switch (trackingMode)
    {
        case TRACKING_MODE_CUMULATIVE:
            return sensorDistanceMm;

        case TRACKING_MODE_WINDOWED:
        {
            float expectedMm, actualMm;
            getWindowedDistances(expectedMm, actualMm);
            return actualMm;
        }

        case TRACKING_MODE_EWMA:
            return ewmaActualMm;

        default:
            return 0.0f;
    }
}

bool FilamentMotionSensor::isInitialized() const
{
    return initialized;
}

float FilamentMotionSensor::getFlowRatio() const
{
    if (!initialized)
    {
        return 0.0f;
    }

    float expectedDistance = getExpectedDistance();
    if (expectedDistance <= 0.0f)
    {
        return 0.0f;
    }

    float actualDistance = getSensorDistance();
    float ratio = actualDistance / expectedDistance;

    // Clamp to reasonable range [0, 1.5]
    if (ratio > 1.5f) ratio = 1.5f;
    if (ratio < 0.0f) ratio = 0.0f;

    return ratio;
}
