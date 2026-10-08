#pragma once
#include <libssh/libssh.h>
#include <string>
#include <iostream>
#include "protocol.h"

#if defined(_WIN32)
#include <windows.h>
#undef FILE_CREATE
#undef FILE_MODIFY
#undef FILE_DELETE
#else
#include <termios.h>
#include <unistd.h>
#endif

class ChannelReader {
    private:
        ssh_channel chan;
        std::vector<char> raw;
        std::vector<char> body;
        std::vector<char> path;
        uint64_t mtime;
        Opcode opcode;
    public:
        explicit ChannelReader(ssh_channel &chan);
        int readChannelMessage();
        Map parseRawHashes();
};

int verifyKnownHost(ssh_session sesh);

int authenticatePassword(ssh_session sesh, const char* username);

int authenticatePublicKey(ssh_session sesh);

ssh_session ConnectToHost(const char* host, const int* port, const char* username);

int RunCommand(const char* command, ssh_session sesh);

int sftpSendFile(ssh_session sesh, const char* contents, const size_t length, const char* destination);

int sftpSendFile(ssh_session sesh, const char* path, const char* destination);
