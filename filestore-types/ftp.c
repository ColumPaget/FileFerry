#define _GNU_SOURCE
#include <fnmatch.h>

#include "ftp.h"

#define FTP_CONTINUE 1
#define FTP_OKAY 2
#define FTP_MORE 4
#define FTP_GOOD 7


//FTP responses must start with a 3-letter digit. If we don't even get that (so there's silence) we
//return FALSE here, and the app exits.
int FtpReadResponse(STREAM *S, char **ResponseCode, char **Verbiage, int RequiredResponse)
{
char *Tempstr=NULL;
int result=FALSE;

Tempstr=STREAMReadLine(Tempstr,S);

if (Settings.Flags & FLAG_VERBOSE) printf("%s\n",Tempstr);

if (StrLen(Tempstr) > 3)
{
*ResponseCode=CopyStrLen(*ResponseCode,Tempstr,3);
switch (**ResponseCode)
{
case '1': if (RequiredResponse & FTP_CONTINUE) result=TRUE; break;
case '2': if (RequiredResponse & FTP_OKAY) result=TRUE; break;
case '3': if (RequiredResponse & FTP_MORE) result=TRUE; break;
break;
}

if (Verbiage) *Verbiage=CopyStr(*Verbiage,Tempstr+4);
while ( (Tempstr[3]=='-') || (isspace(Tempstr[0])) )
{
	StripCRLF(Tempstr);

	if (Verbiage) 
	{
		if (Tempstr[3]=='-') *Verbiage=MCatStr(*Verbiage,"\n",Tempstr+4,NULL);
		else *Verbiage=MCatStr(*Verbiage,"\n",Tempstr,NULL);
	}
	Tempstr=STREAMReadLine(Tempstr,S);
}
}

DestroyString(Tempstr);

return(result);
}
        
  
int DecodePORTStr(char *PortStr, char **Address, int *Port)
{     
char *Tempstr=NULL, *ptr;                                                       int count;

for (count=0; count < StrLen(PortStr); count++) if (! isdigit(PortStr[count])) PortStr[count]=',';                                                              ptr=PortStr;
for (count=0; count < 4; count++)
{
 ptr=GetToken(ptr,",",&Tempstr,0);
 StripTrailingWhitespace(Tempstr); 
 *Address=CatStr(*Address,Tempstr);
 if (count < 3) *Address=CatStr(*Address,".");
}


 ptr=GetToken(ptr,",",&Tempstr,0);
 StripTrailingWhitespace(Tempstr);
 count=atoi(Tempstr);
 *Port=count;
 *Port=*Port << 8;
 
 ptr=GetToken(ptr,",",&Tempstr,0);
 StripTrailingWhitespace(Tempstr);
 count=atoi(Tempstr);
 *Port=*Port | count;

DestroyString(Tempstr);
return(TRUE);
}



STREAM *FtpMakePassiveDataConnection(TFileStore *FS)
{
char *Tempstr=NULL, *Address=NULL;
int Port;
char *sptr, *eptr;
int result;
STREAM *S=NULL;

STREAMWriteLine("PASV\r\n",FS->S);
STREAMFlush(FS->S);

Tempstr=STREAMReadLine(Tempstr,FS->S);

if (Tempstr)
{
if (*Tempstr=='1') Tempstr=STREAMReadLine(Tempstr,FS->S);
if (*Tempstr=='2')
{
   sptr=strrchr(Tempstr,'(');
   if (sptr)
   {
      sptr++;
      eptr=strrchr(sptr,')');
      if (eptr) *eptr=0;
      DecodePORTStr(sptr,&Address,&Port);
      S=STREAMCreate();
      if (! STREAMConnectToHost(S,Address,Port,FALSE))
      {
	      STREAMClose(S);
	      S=NULL;
      }
			else if (FS->Settings & FS_TRANSFER_TYPES)  STREAMAddStandardDataProcessor(S,"compression","zlib","");


  }
}
}

DestroyString(Tempstr);
DestroyString(Address);
return(S);
}



int FtpChDir(TFileStore *FS, char *Dir)
{
char *Tempstr=NULL;
int result=FALSE;

if (strcmp(Dir,"..")==0) STREAMWriteLine("CDUP\r\n",FS->S);
else
{
	Tempstr=FormatStr(Tempstr,"CWD %s\r\n",Dir);
	STREAMWriteLine(Tempstr,FS->S);
}
STREAMFlush(FS->S);

if (FtpReadResponse(FS->S,&Tempstr,NULL, FTP_OKAY))
{
		DefaultChDir(FS,Dir);
		result=TRUE;
}

DestroyString(Tempstr);

return(result);
}



int FtpGetDigest(TFileStore *FS, char *Path, char *Type, char **Digest)
{
char *Tempstr=NULL;
int result=FALSE;

Tempstr=FormatStr(Tempstr,"X%s '%s'\r\n",Type,Path);
STREAMWriteLine(Tempstr,FS->S);
STREAMFlush(FS->S);

if (FtpReadResponse(FS->S,&Tempstr,Digest, FTP_OKAY))
{
	StripTrailingWhitespace(*Digest);
	result=TRUE;
}

DestroyString(Tempstr);

return(result);
}


int FtpChMod(TFileStore *FS, TFileStore *FI, char *Mode)
{
char *Tempstr=NULL;
int result=FALSE;

Tempstr=FormatStr(Tempstr,"SITE CHMOD %s '%s'\r\n",Mode,FI->Name);
STREAMWriteLine(Tempstr,FS->S);
STREAMFlush(FS->S);

if (FtpReadResponse(FS->S,&Tempstr,NULL, FTP_OKAY)) result=TRUE;

DestroyString(Tempstr);

return(result);
}



int FtpMkDir(TFileStore *FS, char *Dir)
{
char *Tempstr=NULL, *EndLine=NULL;
int result=FALSE;

Tempstr=FormatStr(Tempstr,"MKD %s\r\n",Dir);
STREAMWriteLine(Tempstr,FS->S);
STREAMFlush(FS->S);

if (FtpReadResponse(FS->S,&Tempstr,NULL, FTP_OKAY)) result=TRUE;

DestroyString(Tempstr);

return(result);
}


int FtpSymlink(TFileStore *FS, char *FromPath, char *ToPath)
{
int result=FALSE;
char *Tempstr=NULL, *Verbiage=NULL;
 
Tempstr=MCopyStr(Tempstr,"SITE SYMLINK ",FromPath," ",ToPath,"\r\n",NULL);
STREAMWriteLine(Tempstr,FS->S);
STREAMFlush(FS->S);

if (FtpReadResponse(FS->S,&Tempstr,&Verbiage, FTP_OKAY))
{
	HandleEventMessage(Settings.Flags,"%s %s\n",Tempstr,Verbiage);
	result=TRUE;
}

DestroyString(Verbiage);
DestroyString(Tempstr);

return(result);
}



int FtpDeleteFile(TFileStore *FS, TFileInfo *FI)
{
char *Tempstr=NULL;
int result=FALSE;

if (FI->Type==FTYPE_DIR) Tempstr=FormatStr(Tempstr,"RMD %s\r\n",FI->Path);
else Tempstr=FormatStr(Tempstr,"DELE %s\r\n",FI->Path);
STREAMWriteLine(Tempstr,FS->S);
STREAMFlush(FS->S);
if (FtpReadResponse(FS->S,&Tempstr,NULL, FTP_OKAY)) result=TRUE;

DestroyString(Tempstr);

return(result);
}




TFileInfo *Decode_MLSD_Output(char *CurrDir, char *LsLine)
{
TFileInfo *FI;
char *FileFacts=NULL;
char *Name=NULL, *Value=NULL;
char *ptr;
char *FactNames[]={"type","modify","perm","size",NULL};
typedef enum {MLSD_TYPE, MLSD_MTIME, MLSD_PERM,MLSD_SIZE} TMLSD_FIELDS;
int val;

  if (! StrLen(LsLine)) return(NULL);
  FI=(TFileInfo *) calloc(1,sizeof(TFileInfo));
	ptr=GetToken(LsLine,"\\S",&FileFacts,0);
	FI->Path=CopyStr(FI->Path,CurrDir);
  FI->Path=CatStr(FI->Path,ptr);
  FI->Name=CopyStr(FI->Name,ptr);

  ptr=GetNameValuePair(FileFacts,";","=",&Name,&Value);
	while (ptr)
	{
		val=MatchTokenFromList(Name,FactNames,0);
		switch (val)
		{
			case MLSD_TYPE:
				if (strcasecmp(Value,"dir")==0) FI->Type=FTYPE_DIR;
				else FI->Type=FTYPE_FILE;
			break;

			case MLSD_SIZE:
				FI->Size=atoi(Value);
			break;

			case MLSD_MTIME:
				FI->Mtime=DateStrToSecs("%Y%m%d%H%M%S",Value,NULL);
			break;
		}
  	ptr=GetNameValuePair(ptr,";","=",&Name,&Value);
	}

DestroyString(FileFacts);
DestroyString(Name);
DestroyString(Value);

return(FI);
} 

int FtpHasFeature(TFileStore *FS, char *Feature)
{
char *Token=NULL, *ptr;
int result=FALSE;

ptr=GetVar(FS->Vars,"Features");
ptr=GetToken(ptr,",",&Token,0);
while (ptr)
{
	if (strcasecmp(Token,Feature)==0)
	{
		result=TRUE;
		break;
	}
ptr=GetToken(ptr,",",&Token,0);
}

DestroyString(Token);
return(result);
}


int FtpLoadDir(TFileStore *FS, char *InPattern, ListNode *Items, int Flags)
{
int result, MLSD=FALSE;
char *Tempstr=NULL;
TFileInfo *FI;
STREAM *S;

MLSD=FtpHasFeature(FS,"MLSD");
S=FtpMakePassiveDataConnection(FS);
if (! S) return(FALSE);

if (S)
{
	if (MLSD) STREAMWriteLine("MLSD\r\n",FS->S);
	else STREAMWriteLine("LIST\r\n",FS->S);
	STREAMFlush(FS->S);
	if (FtpReadResponse(FS->S,&Tempstr,NULL, FTP_CONTINUE))
	{
	Tempstr=STREAMReadLine(Tempstr,S);
	while (Tempstr)
	{
			StripTrailingWhitespace(Tempstr);	
			StripLeadingWhitespace(Tempstr);
	
			if (StrLen(Tempstr))
			{
				if (MLSD) FI=Decode_MLSD_Output(FS->CurrDir,Tempstr);
				else FI=Decode_LS_Output(FS->CurrDir,Tempstr);
	
				if (FI)
				{
					//we don't use absolute paths with ftp
					FI->Path=CopyStr(FI->Path,FI->Name);
					ListAddNamedItem(Items,FI->Name,FI);
				}
			}
	
	Tempstr=STREAMReadLine(Tempstr,S);
	}
		//Read 'End of transfer'
	FtpReadResponse(FS->S,&Tempstr,NULL,FTP_OKAY);
	}

STREAMClose(S);
}

DestroyString(Tempstr);

return(TRUE);
}




STREAM *FtpOpenFile(TFileStore *FS, TFileInfo *FI, int Flags)
{
char *Tempstr=NULL, *Verbiage=NULL;
STREAM *S;

S=FtpMakePassiveDataConnection(FS);
if (S)
{
   if (Flags & OPEN_WRITE)
   {
			STREAMSetFlushType(FS->S,FLUSH_ALWAYS,0);
   		Tempstr=FormatStr(Tempstr,"STOR %s\r\n",FI->Path);
   		STREAMWriteLine(Tempstr,FS->S);
   }
   else
   {
			if ((Flags & OPEN_RESUME) && (FI->ResumePoint > 0))
			{
   			Tempstr=FormatStr(Tempstr,"REST %d\r\n",FI->ResumePoint);
   			STREAMWriteLine(Tempstr,FS->S);
				printf("%s\n",Tempstr);
				FtpReadResponse(FS->S,&Tempstr,&Verbiage, FTP_OKAY);
				printf("REST %s\n",Tempstr);
			}
   		Tempstr=FormatStr(Tempstr,"RETR %s\r\n",FI->Path);
   		STREAMWriteLine(Tempstr,FS->S);
   }
	STREAMFlush(FS->S);
}

if (! FtpReadResponse(FS->S,&Tempstr,&Verbiage, FTP_CONTINUE | FTP_OKAY))
{
	Tempstr=MCatStr(Tempstr," ",Verbiage,NULL);
	WriteLog(Tempstr);
	STREAMClose(S);
	S=NULL;
}

DestroyString(Tempstr);
DestroyString(Verbiage);

 return(S);
}



int FtpReadBytes(TFileStore *FS, STREAM *S, char *Buffer, int BuffLen)
{
int result;
char *ptr;

result=ReadBytesToTerm(S, Buffer, BuffLen, '\n');
if (result > 0)
{
   Buffer[result]=0; 
   ptr=Buffer+(result-StrLen(FS->EndLine)-1);
   if (strncmp(ptr,FS->EndLine,StrLen(FS->EndLine))==0) 
   {
	   result-=StrLen(FS->EndLine);
	   FS->EndLine=CopyStr(FS->EndLine,"");
   }
}

return(result);
}





int FtpCloseFile(TFileStore *FS, STREAM *S)
{
char *Tempstr=NULL, *Verbiage=NULL;
int RetVal=FALSE;

STREAMClose(S);
if (FtpReadResponse(FS->S,&Tempstr,&Verbiage, FTP_OKAY)) RetVal=TRUE;
else switch(atoi(Tempstr))
{
case 452: RetVal=ERR_FULL; break;
case 530:
case 532: RetVal=ERR_FORBID; break;
case 550: RetVal=ERR_NOEXIST; break;
case 552: RetVal=ERR_TOOBIG; break;
case 553: RetVal=ERR_BADNAME; break;
}

DestroyString(Tempstr);
DestroyString(Verbiage);
return(RetVal);
}


//discover features supported
void FtpReadFeatures(TFileStore *FS)
{
char *Tempstr=NULL, *Verbiage=NULL, *Token=NULL, *FeatureList=NULL, *ptr;

	STREAMWriteLine("FEAT\r\n",FS->S);
	STREAMFlush(FS->S);

	if (FtpReadResponse(FS->S,&Tempstr,&Verbiage, FTP_GOOD))
	{
	//First line will be 'Features follow' or something like that
	ptr=GetToken(Verbiage,"\n",&Token,0);
	ptr=GetToken(ptr,"\n",&Token,0);
	while (ptr)
	{
		StripLeadingWhitespace(Token);
		StripTrailingWhitespace(Token);
		if (StrLen(Token))
		{
			if (StrLen(FeatureList)) FeatureList=MCatStr(FeatureList,", ",Token,NULL);
			else FeatureList=CatStr(FeatureList,Token);

			if (strcmp(Token,"MODE Z")==0) FS->Features |= FS_TRANSFER_TYPES;
			if (strcmp(Token,"REST STREAM")==0) FS->Features |= FS_RESUME_TRANSFERS;
			if (strcmp(Token,"XMD5")==0) 
			{
				FS->Features |= FS_FILE_HASHES;
				Tempstr=MCopyStr(Tempstr,GetVar(FS->Vars,"HashTypes"),"md5 ",NULL);
				SetVar(FS->Vars,"HashTypes",Tempstr);
			}
			if (strcmp(Token,"XSHA")==0) 
			{
				FS->Features |= FS_FILE_HASHES;
				Tempstr=MCopyStr(Tempstr,GetVar(FS->Vars,"HashTypes"),"sha1 ",NULL);
				SetVar(FS->Vars,"HashTypes",Tempstr);
			}

			if (strcmp(Token,"SITE SYMLINK")==0) FS->Features |= FS_SYMLINKS;
			if (strcmp(Token,"SITE CHMOD")==0) FS->ChMod=FtpChMod;

		}
		ptr=GetToken(ptr,"\n",&Token,0);
	}
	SetVar(FS->Vars,"Features",FeatureList);
	}


DestroyString(FeatureList);
DestroyString(Verbiage);
DestroyString(Tempstr);
DestroyString(Token);
}


int FtpClose(TFileStore *FS)
{
int result;

   STREAMWriteLine("QUIT\r\n",FS->S);
   STREAMClose(FS->S);

return(TRUE);
}


int FtpRenameFile(TFileStore *FS, char *FromPath, char *ToPath)
{
int result=FALSE;
char *Tempstr=NULL, *Verbiage=NULL, *FileName=NULL;
char *CorrectedToPath=NULL, *ptr;
TFileInfo *FI;
 
Tempstr=MCopyStr(Tempstr,"RNFR ",FromPath,"\r\n",NULL);
STREAMWriteLine(Tempstr,FS->S);
STREAMFlush(FS->S);
if (FtpReadResponse(FS->S,&Tempstr,NULL,FTP_OKAY|FTP_MORE))
{
	ptr=strrchr(FromPath,'/');
	if (ptr) ptr++;
	else ptr=FromPath;
	
	FileName=CopyStr(FileName,ptr);
	
	//Handle '.' meaning 'move to current dir'
	if (strcmp(ToPath,".")==0)
	{
		CorrectedToPath=CopyStr(CorrectedToPath,ptr);
	}
	else if (strcmp(ToPath,"..")==0) CorrectedToPath=MCopyStr(CorrectedToPath,"../",FileName,NULL);
	else 
	{
		FI=FileStoreGetFileInfo(FS,ToPath);
		if (FI && (FI->Type==FTYPE_DIR)) 
		{
			CorrectedToPath=CopyStr(CorrectedToPath,ToPath);
			CorrectedToPath=TerminateDirectoryPath(CorrectedToPath);
			CorrectedToPath=CatStr(CorrectedToPath,FileName);
		}
		else CorrectedToPath=CopyStr(CorrectedToPath,ToPath);
	}
	
	
	Tempstr=MCopyStr(Tempstr,"RNTO ",CorrectedToPath,"\r\n",NULL);
	
	STREAMWriteLine(Tempstr,FS->S);
	STREAMFlush(FS->S);
	if (FtpReadResponse(FS->S,&Tempstr,&Verbiage, FTP_OKAY))
	{
		HandleEventMessage(Settings.Flags,"%s %s\n",Tempstr,Verbiage);
		result=TRUE;
	}
}

DestroyString(CorrectedToPath);
DestroyString(FileName);
DestroyString(Verbiage);
DestroyString(Tempstr);

return(result);
}





int FTPLogin(TFileStore *FS)
{
char *Tempstr=NULL, *Verbiage=NULL, *LoginBanner=NULL;
int result=FALSE;

	LoginBanner=CopyStr(LoginBanner,GetVar(FS->Vars,"LoginBanner"));

	Tempstr=FormatStr(Tempstr,"USER %s\r\n",FS->Logon);
	STREAMWriteLine(Tempstr,FS->S);
	STREAMFlush(FS->S);

	FtpReadResponse(FS->S,&Tempstr,&Verbiage, FTP_OKAY | FTP_CONTINUE);
	if (strcmp(Tempstr,"230")==0)
	{
		LoginBanner=CatStr(LoginBanner,Verbiage);
		result=TRUE;
	}
	else
	{
		if (strcmp(Tempstr,"331")==0)
		{
			Tempstr=FormatStr(Tempstr,"PASS %s\r\n",FS->Passwd);
			STREAMWriteLine(Tempstr,FS->S);
			STREAMFlush(FS->S);

			FtpReadResponse(FS->S,&Tempstr,&Verbiage, FTP_OKAY);
			if (strcmp(Tempstr,"230")==0)
			{
					if (StrLen(Verbiage))
					{
						if (StrLen(Verbiage) > 4) LoginBanner=MCatStr(LoginBanner,"\n",Verbiage,NULL);
						if (! (FS->Flags & FS_FEATURES_READ)) FtpReadFeatures(FS);
					}
  				result=TRUE;
			}
			else HandleEventMessage(Settings.Flags,"LOGIN ERROR: %s",Verbiage);
		}	
	}


SetVar(FS->Vars,"LoginBanner",LoginBanner);

DestroyString(LoginBanner);
DestroyString(Tempstr);
DestroyString(Verbiage);

return(result);
}


int FtpOpen(TFileStore *FS)
{
char *Tempstr=NULL, *Verbiage=NULL, *Token=NULL, *LoginBanner=NULL, *ptr;
int result=FALSE;

if (! FS) 
{
return(FALSE);
}

FS->S=STREAMCreate();

if (STREAMConnectToHost(FS->S,FS->Host,FS->Port,0))
{
	STREAMSetTimeout(FS->S,30);
	FtpReadResponse(FS->S,&Tempstr,&Verbiage, FTP_OKAY);
	if (StrLen(Verbiage) > 4) LoginBanner=CopyStr(LoginBanner,Verbiage);
	//SetVar(FS->Vars,"Greeting",Verbiage);

	if (StrLen(Tempstr) && (*Tempstr=='2'))
	{
		if (! (FS->Flags & FS_FEATURES_READ))
		{
			FtpReadFeatures(FS);
			ptr=GetVar(FS->Vars,"Features");
			if (StrLen(ptr)) LoginBanner=MCatStr(LoginBanner,"\nFeatures: ",ptr,NULL);
			result=FTPLogin(FS);
			while (! result)
			{
				FS->Logon=CopyStr(FS->Logon,"");
				FS->Passwd=CopyStr(FS->Passwd,"");
				RequestAuthDetails(&FS->Logon,&FS->Passwd);
				result=FTPLogin(FS);
			}
		}
	}
 }

 FS->Flags |= FS_FEATURES_READ;

if (! result)
{
	STREAMClose(FS->S);
	FS->S=NULL;
}
else
{
	//binary transfers
	STREAMWriteLine("TYPE I\r\n",FS->S);
	STREAMFlush(FS->S);
	FtpReadResponse(FS->S,&Tempstr,NULL, FTP_OKAY);

	if (
				(FS->Settings & FS_TRANSFER_TYPES) &&
				(DataProcessorAvailable("Compression","zlib"))
			)
	{
	STREAMWriteLine("MODE Z\r\n",FS->S);
	}
	else STREAMWriteLine("MODE S\r\n",FS->S);

	STREAMFlush(FS->S);
	FtpReadResponse(FS->S,&Tempstr,NULL, FTP_OKAY);

	if (StrLen(FS->InitDir))  FtpChDir(FS, FS->InitDir);
	PrintConnectionDetails(FS,FS->S);
}

SetVar(FS->Vars,"LoginBanner",LoginBanner);

DestroyString(Tempstr);
DestroyString(Verbiage);
DestroyString(LoginBanner);
DestroyString(Token);

return(result);
}



TFileStore *FtpFileStoreCreate(char *Name, char *ConnectSetup)
{
TFileStore *FS;

FS=(TFileStore *) calloc(1,sizeof(TFileStore));
FS->Flags=FS_WRITEABLE | FS_RESTART_SETTINGS | FS_CHDIR_FULLPATH;
FS->Features=FS_FILE_SIZE;
FS->Vars=ListCreate();
FS->CurrDir=CopyStr(FS->CurrDir,"");
FS->InitArg=CopyStr(FS->InitArg,ConnectSetup);
FS->Name=CopyStr(FS->Name,Name);
FS->Port=21;


FS->Create=FtpFileStoreCreate;
FS->Open=FtpOpen;
FS->Close=FtpClose;
FS->ChDir=FtpChDir;
FS->MkDir=FtpMkDir;
FS->LoadDir=FtpLoadDir;
FS->OpenFile=FtpOpenFile;
FS->CloseFile=FtpCloseFile;
FS->ReadBytes=DefaultReadBytes;
FS->WriteBytes=DefaultWriteBytes;
FS->RenameFile=FtpRenameFile;
FS->Symlink=FtpSymlink;
FS->DeleteFile=FtpDeleteFile;
FS->GetFileSize=FileStoreGetFileSize;
FS->GetDigest=FtpGetDigest;

return(FS);
}
