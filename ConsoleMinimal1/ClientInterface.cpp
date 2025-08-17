// Build with: open62541 (client + encryption)
// Writes 50.0f to three nodes every 100 ms with Basic256Sha256 + username/password.

#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include "ACSharedOut.h"

#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <chrono>

static const int DELAY = 50; // ms between updates

// Simple batch write function
static bool batchWriteValues(UA_Client* client, 
                           const std::vector<std::string>& nodeIds,
                           const std::vector<UA_Variant>& variants) {
    if (nodeIds.empty() || nodeIds.size() != variants.size()) {
        return false;
    }

    // Create arrays for the batch operation
    std::vector<UA_NodeId> nodeIdArray(nodeIds.size());
    std::vector<UA_WriteValue> writeValues(nodeIds.size());
    
    // Setup the write values
    for (size_t i = 0; i < nodeIds.size(); ++i) {
        nodeIdArray[i] = UA_NODEID_STRING_ALLOC(3, nodeIds[i].c_str());
        
        UA_WriteValue_init(&writeValues[i]);
        writeValues[i].nodeId = nodeIdArray[i];
        writeValues[i].attributeId = UA_ATTRIBUTEID_VALUE;
        writeValues[i].value.hasValue = true;
        writeValues[i].value.value = variants[i];
    }

    UA_WriteRequest req;
    UA_WriteRequest_init(&req);
    req.nodesToWrite = writeValues.data();
    req.nodesToWriteSize = writeValues.size();

    UA_WriteResponse resp = UA_Client_Service_write(client, req);
    bool allOk = (resp.responseHeader.serviceResult == UA_STATUSCODE_GOOD) &&
                 (resp.resultsSize == writeValues.size());

    if (allOk) {
        for (size_t i = 0; i < resp.resultsSize; ++i) {
            if (resp.results[i] != UA_STATUSCODE_GOOD) {
                std::cerr << "Batch write failed for operation " << i 
                          << " (ns=3;s:" << nodeIds[i]
                          << ") status=0x" << std::hex << (unsigned)resp.results[i] << std::dec << "\n";
                allOk = false;
            }
        }
    } else {
        std::cerr << "Batch write request failed: status=0x" << std::hex 
                  << (unsigned)resp.responseHeader.serviceResult << std::dec << "\n";
    }

    // Cleanup
    UA_WriteResponse_clear(&resp);
    for (size_t i = 0; i < nodeIdArray.size(); ++i) {
        UA_NodeId_clear(&nodeIdArray[i]);
    }
    
    return allOk;
}

// Load an entire file into a UA_ByteString (DER cert/key helper)
static UA_ByteString loadFile(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "Cannot open file: " << path << "\n";
        return UA_BYTESTRING_NULL;
    }
    f.seekg(0, std::ios::end);
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);

    UA_ByteString out;
    UA_ByteString_init(&out);
    if (UA_ByteString_allocBuffer(&out, static_cast<size_t>(size)) != UA_STATUSCODE_GOOD) {
        std::cerr << "alloc failed for: " << path << "\n";
        return UA_BYTESTRING_NULL;
    }
    if (!f.read(reinterpret_cast<char*>(out.data), size)) {
        std::cerr << "read failed for: " << path << "\n";
        UA_ByteString_clear(&out);
        return UA_BYTESTRING_NULL;
    }
    return out;
}

// Placeholder for writing integers
static bool writeInteger(UA_Client* client, const char* nodeIdString, int value) {
    UA_Variant v; UA_Variant_init(&v);
    UA_Int32 val = value;
    UA_Variant_setScalar(&v, &val, &UA_TYPES[UA_TYPES_INT32]);

    UA_NodeId nid = UA_NODEID_STRING_ALLOC(3, nodeIdString); // ns=3;s:<nodeIdString>
    UA_WriteValue wv; UA_WriteValue_init(&wv);
    wv.nodeId = nid;
    wv.attributeId = UA_ATTRIBUTEID_VALUE;
    wv.value.hasValue = true;
    wv.value.value = v;

    UA_WriteRequest req; UA_WriteRequest_init(&req);
    req.nodesToWrite = &wv;
    req.nodesToWriteSize = 1;

    UA_WriteResponse resp = UA_Client_Service_write(client, req);
    bool ok = (resp.responseHeader.serviceResult == UA_STATUSCODE_GOOD) &&
        (resp.resultsSize == 1) &&
        (resp.results[0] == UA_STATUSCODE_GOOD);

    if (!ok) {
        std::cerr << "Write failed for ns=3;s:" << nodeIdString
            << " status=0x" << std::hex
            << (unsigned)(resp.resultsSize ? resp.results[0] : resp.responseHeader.serviceResult)
            << std::dec << "\n";
    }

    UA_WriteResponse_clear(&resp);
    UA_NodeId_clear(&nid);
    return ok;
}

// Placeholder for writing strings
static bool writeString(UA_Client* client, const char* nodeIdString, const std::string& value) {
    UA_Variant v; UA_Variant_init(&v);
    UA_String val = UA_STRING_ALLOC(value.c_str());
    UA_Variant_setScalar(&v, &val, &UA_TYPES[UA_TYPES_STRING]);

    UA_NodeId nid = UA_NODEID_STRING_ALLOC(3, nodeIdString); // ns=3;s:<nodeIdString>
    UA_WriteValue wv; UA_WriteValue_init(&wv);
    wv.nodeId = nid;
    wv.attributeId = UA_ATTRIBUTEID_VALUE;
    wv.value.hasValue = true;
    wv.value.value = v;

    UA_WriteRequest req; UA_WriteRequest_init(&req);
    req.nodesToWrite = &wv;
    req.nodesToWriteSize = 1;

    UA_WriteResponse resp = UA_Client_Service_write(client, req);
    bool ok = (resp.responseHeader.serviceResult == UA_STATUSCODE_GOOD) &&
        (resp.resultsSize == 1) &&
        (resp.results[0] == UA_STATUSCODE_GOOD);

    if (!ok) {
        std::cerr << "Write failed for ns=3;s:" << nodeIdString
            << " status=0x" << std::hex
            << (unsigned)(resp.resultsSize ? resp.results[0] : resp.responseHeader.serviceResult)
            << std::dec << "\n";
    }

    UA_WriteResponse_clear(&resp);
    UA_NodeId_clear(&nid);
    UA_String_clear(&val);
    return ok;
}

// Method to write float values to the OPC UA server
static bool writeFloat(UA_Client* client, const char* nodeIdString, float value) {
    UA_Variant v; 
    UA_Variant_init(&v);
    UA_Float val = value;
    UA_Variant_setScalar(&v, &val, &UA_TYPES[UA_TYPES_FLOAT]);

    UA_NodeId nid = UA_NODEID_STRING_ALLOC(3, nodeIdString); // ns=3;s:<nodeIdString>
    UA_WriteValue wv; 
    UA_WriteValue_init(&wv);
    wv.nodeId = nid;
    wv.attributeId = UA_ATTRIBUTEID_VALUE;
    wv.value.hasValue = true;
    wv.value.value = v;

    UA_WriteRequest req; 
    UA_WriteRequest_init(&req);
    req.nodesToWrite = &wv;
    req.nodesToWriteSize = 1;

    UA_WriteResponse resp = UA_Client_Service_write(client, req);
    bool ok = (resp.responseHeader.serviceResult == UA_STATUSCODE_GOOD) &&
              (resp.resultsSize == 1) &&
              (resp.results[0] == UA_STATUSCODE_GOOD);

    if (!ok) {
        std::cerr << "Write failed for ns=3;s:" << nodeIdString
                  << " status=0x" << std::hex
                  << (unsigned)(resp.resultsSize ? resp.results[0] : resp.responseHeader.serviceResult)
                  << std::dec << "\n";
    }

    UA_WriteResponse_clear(&resp);
    UA_NodeId_clear(&nid);
    return ok;
}

int main() {
    ACSharedOut ac;
    if (!ac.initialize()) {
        std::cerr << "Failed to connect to Assetto Corsa shared memory.\n";
        return 1;
    }

    const char* endpoint = "opc.tcp://desktop-michal:48031";
    const char* username = "opcuauser";
    const char* password = "Password1";

    // Load creds/certs (DER)
    UA_ByteString clientCert = loadFile("certs/client_cert.der");
    UA_ByteString clientKey = loadFile("certs/client_key.der");
    UA_ByteString serverCert = loadFile("certs/server_cert.der"); // put server cert here (DER)

    if (clientCert.length == 0 || clientKey.length == 0) {
        std::cerr << "Missing client_cert.der or client_key.der (DER encoded).\n";
        return 1;
    }

    // added

    UA_Client* client = UA_Client_new();
    UA_ClientConfig* cc = UA_Client_getConfig(client);

    // Make session/user auth waits tolerant (20–30s)
    cc->timeout = 30000;                       // ms: per-service timeout (covers ActivateSession)

    // Keep the channel & session around long enough
    cc->secureChannelLifeTime = 600000;    // ms: 10 minutes (server may revise)
    cc->requestedSessionTimeout = 600000.0;  // ms: request 10 minutes

    // Recommended: ensure the app URI matches your cert's SAN:URI
    cc->clientDescription.applicationUri =
        UA_STRING_ALLOC("urn:desktop-michal:SimpleUAClient");
    cc->clientDescription.applicationType = UA_APPLICATIONTYPE_CLIENT;
    cc->clientDescription.applicationName =
        UA_LOCALIZEDTEXT_ALLOC("en-US", "SimpleUAClient");
    cc->clientDescription.productUri = UA_STRING_ALLOC("urn:SimpleUAClient");

    // Trust list: just the server certificate (can add more)
    std::vector<UA_ByteString> trustList;
    if (serverCert.length > 0) {
        trustList.push_back(serverCert);
    }
    else {
        std::cerr << "Warning: server_cert.der not found; connection may fail due to trust check.\n";
    }

    // Configure encryption + enforce Basic256Sha256 + SignAndEncrypt
    UA_StatusCode sc = UA_ClientConfig_setDefaultEncryption(
        cc,
        clientCert, clientKey,
        trustList.empty() ? nullptr : trustList.data(), trustList.size(),
        nullptr, 0);

    if (sc != UA_STATUSCODE_GOOD) {
        std::cerr << "UA_ClientConfig_setDefaultEncryption failed: 0x" << std::hex << sc << std::dec << "\n";
        UA_Client_delete(client);
        UA_ByteString_clear(&clientCert);
        UA_ByteString_clear(&clientKey);
        UA_ByteString_clear(&serverCert);
        return 1;
    }

    cc->securityMode = UA_MESSAGESECURITYMODE_SIGNANDENCRYPT;
    cc->securityPolicyUri = UA_STRING_ALLOC("http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256");



    // Connect with username/password
    sc = UA_Client_connectUsername(client, endpoint, username, password);
    if (sc != UA_STATUSCODE_GOOD) {
        std::cerr << "Connect failed: 0x" << std::hex << sc << std::dec << "\n";
        UA_Client_delete(client);
        UA_ByteString_clear(&clientCert);
        UA_ByteString_clear(&clientKey);
        UA_ByteString_clear(&serverCert);
        return 1;
    }

    std::cout << "Connected. Writing every 100 ms. Press Ctrl+C to stop.\n";

    while (true) {
        // 1. read the values from the shared memory
        ACSharedOutData snap = ac.readGame();
        if (!snap.ok) {
            std::cerr << "Read failed.\n";
            break;
        }

        /// show debug output
        // Simple output demonstrating data (caller can remove or adapt)
        system("cls");
        std::cout << " #####################################\n";
        std::cout << " # Assetto Corsa - XChange Interface #\n";
        std::cout << " #####################################\n\n";

        std::cout << "CAR DATA: " << DELAY << "ms update" << "\n";
        std::cout << "--------------------------\n";
        std::cout << "Speed:        " << snap.vehicle["speedKmh"] << " km/h\n";
        std::cout << "Engine RPM:   " << snap.vehicle["engineRPM"] << " RPM\n";
        std::cout << "Steer Angle:  " << snap.vehicle["steerAngle"] << " degrees\n";
        std::cout << "Gear:         " << snap.vehicle["gear"] - 1 << "\n";
        std::cout << "Fuel:         " << snap.vehicle["fuel"] << " liters\n\n";

        std::cout << "GAME INFO:\n";
        std::cout << "--------------------------\n";
        std::cout << "Current Lap:  " << snap.env["currentLap"];
        std::cout << "\n";
        std::cout << "Position:     " << snap.env["position"] << "\n";
        std::cout << "Current Time: " << snap.times["currentTime"] << "\n";
        std::cout << "Last Time:    " << snap.env["lastTime"] << "\n";
        std::cout << "Best Time:    " << snap.env["bestTime"] << "\n\n";

        std::cout << "Press Ctrl+C to exit...";
  

        // 2. write values to the server using batch write
        std::vector<std::string> nodeIds;
        std::vector<UA_Variant> variants;
        
        nodeIds.reserve(15);
        variants.reserve(15);
        
        // Prepare integer values
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
        
        // Prepare string values
        UA_String currentTimeVal = UA_STRING_ALLOC(snap.times["currentTime"].c_str());
        UA_String lastTimeVal = UA_STRING_ALLOC(snap.times["lastTime"].c_str());
        UA_String bestTimeVal = UA_STRING_ALLOC(snap.times["bestTime"].c_str());
        
        // Prepare float values
        UA_Float windSpeedVal = snap.miscFloats["windSpeed"];
        UA_Float windDirectionVal = snap.miscFloats["windDirection"];
        
        // Create variants
        UA_Variant v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15;
        
        UA_Variant_init(&v1); UA_Variant_setScalar(&v1, &speedVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v2); UA_Variant_setScalar(&v2, &rpmVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v3); UA_Variant_setScalar(&v3, &fuelVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v4); UA_Variant_setScalar(&v4, &steerVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v5); UA_Variant_setScalar(&v5, &gearVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v6); UA_Variant_setScalar(&v6, &gasVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v7); UA_Variant_setScalar(&v7, &brakeVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v8); UA_Variant_setScalar(&v8, &currentTimeVal, &UA_TYPES[UA_TYPES_STRING]);
        UA_Variant_init(&v9); UA_Variant_setScalar(&v9, &lastTimeVal, &UA_TYPES[UA_TYPES_STRING]);
        UA_Variant_init(&v10); UA_Variant_setScalar(&v10, &bestTimeVal, &UA_TYPES[UA_TYPES_STRING]);
        UA_Variant_init(&v11); UA_Variant_setScalar(&v11, &numLapsVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v12); UA_Variant_setScalar(&v12, &positionVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v13); UA_Variant_setScalar(&v13, &completedLapsVal, &UA_TYPES[UA_TYPES_INT32]);
        UA_Variant_init(&v14); UA_Variant_setScalar(&v14, &windSpeedVal, &UA_TYPES[UA_TYPES_FLOAT]);
        UA_Variant_init(&v15); UA_Variant_setScalar(&v15, &windDirectionVal, &UA_TYPES[UA_TYPES_FLOAT]);
        
        // Add to vectors
        nodeIds.push_back("719:Car.speed");
        nodeIds.push_back("719:Car.rpm");
        nodeIds.push_back("719:Car.fuel");
        nodeIds.push_back("719:Car.steerAngle");
        nodeIds.push_back("719:Car.currentGear");
        nodeIds.push_back("719:Car.gas");
        nodeIds.push_back("719:Car.brake");
        nodeIds.push_back("723:GameEnviroment.currentTime");
        nodeIds.push_back("723:GameEnviroment.lastTime");
        nodeIds.push_back("723:GameEnviroment.bestTime");
        nodeIds.push_back("723:GameEnviroment.numberOfLaps");
        nodeIds.push_back("723:GameEnviroment.position");
        nodeIds.push_back("723:GameEnviroment.completedLaps");
        nodeIds.push_back("723:GameEnviroment.windSpeed");
        nodeIds.push_back("723:GameEnviroment.windDirection");
        
        variants.push_back(v1);
        variants.push_back(v2);
        variants.push_back(v3);
        variants.push_back(v4);
        variants.push_back(v5);
        variants.push_back(v6);
        variants.push_back(v7);
        variants.push_back(v8);
        variants.push_back(v9);
        variants.push_back(v10);
        variants.push_back(v11);
        variants.push_back(v12);
        variants.push_back(v13);
        variants.push_back(v14);
        variants.push_back(v15);
        
        // Execute batch write
        bool writeSuccess = batchWriteValues(client, nodeIds, variants);
        if (!writeSuccess) {
            std::cerr << "Batch write operation failed\n";
        }
        
        // Cleanup string values
        UA_String_clear(&currentTimeVal);
        UA_String_clear(&lastTimeVal);
        UA_String_clear(&bestTimeVal);

        // Service keepalive / subscriptions, etc.
        UA_Client_run_iterate(client, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(DELAY));
    }

    // Cleanup (not reached in this simple loop)
    UA_Client_disconnect(client);
    UA_Client_delete(client);
    UA_ByteString_clear(&clientCert);
    UA_ByteString_clear(&clientKey);
    UA_ByteString_clear(&serverCert);
    return 0;
}
