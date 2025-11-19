#include "FilamentFlowTracker.h"

FilamentFlowTracker::FilamentFlowTracker()
{
    reset();
}

void FilamentFlowTracker::reset()
{
    head            = 0;
    count           = 0;
    outstandingMm   = 0;
    deficitStartMs  = 0;
    deficitActive   = false;
    for (size_t i = 0; i < MAX_CHUNKS; i++)
    {
        chunks[i].timestamp = 0;
        chunks[i].remaining = 0;
    }
}

void FilamentFlowTracker::addExpected(float amount, unsigned long timestamp,
                                      unsigned long pruneWindowMs)
{
    if (amount <= 0)
    {
        return;
    }

    prune(timestamp, pruneWindowMs);
    if (count >= MAX_CHUNKS)
    {
        discardOldest();
    }

    size_t index            = (head + count) % MAX_CHUNKS;
    chunks[index].timestamp = timestamp;
    chunks[index].remaining = amount;
    if (count < MAX_CHUNKS)
    {
        count++;
    }
    outstandingMm += amount;
}

void FilamentFlowTracker::addActual(float amount)
{
    float remaining = amount;
    while (remaining > 0 && count > 0)
    {
        FlowChunk &chunk = chunks[head];
        float      use   = chunk.remaining < remaining ? chunk.remaining : remaining;
        chunk.remaining -= use;
        remaining -= use;
        outstandingMm -= use;

        if (chunk.remaining <= 0.0001f)
        {
            discardOldest(false);
        }
    }

    if (outstandingMm < 0)
    {
        outstandingMm = 0;
    }
}

float FilamentFlowTracker::outstanding(unsigned long now, unsigned long pruneWindowMs)
{
    prune(now, pruneWindowMs);
    if (outstandingMm < 0)
    {
        outstandingMm = 0;
    }
    return outstandingMm;
}

bool FilamentFlowTracker::deficitSatisfied(float outstandingValue, unsigned long now,
                                           float threshold, unsigned long holdWindowMs)
{
    if (threshold <= 0 || holdWindowMs == 0)
    {
        deficitActive  = false;
        deficitStartMs = 0;
        return false;
    }

    if (outstandingValue >= threshold)
    {
        if (!deficitActive)
        {
            deficitActive  = true;
            deficitStartMs = now;
        }
    }
    else
    {
        deficitActive  = false;
        deficitStartMs = 0;
    }

    if (!deficitActive)
    {
        return false;
    }

    return (now - deficitStartMs) >= holdWindowMs;
}

unsigned long FilamentFlowTracker::getDeficitStartMs() const
{
    return deficitActive ? deficitStartMs : 0;
}

void FilamentFlowTracker::discardOldest(bool adjustOutstanding)
{
    if (count == 0)
    {
        return;
    }

    if (adjustOutstanding)
    {
        outstandingMm -= chunks[head].remaining;
        if (outstandingMm < 0)
        {
            outstandingMm = 0;
        }
    }

    head = (head + 1) % MAX_CHUNKS;
    if (count > 0)
    {
        count--;
    }
}

void FilamentFlowTracker::prune(unsigned long now, unsigned long pruneWindowMs)
{
    if (pruneWindowMs == 0)
    {
        return;
    }

    while (count > 0)
    {
        unsigned long age =
            now >= chunks[head].timestamp ? now - chunks[head].timestamp : 0;
        if (age > pruneWindowMs)
        {
            discardOldest();
        }
        else
        {
            break;
        }
    }
}
