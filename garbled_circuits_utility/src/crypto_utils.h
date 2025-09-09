#pragma once

#include "common.h"
#include <openssl/sha.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/evp.h>

/**
 * Cryptographic utilities for garbled circuits
 * Provides PRF, encryption/decryption, and random generation
 */
class CryptoUtils {
public:
    // Generate random wire label
    static WireLabel generate_random_label();
    
    // Generate multiple random labels
    static std::vector<WireLabel> generate_random_labels(size_t count);
    
    // Pseudorandom function G(key1, key2, gate_id) -> output
    static std::vector<uint8_t> PRF(const WireLabel& key1, const WireLabel& key2, uint32_t gate_id);
    
    // Encrypt wire label using two input keys
    static std::vector<uint8_t> encrypt_label(const WireLabel& output_label, 
                                            const WireLabel& key1, 
                                            const WireLabel& key2, 
                                            uint32_t gate_id);
    
    // Decrypt to recover wire label
    static WireLabel decrypt_label(const std::vector<uint8_t>& ciphertext,
                                 const WireLabel& key1,
                                 const WireLabel& key2,
                                 uint32_t gate_id);
    
    // Check if decryption was successful (padding verification)
    static bool is_valid_decryption(const std::vector<uint8_t>& decrypted_data);
    
    // XOR two wire labels
    static WireLabel xor_labels(const WireLabel& a, const WireLabel& b);
    
    // Compare wire labels for equality
    static bool labels_equal(const WireLabel& a, const WireLabel& b);
    
    // Convert wire label to hex string (for debugging)
    static std::string label_to_hex(const WireLabel& label);
    
    // Convert hex string to wire label
    static WireLabel hex_to_label(const std::string& hex);
    
    // Hash function for general use
    static std::vector<uint8_t> sha256(const std::vector<uint8_t>& data);
    
    // Serialize wire label to bytes
    static std::vector<uint8_t> serialize_label(const WireLabel& label);
    
    // Deserialize bytes to wire label
    static WireLabel deserialize_label(const std::vector<uint8_t>& data, size_t offset = 0);
    
    // Initialize OpenSSL (called automatically)
    static void init_openssl();
    
    // Cleanup OpenSSL resources
    static void cleanup_openssl();
    
private:
    
    // Internal encryption function
    static std::vector<uint8_t> aes_encrypt(const std::vector<uint8_t>& plaintext, 
                                          const std::vector<uint8_t>& key);
    
    // Internal decryption function
    static std::vector<uint8_t> aes_decrypt(const std::vector<uint8_t>& ciphertext,
                                          const std::vector<uint8_t>& key);
    
    static bool openssl_initialized;
};

/**
 * RAII wrapper for managing OpenSSL context
 */
class OpenSSLContext {
public:
    OpenSSLContext();
    ~OpenSSLContext();
    
    // Non-copyable
    OpenSSLContext(const OpenSSLContext&) = delete;
    OpenSSLContext& operator=(const OpenSSLContext&) = delete;
    
private:
    bool initialized;
};
