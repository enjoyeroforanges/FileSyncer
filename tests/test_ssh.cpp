#include "../include/ssh.h"
#include <fstream>
#include <stdio.h>
#include <vector>


int test_pubkeyauth(){
    int port = 2200;
    // NOTE: "myserver" should be whatever the identity is in the config file
    ssh_session sesh = ConnectToHost("myserver", &port, "root");
    if (verifyKnownHost(sesh) == SSH_OK){
        if (authenticatePublicKey(sesh) == SSH_OK){
            std::cout << "public key connection works" << std::endl;
            ssh_disconnect(sesh);
            ssh_free(sesh);
            return 0;
        }
    }
    else{
        std::cout << "could not establish connection" << std::endl;
    }
    ssh_disconnect(sesh);
    ssh_free(sesh);
    return -1;
}

int test_sftpSendFile(ssh_session sesh, std::string filename, std::string dest) {
    int rc;
    std::ifstream file(filename.c_str(), std::ios::binary);
    if (!file.is_open()) {
        fprintf(stderr, "failed to open %s\n", filename.c_str());
        return SSH_ERROR;
    }
    file.seekg(0, std::ios::end);
    size_t length= file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(length);
    file.read(buffer.data(), length);
    file.close();
    rc = sftpSendFile(sesh, buffer.data(), length, dest.c_str());
    if (rc != SSH_OK) {
        std::cout << "sftpSendFile failed" << std::endl;
    }
    return rc;
}

int main(int argc, char *argv[]){
    int port = 2200;
    int rc;
    std::cout << argv[1] << '\n';

    if (argc > 1 && std::string_view(argv[1]) == "filetransfer") {
        // only use when pubkeyauth works
        ssh_session sesh = ConnectToHost("myserver", &port, "root");
        rc = authenticatePublicKey(sesh);
        if (rc != SSH_OK) {
            fprintf(stderr, "failed to authenticate public key");
        }
        rc = test_sftpSendFile(sesh, "../tests/test_ssh_files/payload1", "payloads/payload1");
        ssh_disconnect(sesh);
        ssh_free(sesh);
        if (rc != SSH_OK) {
            fprintf(stderr, "sftpSendFile failed");
        }
        return rc;
    }
    ssh_session sesh = ConnectToHost("localhost", &port, "goy");
    if (verifyKnownHost(sesh) == 0){
        if (authenticatePassword(sesh, "goy") == 0){
            std::cout << "connection good" << std::endl;
        }
        else{
            fprintf(stderr, "password Auth failed");
        }
    }
    else{
        fprintf(stderr, "host verification failed%s", strerror(errno));
    }
    
    ssh_channel channel;

    channel = ssh_channel_new(sesh);
    if (channel == NULL) return SSH_ERROR;
    rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK){
        ssh_channel_free(channel);
        return rc;
    }
    rc = RunCommand("ls", sesh);
    if (rc != SSH_OK){
        std::cout << "error trying to exec command" << std::endl;
    }
    rc = test_sftpSendFile(sesh, "/test_ssh_files/payload1", "/payloads/payload1");
    ssh_disconnect(sesh);
    ssh_free(sesh);

    test_pubkeyauth();
    return 0;
}
