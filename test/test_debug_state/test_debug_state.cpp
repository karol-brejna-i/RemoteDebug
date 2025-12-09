/**
 * Unit tests for DebugState
 * Run with: pio test -e native
 */

#include "stubs/Arduino.h"
#include "stubs/RemoteDebugCfg.h"
#include <unity.h>
#include "DebugState.h"

static DebugState state;

void setUp() {
    // Reset state before each test
    state = DebugState();
}

void tearDown() {
    // Nothing to clean up
}

// --- Level Management ---

void test_default_level_is_debug() {
    TEST_ASSERT_EQUAL(DebugState::DEBUG, state.getLevel());
}

void test_set_level() {
    state.setLevel(DebugState::VERBOSE);
    TEST_ASSERT_EQUAL(DebugState::VERBOSE, state.getLevel());

    state.setLevel(DebugState::ERROR);
    TEST_ASSERT_EQUAL(DebugState::ERROR, state.getLevel());
}

void test_last_level() {
    TEST_ASSERT_EQUAL(DebugState::DEBUG, state.getLastLevel());

    state.setLastLevel(DebugState::WARNING);
    TEST_ASSERT_EQUAL(DebugState::WARNING, state.getLastLevel());
}

void test_level_before_profiler() {
    state.setLevel(DebugState::INFO);
    state.setLevelBeforeProfiler(state.getLevel());
    state.setLevel(DebugState::PROFILER);

    TEST_ASSERT_EQUAL(DebugState::PROFILER, state.getLevel());
    TEST_ASSERT_EQUAL(DebugState::INFO, state.getLevelBeforeProfiler());
}

// --- isActive Logic ---

void test_isActive_any_always_true() {
    state.setLevel(DebugState::ERROR);
    TEST_ASSERT_TRUE(state.isActive(DebugState::ANY));

    state.setLevel(DebugState::VERBOSE);
    TEST_ASSERT_TRUE(state.isActive(DebugState::ANY));

    state.setLevel(DebugState::PROFILER);
    TEST_ASSERT_TRUE(state.isActive(DebugState::ANY));
}

void test_isActive_profiler_level() {
    state.setLevel(DebugState::PROFILER);

    TEST_ASSERT_TRUE(state.isActive(DebugState::PROFILER));
    TEST_ASSERT_FALSE(state.isActive(DebugState::VERBOSE));
    TEST_ASSERT_FALSE(state.isActive(DebugState::DEBUG));
    TEST_ASSERT_FALSE(state.isActive(DebugState::ERROR));
}

void test_isActive_normal_levels() {
    state.setLevel(DebugState::DEBUG);

    TEST_ASSERT_FALSE(state.isActive(DebugState::VERBOSE));  // Below DEBUG
    TEST_ASSERT_TRUE(state.isActive(DebugState::DEBUG));     // Equal
    TEST_ASSERT_TRUE(state.isActive(DebugState::INFO));      // Above
    TEST_ASSERT_TRUE(state.isActive(DebugState::WARNING));   // Above
    TEST_ASSERT_TRUE(state.isActive(DebugState::ERROR));     // Above
}

void test_isActive_error_level() {
    state.setLevel(DebugState::ERROR);

    TEST_ASSERT_FALSE(state.isActive(DebugState::VERBOSE));
    TEST_ASSERT_FALSE(state.isActive(DebugState::DEBUG));
    TEST_ASSERT_FALSE(state.isActive(DebugState::INFO));
    TEST_ASSERT_FALSE(state.isActive(DebugState::WARNING));
    TEST_ASSERT_TRUE(state.isActive(DebugState::ERROR));
}

void test_isActive_verbose_level() {
    state.setLevel(DebugState::VERBOSE);

    TEST_ASSERT_TRUE(state.isActive(DebugState::VERBOSE));
    TEST_ASSERT_TRUE(state.isActive(DebugState::DEBUG));
    TEST_ASSERT_TRUE(state.isActive(DebugState::INFO));
    TEST_ASSERT_TRUE(state.isActive(DebugState::WARNING));
    TEST_ASSERT_TRUE(state.isActive(DebugState::ERROR));
}

// --- Display Flags ---

void test_showTime_default_false() {
    TEST_ASSERT_FALSE(state.showTime());
}

void test_showTime_toggle() {
    state.setShowTime(true);
    TEST_ASSERT_TRUE(state.showTime());

    state.setShowTime(false);
    TEST_ASSERT_FALSE(state.showTime());
}

void test_showProfiler() {
    TEST_ASSERT_FALSE(state.showProfiler());

    state.setShowProfiler(true);
    TEST_ASSERT_TRUE(state.showProfiler());
}

void test_minTimeShowProfiler() {
    TEST_ASSERT_EQUAL(0, state.minTimeShowProfiler());

    state.setMinTimeShowProfiler(100);
    TEST_ASSERT_EQUAL(100, state.minTimeShowProfiler());
}

void test_showDebugLevel_default_true() {
    TEST_ASSERT_TRUE(state.showDebugLevel());
}

void test_showDebugLevel_toggle() {
    state.setShowDebugLevel(false);
    TEST_ASSERT_FALSE(state.showDebugLevel());
}

void test_showColors() {
    TEST_ASSERT_FALSE(state.showColors());

    state.setShowColors(true);
    TEST_ASSERT_TRUE(state.showColors());
}

void test_showRaw() {
    TEST_ASSERT_FALSE(state.showRaw());

    state.setShowRaw(true);
    TEST_ASSERT_TRUE(state.showRaw());
}

// --- Filter ---

void test_filter_default_inactive() {
    TEST_ASSERT_FALSE(state.filterActive());
    TEST_ASSERT_TRUE(state.filter().isEmpty());
}

void test_setFilter() {
    state.setFilter("error");

    TEST_ASSERT_TRUE(state.filterActive());
    TEST_ASSERT_EQUAL_STRING("error", state.filter().c_str());
}

void test_setFilter_converts_to_lowercase() {
    state.setFilter("ERROR");

    TEST_ASSERT_EQUAL_STRING("error", state.filter().c_str());
}

void test_clearFilter() {
    state.setFilter("test");
    TEST_ASSERT_TRUE(state.filterActive());

    state.clearFilter();
    TEST_ASSERT_FALSE(state.filterActive());
    TEST_ASSERT_TRUE(state.filter().isEmpty());
}

// --- Silence ---

void test_silence_default_false() {
    TEST_ASSERT_FALSE(state.isSilence());
}

void test_silence_toggle() {
    state.setSilence(true);
    TEST_ASSERT_TRUE(state.isSilence());

    state.setSilence(false);
    TEST_ASSERT_FALSE(state.isSilence());
}

void test_silence_timeout() {
    TEST_ASSERT_EQUAL(0, state.silenceTimeout());

    state.setSilenceTimeout(5000);
    TEST_ASSERT_EQUAL(5000, state.silenceTimeout());
}

// --- Serial ---

void test_serialEnabled_default_false() {
    TEST_ASSERT_FALSE(state.serialEnabled());
}

void test_serialEnabled_toggle() {
    state.setSerialEnabled(true);
    TEST_ASSERT_TRUE(state.serialEnabled());
}

// --- Timing ---

void test_lastTimePrint() {
    TEST_ASSERT_EQUAL(0, state.lastTimePrint());

    state.setLastTimePrint(12345);
    TEST_ASSERT_EQUAL(12345, state.lastTimePrint());
}

void test_newLine_default_true() {
    TEST_ASSERT_TRUE(state.isNewLine());
}

void test_newLine_toggle() {
    state.setNewLine(false);
    TEST_ASSERT_FALSE(state.isNewLine());

    state.setNewLine(true);
    TEST_ASSERT_TRUE(state.isNewLine());
}

// --- Main ---

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Level Management
    RUN_TEST(test_default_level_is_debug);
    RUN_TEST(test_set_level);
    RUN_TEST(test_last_level);
    RUN_TEST(test_level_before_profiler);

    // isActive Logic
    RUN_TEST(test_isActive_any_always_true);
    RUN_TEST(test_isActive_profiler_level);
    RUN_TEST(test_isActive_normal_levels);
    RUN_TEST(test_isActive_error_level);
    RUN_TEST(test_isActive_verbose_level);

    // Display Flags
    RUN_TEST(test_showTime_default_false);
    RUN_TEST(test_showTime_toggle);
    RUN_TEST(test_showProfiler);
    RUN_TEST(test_minTimeShowProfiler);
    RUN_TEST(test_showDebugLevel_default_true);
    RUN_TEST(test_showDebugLevel_toggle);
    RUN_TEST(test_showColors);
    RUN_TEST(test_showRaw);

    // Filter
    RUN_TEST(test_filter_default_inactive);
    RUN_TEST(test_setFilter);
    RUN_TEST(test_setFilter_converts_to_lowercase);
    RUN_TEST(test_clearFilter);

    // Silence
    RUN_TEST(test_silence_default_false);
    RUN_TEST(test_silence_toggle);
    RUN_TEST(test_silence_timeout);

    // Serial
    RUN_TEST(test_serialEnabled_default_false);
    RUN_TEST(test_serialEnabled_toggle);

    // Timing
    RUN_TEST(test_lastTimePrint);
    RUN_TEST(test_newLine_default_true);
    RUN_TEST(test_newLine_toggle);

    return UNITY_END();
}
