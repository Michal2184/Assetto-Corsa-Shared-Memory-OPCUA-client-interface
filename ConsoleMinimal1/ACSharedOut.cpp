#include "ACSharedOut.h"
#include <cstdio>
#include <cwchar>

ACSharedOut::ACSharedOut() {}
ACSharedOut::~ACSharedOut() { cleanup(); }

bool ACSharedOut::initialize() {
    // Open physics mapping
    hMapFilePhysics = OpenFileMappingW(FILE_MAP_READ, FALSE, L"Local\\acpmf_physics");
    if (!hMapFilePhysics) return false;

    acPhysics = static_cast<SPageFilePhysics*>(MapViewOfFile(hMapFilePhysics, FILE_MAP_READ, 0, 0, sizeof(SPageFilePhysics)));
    if (!acPhysics) { cleanup(); return false; }

    // Open graphics mapping
    hMapFileGraphics = OpenFileMappingW(FILE_MAP_READ, FALSE, L"Local\\acpmf_graphics");
    if (!hMapFileGraphics) { cleanup(); return false; }

    acGraphics = static_cast<SPageFileGraphics*>(MapViewOfFile(hMapFileGraphics, FILE_MAP_READ, 0, 0, sizeof(SPageFileGraphics)));
    if (!acGraphics) { cleanup(); return false; }

    connected = true;
    return true;
}

void ACSharedOut::cleanup() {
    if (acGraphics) { UnmapViewOfFile(acGraphics); acGraphics = nullptr; }
    if (hMapFileGraphics) { CloseHandle(hMapFileGraphics); hMapFileGraphics = nullptr; }
    if (acPhysics) { UnmapViewOfFile(acPhysics); acPhysics = nullptr; }
    if (hMapFilePhysics) { CloseHandle(hMapFilePhysics); hMapFilePhysics = nullptr; }
    connected = false;
}

std::string ACSharedOut::formatTime(int ms) {
    if (ms <= 0) return "--:--.---";
    int minutes = ms / 60000;
    int seconds = (ms % 60000) / 1000;
    int millis  = ms % 1000;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d.%03d", minutes, seconds, millis);
    return std::string(buf);
}

ACSharedOutData ACSharedOut::readGame() {
    ACSharedOutData data;
    if (!connected || !acPhysics || !acGraphics) return data; // ok stays false

    // Vehicle numeric/int-like values (truncate where float)
    data.vehicle["speedKmh"] = acPhysics->speedKmh;
    data.vehicle["engineRPM"] = acPhysics->engineRPM;
    data.vehicle["gear"] = acPhysics->gear - 1; // convert to human (N=0)
    data.vehicle["fuel"] = acPhysics->fuel;
    data.vehicle["steerAngle"] = acPhysics->steerAngle * 100;
    data.vehicle["gas"] = acPhysics->gas * 100;
    data.vehicle["brake"] = acPhysics->brake * 100;

    // Laps / session numeric values
    data.env["numberOfLaps"] = acGraphics->numberOfLaps ;
    data.env["position"] = acGraphics->position; // Store as integer
    data.env["completedLaps"] = acGraphics->completedLaps;

    data.miscFloats["windSpeed"] = acGraphics->windSpeed;
    data.miscFloats["windDirection"] = acGraphics->windDirection;

    // Format all times consistently as strings
    data.times["currentTime"] = formatTime(acGraphics->iCurrentTime);
    data.times["lastTime"] = formatTime(acGraphics->iLastTime);
    data.times["bestTime"] = formatTime(acGraphics->iBestTime);

    data.ok = true;
    return data;
}
