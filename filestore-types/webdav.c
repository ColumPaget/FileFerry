#define _GNU_SOURCE
#include <fnmatch.h>

#include "webdav.h"

#define OPEN_READ 0
#define OPEN_WRITE 1
#define OPEN_WEBDAV_PROPFIND 2

#define PROPFIND_XML "<?xml version=\"1.0\" encoding=\"utf-8\" ?> <D:propfind xmlns:D=\"DAV:\" > <D:prop> <D:getcontentlength/> <D:resourcetype/> <D:iscollection/> <D:creationdate/> <D:getlastmodified/> <executable xmlns=\"http://apache.org/dav/props/\"/> <md5-checksum xmlns=\"http://subversion.tigris.org/xmlns/dav/\"/> </D:prop> </D:propfind>"


//#define PROPFIND_XML "<?xml.version=\"1.0\" encoding=\"utf-8\"?><propfind xmlns=\"DAV:\"><prop><getcontentlength xmlns=\"DAV:\"/><getlastmodified xmlns=\"DAV:\"/><executable xmlns=\"http://apache.org/dav/props/\"/><resourcetype xmlns=\"DAV:\"/><checked-in xmlns=\"DAV:\"/> <checked-out xmlns=\"DAV:\"/></prop></propfind>"

#define ALLPROP_XML "<?xml version=\"1.0\" encoding=\"utf-8\" ?> <D:propfind xmlns:D=\"DAV:\" > <D:allprop/> </D:propfind>"


int WebDAVPerformBasicCommand(TFileStore *FS, char *Command, char *Path, char *Destination)
{
char *Tempstr=NULL;
HTTPInfoStruct *HTTPInfo=NULL;
int result=FALSE;

if (*Path=='/') Tempstr=CopyStr(Tempstr,Path);
else Tempstr=MCopyStr(Tempstr,FS->CurrDir,Path,NULL);
HTTPInfo=(HTTPInfoStruct *) FS->Extra;

if (HTTPInfo)
{
	HTTPInfoSetValues(HTTPInfo, FS->Host,FS->Port,FS->Logon,FS->Passwd,Command,Tempstr,"",0);

	if (*Destination=='/') HTTPInfo->Destination=CopyStr(HTTPInfo->Destination,Destination);
	else HTTPInfo->Destination=MCopyStr(HTTPInfo->Destination,FS->CurrDir,Destination,NULL);

	FS->S=HTTPTransact(HTTPInfo);

	if (HTTPInfo->ResponseCode && (*HTTPInfo->ResponseCode=='2')) result=TRUE;
	STREAMClose(FS->S);
	HTTPInfo->S=NULL;
	FS->S=NULL;
}


DestroyString(Tempstr);
return(result);
}




int WebDAVMkDir(TFileStore *FS, char *Path)
{
return(WebDAVPerformBasicCommand(FS, "MKCOL", Path,""));
}

int WebDAVDeleteFile(TFileStore *FS, TFileInfo *FI)
{
return(WebDAVPerformBasicCommand(FS, "DELETE", FI->Path,""));
}

int WebDAVRenameFile(TFileStore *FS, char *Path, char *NewPath)
{
char *Tempstr=NULL;
int result;

result=WebDAVPerformBasicCommand(FS, "MOVE", Path, NewPath);

return(result);
}



typedef enum {TAG_RESPONSE, TAG_HREF,TAG_CONTENTTYPE, TAG_SIZE, TAG_MTIME, TAG_CTIME, TAG_RESOURCETYPE, TAG_COLLECTION, TAG_ISCOLLECTION, TAG_MD5CHECKSUM, TAG_EXECUTABLE,TAG_RESPONSE_END} TWEBDAV_XML_TAGS;
char *Tags[]={"response","href","getcontenttype","getcontentlength","getlastmodified","creationdate","resourcetype","collection","iscollection","md5-checksum","executable","/response",NULL};

char *WebdavReadTagData(char *RetBuff, char *Line)
{
char *TagData=NULL, *ptr;

TagData=CopyStr(RetBuff,"");
ptr=Line;

while ((*ptr != '<') && (*ptr !='\0'))
{
TagData=AddCharToStr(TagData,*ptr);
ptr++;
}
while ((*ptr != '>') && (*ptr !='\0')) ptr++;
return(TagData);
}


char *WebDavGetTag(char *Input, int *TagType, char **TagData)
{
char *ptr=NULL, *tptr;
char *Token=NULL, *TagTypeStr=NULL, *Host=NULL;
int len, CloseTag=FALSE;

if (! StrLen(Input)) return(NULL);
TagTypeStr=CopyStr(TagTypeStr,"");

ptr=XMLGetTag(Input,&Token,&TagTypeStr,TagData);

*TagType=MatchTokenFromList(TagTypeStr,Tags,0);



switch (*TagType)
{
	case TAG_HREF:
	if (strncmp(ptr,"http://",7)==0) 
	{
		ptr+=7;
		ptr=GetToken(ptr,"/",&Host,0);
		ptr--; //to get '/' at start of path
	}
	else if (strncmp(ptr,"https://",8)==0) 
	{
		ptr+=8;
		ptr=GetToken(ptr,"/",&Host,0);
		ptr--; //to get '/' at start of path
	}
	*TagData=WebdavReadTagData(*TagData,ptr);
	break;
		
	case TAG_SIZE:
	case TAG_CTIME:
	case TAG_MTIME:
	case TAG_ISCOLLECTION:
	case TAG_EXECUTABLE:
	case TAG_MD5CHECKSUM:
	case TAG_CONTENTTYPE:
	*TagData=WebdavReadTagData(*TagData,ptr);
	break;
}


DestroyString(Token);
DestroyString(Host);
DestroyString(TagTypeStr);
return(ptr);
}



TFileInfo *WebDAVParseDirEntry(TFileStore *FS, ListNode *Items, char **XML)
{
TFileInfo *FI=NULL;
char *Tempstr=NULL, *Token=NULL;
char *ptr=NULL;
int TagType=0, result=0;
int BufSiz=8192, len=0;


FI=(TFileInfo *) calloc(1,sizeof(TFileInfo));

ptr=*XML;
while (StrLen(ptr))
{
ptr=WebDavGetTag(ptr, &TagType, &Token);


if (TagType==TAG_RESPONSE_END) break;
switch (TagType)
{
	case TAG_HREF:
		FI->Path=HTTPUnQuote(FI->Path,Token);
	break;
	
	case TAG_SIZE:
		FI->Size=atoi(Token);
	break;

	case TAG_MTIME:
		if (strchr(Token,' ')) FI->Mtime=DateStrToSecs("%a, %d %b %Y %H:%M:%S %Z",Token,NULL);
		else FI->Mtime=DateStrToSecs("%Y-%m-%dT%H:%M:%S",Token,NULL);
	break;
	
	case TAG_COLLECTION:
		FI->Type=FTYPE_DIR;
	break;

	case TAG_MD5CHECKSUM:
		SetVar(FI->Vars,"md5",Token);
	break;

	case TAG_EXECUTABLE:
		if (strcmp(Token,"T")==0) FI->Type=FTYPE_EXE;
	break;

	case TAG_CONTENTTYPE:
		FI->MediaType=CopyStr(FI->MediaType,Token);
	break;


}
} 


FI->Name=CopyStr(FI->Name,GetBasename(FI->Path));
StripDirectorySlash(FI->Name);

*XML=ptr;

DestroyString(Tempstr);
DestroyString(Token);

if ((! StrLen(FI->Path)) || (strcmp(FI->Path,FS->CurrDir)==0))
{
FileInfoDestroy(FI);
return(NULL);
} 

return(FI);
}


void WebDAVParseDirEntries(TFileStore *FS, ListNode *Items, int HTTPFlags)
{
TFileInfo *FI=NULL;
char *Tempstr=NULL, *Token=NULL, *CurrDir=NULL;
char *ptr=NULL;
int TagType=0, result=0;
int BufSiz=8192, len=0;

CurrDir=CopyStr(CurrDir,FS->CurrDir);
CurrDir=TerminateDirectoryPath(CurrDir);


Tempstr=STREAMReadDocument(Tempstr, FS->S, FALSE);

ptr=Tempstr;
while (StrLen(ptr))
{
ptr=WebDavGetTag(ptr, &TagType, &Token);


if (TagType==TAG_RESPONSE) 
{
	FI=WebDAVParseDirEntry(FS, Items, &ptr);

	if (FI)
	{
	  if (StrLen(FI->Path) ) 
		{
			if (
				(FI->Type==FTYPE_DIR) && 
				(strcmp(FI->Name,GetBasename(FS->CurrDir))==0)
				) 
			FileInfoDestroy(FI);
			else ListAddNamedItem(Items,FI->Name,FI);
		}
	}
}

} 


DestroyString(Tempstr);
DestroyString(Token);
}


int WebDAVGetQuota(TFileStore *FS, HTTPInfoStruct *HTTPInfo)
{
char *Tempstr=NULL;
int result=FALSE;
char *XML=NULL;

XML=CopyStr(XML,
"<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
"<D:propfind xmlns:D=\"DAV:\" >"
"<D:prop>"
"  <D:quota-available-bytes/>"
"  <D:quota-used-bytes/>"
"</D:prop>"
"</D:propfind>");


HTTPInfo->ContentLength=StrLen(XML);
HTTPInfo->ContentType=CopyStr(HTTPInfo->ContentType,"application/xml");
HTTPInfo->Method=CopyStr(HTTPInfo->Method,"PROPFIND");
HTTPInfo->Doc=CopyStr(HTTPInfo->Doc,FS->CurrDir);
HTTPInfo->PostData=CopyStr(HTTPInfo->PostData,XML);
HTTPInfo->Depth=0;
FS->S=HTTPTransact(HTTPInfo);

if (FS->S)
{
	if (strncmp(HTTPInfo->ResponseCode,"4",1) !=0) 
	{
		result=TRUE;
	}

Tempstr=STREAMReadDocument(Tempstr, FS->S, TRUE);
}

STREAMClose(FS->S);
FS->S=NULL;
HTTPInfo->S=NULL;

DestroyString(XML);
DestroyString(Tempstr);

return(result);
}



int WebDAVSendDirPropFind(TFileStore *FS, HTTPInfoStruct *HTTPInfo, char *XML)
{
char *Tempstr=NULL;
int result=FALSE;


HTTPInfo->PostContentType=CopyStr(HTTPInfo->PostContentType,"application/xml");
HTTPInfo->PostData=CopyStr(HTTPInfo->PostData,XML);
HTTPInfo->PostContentLength=StrLen(XML);
FS->S=HTTPTransact(HTTPInfo);

if (FS->S) result=HTTPParseResponse(HTTPInfo->ResponseCode);
else result=ERR_FAIL;

DestroyString(Tempstr);

return(result);
}





int WebDAVPropFind(TFileStore *FS, char *InPattern, ListNode *Items, char *XML)
{
char *Tempstr=NULL;
HTTPInfoStruct *HTTPInfo;
TFileInfo *FI;
char *ptr;
int len, result=FALSE;

//len=StrLen(Tempstr)-1;
//if ((len > 0) && (*(Tempstr+len)=='/')) *(Tempstr+len)='\0';
HTTPInfo=(HTTPInfoStruct *) FS->Extra;
HTTPInfoSetValues(HTTPInfo, FS->Host,FS->Port,FS->Logon,FS->Passwd,"PROPFIND",FS->CurrDir,"",0);
HTTPInfo->Depth=1;


result=WebDAVSendDirPropFind(FS, HTTPInfo, XML);
if (result==TRANSFER_OKAY)
{
	if (strcmp(HTTPInfo->Doc,FS->CurrDir) !=0) FS->ChDir(FS,HTTPInfo->Doc); 
	WebDAVParseDirEntries(FS, Items, HTTPInfo->Flags);
}


STREAMClose(FS->S);
FS->S=NULL;
HTTPInfo->S=NULL;
HTTPInfo->Depth=0;

DestroyString(Tempstr);

return(result);
}



int WebDAVLoadDir(TFileStore *FS, char *InPattern, ListNode *Items, int Flags)
{
if (WebDAVPropFind(FS, InPattern, Items, PROPFIND_XML)==TRANSFER_OKAY) return(TRUE);
return(FALSE);
}


STREAM *WebDAVOpenFile(TFileStore *FS, TFileInfo *FI, int Flags)
{
HTTPInfoStruct *HTTPInfo;
char *Tempstr=NULL, *ptr;
STREAM *S;
int val=0, HttpFlags=0;

if (*FI->Path=='/') Tempstr=CopyStr(Tempstr,FI->Path);
else Tempstr=MCopyStr(Tempstr,FS->CurrDir,FI->Path,NULL);

HTTPInfo=(HTTPInfoStruct *) FS->Extra;
	HTTPInfo->Depth=0;

if (Flags & OPEN_WRITE) 
{
	HTTPInfoSetValues(HTTPInfo, FS->Host,FS->Port,FS->Logon,FS->Passwd,"PUT",Tempstr,"",0);
	HTTPInfo->PostContentLength=FI->Size;
	HTTPInfo->PostContentType=CopyStr(HTTPInfo->PostContentType,FI->MediaType);
	S=HTTPTransact(HTTPInfo);
	if (S) S->Flags |= SF_WRONLY;
	STREAMSetFlushType(S,FLUSH_ALWAYS,0);
}
else 
{
	HTTPInfoSetValues(HTTPInfo, FS->Host,FS->Port,FS->Logon,FS->Passwd,"GET",Tempstr,"",0);
	S=HTTPTransact(HTTPInfo);
	if (S)
	{
	ptr=STREAMGetValue(S,"http:content-length");
	if (StrLen(ptr)) STREAMSetValue(S,"filesize",ptr);
	}
}

DestroyString(Tempstr);

return(S);
}




int WebDAVCloseFile(TFileStore *FS, STREAM *S)
{
char *Tempstr=NULL;
HTTPInfoStruct *Info;
int RetVal=FALSE;

if (! S) return(TRUE);

Info=(HTTPInfoStruct *) FS->Extra;
STREAMFlush(S);
if (S->Flags & SF_WRONLY) 
{
	HTTPTransact(Info);
	Tempstr=STREAMReadDocument(Tempstr, S, FALSE);
}
else RetVal=TRUE;

RetVal=HTTPParseResponse(STREAMGetValue(S,"HTTP:ResponseCode"));

STREAMClose(S);
Info->S=NULL;

DestroyString(Tempstr);

return(RetVal);
}



int WebDAVOpen(TFileStore *FS)
{
int result=FALSE;
char *Tempstr=NULL;
HTTPInfoStruct *Info;
int val=0;

//This won't talk WEBDAV, as it uses 'DefaultChDir' function that just changes
//internal idea of what dir is

if (StrLen(FS->InitDir)) FS->ChDir(FS,FS->InitDir);

//we need an http info. Probably won't use the 'options' call, it's just a placeholder
Info=HTTPInfoCreate(FS->Host,FS->Port,FS->Logon,FS->Passwd,"OPTIONS",FS->InitDir,"",0);
if (FS->Flags & FS_SSL) Info->Flags |= HTTP_SSL;
FS->Extra=(void *) Info;

val=WebDAVPropFind(FS, "*", NULL, ALLPROP_XML);

switch (val)
{
	case ERR_FORBID:
	RequestAuthDetails(&FS->Logon,&FS->Passwd);
	HTTPInfoSetValues(Info, FS->Host, FS->Port, FS->Logon, FS->Passwd, "HEAD", "/", "", 0);
	break;

	case TRANSFER_OKAY:
	result=TRUE;
	break;
}

if (FS->S)
{
PrintConnectionDetails(FS,Info->S);
STREAMClose(Info->S);
result=TRUE;
Info->S=NULL;
}

DestroyString(Tempstr);
return(result);
}


int WebDAVClose(TFileStore *FS)
{
int result;


if (FS->Extra) HTTPInfoDestroy(FS->Extra);
return(TRUE);
}




TFileStore *WebDAVFileStoreCreate(char *Name, char *ConnectSetup)
{
TFileStore *FS;
char *ptr;

FS=(TFileStore *) calloc(1,sizeof(TFileStore));
FS->Flags=FS_WRITEABLE | FS_CHDIR_FULLPATH;
FS->Vars=ListCreate();
//FS->InitArg=CopyStr(FS->InitArg,ConnectSetup);
FS->InitDir=CopyStr(FS->InitDir,"/");
FS->Name=CopyStr(FS->Name,Name);
FS->CurrDir=CopyStr(FS->CurrDir,"/");
FS->Port=80;

FS->Create=WebDAVFileStoreCreate;
FS->Open=WebDAVOpen;
FS->Close=WebDAVClose;
FS->ChDir=DefaultChDir;
FS->MkDir=WebDAVMkDir;
FS->LoadDir=WebDAVLoadDir;
FS->OpenFile=WebDAVOpenFile;
FS->CloseFile=WebDAVCloseFile;
FS->ReadBytes=DefaultReadBytes;
FS->WriteBytes=DefaultWriteBytes;
FS->RenameFile=WebDAVRenameFile;
FS->DeleteFile=WebDAVDeleteFile;


return(FS);
}

TFileStore *SWebDAVFileStoreCreate(char *Name, char *ConnectSetup)
{
TFileStore *FS;

FS=WebDAVFileStoreCreate(Name, ConnectSetup);
FS->Create=SWebDAVFileStoreCreate;
FS->Flags |= FS_SSL;
FS->Port=443;
}
