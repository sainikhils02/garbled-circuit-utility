#include "ot_handler.h"
#include "crypto_utils.h"
#include "socket_utils.h"
#include "common.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <openssl/sha.h>

using namespace osuCrypto;

block OTHandler::wire_label_to_block(const WireLabel& label) {
    block b{};
    std::memcpy(&b, label.data(), std::min<size_t>(16, label.size()));
    return b;
}

WireLabel OTHandler::block_to_wire_label(const block& b) {
    WireLabel wl{0};
    std::memcpy(wl.data(), &b, std::min(sizeof(block), wl.size()));
    return wl;
}

OTHandler::OTHandler()
    : initialized(false), is_sender(false), total_ots_performed(0), prng(nullptr) {}

OTHandler::~OTHandler() { cleanup(); }

OTHandler::OTHandler(OTHandler&& other) noexcept
    : initialized(other.initialized), is_sender(other.is_sender), total_ots_performed(other.total_ots_performed), prng(std::move(other.prng)) {
    other.initialized = false; other.is_sender = false; other.total_ots_performed = 0;
}

OTHandler& OTHandler::operator=(OTHandler&& other) noexcept {
    if (this != &other) {
        cleanup();
        initialized = other.initialized;
        is_sender = other.is_sender;
        total_ots_performed = other.total_ots_performed;
        prng = std::move(other.prng);
        other.initialized = false; other.is_sender = false; other.total_ots_performed = 0;
    }
    return *this;
}

void OTHandler::init_sender(SocketConnection& connection) {
    if (initialized) throw OTException("OTHandler already initialized");
    if (!connection.is_connected()) throw OTException("Sender socket not connected");
    prng = std::make_unique<PRNG>(sysRandomSeed());
    is_sender = true;
    initialized = true;
    LOG_INFO("SimplestOT sender initialized");
}

void OTHandler::init_receiver(SocketConnection& connection) {
    if (initialized) throw OTException("OTHandler already initialized");
    if (!connection.is_connected()) throw OTException("Receiver socket not connected");
    prng = std::make_unique<PRNG>(sysRandomSeed());
    is_sender = false;
    initialized = true;
    LOG_INFO("SimplestOT receiver initialized");
}

// For now, no direct SimplestOT over raw POSIX socket is implemented.
// We fail fast until a coproto::Socket bridge is provided.
bool OTHandler::send_ot(const std::vector<std::pair<WireLabel, WireLabel>>& pairs, SocketConnection& connection) {
    if (!initialized || !is_sender) throw OTException("OT sender not properly initialized");
    if (pairs.empty()) return true;
    auto ep = resolve_endpoint();
    // Run base OTs to get random blocks
    std::vector<std::array<block,2>> otBlocks(pairs.size());
    simplest_ot_send(pairs.size(), otBlocks, ep);
    // Mask provided labels with KDF(H(block||role||index||bit))
    std::vector<std::array<WireLabel,2>> masked(pairs.size());
    kdf_mask_labels(pairs, otBlocks, masked);
    // Send masked pairs sequentially over existing application socket
    for (size_t i=0;i<masked.size();++i) {
        SocketUtils::send_wire_label(connection.get_socket(), masked[i][0]);
        SocketUtils::send_wire_label(connection.get_socket(), masked[i][1]);
    }
    total_ots_performed += pairs.size();
    return true;
}

std::vector<WireLabel> OTHandler::receive_ot(const std::vector<bool>& choices, SocketConnection& connection) {
    if (!initialized || is_sender) throw OTException("OT receiver not properly initialized");
    if (choices.empty()) return {};
    auto ep = resolve_endpoint();
    std::vector<block> recvBlocks(choices.size());
    simplest_ot_receive(choices.size(), choices, recvBlocks, ep);
    // Receive masked pairs
    std::vector<std::array<WireLabel,2>> masked(choices.size());
    for (size_t i=0;i<choices.size();++i) {
        masked[i][0] = SocketUtils::receive_wire_label(connection.get_socket());
        masked[i][1] = SocketUtils::receive_wire_label(connection.get_socket());
    }
    std::vector<WireLabel> out; out.reserve(choices.size());
    derive_chosen_labels(masked, recvBlocks, choices, out);
    total_ots_performed += choices.size();
    return out;
}

void OTHandler::reset() {
    cleanup();
    initialized = false; is_sender = false; total_ots_performed = 0;
}

void OTHandler::cleanup() { prng.reset(); }

std::string OTHandler::resolve_endpoint() const {
    const char* env = std::getenv("GC_OT_ENDPOINT");
    if (env && std::strlen(env) > 0) return std::string(env);
    return "127.0.0.1:9100"; // default
}

void OTHandler::simplest_ot_send(size_t n, std::vector<std::array<block,2>>& outPairs, const std::string& endpoint) {
#ifdef COPROTO_ENABLE_BOOST
    SimplestOT sender;
    // listen
    auto sock = coproto::asioConnect(endpoint, true);
    coproto::sync_wait(sender.send(outPairs, *prng, sock));
    coproto::sync_wait(sock.flush());
#else
    throw OTException("SimplestOT requires coproto Boost build");
#endif
}

void OTHandler::simplest_ot_receive(size_t n, const std::vector<bool>& choices, std::vector<block>& outMsgs, const std::string& endpoint) {
#ifdef COPROTO_ENABLE_BOOST
    SimplestOT recv;
    auto sock = coproto::asioConnect(endpoint, false);
    BitVector choiceBits(n);
    for (size_t i=0;i<n;++i) choiceBits[i] = choices[i];
    coproto::sync_wait(recv.receive(choiceBits, outMsgs, *prng, sock));
    coproto::sync_wait(sock.flush());
#else
    throw OTException("SimplestOT requires coproto Boost build");
#endif
}

static void sha256_block_mask(const block& b, uint8_t tweak, size_t index, uint8_t which, uint8_t* out, size_t outLen){
    // Simple KDF = SHA256(b || tweak || index || which)
    uint8_t input[16 + 1 + 8 + 1];
    std::memcpy(input, &b, 16);
    input[16] = tweak;
    std::memcpy(input+17, &index, 8);
    input[25] = which;
    unsigned char hash[32];
    SHA256(input, sizeof(input), hash);
    std::memcpy(out, hash, outLen);
}

void OTHandler::kdf_mask_labels(const std::vector<std::pair<WireLabel,WireLabel>>& in,
                                const std::vector<std::array<block,2>>& otBlocks,
                                std::vector<std::array<WireLabel,2>>& masked) const {
    if (in.size() != otBlocks.size()) throw OTException("kdf size mismatch");
    if (masked.size() != in.size()) masked.resize(in.size());
    for (size_t i=0;i<in.size();++i){
        for (int bit=0; bit<2; ++bit){
            WireLabel mask{};
            sha256_block_mask(otBlocks[i][bit], 0xA5, i, (uint8_t)bit, mask.data(), mask.size());
            const WireLabel& src = (bit == 0 ? in[i].first : in[i].second);
            for (size_t j=0;j<mask.size();++j){
                masked[i][bit][j] = src[j] ^ mask[j];
            }
        }
    }
}

void OTHandler::derive_chosen_labels(const std::vector<std::array<WireLabel,2>>& masked,
                                     const std::vector<block>& recvBlocks,
                                     const std::vector<bool>& choices,
                                     std::vector<WireLabel>& out) const {
    if (masked.size() != recvBlocks.size()) throw OTException("derive size mismatch");
    out.resize(masked.size());
    for (size_t i=0;i<masked.size();++i){
        bool c = choices[i];
        WireLabel mask{};
        sha256_block_mask(recvBlocks[i], 0xA5, i, (uint8_t)c, mask.data(), mask.size());
        for(size_t j=0;j<mask.size();++j){
            out[i][j] = masked[i][c][j] ^ mask[j];
        }
    }
}