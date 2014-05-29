#include "disk.h"
#include <pwd.h>
#include <grp.h>

STREAM *DiskOpenFile(TFileStore *FS, TFileInfo *FI, int Flags)
{
char *Tempstr=NULL;
long pos;
STREAM *S;

Tempstr=FileStoreFormatPath(Tempstr, FS, FI->Path);
if (Flags & OPEN_WRITE)
{
 if (Flags & OPEN_RESUME)	S=STREAMOpenFile(Tempstr,O_CREAT | O_WRONLY);
 else S=STREAMOpenFile(Tempstr,O_CREAT | O_TRUNC | O_WRONLY);

 if (StrLen(FI->Permissions)) chmod(Tempstr,ConvertFModeToNum(FI->Permissions));

}
else S=STREAMOpenFile(Tempstr,O_RDONLY);

if (Flags & OPEN_RESUME) pos=STREAMSeek(S,0,SEEK_END);
DestroyString(Tempstr);

return(S);
}

int DiskCloseFile(TFileStore *FS, STREAM *S)
{
	STREAMClose(S);
	return(TRUE);
}


int DiskGetFileSize(TFileStore *FS, char *FName)
{
struct stat FileStat;
char *Tempstr=NULL;

Tempstr=FileStoreFormatPath(Tempstr, FS, FName);
lstat(Tempstr,&FileStat);

DestroyString(Tempstr);
return(FileStat.st_size);
}

int DiskGetDigest(TFileStore *FS, char *FName, char *Type, char **Digest)
{
char *Tempstr=NULL;
STREAM *S;
int result, RetVal=FALSE;
THash *Hash;

*Digest=CopyStr(*Digest,"");
Tempstr=FileStoreFormatPath(Tempstr, FS, FName);

Hash=HashInit(Type);
if (Hash)
{
	S=STREAMOpenFile(Tempstr,O_RDONLY);
	if (S)
	{
		RetVal=TRUE;
		Tempstr=SetStrLen(Tempstr,4096);
		result=STREAMReadBytes(S,Tempstr,4096);
		while (result > 0)
		{
			Hash->Update(Hash,Tempstr,result);
		result=STREAMReadBytes(S,Tempstr,4096);
		}

	STREAMClose(S);
	}
Hash->Finish(Hash, ENCODE_HEX, Digest);
HashDestroy(Hash);
}

DestroyString(Tempstr);

return(RetVal);
}



int DiskChDir(TFileStore *FS, char *Dir)
{
char *Tempstr=NULL;
int result=TRUE;

//make a copy of curr dir incase we have to go back
Tempstr=CopyStr(Tempstr,FS->CurrDir);

//DefaultChDir changes FS->CurrDir. It handles leading '/' etc
DefaultChDir(FS,Dir);
if (access(FS->CurrDir,X_OK)!=0)
{
	FS->CurrDir=CopyStr(FS->CurrDir,Tempstr);
	result=FALSE;
}

DestroyString(Tempstr);
return(result);
}


int DiskMkDir(TFileStore *FS, char *Dir)
{
int result;
char *Tempstr=NULL;


Tempstr=FileStoreFormatPath(Tempstr, FS, Dir);
result=mkdir(Tempstr, 0777);

DestroyString(Tempstr);
if (result==0) return(TRUE);
return(FALSE);
}


int DiskDeleteFile(TFileStore *FS, TFileInfo *FI)
{
int result;

result=unlink(FI->Path);

if (result==0) return(TRUE);
return(FALSE);
}

int DiskRenameFile(TFileStore *FS, char *OldPath, char *NewPath)
{
int result;
char *From=NULL, *To=NULL;

From=FileStoreFormatPath(From, FS, OldPath);
To=FileStoreFormatPath(To, FS, NewPath);
result=rename(From,To);

DestroyString(From);
DestroyString(To);

if (result==0) return(TRUE);
return(FALSE);
}

int DiskChMod(TFileStore *FS, TFileInfo *FI, char *Mode)
{
 chmod(FI->Path,ConvertFModeToNum(Mode));
 return(TRUE);
}

int DiskLoadDir(TFileStore *FS, char *InPattern, ListNode *Items, int Flags)
{
glob_t myGlob;
char *Tempstr=NULL;
char *path, *basename, *matchptr;
int i, val=0, type=0;
struct stat myStat;
TFileInfo *FI;
struct passwd *pwent;
struct group *grpent;

if (*InPattern=='/') Tempstr=CopyStr(Tempstr,InPattern);
else
{
//	Tempstr=QuoteCharsInStr(Tempstr,FS->CurrDir,"[]?*");
	Tempstr=CopyStr(Tempstr,FS->CurrDir);
	strrep(Tempstr,'[','?');
	strrep(Tempstr,']','?');
	Tempstr=CatStr(Tempstr,InPattern);
}

glob(Tempstr,GLOB_PERIOD,0,&myGlob);

for (i=0; i < myGlob.gl_pathc; i++)
{
  path=myGlob.gl_pathv[i];
  basename=GetBasename(path);

	memset(&myStat,0,sizeof(myStat));
  if (lstat(path,&myStat)==-1) perror("STAT:");

	
  FI=NULL;
	if (S_ISDIR(myStat.st_mode)) type=FTYPE_DIR;
	else if (S_ISLNK(myStat.st_mode)) type=FTYPE_LINK;
	else if (S_ISCHR(myStat.st_mode)) type=FTYPE_CHARDEV;
	else if (S_ISBLK(myStat.st_mode)) type=FTYPE_BLKDEV;
	else if (S_ISSOCK(myStat.st_mode)) type=FTYPE_SOCKET;
	else if (S_ISFIFO(myStat.st_mode)) type=FTYPE_SOCKET;
	else type=FTYPE_FILE;

	if ((strcmp(basename,".")==0) || (strcmp(basename,"..")==0))  ; //do  nothing
	else
  {
	FI=FileInfoCreate(basename,path,type,myStat.st_size,myStat.st_mtime);
	FI->Permissions=ConvertNumToFMode(FI->Permissions,myStat.st_mode);
	if (type==FTYPE_LINK) 
	{
		FI->EditPath=SetStrLen(FI->EditPath,myStat.st_size);
		readlink(path,FI->EditPath,myStat.st_size);
	}
	ListAddNamedItem(Items,FI->Name,FI);
  }

  if (FI)
  {
    pwent=getpwuid(myStat.st_uid);
    if (! pwent) FI->User=FormatStr(FI->User,"%d",myStat.st_uid);
    else FI->User=CopyStr(FI->User,pwent->pw_name);
    grpent=getgrgid(myStat.st_gid);
    if (! grpent) FI->User=FormatStr(FI->User,"%d",myStat.st_gid);
    else FI->Group=CopyStr(FI->Group,grpent->gr_name);

		if (Flags & CMP_MD5) DiskGetDigest(FS, FI->Name, "md5", &FI->Hash);
		if (Flags & CMP_SHA1) DiskGetDigest(FS, FI->Name, "sha1", &FI->Hash);
  }

}

globfree(&myGlob);

DestroyString(Tempstr);

return(TRUE);
}


time_t DiskGetMtime(TFileStore *FS, char *Path)
{
struct stat MyStat;
char *Tempstr=NULL;

Tempstr=FileStoreFormatPath(Tempstr, FS, Path);
stat(Path,&MyStat);

DestroyString(Tempstr);
return(MyStat.st_mtime);
}


int DiskOpenFS(TFileStore *FS)
{
char *Tempstr=NULL;

Tempstr=SetStrLen(Tempstr,BUFSIZ);
getcwd(Tempstr,BUFSIZ);

FS->CurrDir=CopyStr(FS->CurrDir,Tempstr);
FS->CurrDir=TerminateDirectoryPath(FS->CurrDir);

DestroyString(Tempstr);

return(TRUE);
}

int DiskCloseFS(TFileStore *FS)
{
return(TRUE);
}

TFileStore *DiskFileStoreCreate(char *Name, char *InitDir)
{
TFileStore *FS;

FS=(TFileStore *) calloc(1,sizeof(TFileStore));
FS->Create=DiskFileStoreCreate;
FS->CurrDir=CopyStr(FS->CurrDir,InitDir);
FS->LoadDir=DiskLoadDir;
FS->Flags |= FS_LOCAL | FS_WRITEABLE | FS_CHDIR_FULLPATH;
FS->Features |= FS_FILE_HASHES | FS_FILE_SIZE | FS_RESUME_TRANSFERS;
FS->Vars=ListCreate();
SetVar(FS->Vars,"HashTypes","md5 sha1");

FS->Open=DiskOpenFS;
FS->Close=DiskCloseFS;
FS->ChDir=DiskChDir;
FS->MkDir=DiskMkDir;
FS->GetMtime=DiskGetMtime;
FS->DeleteFile=DiskDeleteFile;
FS->RenameFile=DiskRenameFile;
FS->OpenFile=DiskOpenFile;
FS->CloseFile=DiskCloseFile;
FS->GetFileSize=DiskGetFileSize;
FS->GetDigest=DiskGetDigest;
FS->ReadBytes=DefaultReadBytes;
FS->WriteBytes=DefaultWriteBytes;
FS->InitDir=CopyStr(FS->InitDir,InitDir);
FS->Name=CopyStr(FS->Name,Name);
FS->ChMod=DiskChMod;

return(FS);
}
