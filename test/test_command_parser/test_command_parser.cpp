/**
 * Unit tests for CommandParser
 * Run with: pio test -e native
 */

#include "stubs/Arduino.h"
#include "stubs/RemoteDebugCfg.h"
#include <unity.h>
#include "CommandParser.h"

static CommandParser parser;

// --- Help Commands ---

void test_parse_help_h() {
    auto res = parser.parse("h");
    TEST_ASSERT_EQUAL(CommandParser::Command::Help, res.command);
    TEST_ASSERT_TRUE(res.argument.isEmpty());
}

void test_parse_help_question() {
    auto res = parser.parse("?");
    TEST_ASSERT_EQUAL(CommandParser::Command::Help, res.command);
}

void test_parse_help_word() {
    auto res = parser.parse("help");
    TEST_ASSERT_EQUAL(CommandParser::Command::Help, res.command);
}

void test_parse_help_uppercase() {
    auto res = parser.parse("H");
    TEST_ASSERT_EQUAL(CommandParser::Command::Help, res.command);
}

// --- Quit ---

void test_parse_quit() {
    auto res = parser.parse("q");
    TEST_ASSERT_EQUAL(CommandParser::Command::Quit, res.command);
}

// --- Memory ---

void test_parse_memory() {
    auto res = parser.parse("m");
    TEST_ASSERT_EQUAL(CommandParser::Command::Memory, res.command);
}

// --- Debug Levels ---

void test_parse_level_verbose() {
    auto res = parser.parse("v");
    TEST_ASSERT_EQUAL(CommandParser::Command::LevelVerbose, res.command);
}

void test_parse_level_debug() {
    auto res = parser.parse("d");
    TEST_ASSERT_EQUAL(CommandParser::Command::LevelDebug, res.command);
}

void test_parse_level_info() {
    auto res = parser.parse("i");
    TEST_ASSERT_EQUAL(CommandParser::Command::LevelInfo, res.command);
}

void test_parse_level_warning() {
    auto res = parser.parse("w");
    TEST_ASSERT_EQUAL(CommandParser::Command::LevelWarning, res.command);
}

void test_parse_level_error() {
    auto res = parser.parse("e");
    TEST_ASSERT_EQUAL(CommandParser::Command::LevelError, res.command);
}

// --- Toggle Commands ---

void test_parse_toggle_level() {
    auto res = parser.parse("l");
    TEST_ASSERT_EQUAL(CommandParser::Command::ToggleLevel, res.command);
}

void test_parse_toggle_time() {
    auto res = parser.parse("t");
    TEST_ASSERT_EQUAL(CommandParser::Command::ToggleTime, res.command);
}

void test_parse_toggle_silence() {
    auto res = parser.parse("s");
    TEST_ASSERT_EQUAL(CommandParser::Command::ToggleSilence, res.command);
}

void test_parse_toggle_colors() {
    auto res = parser.parse("c");
    TEST_ASSERT_EQUAL(CommandParser::Command::ToggleColors, res.command);
}

void test_parse_toggle_profiler() {
    auto res = parser.parse("p");
    TEST_ASSERT_EQUAL(CommandParser::Command::ToggleProfiler, res.command);
}

// --- Profiler with Arguments ---

void test_parse_profiler_min() {
    auto res = parser.parse("p 100");
    TEST_ASSERT_EQUAL(CommandParser::Command::ProfilerMin, res.command);
    TEST_ASSERT_EQUAL_STRING("100", res.argument.c_str());
}

void test_parse_profiler_level() {
    auto res = parser.parse("P");
    TEST_ASSERT_EQUAL(CommandParser::Command::ProfilerLevel, res.command);
}

void test_parse_profiler_level_with_time() {
    auto res = parser.parse("P 5000");
    TEST_ASSERT_EQUAL(CommandParser::Command::ProfilerLevel, res.command);
    TEST_ASSERT_EQUAL_STRING("5000", res.argument.c_str());
}

void test_parse_auto_profiler() {
    auto res = parser.parse("A");
    TEST_ASSERT_EQUAL(CommandParser::Command::AutoProfiler, res.command);
}

void test_parse_auto_profiler_with_time() {
    auto res = parser.parse("A 1000");
    TEST_ASSERT_EQUAL(CommandParser::Command::AutoProfiler, res.command);
    TEST_ASSERT_EQUAL_STRING("1000", res.argument.c_str());
}

// --- Timeout ---

void test_parse_timeout_get() {
    auto res = parser.parse("timeout");
    TEST_ASSERT_EQUAL(CommandParser::Command::Timeout, res.command);
    TEST_ASSERT_TRUE(res.argument.isEmpty());
}

void test_parse_timeout_set() {
    auto res = parser.parse("timeout 120");
    TEST_ASSERT_EQUAL(CommandParser::Command::Timeout, res.command);
    TEST_ASSERT_EQUAL_STRING("120", res.argument.c_str());
}

// --- Filter ---

void test_parse_filter() {
    auto res = parser.parse("filter error");
    TEST_ASSERT_EQUAL(CommandParser::Command::Filter, res.command);
    TEST_ASSERT_EQUAL_STRING("error", res.argument.c_str());
}

void test_parse_filter_with_spaces() {
    auto res = parser.parse("filter my pattern");
    TEST_ASSERT_EQUAL(CommandParser::Command::Filter, res.command);
    TEST_ASSERT_EQUAL_STRING("my pattern", res.argument.c_str());
}

void test_parse_nofilter() {
    auto res = parser.parse("nofilter");
    TEST_ASSERT_EQUAL(CommandParser::Command::NoFilter, res.command);
}

// --- Reset ---

void test_parse_reset() {
    auto res = parser.parse("reset");
    TEST_ASSERT_EQUAL(CommandParser::Command::Reset, res.command);
}

// --- Debugger ---

void test_parse_debugger() {
    auto res = parser.parse("dbg");
    TEST_ASSERT_EQUAL(CommandParser::Command::Debugger, res.command);
}

void test_parse_debugger_command() {
    auto res = parser.parse("dbg break");
    TEST_ASSERT_EQUAL(CommandParser::Command::Debugger, res.command);
    TEST_ASSERT_EQUAL_STRING("break", res.argument.c_str());
}

// --- Edge Cases ---

void test_parse_empty() {
    auto res = parser.parse("");
    TEST_ASSERT_EQUAL(CommandParser::Command::None, res.command);
}

void test_parse_unknown() {
    auto res = parser.parse("foobar");
    TEST_ASSERT_EQUAL(CommandParser::Command::Custom, res.command);
}

void test_parse_whitespace_only() {
    auto res = parser.parse("   ");
    TEST_ASSERT_EQUAL(CommandParser::Command::None, res.command);
}

void test_parse_leading_trailing_spaces() {
    auto res = parser.parse("  h  ");
    TEST_ASSERT_EQUAL(CommandParser::Command::Help, res.command);
}

void test_parse_case_insensitive() {
    auto res = parser.parse("HELP");
    TEST_ASSERT_EQUAL(CommandParser::Command::Help, res.command);
}

void test_parse_mixed_case() {
    auto res = parser.parse("FiLtEr Test");
    TEST_ASSERT_EQUAL(CommandParser::Command::Filter, res.command);
    TEST_ASSERT_EQUAL_STRING("Test", res.argument.c_str());
}

// --- Main ---

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Help
    RUN_TEST(test_parse_help_h);
    RUN_TEST(test_parse_help_question);
    RUN_TEST(test_parse_help_word);
    RUN_TEST(test_parse_help_uppercase);

    // Quit & Memory
    RUN_TEST(test_parse_quit);
    RUN_TEST(test_parse_memory);

    // Debug Levels
    RUN_TEST(test_parse_level_verbose);
    RUN_TEST(test_parse_level_debug);
    RUN_TEST(test_parse_level_info);
    RUN_TEST(test_parse_level_warning);
    RUN_TEST(test_parse_level_error);

    // Toggles
    RUN_TEST(test_parse_toggle_level);
    RUN_TEST(test_parse_toggle_time);
    RUN_TEST(test_parse_toggle_silence);
    RUN_TEST(test_parse_toggle_colors);
    RUN_TEST(test_parse_toggle_profiler);

    // Profiler
    RUN_TEST(test_parse_profiler_min);
    RUN_TEST(test_parse_profiler_level);
    RUN_TEST(test_parse_profiler_level_with_time);
    RUN_TEST(test_parse_auto_profiler);
    RUN_TEST(test_parse_auto_profiler_with_time);

    // Timeout
    RUN_TEST(test_parse_timeout_get);
    RUN_TEST(test_parse_timeout_set);

    // Filter
    RUN_TEST(test_parse_filter);
    RUN_TEST(test_parse_filter_with_spaces);
    RUN_TEST(test_parse_nofilter);

    // Reset & Debugger
    RUN_TEST(test_parse_reset);
    RUN_TEST(test_parse_debugger);
    RUN_TEST(test_parse_debugger_command);

    // Edge Cases
    RUN_TEST(test_parse_empty);
    RUN_TEST(test_parse_unknown);
    RUN_TEST(test_parse_whitespace_only);
    RUN_TEST(test_parse_leading_trailing_spaces);
    RUN_TEST(test_parse_case_insensitive);
    RUN_TEST(test_parse_mixed_case);

    return UNITY_END();
}
