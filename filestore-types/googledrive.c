#define _GNU_SOURCE
#include <fnmatch.h>

#include "google-data.h"
#include "googledrive.h"


char *JSONReadList(char *RetStr, STREAM *S)
{
char *Data=NULL, *Tempstr=NULL, *ptr;

			Data=CopyStr(RetStr,"");
			Tempstr=STREAMReadLine(Tempstr,S);
			StripTrailingWhitespace(Tempstr);
			StripLeadingWhitespace(Tempstr);
			while (*Tempstr != '}')
			{
			Data=CatStr(Data,Tempstr);
			Tempstr=STREAMReadLine(Tempstr,S);
			StripTrailingWhitespace(Tempstr);
			StripLeadingWhitespace(Tempstr);
			}

DestroyString(Tempstr);

return(Data);
}


void GDriveGetQuota(TFileStore *FS)
{
char *Tempstr=NULL, *Name=NULL, *Value=NULL, *ptr;
HTTPInfoStruct *Info;

//About user and seetings
Tempstr=CopyStr(Tempstr,"https://www.googleapis.com/drive/v2/about");
Info=GDataConnect(FS,"GET", Tempstr,"", NULL, 0);
if (Info)
{
  Tempstr=STREAMReadLine(Tempstr,Info->S);
  while (Tempstr)
  { 
	ptr=GetToken(Tempstr,":",&Name,GETTOKEN_QUOTES);
	ptr=GetToken(ptr,",",&Value,GETTOKEN_QUOTES);
	StripTrailingWhitespace(Name);
	StripLeadingWhitespace(Name);
	StripTrailingWhitespace(Value);
	StripLeadingWhitespace(Value);

	switch (*Name)
	{
		case 'q':
			if (strcmp(Name,"quotaBytesTotal")==0) FS->BytesAvailable=atoi(Value);
			if (strcmp(Name,"quotaBytesUsed")==0) FS->BytesUsed=atoi(Value);
		break;
	}
  Tempstr=STREAMReadLine(Tempstr,Info->S);
  } 
  //STREAMClose(Info->S);
  HTTPInfoDestroy(Info);
}

DestroyString(Tempstr);
DestroyString(Name);
DestroyString(Value);
}



STREAM *GDriveOpenFile(TFileStore *FS, TFileInfo *FI, int Flags)
{
HTTPInfoStruct *Info;
ListNode *Headers;
char *Boundary=NULL, *JSON=NULL, *Tempstr=NULL;


Headers=ListCreate();
FS->Extra=NULL;

if (Flags & OPEN_WRITE)
{
	Boundary=FormatStr(Boundary,"BOUNDARY-%d-%d-%d",getpid(),time(NULL),rand());
	SetVar(FS->Vars,"HTTP-BOUNDARY",Boundary);
	Tempstr=MCopyStr(Tempstr,"multipart/related; boundary=",Boundary,NULL);
	SetVar(Headers,"Content-Type",Tempstr);
	
	JSON=MCopyStr(JSON,"--",Boundary,"\r\nContent-Type: application/json\r\n\r\n","{\r\n\"title\": \"",FI->Name,"\"\r\n}\r\n--",Boundary,"\r\nContent-Type: ",FI->MediaType,"\r\n\r\n",NULL);
	
	Tempstr=FormatStr(Tempstr,"%d",StrLen(JSON)+FI->Size+StrLen(Boundary)+8);
	SetVar(Headers,"Content-Length",Tempstr);
	
	Info=GDataConnect(FS,"POST", "https://www.googleapis.com/upload/drive/v2/files?uploadType=multipart",NULL, Headers, 0);
	STREAMWriteBytes(Info->S,JSON,StrLen(JSON));
}
else Info=GDataConnect(FS,"GET", FI->Path,"", NULL, 0);

FS->Extra=(void *) Info;
DestroyString(JSON);
DestroyString(Tempstr);
DestroyString(Boundary);
ListDestroy(Headers,DestroyString);

return(Info->S);
}

int GDriveCloseFile(TFileStore *FS, STREAM *S)
{ 
	char *Tempstr=NULL, *Response=NULL;
	HTTPInfoStruct *Info;
	int RetVal=FALSE;
            
	Info=(HTTPInfoStruct *) FS->Extra;

	//if doing a post
	if (S->Flags & SF_WRONLY)
	{
		Tempstr=MCopyStr(Tempstr,"\r\n--",GetVar(FS->Vars,"HTTP-BOUNDARY"),"--\r\n",NULL);
		STREAMWriteLine(Tempstr,Info->S);
		STREAMFlush(Info->S);

		S=HTTPTransact(Info);
		if (S)
		{
				Tempstr=STREAMReadLine(Tempstr,S);
				while (Tempstr)
				{
					Response=HtmlDeQuote(Response,Tempstr);
					Tempstr=STREAMReadLine(Tempstr,S);
				}
				STREAMClose(S);

		}
	}

	//Must do this here, because might have done HTTPTransact above if writing/post
	RetVal=HTTPParseResponse(Info->ResponseCode);

	if (! RetVal) 
	{
		SetVar(FS->Vars,"Error",Response);
		RetVal=ERR_CUSTOM;
	}

	//if we destroy http info, we must set FS->Extra to NULL
	HTTPInfoDestroy(Info);
	DestroyString(Tempstr);
	DestroyString(Response);

	FS->Extra=NULL;

	return(RetVal);
}



TFileInfo *GDriveParseFileInfo(TFileStore *FS, STREAM *S)
{
char *Tempstr=NULL, *Data=NULL, *Name=NULL, *Value=NULL, *ptr;
TFileInfo *FI;

FI=(TFileInfo *) calloc(1,sizeof(TFileInfo));
Tempstr=STREAMReadLine(Tempstr,S);
while (Tempstr)
{

	ptr=GetToken(Tempstr,":",&Name,GETTOKEN_QUOTES);
	ptr=GetToken(ptr,",",&Value,GETTOKEN_QUOTES);
	StripTrailingWhitespace(Name);
	StripLeadingWhitespace(Name);
	StripTrailingWhitespace(Value);
	StripLeadingWhitespace(Value);

	//don't expect just '}', can be '},'
	if (*Name=='}') break;
	if ((strcmp(Name,"{")==0) || (strcmp(Value,"{")==0)) Data=JSONReadList(Data,S);
	switch (*Name)
	{
		case 'd':
			if (strcmp(Name,"downloadUrl")==0) FI->Path=CopyStr(FI->Path,Value);
		break;


		case 'f':
			if (strcmp(Name,"fileSize")==0) FI->Size=atoi(Value);
		break;

		case 'i':
			if (strcmp(Name,"id")==0) FI->ID=CopyStr(FI->ID,Value);
			if (StrLen(FI->Path)==0) 
			{
				FI->Path=MCopyStr(FI->Path, "https://docs.google.com/feeds/download/documents/export/Export?id=",Value,"&exportFormat=txt",NULL);
			}
		break;

		case 'm':
			if (strcmp(Name,"md5Checksum")==0) FI->Hash=CopyStr(FI->Hash,Value);
			else if (strcmp(Name,"mimeType")==0) FI->MediaType=CopyStr(FI->MediaType,Value);
			else if (strcmp(Name,"modifiedDate")==0) FI->Mtime=DateStrToSecs("%Y-%m-%dT%H:%M:%S.",Value,NULL);
		break;


		case 't':
			if (strcmp(Name,"title")==0) 
			{
				FI->Name=CopyStr(FI->Name,Value);
			}
		break;

	}	

	Tempstr=STREAMReadLine(Tempstr,S);
}

DestroyString(Tempstr);
DestroyString(Name);
DestroyString(Value);
DestroyString(Data);

return(FI);
}



int GDriveLoadDir(TFileStore *FS, char *InPattern, ListNode *Items, int Flags)
{
int result;
char *Tempstr=NULL, *Name=NULL, *Value=NULL, *ptr;
TFileInfo *FI;
HTTPInfoStruct *Info;

//Info=GDataConnect(FS,"GET", "https://www.googleapis.com/drive/v2/files/root/children","", NULL, 0);
Info=GDataConnect(FS,"GET", "https://www.googleapis.com/drive/v2/files","", NULL, 0);
Tempstr=STREAMReadLine(Tempstr,Info->S);
while (Tempstr)
{
StripTrailingWhitespace(Tempstr);
StripLeadingWhitespace(Tempstr);
ptr=GetToken(Tempstr,":",&Name,GETTOKEN_QUOTES);
ptr=GetToken(ptr,",",&Value,GETTOKEN_QUOTES);


if (Name && Value)
{
if ((strcmp(Name,"kind")==0)  && (strcmp(Value,"drive#file")==0)) 
{
	FI=GDriveParseFileInfo(FS,Info->S);
	if (Items) ListAddNamedItem(Items,FI->Name,FI);
}
}

Tempstr=STREAMReadLine(Tempstr,Info->S);
}

HTTPInfoDestroy(Info);
GDriveGetQuota(FS);

DestroyString(Tempstr);
DestroyString(Name);
DestroyString(Value);
return(TRUE);
}





int GDriveRenameFile(TFileStore *FS, char *FromPath, char *ToPath)
{
char *Tempstr=NULL, *XML=NULL, *ptr;
TFileInfo *FI, *Dest;
HTTPInfoStruct *Info;
ListNode *Headers;

Dest=FileStoreGetFileInfo(FS,GetBasename(ToPath));
if (Dest)
{
FI=FileStoreGetFileInfo(FS,FromPath);

Headers=ListCreate();
//"<id>https://docs.google.com/feeds/default/private/full/document%3Adocument_id</id>"
XML=MCopyStr(XML,"<?xml version='1.0' encoding='UTF-8'?>", 
"<atom:entry xmlns:atom=\"http://www.w3.org/2005/Atom\">", 
"<atom:id>",FI->Path,"</atom:id>",
"</atom:entry>\n",NULL);

SetVar(Headers,"Content-type","application/atom+xml");
Tempstr=FormatStr(Tempstr,"%d",StrLen(XML));
SetVar(Headers,"Content-length",Tempstr);

Info=GDataConnect(FS,"POST", Dest->Path, XML,Headers,0);

XML=STREAMReadDocument(XML,Info->S,FALSE);
}

ListDestroy(Headers,DestroyString);
if (Info) HTTPInfoDestroy(Info);
DestroyString(Tempstr);
DestroyString(XML);
DestroyString(Tempstr);
}


void GDriveMkDir(TFileStore *FS, char *Title)
{
char *JSON=NULL, *Tempstr=NULL;
HTTPInfoStruct *Info;
ListNode *Headers;

Headers=ListCreate();

JSON=MCopyStr(JSON,"{\n\"title\": \"",Title,"\",\n\"mimeType\": \"application/vnd.google-apps.folder\"\n}\n",NULL);

//"parents": [{"id":"0ADK06pfg"}]

SetVar(Headers,"Content-type","application/json");
Tempstr=FormatStr(Tempstr,"%d",StrLen(JSON));
SetVar(Headers,"Content-length",Tempstr);

Tempstr=CopyStr(Tempstr,"https://www.googleapis.com/drive/v2/files");
Info=GDataConnect(FS,"POST", Tempstr, JSON,Headers,0);

JSON=STREAMReadDocument(JSON,Info->S,FALSE);

printf("%s\n",JSON);

DestroyString(Tempstr);
DestroyString(JSON);

ListDestroy(Headers,DestroyString);
if (Info) HTTPInfoDestroy(Info);
}


int GDriveDeleteFile(TFileStore *FS, TFileInfo *FI)
{
char *Tempstr=NULL;
HTTPInfoStruct *Info;
int result=FALSE;

Tempstr=MCopyStr(Tempstr,"https://www.googleapis.com/drive/v2/files/",FI->ID,NULL);
Info=GDataConnect(FS,"DELETE", Tempstr, NULL, NULL,0);

if (Info->ResponseCode && (*Info->ResponseCode=='2')) result=TRUE;

Tempstr=STREAMReadLine(Tempstr,Info->S);
while (Tempstr)
{
Tempstr=STREAMReadLine(Tempstr,Info->S);
}

HTTPInfoDestroy(Info);
DestroyString(Tempstr);

return(result);
}


int GDriveOpen(TFileStore *FS)
{
HTTPInfoStruct *Info;
char *Tempstr=NULL;
int result=FALSE;

if (! FS) 
{
return(FALSE);
}

FS->S=STREAMCreate();

if (! StrLen(FS->Passwd)) 
{
	SetVar(FS->Vars,"OAuthScope","https://www.googleapis.com/auth/drive");
	GoogleOAuth(FS);
	Tempstr=MCopyStr(Tempstr, "Bearer ", FS->Passwd, NULL);
}
else if (FS->Settings & FS_OAUTH) Tempstr=MCopyStr(Tempstr, "Bearer ", FS->Passwd, NULL);

if (StrLen(Tempstr))
{
SetVar(FS->Vars,"AuthToken",Tempstr);
result=TRUE;

//Use 'about' to get connection details
Tempstr=CopyStr(Tempstr,"https://www.googleapis.com/drive/v2/about");
Info=GDataConnect(FS,"GET", Tempstr,"", NULL, 0);
if (Info) 
{
	PrintConnectionDetails(FS,Info->S);
	Tempstr=STREAMReadLine(Tempstr,Info->S);
	while (Tempstr)
	{
		StripTrailingWhitespace(Tempstr);
		Tempstr=STREAMReadLine(Tempstr,Info->S);
	}
	//STREAMClose(Info->S);
	HTTPInfoDestroy(Info);
}
}

DestroyString(Tempstr);

return(result);
}


int GDriveClose(TFileStore *FS)
{
int result;
char *Tempstr=NULL;


   DestroyString(Tempstr);
return(TRUE);
}




TFileStore *GDriveFileStoreCreate(char *Name, char *ConnectSetup)
{
TFileStore *FS;

FS=(TFileStore *) calloc(1,sizeof(TFileStore));
FS->Flags=FS_WRITEABLE | FS_SSL;
FS->Features=FS_FILE_SIZE;
FS->Vars=ListCreate();
FS->CurrDir=CopyStr(FS->CurrDir,"");


FS->Create=GDriveFileStoreCreate;
FS->Open=GDriveOpen;
FS->Close=GDriveClose;
//FS->ChDir=GDriveChDir;
FS->LoadDir=GDriveLoadDir;
FS->DeleteFile=GDriveDeleteFile;

FS->OpenFile=GDriveOpenFile;
FS->ReadBytes=DefaultReadBytes;
FS->WriteBytes=DefaultWriteBytes;
FS->CloseFile=GDriveCloseFile;

FS->MkDir=GDriveMkDir;

/*
FS->RenameFile=GDriveRenameFile;
FS->GetFileSize=FileStoreGetFileSize;
*/

FS->InitArg=CopyStr(FS->InitArg,ConnectSetup);
FS->Name=CopyStr(FS->Name,Name);

return(FS);
}
