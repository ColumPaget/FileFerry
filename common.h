
#ifndef FILERUNNER_COMMON_H
#define FILERUNNER_COMMON_H


#include "libUseful-2.0/libUseful.h"
#include "glob.h"
#define _GNU_SOURCE
#include <fnmatch.h>
#include <signal.h>
#include <sys/stat.h>


#define FTYPE_NONE 0
#define FTYPE_DIR  1
#define FTYPE_FILE 2
#define FTYPE_EXE 3
#define FTYPE_LINK 4
#define FTYPE_CHARDEV 5
#define FTYPE_BLKDEV 6
#define FTYPE_SOCKET 7
#define FTYPE_FS 100
#define FTYPE_IMAGE 101
#define FTYPE_PODCAST 102
#define FTYPE_MEDIA 103

#define THUMB_PIXELS 64

extern char *CacheDir;
extern time_t StartupTime;

#define KILOBYTE 1000
#define MEGABYTE 1000000
#define GIGABYTE 1000000000
#define TERABYTE 1000000000000

#define TRANSFER_COPY 1
#define TRANSFER_MOVE 2
#define TRANSFER_LINK 3

#define LS_INCLUDE_DIRS 1
#define LS_REFRESH	2

#define OPEN_WRITE 1
#define OPEN_PUBLIC 2
#define OPEN_CLONE 4
#define OPEN_RESUME 8

//GlobalSettings
#define FLAG_QUIET 1
#define FLAG_VERBOSE 2
#define FLAG_INTEGRITY_CHECK 4
#define FLAG_INTERRUPT 8

//Cannot be set except maybe on command line
#define FLAG_BACKGROUND 1024
#define FLAG_CONSOLE 2048


//Result codes for transfers
#define TRANSFER_OKAY 1
#define ERR_NOEXIST -1
#define ERR_EXISTS -2
#define ERR_FORBID -3
#define ERR_BADNAME -4
#define ERR_LOCKED -5
#define ERR_NOLENGTH -6
#define ERR_TOOBIG -7
#define ERR_FULL -8
#define ERR_RATELIMIT -9
#define ERR_SOURCEFILE -10
#define ERR_DESTFILE -11
#define ERR_INTERRUPTED -12
#define ERR_PARTIAL -13
#define ERR_FAIL -14
#define ERR_NOFILES -15
#define ERR_CANCEL -16
#define ERR_READ_NOTSUPPORTED -253
#define ERR_WRITE_NOTSUPPORTED -254
#define ERR_NOT_SUPPORTED -255
#define ERR_CUSTOM -65536

//Basic command flags. These must not
//clash with GET/PUT or LS modifiers
#define FLAG_CMD_DEBUG 1
#define FLAG_CMD_VERBOSE 2
#define FLAG_CMD_QUIET 4
#define FLAG_CMD_RECURSE 8
#define FLAG_CMD_NOMORESWITCHES 16

//GET/PUT modifiers
#define FLAG_CMD_SYNC 32
#define FLAG_CMD_FULLPATH 64
#define FLAG_CMD_PUBLIC 128
#define FLAG_CMD_THROTTLE 256
#define FLAG_CMD_CHMOD 512
#define FLAG_CMD_EXTN_SOURCE 1024
#define FLAG_CMD_EXTN_DEST 2048
#define FLAG_CMD_EXTN_TEMP 4096
#define FLAG_CMD_YOUNGERTHAN 8192
#define FLAG_CMD_OLDERTHAN 16384
#define FLAG_CMD_MTIME 32768
#define FLAG_CMD_LISTFROMFILE 65536
#define FLAG_CMD_RESUME 131072

//LS modifiers
#define FLAG_LS_DETAILS 2
#define FLAG_LS_LONGDETAILS 4
#define FLAG_LS_MEDIADETAILS 8
#define FLAG_LS_SIZE  16
#define FLAG_LS_SHARELINK  32
#define FLAG_LS_VERSION  64
#define FLAG_LS_REFRESH 128


//These are used both to specify the type of compare that should be used for checks
//and to provide a result that indicates which level of compare a file achieved
#define CMP_DIR -2
#define CMP_REMOTE -1

#define CMP_FAIL 0
#define CMP_EXISTS 1
#define CMP_SIZE 2
#define CMP_SIZE_TIME 4
#define CMP_CRC 8
#define CMP_MD5 16
#define CMP_SHA1 32

//These are used by the 'LoadDir' functions, and must not clash with any CMP_ values, 
//as those flags can be passed to the 'LoadDir'functions too
#define LIST_INCLUDE_DIRS 262144
#define LIST_REFRESH 524288


extern STREAM *LogFile;
extern int Mode;



typedef struct
{
char *Name;
char *Path;
char *EditPath;
char *Permissions;
char *User;
char *Group;
char *MediaType;
char *Hash;
char *ID;
int Type;
int Size;
int ResumePoint;
int Version;
int Mtime;
ListNode *Vars;
//TResource *Image;
}  TFileInfo;

typedef struct
{
char *MimeType;
char *Extn;
char *IconPath;
char *DefaultApplication;
} TMimeType;

typedef struct
{
int Flags;
char *HashType;
ListNode *Vars;
} TSettings;

extern TSettings Settings;

char *GetCompareLevelName(int Type);
int GetCompareLevelTypes(char *TypeList);

void WriteLog(char *Msg);
void HandleEventMessage(int Flags, char *FmtStr, ...);

char *CleanBadChars(char *OutBuff, char *InStr);
char *GetBasename(char *);
char *TerminateDirectoryPath(char *DirPath);
char *PathChangeExtn(char *RetStr, char *Path, char *Extn);
int ConvertFModeToNum(char *Mode);

void AddSource(char *Extn, char *MediaType);
void SetIntegerVar(ListNode *VarList, char *VarName, int Value);

TFileInfo *FileInfoCreate(char *Name, char *Path, int Type, int Size, int Mtime);
void FileInfoDestroy(TFileInfo *FI);
TFileInfo *FileInfoClone(TFileInfo *FI);
TFileInfo *OpenFileParseArgs(char *Name, char *Args);
char *STREAMReadDocument(char *Buffer, STREAM *S, int StripCR);

int GetPathType(char *Path);
int GetItemType(TFileInfo *Item);
int CheckFileMtime(TFileInfo *FI, int CmdFlags, ListNode *Vars);

void RequestAuthDetails(char **Logon, char **Passwd);
int FileIncluded(TFileInfo *FI, char *IncludePattern,char *ExcludePattern, int CmdFlags, ListNode *Vars);

#endif
