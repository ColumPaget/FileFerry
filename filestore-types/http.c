#define _GNU_SOURCE
#include <fnmatch.h>

#include "http.h"
#include "xml.h"

int HTTPParseResponse(const char *ResponseCode)
{
int RetVal=FALSE;


	if (*ResponseCode=='2') RetVal=TRUE;
	else switch (atoi(ResponseCode))
	{
	case 401:
	case 402:
	case 405: // Method not allowed
	case 403: RetVal=ERR_FORBID; break;
	case 404:
	case 410: RetVal=ERR_NOEXIST; break;
	case 409: RetVal=ERR_EXISTS; break;
	case 411: RetVal=ERR_NOLENGTH; break;
	case 413: RetVal=ERR_TOOBIG; break;
	case 423: RetVal=ERR_LOCKED; break;
	case 429: RetVal=ERR_RATELIMIT; break;
	case 507: RetVal=ERR_FULL; break;
  }

return(RetVal);
}

int HTTPPerformBasicCommand(TFileStore *FS, char *Command, char *Path, char *Destination)
{
char *Tempstr=NULL;
HTTPInfoStruct *HTTPInfo;
int RetVal=0;

if (*Path=='/') Tempstr=CopyStr(Tempstr,Path);
else Tempstr=MCopyStr(Tempstr,FS->CurrDir,Path,NULL);
HTTPInfo=HTTPInfoCreate(FS->Host,FS->Port,FS->Logon,FS->Passwd,Command,Tempstr,"",0);
HTTPInfo->Destination=CopyStr(HTTPInfo->Destination,Destination);

HTTPTransact(HTTPInfo);
RetVal=atoi(HTTPInfo->ResponseCode);
STREAMClose(HTTPInfo->S);
DestroyString(Tempstr);
HTTPInfoDestroy(HTTPInfo);

return(RetVal);
}


int HTTPChDir(TFileStore *FS, char *Path)
{
ListNode *Node;
char *Type=NULL, *PortStr=NULL;
int result=FALSE;
TFileInfo *FI;

if (
		(strncmp(Path,"http:",5)==0) &&
		(strncmp(Path,"https:",5)==0)
	) ParseConnectDetails(Path, &Type, &FS->Host, &PortStr, &FS->Logon, &FS->Passwd, &FS->CurrDir);
	else
	{
		Node=ListFindNamedItem(FS->DirListing,Path);
		if (! Node) return(FALSE);
		FI=(TFileInfo *) Node->Item;
		ParseConnectDetails(FI->Path, &Type, &FS->Host, &PortStr, &FS->Logon, &FS->Passwd, &FS->CurrDir);
	}

	FS->Port=atoi(PortStr);
DestroyString(Type);
return(TRUE);
}



int HTTPGetFileSize(TFileStore *FS, char *Path)
{
STREAM *S;
HTTPInfoStruct *FSHTTPInfo, *HTTPInfo;
char *Tempstr=NULL, *ptr;
int Size=0;

HTTPInfo=HTTPInfoFromURL("HEAD",Path);
HTTPInfoSetAuth(HTTPInfo, FS->Logon, FS->Passwd, 0);

S=HTTPTransact(HTTPInfo);
if (S)
{
//ptr=STREAMGetValue(S,"HTTP:Content-Length");
ptr=STREAMGetValue(S,"http:content-length");
if (ptr) Size=atoi(ptr);

STREAMClose(S);
}

HTTPInfoDestroy(HTTPInfo);

DestroyString(Tempstr);

return(Size);
}



void HTTPReadLink(TFileStore *FS, char *TagData, char *Pattern, ListNode *Items)
{
char *Name=NULL, *Value=NULL, *URL=NULL, *ptr, *ptr2;
TFileInfo *FI=NULL;

ptr=GetNameValuePair(TagData," ","=",&Name,&Value);
while (ptr)
{
if (
			(strcasecmp(Name,"href")==0) &&
			(strncasecmp(Value,"javascript:",11)!=0) &&
			(fnmatch(Pattern,Value,FNM_CASEFOLD)==0)
		)
{
	if (*Value=='/') URL=FormatStr(URL,"%s://%s:%d%s","http",FS->Host,FS->Port,Value);
	else if (
						 (strncmp(Value,"http:",5) !=0) &&
						 (strncmp(Value,"https:",6) !=0)
				) URL=FormatStr(URL,"%s://%s:%d%s/%s","http",FS->Host,FS->Port,FS->CurrDir,Value);
	else URL=CopyStr(URL,Value);

	FI=FileInfoCreate(GetBasename(Value), URL, FTYPE_FILE, 0, 0);
	ptr2=strchr(FI->Name,'?');
	if (ptr2) *ptr2='\0';
}
ptr=GetNameValuePair(ptr," ","=",&Name,&Value);
}

if (FI) 
{
	if (! ListFindNamedItem(Items,FI->Name)) 
	{
		ListAddNamedItem(Items,FI->Name,FI); 
	}
	else FileInfoDestroy(FI);
}


DestroyString(Name);
DestroyString(Value);

}



char *RSSParse(char *XML, TFileStore *FS, ListNode *Items)
{
char *Tempstr=NULL, *TagName=NULL, *TagData=NULL;
TFileInfo *FI;
char *ptr;
int len, result=FALSE;

ptr=XMLGetTag(XML,NULL,&TagName,&TagData);
while (ptr)
{
	if (strcmp(TagName,"item")==0)
	{
		ptr=XMLReadItemEntry(FS, ptr, Items);
	}
	else if (strcmp(TagName,"a")==0) HTTPReadLink(FS, TagData, "*", Items);
	else if (strcasecmp(TagName,"/rss")==0) break;

ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
}

DestroyString(Tempstr);
DestroyString(TagName);
DestroyString(TagData);

return(ptr);
}


int HTTPLoadDir(TFileStore *FS, char *InPattern, ListNode *Items, int Flags)
{
char *Tempstr=NULL, *HTML=NULL, *TagName=NULL, *TagData=NULL;
TFileInfo *FI;
char *ptr;
int len, result=FALSE;
int IsRSS=FALSE;
HTTPInfoStruct *HTTPInfo;
STREAM *S;

if (! FS->CurrDir) FS->CurrDir=CopyStr(FS->CurrDir,"/");

HTTPInfo=HTTPInfoCreate(FS->Host,FS->Port,FS->Logon,FS->Passwd,"GET",FS->CurrDir,"",0);
if (FS->Flags & FS_SSL) HTTPInfo->Flags |= HTTP_SSL;

SetVar(HTTPInfo->CustomSendHeaders,"Accept","text/html,text/xml,application/xml");

S=HTTPTransact(HTTPInfo);
if (S)
{
  HTML=STREAMReadDocument(HTML, S, FALSE);
	STREAMClose(S);
	result=TRUE;
}

ptr=XMLGetTag(HTML,NULL,&TagName,&TagData);
while (ptr)
{
	if (strcasecmp(TagName,"rss")==0) ptr=RSSParse(ptr,FS,Items);
	else if (strcasecmp(TagName,"a")==0) HTTPReadLink(FS, TagData, InPattern, Items);
ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
}

//if (FI->Size==0) FI->Size=HTTPGetFileSize(FS, FI->Path);
//FI->Size=HTTPGetFileSize(FS, FI->Path);
//HTTPGetLinkInfo(Items);

HTTPInfoDestroy(HTTPInfo);

DestroyString(HTML);
DestroyString(Tempstr);
DestroyString(TagName);
DestroyString(TagData);

return(result);
}







STREAM *HTTPOpenFile(TFileStore *FS, TFileInfo *FI, int Flags)
{
HTTPInfoStruct *HTTPInfo, *GetLinkInfo;
STREAM *S;
char *Tempstr=NULL, *ptr;


//if we have to rebuild a URL, then decide the protocol
if (FS->Flags & FS_SSL) ptr="https";
else ptr="http";

if (strncmp(FI->Path,"https:",6)==0) Tempstr=CopyStr(Tempstr,FI->Path);
else if (strncmp(FI->Path,"http:",5)==0) Tempstr=CopyStr(Tempstr,FI->Path);
else if (*FI->Path=='/') Tempstr=FormatStr(Tempstr,"%s://%s:%d%s",ptr,FS->Host,FS->Port,FI->Path);
else Tempstr=FormatStr(Tempstr,"%s://%s:%d%s%s",ptr,FS->Host,FS->Port,FS->CurrDir,FI->Path);


if (Flags & OPEN_WRITE) 
{
	HTTPInfo=HTTPInfoFromURL("PUT",Tempstr);
	HTTPInfo->PostContentLength=FI->Size;
}
else HTTPInfo=HTTPInfoFromURL("GET",Tempstr);

if (strncmp(Tempstr,"https:",6)==0) HTTPInfo->Flags |= HTTP_SSL;

if ((Flags & OPEN_RESUME) && (FI->ResumePoint > 0))
{
	Tempstr=FormatStr(Tempstr,"%d-",FI->ResumePoint);
	SetVar(HTTPInfo->CustomSendHeaders,"Range", Tempstr);
}

if (StrLen(FS->Logon) || StrLen(FS->Passwd)) HTTPInfoSetAuth(HTTPInfo, FS->Logon, FS->Passwd,HTTP_AUTH_BASIC);

S=HTTPTransact(HTTPInfo);

if (S)
{

if (Flags & OPEN_WRITE)
{
	S->Flags |= SF_WRONLY;
	STREAMSetFlushType(S,FLUSH_ALWAYS,0);
}
else
{
	ptr=STREAMGetValue(S,"http:content-length");
	if (StrLen(ptr)) 
	{
		STREAMSetValue(S,"filesize",ptr);
		FI->Size=atoi(ptr);
	}
}
STREAMSetItem(S,"HTTPInfoStruct",HTTPInfo);
}


DestroyString(Tempstr);

return(S);
}




int HTTPCloseFile(TFileStore *FS, STREAM *S)
{
char *Tempstr=NULL;
HTTPInfoStruct *Info;
int RetVal=FALSE;

STREAMFlush(S);
Info=(HTTPInfoStruct *) STREAMGetItem(S,"HTTPInfoStruct");

if (S->Flags & SF_WRONLY) 
{
	HTTPTransact(Info);
	RetVal=HTTPParseResponse(Info->ResponseCode);
}

STREAMClose(S);
HTTPInfoDestroy(Info);

DestroyString(Tempstr);

return(RetVal);
}



int HTTPOpen(TFileStore *FS)
{
int result=FALSE;
char *Tempstr=NULL;
HTTPInfoStruct *Info;

Info=HTTPInfoCreate(FS->Host,FS->Port,FS->Logon,FS->Passwd,"HEAD","/","",0);
if (FS->Flags & FS_SSL) Info->Flags |= HTTP_SSL;


HTTPTransact(Info);
/*Handle any Redirects*/

if (StrLen(Info->RedirectPath)) 
{
	ParseConnectDetails(Info->RedirectPath, &Tempstr, &FS->Host, &Tempstr, &FS->Logon, &FS->Passwd, &FS->CurrDir);
	FS->Port=atoi(Tempstr);
}


if (Info->ResponseCode && (strcmp(Info->ResponseCode,"401")==0) )
{
RequestAuthDetails(&FS->Logon,&FS->Passwd);
HTTPInfoSetValues(Info, FS->Host, FS->Port, FS->Logon, FS->Passwd, "HEAD", "/", "", 0);
HTTPTransact(Info);
}

HTTPInfoSetValues(Info, FS->Host, FS->Port, FS->Logon, FS->Passwd, "OPTIONS", "/", "", 0);
HTTPTransact(Info);


if (Info->S)
{
	PrintConnectionDetails(FS,Info->S);
	STREAMClose(Info->S);
	Info->S=NULL;
	result=TRUE;
}
HTTPInfoDestroy(Info);

DestroyString(Tempstr);

return(result);
}


int HTTPClose(TFileStore *FS)
{
int result;


return(TRUE);
}




TFileStore *HTTPFileStoreCreate(char *Name, char *ConnectSetup)
{
TFileStore *FS;
char *Type=NULL;

FS=(TFileStore *) calloc(1,sizeof(TFileStore));
FS->Flags=FS_WRITEABLE | FS_CHDIR_FULLPATH;
//FS->Features=FS_FILE_SIZE;
FS->Features=FS_RESUME_TRANSFERS;
FS->InitArg=CopyStr(FS->InitArg,ConnectSetup);
FS->Name=CopyStr(FS->Name,Name);
//FS->CurrDir=CopyStr(FS->CurrDir,"/");



FS->Create=HTTPFileStoreCreate;
FS->Open=HTTPOpen;
FS->Close=HTTPClose;
FS->ChDir=HTTPChDir;
FS->LoadDir=HTTPLoadDir;
FS->OpenFile=HTTPOpenFile;
FS->CloseFile=HTTPCloseFile;
FS->ReadBytes=DefaultReadBytes;
FS->WriteBytes=DefaultWriteBytes;
//FS->DeleteFile=HTTPDeleteFile;
//FS->GetFileSize=HTTPGetFileSize;

ParseConnectDetails(FS->InitArg, &Type, &FS->Host, &FS->Port, &FS->Logon, &FS->Passwd, &FS->CurrDir);
if (strcasecmp(Type,"https")==0)
{
	FS->Flags |= FS_SSL;
	if (FS->Port==0) FS->Port=443;
}
else if (FS->Port==0) FS->Port=80;

DestroyString(Type);

return(FS);
}

