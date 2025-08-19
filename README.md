# Assetto Corsa ➝ AVEVA Application Server Bridge

This project is a C++ interface that connects **Assetto Corsa shared memory** to **AVEVA Application Server (Galaxy)**.  
It continuously reads telemetry data from Assetto Corsa and sends it into Galaxy attributes at a rate of ~180 ms.  

---

## 📊 Data Flow

```mermaid
flowchart LR
    AC[Assetto Corsa] -- Shared Memory --> Bridge[Bridge Application (C++)]
    Bridge -- OPC UA (OpenSSL) --> Galaxy[AVEVA Application Server Galaxy]
```

---

## ✨ Features
- Reads live telemetry from **Assetto Corsa shared memory**
- Writes to **AVEVA Application Server Galaxy attributes**
- Update rate of ~180 ms
- Secure OPC UA client connection using OpenSSL certificates

---

## ⚙️ Prerequisites
- **Windows 10/11**
- **Visual Studio (MSVC toolchain)**
- **[vcpkg](https://github.com/microsoft/vcpkg)**
- **OpenSSL**

---

## 🚀 Installation

### 1. Clone and bootstrap vcpkg
```powershell
git clone https://github.com/microsoft/vcpkg
cd vcpkg
.ootstrap-vcpkg.bat
```

### 2. Integrate vcpkg with Visual Studio / MSBuild
```powershell
.
cpkg integrate install
```

### 3. Install dependencies
```powershell
.
cpkg install open62541[openssl]:x64-windows
```

---

## 🔐 Certificate Setup

### 1. Create `openssl_client.cnf`
Create a file named **`openssl_client.cnf`** with the following content:

```ini
[ req ]
default_bits       = 2048
prompt             = no
default_md         = sha256
distinguished_name = req_dn
x509_extensions    = v3_req

[ req_dn ]
CN = SimpleUAClient

[ v3_req ]
basicConstraints = CA:FALSE
keyUsage = digitalSignature, keyEncipherment, dataEncipherment
extendedKeyUsage = clientAuth
subjectAltName = @alt_names

[ alt_names ]
# MUST match cc->clientDescription.applicationUri above:
URI.1 = urn:<hostname>:SimpleUAClient
# Optional but nice to have:
DNS.1 = <hostname>
IP.1  = 127.0.0.1
```

---

### 2. Generate client certificates
Run the following commands:

```powershell
# Generate private key
openssl genrsa -out client_key.pem 2048

# Generate self-signed certificate
openssl req -new -x509 -days 825 `
  -key client_key.pem -out client_cert.pem `
  -config openssl_client.cnf -extensions v3_req
```

---

### 3. Convert to DER format
```powershell
openssl x509 -in client_cert.pem -outform DER -out client_cert.der
openssl pkcs8 -topk8 -inform PEM -outform DER `
  -in client_key.pem -out client_key.der -nocrypt
```

---

## ▶️ Usage
1. Build the project in **Visual Studio**.
2. Ensure certificates are available in the same directory as the executable.
3. Launch Assetto Corsa.
4. Run the bridge application to start sending telemetry data into **Galaxy**.

---

## 📄 License
This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
