#ifndef FILESTORE_H
#define FILESTORE_H

#include "common.h"

#define FS_LOCAL 1
#define FS_WRITEABLE 2
#define FS_NEEDS_ENCODING 4
#define FS_CHDIR_FULLPATH 8
#define FS_SHOW_DOTFILES 16
#define FS_REQ_CLONE 32
#define FS_RESTART_SETTINGS 64
#define FS_SSL 128
#define FS_NOEXTENSION 256
#define FS_NOCASE 512
#define FS_NEEDS_BUILT_PATH 2048
#define FS_NOLS_DATA 4096
#define FS_FEATURES_READ 8192
#define FS_NO_FOLDERS 16384
#define FS_NO_SUBFOLDERS 32768
#define FS_NOLOGON 65536


/*Filestore Features*/
#define FS_TRANSFER_TYPES 1
#define FS_FILE_SIZE 2
#define FS_FILE_HASHES 4
#define FS_SYMLINKS 8
#define FS_BACKGROUND_TRANSFERS 16
#define FS_DIRECT_CHDIR 32
#define FS_COMPRESSION 64
#define FS_RESUME_TRANSFERS 256
#define FS_OAUTH 512


typedef enum {FS_DISK, FS_FTP, FS_RSS, FS_HTTP, FS_HTTPS, FS_WEBDAV, FS_SWEBDAV, FS_IDRIVE, FS_SKYDRIVE, FS_GDRIVE, FS_GSITES, FS_PICASA, FS_FAW, FS_RESTBACKUP, FS_SSH, FS_SHELL, FS_TELNET, FS_TTY, FS_POP3, FS_POP3S} FS_TYPES;

typedef struct t_file_store TFileStore;

typedef TFileStore *(*CREATE_FUNC)(char *Name, char *InitData);
typedef int (* FILESIZE_FUNC)(TFileStore *, char *Path);
typedef int (* OPEN_FUNC)(TFileStore *);
typedef int (* CLOSE_FUNC)(TFileStore *);
typedef int (* LOAD_DIR_FUNC)(TFileStore *, char *Pattern, ListNode *Items, int Flags);
typedef int (* CHECK_DIRS_FUNC)(TFileStore *, ListNode *Items);
typedef int (* CHDIR_FUNC)(TFileStore *, char *Dir);
typedef time_t (* GET_MTIME_FUNC)(TFileStore *, char *Dir);
typedef STREAM *(* OPEN_FILE_FUNC)(TFileStore *, TFileInfo *FI, int Flags);
typedef int (* FILE_DELETE_FUNC)(TFileStore *, TFileInfo *FI);
typedef int (* MKDIR_FUNC)(TFileStore *, char *Path);
typedef int (* FILE_RENAME_FUNC)(TFileStore *, char *OldPath, char *NewPath);
typedef int (* SYMLINK_FUNC)(TFileStore *, char *OldPath, char *NewPath);
typedef int (* READ_BYTES_FUNC)(TFileStore *, STREAM *S, char *Buffer, int BuffLen);
typedef int (* WRITE_BYTES_FUNC)(TFileStore *, STREAM *S, char *Buffer, int BuffLen);
typedef int (* RUN_TASK_FUNC)(TFileStore *, char *FilePath);
typedef int (* PUT_FILE_FUNC)(TFileStore *, char *FilePath);
typedef int (* CLOSE_FILE_FUNC)(TFileStore *, STREAM *S);
typedef int (* FILEINFO_FUNC)(TFileStore *FS, char *File, char **RetStr);
typedef int (* SHARELINK_FUNC)(TFileStore *FS, TFileInfo *FI, char *ToList, char *Password);
typedef int (* GETDIGEST_FUNC)(TFileStore *FS, char *FilePath, char *Type, char **Digest);
typedef int (* SWITCHUSER_FUNC)(TFileStore *, char *UserName, char *Password);
typedef int (* CHMOD_FUNC)(TFileStore *, char *Mod, TFileInfo *FI);

struct t_file_store
{
char *Name;
int Flags;

//Features lists available things that can be turned on/off in Settings
int Features;
//Settings says which features are active
int Settings;
char *Type;
char *URL;
char *InitArg;
char *InitDir;
char *CurrDir;
char *CurrDirPath;
char *EndLine;
char *Logon;
char *Passwd;
char *RefreshToken;
char *Host;
int Port;
ListNode *Vars;
CREATE_FUNC Create;
OPEN_FUNC Open;
CLOSE_FUNC Close;
CHDIR_FUNC ChDir;
GET_MTIME_FUNC GetMtime;
MKDIR_FUNC MkDir;
LOAD_DIR_FUNC LoadDir;
OPEN_FILE_FUNC OpenFile;
READ_BYTES_FUNC ReadBytes;
WRITE_BYTES_FUNC WriteBytes;
PUT_FILE_FUNC PutFile;
CLOSE_FILE_FUNC CloseFile;
FILE_DELETE_FUNC DeleteFile;
FILE_RENAME_FUNC RenameFile;
FILE_RENAME_FUNC LinkFile;
FILESIZE_FUNC GetFileSize;
SYMLINK_FUNC Symlink;
RUN_TASK_FUNC RunTask;
CHECK_DIRS_FUNC GatherExtraDirData;
CHMOD_FUNC ChMod;
FILEINFO_FUNC GetFileInfo;
SHARELINK_FUNC ShareLink;
GETDIGEST_FUNC GetDigest;
SWITCHUSER_FUNC SwitchUser;
STREAM *S;
void *Extra;
unsigned long BuffSize;
double BytesAvailable;
double BytesUsed;
ListNode *DirListing;
};


typedef struct
{
int ViewType;
int Flags;
TFileStore *FS;
ListNode *Items;
char *ThumbnailsPath;
time_t LoadTime;
} TDirMenuInfo;

typedef void (*TRANSFER_DONE_CALLBACK)(char *, void *);


typedef struct
{
TFileStore *FromFS;
TFileStore *ToFS;
char *FromPath;
char *ToPath;
int Pid;
TRANSFER_DONE_CALLBACK OnTransferDone;
void *UserData;
} TFileTransfer;

//This is a bit flash, or maybe nasty. We define two types of function pointer, and then pass functions to a function
//that breaks up the file pattern list into individual patterns, and calls the functor against each of them
typedef int (*TRANSFER_COMMAND_FUNCTOR)(TFileStore *FromFS, TFileStore *ToFS, int Command, char *PatternList, int CmdFlags, ListNode *Vars);
typedef int (*INFO_COMMAND_FUNCTOR)(TFileStore *FromFS, char *PatternList, char *Arg, int CmdFlags, ListNode *Vars);

TFileStore *FileStoreCreate(char *Name, char *FSPath);
int FileStoreCopyFile(TFileStore *FromFS, char *FromPath, TFileStore *ToFS, char *ToPath);
int FileStoreLocalTransfer(TFileStore *FromFS, TFileStore *ToFS, char *FileName, int TransferType);
TFileInfo *Decode_LS_Output(char *CurrDir, char *LsLine);
int DefaultChDir(TFileStore *FS, char *Dir);
int DefaultReadBytes(TFileStore *FS, STREAM *S, char *Buffer, int MaxLen);
int DefaultWriteBytes(TFileStore *FS, STREAM *S, char *Buffer, int MaxLen);

TFileStore *FileStoreClone(TFileStore *FS);
char *FormatDirPath(char *RetStr,char *D1, char *D2);
int FileStoreChDir(TFileStore *FS, char *Dir);
int FileStoreChMod(TFileStore *FS, TFileInfo *FI, char *Mode);
int FileStoreLoadDir(TFileStore *FS,char *PatternMatch,ListNode *Items, int Flags);
void PrintConnectionDetails(TFileStore *FS, STREAM *S);

int FileStoreMkDir(TFileStore *FS,char *Dir, int CmdFlags);
int FileStoreDelete(TFileStore *FS,char *PatternMatch, int CmdFlags);
int FileStoreRename(TFileStore *FS,char *OldPath, char *NewPath);
int FileStoreAnalyzePath(char *Path);
int FileStoreGetFileSize(TFileStore *FS, char *FName);
//int FileStoreCompareFile(TFileStore *FS, ListNode *Items,TFileInfo *CmpFile);
int FileStoreCompareFile(TFileStore *FS, TFileStore *CmpFS, TFileInfo *FI, TFileInfo *CmpFile, int CompareLevel);
int FileStoreHashCompare(TFileStore *FS, TFileStore *CmpFS, char *HashType, char *Name);
TFileInfo *FileStoreGetFileInfo(TFileStore *FS, char *FName);
void FileStoreParseConfig(STREAM *S, char *Name);
void FileStoreSaveDetails(char *Path, char *SaveName, char *ConnectStr);
int FileStoreChangeSetting(TFileStore *FS, int Flag, char *Value);


char *FileStoreSelectOptionType(char *RetStr, TFileStore *FS1, TFileStore *FS2, char *AvailableVar, char *Choice);

char *MakeFullPath(char *RetStr,TFileStore *FS, char *FilePath);
int FileStoreDigestCompare(TFileStore *DiskFS, TFileStore *FS, char *FileName);
int FileStoreSelectCompareLevel(TFileStore *FromFS, TFileStore *ToFS, char *CompareLevel);

void DestroyFileStore(void *ptr);

#endif
