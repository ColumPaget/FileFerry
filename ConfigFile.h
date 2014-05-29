#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include "filestore.h"

TFileStore *ConfigFileReadFileStore(STREAM *S, char *Name);

ListNode *ConfigFileLoadFileStores(char *Path);
void ConfigFileSaveFileStore(TFileStore *FS);


#endif
