// src/networking/ed25519_signer.cpp
#include "ed25519_signer.h"
#include <sodium.h>
#include <stdexcept>
#include <array>
#include <algorithm>

namespace pascal {
namespace crypto {

class Ed25519Signer::Impl {
public:
    std::array<unsigned char, crypto_sign_SECRETKEYBYTES> secret_key;
    std::array<unsigned char, crypto_sign_PUBLICKEYBYTES> public_key;
    
    Impl(const std::string& private_key_pem) {
        if (sodium_init() < 0) {
            throw std::runtime_error("Failed to initialize libsodium");
        }
        
        // Parse PEM private key
        load_private_key_from_pem(private_key_pem);
    }
    
    void load_private_key_from_pem(const std::string& pem_data) {
        // Remove PEM headers and decode base64
        std::string key_data = extract_key_from_pem(pem_data);
        
        // Ed25519 private key in PEM is 32 bytes after ASN.1 parsing
        if (key_data.size() != 32) {
            throw std::runtime_error("Invalid Ed25519 private key size");
        }
        
        // Copy seed (private key)
        std::copy(key_data.begin(), key_data.end(), secret_key.begin());
        
        // Generate public key from private key
        crypto_sign_seed_keypair(public_key.data(), secret_key.data(), 
                               reinterpret_cast<const unsigned char*>(key_data.data()));
    }
    
    std::string extract_key_from_pem(const std::string& pem_data) {
        // Simple PEM parser - in production use OpenSSL or proper ASN.1 parser
        size_t start = pem_data.find("-----BEGIN PRIVATE KEY-----");
        size_t end = pem_data.find("-----END PRIVATE KEY-----");
        
        if (start == std::string::npos || end == std::string::npos) {
            throw std::runtime_error("Invalid PEM format");
        }
        
        start += 27; // Length of "-----BEGIN PRIVATE KEY-----"
        std::string base64_data = pem_data.substr(start, end - start);
        
        // Remove whitespace
        base64_data.erase(std::remove_if(base64_data.begin(), base64_data.end(),
                         [](char c) { return std::isspace(c); }), base64_data.end());
        
        return base64_decode(base64_data);
    }
    
    std::string base64_decode(const std::string& encoded) {
        // Implement base64 decode or use a library
        // For production, use a proper base64 library
        // This is simplified for demonstration
        std::vector<unsigned char> decoded(encoded.size());
        size_t decoded_len;
        
        if (sodium_base642bin(decoded.data(), decoded.size(),
                            encoded.c_str(), encoded.size(),
                            nullptr, &decoded_len, nullptr,
                            sodium_base64_VARIANT_ORIGINAL) != 0) {
            throw std::runtime_error("Base64 decode failed");
        }
        
        return std::string(decoded.begin(), decoded.begin() + decoded_len);
    }
    
    std::string base64_encode(const unsigned char* data, size_t len) {
        std::vector<char> encoded(sodium_base64_ENCODED_LEN(len, sodium_base64_VARIANT_ORIGINAL));
        
        sodium_bin2base64(encoded.data(), encoded.size(),
                         data, len, sodium_base64_VARIANT_ORIGINAL);
        
        return std::string(encoded.data());
    }
};

Ed25519Signer::Ed25519Signer(const std::string& private_key_pem)
    : pimpl_(std::make_unique<Impl>(private_key_pem)) {
}


std::string Ed25519Signer::sign_payload(const std::string& payload) const {
    std::array<unsigned char, crypto_sign_BYTES> signature;
    unsigned long long signature_len;
    
    // Sign the payload
    if (crypto_sign_detached(signature.data(), &signature_len,
                           reinterpret_cast<const unsigned char*>(payload.c_str()),
                           payload.size(),
                           pimpl_->secret_key.data()) != 0) {
        throw std::runtime_error("Ed25519 signing failed");
    }
    
    // Return base64 encoded signature
    return pimpl_->base64_encode(signature.data(), signature_len);
}

std::string Ed25519Signer::get_public_key_base64() const {
    return pimpl_->base64_encode(pimpl_->public_key.data(), crypto_sign_PUBLICKEYBYTES);
}

Ed25519Signer::~Ed25519Signer() = default;

}} // namespace pascal::crypto