#pragma once

#include <Arduino.h>

#include "RemoteDebugCfg.h"

/**
 * Encapsulates debug configuration and display state.
 * This class holds level, filter, time/profiler flags, and colors settings,
 * making state management testable without transport dependencies.
 */
class DebugState {
   public:
    // Debug levels (mirrored from RemoteDebug for compatibility)
    static const uint8_t PROFILER = 0;
    static const uint8_t VERBOSE = 1;
    static const uint8_t DEBUG = 2;
    static const uint8_t INFO = 3;
    static const uint8_t WARNING = 4;
    static const uint8_t ERROR = 5;
    static const uint8_t ANY = 6;

    DebugState() = default;

    // --- Level management ---
    uint8_t getLevel() const { return _level; }
    void setLevel(uint8_t level) { _level = level; }

    uint8_t getLastLevel() const { return _lastLevel; }
    void setLastLevel(uint8_t level) { _lastLevel = level; }

    uint8_t getLevelBeforeProfiler() const { return _levelBeforeProfiler; }
    void setLevelBeforeProfiler(uint8_t level) { _levelBeforeProfiler = level; }

    uint32_t getLevelProfilerDisable() const { return _levelProfilerDisable; }
    void setLevelProfilerDisable(uint32_t time) { _levelProfilerDisable = time; }

    uint32_t getAutoLevelProfiler() const { return _autoLevelProfiler; }
    void setAutoLevelProfiler(uint32_t ms) { _autoLevelProfiler = ms; }

    bool isActive(uint8_t level) const {
        // ANY level always active; PROFILER requires profiler level
        if (level == ANY) return true;
        if (_level == PROFILER) return (level == PROFILER);
        return (level >= _level);
    }

    // --- Display flags ---
    bool showTime() const { return _showTime; }
    void setShowTime(bool show) { _showTime = show; }

    bool showProfiler() const { return _showProfiler; }
    void setShowProfiler(bool show) { _showProfiler = show; }

    uint32_t minTimeShowProfiler() const { return _minTimeShowProfiler; }
    void setMinTimeShowProfiler(uint32_t ms) { _minTimeShowProfiler = ms; }

    bool showDebugLevel() const { return _showDebugLevel; }
    void setShowDebugLevel(bool show) { _showDebugLevel = show; }

    bool showColors() const { return _showColors; }
    void setShowColors(bool show) { _showColors = show; }

    bool showRaw() const { return _showRaw; }
    void setShowRaw(bool show) { _showRaw = show; }

    // --- Filter ---
    const String& filter() const { return _filter; }
    bool filterActive() const { return _filterActive; }

    void setFilter(const String& f) {
        _filter = f;
        _filter.toLowerCase();
        _filterActive = true;
    }

    void clearFilter() {
        _filter = "";
        _filterActive = false;
    }

    // --- Silence mode ---
    bool isSilence() const { return _silence; }
    void setSilence(bool s) { _silence = s; }

    uint32_t silenceTimeout() const { return _silenceTimeout; }
    void setSilenceTimeout(uint32_t t) { _silenceTimeout = t; }

    // --- Serial echo ---
    bool serialEnabled() const { return _serialEnabled; }
    void setSerialEnabled(bool e) { _serialEnabled = e; }

    // --- Profiler timing ---
    uint32_t lastTimePrint() const { return _lastTimePrint; }
    void setLastTimePrint(uint32_t t) { _lastTimePrint = t; }

    // --- New line tracking ---
    bool isNewLine() const { return _newLine; }
    void setNewLine(bool n) { _newLine = n; }

   private:
    uint8_t _level = DEBUG;
    uint8_t _lastLevel = DEBUG;
    uint8_t _levelBeforeProfiler = DEBUG;
    uint32_t _levelProfilerDisable = 0;
    uint32_t _autoLevelProfiler = 0;

    bool _showTime = false;
    bool _showProfiler = false;
    uint32_t _minTimeShowProfiler = 0;
    bool _showDebugLevel = true;
    bool _showColors = false;
    bool _showRaw = false;

    String _filter = "";
    bool _filterActive = false;

    bool _silence = false;
    uint32_t _silenceTimeout = 0;

    bool _serialEnabled = false;

    uint32_t _lastTimePrint = 0;
    bool _newLine = true;
};
