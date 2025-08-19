#define _CRT_SECURE_NO_WARNINGS  // must be before any includes to silence getenv in dotenv.h

#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include "ACSharedOut.h"
#include "dotenv.h"

#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <string>


// Portable safe getenv
#ifdef _MSC_VER
static std::string safe_getenv(const char* name) {
    char* buf = nullptr;
    size_t sz = 0;
    if (_dupenv_s(&buf, &sz, name) == 0 && buf) {
        std::string val(buf);
        free(buf);
        return val;
    }
    return {};
}
#else
static std::string safe_getenv(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}
#endif

static UA_ByteString loadFile(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { std::cerr << "Cannot open file: " << path << "\n"; return UA_BYTESTRING_NULL; }
    f.seekg(0, std::ios::end);
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    UA_ByteString out; UA_ByteString_init(&out);
    if (UA_ByteString_allocBuffer(&out, static_cast<size_t>(size)) != UA_STATUSCODE_GOOD) {
        std::cerr << "alloc failed for: " << path << "\n"; return UA_BYTESTRING_NULL;
    }
    if (!f.read(reinterpret_cast<char*>(out.data), size)) {
        std::cerr << "read failed for: " << path << "\n"; UA_ByteString_clear(&out); return UA_BYTESTRING_NULL;
    }
    return out;
}

static bool batchWriteValues(UA_Client* client,
    const std::vector<std::string>& nodeIds,
    const std::vector<UA_Variant>& variants) {
    if (nodeIds.empty() || nodeIds.size() != variants.size()) return false;

    std::vector<UA_NodeId> ids(nodeIds.size());
    std::vector<UA_WriteValue> w(nodeIds.size());

    for (size_t i = 0; i < nodeIds.size(); ++i) {
        ids[i] = UA_NODEID_STRING_ALLOC(3, nodeIds[i].c_str());
        UA_WriteValue_init(&w[i]);
        w[i].nodeId = ids[i];
        w[i].attributeId = UA_ATTRIBUTEID_VALUE;
        w[i].value.hasValue = true;
        w[i].value.value = variants[i];
    }

    UA_WriteRequest req; UA_WriteRequest_init(&req);
    req.nodesToWrite = w.data();
    req.nodesToWriteSize = w.size();

    UA_WriteResponse resp = UA_Client_Service_write(client, req);
    bool ok = (resp.responseHeader.serviceResult == UA_STATUSCODE_GOOD) && (resp.resultsSize == w.size());
    if (ok) {
        for (size_t i = 0; i < resp.resultsSize; ++i) {
            if (resp.results[i] != UA_STATUSCODE_GOOD) {
                std::cerr << "Batch write failed at " << i << " (ns=3;s:" << nodeIds[i]
                    << ") status=0x" << std::hex << (unsigned)resp.results[i] << std::dec << "\n";
                ok = false;
            }
        }
    }
    else {
        std::cerr << "Batch write request failed: status=0x" << std::hex
            << (unsigned)resp.responseHeader.serviceResult << std::dec << "\n";
    }

    UA_WriteResponse_clear(&resp);
    for (auto& id : ids) UA_NodeId_clear(&id);
    return ok;
}

int main() {
    // Load .env
    dotenv::init();

    std::string endpoint = safe_getenv("ENDPOINT");
    std::string username = safe_getenv("USERNAME");
    std::string password = safe_getenv("PASSWORD");
    std::string delayStr = safe_getenv("DELAY_MS");
    std::string hostname = safe_getenv("HOSTNAME");
    std::string uri = "urn:" + hostname + ":SimpleUAClient";

    int DELAY = delayStr.empty() ? 100 : std::stoi(delayStr);

    if (endpoint.empty() || username.empty() || password.empty()) {
        std::cerr << "Missing .env file\n";
        return 1;
    }

    ACSharedOut ac;
    if (!ac.initialize()) {
        std::cerr << "Failed to connect to Assetto Corsa shared memory.\n";
        return 1;
    }

    // Certs (DER)
    UA_ByteString clientCert = loadFile("certs/client_cert.der");
    UA_ByteString clientKey = loadFile("certs/client_key.der");
    UA_ByteString serverCert = loadFile("certs/server_cert.der");

    if (clientCert.length == 0 || clientKey.length == 0) {
        std::cerr << "Missing client_cert.der or client_key.der (DER encoded).\n";
        return 1;
    }

    UA_Client* client = UA_Client_new();
    UA_ClientConfig* cc = UA_Client_getConfig(client);

    cc->timeout = 30000;
    cc->secureChannelLifeTime = 600000;
    cc->requestedSessionTimeout = 600000.0;

    cc->clientDescription.applicationUri = UA_STRING_ALLOC(uri.c_str());
    cc->clientDescription.applicationType = UA_APPLICATIONTYPE_CLIENT;
    cc->clientDescription.applicationName = UA_LOCALIZEDTEXT_ALLOC("en-US", "SimpleUAClient");
    cc->clientDescription.productUri = UA_STRING_ALLOC("urn:SimpleUAClient");

    std::vector<UA_ByteString> trustList;
    if (serverCert.length > 0) trustList.push_back(serverCert);

    UA_StatusCode sc = UA_ClientConfig_setDefaultEncryption(
        cc, clientCert, clientKey,
        trustList.empty() ? nullptr : trustList.data(), trustList.size(),
        nullptr, 0);

    if (sc != UA_STATUSCODE_GOOD) {
        std::cerr << "UA_ClientConfig_setDefaultEncryption failed: 0x"
            << std::hex << sc << std::dec << "\n";
        UA_Client_delete(client);
        UA_ByteString_clear(&clientCert);
        UA_ByteString_clear(&clientKey);
        UA_ByteString_clear(&serverCert);
        return 1;
    }

    cc->securityMode = UA_MESSAGESECURITYMODE_SIGNANDENCRYPT;
    cc->securityPolicyUri =
        UA_STRING_ALLOC("http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256");

    sc = UA_Client_connectUsername(client,
        endpoint.c_str(),
        username.c_str(),
        password.c_str());
    if (sc != UA_STATUSCODE_GOOD) {
        std::cerr << "Connect failed: 0x" << std::hex << sc << std::dec << "\n";
        UA_Client_delete(client);
        UA_ByteString_clear(&clientCert);
        UA_ByteString_clear(&clientKey);
        UA_ByteString_clear(&serverCert);
        return 1;
    }

    while (true) {
        ACSharedOutData snap = ac.readGame();
        if (!snap.ok) { std::cerr << "Read failed.\n"; break; }

        system("cls");
        std::cout << " #####################################\n";
        std::cout << " # Assetto Corsa - XChange Interface #\n";
        std::cout << " #####################################\n\n";
        std::cout << "Connected. Writing every " << DELAY << " ms. Press Ctrl+C to stop.\n\n";

        std::cout << "CAR DATA: " << DELAY << "ms update\n";
        std::cout << "--------------------------\n";
        std::cout << "Speed:        " << snap.vehicle["speedKmh"] << " km/h\n";
        std::cout << "Engine RPM:   " << snap.vehicle["engineRPM"] << " RPM\n";
        std::cout << "Steer Angle:  " << snap.vehicle["steerAngle"] << " degrees\n";
        std::cout << "Gear:         " << snap.vehicle["gear"] << "\n";
        std::cout << "Fuel:         " << snap.vehicle["fuel"] << " liters\n\n";

        std::cout << "GAME INFO:\n";
        std::cout << "--------------------------\n";
        std::cout << "Completed Laps: " << snap.env["completedLaps"] << "\n";
        std::cout << "Position:       " << snap.env["position"] << "\n";
        std::cout << "Current Time:   " << snap.times["currentTime"] << "\n";
        std::cout << "Last Time:      " << snap.times["lastTime"] << "\n";
        std::cout << "Best Time:      " << snap.times["bestTime"] << "\n\n";
        std::cout << "Press Ctrl+C to exit...";

        // Prepare write variants
        UA_Int32 speedVal = snap.vehicle["speedKmh"];
        UA_Int32 rpmVal = snap.vehicle["engineRPM"];
        UA_Int32 fuelVal = snap.vehicle["fuel"];
        UA_Int32 steerVal = snap.vehicle["steerAngle"];
        UA_Int32 gearVal = snap.vehicle["gear"];
        UA_Int32 gasVal = snap.vehicle["gas"];
        UA_Int32 brakeVal = snap.vehicle["brake"];
        UA_Int32 numLapsVal = snap.env["numberOfLaps"];
        UA_Int32 positionVal = snap.env["position"];
        UA_Int32 completedLapsVal = snap.env["completedLaps"];
        UA_String currentTimeVal = UA_STRING_ALLOC(snap.times["currentTime"].c_str());
        UA_String lastTimeVal = UA_STRING_ALLOC(snap.times["lastTime"].c_str());
        UA_String bestTimeVal = UA_STRING_ALLOC(snap.times["bestTime"].c_str());
        UA_Float windSpeedVal = snap.miscFloats["windSpeed"];
        UA_Float windDirectionVal = snap.miscFloats["windDirection"];

        UA_Variant v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15;
        UA_Variant_init(&v1);  UA_Variant_setScalar(&v1, &speedVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v2);  UA_Variant_setScalar(&v2, &rpmVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v3);  UA_Variant_setScalar(&v3, &fuelVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v4);  UA_Variant_setScalar(&v4, &steerVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v5);  UA_Variant_setScalar(&v5, &gearVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v6);  UA_Variant_setScalar(&v6, &gasVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v7);  UA_Variant_setScalar(&v7, &brakeVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v8);  UA_Variant_setScalar(&v8, &currentTimeVal, &UA_TYPES[UA_TYPES_STRING]);
        UA_Variant_init(&v9);  UA_Variant_setScalar(&v9, &lastTimeVal, &UA_TYPES[UA_TYPES_STRING]);
        UA_Variant_init(&v10); UA_Variant_setScalar(&v10, &bestTimeVal, &UA_TYPES[UA_TYPES_STRING]);
        UA_Variant_init(&v11); UA_Variant_setScalar(&v11, &numLapsVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v12); UA_Variant_setScalar(&v12, &positionVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v13); UA_Variant_setScalar(&v13, &completedLapsVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v14); UA_Variant_setScalar(&v14, &windSpeedVal, &UA_TYPES[UA_TYPES_FLOAT]);
        UA_Variant_init(&v15); UA_Variant_setScalar(&v15, &windDirectionVal, &UA_TYPES[UA_TYPES_FLOAT]);

        std::vector<std::string> nodeIds = {
            "719:Car.speed","719:Car.rpm","719:Car.fuel","719:Car.steerAngle",
            "719:Car.currentGear","719:Car.gas","719:Car.brake",
            "723:GameEnviroment.currentTime","723:GameEnviroment.lastTime","723:GameEnviroment.bestTime",
            "723:GameEnviroment.numberOfLaps","723:GameEnviroment.position","723:GameEnviroment.completedLaps",
            "723:GameEnviroment.windSpeed","723:GameEnviroment.windDirection"
        };
        std::vector<UA_Variant> variants = { v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13,v14,v15 };

        if (!batchWriteValues(client, nodeIds, variants))
            std::cerr << "Batch write operation failed\n";

        UA_String_clear(&currentTimeVal);
        UA_String_clear(&lastTimeVal);
        UA_String_clear(&bestTimeVal);

        UA_Client_run_iterate(client, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(DELAY));
    }

    UA_Client_disconnect(client);
    UA_Client_delete(client);
    UA_ByteString_clear(&clientCert);
    UA_ByteString_clear(&clientKey);
    UA_ByteString_clear(&serverCert);
    return 0;
}
