#ifndef HTTP_H
#define HTTP_H


#include "includes.h"

int HTTPParseResponse(const char *ResponseCode);
TFileStore *HTTPFileStoreCreate(char *Name, char *ConnectSetup);
TFileStore *HTTPSFileStoreCreate(char *Name, char *ConnectSetup);
TFileStore *RSSFileStoreCreate(char *Name, char *ConnectSetup);

void HTTPReadLink(TFileStore *FS, char *TagData, char *Pattern, ListNode *Items);

#endif
