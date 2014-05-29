
#ifndef FF_FILE_INFO_COMMANDS
#define FF_FILE_INFO_COMMANDS

#include "common.h"
#include "filestore.h"

char *LSCommandParseArg(char *Args,int *CmdFlags, ListNode *Vars);
void ProcessLSCmd(TFileStore *FS, TFileStore *DiskFS, int CmdFlags, char *Pattern, int LinesToList);
int ProcessGetFileInfo(TFileStore *FS, char *Pattern, char *Arg, int CmdFlags, ListNode *Vars);
int ProcessGetDigest(TFileStore *FS, char *Pattern, char *Type, int CmdFlags, ListNode *Vars);
int ProcessChMod(TFileStore *FS, char *Pattern, char *Mode, int CmdFlags, ListNode *Vars);
int ProcessCompare(TFileStore *DiskFS, TFileStore *FS, char *Command, char *Pattern, int CmdFlags, ListNode *Vars);
int ProcessHashCompare(TFileStore *DiskFS, TFileStore *FS, char *Command, char *Pattern, int CmdFlags, ListNode *Vars);


#endif
