//This file contains functions related to parsing the interactive file transfer/manipulation commands
#ifndef FILEFERRY_PARSE_COMMAND_H
#define FILEFERRY_PARSE_COMMAND_H

#include "common.h"

extern char *Commands[];
typedef enum {
//Remote Versions
CMD_CD, CMD_CHDIR, CMD_PWD, CMD_CREATE, CMD_MKDIR, CMD_DEL, CMD_RM, CMD_RMDIR, CMD_MOVE, CMD_RENAME, CMD_CHEXT, CMD_LS, CMD_DIR, CMD_MD5, CMD_SHA1, CMD_HASH, CMD_CHMOD, CMD_INFO, CMD_SHOW,
//Local Versions
CMD_LCD, CMD_LCHDIR, CMD_LPWD, CMD_LCREATE, CMD_LMKDIR, CMD_LDEL, CMD_LRM, CMD_LRMDIR, CMD_LMOVE, CMD_LRENAME, CMD_LCHEXT, CMD_LLS, CMD_LDIR, CMD_LMD5, CMD_LSHA1, CMD_LHASH, CMD_LCHMOD, CMD_LINFO, CMD_LSHOW, 

//Comamnds that are neither remote nor local
CMD_GET, CMD_PUT, CMD_CMP, CMD_HASHCMP, CMD_SET, CMD_TIME, CMD_HELP, CMD_QUIT, CMD_EXIT  
} TCmds;


char *DefaultCommandParseArg(char *Args,int *CmdFlags, ListNode *Vars);
char *CommandParseArg(char *Args,int Command, int *CmdFlags, ListNode *Vars);
char *ParseCommand(char *Line, int *Type, int *CmdFlags, ListNode *Vars);
void ProcessHelpCmd(int CmdFlags, char *Arg);

#endif
