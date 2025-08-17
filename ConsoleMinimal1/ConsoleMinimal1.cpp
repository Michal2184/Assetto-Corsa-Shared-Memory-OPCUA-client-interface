#include <iostream>
#include <Windows.h>
#include "ACSharedOut.h"

int mainF() {
    ACSharedOut ac;
    if (!ac.initialize()) {
        std::cerr << "Failed to connect to Assetto Corsa shared memory.\n";
        return 1;
    }

    std::cout << "Connected. Press Ctrl+C to stop.\n";

    while (true) {
        ACSharedOutData snap = ac.readGame();
        if (!snap.ok) {
            std::cerr << "Read failed.\n";
            break;
        }

        // Simple output demonstrating data (caller can remove or adapt)
        std::cout << "Speed=" << snap.vehicle["speedKmh"]
                  << " RPM=" << snap.vehicle["engineRPM"]
                  << " Gear=" << snap.vehicle["gear"]
                  << " Lap=" << snap.env["currentLap"]
                  << " Pos=" << snap.env["position"]
                  << " CurrLapTime=" << snap.env["currentTime"]
                  << "\r"; // carriage return for inline updating
        std::cout.flush();

        Sleep(100); // ~100ms update
    }

    return 0;
}
