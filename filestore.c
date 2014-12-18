#include "filestore.h"


char *FileStoreTypeStrings[]={"disk","ftp","rss","http","https","webdav","swebdav","idrive", "skydrive", "gdrive","gsites","picasa","faw","restbackup","ssh","shell","telnet","tty","pop3","pop3s",NULL};


int DefaultReadBytes(TFileStore *FS, STREAM *S, char *Buffer, int MaxLen)
{
return(STREAMReadBytes(S,Buffer,MaxLen));
}

int DefaultWriteBytes(TFileStore *FS, STREAM *S, char *Buffer, int MaxLen)
{
return(STREAMWriteBytes(S,Buffer,MaxLen));
}


char *FormatDirPath(char *RetStr, char *D1,  char *D2)
{
char *NewPath=NULL, *ptr;

NewPath=CopyStr(RetStr,D1);
if (*D2=='/') NewPath=CopyStr(RetStr,D2);
else if (strcmp(D2,".")==0) /* do nothing */ ;
else if (strcmp(D2,"..")==0)
{
 ptr=strrchr(NewPath,'/');
 
 if (ptr)
 {
 //if the line ends in '/', then try again
 if (*(ptr+1)=='\0')
 {
	*ptr='\0';
	ptr=strrchr(NewPath,'/');
 }
 
 //Now clip off dir component	
 if (ptr) *ptr='\0';
 }
}
else
{
	//in case
	NewPath=TerminateDirectoryPath(NewPath);
	NewPath=CatStr(NewPath,D2);
}

NewPath=TerminateDirectoryPath(NewPath);

return(NewPath);
}

int DefaultChDir(TFileStore *FS, char *Dir)
{
char *Tempstr=NULL;

Tempstr=FormatDirPath(Tempstr, FS->CurrDir, Dir);
FS->CurrDir=CopyStr(FS->CurrDir,Tempstr);

//Any directory listing we had is now invalid
ListClear(FS->DirListing,FileInfoDestroy);

DestroyString(Tempstr);

return(TRUE);
}




TFileStore *FileStoreClone(TFileStore *FS)
{
TFileStore *NewFS;
ListNode *Curr;
TFileInfo *FI;

NewFS=FS->Create(FS->Name,FS->InitArg);
NewFS->Name=CopyStr(NewFS->Name,FS->Name);
NewFS->Type=CopyStr(NewFS->Type,FS->Type);
NewFS->Host=CopyStr(NewFS->Host,FS->Host);
NewFS->Port=FS->Port;
NewFS->Logon=CopyStr(NewFS->Logon,FS->Logon);
NewFS->Passwd=CopyStr(NewFS->Passwd,FS->Passwd);
NewFS->CurrDir=CopyStr(NewFS->CurrDir,FS->CurrDir);
NewFS->Features=FS->Features;
NewFS->Settings=FS->Settings;
CopyVars(NewFS->Vars,FS->Vars);
if (FS->DirListing)
{
	NewFS->DirListing=ListCreate();
	NewFS->DirListing->Flags |= LIST_FLAG_CASE;
	Curr=ListGetNext(FS->DirListing);
	while (Curr)
	{
		FI=(TFileInfo *) Curr->Item;
		ListAddNamedItem(NewFS->DirListing,FI->Name,FileInfoCreate(FI->Name,FI->Path,FI->Type,FI->Size,FI->Mtime));
		Curr=ListGetNext(Curr);
	}
}


return(NewFS);
}


void DestroyFileStore(void *ptr)
{
TFileStore *FS;

FS=(TFileStore *) ptr;
DestroyString(FS->Name);
DestroyString(FS->Type);
DestroyString(FS->URL);
DestroyString(FS->InitArg);
DestroyString(FS->InitDir);
DestroyString(FS->CurrDir);
DestroyString(FS->CurrDirPath);
DestroyString(FS->EndLine);
DestroyString(FS->Logon);
DestroyString(FS->Passwd);
DestroyString(FS->Host);

ListDestroy(FS->Vars,DestroyString);

free(FS);
}



int FindTokenInString(char *Token, char *String, char *Separator)
{
char *SToken=NULL, *ptr;
int result=-1, count=0;

ptr=GetToken(String,Separator,&SToken,0);
while (ptr)
{
	if (strcmp(Token,SToken)==0) 
	{
		result=count;
		break;
	}
	count++;
	ptr=GetToken(ptr,Separator,&SToken,0);
}

DestroyString(SToken);
return(result);
}


//Select an optional feature from a list of available types. 'Choice'
//contains the name contains the selection the user has made. 
//The 'AvailableVar' string holds the NAME OF THE VARIABLE that
//holds the list of availble types 

//For example, the 'CompressionTypes' variable stores any available compression types.
//
//'Choice' can be set to 'on', which will choose the first Available item

char *FileStoreSelectOptionType(char *RetStr, TFileStore *FS1, TFileStore *FS2, char *AvailableVar, char *Choice)
{
char *p1_Available, *p2_Available, *ptr, *Token=NULL;

if (! StrLen(Choice)) return(CopyStr(RetStr,""));
if (strcmp(Choice,"off")==0) return(CopyStr(RetStr,""));

p1_Available=GetVar(FS1->Vars,AvailableVar);
if (! StrLen(p1_Available)) return(CopyStr(RetStr,""));
p2_Available=GetVar(FS2->Vars,AvailableVar);
if (! StrLen(p2_Available)) return(CopyStr(RetStr,""));


if (strcmp(Choice,"on")==0) 
{
	ptr=GetToken(p1_Available," ",&Token,0);
	while (ptr)
	{
		if (FindTokenInString(Token, p2_Available, " "))
		{
			RetStr=CopyStr(RetStr,Token);
			break;
		}
		ptr=GetToken(ptr," ",&Token,0);
	}
}
else 
{
	if (
		(FindTokenInString(Choice, p1_Available, " ")) &&
		(FindTokenInString(Choice, p2_Available, " "))
	) RetStr=CopyStr(RetStr,Token);
}

DestroyString(Token);
return(RetStr);
}



int FileStoreLocalTransfer(TFileStore *FromFS, TFileStore *ToFS, char *FileName, int TransferType)
{
char *FromPath=NULL, *ToPath=NULL;

FromPath=FormatStr(FromPath,"%s/%s",FromFS->CurrDir,FileName);
ToPath=FormatStr(ToPath,"%s/%s",ToFS->CurrDir,FileName);


if ((TransferType==TRANSFER_MOVE) && FromFS->RenameFile) FromFS->RenameFile(FromFS,FromPath,ToPath);
if ((TransferType==TRANSFER_LINK) && FromFS->LinkFile) FromFS->LinkFile(FromFS,FromPath,ToPath);

DestroyString(FromPath);
DestroyString(ToPath);

return(TRUE);
}



TFileInfo *Decode_LS_Output(char *CurrDir, char *LsLine)
{
TFileInfo *FI;
char *Token=NULL, *DateStr=NULL;
char *ptr;

	if (! StrLen(LsLine)) return(NULL);
	FI=(TFileInfo *) calloc(1,sizeof(TFileInfo));

	ptr=GetToken(LsLine,"\\S",&Token,0);
	if (StrLen(Token)==8) //dos style
	{
		ptr=GetToken(ptr,"\\S",&Token,0);
		ptr=GetToken(ptr,"\\S",&Token,0);
		StripTrailingWhitespace(Token);
		if (strcmp(Token,"<DIR>")==0) FI->Type=FTYPE_DIR;
		else 
		{
			FI->Type=FTYPE_FILE;
			FI->Size=atoi(Token);
		}
		FI->Path=CopyStr(FI->Path,CurrDir);
		FI->Path=CatStr(FI->Path,ptr);
		FI->Name=CopyStr(FI->Name,ptr);
	}
	else //unix style
	{
		switch (*Token)
		{
			case 'd': FI->Type=FTYPE_DIR; break;
			case 'l': FI->Type=FTYPE_LINK; break;
			case 'c': FI->Type=FTYPE_CHARDEV; break;
			case 'b': FI->Type=FTYPE_BLKDEV; break;
			case 's': FI->Type=FTYPE_SOCKET; break;
			default: FI->Type=FTYPE_FILE; break;
		}
		FI->Permissions=CopyStr(FI->Permissions,Token);

		ptr=GetToken(ptr,"\\S",&Token,0);
		ptr=GetToken(ptr,"\\S",&FI->User,0);
		ptr=GetToken(ptr,"\\S",&FI->Group,0);
		ptr=GetToken(ptr,"\\S",&Token,0);
		StripTrailingWhitespace(Token);
		FI->Size=atoi(Token);

		ptr=GetToken(ptr,"\\S",&DateStr,0);
		ptr=GetToken(ptr,"\\S",&Token,0);
		DateStr=MCatStr(DateStr," ",Token,NULL);
		ptr=GetToken(ptr,"\\S",&Token,0);
		DateStr=MCatStr(DateStr," ",Token,NULL);
	
	
		if (DateStr[4]=='-')
		{
			//full iso format
			FI->Mtime=DateStrToSecs("%Y-%m-%d %H:%M:%S.000000000 %Z",DateStr,NULL);
		}
		else
		{
			//standard
			if (strchr(Token,':')) 
			{
				DateStr=CatStr(DateStr,":00 ");
	
				//if it gives a time, it means this year
				DateStr=CatStr(DateStr,GetDateStr("%Y",NULL));
				FI->Mtime=DateStrToSecs("%b %d %H:%M:%S %Y",DateStr,NULL);
			}
			else 
			{
				//if no time, then will include year instead. Add time
				//or else conversion fails
				DateStr=CatStr(DateStr," 00:00:00");
				FI->Mtime=DateStrToSecs("%b %d %Y %H:%M:%S",DateStr,NULL);
			}
		}
	
		FI->Path=CopyStr(FI->Path,CurrDir);
		FI->Path=CatStr(FI->Path,ptr);
		FI->Name=CopyStr(FI->Name,ptr);
	}


	DestroyString(Token);
	DestroyString(DateStr);
	return(FI);
}



TFileInfo *FileStoreGetFileInfo(TFileStore *FS, char *Path)
{
ListNode *Curr;
TFileInfo *FI;

Curr=ListGetNext(FS->DirListing);
while (Curr)
{
	FI=(TFileInfo *) Curr->Item;

	if (StrLen(FI->Name) && (fnmatch(Path,FI->Name,0)==0)) return(FI);
	if (StrLen(FI->Path) && (fnmatch(Path,FI->Path,0)==0)) return(FI);
Curr=ListGetNext(Curr);
}

return(NULL);
}


int FileStoreGetFileSize(TFileStore *FS, char *FName)
{
TFileInfo *FI;

FI=FileStoreGetFileInfo(FS,FName);
if (FI) return(FI->Size);
return(0);
}

int FileStoreHashCompare(TFileStore *FS, TFileStore *CmpFS, char *HashType, char *Name)
{
char *D1=NULL, *D2=NULL;
int result=FALSE;

if (! FS->GetDigest) return(CMP_EXISTS);
if (! (FS->Features & FS_FILE_HASHES)) return(CMP_EXISTS);

result=FS->GetDigest(FS,Name,HashType,&D1);
result=CmpFS->GetDigest(CmpFS,Name,HashType,&D2);


if (strcasecmp(D1,D2)==0)
{
if (strcmp(HashType,"md5")==0) result=CMP_MD5;
else if (strcmp(HashType,"sha1")==0) result=CMP_SHA1;
else if (strcmp(HashType,"sha")==0) result=CMP_SHA1;
else result=CMP_CRC;
}

DestroyString(D1);
DestroyString(D2);

return(result);
}




int FileStoreCompareFile(TFileStore *FS, TFileStore *CmpFS, TFileInfo *FI,TFileInfo *CmpFI, int CompareLevel)
{
ListNode *Curr;
char *CmpName=NULL, *ptr;
int result=CMP_FAIL;
int val;

if (! FI) return(CMP_FAIL);
if (! CmpFI) return(CMP_FAIL);

CmpName=CopyStr(CmpName,CmpFI->Name);
if (FS->Flags & FS_NOEXTENSION) 
{
ptr=strrchr(CmpName,'.');
if (ptr) *ptr='\0';
}

if ((FS->Flags & FS_NOCASE) && (strcasecmp(FI->Name,CmpName)==0)) result=CMP_EXISTS;
else if (strcmp(FI->Name,CmpName)==0) result=CMP_EXISTS;

//if it exists, try other checks
if ((result == CMP_EXISTS) && (CompareLevel > CMP_EXISTS))
{
	if ( (FI->Type == FTYPE_DIR) && (CmpFI->Type == FTYPE_DIR) ) result=CMP_DIR;
	else
	{
		if (CmpFI->Size == FI->Size)
		{
			result=CMP_SIZE;

			if ((result==CMP_SIZE) && (CompareLevel > CMP_SIZE))
			{
				val=CmpFI->Mtime - FI->Mtime;
				if (val < 0) val=0-val;
				if (val < 60) result=CMP_SIZE_TIME;

				//Even if SIZE_TIME failed, a hash might prove that the files are the same
				if ( ((CompareLevel==CMP_MD5) || (CompareLevel==CMP_SHA1)) && StrLen(FI->Hash) && StrLen(CmpFI->Hash))
				{
				 	if (strcmp(FI->Hash,CmpFI->Hash)==0) result=CompareLevel;
				}
				else
				{
					if (CompareLevel == CMP_MD5) result=FileStoreHashCompare(FS,CmpFS,"md5",FI->Name);
					if (CompareLevel == CMP_SHA1) result=FileStoreHashCompare(FS,CmpFS,"sha1",FI->Name);
				}
			}
		}
	}

}


DestroyString(CmpName);
return(result);
}



int FileStoreGetFileType(TFileStore *FS, TFileInfo *FI)
{
STREAM *S;
char *Tempstr=NULL, *ptr;
int result;

FI->MediaType=GuessMediaTypeFromPath(FI->MediaType,FI->Path);

if (FS->Flags & FS_LOCAL)
{
	S=FS->OpenFile(FS,FI,0);
	if (S)
	{
		Tempstr=SetStrLen(Tempstr,4096);
		result=FS->ReadBytes(FS,S,Tempstr,4096);
		if (result > 0)
		{
			StripLeadingWhitespace(Tempstr);
			ptr=Tempstr;
			if (*ptr=='<')
			{	
				ptr++;
				while (isspace(*ptr)) ptr++;

				if (strncmp(ptr,"html",4)==0) FI->MediaType=CopyStr(FI->MediaType,"text/html");
				if (strncmp(ptr,"?xml",4)==0) 
				{
					FI->MediaType=CopyStr(FI->MediaType,"application/xml");
					if (strstr(Tempstr,"xmlns:sites='http://schemas.google.com/sites/2008'")) FI->MediaType=CopyStr(FI->MediaType,"application/googlesites");
				}
			}
		}
		STREAMClose(S);
	}
}

DestroyString(Tempstr);

return(result);
}



int CheckMatchPatternList(ListNode *PatternList, char *FilePath)
{
ListNode *Curr;

if (! StrLen(FilePath)) return(FALSE);
Curr=ListGetNext(PatternList);
while (Curr)
{
if (fnmatch((char *) Curr->Item,FilePath,0)==0) return(TRUE);
Curr=ListGetNext(Curr);
}

return(FALSE);
}


//For some types of filestore this function only goes one level
//deep and has to be called many times to descend multiple levels
int FileStoreInternalChDir(TFileStore *FS, char *Dir)
{
char *OldDir=NULL;

OldDir=CopyStr(OldDir,FS->CurrDir);


if (FS->ChDir(FS,Dir))
{
	if (FileStoreLoadDir(FS,"",NULL, LIST_REFRESH|LIST_INCLUDE_DIRS))
	{
		DestroyString(OldDir);
		return(TRUE);
	}
}

FS->CurrDir=CopyStr(FS->CurrDir,OldDir);
DestroyString(OldDir);

return(FALSE);
}


int FileStoreChDir(TFileStore *FS, char *Dir)
{
char *Token=NULL, *ptr;
char *OldDir=NULL;

if (! FS->ChDir) return(FALSE);
//if filestore supports going straight to a directory by full path, then
//do so.
if (FS->Flags & FS_CHDIR_FULLPATH) return(FileStoreInternalChDir(FS,Dir));

if (strcmp(Dir,"/")==0) return(FileStoreInternalChDir(FS,"/"));

OldDir=CopyStr(OldDir,FS->CurrDir);
ptr=GetToken(Dir,"/",&Token,0);
while (ptr)
{
if (StrLen(Token))
{
	 if (! FileStoreInternalChDir(FS,Token))
	 {
		FS->CurrDir=CopyStr(FS->CurrDir,OldDir);
		DestroyString(OldDir);
		DestroyString(Token);
		return(FALSE);
	 }
}
ptr=GetToken(ptr,"/",&Token,0);
}

DestroyString(OldDir);
DestroyString(Token);

return(TRUE);
}


int FileStoreChMod(TFileStore *FS, TFileInfo *FI, char *Mode)
{
if (StrLen(Mode)==0) return(FALSE);
if (! FS->ChMod) return(FALSE);

return(FS->ChMod(FS,FI,Mode));
}



#define DIR_CACHE_ACTIVE 0
#define DIR_CACHE_RELOAD 1
#define DIR_CACHE_IGNORE 2

int FileStoreLoadDir(TFileStore *FS, char *PatternMatch, ListNode *Items, int Flags)
{
int count=0, CacheFlags=DIR_CACHE_ACTIVE;
ListNode *NewItems=NULL, *Patterns=NULL, *Curr;
TFileInfo *FI, *NewFI;
char *ptr, *Token=NULL;
int result=FALSE;

/*
Patterns=ListCreate();
ptr=GetToken(PatternMatch,"\\S",&Token,GETTOKEN_QUOTES);
while (ptr)
{
ListAddItem(Patterns,CopyStr(NULL,Token));
ptr=GetToken(ptr,"\\S",&Token,GETTOKEN_QUOTES);
}
*/


//if the directory cache doesn't exist, then create it
if (! FS->DirListing) 
{
	FS->DirListing=ListCreate();
	FS->DirListing->Flags |= LIST_FLAG_CASE;
}


NewItems=ListCreate();
NewItems->Flags |= LIST_FLAG_CASE;

//if we're given a full path then it's not the current directory,
//so the cached data in FS->DirListing isn't relevant
if (*PatternMatch=='/')
{
	FS->LoadDir(FS,PatternMatch,NewItems,Flags);
	CacheFlags=DIR_CACHE_IGNORE;

}
//else if we've been asked to list files in the current directory
//load all files in the current directory
else if ((ListSize(FS->DirListing)==0) || (Flags & LIST_REFRESH) )
{
	if (FS->LoadDir(FS,"*",NewItems,Flags))
	{
		CacheFlags=DIR_CACHE_RELOAD;
		ListClear(FS->DirListing,FileInfoDestroy);
	}
}


// if we are not reloading the directory cache, then get items out of that
//else we must get it from 'new items' and add the to the directory cache
if (CacheFlags==DIR_CACHE_ACTIVE) Curr=ListGetNext(FS->DirListing);
else Curr=ListGetNext(NewItems);

while (Curr)
{
		FI=(TFileInfo *) Curr->Item;

		//if we are reloading the directory, then add the item to the directory cache
		if (CacheFlags==DIR_CACHE_RELOAD) 
		{
			ListAddNamedItem(FS->DirListing,FI->Name,FI);
			Curr->Item=NULL;
		}

		/*Do a little pre-processing on the found items */
		if (
				(StrLen(FI->MediaType)==0) ||
				(strchr(FI->MediaType,'?'))
		) FI->MediaType=GuessMediaTypeFromPath(FI->MediaType,FI->Path);


		//Add the found item to our 'Items' return list
		if (
					(CacheFlags == DIR_CACHE_IGNORE) ||
					((Flags & LIST_INCLUDE_DIRS) && (FI->Type==FTYPE_DIR)) ||
					(fnmatch(PatternMatch,FI->Name,0)==0) || 
					(fnmatch(PatternMatch,FI->Path,0)==0) 
					//(CheckMatchPatternList(Patterns, FI->Name))
				)
		{
			NewFI=FileInfoClone(FI);
			ListAddNamedItem(Items,FI->Name,NewFI);
		}
Curr=ListGetNext(Curr);
}

ListDestroy(NewItems,FileInfoDestroy);
//ListDestroy(Patterns,DestroyString);
DestroyString(Token);

return(TRUE);
}


int FileStoreDelete(TFileStore *FS,char *PatternMatch, int CmdFlags)
{
int result=FALSE;
ListNode *Node, *Curr, *Items;
TFileInfo *FI;

if(! FS->DeleteFile) return(FALSE);

Items=ListCreate();
FileStoreLoadDir(FS,PatternMatch,Items, 0);
if (ListSize(Items) > 0)
{
Curr=ListGetNext(Items);
while (Curr)
{
	FI=(TFileInfo *) Curr->Item;
	if ((FI->Type==FTYPE_DIR) && (CmdFlags & FLAG_CMD_RECURSE))
	{
		if (FileStoreChDir(FS,FI->Name))
		{
		FileStoreDelete(FS,"*", CmdFlags);
		FileStoreChDir(FS,"..");
		}
	}
	
	if (FS->DeleteFile(FS,FI) )
	{
		printf("OKAY: Deleted %s\n",FI->Name);

		result=TRUE;
		Node=ListFindNamedItem(FS->DirListing,FI->Name);
		if (Node) 
		{
			FileInfoDestroy(Node->Item);
			ListDeleteNode(Node);
		}
	}
	else printf("FAIL: Couldn't remove %s\n",FI->Name);
	Curr=ListGetNext(Curr);
}
}

ListDestroy(Items,FileInfoDestroy);

return(result);
}


int FileStoreMkDir(TFileStore *FS,char *Dir, int CmdFlags)
{
int result=FALSE;
ListNode *Node, *Curr, *Items;
TFileInfo *FI;

if(! FS->MkDir) return(FALSE);

if (FS->MkDir(FS,Dir))
{
	ListAddNamedItem(FS->DirListing,Dir,FileInfoCreate(Dir,Dir,FTYPE_DIR,0,time(NULL)));
	result=TRUE;
}

return(result);
}


int FileStoreDestIsDirectory(TFileStore *FS, char *DestName)
{
TFileInfo *FI;

if (strcmp(DestName,"/")==0) return(TRUE);
if (strcmp(DestName,"..")==0) return(TRUE);

if ((strchr(DestName,'/') && (strncmp(FS->CurrDir,DestName,StrLen(FS->CurrDir)) !=0))) return(TRUE);

FI=FileStoreGetFileInfo(FS, DestName);
if (FI && (FI->Type==FTYPE_DIR)) return(TRUE);

return(FALSE);
}


int FileStoreRename(TFileStore *FS,char *OldName, char *NewName)
{
int result=FALSE;
ListNode *Node, *Curr;
TFileInfo *FI;

if(! FS->RenameFile) return(FALSE);

//Does the filestore provide full paths for items that we've looked up
//in 'ls', or does it require us to build them?
if (FS->RenameFile(FS,OldName,NewName) )
{
	result=TRUE;
	Node=ListFindNamedItem(FS->DirListing,GetBasename(OldName));
	if (Node) 
	{
		FI=(TFileInfo *) Node->Item;
		if (FileStoreDestIsDirectory(FS, NewName))
		{
			ListDeleteNode(Node);
			FileInfoDestroy(FI);
		}
		else
		{
			FI->Name=CopyStr(FI->Name,NewName);
			FI->Path=CopyStr(FI->Path,NewName);
			Node->Tag=CopyStr(Node->Tag,NewName);
		}
	}
}


return(result);
}


int FileStoreAnalyzePath(char *Path)
{
char *ptr, *Token=NULL;
int result;

ptr=GetToken(Path,":",&Token,0);
if (! StrLen(Token)) result=FS_DISK;
result=MatchTokenFromList(Token,FileStoreTypeStrings,0);

DestroyString(Token);
return(result);
}


TFileStore *FileStoreCreate(char *Name, char *FSPath)
{
int result;
TFileStore *FS=NULL;
char *ptr, *Token=NULL, *Type=NULL;

//if no <fstype>: in path, then return null
if (! strchr(FSPath,':')) return(NULL);

GetToken(FSPath,":",&Type,0);
result=MatchTokenFromList(Type,FileStoreTypeStrings,0);


switch (result)
{
	case FS_SSH:
	case FS_SHELL:
		FS=SSHShellFileStoreCreate(Name,FSPath);
	break;

	case FS_TELNET:
		FS=TelnetShellFileStoreCreate(Name,FSPath);
	break;

	case FS_TTY:
		FS=SerialShellFileStoreCreate(Name,FSPath);
	break;

	case FS_FTP:
		FS=FtpFileStoreCreate(Name,FSPath);
	break; 

	case FS_POP3:
		FS=Pop3FileStoreCreate(Name,FSPath);
	break; 

	case FS_POP3S:
		FS=Pop3SFileStoreCreate(Name,FSPath);
	break; 

	case FS_HTTP:
	case FS_HTTPS:
		FS=HTTPFileStoreCreate(Name,FSPath);
	break;

	case FS_WEBDAV:
		FS=WebDAVFileStoreCreate(Name,FSPath);
	break;

	case FS_SWEBDAV:
		FS=SWebDAVFileStoreCreate(Name,FSPath);
	break;

	case FS_GSITES:
		FS=GSitesFileStoreCreate(Name,FSPath);
	break;

	case FS_PICASA:
		FS=PicasaFileStoreCreate(Name,FSPath);
	break;

	case FS_FAW:
		FS=FAWFileStoreCreate(Name,FSPath);
	break;

	case FS_RESTBACKUP:
		FS=RestBackupFileStoreCreate(Name,FSPath);
	break;

	case FS_IDRIVE:
		FS=IDriveFileStoreCreate(Name,FSPath);
	break;

	case FS_GDRIVE:
		FS=GDriveFileStoreCreate(Name,FSPath);
	break;


	case FS_DISK:
		FS=DiskFileStoreCreate(Name,FSPath);
	break;


	case FS_SKYDRIVE:
		FS=SkyDriveFileStoreCreate(Name,FSPath);
	break;
}

if (FS) 
{
	ParseConnectDetails(FSPath,&FS->Type,&FS->Host,&Token,&FS->Logon,&FS->Passwd,&FS->InitDir);

	if (StrLen(Token))
	{
	FS->Port=strtol(Token,&ptr,10);
	if (ptr) SetVar(FS->Vars,"ConnectSettings",ptr);
	}

	if (FS->BuffSize==0) FS->BuffSize=4096;
}

if (! StrLen(FS->Name)) FS->Name=MCopyStr(FS->Name,FS->Type,":",FS->Logon,"@",FS->Host,NULL);

DestroyString(Token);
DestroyString(Type);
return(FS);
}



void PrintQuota(TFileStore *FS)
{
double Total=0, Used=0, Percent;
char *ptr;

ptr=GetVar(FS->Vars,"QuotaTotal");
if (StrLen(ptr)) Total=atof(ptr);
ptr=GetVar(FS->Vars,"QuotaUsed");
if (StrLen(ptr)) Used=atof(ptr);

Percent=(Used/Total) * 100.0;

//Must print seperately as 'GetHumanReadable' returns a pointer to
//a static buffer
printf("QUOTA: %s ",GetHumanReadableDataQty(Used,0));
printf("of %s (%0.1f%%) used\n", GetHumanReadableDataQty(Total,0),Percent);
}

void PrintConnectionDetails(TFileStore *FS, STREAM *S)
{
char *ptr=NULL, *Token=NULL;

if (S)
{
  if (FS->Flags & FS_SSL)
  {
		ptr=STREAMGetValue(S,"SSL-Cipher");

		if (! StrLen(ptr))
		{
			 if (! SSLAvailable()) printf("ERROR: SSL not available (not compiled in?)\n");
			 else printf("ERROR: SSL Setup failed.\n"); 
		}
		else
		{
    	ptr=STREAMGetValue(S,"SSL-Certificate-Verify");
    	if (StrLen(ptr) && (strcmp(ptr,"OK")==0))
    	{
     	 printf("INFO: SECURE CONNECTION: Using Cipher: %s SSL Verified by %s\n",STREAMGetValue(S,"SSL-Cipher"), STREAMGetValue(S,"SSL-Certificate-Issuer"));
    	}
    	else
			{
			 printf("ERROR: SSL problem: %s. Connection may be unusable or insecure\n",ptr);
      ptr=STREAMGetValue(S,"SSL-Certificate-Issuer");
      if (StrLen(ptr)) printf("Using Cipher: %s SSL Certificate Issued by %s\n",STREAMGetValue(S,"SSL-Cipher"),ptr);
			}
		}
  }


	if (FS->Flags & FS_NO_FOLDERS) printf("INFO: This filestore does not support folders. 'mkdir' and 'chdir' commands will not work\n");

	if (FS->Features & FS_TRANSFER_TYPES) printf("INFO: Compression Types. %s\n",GetVar(FS->Vars,"CompressionTypes"));
	if (FS->Features & FS_FILE_HASHES) printf("INFO: File Hashing Available. %s\n",GetVar(FS->Vars,"HashTypes"));

	if (FS->Features & FS_RESUME_TRANSFERS) printf("INFO: Transfer Resume supported.\n");

	ptr=GetVar(FS->Vars,"QuotaTotal");
	if (StrLen(ptr)) PrintQuota(FS);

	ptr=GetVar(FS->Vars,"LoginBanner");
	if (StrLen(ptr)) 
	{
			ptr=GetToken(ptr,"\n",&Token,0);
			while (ptr)
			{
				StripTrailingWhitespace(Token);
				if (StrLen(Token))printf("%s\n",Token);
				ptr=GetToken(ptr,"\n",&Token,0);
			}
	}
}
 
DestroyString(Token);
}




char *MakeFullPath(char *RetStr,TFileStore *FS, char *FilePath)
{
char *Tempstr=NULL;

if (*FilePath != '/')
{
	Tempstr=MCopyStr(RetStr,FS->CurrDir,FilePath,NULL);
}
else Tempstr=CopyStr(RetStr,FilePath);

return(Tempstr);
}


int FileStoreChangeSetting(TFileStore *FS, int Flag, char *Value)
{
if (! (FS->Features & Flag)) return(FALSE);

if (strcasecmp(Value,"on")==0) FS->Settings |= Flag;
else if (strcasecmp(Value,"off")==0) FS->Settings &= ~Flag;
else if (strcasecmp(Value,"yes")==0) FS->Settings |= Flag;
else if (strcasecmp(Value,"no")==0) FS->Settings &= ~Flag;

if (FS->Flags & FS_RESTART_SETTINGS)
{
	FS->Close(FS);
	FS->Open(FS);
}

return(TRUE);
}


int StringHasSubtoken(char *String, char *Divider, char *Subtoken)
{
char *ptr, *Token=NULL;
int result=FALSE;

if (! String) return(FALSE);
ptr=GetToken(String,Divider,&Token,0);
while (ptr)
{
	if (strcasecmp(Subtoken,Token)==0)
	{
		result=TRUE;
		break;
	}
	ptr=GetToken(ptr,Divider,&Token,0);
}

DestroyString(Token);
return(result);
}


//This function decides what level of comparision between files or
//integrity checking filestores support when syncing etc
int FileStoreSelectCompareLevel(TFileStore *FromFS, TFileStore *ToFS, char *CompareLevel)
{
TFileStore *FS1, *FS2;
int result=CMP_EXISTS;
int i, v0=0, v1=0, v2=0;

if (! ToFS) return(CMP_EXISTS);
if (ToFS->Features & FromFS->Features & FS_FILE_SIZE) result=CMP_SIZE;

if (StrLen(CompareLevel))
{
	if (strcmp(CompareLevel,"size+time")==0) result=CMP_SIZE_TIME;
	else v0=GetCompareLevelTypes(CompareLevel);
}
else v0=CMP_MD5|CMP_SHA1;

if (ToFS->Features & FromFS->Features & FS_FILE_HASHES) 
{
	v1=GetCompareLevelTypes(GetVar(ToFS->Vars,"HashTypes"));
	v2=GetCompareLevelTypes(GetVar(FromFS->Vars,"HashTypes"));

	if (v0 & v1 & v2 & CMP_MD5) result=CMP_MD5;
	else if (v0 & v1 & v2 & CMP_SHA1) result=CMP_SHA1;
}

return(result);
}


char *FileStoreFormatPath(char *RetBuff, TFileStore *FS, char *Path)
{
char *RetStr=NULL;

if (*Path=='/') RetStr=CopyStr(RetBuff,Path);
else RetStr=MCopyStr(RetBuff,FS->CurrDir,"/",Path,NULL);

return(RetStr);
}
