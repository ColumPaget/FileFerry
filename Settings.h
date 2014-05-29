#ifndef FILEFERRY_SETTINGS_H
#define FILEFERRY_SETTINGS_H

#include "filestore.h"

int ChangeFilestoreSetting(TFileStore *FS, char *Name, char *Value);
void DisplaySettings(TFileStore *FS);
void ProcessSetting(TFileStore *FS, char *Args);
void SetDefaultHashType(char *HashTypes);

#endif
