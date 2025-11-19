#ifndef FILAMENT_FLOW_TRACKER_H
#define FILAMENT_FLOW_TRACKER_H

#include <stddef.h>
#include <stdint.h>

class FilamentFlowTracker
{
   public:
    FilamentFlowTracker();

    void reset();
    void addExpected(float amount, unsigned long timestamp, unsigned long pruneWindowMs);
    void addActual(float amount);
    float outstanding(unsigned long now, unsigned long pruneWindowMs);
    bool deficitSatisfied(float outstanding, unsigned long now, float threshold,
                          unsigned long holdWindowMs);
    unsigned long getDeficitStartMs() const;

   private:
    struct FlowChunk
    {
        unsigned long timestamp;
        float         remaining;
    };

    static const size_t MAX_CHUNKS = 16;

    FlowChunk     chunks[MAX_CHUNKS];
    size_t        head;
    size_t        count;
    float         outstandingMm;
    unsigned long deficitStartMs;
    bool          deficitActive;

    void discardOldest(bool adjustOutstanding = true);
    void prune(unsigned long now, unsigned long pruneWindowMs);
};

#endif  // FILAMENT_FLOW_TRACKER_H
