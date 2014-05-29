#define _GNU_SOURCE
#include <fnmatch.h>

#include "filesanywhere.h"

#define LOGIN_URL "http://api.filesanywhere.com/AccountLogin"
#define API_KEY "40D380BA-4E80-46D9-8629-6A2F229DBA94"

int FAWCommand(TFileStore *FS, char *XML, char *SOAPAction, char **ResponseData)
{
int result=FALSE;
HTTPInfoStruct *HTTPInfo;
STREAM *S;

if (! FS) 
{
return(FALSE);
}

if (! FS->Extra) HTTPInfo=HTTPInfoFromURL("POST", "http://api.filesanywhere.com/fawapi.asmx");
else HTTPInfo=(HTTPInfoStruct *) FS->Extra;
SetVar(HTTPInfo->CustomSendHeaders,"SOAPAction", SOAPAction);
 HTTPInfo->PostContentType=CopyStr(HTTPInfo->PostContentType,"text/xml; charset=utf-8");
 HTTPInfo->PostData=CopyStr(HTTPInfo->PostData,XML);
 HTTPInfo->PostContentLength=StrLen(XML);
 S=HTTPTransact(HTTPInfo);

*ResponseData=CopyStr(*ResponseData,"");
*ResponseData=SetStrLen(*ResponseData,HTTPInfo->ContentLength);
memset(*ResponseData,0,HTTPInfo->ContentLength);

if (S) 
{
  if (strcmp(SOAPAction,LOGIN_URL)==0) PrintConnectionDetails(FS,S);
  result=STREAMReadBytes(S,*ResponseData,HTTPInfo->ContentLength);

  STREAMClose(S);
}

if (strcmp(HTTPInfo->ResponseCode,"200")==0) return(TRUE);
return(FALSE);
}



TFileInfo *FAWReadFileEntry(char **XML)
{
char *Token=NULL, *TagName=NULL, *TagData=NULL, *ptr;
char *Tags[]={"Type","Name","Size","Path","DateLastModified","/Item",NULL};
typedef enum {FAW_TYPE,FAW_NAME,FAW_SIZE,FAW_PATH,FAW_MTIME,FAW_END};
TFileInfo *FI=NULL;
int result;

		FI=(TFileInfo *) calloc(1,sizeof(TFileInfo));
		*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
		while (*XML)
		{
			result=MatchTokenFromList(TagName,Tags,0);
			if (result==FAW_END) break;
			switch (result)
			{
				case FAW_NAME: 
					*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
					FI->Name=CopyStr(FI->Name,TagData);
				break;

				case FAW_PATH: 
					*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
					FI->Path=CopyStr(FI->Path,TagData);
					//strrep(FI->Path,'\\','/');
				break;


				case FAW_MTIME: 
					*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
					FI->Mtime=DateStrToSecs("%m/%d/%Y %H:%M:%S",TagData,NULL);
				break;

				case FAW_SIZE: 
					*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
					FI->Size=atoi(TagData);
				break;

				case FAW_TYPE: 
					*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
					if (strcasecmp(TagData,"Folder")==0) FI->Type=FTYPE_DIR;
					else FI->Type=FTYPE_FILE;
				break;
			}

		*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
		}

FI->Path=CatStr(FI->Path,FI->Name);

DestroyString(TagName);
DestroyString(TagData);
DestroyString(Token);

return(FI);
}

int FAWLoadDir(TFileStore *FS, char *InPattern, ListNode *Items, int Flags)
{
char *Tempstr=NULL, *XML=NULL;
char *TagName=NULL, *TagData=NULL, *ptr=NULL;
TFileInfo *FI;
int result=FALSE;

XML=MCopyStr(XML,"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n",
"<soap:Envelope xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">\n",
"<soap:Body>\n",
"<ListItems xmlns=\"http://api.filesanywhere.com/\">\n",
"<Token>",GetVar(FS->Vars,"AuthToken"),"</Token>\n", 
"<Path>",FS->CurrDir,"</Path>\n",
"</ListItems>\n",
"</soap:Body>\n", 
"</soap:Envelope>\n",
NULL);

result=FAWCommand(FS, XML, "http://api.filesanywhere.com/ListItems",&Tempstr);

ptr=XMLGetTag(Tempstr,NULL,&TagName,&TagData);
while (ptr)
{
	if (result && (strcasecmp(TagName,"Item")==0) )
	{
		
		FI=FAWReadFileEntry(&ptr);
		if (fnmatch(InPattern,FI->Name,0)==0) ListAddNamedItem(Items,FI->Name,FI);
		else FileInfoDestroy(FI);
	}
	ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
}

DestroyString(Tempstr);
DestroyString(XML);

return(TRUE);
}




int FAWMkDir(TFileStore *FS, char *Dir)
{
char *Tempstr=NULL, *XML=NULL;
int result=FALSE;

if (! FS) 
{
return(FALSE);
}


XML=MCopyStr(XML,"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n",
"<soap:Envelope xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"",
" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\"",
" xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">\n",
"<soap:Body>\n",
"<CreateFolder xmlns=\"http://api.filesanywhere.com/\">\n",
"<Token>",GetVar(FS->Vars,"AuthToken"),"</Token>\n",
"<Path>",FS->CurrDir,"</Path>\n",
"<NewFolderName>",Dir,"</NewFolderName>\n",
"</CreateFolder> </soap:Body> </soap:Envelope>\n",NULL);

result=FAWCommand(FS, XML, "http://api.filesanywhere.com/CreateFolder",&Tempstr);

DestroyString(Tempstr);
DestroyString(XML);

return(result);
}


int FAWGenerateShareLink(TFileStore *FS, TFileInfo *FI, char *ToList, char *Passwd)
{
char *Tempstr=NULL, *XML=NULL, *FType=NULL;
int result=FALSE;

if (! FS) 
{
return(FALSE);
}

if (FI->Type==FTYPE_DIR) FType=CopyStr(FType,"folder");
else FType=CopyStr(FType,"file");

XML=MCopyStr(XML,"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n",
"<soap:Envelope xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"",
" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\"",
" xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">\n",
" <soap:Body>\n",
" <SendItemsELinkWithDefaults xmlns=\"http://api.filesanywhere.com/\">\n",
" <Token>",GetVar(FS->Vars,"AuthToken"),"</Token>\n",
" <ELinkItems><Item><Type>",FType,"</Type><Path>",FI->Path,"</Path></Item></ELinkItems>\n",
" <RecipientsEmailAddresses>",ToList,"</RecipientsEmailAddresses><SendEmail>Y</SendEmail>\n",
" <ElinkPassWord>",Passwd,"</ElinkPassWord>\n",
" </SendItemsELinkWithDefaults>\n",
" </soap:Body></soap:Envelope>\n",NULL);
 

result=FAWCommand(FS, XML, "http://api.filesanywhere.com/SendItemsELinkWithDefaults",&Tempstr);


DestroyString(Tempstr);
DestroyString(XML);
DestroyString(FType);

return(result);
}






STREAM *FAWOpenFile(TFileStore *FS, TFileInfo *FI, int Flags)
{
char *Tempstr=NULL, *FileName=NULL, *XML=NULL, *ptr;
char *TagName=NULL, *TagData=NULL;
TFileStore *SendFS=NULL;
ListNode *Headers=NULL;
HTTPInfoStruct *HTTPInfo=NULL;
STREAM *InfoS=NULL;
int result;


if (Flags & OPEN_WRITE)
{
	FileName=HTTPQuote(FileName,FI->Path);
 	HTTPInfo=HTTPInfoFromURL("POST", "http://api.filesanywhere.com/fawapi.asmx");
	SetVar(HTTPInfo->CustomSendHeaders,"SOAPAction", "http://api.filesanywhere.com/AppendChunk");
	HTTPInfo->PostContentType=CopyStr(HTTPInfo->PostContentType,"text/xml; charset=utf-8");
//	HTTPInfo->Flags |= HTTP_KEEPALIVE | HTTP_NOCOMPRESS;	
	HTTPInfo->Flags |= HTTP_KEEPALIVE;	
	InfoS=STREAMCreate();
	STREAMSetValue(InfoS,"filename", FileName);
	STREAMSetValue(InfoS,"filechunkoffset", "0");
	Tempstr=FormatStr(Tempstr,"%d",FI->Size);
	STREAMSetValue(InfoS,"filesize", Tempstr);
}
else
{
	XML=MCopyStr(XML,"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n",
	"<soap:Envelope xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"",
	" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\"",
	" xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">\n",
	"<soap:Body>\n",
	"<DownloadFile xmlns=\"http://api.filesanywhere.com/\">\n",
	"<Token>",GetVar(FS->Vars,"AuthToken"),"</Token>\n",
	"<Path>",FI->Path,"</Path>\n",
	"</DownloadFile> </soap:Body> </soap:Envelope>\n",NULL);

	result=FAWCommand(FS, XML, "http://api.filesanywhere.com/DownloadFile",&Tempstr);

	ptr=XMLGetTag(Tempstr,NULL,&TagName,&TagData);
	while (ptr)
	{
		if (strcasecmp(TagName,"URL")==0)
		{
			ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
			FileName=CopyStr(FileName,TagData);
		}
		ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);

		if (StrLen(FileName))
		{
 			HTTPInfo=HTTPInfoFromURL("GET", FileName);
			HTTPInfo->S=HTTPTransact(HTTPInfo);
			InfoS=HTTPInfo->S;
		}
		else HTTPInfo=NULL;
	}

}


if (! HTTPInfo) return(NULL);
FS->Extra=(void *) HTTPInfo;
ListDestroy(Headers,DestroyString);
DestroyString(Tempstr);
DestroyString(FileName);
DestroyString(TagName);
DestroyString(TagData);
DestroyString(XML);

return(InfoS);
}


int FAWCloseFile(TFileStore *FS, STREAM *S)
{ 
	char *Tempstr=NULL;
	HTTPInfoStruct *Info;
   
	STREAMClose(S);
	Info=(HTTPInfoStruct *) FS->Extra;
	//HTTPInfoDestroy(Info);

	return(TRUE);
}



int FAWWriteBytes(TFileStore *FS, STREAM *InfoS, char *Buffer, int BuffLen)
{
int result=-1, BytesSent, EncodedLen, val;
char *Base64=NULL;
HTTPInfoStruct *HTTPInfo;
char *Tempstr=NULL, *XML=NULL;
char *TagName=NULL, *TagData=NULL, *ptr, *eptr;
STREAM *S;

if (! FS) 
{
return(FALSE);
}

//This is the amount that will be sent after we've finished this chunk
BytesSent=atoi(STREAMGetValue(InfoS,"filechunkoffset"))+BuffLen;

Tempstr=MCopyStr(Tempstr,FS->CurrDir,STREAMGetValue(InfoS,"filename"),NULL);

XML=MCopyStr(XML,"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n",
"<soap:Envelope xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">\n", 
"<soap:Body>\n",
"<AppendChunk xmlns=\"http://api.filesanywhere.com/\">\n",
"<Token>",GetVar(FS->Vars,"AuthToken"),"</Token>\n",
"<Path>",Tempstr,"</Path>\n",NULL);

Base64=EncodeBase64(Base64,Buffer,BuffLen);
EncodedLen=StrLen(Tempstr);

/*
ptr=Tempstr;
eptr=Tempstr+EncodedLen;
if (EncodedLen > 76)
{
	//Break the encoded text up into lines!
	while (ptr < eptr)
	{
		Base64=CatStrLen(Base64,ptr,76);
		Base64=CatStr(Base64,"\r\n");
		ptr+=76;
	}
}
else Base64=CopyStr(Base64,Tempstr);
*/

Tempstr=FormatStr(Tempstr,"%d",BuffLen);
XML=MCatStr(XML,"<ChunkData>",Base64,"</ChunkData>","<Offset>",STREAMGetValue(InfoS,"filechunkoffset"),"</Offset>","<BytesRead>",Tempstr,"</BytesRead>",NULL); 

Tempstr=FormatStr(Tempstr,"%d",BytesSent);
STREAMSetValue(InfoS,"filechunkoffset",Tempstr);

if (BytesSent >= atoi(STREAMGetValue(InfoS,"filesize")))
{
 XML=CatStr(XML,"<isLastChunk>true</isLastChunk>"); 
}
else  XML=CatStr(XML,"<isLastChunk>false</isLastChunk>"); 

XML=CatStr(XML,"</AppendChunk> </soap:Body> </soap:Envelope>");


HTTPInfo=(HTTPInfoStruct *) FS->Extra;


HTTPInfo->PostContentType=CopyStr(HTTPInfo->PostContentType,"text/xml; charset=utf-8");
HTTPInfo->PostData=CopyStr(HTTPInfo->PostData,XML);
HTTPInfo->PostContentLength=StrLen(XML);


S=HTTPTransact(HTTPInfo);

if (S)
{
	if (* HTTPInfo->ResponseCode=='2')	result=BuffLen;
	if (HTTPInfo->ContentLength > 0)
	{
		//Consume any  XML that's sent
		XML=SetStrLen(XML,HTTPInfo->ContentLength);
	  val=STREAMReadBytes(S,XML,HTTPInfo->ContentLength);
	}

if (! (HTTPInfo->Flags & HTTP_KEEPALIVE))
{
 STREAMClose(HTTPInfo->S);
 HTTPInfo->S=NULL;
}
}


DestroyString(TagName);
DestroyString(TagData);
DestroyString(Tempstr);
DestroyString(XML);
DestroyString(Base64);
return(result);
}




int FAWOpen(TFileStore *FS)
{
int result=FALSE;
char *Tempstr=NULL, *XML=NULL;
char *TagName=NULL, *TagData=NULL, *ptr;

if (! FS) 
{
return(FALSE);
}

HTTPSetFlags(HTTP_NOCOMPRESS);

//Always require login for FAW
if (! StrLen(FS->Passwd)) RequestAuthDetails(&FS->Logon,&FS->Passwd);

XML=MCopyStr(XML,"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n",
"<soap:Envelope xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"",
" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\"",
" xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">\n",
"<soap:Body>\n",
"<AccountLogin xmlns=\"http://api.filesanywhere.com/\">\n",
"<APIKey>", API_KEY, "</APIKey>\n",
"<OrgID>0</OrgID>\n",
"<UserName>",FS->Logon,"</UserName>\n",
"<Password>",FS->Passwd,"</Password>\n",
"<ClientEncryptParam>ENCRYPTEDNO</ClientEncryptParam>\n",
//"<AllowedIPList>string</AllowedIPList>",
"</AccountLogin>\n</soap:Body>\n</soap:Envelope>\n",NULL);


if (FAWCommand(FS, XML, LOGIN_URL, &Tempstr))
{
ptr=XMLGetTag(Tempstr,NULL,&TagName,&TagData);
while (ptr)
{
	if (strcasecmp(TagName,"Token")==0)
	{
		ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
		SetVar(FS->Vars,"AuthToken",TagData);
		result=TRUE;
	}
ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
}
}

FS->CurrDir=MCopyStr(FS->CurrDir,"/",FS->Logon,"/",NULL);

DestroyString(TagName);
DestroyString(TagData);
DestroyString(Tempstr);
DestroyString(XML);

return(result);
}

int FAWClose(TFileStore *FS)
{
int result;
char *Tempstr=NULL;


   DestroyString(Tempstr);
return(TRUE);
}


int FAWRenameFile(TFileStore *FS, char *FromPath, char *ToPath)
{
char *Tempstr=NULL, *XML=NULL;
char *QuotedTo=NULL, *QuotedFrom=NULL;
char *TagName=NULL, *TagData=NULL, *ptr;
int result=FALSE;

QuotedFrom=CopyStr(QuotedFrom,FromPath);

//Full path not allowed when changing file name
QuotedTo=CopyStr(QuotedTo,GetBasename(ToPath));

XML=MCopyStr(XML,"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n",
"<soap:Envelope xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"",
" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\"",
" xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">\n",
"<soap:Body>\n",
"<RenameItem xmlns=\"http://api.filesanywhere.com/\">\n",
"<Token>",GetVar(FS->Vars,"AuthToken"),"</Token>\n",
"<Path>",QuotedFrom,"</Path>\n",
"<Type>","file","</Type>\n",
"<NewName>",QuotedTo,"</NewName>\n",
"</RenameItem>\n</soap:Body>\n</soap:Envelope>\n",NULL);

FAWCommand(FS, XML, "http://api.filesanywhere.com/RenameItem",&Tempstr);
ptr=XMLGetTag(Tempstr,NULL,&TagName,&TagData);
while (ptr)
{
if (strcasecmp(TagName,"Renamed")==0)
{
ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
if (strcmp(TagData,"true")==0) result=TRUE;
}
ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
}

DestroyString(QuotedTo);
DestroyString(QuotedFrom);
DestroyString(Tempstr);
DestroyString(TagName);
DestroyString(TagData);
DestroyString(XML);

return(result);
}


int FAWDeleteFile(TFileStore *FS, TFileInfo *FI)
{
int result=FALSE;
char *Tempstr=NULL, *XML=NULL;
char *TagData=NULL, *TagName=NULL, *ptr;

if (! FS) 
{
return(FALSE);
}


XML=MCopyStr(XML,"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n",
"<soap:Envelope xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"",
" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\"",
" xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">\n",
"<soap:Body>\n",
"<DeleteItems xmlns=\"http://api.filesanywhere.com/\">\n",
"<Token>",GetVar(FS->Vars,"AuthToken"),"</Token>\n",NULL);

if (FI->Type==FTYPE_DIR) XML=MCatStr(XML,"<ItemsToDelete> <Item> <Type>folder</Type> <Path>",FI->Path,"</Path> </Item>\n","</ItemsToDelete> </DeleteItems> </soap:Body> </soap:Envelope>\n",NULL);
else XML=MCatStr(XML,"<ItemsToDelete> <Item> <Type>file</Type> <Path>",FI->Path,"</Path> </Item>\n","</ItemsToDelete> </DeleteItems> </soap:Body> </soap:Envelope>\n",NULL);

FAWCommand(FS, XML, "http://api.filesanywhere.com/DeleteItems",&Tempstr);

ptr=XMLGetTag(Tempstr,NULL,&TagName,&TagData);
while (ptr)
{
if (strcasecmp(TagName,"deleted")==0)
{
ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
if (strcmp(TagData,"true")==0) result=TRUE;
}
ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
}



DestroyString(TagName);
DestroyString(TagData);
DestroyString(Tempstr);
DestroyString(XML);

return(result);
}




TFileStore *FAWFileStoreCreate(char *Name, char *ConnectSetup)
{
TFileStore *FS;

FS=(TFileStore *) calloc(1,sizeof(TFileStore));
FS->Flags=FS_WRITEABLE | FS_CHDIR_FULLPATH;
FS->Features=FS_FILE_SIZE;
FS->Create=FAWFileStoreCreate;
FS->Vars=ListCreate();
FS->CurrDir=CopyStr(FS->CurrDir,"");
FS->InitArg=CopyStr(FS->InitArg,ConnectSetup);
FS->Name=CopyStr(FS->Name,Name);
FS->BuffSize=4096 * 10;

FS->Open=FAWOpen;
FS->Close=FAWClose;
FS->ChDir=DefaultChDir;
FS->MkDir=FAWMkDir;
FS->LoadDir=FAWLoadDir;
FS->OpenFile=FAWOpenFile;
FS->CloseFile=FAWCloseFile;
FS->ReadBytes=DefaultReadBytes;
FS->WriteBytes=FAWWriteBytes;
FS->RenameFile=FAWRenameFile;
FS->DeleteFile=FAWDeleteFile;
FS->ShareLink=FAWGenerateShareLink;
//FS->GetFileSize=FAWGetFileSize;

return(FS);
}
