#pragma once
#include <string>
#include <vector>
#include <memory>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>

namespace pascal {
    namespace crypto {

    class Ed25519Signer {
    public:
        // Constructor loads private key from PEM format
        Ed25519Signer() : private_key_(nullptr) {}
        bool loadPrivateKeyFromFile(const std::string& filename);
        bool loadPrivateKeyFromString(const std::string& pem_data);

        
        // Sign payload and return base64 signature
        std::string sign_payload(const std::string& payload) const;

        std::string base64encode(unsigned char* data, size_t len) const;
        
        ~Ed25519Signer() {
            if (private_key_) {
                EVP_PKEY_free(private_key_);
            }
        }

    private:
        EVP_PKEY* private_key_;
    };
    }
}