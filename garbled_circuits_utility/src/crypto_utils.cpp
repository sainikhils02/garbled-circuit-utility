#include "crypto_utils.h"
#include <iomanip>
#include <sstream>
#include <cstring>

bool CryptoUtils::openssl_initialized = false;

void CryptoUtils::init_openssl() {
    if (!openssl_initialized) {
        openssl_initialized = true;
    }
}

void CryptoUtils::cleanup_openssl() {
    if (openssl_initialized) {
        openssl_initialized = false;
    }
}

WireLabel CryptoUtils::generate_random_label() {
    init_openssl();
    
    WireLabel label;
    if (RAND_bytes(label.data(), WIRE_LABEL_SIZE) != 1) {
        throw CryptoException("Failed to generate random label");
    }
    return label;
}

std::vector<WireLabel> CryptoUtils::generate_random_labels(size_t count) {
    std::vector<WireLabel> labels;
    labels.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        labels.push_back(generate_random_label());
    }
    
    return labels;
}

std::vector<uint8_t> CryptoUtils::PRF(const WireLabel& key1, const WireLabel& key2, uint32_t gate_id) {
    init_openssl();
    
    // Combine keys and gate_id for PRF input
    std::vector<uint8_t> input;
    input.insert(input.end(), key1.begin(), key1.end());
    input.insert(input.end(), key2.begin(), key2.end());
    
    // Add gate_id in big-endian format
    input.push_back((gate_id >> 24) & 0xFF);
    input.push_back((gate_id >> 16) & 0xFF);
    input.push_back((gate_id >> 8) & 0xFF);
    input.push_back(gate_id & 0xFF);
    
    // Use SHA-256 as PRF
    return sha256(input);
}

std::vector<uint8_t> CryptoUtils::encrypt_label(const WireLabel& output_label,
                                              const WireLabel& key1,
                                              const WireLabel& key2,
                                              uint32_t gate_id) {
    // Create plaintext: output_label + padding for verification
    std::vector<uint8_t> plaintext;
    plaintext.insert(plaintext.end(), output_label.begin(), output_label.end());
    
    // Add padding (16 zero bytes for verification)
    plaintext.insert(plaintext.end(), 16, 0x00);
    
    // Generate encryption key from PRF
    auto prf_output = PRF(key1, key2, gate_id);
    std::vector<uint8_t> enc_key(prf_output.begin(), prf_output.begin() + WIRE_LABEL_SIZE);
    
    // Encrypt using AES
    auto ciphertext = aes_encrypt(plaintext, enc_key);
    return ciphertext;
}

WireLabel CryptoUtils::decrypt_label(const std::vector<uint8_t>& ciphertext,
                                   const WireLabel& key1,
                                   const WireLabel& key2,
                                   uint32_t gate_id) {
    // Generate decryption key from PRF
    auto prf_output = PRF(key1, key2, gate_id);
    std::vector<uint8_t> dec_key(prf_output.begin(), prf_output.begin() + WIRE_LABEL_SIZE);
    
    // Decrypt using AES
    auto plaintext = aes_decrypt(ciphertext, dec_key);
    
    if (plaintext.size() < WIRE_LABEL_SIZE) {
        throw CryptoException("Decryption failed: insufficient data");
    }
    
    // Extract wire label
    WireLabel label;
    std::copy(plaintext.begin(), plaintext.begin() + WIRE_LABEL_SIZE, label.begin());
    
    return label;
}

bool CryptoUtils::is_valid_decryption(const std::vector<uint8_t>& decrypted_data) {
    if (decrypted_data.size() < WIRE_LABEL_SIZE + 16) {
        return false;
    }
    
    // Check padding (last 16 bytes should be zero)
    for (size_t i = WIRE_LABEL_SIZE; i < WIRE_LABEL_SIZE + 16; ++i) {
        if (decrypted_data[i] != 0x00) {
            return false;
        }
    }
    
    return true;
}

WireLabel CryptoUtils::xor_labels(const WireLabel& a, const WireLabel& b) {
    WireLabel result;
    for (size_t i = 0; i < WIRE_LABEL_SIZE; ++i) {
        result[i] = a[i] ^ b[i];
    }
    return result;
}

bool CryptoUtils::labels_equal(const WireLabel& a, const WireLabel& b) {
    return std::equal(a.begin(), a.end(), b.begin());
}

std::string CryptoUtils::label_to_hex(const WireLabel& label) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t byte : label) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

WireLabel CryptoUtils::hex_to_label(const std::string& hex) {
    if (hex.length() != WIRE_LABEL_SIZE * 2) {
        throw CryptoException("Invalid hex string length for wire label");
    }
    
    WireLabel label;
    for (size_t i = 0; i < WIRE_LABEL_SIZE; ++i) {
        std::string byte_str = hex.substr(i * 2, 2);
        label[i] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
    }
    return label;
}

std::vector<uint8_t> CryptoUtils::sha256(const std::vector<uint8_t>& data) {
    init_openssl();
    
    std::vector<uint8_t> hash(32); // SHA256 digest length
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    
    if (!ctx) {
        throw CryptoException("Failed to create hash context");
    }
    
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        throw CryptoException("Failed to initialize hash");
    }
    
    if (EVP_DigestUpdate(ctx, data.data(), data.size()) != 1) {
        EVP_MD_CTX_free(ctx);
        throw CryptoException("Failed to update hash");
    }
    
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(ctx, hash.data(), &hash_len) != 1) {
        EVP_MD_CTX_free(ctx);
        throw CryptoException("Failed to finalize hash");
    }
    
    EVP_MD_CTX_free(ctx);
    hash.resize(hash_len);
    return hash;
}

std::vector<uint8_t> CryptoUtils::serialize_label(const WireLabel& label) {
    return std::vector<uint8_t>(label.begin(), label.end());
}

WireLabel CryptoUtils::deserialize_label(const std::vector<uint8_t>& data, size_t offset) {
    if (data.size() < offset + WIRE_LABEL_SIZE) {
        throw CryptoException("Insufficient data for label deserialization");
    }
    
    WireLabel label;
    std::copy(data.begin() + offset, data.begin() + offset + WIRE_LABEL_SIZE, label.begin());
    return label;
}

std::vector<uint8_t> CryptoUtils::aes_encrypt(const std::vector<uint8_t>& plaintext,
                                            const std::vector<uint8_t>& key) {
    init_openssl();
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw CryptoException("Failed to create cipher context");
    }
    
    // Initialize encryption
    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, key.data(), NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw CryptoException("Failed to initialize encryption");
    }
    
    // Disable padding
    EVP_CIPHER_CTX_set_padding(ctx, 0);
    
    std::vector<uint8_t> ciphertext(plaintext.size() + AES_BLOCK_SIZE);
    int len1, len2;
    
    // Encrypt data
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len1, plaintext.data(), plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw CryptoException("Failed to encrypt data");
    }
    
    // Finalize encryption
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len1, &len2) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw CryptoException("Failed to finalize encryption");
    }
    
    EVP_CIPHER_CTX_free(ctx);
    ciphertext.resize(len1 + len2);
    
    return ciphertext;
}

std::vector<uint8_t> CryptoUtils::aes_decrypt(const std::vector<uint8_t>& ciphertext,
                                            const std::vector<uint8_t>& key) {
    init_openssl();
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw CryptoException("Failed to create cipher context");
    }
    
    // Initialize decryption
    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, key.data(), NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw CryptoException("Failed to initialize decryption");
    }
    
    // Disable padding
    EVP_CIPHER_CTX_set_padding(ctx, 0);
    
    std::vector<uint8_t> plaintext(ciphertext.size() + AES_BLOCK_SIZE);
    int len1, len2;
    
    // Decrypt data
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len1, ciphertext.data(), ciphertext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw CryptoException("Failed to decrypt data");
    }
    
    // Finalize decryption
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len1, &len2) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw CryptoException("Failed to finalize decryption");
    }
    
    EVP_CIPHER_CTX_free(ctx);
    plaintext.resize(len1 + len2);
    return plaintext;
}

// OpenSSLContext implementation
OpenSSLContext::OpenSSLContext() : initialized(false) {
    CryptoUtils::init_openssl();
    initialized = true;
}

OpenSSLContext::~OpenSSLContext() {
    if (initialized) {
        CryptoUtils::cleanup_openssl();
    }
}
