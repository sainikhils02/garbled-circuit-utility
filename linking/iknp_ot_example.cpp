#include <iostream>
#include <vector>
#include <coproto/Socket/AsioSocket.h>
#include <cryptoTools/Common/BitVector.h>
#include <cryptoTools/Common/Timer.h>
#include <cryptoTools/Crypto/PRNG.h>
#include <libOTe/TwoChooseOne/Iknp/IknpOtExtReceiver.h>
#include <libOTe/TwoChooseOne/Iknp/IknpOtExtSender.h>
#include <libOTe/Base/BaseOT.h>

using namespace osuCrypto;

void runSender(std::string ip, int port, int numOTs)
{
    std::cout << "Starting OT sender on " << ip << ":" << port << std::endl;
    
    auto socket = coproto::asioConnect(ip + ":" + std::to_string(port), true);
    
    PRNG prng(block(12345, 67890)); // Use same seed for both parties
    
    // Prepare data structures
    std::vector<block> baseRecv(128);
    std::vector<std::array<block, 2>> sendMsg(numOTs);
    BitVector baseChoice(128);
    baseChoice.randomize(prng); // Randomize the base choices
    
    // Create the OT sender
    IknpOtExtSender sender;
    
    // Perform base OTs - sender acts as RECEIVER in base OT
    SimplestOT baseOT;
    auto baseTask = baseOT.receive(baseChoice, baseRecv, prng, socket);
    coproto::sync_wait(baseTask);
    
    // Set the base OTs
    sender.setBaseOts(baseRecv, baseChoice);
    
    // Generate random messages for the OTs
    for (int i = 0; i < numOTs; i++) {
        sendMsg[i][0] = prng.get<block>();
        sendMsg[i][1] = prng.get<block>();
    }
    
    // Send the actual OTs
    auto task = sender.send(sendMsg, prng, socket);
    coproto::sync_wait(task);
    
    // Display the first few OT messages
    std::cout << "Sender OT messages (first 5):" << std::endl;
    for (int i = 0; i < std::min(5, numOTs); i++) {
        std::cout << "OT[" << i << "]: " << sendMsg[i][0] << ", " << sendMsg[i][1] << std::endl;
    }
    
    std::cout << "OT sender finished" << std::endl;
}

void runReceiver(std::string ip, int port, int numOTs)
{
    std::cout << "Starting OT receiver on " << ip << ":" << port << std::endl;
    
    auto socket = coproto::asioConnect(ip + ":" + std::to_string(port), false);
    
    PRNG prng(block(12345, 67890)); // Use same seed for both parties
    
    // FIRST use the PRNG exactly as the sender does
    // This dummy choice simulates the sender's baseChoice randomization
    BitVector dummyChoice(128);
    dummyChoice.randomize(prng);
    
    // THEN prepare receiver data structures
    std::vector<block> recvMsg(numOTs);
    std::vector<std::array<block, 2>> baseSend(128);
    BitVector choices(numOTs);
    choices.randomize(prng);
    
    // Create the OT receiver
    IknpOtExtReceiver receiver;
    
    // Perform base OTs - receiver acts as SENDER in base OT
    SimplestOT baseOT;
    auto baseTask = baseOT.send(baseSend, prng, socket);
    coproto::sync_wait(baseTask);
    
    // Set the base OTs
    receiver.setBaseOts(baseSend);
    
    // Receive the actual OTs
    auto task = receiver.receive(choices, recvMsg, prng, socket);
    coproto::sync_wait(task);
    
    // Display the first few received OTs and choices
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