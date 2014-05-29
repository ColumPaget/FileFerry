#ifndef FERRY__FILETYPES_H
#define FERRY__FILETYPES_H

#include "common.h"

TMimeType *AddMediaType(char *Extn, char *MediaType);
char *GuessMediaTypeFromPath(char *OutBuff,char *Path);
char *GetExtensionForMediaType(char *OutBuff,char *MediaType);
int GetMediaType(char *Type);

#endif
