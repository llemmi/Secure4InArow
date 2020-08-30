//
// Created by iacopo on 14/08/20.
//

#include "UserConnectionManager.h"

UserConnectionManager::UserConnectionManager(Server *srv, sockaddr_in client_addr, int clSocket) {

    //setting UserConnectionManager attributes
    server = srv;
    clAdd = client_addr;
    userSocket = clSocket;

    RAND_poll();

    diffieHellmannManager = new DiffieHellmannManager();
    cout<<"UserConnectionManager created successfully"<<endl;

}

void UserConnectionManager::openNewconnectionwithClient() {

    if(!this->establishSecureConnection()){
        server->removeUser(this->userName);
        delete this;
        return;
    }else
        cout<<"Secure connection established"<<std::endl;

    if(!sharePlayersList()){
        cout<<"The game cannot start. Failed the players credential sharing"<<endl;
        delete this;
        return;
    }

}


bool UserConnectionManager::establishSecureConnection() {


    //wait for hello message
    if(!waitForHelloMessage()){
        cerr<<"Error in receiving Hello Message"<<endl;
        delete this;
        return false;
    }


    //preparing certificate message
    size_t cert_msg_len;
    unsigned char *certificateMsg;
    certificateMsg = createCertificateMessage(cert_msg_len);

    //sending certificate message
    if(!sendCertificate(certificateMsg, cert_msg_len)){
        cerr<<"Error in sending certificate"<<endl;
        delete this;
        return false;
    }else{
        cout<<"Certificate sent successfully"<<endl;
    }

    if(!waitForClientPubKey()){
        cerr<<"Error in receiving client pubkey"<<endl;
        delete this;
        return false;
    }else{
        cout<<"PubKey received successfully"<<endl;
    }

    if(!sendMyPubKey()){
        cerr<<"Error in sending my pubkey"<<endl;
        delete this;
        return false;
    }else{
        cout<<"PubKey sent successfully"<<endl;
    }

    createSessionKey();
    server->insertUserConnectionInMap(*userName, this);


    return true;
}
bool UserConnectionManager::waitForHelloMessage(){

    auto *buffer = new unsigned char[HELLOMESSAGELENGTH];
    size_t ret;

    ret = recv(userSocket, buffer, HELLOMESSAGELENGTH, MSG_WAITALL);
    if(ret <= 0){
        cout<<"Error in receiving HelloMessage\n";
        return false;
    }

    cout<<"Dimensione HelloMessage: "<<ret<<endl;
    if(buffer[0] == HELLOMSGCODE) {
        cout << "HelloMessage opcode verified\n";
    }
    else{
        cerr<<"Wrong message!\n";
        delete []buffer;
        return false;
    }

    clientNonce = new unsigned char[NONCELENGTH];
    memcpy(clientNonce, &buffer[1], NONCELENGTH);

    buffer[ret-1] = '\0';
    this->userName = new string((const char*)&buffer[1 + NONCELENGTH]);

    cout<<"THE RECEIVED USERNAME IS: "<<userName->c_str()<<endl;
    delete []buffer;
    return true;

}
bool UserConnectionManager::sendCertificate(unsigned char* msg, size_t msg_len){
    cout<<"sendCertificate\n";

    size_t ret;
    ret = send(userSocket, msg, msg_len, 0);
    if(ret < msg_len){
        cout<<"Error in sending certificate\n";
        return false;
    }else
        return true;
}
unsigned char* UserConnectionManager::createCertificateMessage(size_t& msg_len){
    unsigned char *cert;
    int cert_len;
    int pos = 0;
    cert = server->geti2dCertificate(cert_len);

    //creo il buffer e copio opcode
    msg_len = (size_t)cert_len + NONCELENGTH + 1;
    unsigned char *buffer  = new unsigned char[msg_len];
    buffer[pos] = CERTIFICATEMSGCODE;
    pos++;

    //creo nonce e lo copio nel buffer
    myNonce = new unsigned char[NONCELENGTH];
    RAND_bytes(&myNonce[0], NONCELENGTH);
    memcpy((buffer+pos), myNonce, NONCELENGTH);
    pos += NONCELENGTH;

    //copio certificato nel buffer
    memcpy((buffer + pos), cert, (size_t)(cert_len));
    pos += (size_t)(cert_len);
    cout<<"Certificate message created succesfully "<<endl;

    return buffer;
}
bool UserConnectionManager::waitForClientPubKey() {

    auto* buffer = new unsigned char[MAXPUBKEYMESSAGELENGTH];
    size_t ret = recv(userSocket ,buffer, MAXPUBKEYMESSAGELENGTH, MSG_WAITALL);
    if(ret <= 0){
        cout<<"Error receiving the public key message"<<endl;
        delete [] buffer;
        return false;
    }

    size_t pos = 0;
    if(buffer[pos] == PUBKEYMESSAGECODE) {
        cout << "Pubkey message opcode verified" << endl;
        pos++;
    }
    else{
        cout<<"Wrong message. Expected pubkey message."<<endl;
        delete [] buffer;
        return false;
    }

    //copio client nonce
    auto* receivedClientNonce = new unsigned char[NONCELENGTH];
    memcpy(receivedClientNonce, (buffer+pos), NONCELENGTH);
    pos += NONCELENGTH;

    //copio il mio nonce
    auto* myReceivedNonce = new unsigned char[NONCELENGTH];
    memcpy(myReceivedNonce, (buffer+pos), NONCELENGTH);
    pos += NONCELENGTH;

    //copio lo username
    string *username= new string((const char*)&buffer[pos]);
    pos += username->length()+1;

    //prelevo la pubkey
    auto *clientPubKey = new unsigned char[PUBKEYLENGTH];
    memcpy(clientPubKey, (buffer+pos), PUBKEYLENGTH);
    pos += PUBKEYLENGTH;


    auto *signature = new unsigned char[SIGNATURELENGTH];
    auto *messageToVerify = new unsigned char[pos];
    memcpy(messageToVerify, buffer, pos);
    memcpy(signature, buffer+pos, ret-pos);

    //verifico la firma
    if(!signatureManager->verifyThisSignature(signature, ret-pos, messageToVerify, pos)) {
        cout<<"Signature not verified"<<endl;
        delete [] buffer;
        delete [] signature;
        delete [] messageToVerify;
        delete [] receivedClientNonce;
        delete [] myReceivedNonce;
        delete username;
        delete [] clientPubKey;
        return false;
    }

    delete [] signature;
    delete [] messageToVerify;



    //verifico client nonce
    if(!verifyNonce(clientNonce, receivedClientNonce)){
        cout<<"The client sent a different nonce"<<endl;
        delete [] buffer;
        delete [] receivedClientNonce;
        delete [] myReceivedNonce;
        delete username;
        return false;
    }

    delete [] receivedClientNonce;

    //verifico il mio nonce
    if(!verifyNonce(myNonce, myReceivedNonce)){
        cout<<"My nonce is not verified"<<endl;
        delete [] buffer;
        delete [] myReceivedNonce;
        delete username;
        return false;
    }

    delete [] myReceivedNonce;

    //verifico lo username
     if(strcmp(username->c_str(), this->userName->c_str()) != 0 || (username->length() != this->userName->length())){
            cout<<"The username is different from the one sent in the Hello Message."<<endl;
            delete [] buffer;
            delete username;
            return false;
     }

     delete username;


    //chiamo DH
    diffieHellmannManager->setPeerPubKey(clientPubKey, PUBKEYLENGTH);
    delete []clientPubKey;


    return true;


}
bool UserConnectionManager::verifyNonce(unsigned char* knownNonce, unsigned char* receivedNonce){
    //comparison between the two nonces

    if(memcmp(knownNonce, receivedNonce, NONCELENGTH) == 0) {
        cout<<"Nonce verified"<<endl;
        return true;
    }
    else {
        cout<<"Nonce has not been verified"<<endl;
        return false;
    }
}
bool UserConnectionManager::sendMyPubKey() {
    auto *buffer = new unsigned char[MAXPUBKEYMESSAGELENGTH];

    //inserisco opcode
    size_t pos = 0;
    buffer[pos] = PUBKEYMESSAGECODE;
    pos++;

    //inserisco client nonce
    memcpy((buffer+pos), clientNonce, NONCELENGTH);
    pos += NONCELENGTH;

    //inserisco myNonce
    memcpy((buffer+pos), myNonce, NONCELENGTH);
    pos += NONCELENGTH;

    //inserisco '\0' al posto dello username
    buffer[pos] = '\0';
    pos++;

    //inserisco la chiave
    size_t myKey_len = PUBKEYLENGTH;
    unsigned char* myKey = diffieHellmannManager->getMyPubKey(myKey_len);
    memcpy((buffer+pos), myKey, myKey_len);
    pos += myKey_len;

    delete [] myKey;

    //firmo il messaggio
    size_t signature_len = pos;
    unsigned char* signedMessage = signatureManager->signTHisMessage(buffer, signature_len);

    //copio la firma nel buffer
    memcpy((buffer+pos), signedMessage, signature_len);
    pos += signature_len;

    //invio
    size_t ret = send(userSocket, buffer, pos, 0);
    if(ret != pos){
        cout<<"Error in sending my pubkey"<<endl;
        delete [] buffer;
        return false;
    }

    delete [] buffer;
    return true;


}
void UserConnectionManager::createSessionKey() {

    size_t sharedSecret_len;
    unsigned char*sharedSecret = diffieHellmannManager->getSharedSecret(sharedSecret_len);
    auto* usefulSecret = new unsigned char[USEFULSECRETLENGTH];

    //copio i 16 byte meno significati
    memcpy(usefulSecret, sharedSecret, USEFULSECRETLENGTH);
    delete [] sharedSecret;

    unsigned char*digest;
    unsigned int digest_len;

    EVP_MD_CTX *Hctx;
    Hctx = EVP_MD_CTX_new();

    digest = (unsigned char*)malloc(EVP_MD_size(EVP_sha256()));

    EVP_DigestInit(Hctx, EVP_sha256());
    EVP_DigestUpdate(Hctx, (unsigned char*)sharedSecret, USEFULSECRETLENGTH);
    EVP_DigestFinal(Hctx, digest, &digest_len);

    EVP_MD_CTX_free(Hctx);

    delete [] usefulSecret;

    symmetricEncryptionManager = new SymmetricEncryptionManager(digest, digest_len);
    delete [] digest;
}
bool UserConnectionManager::sharePlayersList() {
    bool waiting = true;
    while(waiting) {
        if (!waitForPlayersRequest()) {
            cout << "Error in players list request" << endl;
            return false;
        }

        if (!sendPlayerList()) {
            cout << "Error in sending players list" << endl;
            return false;
        }

        string *choice = waitForClientChoice(waiting);
        if (choice == nullptr && !waiting) {
            cout << "Error in receiving player choice" << endl;
            return false;
        }else{
            if(!waiting){
                if(!sendChallengerRequest(choice)) {
                    delete choice;
                    return false;
                }
            }else{
                bool stillWait;
                string *opponent = waitForChallengedResponse(stillWait);
                if(opponent == nullptr) {
                    return false;
                }else{
                    if(stillWait)
                        waiting = true;
                    else{
                        //invio la chiave di opponent al challenged
                        waiting = false;
                    }
                }

            }
        }
    }

    return true;
}
bool UserConnectionManager::waitForPlayersRequest() {

    auto* buffer = new unsigned char[MAXPLAYERSREQUESTMESSAGELENGTH];
    size_t ret = recv(userSocket, buffer, MAXPLAYERSREQUESTMESSAGELENGTH, MSG_WAITALL);
    if(ret < 0){
        cout<<"Error receiving Players Request Message"<<endl;
        delete []buffer;
        return false;
    }

    if(buffer[0] == LISTREQUESTMESSAGE){
        cout<<"Players list request message verified"<<endl;
    }else{
        cout<<"Wrong message"<<endl;
        delete [] buffer;
        return false;
    }

    size_t pos = 1;

    //prelevo IV
    auto *iv = new unsigned char[AESGCMIVLENGTH];
    memcpy(iv, &buffer[pos], AESGCMIVLENGTH);
    pos += AESGCMIVLENGTH;

    //prelevo counter
    uint32_t cont;
    memcpy(&cont, &buffer[pos], COUNTERLENGTH);
    this->counter = cont;
    pos+= COUNTERLENGTH;

    //copio AAD
    size_t aad_len = OPCODELENGTH+AESGCMIVLENGTH+COUNTERLENGTH;
    auto* AAD = new unsigned char[aad_len];
    memcpy(AAD, buffer, aad_len);



    //copio dati criptati
    size_t encrypted_len = ret - AESGCMTAGLENGTH;
    auto* encryptedData = new unsigned char[encrypted_len];
    memcpy(encryptedData, buffer+pos, encrypted_len);
    pos += encrypted_len;

    //copio il tag
    size_t tag_len = AESGCMTAGLENGTH;
    auto *tag = new unsigned char[tag_len];
    memcpy(tag, buffer+pos, tag_len);
    pos += tag_len;

    delete [] buffer;

    unsigned char* plaintext = symmetricEncryptionManager->decryptThisMessage(encryptedData, encrypted_len, AAD, aad_len, tag, iv);

    delete [] AAD;
    delete [] iv;
    delete [] encryptedData;
    delete [] tag;

    if(strcmp((const char*)plaintext, userName->c_str())!= 0){
        cout<<"Error! Unexpected username"<<endl;
        return false;
    }
    return true;

}
bool UserConnectionManager::sendPlayerList() {

    vector<string> list;

    list = server->getUserList(userName);
/*
    if(list.size() > 0) {
        for(auto& v: list)
            cout<<v.c_str()<<" ";
        cout<<endl;
    }
*/
   size_t msg_len;
   unsigned char *buffer = createPlayerListMsg(list, msg_len);

   size_t ret = send(userSocket, buffer, msg_len, 0);
   if (ret < 0) {
       cerr << "Error in sending players list\n";
       return false;
   }
   if(list.size() == 0){
        cout<<"No player available for the user\n";
    }

   cout << "Players list has been sent\n";
   return true;

}
unsigned char* UserConnectionManager::createPlayerListMsg(vector<string> list, size_t& msg_len) {


    //prendo la lista dei giocatori e quanti e li metto nel buffer del messaggio in chiaro
    unsigned char* playerList = (unsigned char*)malloc(MAXUSERNAMELENGTH * list.size());

    size_t pos = 0;
    for(auto i = list.begin(); i != list.end(); i++){
        memcpy(playerList + pos, i->c_str(), strlen(i->c_str()) + 1);
        pos += strlen(i->c_str())+1;
    }

    cout<<"Users in buffer: ";
    for(int j = 0; j < pos; j++) {
        if(playerList[j] == '\0')
            cout<<" ";
        cout << playerList[j];
    }

    cout<<endl;


    auto* plainMessage = new unsigned char[pos + SIZETLENGTH];
    size_t num_players = list.size();
    cout<<"NUMERO GIOCATORI "<<num_players<<endl;
    num_players = htons(num_players);
    memcpy(plainMessage, &(num_players), SIZETLENGTH);
    memcpy(plainMessage + SIZETLENGTH, playerList, pos);

    delete [] playerList;
    size_t plainMsg_len = pos + SIZETLENGTH;

    //preparo AAD

    unsigned char *AAD = new unsigned char[OPCODELENGTH+AESGCMIVLENGTH+COUNTERLENGTH];
    AAD[0] = PLAYERSLISTMESSAGECODE;

    size_t iv_len = AESGCMIVLENGTH;
    auto *iv = new unsigned char[iv_len];
    RAND_bytes(iv, iv_len);

    counter++;
    memcpy(&AAD[1], iv, iv_len);
    memcpy(&AAD[1+iv_len], &counter, COUNTERLENGTH);
    size_t aad_len = 1 + AESGCMIVLENGTH + COUNTERLENGTH;

    auto *tag = new unsigned char[AESGCMTAGLENGTH];

    unsigned char *encryptedMessage = symmetricEncryptionManager->encryptThisMessage(plainMessage, plainMsg_len, AAD, aad_len, iv, iv_len, tag);

    delete [] AAD;

    auto *buffer = new unsigned char[aad_len+plainMsg_len+AESGCMTAGLENGTH];

    //copio opcode
    pos = 0;
    buffer[0] = PLAYERSLISTMESSAGECODE;
    pos++;

    //copio iv
    memcpy(buffer+pos, iv, iv_len);
    pos += iv_len;
    delete [] iv;

    //copio counter
    memcpy((buffer+pos), &this->counter, COUNTERLENGTH);
    pos += COUNTERLENGTH;

    //copio encrypted
    memcpy((buffer+pos), encryptedMessage, plainMsg_len);
    pos += plainMsg_len;
    delete[] encryptedMessage;

    //copio tag
    memcpy((buffer+pos), tag, AESGCMTAGLENGTH);
    pos += AESGCMTAGLENGTH;
    delete [] tag;

    msg_len = pos;

    pos = iv_len = plainMsg_len = aad_len = num_players = 0;
    return buffer;




}
string *UserConnectionManager::waitForClientChoice(bool& waiting) {


    auto* buffer = new unsigned char[MAXPLAYERSREQUESTMESSAGELENGTH];
    size_t ret = recv(userSocket, buffer, MAXPLAYERSREQUESTMESSAGELENGTH, MSG_WAITALL);
    if(ret < 0){
        cout<<"Error receiving Players Request Message"<<endl;
        delete []buffer;
        waiting = false;
        return nullptr;
    }

    if(buffer[0] == PLAYERCHOSENMESSAGECODE){
        cout<<"Players list request message verified"<<endl;
    }else{
        cout<<"Wrong message"<<endl;
        delete [] buffer;
        waiting = false;
        return nullptr;
    }

    size_t pos = 1;

    //prelevo IV
    auto *iv = new unsigned char[AESGCMIVLENGTH];
    memcpy(iv, &buffer[pos], AESGCMIVLENGTH);
    pos += AESGCMIVLENGTH;

    //prelevo counter
    int cont;
    memcpy(&cont, &buffer[pos], COUNTERLENGTH);
    if(this->counter+1 != cont){
        cout<<"The counter has a wrong value"<<endl;
        delete [] iv;
        delete [] buffer;
        waiting = false;
        return nullptr;
    }
    counter++;
    pos+= COUNTERLENGTH;

    //copio AAD
    size_t aad_len = OPCODELENGTH+AESGCMIVLENGTH+COUNTERLENGTH;
    auto* AAD = new unsigned char[aad_len];
    memcpy(AAD, buffer, aad_len);



    //copio dati criptati
    size_t encrypted_len = ret - AESGCMTAGLENGTH;
    auto* encryptedData = new unsigned char[encrypted_len];
    memcpy(encryptedData, buffer+pos, encrypted_len);
    pos += encrypted_len;

    //copio il tag
    size_t tag_len = AESGCMTAGLENGTH;
    auto *tag = new unsigned char[tag_len];
    memcpy(tag, buffer+pos, tag_len);
    pos += tag_len;

    delete [] buffer;

    unsigned char* plaintext = symmetricEncryptionManager->decryptThisMessage(encryptedData, encrypted_len, AAD, aad_len, tag, iv);

    delete [] AAD;
    delete [] iv;
    delete [] encryptedData;
    delete [] tag;


    string *player;
    plaintext[encrypted_len-1] = '\0';
    player = new string((const char*)&plaintext[1]);
    cout<<"The chosen player is: "<<player->c_str()<<endl;

    if(player->length() < 1){
        waiting = true;
    }else
        waiting = false;
    return player;


}
/*

EVP_PKEY *UserConnectionManager::getUserPubKey(string* opponent) {

    return server->getUserConnection(opponent->c_str())->signatureManager->
}

*/
bool UserConnectionManager::sendChallengerRequest(string *challenged) {

    size_t plain_len = userName->length()+1;
    unsigned char* plainMsg = new unsigned char[plain_len];
    memcpy(plainMsg, userName->c_str(), plain_len);

    UserConnectionManager * challengedUCM = server->getUserConnection(challenged->c_str());
    //AAD
    size_t aad_len = OPCODELENGTH + AESGCMIVLENGTH + COUNTERLENGTH;
    auto *AAD = new unsigned char[aad_len];

    //copio opcode
    AAD[0] = PLAYERCHOSENMESSAGECODE;

    //copio iv
    size_t iv_len = AESGCMIVLENGTH;
    auto *iv = new unsigned char[iv_len];
    RAND_bytes(iv,iv_len);
    memcpy(AAD+1, iv, iv_len);

    //copio counter
    challengedUCM->counter++;
    memcpy(AAD+1+iv_len, &challengedUCM->counter, COUNTERLENGTH);

    auto*tag = new unsigned char[AESGCMTAGLENGTH];
    unsigned char* encrypted = challengedUCM->symmetricEncryptionManager->encryptThisMessage(plainMsg, plain_len, AAD, aad_len, iv, iv_len, tag);
    delete [] plainMsg;
    delete [] iv;

    if(plain_len < AESBLOCKLENGTH){
        cout<<"Error in encryting the username"<<endl;
        delete [] encrypted;
        delete [] tag;
        delete [] AAD;
        return false;
    }

    size_t message_len = aad_len + plain_len + AESGCMTAGLENGTH;
    size_t pos = 0;

    //copio aad
    unsigned char *buffer = new unsigned char[message_len];
    memcpy(buffer, AAD, aad_len);
    delete [] AAD;
    pos += aad_len;

    //copio encrypted
    memcpy(buffer+pos, encrypted, plain_len);
    delete [] encrypted;
    pos += plain_len;

    memcpy(buffer+pos, tag, AESGCMTAGLENGTH);
    delete [] tag;

    int challengedSocket = challengedUCM->userSocket;
    size_t ret = send(challengedSocket, buffer, message_len, 0);

    if(ret < message_len){
        cerr<<"Error during sending challenge message to the challenged player\n";
        return false;
    }

    message_len = iv_len = plain_len = aad_len = pos = 0;
    cout<<"Challenger request message has been sent\n";
    return true;
}

string* UserConnectionManager::waitForChallengedResponse(bool& stillWaiting) {

    auto *buffer = new unsigned char[MAXCHALLENGEDRESPONSEMESSAGELENGTH];
    size_t ret = recv(userSocket, buffer, MAXCHALLENGEDRESPONSEMESSAGELENGTH, MSG_WAITALL);

    if(ret < OPCODELENGTH + AESGCMIVLENGTH + COUNTERLENGTH + AESBLOCKLENGTH + AESGCMTAGLENGTH){
        cerr<<"Error receiving the challenge response message\n";
        stillWaiting = false;
        return NULL;
    }

    //copio aad
    size_t pos = 0;
    size_t aad_len = OPCODELENGTH + AESGCMIVLENGTH + COUNTERLENGTH;
    auto *aad = new unsigned char[aad_len];
    memcpy(aad, buffer, aad_len);
    pos += aad_len;

    //controllo counter
    uint32_t cont;
    memcpy(&cont, aad + OPCODELENGTH + AESGCMIVLENGTH, COUNTERLENGTH);
    if(cont != this->counter+1){
        cout<<"The counter value was not the expencted one"<<endl;
        delete [] buffer;
        delete [] aad;
        stillWaiting = false;
        return nullptr;
    }

    this->counter++;

    //controllo opcode
    if(aad[0] != CHALLENGEDRESPONSEMESSAGECODE){
        cout<<"Wrong message"<<endl;
        stillWaiting = false;
        delete [] aad;
        delete [] buffer;
        return nullptr;
    }

    size_t iv_len = AESGCMIVLENGTH;
    auto *iv = new unsigned char[iv_len];
    memcpy(iv, aad+1, iv_len);

    size_t encrypted_len = ret - AESGCMTAGLENGTH - pos;
    auto* encrypted = new unsigned char[encrypted_len];
    memcpy(encrypted, buffer+pos, encrypted_len);
    pos += encrypted_len;

    auto *tag = new unsigned char[AESGCMTAGLENGTH];
    memcpy(tag, buffer+pos, AESGCMTAGLENGTH);

    delete [] buffer;
    unsigned char*plainMessage = symmetricEncryptionManager->decryptThisMessage(encrypted, encrypted_len, aad, aad_len, tag, iv);

    delete [] aad;
    delete [] encrypted;
    delete [] iv;
    delete [] tag;

    char response;
    string *opponent = new string();
    opponent->append((const char *)(plainMessage+1));
    response= plainMessage[0];
    cout<<"THE OPPONENT IS = "<<opponent->c_str()<<endl;

    delete[] plainMessage;
    encrypted_len = iv_len = aad_len = pos = cont = 0;

    if(response == 'Y'){
        cout<<"The player accepted the challenge\n";
        stillWaiting = false;
        return opponent;
    }else {
        cout<<"The player refused the challenge\n";
        stillWaiting = true;
        return opponent;
    }
}
/*
bool UserConnectionManager::sendOpponentKey(string *opponent) {

    EVP_PKEY *opponentPubKey = getUserPubKey(opponent);

    struct in_addr ipOpponent = server->getUserConnection(opponent->c_str())->clAdd.sin_addr;
    size_t opponentPort = server->getUserConnection(opponent->c_str())->clAdd.sin_port;

    BIO *mbio = BIO_new(BIO_s_mem());
    PEM_write_bio_PUBKEY(mbio, opponentPubKey);
    unsigned char* pubkey_buf;
    long pubkey_size = BIO_get_mem_data(mbio, &pubkey_buf);

    auto *plainMsg = new unsigned char[pubkey_size + IPLENGTH + SIZETLENGTH + 1];
    plainMsg[0] = OPPONENTKEYCODE;

    size_t htons_port = htons(opponentPort);

    int pos = 1;
    memcpy(plainMsg+pos, (void*)&ipOpponent, IPLENGTH);
    pos += IPLENGTH;

    memcpy(plainMsg+pos, &htons_port, SIZETLENGTH);
    pos += SIZETLENGTH;

    memcpy(plainMsg + pos, pubkey_buf, pubkey_size);
    pos+= pubkey_size;

    size_t toEncrypt_len = pos;
    unsigned char* buffer = symmetricEncryptionManager->encryptNMACThisMessage(plainMsg, toEncrypt_len);

    delete []plainMsg;
    BIO_free(mbio);

    size_t ret = send(userSocket, buffer, toEncrypt_len, 0);
    if(ret != toEncrypt_len){
        cerr<<"Error sending opponent key\n";
        return false;
    }
    cout<<"Adversary key and address sent correctly\n";
    return true;
}

bool UserConnectionManager::waitForOpponentReady(unsigned int& port) {

    auto *buffer = new unsigned char[MAXENCRYPTEDUSERLENGTH+HMACLENGTH];
    size_t ret = recv(userSocket, buffer, MAXENCRYPTEDUSERLENGTH+HMACLENGTH, 0);

    if(ret < AESBLOCKLENGTH+HMACLENGTH){
        cerr<<"Error in receving waitForOpponentReady message,received "<<ret<<" bytes"<<endl;
        return false;
    }

    unsigned char *plainMsg = symmetricEncryptionManager->decryptNVerifyMACThisMessage(buffer, ret);

    size_t plain_len = ret;

    if(plainMsg[0] == CLIENTREADY4CHALLENGECODE){
        cout<<"waitForOpponentReady message opcode has been verified\n";
    }else{
        cerr<<"Wrong message: waitForOpponentReady message was expected\n";
        return false;
    }

    unsigned int port_received;
    size_t port_len = sizeof(port);
    memcpy(&port_received, plainMsg+1, port_len);

    port = port_received;

    string *user = new string(reinterpret_cast<const char *>(&plainMsg[port_len+1]));

    return true;
}

bool UserConnectionManager::sendMyKeyToChallenger(string *challenger, int port) {

    EVP_PKEY *myClientPubKey = getUserPubKey(userName);

    struct in_addr myClientIP = this->clAdd.sin_addr;
    //uint32_t myClientIP = this->clAdd.sin_addr.s_addr;
    size_t myClientPort = this->clAdd.sin_port;


    char *IPbuffer = inet_ntoa(this->clAdd.sin_addr);
    printf("Host IP: %s", IPbuffer);

    BIO *mbio = BIO_new(BIO_s_mem());
    PEM_write_bio_PUBKEY(mbio, myClientPubKey);
    unsigned char* pubkey_buf;
    long pubkey_size = BIO_get_mem_data(mbio, &pubkey_buf);

    auto *plainMsg = new unsigned char[pubkey_size + IPLENGTH + SIZETLENGTH + 1];
    plainMsg[0] = OPPONENTKEYCODE;

    size_t htons_port = port;

    int pos = 1;
    memcpy(plainMsg+pos, &myClientIP, IPLENGTH);
    pos += IPLENGTH;

    memcpy(plainMsg+pos, &htons_port, SIZETLENGTH);
    pos += SIZETLENGTH;

    memcpy(plainMsg + pos, pubkey_buf, pubkey_size);
    pos+= pubkey_size;

    size_t toEncrypt_len = pos;

    UserConnectionManager *challengerUCM = this->server->getUserConnection(challenger->c_str());
    unsigned char* buffer = challengerUCM->symmetricEncryptionManager->encryptNMACThisMessage(plainMsg, toEncrypt_len);

    delete []plainMsg;
    BIO_free(mbio);

    size_t ret = send(challengerUCM->userSocket, buffer, toEncrypt_len, 0);
    if(ret != toEncrypt_len){
        cerr<<"Error sending opponent key\n";
        return false;
    }

    return true;
}

*/

UserConnectionManager::~UserConnectionManager() {

    close(userSocket);
    delete [] server;
    delete userName;
    delete [] symmetricEncryptionManager;
    delete [] signatureManager;
    delete diffieHellmannManager;
    delete [] clientNonce;
    delete [] myNonce;
}