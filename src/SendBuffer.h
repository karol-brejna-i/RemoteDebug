#pragma once

#include <Arduino.h>

#include "RemoteDebugCfg.h"

// Small helper to buffer outbound data before writing to the transport.
class SendBuffer {
   public:
    SendBuffer() = default;

    void reserve(size_t size) { _buffer.reserve(size); }

    void reset(uint32_t now = millis()) {
        _buffer = "";
        _size = 0;
        _lastSend = now;
    }

    template <typename SendFn>
    void tick(uint32_t now, SendFn send) {
        if (_size == 0) {
            return;
        }
        if ((now - _lastSend) >= DELAY_TO_SEND) {
            send(_buffer);
            reset(now);
        }
    }

    template <typename SendFn>
    void enqueue(const String& chunk, uint32_t now, bool rawMode, SendFn send) {
        const uint16_t chunkSize = chunk.length();

        // Flush current buffer if incoming chunk would overflow limits.
        if ((_size + chunkSize) >= MAX_SIZE_SEND) {
            if (_size) {
                send(_buffer);
            }
            reset(now);
        }

        _buffer.concat(chunk);
        _size += chunkSize;

        // Raw output or elapsed interval -> flush immediately.
        if (rawMode || (now - _lastSend) >= DELAY_TO_SEND) {
            send(_buffer);
            reset(now);
        }
    }

   private:
    String _buffer;
    uint16_t _size = 0;
    uint32_t _lastSend = 0;
};
