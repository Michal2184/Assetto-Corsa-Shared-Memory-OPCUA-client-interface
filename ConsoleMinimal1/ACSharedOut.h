#pragma once
#include <Windows.h>
#include <map>
#include <string>
#include <cstdio>
#include <cwchar>

// Physics shared memory structure (truncated to what's currently used)
struct SPageFilePhysics {
    int packetId;
    float gas;
    float brake;
    float fuel;
    int gear;
    int engineRPM;
    float steerAngle;   // radians? (original source comment kept)
    float speedKmh;
    float velocity[3];
    float accG[3];
    float wheelSlip[4];
    float wheelLoad[4];
    float wheelsPressure[4];
    float wheelAngularSpeed[4];
    float tyreWear[4];
    float tyreDirtyLevel[4];
    float tyreCoreTemperature[4];
    float camberRAD[4];
    float suspensionTravel[4];
    float drs;
    float tc;
    float heading;
    float pitch;
    float roll;
    float cgHeight;
    float carDamage[5];
    int numberOfTyresOut;
    int pitLimiterOn;
    float abs;
    float kersCharge;
    float kersInput;
    int autoShifterOn;
    float rideHeight[2];
    float turboBoost;
    float ballast;
    float airDensity;
    float airTemp;
    float roadTemp;
    float localAngularVel[3];
    float finalFF;
    float performanceMeter;
    int engineBrake;
    int ersRecoveryLevel;
    int ersPowerLevel;
    int ersHeatCharging;
    int ersIsCharging;
    float kersCurrentKJ;
    int drsAvailable;
    int drsEnabled;
    float brakeTemp[4];
    float clutch;
    float tyreTempI[4];
    float tyreTempM[4];
    float tyreTempO[4];
    int isAIControlled;
    float tyreContactPoint[4][3];
    float tyreContactNormal[4][3];
    float tyreContactHeading[4][3];
    float brakeBias;
    float localVelocity[3];
};

// Graphics shared memory structure (truncated to what's used)
struct SPageFileGraphics {
    int packetId;
    int status;
    int session;
    wchar_t currentTime[15];
    wchar_t lastTime[15];
    wchar_t bestTime[15];
    wchar_t split[15];
    int completedLaps;
    int position;
    int iCurrentTime;
    int iLastTime;
    int iBestTime;
    float sessionTimeLeft;
    float distanceTraveled;
    int isInPit;
    int currentSectorIndex;
    int lastSectorTime;
    int numberOfLaps;
    wchar_t tyreCompound[33];
    float replayTimeMultiplier;
    float normalizedCarPosition;
    float carCoordinates[3];
    float penaltyTime;
    int flag;
    int idealLineOn;
    int isInPitLane;
    float surfaceGrip;
    int mandatoryPitDone;
    float windSpeed;
    float windDirection;
};

struct ACSharedOutData {
    std::map<std::string, int> vehicle; // integer values (converted / truncated)
    std::map<std::string, std::string> times;
    std::map<std::string, int> env;
    std::map<std::string, float> miscFloats; // string times (formatted)
    bool ok{ false }; // indicates read success
};

class ACSharedOut {
public:
    ACSharedOut() {}
    ~ACSharedOut() { cleanup(); }

    bool initialize() {
        // Open physics mapping
        hMapFilePhysics = OpenFileMappingW(FILE_MAP_READ, FALSE, L"Local\\acpmf_physics");
        if (!hMapFilePhysics) return false;

        acPhysics = static_cast<SPageFilePhysics*>(
            MapViewOfFile(hMapFilePhysics, FILE_MAP_READ, 0, 0, sizeof(SPageFilePhysics)));
        if (!acPhysics) { cleanup(); return false; }

        // Open graphics mapping
        hMapFileGraphics = OpenFileMappingW(FILE_MAP_READ, FALSE, L"Local\\acpmf_graphics");
        if (!hMapFileGraphics) { cleanup(); return false; }

        acGraphics = static_cast<SPageFileGraphics*>(
            MapViewOfFile(hMapFileGraphics, FILE_MAP_READ, 0, 0, sizeof(SPageFileGraphics)));
        if (!acGraphics) { cleanup(); return false; }

        connected = true;
        return true;
    }

    void cleanup() {
        if (acGraphics) { UnmapViewOfFile(acGraphics); acGraphics = nullptr; }
        if (hMapFileGraphics) { CloseHandle(hMapFileGraphics); hMapFileGraphics = nullptr; }
        if (acPhysics) { UnmapViewOfFile(acPhysics); acPhysics = nullptr; }
        if (hMapFilePhysics) { CloseHandle(hMapFilePhysics); hMapFilePhysics = nullptr; }
        connected = false;
    }

    ACSharedOutData readGame() {
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
        data.env["numberOfLaps"] = acGraphics->numberOfLaps;
        data.env["position"] = acGraphics->position;
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

private:
    HANDLE hMapFilePhysics{ nullptr };
    HANDLE hMapFileGraphics{ nullptr };
    SPageFilePhysics* acPhysics{ nullptr };
    SPageFileGraphics* acGraphics{ nullptr };
    bool connected{ false };

    std::string formatTime(int ms) {
        if (ms <= 0) return "--:--.---";
        int minutes = ms / 60000;
        int seconds = (ms % 60000) / 1000;
        int millis = ms % 1000;
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d:%02d.%03d", minutes, seconds, millis);
        return std::string(buf);
    }
};
