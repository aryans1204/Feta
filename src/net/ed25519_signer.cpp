// src/networking/ed25519_signer.cpp
#include "net/ed25519_signer.h"
#include <stdexcept>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>

namespace pascal {
namespace crypto {

bool Ed25519Signer::loadPrivateKeyFromFile(const std::string& filename) {
    FILE* fp = fopen(filename.c_str(), "rb");
    if (!fp) {
        std::cerr << "Error: Cannot open private key file: " << filename << std::endl;
        return false;
    }

    private_key_ = PEM_read_PrivateKey(fp, nullptr, nullptr, nullptr);
    fclose(fp);

    if (!private_key_) {
        std::cerr << "Error: Failed to load private key from PEM file" << std::endl;
        ERR_print_errors_fp(stderr);
        return false;
    }

    // Verify it's an Ed25519 key
    if (EVP_PKEY_id(private_key_) != EVP_PKEY_ED25519) {
        std::cerr << "Error: Key is not Ed25519" << std::endl;
        EVP_PKEY_free(private_key_);
        private_key_ = nullptr;
        return false;
    }

    return true;
}

bool Ed25519Signer::loadPrivateKeyFromString(const std::string& pem_data) {
    BIO* bio = BIO_new_mem_buf(pem_data.c_str(), -1);
    if (!bio) {
        std::cerr << "Error: Failed to create BIO from PEM data" << std::endl;
        return false;
    }

    private_key_ = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);

    if (!private_key_) {
        std::cerr << "Error: Failed to load private key from PEM string" << std::endl;
        ERR_print_errors_fp(stderr);
        return false;
    }

    // Verify it's an Ed25519 key
    if (EVP_PKEY_id(private_key_) != EVP_PKEY_ED25519) {
        std::cerr << "Error: Key is not Ed25519" << std::endl;
        EVP_PKEY_free(private_key_);
        private_key_ = nullptr;
        return false;
    }

    return true;
}

std::string Ed25519Signer::sign_payload(const std::string& payload) const {
    if (!private_key_) {
        std::cerr << "Error: No private key loaded" << std::endl;
        return "";
    }

    // Create signing context
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        std::cerr << "Error: Failed to create signing context" << std::endl;
        return "";
    }

    // Initialize signing
    if (EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, private_key_) <= 0) {
        std::cerr << "Error: Failed to initialize signing" << std::endl;
        EVP_MD_CTX_free(ctx);
        ERR_print_errors_fp(stderr);
        return "";
    }

    // Determine signature length
    size_t sig_len = 0;
    if (EVP_DigestSign(ctx, nullptr, &sig_len, 
                        reinterpret_cast<const unsigned char*>(payload.c_str()), 
                        payload.length()) <= 0) {
        std::cerr << "Error: Failed to determine signature length" << std::endl;
        EVP_MD_CTX_free(ctx);
        ERR_print_errors_fp(stderr);
        return "";
    }

    // Create signature
    std::vector<unsigned char> signature(sig_len);
    if (EVP_DigestSign(ctx, signature.data(), &sig_len,
                        reinterpret_cast<const unsigned char*>(payload.c_str()),
                        payload.length()) <= 0) {
        std::cerr << "Error: Failed to create signature" << std::endl;
        EVP_MD_CTX_free(ctx);
        ERR_print_errors_fp(stderr);
        return "";
    }

    EVP_MD_CTX_free(ctx);

    // Encode to base64
    return base64encode(signature.data(), sig_len);
}

std::string Ed25519Signer::base64encode(unsigned char* data, size_t length) const {
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    
    // Don't add newlines
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    
    bio = BIO_push(b64, bio);
    
    BIO_write(bio, data, length);
    BIO_flush(bio);
    
    BUF_MEM* buffer;
    BIO_get_mem_ptr(bio, &buffer);
    
    std::string result(buffer->data, buffer->length);
    
    BIO_free_all(bio);
    
    return result;
}


}} // namespace pascal::crypto