#pragma once
#include <string>
#include <vector>
#include <memory>

namespace pascal {
    namespace crypto {

    class Ed25519Signer {
    public:
        // Constructor loads private key from PEM format
        explicit Ed25519Signer(const std::string& private_key_pem);
        
        
        // Sign payload and return base64 signature
        std::string sign_payload(const std::string& payload) const;
        
        // Get public key in base64 format (for API key registration)
        std::string get_public_key_base64() const;
        
        ~Ed25519Signer();

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };
    }
}