#ifndef GOOGLEDATA_H
#define GOOGLEDATA_H

#include "includes.h"

int GoogleAuth(TFileStore *FS, char **AuthToken);
int GoogleOAuth(TFileStore *FS);
HTTPInfoStruct *GDataConnect(TFileStore *FS, char *Method, char *URL, char *PostData, ListNode *Headers, int HttpFlags);
int GDataLoadDir(TFileStore *FS, char *URL, char *InPattern, ListNode *Items, int Flags);
int GDataDeleteFile(TFileStore *FS, TFileInfo *FI);



#endif
