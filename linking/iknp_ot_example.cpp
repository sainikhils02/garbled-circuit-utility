#include <iostream>
#include <vector>
#include <coproto/Socket/AsioSocket.h>
#include <cryptoTools/Common/BitVector.h>
#include <cryptoTools/Crypto/PRNG.h>
#include <libOTe/TwoChooseOne/Iknp/IknpOtExtReceiver.h>
#include <libOTe/TwoChooseOne/Iknp/IknpOtExtSender.h>
#include <libOTe/Base/BaseOT.h>

using namespace osuCrypto;

/**
 * Sender function for IKNP OT Extension protocol
 * In OT, the sender has two messages per OT instance and doesn't know which one the receiver gets
 */
void runSender(std::string ip, int port, int numOTs)
{
    std::cout << "Starting OT sender on " << ip << ":" << port << std::endl;
    
    // Establish network connection (sender connects first)
    auto socket = coproto::asioConnect(ip + ":" + std::to_string(port), true);
    
    // Initialize PRNG with fixed seed for reproducible results
    //PRNG prng(block(12345, 67890));
    PRNG prng(sysRandomSeed());
    
    // Prepare data structures for base OTs (128 base OTs required for IKNP)
    std::vector<block> baseRecv(128);        // Received messages from base OT
    BitVector baseChoice(128);               // Sender's choices in base OT
    baseChoice.randomize(prng);              // Randomize base OT choices
    
    // Prepare messages for extended OTs
    std::vector<std::array<block, 2>> sendMsg(numOTs);
    
    // Create IKNP OT Extension sender
    IknpOtExtSender sender;
    
    // Step 1: Perform base OTs (sender acts as RECEIVER in base OT phase)
    SimplestOT baseOT;
    auto baseTask = baseOT.receive(baseChoice, baseRecv, prng, socket);
    coproto::sync_wait(baseTask);
    
    // Step 2: Set the base OTs for the extension protocol
    sender.setBaseOts(baseRecv, baseChoice);
    
    // Step 3: Generate random messages for each OT instance
    for (int i = 0; i < numOTs; i++) {
        sendMsg[i][0] = prng.get<block>();   // Message for choice 0
        sendMsg[i][1] = prng.get<block>();   // Message for choice 1
    }
    
    // Step 4: Execute the OT extension protocol
    auto task = sender.send(sendMsg, prng, socket);
    coproto::sync_wait(task);
    
    // Display results for verification
    std::cout << "Sender OT messages (first 5):" << std::endl;
    for (int i = 0; i < std::min(5, numOTs); i++) {
        std::cout << "OT[" << i << "]: " << sendMsg[i][0] << ", " << sendMsg[i][1] << std::endl;
    }
    
    std::cout << "OT sender finished" << std::endl;
}

/**
 * Receiver function for IKNP OT Extension protocol
 * The receiver chooses which message to receive from each OT instance
 */
void runReceiver(std::string ip, int port, int numOTs)
{
    std::cout << "Starting OT receiver on " << ip << ":" << port << std::endl;
    
    // Establish network connection (receiver connects second)
    auto socket = coproto::asioConnect(ip + ":" + std::to_string(port), false);
    
    // Initialize PRNG with same seed as sender for proper synchronization
    //PRNG prng(block(12345, 67890));
    PRNG prng(sysRandomSeed()); 
    
    // CRITICAL: Synchronize PRNG state with sender
    // Sender uses PRNG first for baseChoice, so we must match that
    BitVector dummyChoice(128);
    dummyChoice.randomize(prng);             // Matches sender's baseChoice.randomize()
    
    // Prepare receiver data structures
    std::vector<block> recvMsg(numOTs);      // Messages received from OT
    std::vector<std::array<block, 2>> baseSend(128);  // Messages for base OT
    BitVector choices(numOTs);               // Receiver's choices for each OT
    choices.randomize(prng);                 // Generate random choices
    
    // Create IKNP OT Extension receiver
    IknpOtExtReceiver receiver;
    
    // Step 1: Perform base OTs (receiver acts as SENDER in base OT phase)
    SimplestOT baseOT;
    auto baseTask = baseOT.send(baseSend, prng, socket);
    coproto::sync_wait(baseTask);
    
    // Step 2: Set the base OTs for the extension protocol
    receiver.setBaseOts(baseSend);
    
    // Step 3: Execute the OT extension protocol to receive messages
    auto task = receiver.receive(choices, recvMsg, prng, socket);
    coproto::sync_wait(task);
    
    // Display results for verification
    std::cout << "Receiver choices and received messages (first 5):" << std::endl;
    for (int i = 0; i < std::min(5, numOTs); i++) {
        std::cout << "Choice[" << i << "]: " << (choices[i] ? "1" : "0") 
                  << ", Received: " << recvMsg[i] << std::endl;
    }
    
    std::cout << "OT receiver finished" << std::endl;
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " [sender|receiver] [ip:port] [numOTs]" << std::endl;
        return 1;
    }
    
    std::string role = argv[1];
    std::string connection = argv[2];
    
    // Parse IP and port
    size_t colonPos = connection.find(':');
    if (colonPos == std::string::npos) {
        std::cerr << "Invalid connection format. Use ip:port" << std::endl;
        return 1;
    }
    
    std::string ip = connection.substr(0, colonPos);
    int port = std::stoi(connection.substr(colonPos + 1));
    
    // Number of OTs to perform
    int numOTs = 128;
    if (argc > 3) {
        numOTs = std::stoi(argv[3]);
    }
    
    try {
        if (role == "sender") {
            runSender(ip, port, numOTs);
        } else if (role == "receiver") {
            runReceiver(ip, port, numOTs);
        } else {
            std::cerr << "Invalid role. Use 'sender' or 'receiver'" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}