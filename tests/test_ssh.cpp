#include "../include/ssh.h"
#include <stdio.h>


int test_pubkeyauth(){
    int host = 2200;
    ssh_session sesh = ConnectToHost("localhost", &host, "goy");
    if (verifyKnownHost(sesh) == SSH_OK){
        if (authenticatePublicKey(sesh) == SSH_OK){
            std::cout << "public key connection works" << std::endl;
            ssh_disconnect(sesh);
            ssh_free(sesh);
            return 0;
        }
        else{
            std::cout << "public key authentication failed"<<std::endl;

        }

    }
    else{
        std::cout << "could not establish connection" << std::endl;
    }
    ssh_disconnect(sesh);
    ssh_free(sesh);
    return -1;
}

int create_file_with_stuff(){
    
}

int main(){
    int host = 2200;
    ssh_session sesh = ConnectToHost("localhost", &host, "goy");
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
    int rc;

    channel = ssh_channel_new(sesh);
    if (channel == NULL) return SSH_ERROR;
    rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK){
        ssh_channel_free(channel);
        return rc;
    }
    rc = ssh_channel_request_exec(channel, "mkdir something");
    ssh_channel_free(channel);
    if (rc != SSH_OK){
        std::cout << "error trying to exec command" << std::endl;
    }
    else{
        std::cout << "dir created" << std::endl;
    }
    rc = RunCommand("ls", sesh);
    if (rc != SSH_OK){
        std::cout << "error trying to exec command" << std::endl;
    }

    ssh_disconnect(sesh);
    ssh_free(sesh);

    test_pubkeyauth();
    return 0;
}
