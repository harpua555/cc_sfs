#include <unity.h>

#include "../../src/FilamentFlowTracker.h"
#include "../../src/FilamentFlowTracker.cpp"

void setUp() {}
void tearDown() {}

void test_deficit_requires_hold_window()
{
    FilamentFlowTracker tracker;
    tracker.addExpected(8.0f, 0, 2000);

    float outstanding = tracker.outstanding(0, 2000);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 8.0f, outstanding);
    TEST_ASSERT_FALSE(tracker.deficitSatisfied(outstanding, 0, 5.0f, 1000));

    outstanding = tracker.outstanding(1000, 2000);
    TEST_ASSERT_TRUE(tracker.deficitSatisfied(outstanding, 1000, 5.0f, 1000));
}

void test_actual_flow_clears_deficit()
{
    FilamentFlowTracker tracker;
    tracker.addExpected(10.0f, 0, 2000);
    tracker.addActual(6.0f);

    float outstanding = tracker.outstanding(500, 2000);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, outstanding);
    TEST_ASSERT_FALSE(tracker.deficitSatisfied(outstanding, 500, 5.0f, 500));

    tracker.addActual(4.0f);
    outstanding = tracker.outstanding(600, 2000);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, outstanding);
    TEST_ASSERT_FALSE(tracker.deficitSatisfied(outstanding, 600, 5.0f, 500));
}

void test_prune_discards_stale_expectations()
{
    FilamentFlowTracker tracker;
    tracker.addExpected(3.0f, 0, 2000);
    tracker.addExpected(3.0f, 500, 2000);

    float outstanding = tracker.outstanding(500, 2000);
    tracker.deficitSatisfied(outstanding, 500, 2.0f, 500);

    outstanding = tracker.outstanding(2500, 2000);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, outstanding);
    TEST_ASSERT_TRUE(tracker.deficitSatisfied(outstanding, 2500, 2.0f, 500));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_deficit_requires_hold_window);
    RUN_TEST(test_actual_flow_clears_deficit);
    RUN_TEST(test_prune_discards_stale_expectations);
    return UNITY_END();
}
