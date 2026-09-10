#pragma once
#include <libssh/libssh.h>
#include <string>
#include <iostream>
#include <strings.h>


#if defined(_WIN32)
#include <windows.h>
#undef FILE_CREATE
#undef FILE_MODIFY
#undef FILE_DELETE
#else
#include <termios.h>
#include <unistd.h>
#endif



int verifyKnownHost(ssh_session sesh);

int authenticatePassword(ssh_session sesh, const char* username);

int authenticatePublicKey(ssh_session sesh);

ssh_session ConnectToHost(const char* host, const int* port, const char* username);

int RunCommand(const char* command, ssh_session sesh);
