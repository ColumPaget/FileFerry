#define _GNU_SOURCE
#include <fnmatch.h>

#include "skydrive.h"


#define SKYDRIVE_CLIENT_ID "000000004011560F"
#define SKYDRIVE_CLIENT_SECRET "zYdVUtaR8mh00vfdZwKcY2iU1GPtC-9h"


void SkyDriveReadJSON(STREAM *S, ListNode *Vars)
{
char *Tempstr=NULL, *JSON=NULL, *Name=NULL, *Value=NULL;
char *ptr;


JSON=STREAMReadDocument(JSON,S,FALSE);
strrep(JSON,'\r','\n');
StripLeadingWhitespace(JSON);
StripTrailingWhitespace(JSON);
ptr=GetNameValuePair(JSON,"\n",":",&Name,&Value);
while (ptr)
{
	StripTrailingWhitespace(Name);
	StripLeadingWhitespace(Name);
	StripTrailingWhitespace(Value);
	StripLeadingWhitespace(Value);

	SetVar(Vars,Name,Value);

	ptr=GetNameValuePair(ptr, "\n",":",&Name,&Value);
}

DestroyString(Tempstr);
DestroyString(JSON);
DestroyString(Name);
DestroyString(Value);
}




/*
SD: [{^M   "id": "folder.8d458c461c4417fc", ^M   "from": {^M      "name": null, ^M      "id": null^M   }, ^M   "name": "SkyDrive", ^M   "description": "", ^M   "parent_id": null, ^M   "size": 0, ^M   "upload_location": "https://apis.live.net/v5.0/folder.8d458c461c4417fc/files/", ^M   "comments_count": 0, ^M   "comments_enabled": false, ^M   "is_embeddable": false, ^M   "count": 5, ^M   "link": "https://skydrive.live.com?cid=8d458c461c4417fc", ^M   "type": "folder", ^M   "shared_with": {^M      "access": "Just me"^M   }, ^M   "created_time": null, ^M   "updated_time": "2014-02-07T18:50:16+0000", ^M   "client_updated_time": "2014-02-07T18:50:16+0000"^M}]
*/


TFileInfo *SkyDriveParseFile(char **JSON)
{
char *Tempstr=NULL, *Name=NULL, *Value=NULL, *ptr, *ptr2;
TFileInfo *FI;
 
if ((! JSON) || (! *JSON)) return(NULL);
FI=(TFileInfo *) calloc(1,sizeof(TFileInfo));
//SkyDrive JSON is horrible. It's terminated with \r
printf("JSON: %s\n",*JSON);

*JSON=GetNameValuePair(*JSON,"\r",":",&Name,&Value);
while (*JSON)
{
StripTrailingWhitespace(Name);
StripLeadingWhitespace(Name);

StripTrailingWhitespace(Value);
StripLeadingWhitespace(Value);


if (strcmp(Name,"}, {")==0) break;
ptr2=Value+StrLen(Value)-1;
if (*ptr2==',') *ptr2='\0';
StripQuotes(Value);

printf("%s -> %s\n",Name,Value);

switch (*Name)
{
case 'f':
if ((strcmp(Name,"from")==0) && (strcmp(Value,"{")==0))
{
while (*JSON) 
{
	StripTrailingWhitespace(Name);
	StripLeadingWhitespace(Name);

	printf("FROM: [%s] [%s]\n",Name,Value);
	if (StrLen(Name) && (*Name=='}')) break;
	*JSON=GetNameValuePair(*JSON,"\r",":",&Name,&Value);
}
}
break;

case 'n':
if (strcmp(Name,"name")==0)
{
	if (strcmp(Value,"SkyDrive")==0) FI->Name=CopyStr(FI->Name,"/");
	else FI->Name=CopyStr(FI->Name,Value);
}
break;

case 't':
if (strcmp(Name,"type")==0)
{
	if (strcmp(Value,"folder")==0) FI->Type=FTYPE_DIR;
	else FI->Type=FTYPE_FILE;
}
break;

case 'i': if (strcmp(Name,"id")==0) FI->ID=CopyStr(FI->ID,Value); break;

case 's': if (strcmp(Name,"size")==0) FI->Size=atoi(Value); break;

case 'u': if (strcmp(Name,"upload_location")==0) FI->EditPath=CopyStr(FI->EditPath,Value); break;

case 'l': if (strcmp(Name,"link")==0) FI->Path=CopyStr(FI->Path,Value); break;
}

*JSON=GetNameValuePair(*JSON,"\r",":",&Name,&Value);
}

printf("FI: %s %d\n",FI->Name,FI->Size);

DestroyString(Tempstr);
DestroyString(Name);
DestroyString(Value);


//if we didn't get at least an ID for the file, then don't count
//it
if (! StrLen(FI->ID))
{
FileInfoDestroy(FI);
return(NULL);
}

return(FI);
}


int SkyDriveLoadDir(TFileStore *FS, char *InPattern, ListNode *Items, int Flags)
{
int result;
char *Tempstr=NULL, *JSON=NULL, *Name=NULL, *Value=NULL, *ptr, *ptr2;
TFileInfo *FI;
HTTPInfoStruct *Info;
ListNode *Vars;

//Tempstr=MCopyStr(Tempstr,"https://apis.live.net/v5.0/me/skydrive/files?access_token=",FS->Passwd,NULL);
Tempstr=MCopyStr(Tempstr,"https://apis.live.net/v5.0/",FS->CurrDirPath,"/files",NULL);

printf("LD: %s\n",Tempstr);
FS->S=HTTPGet(Tempstr,HTTP_AUTH_BY_TOKEN,FS->Passwd);

JSON=STREAMReadDocument(JSON,FS->S,FALSE);

ptr=JSON;
FI=SkyDriveParseFile(&ptr);
while (FI) 
{
ListAddNamedItem(Items,FI->Name,FI);
FI=SkyDriveParseFile(&ptr);
}

STREAMClose(FS->S);

//FS->CurrDirPath=CopyStr(FS->CurrDirPath,FI->EditPath);

//Tempstr=MCopyStr(Tempstr,"https://apis.live.net/v5.0/me/skydrive/quota?access_token=",FS->Passwd,NULL);
Tempstr=CopyStr(Tempstr,"https://apis.live.net/v5.0/me/skydrive/quota");
FS->S=HTTPGet(Tempstr,HTTP_AUTH_BY_TOKEN,FS->Passwd);


Vars=ListCreate();
SkyDriveReadJSON(FS->S, Vars);

ptr=GetVar(Vars,"quota");
if (StrLen(ptr)) FS->BytesAvailable=strtod(ptr,NULL);
//this is not really bytes used, it's bytes remaining. 
//we must process it below
ptr=GetVar(Vars,"available");
if (StrLen(ptr)) FS->BytesUsed=strtod(ptr,NULL);

FS->BytesUsed=FS->BytesAvailable-FS->BytesUsed;

ListDestroy(Vars,DestroyString);
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
//PUT https://apis.live.net/v5.0/me/skydrive/files/HelloWorld.txt?access_token=ACCESS_TOKEN

	Tempstr=MCopyStr(Tempstr,"https://apis.live.net/v5.0/",FS->CurrDirPath,"/files/",FI->Name,NULL);
	Info=HTTPInfoFromURL("PUT", Tempstr);
	HTTPInfoSetAuth(Info, "", FS->Passwd, HTTP_AUTH_TOKEN);
	Info->PostContentLength=FI->Size;
	S=HTTPTransact(Info);
	S->Flags |= SF_WRONLY;
}
else S=HTTPGet(FI->Path,HTTP_AUTH_BY_TOKEN,FS->Passwd);

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
		STREAMFlush(S);

		S=HTTPTransact(Info);
		if (S)
		{
				Tempstr=STREAMReadDocument(Tempstr,S,FALSE);
	printf("CF: %s %s\n",Info->ResponseCode, Tempstr);
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
*/


void SkyDriveChDir(TFileStore *FS, char *Dir)
{
ListNode *Node;
TFileInfo *FI;

	if ((! StrLen(Dir)) || (strcmp(Dir,"/")==0))
	{
	FS->CurrDirPath=CopyStr(FS->CurrDirPath,"me/skydrive/");
	}
	else 
	{
		Node=ListFindNamedItem(FS->DirListing,Dir);
		if (Node) 
		{
			FI=(TFileInfo *) Node->Item;
			FS->CurrDirPath=CopyStr(FS->CurrDirPath,FI->ID);
		}
	}

	FS->CurrDir=CopyStr(FS->CurrDir,Dir);
}



int SkyDriveMkDir(TFileStore *FS, char *Name)
{
char *JSON=NULL, *ptr;
int result=FALSE;
TFileInfo *FI;

JSON=MCopyStr(JSON,"{\n\"name\": \"",Name,"\"\n}\n",NULL);
FS->S=HTTPPost(FS->CurrDirPath, HTTP_AUTH_BY_TOKEN, FS->Passwd, "application/json", JSON);

/* RESPONSE LOOKS LIKE:
{
   "upload_location": "https://apis.live.net/v5.0/foldef.8d458c461c4417fc.8D458C   "link": "https://onedfive.live.com/fedif.aspx?cid=8d458c461c4417fc&page=view&}  "client_updated_time": "2014-05-30T21:29:38+0000"5"
*/

JSON=STREAMReadDocument(JSON,FS->S,FALSE);
FI=SkyDriveParseFile(&JSON);
if (FI && StrLen(FI->Path))
{
	ListAddNamedItem(FS->DirListing,Name,FI);
	result=TRUE;
}

DestroyString(JSON);

return(result);
}





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
	printf("Login and/or grant access, then skydrive will send you to a blank page. Copy the entire URL out of the 'location' bar in your browser, and paste it into here.\n");

	
	Tempstr=CopyStr(Tempstr,"");
	while (! StrLen(Tempstr))
	{
		printf("\nAccess Code: "); fflush(NULL);

		Tempstr=SkyDriveOAuthReadAuthorizationCode(Tempstr);
		if (StrLen(Tempstr)) break;
		printf("\nERROR: Couldn't extract oauth code. Paste entire URL from your browser.\n");
	}

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
}

Tempstr=MCopyStr(Tempstr, "Bearer ", FS->Passwd, NULL);
FS->Passwd=CopyStr(FS->Passwd,Tempstr);

if (StrLen(Tempstr))
{
SetVar(FS->Vars,"AuthToken",Tempstr);
result=TRUE;


SkyDriveChDir(FS, "/");
SkyDriveLoadDir(FS, "/", NULL, 0);

printf("SDLD: %s\n",FS->CurrDirPath);

/*
URL=MCopyStr(URL,FS->CurrDirPath,"?access_token=",FS->Passwd,NULL);
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
FS->MkDir=SkyDriveMkDir;
FS->LoadDir=SkyDriveLoadDir;
FS->ReadBytes=DefaultReadBytes;
FS->WriteBytes=DefaultWriteBytes;
FS->ChDir=SkyDriveChDir;

/*
FS->DeleteFile=GDriveDeleteFile;


FS->RenameFile=GDriveRenameFile;
FS->GetFileSize=FileStoreGetFileSize;
*/

FS->InitArg=CopyStr(FS->InitArg,ConnectSetup);
FS->Name=CopyStr(FS->Name,Name);

return(FS);
}
