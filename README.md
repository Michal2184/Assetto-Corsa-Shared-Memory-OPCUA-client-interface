# In PowerShell
git clone https://github.com/microsoft/vcpkg
cd vcpkg
.\bootstrap-vcpkg.bat

# Make vcpkg auto-integrate with VS/MSBuild
.\vcpkg integrate install

# OpenSSL backend (most common on Windows)
.\vcpkg install open62541[openssl]:x64-windows

# Create openssl_client.cnf
Add to file:
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
URI.1 = urn:desktop-michal:SimpleUAClient
# Optional but nice to have:
DNS.1 = desktop-michal
IP.1  = 127.0.0.1

# Create Certs
openssl genrsa -out client_key.pem 2048

# Self-signed cert 
openssl req -new -x509 -days 825 \
  -key client_key.pem -out client_cert.pem \
  -config openssl_client.cnf -extensions v3_req

# Convert to DER
openssl x509 -in client_cert.pem -outform DER -out client_cert.der
openssl pkcs8 -topk8 -inform PEM -outform DER \
  -in client_key.pem -out client_key.der -nocrypt


