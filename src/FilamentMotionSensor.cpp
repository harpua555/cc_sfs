#include "FilamentMotionSensor.h"

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
    lastExpectedUpdateMs  = 0;

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
    // Track significant changes (> 0.1mm) to trigger grace period
    else if (abs(totalExtrusionMm - expectedPositionMm) > 0.1f)
    {
        lastExpectedUpdateMs = currentTime;
    }

    // Calculate deltas for windowed/EWMA modes
    float expectedDelta = totalExtrusionMm - expectedPositionMm;

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

bool FilamentMotionSensor::isJammed(float detectionLengthMm, unsigned long gracePeriodMs) const
{
    if (!initialized || detectionLengthMm <= 0.0f)
    {
        return false;
    }

    // Grace period: Don't check immediately after expected position update
    if (gracePeriodMs > 0)
    {
        unsigned long timeSinceUpdate = millis() - lastExpectedUpdateMs;
        if (timeSinceUpdate < gracePeriodMs)
        {
            return false;  // Still within grace period
        }
    }

    // Get distances based on tracking mode
    float expectedDistance = getExpectedDistance();
    float actualDistance   = getSensorDistance();

    // Minimum distance check (prevents false positives on tiny movements)
    float minDistanceBeforeCheck = detectionLengthMm * 0.5f;
    if (expectedDistance < minDistanceBeforeCheck)
    {
        return false;
    }

    // Calculate deficit
    float deficit = expectedDistance - actualDistance;
    if (deficit < 0.0f) deficit = 0.0f;

    return deficit > detectionLengthMm;
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

    // Need at least 1mm of expected movement for meaningful ratio
    if (expectedDistance < 1.0f)
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
