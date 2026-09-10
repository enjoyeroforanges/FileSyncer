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



inline int verifyKnownHost(ssh_session sesh) {}

inline int authenticatePassword(ssh_session sesh, const char* username){}

inline int authenticatePublicKey(ssh_session sesh){}

inline ssh_session ConnectToHost(const char* host, const int* port, const char* username){}

inline int RunCommand(const char* command, ssh_session sesh){}
