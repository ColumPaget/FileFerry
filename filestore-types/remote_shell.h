#ifndef REMOTE_SHELL_H
#define REMOTE_SHELL_H

#include "includes.h"

TFileStore *RemoteShellFileStoreCreate(char *Name, char *ConnectSetup);
TFileStore *SerialShellFileStoreCreate(char *Name, char *ConnectSetup);
TFileStore *TelnetShellFileStoreCreate(char *Name, char *ConnectSetup);

#endif
