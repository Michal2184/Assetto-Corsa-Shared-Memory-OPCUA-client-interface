// Build with: open62541 (client + encryption)
// Writes 50.0f to three nodes every 100 ms with Basic256Sha256 + username/password.

#include <open62541/client.h>
#include <open62541/client_config_default.h>

#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <chrono>

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

    UA_NodeId nid = UA_NODEID_STRING_ALLOC(3, nodeIdString); // ns=3;s=<nodeIdString>
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

int main() {
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

    // Loop (100 ms)
    while (true) {
        (void)writeInteger(client, "719:Car.speed", 50);
        (void)writeInteger(client, "719:Car.rpm", 50);
        (void)writeInteger(client, "719:Car.fuel", 20);

        // Service keepalive / subscriptions, etc.
        UA_Client_run_iterate(client, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Cleanup (not reached in this simple loop)
    UA_Client_disconnect(client);
    UA_Client_delete(client);
    UA_ByteString_clear(&clientCert);
    UA_ByteString_clear(&clientKey);
    UA_ByteString_clear(&serverCert);
    return 0;
}
