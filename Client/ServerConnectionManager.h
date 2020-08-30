//
// Created by iacopo on 14/08/20.
//
#include "../Libraries/SymmetricEncryptionManager.h"
#include "../Libraries/DiffieHellmannManager.h"
#include "CertificateManager.h"
#include "../Libraries/SignatureManager.h"
#include <string>
#include <vector>
#include <thread>
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509_vfy.h>
#include <openssl/err.h> // for error descriptions
#include <stdexcept>
#include <cstdio>
/*
#ifndef PROGETTO_SERVERCONNECTIONMANAGER_H
#define PROGETTO_SERVERCONNECTIONMANAGER_H
*/
using namespace std;
class Client;

class ServerConnectionManager {

private:
    string* userName;
    string* prvKeyFilePasswd;
    int serverSocket;
    int serverNonce;
    int myNonce;
    int P2Pport;
    unsigned int counter;
    struct sockaddr_in serverAddr;
    SymmetricEncryptionManager* symmetricEncryptionManager;
    SignatureManager* signatureManager;
    CertificateManager* certificateManager;
    DiffieHellmannManager* diffieHellmannManager;

    bool sendHelloMessage(unsigned char*, size_t&);
    bool verifyCertificate(unsigned char*, int);
    bool sendMyPubKey();
    bool waitForPeerPubkey();
    bool sendPlayersListRequest();
    bool waitForPlayers(std::string*&);
    bool sendSelectedPlayer();
    bool waitForSomething();
    unsigned char* waitForOpponentKey(struct in_addr & ipOpponent, size_t& port);

    string* waitForPlayers(bool&);
    string* selectPlayer(vector<string>);
    string* waitForChallenge();

    unsigned char* createHelloMessage(size_t& helloMessageBufferLen);
    unsigned char* createPubKeyMessage(size_t&);
    unsigned char* waitForCertificate(int&);
    unsigned char* createPlayersListRequestMessage();
    unsigned char* createSelectedPlayerMessage();

public:



    ServerConnectionManager(const char* server_addr, int port, string* user);
    bool sendServerNonce(const char*, size_t);
    bool establishConnectionToServer();
    bool secureTheConnection();

    ~ServerConnectionManager();
    bool createConnectionWithServer();

};


//#endif PROGETTO_SERVERCONNECTIONMANAGER_H