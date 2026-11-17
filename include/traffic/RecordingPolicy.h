#pragma once
#include <stdint.h>
#include <string.h>
#include <ctype.h>

// Shared calendar policy. 2026 is the explicit migration floor: all older
// recording folders are archives. This is not a show-season start year.
namespace traffic {
constexpr int kRecordingYearFloor = 2026;
inline bool validYear(int y) { return y >= kRecordingYearFloor && y <= 2099; }
inline bool leapYear(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }
inline bool validDate(int y, int m, int d) {
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return validYear(y) && m >= 1 && m <= 12 && d >= 1 &&
           d <= days[m-1] + (m == 2 && leapYear(y));
}
inline int blackFridayDay(int year) {
    // Gregorian weekday of November 1, Sunday=0. Fourth Thursday + one day.
    int y = year;
    int weekday = (y + y/4 - y/100 + y/400 + 2 + 1) % 7;
    return 1 + (4 - weekday + 7) % 7 + 21 + 1;
}
inline int selectRecordingYear(int saved, int clock, bool trustedNetworkTime) {
    int floor = validYear(saved) ? saved : kRecordingYearFloor;
    if (!validYear(clock) || clock < floor) return floor;
    // RTC can roll forward one year; larger jumps need network confirmation.
    return clock <= floor + 1 || trustedNetworkTime ? clock : floor;
}
inline bool writablePath(const char* path, int year, bool mkdirOnly = false) {
    // Reject ambiguous FAT paths rather than allowing aliases to bypass guards.
    if (!path || path[0] != '/' || !path[1] || !validYear(year)) return false;
    for (const char* p = path; *p; ++p) {
        if (*p == '\\' || *p == ':' || *p == '%' || (unsigned char)*p < 32) return false;
        if (*p == '/' && p[1] == '/') return false;
        if (*p == '/' && (p[1] == '.' && (p[2] == '/' || p[2] == 0 ||
            (p[2] == '.' && (p[3] == '/' || p[3] == 0))))) return false;
        if (*p != '/' && (p[1] == '/' || p[1] == 0) && (*p == ' ' || *p == '.')) return false;
    }
    // FAT is case insensitive. Protect both counter trees even on a swapped SD.
    bool annual = (tolower(path[1]) == 'c' || tolower(path[1]) == 'g') &&
                  tolower(path[2]) == 'c' && (path[3] == '/' || path[3] == 0);
    if (!annual) return true;
    if (path[3] == 0) return mkdirOnly; // Never remove/rename the archive parent.
    const char* p = path + 4;
    int found = 0;
    for (int i=0; i<4; ++i) {
        if (p[i] < '0' || p[i] > '9') return false;
        found = found*10 + p[i]-'0';
    }
    return found == year && (p[4] == 0 || p[4] == '/');
}
}
