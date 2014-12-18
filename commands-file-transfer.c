#include "commands-file-transfer.h"

//need this for CMD_ IDs
#include "commands-parse.h"
#include "common.h"
#include <sys/ioctl.h>


#define SKIP_EXISTS -1
#define SKIP_AGE -2
#define SKIP_FILETYPE -3

typedef struct
{
TFileStore *SrcFS;
TFileStore *DestFS;
int Command;
int CmdFlags;
int Throttle;
int CompareLevel;
int IntegrityLevel;
unsigned long Transferred;
unsigned long Failed;
unsigned long Total;
double TotalBytes;
char *ActionName;
char *IncludePattern;
char *ExcludePattern;
char *PreCopyHook;
char *PostCopyHook;
ListNode *Vars;
} TTransferContext;


TTransferContext *TransferContextCreate(TFileStore *SrcFS, TFileStore *DestFS, int Command, int CmdFlags, ListNode *Vars)
{
TTransferContext *Ctx;
char *ptr;

Ctx=(TTransferContext *) calloc(1,sizeof(TTransferContext));
Ctx->SrcFS=SrcFS;
Ctx->DestFS=DestFS;
Ctx->Command=Command;
Ctx->CmdFlags=CmdFlags;
Ctx->Vars=Vars;

ptr=GetVar(Vars,"Throttle");
if (ptr) Ctx->Throttle=atoi(ptr);
Ctx->IncludePattern=CopyStr(Ctx->IncludePattern,GetVar(Vars,"IncludePattern"));
Ctx->ExcludePattern=CopyStr(Ctx->ExcludePattern,GetVar(Vars,"ExcludePattern"));
Ctx->PostCopyHook=CopyStr(Ctx->PostCopyHook,GetVar(Vars,"PostCopyHook"));
Ctx->PreCopyHook=CopyStr(Ctx->PreCopyHook,GetVar(Vars,"PreCopyHook"));

if (Command == CMD_PUT) Ctx->ActionName=CopyStr(Ctx->ActionName,"put");
else if (Command == CMD_CREATE) Ctx->ActionName=CopyStr(Ctx->ActionName,"create");
else Ctx->ActionName=CopyStr(Ctx->ActionName,"get");

ptr=GetVar(Vars,"CompareLevel");
Ctx->CompareLevel=FileStoreSelectCompareLevel(SrcFS,DestFS,ptr);
ptr=GetVar(Vars,"IntegrityLevel");
if (StrLen(ptr)) Ctx->IntegrityLevel=FileStoreSelectCompareLevel(SrcFS,DestFS,ptr);


return(Ctx);
}

void TransferContextDestroy(TTransferContext *Ctx)
{
DestroyString(Ctx->IncludePattern);
DestroyString(Ctx->ExcludePattern);
DestroyString(Ctx->ActionName);

free(Ctx);
}


void DisplayTransferStatus(char *Line, unsigned int transferred, unsigned int total, unsigned int *percent, unsigned int secs,int CmdFlags, int Throttle)
{
int result=0, cols=80, bps=0;
char *Tempstr=NULL, *TimeStr=NULL, *ptr=NULL;
static int ThrotCount=0;
struct winsize w;


	if (Settings.Flags & FLAG_QUIET) return;
	if (CmdFlags & FLAG_QUIET) return;
	if (! isatty(1)) return;

	ptr=getenv("COLUMNS");
	if (ptr) cols=atoi(ptr);
	printf("\r");
	if (total==0) Tempstr=FormatStr(Tempstr,"%s bytes sent ",GetHumanReadableDataQty(transferred,0));
	else
	{
	result=(transferred * 100) / total;
//	if (result != *percent)
	{
		Tempstr=FormatStr(Tempstr,"%d%% %s of ",result,GetHumanReadableDataQty(transferred,0));
		Tempstr=CatStr(Tempstr,GetHumanReadableDataQty(total,0));
	}
	*percent=result;
	}

bps=transferred / secs;
TimeStr=FormatStr(TimeStr," in %d secs %s Bps ",secs,GetHumanReadableDataQty(bps,0) );

if ((Throttle > 0) && (bps > Throttle)) 
{
	ThrotCount++;
	usleep(ThrotCount * 250000);
}
else if (ThrotCount > 0) ThrotCount--;

if (ThrotCount > 0) Tempstr=MCatStr(Tempstr,TimeStr, Line, " (throttling)      ",NULL);
else Tempstr=MCatStr(Tempstr,TimeStr, Line, "           ",NULL);


ioctl(0, TIOCGWINSZ, &w);
if (StrLen(Tempstr) > w.ws_col) Tempstr[w.ws_col]='\0';

printf("%s",Tempstr);
fflush(NULL);

DestroyString(TimeStr);
DestroyString(Tempstr);
}



int AlterFileExtn(TFileStore *FS, int CmdFlags, char *FName, char *NewExtn)
{
 char *NewName=NULL;
 int result;

 NewName=PathChangeExtn(NewName, FName, NewExtn);
 result=FileStoreRename(FS,FName,NewName);
printf("ALTER: %s %s\n",FName,NewName);
 if (! result) HandleEventMessage(Settings.Flags,"FAIL: Cannot rename %s -> %s",FName,NewName);	
 else if (CmdFlags & FLAG_CMD_VERBOSE) HandleEventMessage(Settings.Flags,"OKAY: Renamed %s -> %s",FName,NewName);	

 DestroyString(NewName);
 return(result);
}


int ProcessRename(TFileStore *FS, char *From, char *To, int CmdFlags, ListNode *Vars)
{
ListNode *Items, *Curr;
TFileInfo *FI, *DestFI=NULL;
int result, RetVal=ERR_NOFILES;
char *Tempstr=NULL, *ptr;


if (! FS->RenameFile) 
{
	 printf("Filestore does not support renaming files\n");
	return(FALSE);
}

Items=ListCreate();
/*
FileStoreLoadDir(FS,To,Items,0);
Curr=ListGetNext(Items);
if (Curr) DestFI=(TFileInfo *) Curr->Item;
ListClear(Items,NULL);
*/
	
FileStoreLoadDir(FS,From,Items,0);
Curr=ListGetNext(Items);

	//If we don't know about these items, we have to hope that
	//the server we are talking to does
	if (Curr==NULL)
	{
			if (FileStoreRename(FS,From,To))
			{
				FileStoreLoadDir(FS,"",NULL, LIST_REFRESH|LIST_INCLUDE_DIRS);
				RetVal=TRANSFER_OKAY;
			}
			else RetVal=ERR_FAIL;
	}
	else
	{
	while (Curr)
	{
		FI=(TFileInfo *) Curr->Item;

		if (DestFI && (DestFI->Type==FTYPE_DIR))
		{
			//Tempstr=MCopyStr(Tempstr,DestFI->Path,"/",FI->Name,NULL); 
			Tempstr=MCopyStr(Tempstr,DestFI->Path,NULL); 
		}
		else  Tempstr=CopyStr(Tempstr,To);

		if (CmdFlags) result=AlterFileExtn(FS,CmdFlags,FI->Path,To);
		else result=FileStoreRename(FS,FI->Path,Tempstr);

		if (result)
		{
			if ((RetVal==ERR_NOFILES) ||(RetVal==TRANSFER_OKAY )) RetVal=TRANSFER_OKAY;
			else RetVal=ERR_PARTIAL;
		}
		else
		{
			if (RetVal==TRANSFER_OKAY) RetVal=ERR_PARTIAL;
			else RetVal=ERR_FAIL;
		}

		Curr=ListGetNext(Curr);
	}
	}


	ListDestroy(Items,FileInfoDestroy);

	FileInfoDestroy(DestFI);

DestroyString(Tempstr);

return(RetVal);
}



char *TransferDecidePermissions(TFileInfo *SrcFI)
{
if (StrLen(SrcFI->Permissions)) return(SrcFI->Permissions);
if (SrcFI->Type==FTYPE_DIR) return(GetVar(Settings.Vars,"dirmode"));
if (SrcFI->Type==FTYPE_EXE) return(GetVar(Settings.Vars,"exemode"));
return(GetVar(Settings.Vars,"filemode"));
}


STREAM *InternalCopyOpenSource(TTransferContext *Ctx, TFileInfo *SrcFI)
{
STREAM *S;
char *ptr;
int val=0;

//Specify the DESTINATION size in the source, for use in 'resume' transfers
if (Ctx->DestFS->GetFileSize) SrcFI->ResumePoint=Ctx->DestFS->GetFileSize(Ctx->DestFS, SrcFI->Name);

//Set version info if getting a particular version of file
ptr=GetVar(Ctx->Vars,"Version");
SetVar(SrcFI->Vars,"Version,",ptr);

if (
	(Ctx->CmdFlags & FLAG_CMD_RESUME) &&
	(Ctx->SrcFS->Features & FS_RESUME_TRANSFERS) &&
	(Ctx->DestFS->Features & FS_RESUME_TRANSFERS) 
) val |= OPEN_RESUME;

S=Ctx->SrcFS->OpenFile(Ctx->SrcFS,SrcFI, val);
//OPEN SOURCE DONE

//Construct 'write' args. THESE ARE ARGS TO OPEN THE DEST FILE

if (S)
{
	ptr=STREAMGetValue(S,"filesize");
	if (StrLen(ptr)) SrcFI->Size=atoi(ptr);
	else if (Ctx->SrcFS->GetFileSize) SrcFI->Size=Ctx->SrcFS->GetFileSize(Ctx->SrcFS, SrcFI->Path);
}

return(S);
}



STREAM *InternalCopyOpenDest(TTransferContext *Ctx, TFileInfo *SrcFI, TFileInfo *DestFI)
{
STREAM *S;
char *ptr;
int val=0;

	//For the destination we are writing the file into the current directory, so never use a full path
	//use the name of the source file as the path
	if (Ctx->CmdFlags & FLAG_CMD_EXTN_TEMP) DestFI->Path=PathChangeExtn(DestFI->Path, SrcFI->Name, GetVar(Ctx->Vars,"TransferTempExtn"));
	else DestFI->Path=CopyStr(DestFI->Path, SrcFI->Name);
	DestFI->Permissions=CopyStr(DestFI->Permissions,TransferDecidePermissions(SrcFI));

	ptr=GetVar(Ctx->Vars,"ContentType");
	if (StrLen(ptr)) DestFI->MediaType=CopyStr(DestFI->MediaType,ptr);
	else FileStoreGetFileType(Ctx->SrcFS, DestFI);

	val=OPEN_WRITE;
	if (Ctx->CmdFlags & FLAG_CMD_PUBLIC) val |= OPEN_PUBLIC;
	if (
			(Ctx->CmdFlags & FLAG_CMD_RESUME) &&
			(Ctx->SrcFS->Features & FS_RESUME_TRANSFERS) &&
			(Ctx->DestFS->Features & FS_RESUME_TRANSFERS) 
		) val |= OPEN_RESUME;

	S=Ctx->DestFS->OpenFile(Ctx->DestFS,DestFI, val);

return(S);
}


TProcessingModule *ActivateCompression(TFileStore *FS1, TFileStore *FS2)
{
char *p_CompTypes1, *p_CompTypes2;
char *Token1=NULL, *Token2=NULL, *Selection=NULL;
TProcessingModule *CompressionMod=NULL;

	p_CompTypes1=GetVar(FS1->Vars,"CompressionTypes");
	p_CompTypes2=GetVar(FS2->Vars,"CompressionTypes");

	p_CompTypes1=GetToken(p_CompTypes1," ",&Token1,0);
	while (p_CompTypes1)
	{
		p_CompTypes2=GetToken(p_CompTypes2," ",&Token2,0);
		while (p_CompTypes2)
		{
			if (strcmp(Token1,Token2)==0) 
			{
				Selection=CopyStr(Selection,Token1);
				break;
			}
			if (StrLen(Selection)) break;
			p_CompTypes2=GetToken(p_CompTypes2," ",&Token2,0);
		}
		p_CompTypes1=GetToken(p_CompTypes1," ",&Token1,0);
	}

	printf("CompSelect: %s\n",Selection);
	if (StrLen(Selection)) CompressionMod=StandardDataProcessorCreate("Compression",Selection,"");

DestroyString(Token1);
DestroyString(Token2);
DestroyString(Selection);

return(CompressionMod);
}


int HandleCompression(TProcessingModule *CompressionMod, char *Buffer, int Bytes)
{


}


int InternalCopyFile(TTransferContext *Ctx, TFileInfo *iSrcFI)
{
STREAM *Src=NULL, *Dest=NULL;
TFileInfo *SrcFI=NULL, *DestFI=NULL;
char *Buffer=NULL, *Tempstr=NULL, *ptr;
int BuffSize=4096;
int result, bytes=0, towrite=0; 
unsigned int  TotalBytes=0, BytesSent=0, percent=0, secs=0, val;
double bigval;
//TFileInfo *FI;
int RetVal=TRANSFER_OKAY;
struct timeval StartTime, Now;
TProcessingModule *CompressionMod=NULL;

if (! Ctx->SrcFS->ReadBytes) return(ERR_READ_NOTSUPPORTED);
if (Ctx->DestFS && (! Ctx->DestFS->WriteBytes)) return(ERR_WRITE_NOTSUPPORTED);


gettimeofday(&StartTime,NULL); 


if (Ctx->DestFS && Ctx->DestFS->BuffSize) BuffSize=Ctx->DestFS->BuffSize;
Buffer=SetStrLen(Buffer,BuffSize*2+1);


SrcFI=FileInfoClone(iSrcFI);
DestFI=FileInfoClone(iSrcFI);
DestFI->Path=FileStoreFormatPath(DestFI->Path, Ctx->DestFS, iSrcFI->Name);

//if a PreCopyHook script is specified, then run it
if (StrLen(Ctx->PreCopyHook))
{
	Tempstr=FormatStr(Tempstr,"%s '%s' '%s' '%s'",Ctx->PreCopyHook,DestFI->Name,DestFI->Path,"");
	system(Tempstr);
}


Src=InternalCopyOpenSource(Ctx, SrcFI);

TotalBytes=SrcFI->Size;
if (Ctx->CmdFlags & FLAG_CMD_RESUME) BytesSent=SrcFI->ResumePoint;

if (Src)
{
	if (Ctx->SrcFS->Settings & FS_COMPRESSION) CompressionMod=ActivateCompression(Ctx->DestFS,Ctx->SrcFS);


	if (Ctx->DestFS) Dest=InternalCopyOpenDest(Ctx, SrcFI, DestFI);
	else Dest=STREAMFromFD(1);

	if (Dest)
	{

		result=Ctx->SrcFS->ReadBytes(Ctx->SrcFS,Src,Buffer,BuffSize*2);
		while ((bytes > 0) || (result != EOF))
		{
			if (CompressionMod) result=HandleCompression(CompressionMod,Buffer,result);
			if (result > 0) bytes+=result;
			if (bytes > BuffSize) towrite=BuffSize;
			else towrite=bytes;

			if (Ctx->DestFS) result=Ctx->DestFS->WriteBytes(Ctx->DestFS,Dest,Buffer,towrite);
			else result=STREAMWriteBytes(Dest,Buffer,towrite);

			if (result==-1)
			{
				RetVal=ERR_INTERRUPTED;
				break;
			}


			if (Settings.Flags & FLAG_INTERRUPT)
			{
				Settings.Flags &= ~FLAG_INTERRUPT;
				RetVal=ERR_CANCEL;
				break;
			}

			BytesSent+=result;
			bytes-=result;
			if (bytes > 0) memmove(Buffer,Buffer+result,bytes);
			
			gettimeofday(&Now,NULL);
			val=Now.tv_sec - StartTime.tv_sec;
			if (val > secs)
			{
				secs=val;
				if (secs > 0) DisplayTransferStatus(SrcFI->Name,BytesSent,TotalBytes,&percent,secs,Ctx->CmdFlags,Ctx->Throttle);
			}

			result=Ctx->SrcFS->ReadBytes(Ctx->SrcFS,Src,Buffer+bytes,BuffSize*2 -bytes);
		}

		if (Ctx->SrcFS->CloseFile) Ctx->SrcFS->CloseFile(Ctx->SrcFS,Src);

		if (Ctx->DestFS && Ctx->DestFS->CloseFile) 
		{
			result=Ctx->DestFS->CloseFile(Ctx->DestFS,Dest);
			if (result !=TRUE)
			{
				//Handle Errors returned by Close file
				if (RetVal >= FALSE) RetVal=result;
				SetVar(Ctx->Vars,"Error",GetVar(Ctx->DestFS->Vars,"Error"));
				SetVar(Ctx->DestFS->Vars,"Error","");
			}
			else ListAddNamedItem(Ctx->DestFS->DirListing,DestFI->Name,FileInfoClone(DestFI));
		}
		else STREAMFlush(Dest);

	if ((TotalBytes > 0) && (BytesSent < TotalBytes)) RetVal=ERR_INTERRUPTED;
	}
	else RetVal=ERR_DESTFILE;

secs=Now.tv_sec - StartTime.tv_sec;
if (secs < 1) secs=1;
DisplayTransferStatus(SrcFI->Name,BytesSent,TotalBytes,&percent,secs,0,0);
//if (! (CmdFlags & FLAG_QUIET)) 

printf("\n");
}
else RetVal=ERR_SOURCEFILE;

Ctx->TotalBytes+=BytesSent;


FileInfoDestroy(DestFI);
FileInfoDestroy(SrcFI);
DestroyString(Buffer);
DestroyString(Tempstr);

return(RetVal);
}




void HandleTransferResult(TTransferContext *Ctx, TFileInfo *FI, int result)
{
TFileInfo *NewFI;
char *Tempstr=NULL, *ResultStr=NULL;
int val;

//A 'show' command might result in this being called with no 'Ctx->DestFS'
if (! Ctx->DestFS) return;


switch (result)
{
	case TRANSFER_OKAY:
 		NewFI=FileInfoClone(FI);
		NewFI->Path=MCopyStr(NewFI->Path,Ctx->DestFS->CurrDir,NewFI->Name,NULL);

		ListAddNamedItem(Ctx->DestFS->DirListing,FI->Name,NewFI);
		if (FI->Type==FTYPE_DIR) 
		{
			if (Ctx->CmdFlags & FLAG_CMD_QUIET) ResultStr=CopyStr(ResultStr,"");
			else ResultStr=FormatStr(ResultStr,"OKAY: %s directory '%s'",Ctx->ActionName,FI->Name);
		}
		else
		{
			if (Settings.Flags & FLAG_INTEGRITY_CHECK) val=FileStoreCompareFile(Ctx->SrcFS,Ctx->DestFS,FI,NewFI,Ctx->IntegrityLevel);
			else val=CMP_EXISTS;

			if (val==CMP_FAIL) 
			{
				ResultStr=FormatStr(ResultStr,"FAIL: %s '%s' integrity check failed",Ctx->ActionName,FI->Name);
				result=FALSE;
			}
			else if (val==CMP_MD5) ResultStr=FormatStr(ResultStr,"OKAY: %s '%s' confirmed with MD5",Ctx->ActionName,FI->Name);
			else if (val==CMP_SHA1) ResultStr=FormatStr(ResultStr,"OKAY: %s '%s' confirmed with SHA1",Ctx->ActionName,FI->Name);
			else if (val==CMP_SIZE) ResultStr=FormatStr(ResultStr,"OKAY: %s '%s' confirmed with filesize",Ctx->ActionName,FI->Name);
			else if (val==CMP_SIZE_TIME) ResultStr=FormatStr(ResultStr,"OKAY: %s '%s' confirmed with filesize & time",Ctx->ActionName,FI->Name);
			else ResultStr=FormatStr(ResultStr,"OKAY: %s '%s' complete",Ctx->ActionName, FI->Name);
		}

  	if (Ctx->CmdFlags & FLAG_CMD_EXTN_TEMP)
		{
			ResultStr=PathChangeExtn(ResultStr, FI->Name, GetVar(Ctx->Vars,"TransferTempExtn"));
			//Different sense than AlterFileExtn! We rename to FI->Name, not from it
			FileStoreRename(Ctx->DestFS,ResultStr,FI->Name);
 			if (Ctx->CmdFlags & FLAG_CMD_VERBOSE) HandleEventMessage(Settings.Flags,"Renaming: %s -> %s",ResultStr,FI->Name);	
		}
  	if (Ctx->CmdFlags & FLAG_CMD_EXTN_DEST) AlterFileExtn(Ctx->DestFS,Ctx->CmdFlags,FI->Name,GetVar(Ctx->Vars,"TransferDestExtn"));

		//Use FI->Path here, because local files can be referred to by full paths
 		if (Ctx->CmdFlags & FLAG_CMD_EXTN_SOURCE) AlterFileExtn(Ctx->SrcFS,Ctx->CmdFlags,FI->Path,GetVar(Ctx->Vars,"TransferSourceExtn"));

		/*
 		if (Ctx->CmdFlags & FLAG_CMD_CHMOD) 
		{
			if (FI->Type==FTYPE_DIR) FileStoreChMod(Ctx->DestFS, FI, GetVar(Ctx->Vars,"DChMod"));
			else FileStoreChMod(Ctx->DestFS, FI, GetVar(Ctx->Vars,"FChMod"));
		}
		*/
	break;


	case ERR_FORBID:
		ResultStr=FormatStr(ResultStr,"FAIL: %s '%s' FORBIDDEN. Insufficient Permissions.",Ctx->ActionName,FI->Name);
	break;

	case ERR_LOCKED:
		ResultStr=FormatStr(ResultStr,"FAIL: %s '%s' LOCKED. File locked.",Ctx->ActionName, FI->Name);
	break;

	case ERR_EXISTS:
		ResultStr=FormatStr(ResultStr,"FAIL: %s '%s' CONFLICT. Cannot overwrite.",Ctx->ActionName,FI->Name);
	break;

	case ERR_TOOBIG:
		ResultStr=FormatStr(ResultStr,"FAIL: %s '%s' TOO BIG. File too big.",Ctx->ActionName,FI->Name);
	break;

	case ERR_FULL:
		ResultStr=FormatStr(ResultStr,"FAIL: %s '%s' FULL. Insufficient storage.",Ctx->ActionName,FI->Name);
	break;

	case ERR_RATELIMIT:
		ResultStr=FormatStr(ResultStr,"FAIL: %s '%s' REFUSED. Rate Limiting in effect. Try later.",Ctx->ActionName,FI->Name);
	break;

	case ERR_BADNAME:
		ResultStr=FormatStr(ResultStr,"FAIL: %s '%s' Bad filename.",Ctx->ActionName,FI->Name);
	break;

	case ERR_SOURCEFILE:
		ResultStr=FormatStr(ResultStr,"FAIL: %s '%s' Cannot open source file for upload",Ctx->ActionName,FI->Name);
	break;

	case ERR_DESTFILE: 
		ResultStr=FormatStr(ResultStr,"FAIL: %s '%s' Cannot open destination file for writing",Ctx->ActionName,FI->Name);
	break;

	case ERR_INTERRUPTED:
		ResultStr=FormatStr(ResultStr,"FAIL: %s '%s' Transfer interrupted.",Ctx->ActionName,FI->Name);
	break;

	case ERR_READ_NOTSUPPORTED:
		ResultStr=FormatStr(ResultStr,"FAIL: %s '%s' Filestore does not support read/get.",Ctx->ActionName,FI->Name);
	break;

	case ERR_WRITE_NOTSUPPORTED:
		ResultStr=FormatStr(ResultStr,"FAIL: %s '%s' Filestore does not support write/put.",Ctx->ActionName,FI->Name);
	break;

	case ERR_CUSTOM:
		ResultStr=FormatStr(ResultStr,"FAIL: %s '%s' %s.",Ctx->ActionName,FI->Name,GetVar(Ctx->Vars,"Error"));
	break;


	default:
		ResultStr=FormatStr(ResultStr,"FAIL: %s '%s' Unknown error.",Ctx->ActionName,FI->Name);
	break;


}

if (StrLen(ResultStr)) HandleEventMessage(Settings.Flags,ResultStr);

if (result==TRANSFER_OKAY)
{
  if (StrLen(Ctx->PostCopyHook))
  {
    Tempstr=FormatStr(Tempstr,"%s '%s' '%s' '%s'",Ctx->PostCopyHook,FI->Name,FI->Path,ResultStr);
    system(Tempstr);
  }
}


DestroyString(ResultStr);
DestroyString(Tempstr);
}








int FileNeedsTransfer(TTransferContext *Ctx, TFileInfo *SrcFI, TFileInfo *DestFI)
{
switch (SrcFI->Type)
{
case FTYPE_CHARDEV:
case FTYPE_BLKDEV:
case FTYPE_SOCKET:
case FTYPE_LINK:
 return(SKIP_FILETYPE); 
break;

case FTYPE_DIR:
if (Ctx->CmdFlags & FLAG_CMD_RECURSE) return(TRUE);
break;
}

//Things that can't be 'switched'
if ((SrcFI->Type != FTYPE_DIR) && (! CheckFileMtime(SrcFI, Ctx->CmdFlags, Ctx->Vars))) return(SKIP_AGE);
if (! (Ctx->CmdFlags & FLAG_CMD_SYNC)) return(TRUE);

if (FileStoreCompareFile(Ctx->SrcFS,Ctx->DestFS,SrcFI,DestFI,Ctx->CompareLevel) < Ctx->CompareLevel) return(TRUE);

return(SKIP_EXISTS);
}


int InternalTransferDir(TTransferContext *Ctx, char *Path, char *Pattern)
{
int result=ERR_SOURCEFILE;
char *Dir=NULL, *DestRoot=NULL, *SrcRoot=NULL, *ptr;

  if (Ctx->DestFS) DestRoot=CopyStr(DestRoot,Ctx->DestFS->CurrDir);
  SrcRoot=CopyStr(SrcRoot,Ctx->SrcFS->CurrDir);

	ptr=GetToken(Path,"/",&Dir,NULL);
	while (ptr)
	{
		if (Ctx->SrcFS->ChDir(Ctx->SrcFS,Dir))
		{
			if (Ctx->DestFS)
			{
				Ctx->DestFS->MkDir(Ctx->DestFS,Dir);
				if (! Ctx->DestFS->ChDir(Ctx->DestFS,Dir)) break;
			}
			result=TRANSFER_OKAY; //set this so we know we entered dir okay
		}
		else break;

		ptr=GetToken(ptr,"/",&Dir,NULL);
	}

  if (result==TRANSFER_OKAY)
  {
    //if ChDir successed then CurrDir will hold the path of the directory we entered
    HandleEventMessage(Settings.Flags,"INFO: Entering Directory: %s",Ctx->SrcFS->CurrDir);
    result=InternalTransferFiles(Ctx, Pattern);
  }
  else HandleEventMessage(Settings.Flags,"ERROR: Failed to Enter Directory: '%s' in %s",Dir,Path);	

	if (Ctx->DestFS) Ctx->DestFS->ChDir(Ctx->DestFS,DestRoot);
	Ctx->SrcFS->ChDir(Ctx->SrcFS,SrcRoot);

DestroyString(DestRoot);
DestroyString(SrcRoot);
DestroyString(Dir);

return(result);
}


//This accepts a single pattern to match against. If that includes a space, it is thought to be a filename
//with a space in. To supply multiple patterns look at "ProcessPutFiles" below.
int InternalTransferFiles(TTransferContext *Ctx, char *Pattern)
{
ListNode *SrcItems=NULL, *DestItems=NULL, *Curr, *Node;
TFileInfo *FI, *NewFI, *DestFI;
int result=FALSE, IncludeResult=0;
char *Tempstr=NULL, *ptr;
int val;

//if we've been given a path to something a few directories down, then do this
if (Ctx->CmdFlags & FLAG_CMD_FULLPATH)
{
	ptr=strrchr(Pattern,'/');
	if (StrLen(ptr) > 1)
	{
 		Tempstr=CopyStrLen(Tempstr,Pattern,ptr-Pattern);
		ptr++;
		result=InternalTransferDir(Ctx, Tempstr, ptr);
		DestroyString(Tempstr);
		return(result);
	}
}


SrcItems=ListCreate();
SrcItems->Flags |= LIST_FLAG_CASE;

if (Ctx->CmdFlags & FLAG_CMD_SYNC)
{
	DestItems=ListCreate();
	DestItems->Flags |= LIST_FLAG_CASE;
	FileStoreLoadDir(Ctx->DestFS, "*", DestItems, LIST_REFRESH | Ctx->CompareLevel);
	FileStoreLoadDir(Ctx->SrcFS, Pattern, SrcItems, LIST_REFRESH | Ctx->CompareLevel);
}
else FileStoreLoadDir(Ctx->SrcFS,Pattern,SrcItems, LIST_REFRESH);


Curr=ListGetNext(SrcItems);
if (! Curr) result=FALSE;

while (Curr)
{
	FI=(TFileInfo *) Curr->Item;


	Node=ListFindNamedItem(DestItems,FI->Name);
	if (Node) DestFI=(TFileInfo *) Node->Item;
	else DestFI=NULL;

	if (FileIncluded(FI,Ctx->IncludePattern,Ctx->ExcludePattern,Ctx->CmdFlags, Ctx->Vars))
	{
	IncludeResult=FileNeedsTransfer(Ctx, FI, DestFI);
	switch (IncludeResult)
	{
		case TRUE:
		if (FI->Type==FTYPE_DIR) 
		{
				result=InternalTransferDir(Ctx, FI->Name, "*");
				if (result != TRANSFER_OKAY) Ctx->Failed++;
		}
		else 
		{
			if ((FI->Type==FTYPE_LINK))
			{
 				if (Ctx->DestFS->Settings & FS_SYMLINKS) result=Ctx->DestFS->Symlink(Ctx->DestFS, FI->Name, FI->EditPath);
			 	else result=InternalTransferDir(Ctx, FI->Name, "*");
			}
			else result=InternalCopyFile(Ctx, FI);

			HandleTransferResult(Ctx, FI, result);
			if (result >= TRANSFER_OKAY) Ctx->Transferred++;
			else Ctx->Failed++;
		}
		break;

		case SKIP_EXISTS:
			if (Ctx->CmdFlags & FLAG_CMD_VERBOSE) HandleEventMessage(Settings.Flags,"INFO: '%s' exists, skipping.",FI->Name); 
		break;

		case SKIP_AGE:
			if (Ctx->CmdFlags & FLAG_CMD_VERBOSE) HandleEventMessage(Settings.Flags,"INFO: '%s' doesn't match required age, skipping.",FI->Name); 
		break;

		case SKIP_FILETYPE:
			if (Ctx->CmdFlags & FLAG_CMD_VERBOSE) HandleEventMessage(Settings.Flags,"INFO: '%s' unsupported filetype, skipping.",FI->Name); 
		break;

	}
	Ctx->Total++;
	}

	if (result==ERR_CANCEL) break;
	Curr=ListGetNext(Curr);
}

ListDestroy(DestItems,FileInfoDestroy);
ListDestroy(SrcItems,FileInfoDestroy);
DestroyString(Tempstr);

return(result);
}


int ProcessTransferFiles(TFileStore *SrcFS, TFileStore *DestFS, int Command, char *Pattern, int CmdFlags, ListNode *Vars)
{
TTransferContext *Ctx;
char *Tempstr=NULL;
int result;

Ctx=TransferContextCreate(SrcFS, DestFS, Command, CmdFlags, Vars);

	result=InternalTransferFiles(Ctx, Pattern);
	Tempstr=FormatStr(Tempstr,"%d",Ctx->Transferred);
	SetVar(Vars,"Transferred",Tempstr);

	Tempstr=FormatStr(Tempstr,"%d",Ctx->Failed);
	SetVar(Vars,"Failed",Tempstr);

	Tempstr=FormatStr(Tempstr,"%d",Ctx->Total);
	SetVar(Vars,"Total",Tempstr);


DestroyString(Tempstr);
TransferContextDestroy(Ctx);

return(result);
}


//This accepts a single pattern to match against. If that includes a space, it is thought to be a filename
//with a space in. To supply multiple patterns look at "ProcessPutFiles" below.
int ProcessCreateFile(TFileStore *FS, char *Name, ListNode *Vars)
{
TFileInfo *FI;
int result;
char *ptr;
STREAM *S;
TTransferContext *Ctx;

FI=FileInfoCreate(Name, Name, FTYPE_FILE, 0, 0);

ptr=GetVar(Vars,"ContentType");
if (StrLen(ptr)) FI->MediaType=CopyStr(FI->MediaType,ptr);
else FileStoreGetFileType(FS, FI);

S=FS->OpenFile(FS, FI, OPEN_WRITE);
result=FS->CloseFile(FS,S);
if (result != TRANSFER_OKAY) 
{
	Ctx=TransferContextCreate(NULL, FS, 0, 0, Vars);
	HandleTransferResult(Ctx,FI, result);
	TransferContextDestroy(Ctx);
}

FileInfoDestroy(FI);
return(result);
}

