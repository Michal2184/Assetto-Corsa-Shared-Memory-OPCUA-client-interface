#pragma once
#include <Windows.h>
#include <map>
#include <string>

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
    std::map<std::string,int> vehicle; // integer values (converted / truncated)
    std::map<std::string,std::string> times;
    std::map<std::string, int> env;
	std::map<std::string, float> miscFloats; // string times (formatted)
    // time strings and misc textual
    bool ok { false }; // indicates read success
};

class ACSharedOut {
public:
    ACSharedOut();
    ~ACSharedOut();

    bool initialize();
    void cleanup();

    // Read current snapshot (no loop). Caller calls every ~100ms.
    ACSharedOutData readGame();

private:
    HANDLE hMapFilePhysics { nullptr };
    HANDLE hMapFileGraphics { nullptr };
    SPageFilePhysics* acPhysics { nullptr };
    SPageFileGraphics* acGraphics { nullptr };
    bool connected { false };

    std::string formatTime(int ms);
};