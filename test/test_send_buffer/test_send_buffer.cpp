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

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_tick_no_data_no_send);
    RUN_TEST(test_enqueue_defers_until_delay);
    RUN_TEST(test_enqueue_batches_multiple_chunks);
    RUN_TEST(test_raw_mode_flushes_immediately);
    RUN_TEST(test_flush_when_exceeding_max_size);
    RUN_TEST(test_reset_updates_last_send_time);

    return UNITY_END();
}
