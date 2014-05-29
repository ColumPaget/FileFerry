#define _GNU_SOURCE
#include <fnmatch.h>

#include "skydrive.h"


/*
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

*/


/*
SD: [{^M   "id": "folder.8d458c461c4417fc", ^M   "from": {^M      "name": null, ^M      "id": null^M   }, ^M   "name": "SkyDrive", ^M   "description": "", ^M   "parent_id": null, ^M   "size": 0, ^M   "upload_location": "https://apis.live.net/v5.0/folder.8d458c461c4417fc/files/", ^M   "comments_count": 0, ^M   "comments_enabled": false, ^M   "is_embeddable": false, ^M   "count": 5, ^M   "link": "https://skydrive.live.com?cid=8d458c461c4417fc", ^M   "type": "folder", ^M   "shared_with": {^M      "access": "Just me"^M   }, ^M   "created_time": null, ^M   "updated_time": "2014-02-07T18:50:16+0000", ^M   "client_updated_time": "2014-02-07T18:50:16+0000"^M}]
*/


TFileInfo *SkyDriveParseFile(char *JSON)
{
char *Tempstr=NULL, *Name=NULL, *Value=NULL, *ptr, *ptr2;
TFileInfo *FI;
 

FI=(TFileInfo *) calloc(1,sizeof(TFileInfo));
//SkyDrive JSON is horrible. It's terminated with \r
ptr=GetToken(JSON,"\r",&Tempstr,0);
while (ptr)
{
StripLeadingWhitespace(Tempstr);
StripTrailingWhitespace(Tempstr);

ptr2=Tempstr+StrLen(Tempstr)-1;
if (*ptr2==',') *ptr2='\0';

ptr2=GetToken(Tempstr,":",&Name,GETTOKEN_QUOTES);
while (isspace(*ptr2)) ptr2++;
ptr2=GetToken(ptr2,":",&Value,GETTOKEN_QUOTES);

printf("%s -> %s\n",Name,Value);

if (strcmp(Name,"name")==0)
{
	if (strcmp(Value,"SkyDrive")==0) FI->Name=CopyStr(FI->Name,"/");
	else FI->Name=CopyStr(FI->Name,Value);
}

if (strcmp(Name,"type")==0)
{
	if (strcmp(Value,"folder")==0) FI->Type=FTYPE_DIR;
	else FI->Type=FTYPE_FILE;
}

if (strcmp(Name,"id")==0) FI->ID=CopyStr(FI->ID,Value);
if (strcmp(Name,"size")==0) FI->Size=atoi(Value);
if (strcmp(Name,"upload_location")==0) FI->EditPath=CopyStr(FI->EditPath,Value);
if (strcmp(Name,"link")==0) FI->Path=CopyStr(FI->Path,Value);

ptr=GetToken(ptr,"\r",&Tempstr,0);
}

DestroyString(Tempstr);
DestroyString(Name);
DestroyString(Value);

return(FI);
}


int SkyDriveLoadDir(TFileStore *FS, char *InPattern, ListNode *Items, int Flags)
{
int result;
char *Tempstr=NULL, *JSON=NULL, *Name=NULL, *Value=NULL, *ptr, *ptr2;
TFileInfo *FI;
HTTPInfoStruct *Info;

Tempstr=CopyStr(Tempstr,"https://apis.live.net/v5.0/me/skydrive?access_token=");
Tempstr=CatStr(Tempstr,FS->Passwd);
FS->S=HTTPGet(Tempstr,"","");
Tempstr=STREAMReadLine(Tempstr,FS->S);
while (Tempstr)
{
StripTrailingWhitespace(Tempstr);
StripLeadingWhitespace(Tempstr);
JSON=CatStr(JSON,Tempstr);
Tempstr=STREAMReadLine(Tempstr,FS->S);
}

FI=SkyDriveParseFile(JSON);
FS->CurrDirPath=CopyStr(FS->CurrDirPath,FI->EditPath);

DestroyString(Tempstr);
DestroyString(Name);
DestroyString(Value);
return(TRUE);
}


STREAM *SkyDriveOpenFile(TFileStore *FS, TFileInfo *FI, int Flags)
{
HTTPInfoStruct *Info;
char *Tempstr=NULL;
STREAM *S;

FS->Extra=NULL;

if (Flags & OPEN_WRITE)
{
	Tempstr=MCopyStr(Tempstr,"https://apis.live.net/v5.0/me/skydrive/files/",FI->Name,"?access_token=",FS->Passwd,NULL);
	Info=HTTPInfoFromURL("PUT", Tempstr);
	Info->PostContentType=CopyStr(Info->PostContentType,"octet/stream");
	Info->PostContentLength=FI->Size;
	printf("WRITE: %s\n",Tempstr);
	Info->Flags |= HTTP_DEBUG;
	S=HTTPTransact(Info);
}
else S=HTTPGet(FI->Path,"","");

FS->Extra=(void *) Info;

DestroyString(Tempstr);

return(S);
}

int SkyDriveCloseFile(TFileStore *FS, STREAM *S)
{ 
	char *Tempstr=NULL, *Response=NULL;
	HTTPInfoStruct *Info;
	int RetVal=FALSE;
            
	Info=(HTTPInfoStruct *) FS->Extra;

	//if doing a post
	if (S->Flags & SF_WRONLY)
	{
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




/*

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

*/




#define SKYDRIVE_CLIENT_ID "000000004011560F"
#define SKYDRIVE_CLIENT_SECRET "zYdVUtaR8mh00vfdZwKcY2iU1GPtC-9h"


char *SkyDriveOAuthReadAuthorizationCode(char *RetBuff)
{
char *Tempstr=NULL, *Name=NULL, *Value=NULL, *ptr;

	Tempstr=SetStrLen(Tempstr,1024);
	read(0,Tempstr,1024);
	StripTrailingWhitespace(Tempstr);

	ptr=strchr(Tempstr,'?');
	if (ptr)
	{
		ptr++;
		ptr=GetNameValuePair(ptr,"&","=",&Name,&Value);
		while (ptr)
		{
			if (StrLen(Name) && (strcasecmp(Name,"code")==0)) RetBuff=CopyStr(RetBuff,Value);
			ptr=GetNameValuePair(ptr,"&","=",&Name,&Value);
		}
	}

DestroyString(Name);
DestroyString(Value);
DestroyString(Tempstr);

return(RetBuff);
}


int SkyDriveOAuth(TFileStore *FS)
{
char *RefreshToken=NULL, *URL=NULL;
char *Tempstr=NULL;


HTTPSetFlags(HTTP_DEBUG);

RefreshToken=CopyStr(RefreshToken,GetVar(FS->Vars,"OAuthRefreshToken"));

if (StrLen(RefreshToken))
{
	//OAuthDeviceRefreshToken("", GOOGLE_CLIENT_ID, GOOGLE_CLIENT_SECRET, RefreshToken, &FS->Passwd);
	printf("REFRESH: %s\n",FS->Passwd);
	SetVar(FS->Vars,"OAuthRefreshToken",RefreshToken);
}
else
{
	OAuthInstalledAppURL("https://login.live.com/oauth20_authorize.srf", SKYDRIVE_CLIENT_ID, GetVar(FS->Vars,"OAuthScope"),"https://login.live.com/oauth20_desktop.srf", &URL);

	printf("CUT AND PASTE THE FOLLOWING URL INTO A WEBBROWSER:\n\n %s\n\n",URL);
	printf("Login and/or grant access, then cut and past the access code back to this program.\n\nAccess Code: ");
	fflush(NULL);


	
	Tempstr=SkyDriveOAuthReadAuthorizationCode(Tempstr);
	printf("CODE: %s\n",Tempstr);

	if (! StrLen(Tempstr)) exit(0);
	OAuthInstalledAppGetAccessToken("https://login.live.com/oauth20_token.srf", SKYDRIVE_CLIENT_ID, SKYDRIVE_CLIENT_SECRET, Tempstr, "https://login.live.com/oauth20_desktop.srf", &FS->Passwd, &RefreshToken);

	SetVar(FS->Vars,"OAuthRefreshToken",RefreshToken);
	printf("ACCESS:  %s\n",FS->Passwd);
	printf("REFRESH: %s\n",RefreshToken);
}

FS->Settings |= FS_OAUTH;
ConfigFileSaveFileStore(FS);

DestroyString(RefreshToken);
DestroyString(Tempstr);
DestroyString(URL);

return(TRUE);
}


int SkyDriveOpen(TFileStore *FS)
{
HTTPInfoStruct *Info;
char *URL=NULL, *Tempstr=NULL;
int result=FALSE;

if (! FS) 
{
return(FALSE);
}

if (! StrLen(FS->Passwd)) 
{
	SetVar(FS->Vars,"OAuthScope","wl.skydrive_update");
	SkyDriveOAuth(FS);
	Tempstr=MCopyStr(Tempstr, "Bearer ", FS->Passwd, NULL);
}
else if (FS->Settings & FS_OAUTH) Tempstr=MCopyStr(Tempstr, "Bearer ", FS->Passwd, NULL);

if (StrLen(Tempstr))
{
SetVar(FS->Vars,"AuthToken",Tempstr);
result=TRUE;

URL=CopyStr(URL,"https://apis.live.net/v5.0/me/skydrive?access_token=");
URL=CatStr(URL,FS->Passwd);

SkyDriveLoadDir(FS, "/", NULL, 0);

printf("SDLD: %s\n",FS->CurrDirPath);

/*
printf("Connect: %s\n",URL);
FS->S=HTTPGet(URL,"","");
if (FS->S) 
{
//	PrintConnectionDetails(FS,FS->S);
	Tempstr=STREAMReadLine(Tempstr,FS->S);
	while (Tempstr)
	{
		StripTrailingWhitespace(Tempstr);
		printf("SD: [%s]\n",Tempstr);
		Tempstr=STREAMReadLine(Tempstr,FS->S);
	}
	STREAMClose(FS->S);
}
*/


}
DestroyString(Tempstr);

return(result);
}


int SkyDriveClose(TFileStore *FS)
{
int result;
char *Tempstr=NULL;


   DestroyString(Tempstr);
return(TRUE);
}




TFileStore *SkyDriveFileStoreCreate(char *Name, char *ConnectSetup)
{
TFileStore *FS;

HTTPSetFlags(HTTP_DEBUG);
FS=(TFileStore *) calloc(1,sizeof(TFileStore));
FS->Flags=FS_WRITEABLE | FS_SSL;
FS->Features=FS_FILE_SIZE;
FS->Vars=ListCreate();
FS->CurrDir=CopyStr(FS->CurrDir,"");


FS->Create=SkyDriveFileStoreCreate;
FS->Open=SkyDriveOpen;
FS->Close=SkyDriveClose;
FS->OpenFile=SkyDriveOpenFile;
FS->CloseFile=SkyDriveCloseFile;
FS->LoadDir=SkyDriveLoadDir;
FS->ReadBytes=DefaultReadBytes;
FS->WriteBytes=DefaultWriteBytes;

/*
//FS->ChDir=GDriveChDir;
FS->DeleteFile=GDriveDeleteFile;

FS->MkDir=GDriveMkDir;

FS->RenameFile=GDriveRenameFile;
FS->GetFileSize=FileStoreGetFileSize;
*/

FS->InitArg=CopyStr(FS->InitArg,ConnectSetup);
FS->Name=CopyStr(FS->Name,Name);

return(FS);
}
