#define _GNU_SOURCE
#include <fnmatch.h>

#include "pop3.h"
#include "simple-inet-protocols.h"

int Pop3DeleteFile(TFileStore *FS, TFileInfo *FI)
{
char *Tempstr=NULL;
int result=FALSE;

Tempstr=MCopyStr(Tempstr,"DELE ",FI->Path,"\r\n",NULL);
STREAMWriteLine(Tempstr,FS->S);
STREAMFlush(FS->S);
if (InetReadResponse(FS->S,&Tempstr,NULL, INET_OKAY)) result=TRUE;

DestroyString(Tempstr);

return(result);
}





int Pop3HasFeature(TFileStore *FS, char *Feature)
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


int Pop3LoadDir(TFileStore *FS, char *InPattern, ListNode *Items, int Flags)
{
int result=FALSE;
char *Tempstr=NULL, *Token=NULL, *ptr;
TFileInfo *FI;


//	STREAMWriteLine("LIST\r\n",FS->S);
	STREAMWriteLine("UIDL\r\n",FS->S);
	STREAMFlush(FS->S);
	if (InetReadResponse(FS->S,&Tempstr,NULL, INET_OKAY))
	{
	Tempstr=STREAMReadLine(Tempstr,FS->S);
	while (Tempstr)
	{
		result=TRUE;
		StripTrailingWhitespace(Tempstr);	
		StripLeadingWhitespace(Tempstr);
		if (strcmp(Tempstr,".")==0) break;
	
		FI=(TFileInfo *) calloc(1,sizeof(TFileInfo));
		ptr=GetToken(Tempstr,"\\S",&FI->Path,0);
		ptr=GetToken(ptr,"\\S",&Token,0);

		//clean off <>
		ptr=strrchr(Token,'>');
		if (ptr) *ptr='\0';
		ptr=Token;
		if (*ptr=='<') ptr++;		
		FI->Name=MCopyStr(FI->Name, FI->Path, " ", ptr, NULL);


		if ((! StrLen(InPattern)) || (fnmatch(InPattern, FI->Path, 0)==0)) 
		{
			ListAddNamedItem(Items,FI->Path,FI);
		}
		else FileInfoDestroy(FI);
	
	Tempstr=STREAMReadLine(Tempstr,FS->S);
	}
	}

DestroyString(Tempstr);
DestroyString(Token);

return(result);
}




STREAM *Pop3OpenFile(TFileStore *FS, TFileInfo *FI, int Flags)
{
char *Tempstr=NULL, *Verbiage=NULL;
STREAM *S;

if (Flags & OPEN_WRITE) return(NULL);
Tempstr=FormatStr(Tempstr,"RETR %s\r\n",FI->Path);
STREAMWriteLine(Tempstr,FS->S);
STREAMFlush(FS->S);

if (InetReadResponse(FS->S,&Tempstr,&Verbiage, INET_OKAY)) S=FS->S;
else
{
	Tempstr=MCatStr(Tempstr," ",Verbiage,NULL);
	WriteLog(Tempstr);
	S=NULL;
}

DestroyString(Tempstr);
DestroyString(Verbiage);

 return(S);
}



int Pop3ReadBytes(TFileStore *FS, STREAM *S, char *Buffer, int BuffLen)
{
int result;

result=ReadBytesToTerm(S, Buffer, BuffLen, '\n');
if (result > 0)
{
   Buffer[result]=0;
   if (strcmp(Buffer,".\r\n")==0) result=EOF;
}
else result=EOF;

return(result);
}





int Pop3CloseFile(TFileStore *FS, STREAM *S)
{
return(TRUE);
}


//discover features supported
void Pop3ReadFeatures(TFileStore *FS)
{
char *Tempstr=NULL, *Verbiage=NULL, *Token=NULL, *FeatureList=NULL, *ptr;

	STREAMWriteLine("FEAT\r\n",FS->S);
	STREAMFlush(FS->S);

	if (InetReadResponse(FS->S,&Tempstr,&Verbiage, INET_GOOD))
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


int Pop3Close(TFileStore *FS)
{
int result=FALSE;

	STREAMWriteLine("QUIT\r\n",FS->S);
	if (InetReadResponse(FS->S, NULL, NULL, INET_OKAY)) result=TRUE;
	STREAMClose(FS->S);

return(result);
}




int Pop3Open(TFileStore *FS)
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
	InetReadResponse(FS->S,&Tempstr,&Verbiage, INET_OKAY);
	if (StrLen(Verbiage) > 4) LoginBanner=CopyStr(LoginBanner,Verbiage);
	SetVar(FS->Vars,"LoginBanner",LoginBanner);

	Tempstr=MCopyStr(Tempstr,"USER ",FS->Logon,"\r\n",NULL);
	STREAMWriteLine(Tempstr,FS->S);

	if (InetReadResponse(FS->S,&Tempstr,&Verbiage, INET_OKAY))
	{
		Tempstr=MCopyStr(Tempstr,"PASS ",FS->Passwd,"\r\n",NULL);
		STREAMWriteLine(Tempstr,FS->S);
		if (InetReadResponse(FS->S,&Tempstr,&Verbiage, INET_OKAY)) result=TRUE;
	}

}

DestroyString(Tempstr);
DestroyString(Verbiage);
DestroyString(LoginBanner);
DestroyString(Token);

return(result);
}



TFileStore *Pop3FileStoreCreate(char *Name, char *ConnectSetup)
{
TFileStore *FS;

FS=(TFileStore *) calloc(1,sizeof(TFileStore));
//FS->Features=FS_FILE_SIZE;
FS->Vars=ListCreate();
FS->CurrDir=CopyStr(FS->CurrDir,"");
FS->InitArg=CopyStr(FS->InitArg,ConnectSetup);
FS->Name=CopyStr(FS->Name,Name);
FS->Port=110;


FS->Create=Pop3FileStoreCreate;
FS->Open=Pop3Open;
FS->Close=Pop3Close;
FS->LoadDir=Pop3LoadDir;
FS->OpenFile=Pop3OpenFile;
FS->CloseFile=Pop3CloseFile;
FS->ReadBytes=Pop3ReadBytes;
FS->DeleteFile=Pop3DeleteFile;
//FS->GetFileSize=FileStoreGetFileSize;

return(FS);
}

TFileStore *Pop3SFileStoreCreate(char *Name, char *ConnectSetup)
{
TFileStore *FS;

FS=Pop3FileStoreCreate(Name, ConnectSetup);
FS->Create=Pop3SFileStoreCreate;
FS->Flags |= FS_SSL;
FS->Port=995;
}
