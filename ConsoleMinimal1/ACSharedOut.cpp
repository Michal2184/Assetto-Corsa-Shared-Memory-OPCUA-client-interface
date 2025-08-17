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
    data.vehicle["speedKmh"] = static_cast<int>(acPhysics->speedKmh);
    data.vehicle["engineRPM"] = acPhysics->engineRPM;
    data.vehicle["gear"] = acPhysics->gear - 1; // convert to human (N=0)
    data.vehicle["fuel"] = static_cast<int>(acPhysics->fuel);
    data.vehicle["steerAngleX100"] = static_cast<int>(acPhysics->steerAngle * 100.0f);
    data.vehicle["gasPct"] = static_cast<int>(acPhysics->gas * 100.0f);
    data.vehicle["brakePct"] = static_cast<int>(acPhysics->brake * 100.0f);
    data.vehicle["clutchPct"] = static_cast<int>(acPhysics->clutch * 100.0f);
    data.vehicle["drsEnabled"] = acPhysics->drsEnabled;
    data.vehicle["drsAvailable"] = acPhysics->drsAvailable;
    data.vehicle["ersPowerLevel"] = acPhysics->ersPowerLevel;
    data.vehicle["ersRecoveryLevel"] = acPhysics->ersRecoveryLevel;
    data.vehicle["pitLimiterOn"] = acPhysics->pitLimiterOn;
    data.vehicle["absPct"] = static_cast<int>(acPhysics->abs * 100.0f);
    data.vehicle["tcPct"] = static_cast<int>(acPhysics->tc * 100.0f);
    data.vehicle["numberOfTyresOut"] = acPhysics->numberOfTyresOut;
    data.vehicle["isAIControlled"] = acPhysics->isAIControlled;

    // Laps / session textual (plus some numeric converted to string)
    data.laps["currentLap"] = std::to_string(acGraphics->completedLaps + 1);
    data.laps["totalLaps"] = std::to_string(acGraphics->numberOfLaps);
    data.laps["position"] = std::to_string(acGraphics->position);
    data.laps["currentTime"] = formatTime(acGraphics->iCurrentTime);
    data.laps["lastTime"] = formatTime(acGraphics->iLastTime);
    data.laps["bestTime"] = formatTime(acGraphics->iBestTime);
    data.laps["isInPit"] = std::to_string(acGraphics->isInPit);
    data.laps["isInPitLane"] = std::to_string(acGraphics->isInPitLane);
    data.laps["sessionTimeLeftMs"] = std::to_string(static_cast<int>(acGraphics->sessionTimeLeft * 1000));

    data.ok = true;
    return data;
}
