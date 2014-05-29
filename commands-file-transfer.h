
#ifndef FF_FILE_TRANSFER_COMMANDS
#define FF_FILE_TRANSFER_COMMANDS

#include "common.h"
#include "filestore.h"


char *TransferCommandParseArg(char *Args,int *CmdFlags, ListNode *Vars);
//int ProcessGetFiles(TFileStore *DiskFS, TFileStore *FS, char *MatchPattern, int CmdFlags, ListNode *Vars);
//int ProcessPutFiles(TFileStore *DiskFS, TFileStore *FS, char *Pattern, int CmdFlags, ListNode *Vars);
int ProcessTransferFiles(TFileStore *SrcFS, TFileStore *DestFS, int CmdType, char *Pattern, int CmdFlags, ListNode *Vars);
int ProcessRename(TFileStore *FS, char *From, char *To, int CmdFlags, ListNode *Vars);
int ProcessCreateFile(TFileStore *FS, char *Name, ListNode *Vars);

#endif


