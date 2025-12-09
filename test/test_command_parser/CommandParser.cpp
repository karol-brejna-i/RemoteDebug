#include "CommandParser.h"

CommandParser::Result CommandParser::parse(const String& raw) const {
    Result res;
    if (raw.length() == 0) {
        return res;
    }

    String trimmed = raw;
    trimmed.trim();

    // After trimming, if empty, return None
    if (trimmed.length() == 0) {
        return res;
    }

    int spaceIndex = trimmed.indexOf(' ');
    String keyword = (spaceIndex > 0) ? trimmed.substring(0, spaceIndex) : trimmed;
    String argument = (spaceIndex > 0) ? trimmed.substring(spaceIndex + 1) : "";
    argument.trim();

    // Commands that rely on original casing
    if (keyword == "P") {
        res.command = Command::ProfilerLevel;
        res.argument = argument;
        return res;
    }
    if (keyword == "A") {
        res.command = Command::AutoProfiler;
        res.argument = argument;
        return res;
    }

    String keywordLower = keyword;
    keywordLower.toLowerCase();

    if (keywordLower == "?" || keywordLower == "h" || keywordLower == "help") {
        res.command = Command::Help;
    } else if (keywordLower == "q") {
        res.command = Command::Quit;
    } else if (keywordLower == "m") {
        res.command = Command::Memory;
#if defined(ESP8266)
    } else if (keywordLower == "cpu80") {
        res.command = Command::Cpu80;
    } else if (keywordLower == "cpu160") {
        res.command = Command::Cpu160;
#endif
    } else if (keywordLower == "v") {
        res.command = Command::LevelVerbose;
    } else if (keywordLower == "d") {
        res.command = Command::LevelDebug;
    } else if (keywordLower == "i") {
        res.command = Command::LevelInfo;
    } else if (keywordLower == "w") {
        res.command = Command::LevelWarning;
    } else if (keywordLower == "e") {
        res.command = Command::LevelError;
    } else if (keywordLower == "l") {
        res.command = Command::ToggleLevel;
    } else if (keywordLower == "t") {
        res.command = Command::ToggleTime;
    } else if (keywordLower == "timeout") {
        res.command = Command::Timeout;
        res.argument = argument;
    } else if (keywordLower == "s") {
        res.command = Command::ToggleSilence;
    } else if (keywordLower == "p" && argument.length() == 0) {
        res.command = Command::ToggleProfiler;
    } else if (keywordLower == "p") {  // has argument
        res.command = Command::ProfilerMin;
        res.argument = argument;
    } else if (keywordLower == "c") {
        res.command = Command::ToggleColors;
    } else if (keywordLower == "filter") {
        res.command = Command::Filter;
        res.argument = argument;
    } else if (keywordLower == "nofilter") {
        res.command = Command::NoFilter;
    } else if (keywordLower == "reset") {
        res.command = Command::Reset;
    } else if (keywordLower.startsWith("dbg")) {
        res.command = Command::Debugger;
        res.argument = argument;
    } else {
        res.command = Command::Custom;
        res.argument = trimmed;
    }

    return res;
}
