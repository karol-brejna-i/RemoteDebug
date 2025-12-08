#pragma once

#include <Arduino.h>

class CommandParser {
   public:
    enum class Command {
        None,
        Help,
        Quit,
        Memory,
        Cpu80,
        Cpu160,
        LevelVerbose,
        LevelDebug,
        LevelInfo,
        LevelWarning,
        LevelError,
        ToggleLevel,
        ToggleTime,
        Timeout,
        ToggleSilence,
        ToggleProfiler,
        ProfilerMin,
        ProfilerLevel,
        AutoProfiler,
        ToggleColors,
        Filter,
        NoFilter,
        Reset,
        Debugger,
        Custom
    };

    struct Result {
        Command command = Command::None;
        String argument = "";
    };

    Result parse(const String& raw) const;
};
