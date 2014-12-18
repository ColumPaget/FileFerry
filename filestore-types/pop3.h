#ifndef POP3_H
#define POP3_H


#include "includes.h"

TFileStore *Pop3FileStoreCreate(char *Name, char *ConnectSetup);
TFileStore *Pop3SFileStoreCreate(char *Name, char *ConnectSetup);


#endif
