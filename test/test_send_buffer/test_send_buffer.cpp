#include "../test_command_parser/stubs/Arduino.h"
#include "../test_command_parser/stubs/RemoteDebugCfg.h"
#include <unity.h>
#include "SendBuffer.h"
#include <vector>
#include <string>

static SendBuffer buffer;
static std::vector<String> sent;

void setUp() {
    buffer.reset(0);
    sent.clear();
}

void tearDown() {
}

static void pushSend(const String& payload) {
    sent.push_back(payload);
}

void test_tick_no_data_no_send() {
    buffer.tick(100, pushSend);
    TEST_ASSERT_EQUAL_UINT(0, sent.size());
}

void test_enqueue_defers_until_delay() {
    buffer.enqueue("abc", 0, false, pushSend);

    buffer.tick(DELAY_TO_SEND - 1, pushSend);
    TEST_ASSERT_EQUAL_UINT(0, sent.size());

    buffer.tick(DELAY_TO_SEND, pushSend);
    TEST_ASSERT_EQUAL_UINT(1, sent.size());
    TEST_ASSERT_EQUAL_STRING("abc", sent[0].c_str());
}

void test_enqueue_batches_multiple_chunks() {
    buffer.enqueue("foo", 0, false, pushSend);
    buffer.enqueue("bar", 1, false, pushSend);

    buffer.tick(DELAY_TO_SEND, pushSend);

    TEST_ASSERT_EQUAL_UINT(1, sent.size());
    TEST_ASSERT_EQUAL_STRING("foobar", sent[0].c_str());
}

void test_raw_mode_flushes_immediately() {
    buffer.enqueue("raw", 5, true, pushSend);
    TEST_ASSERT_EQUAL_UINT(1, sent.size());
    TEST_ASSERT_EQUAL_STRING("raw", sent[0].c_str());

    buffer.tick(20, pushSend);
    TEST_ASSERT_EQUAL_UINT(1, sent.size());
}

void test_flush_when_exceeding_max_size() {
    const std::string first(MAX_SIZE_SEND - 5, 'A');
    const std::string second(10, 'B');

    buffer.enqueue(first.c_str(), 0, false, pushSend);
    buffer.enqueue(second.c_str(), 1, false, pushSend);

    TEST_ASSERT_EQUAL_UINT(1, sent.size());
    TEST_ASSERT_EQUAL_UINT(first.length(), sent[0].length());

    buffer.tick(DELAY_TO_SEND + 1, pushSend);
    TEST_ASSERT_EQUAL_UINT(2, sent.size());
    TEST_ASSERT_EQUAL_UINT(second.length(), sent[1].length());
}

void test_reset_updates_last_send_time() {
    buffer.reset(100);
    buffer.enqueue("delayed", 100, false, pushSend);

    buffer.tick(100 + DELAY_TO_SEND - 1, pushSend);
    TEST_ASSERT_EQUAL_UINT(0, sent.size());

    buffer.tick(100 + DELAY_TO_SEND, pushSend);
    TEST_ASSERT_EQUAL_UINT(1, sent.size());
    TEST_ASSERT_EQUAL_STRING("delayed", sent[0].c_str());
}

// --- Additional coverage tests ---

void test_reserve_does_not_crash() {
    // reserve() should not cause issues - just pre-allocates
    buffer.reserve(512);
    buffer.enqueue("test", 0, false, pushSend);
    buffer.tick(DELAY_TO_SEND, pushSend);
    TEST_ASSERT_EQUAL_UINT(1, sent.size());
    TEST_ASSERT_EQUAL_STRING("test", sent[0].c_str());
}

void test_empty_string_enqueue() {
    buffer.enqueue("", 0, false, pushSend);
    buffer.tick(DELAY_TO_SEND, pushSend);
    // Empty string should still trigger send (size = 0 after empty append)
    TEST_ASSERT_EQUAL_UINT(0, sent.size());
}

void test_multiple_raw_mode_calls() {
    buffer.enqueue("raw1", 0, true, pushSend);
    buffer.enqueue("raw2", 1, true, pushSend);
    buffer.enqueue("raw3", 2, true, pushSend);
    
    TEST_ASSERT_EQUAL_UINT(3, sent.size());
    TEST_ASSERT_EQUAL_STRING("raw1", sent[0].c_str());
    TEST_ASSERT_EQUAL_STRING("raw2", sent[1].c_str());
    TEST_ASSERT_EQUAL_STRING("raw3", sent[2].c_str());
}

void test_exact_max_size_boundary() {
    // Create string exactly at MAX_SIZE_SEND - 1 (will fit)
    const std::string exact(MAX_SIZE_SEND - 1, 'X');
    buffer.enqueue(exact.c_str(), 0, false, pushSend);
    
    // Should not flush yet (under limit)
    TEST_ASSERT_EQUAL_UINT(0, sent.size());
    
    // Adding one more byte would exceed - add it
    buffer.enqueue("Y", 1, false, pushSend);
    
    // First buffer should have been flushed
    TEST_ASSERT_EQUAL_UINT(1, sent.size());
    TEST_ASSERT_EQUAL_UINT(exact.length(), sent[0].length());
}

void test_immediate_flush_on_elapsed_time() {
    // Enqueue at time 0
    buffer.enqueue("first", 0, false, pushSend);
    
    // Now enqueue at time >= DELAY_TO_SEND (elapsed)
    buffer.enqueue("second", DELAY_TO_SEND, false, pushSend);
    
    // Should have flushed immediately due to elapsed time
    TEST_ASSERT_EQUAL_UINT(1, sent.size());
    TEST_ASSERT_EQUAL_STRING("firstsecond", sent[0].c_str());
}

void test_consecutive_ticks_no_double_send() {
    buffer.enqueue("data", 0, false, pushSend);
    
    buffer.tick(DELAY_TO_SEND, pushSend);
    TEST_ASSERT_EQUAL_UINT(1, sent.size());
    
    // Tick again - should not send (buffer is empty)
    buffer.tick(DELAY_TO_SEND + 1, pushSend);
    buffer.tick(DELAY_TO_SEND + 100, pushSend);
    TEST_ASSERT_EQUAL_UINT(1, sent.size());
}

void test_mixed_raw_and_buffered() {
    buffer.enqueue("buffered1", 0, false, pushSend);
    buffer.enqueue("raw", 1, true, pushSend);  // Flushes both
    
    TEST_ASSERT_EQUAL_UINT(1, sent.size());
    TEST_ASSERT_EQUAL_STRING("buffered1raw", sent[0].c_str());
    
    // Now add more buffered
    buffer.enqueue("buffered2", 2, false, pushSend);
    buffer.tick(2 + DELAY_TO_SEND, pushSend);
    
    TEST_ASSERT_EQUAL_UINT(2, sent.size());
    TEST_ASSERT_EQUAL_STRING("buffered2", sent[1].c_str());
}

void test_overflow_flushes_previous_buffer_only() {
    const std::string first(MAX_SIZE_SEND - 10, 'A');
    const std::string second(20, 'B');  // Would overflow
    
    buffer.enqueue(first.c_str(), 0, false, pushSend);
    TEST_ASSERT_EQUAL_UINT(0, sent.size());
    
    buffer.enqueue(second.c_str(), 1, false, pushSend);
    // First buffer flushed due to overflow
    TEST_ASSERT_EQUAL_UINT(1, sent.size());
    TEST_ASSERT_EQUAL_STRING(first.c_str(), sent[0].c_str());
    
    // Second is still buffered
    buffer.tick(1 + DELAY_TO_SEND, pushSend);
    TEST_ASSERT_EQUAL_UINT(2, sent.size());
    TEST_ASSERT_EQUAL_STRING(second.c_str(), sent[1].c_str());
}

void test_large_single_chunk() {
    // Single chunk larger than MAX_SIZE_SEND
    const std::string large(MAX_SIZE_SEND + 100, 'Z');
    
    buffer.enqueue(large.c_str(), 0, false, pushSend);
    // Should still be buffered (no existing data to flush)
    TEST_ASSERT_EQUAL_UINT(0, sent.size());
    
    buffer.tick(DELAY_TO_SEND, pushSend);
    TEST_ASSERT_EQUAL_UINT(1, sent.size());
    TEST_ASSERT_EQUAL_UINT(large.length(), sent[0].length());
}

void test_tick_before_delay_no_send() {
    buffer.enqueue("test", 0, false, pushSend);
    
    buffer.tick(1, pushSend);
    buffer.tick(DELAY_TO_SEND / 2, pushSend);
    buffer.tick(DELAY_TO_SEND - 1, pushSend);
    
    TEST_ASSERT_EQUAL_UINT(0, sent.size());
}

void test_reset_clears_pending_data() {
    buffer.enqueue("pending", 0, false, pushSend);
    buffer.reset(100);
    
    buffer.tick(100 + DELAY_TO_SEND, pushSend);
    // Nothing should be sent - buffer was cleared
    TEST_ASSERT_EQUAL_UINT(0, sent.size());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Original tests
    RUN_TEST(test_tick_no_data_no_send);
    RUN_TEST(test_enqueue_defers_until_delay);
    RUN_TEST(test_enqueue_batches_multiple_chunks);
    RUN_TEST(test_raw_mode_flushes_immediately);
    RUN_TEST(test_flush_when_exceeding_max_size);
    RUN_TEST(test_reset_updates_last_send_time);

    // Additional coverage
    RUN_TEST(test_reserve_does_not_crash);
    RUN_TEST(test_empty_string_enqueue);
    RUN_TEST(test_multiple_raw_mode_calls);
    RUN_TEST(test_exact_max_size_boundary);
    RUN_TEST(test_immediate_flush_on_elapsed_time);
    RUN_TEST(test_consecutive_ticks_no_double_send);
    RUN_TEST(test_mixed_raw_and_buffered);
    RUN_TEST(test_overflow_flushes_previous_buffer_only);
    RUN_TEST(test_large_single_chunk);
    RUN_TEST(test_tick_before_delay_no_send);
    RUN_TEST(test_reset_clears_pending_data);

    return UNITY_END();
}
