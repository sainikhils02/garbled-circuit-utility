#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <stdexcept>

#include <coproto/Socket/AsioSocket.h>
#include <cryptoTools/Common/BitVector.h>
#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Common/Timer.h>
#include <libOTe/Base/SimplestOT.h>

using namespace osuCrypto;

/*
 * A minimal standalone example demonstrating a raw (base) 1-out-of-2 Oblivious
 * Transfer using the SimplestOT protocol. This is the "base OT" primitive (no
 * extension). It performs 'n' independent OTs where the sender provides pairs
 * of random 128-bit blocks and the receiver obtains exactly one block from
 * each pair according to its private choice bit.
 *
 * Usage (two terminals):
 *   Terminal A (acts as sender/server listening first):
 *     ./simplest_ot_example sender 127.0.0.1:9000 128
 *
 *   Terminal B (acts as receiver/client):
 *     ./simplest_ot_example receiver 127.0.0.1:9000 128
 *
 * Notes:
 * - The side that passes 'sender' will create the listening socket (second
 *   argument to asioConnect = true). The receiver connects actively.
 * - Requires coproto built with Boost (COPROTO_ENABLE_BOOST=ON) for Asio.
 * - Randomness is seeded from sysRandomSeed() for security. Do NOT replace
 *   with a fixed constant in production.
 */

void runSender(const std::string& endpoint, int numOTs)
{
    std::cout << "[Sender] Listening on " << endpoint << " for base OTs (SimplestOT)\n";

    // Create a socket that listens (server side = true)
    auto sock = coproto::asioConnect(endpoint, true);

    PRNG prng(sysRandomSeed());

    // Allocate sender messages: one pair per OT.
    std::vector<std::array<block,2>> sendMsgs(numOTs);

    // Instantiate the SimplestOT protocol for the sender role.
    SimplestOT sender;

    // Execute the send part. This fills 'sendMsgs' with the two messages for each OT.
    coproto::sync_wait(sender.send(sendMsgs, prng, sock));

    // Ensure all buffered data is flushed out over the socket.
    coproto::sync_wait(sock.flush());

    std::cout << "[Sender] Completed " << numOTs << " base OTs. Showing first up to 5 pairs:" << std::endl;
    for (int i = 0; i < std::min(numOTs, 5); ++i)
    {
        std::cout << "  OT[" << i << "] = (" << sendMsgs[i][0] << ", " << sendMsgs[i][1] << ")\n";
    }
    std::cout << "[Sender] Done." << std::endl;
}

void runReceiver(const std::string& endpoint, int numOTs)
{
    std::cout << "[Receiver] Connecting to " << endpoint << " for base OTs (SimplestOT)\n";

    // Client side (server flag = false): actively connects.
    auto sock = coproto::asioConnect(endpoint, false);

    PRNG prng(sysRandomSeed());

    // Allocate receiver output: one chosen block per OT.
    std::vector<block> recvMsgs(numOTs);
    BitVector choices(numOTs);
    choices.randomize(prng); // private choices.

    SimplestOT receiver;

    // Execute the receive part.
    coproto::sync_wait(receiver.receive(choices, recvMsgs, prng, sock));

    coproto::sync_wait(sock.flush());

    std::cout << "[Receiver] Completed " << numOTs << " base OTs. Showing first up to 5 results:" << std::endl;
    for (int i = 0; i < std::min(numOTs, 5); ++i)
    {
        std::cout << "  OT[" << i << "] choice=" << int(choices[i]) << " value=" << recvMsgs[i] << "\n";
    }
    std::cout << "[Receiver] Done." << std::endl;
}

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        std::cerr << "Usage: " << argv[0] << " <sender|receiver> <ip:port> <numOTs>" << std::endl;
        return 1;
    }

    std::string role = argv[1];
    std::string endpoint = argv[2];
    int numOTs = std::stoi(argv[3]);
    if (numOTs <= 0) numOTs = 128;

    try
    {
        if (role == "sender")
        {
            runSender(endpoint, numOTs);
        }
        else if (role == "receiver")
        {
            runReceiver(endpoint, numOTs);
        }
        else
        {
            std::cerr << "First argument must be 'sender' or 'receiver'." << std::endl;
            return 1;
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "Unknown exception." << std::endl;
        return 1;
    }

    return 0;
}
