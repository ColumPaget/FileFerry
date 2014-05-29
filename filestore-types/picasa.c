#define _GNU_SOURCE
#include <fnmatch.h>

#include "google-data.h"
#include "picasa.h"

#define PICASA_FEED_URL "http://picasaweb.google.com/data/feed/api/user/default"
#define PICASA_ENTRY_URL "http://picasaweb.google.com/data/entry/api/user/default"
#define PICASA_DATE_FMT "%Y-%m-%dT%H:%M:%S"


HTTPInfoStruct *PicasaConnect(TFileStore *FS, char *Method, char *URL, char *PostData, ListNode *Headers)
{
char *AuthToken=NULL;
HTTPInfoStruct *Info;
ListNode *Curr;


	AuthToken=CopyStr(AuthToken, GetVar(FS->Vars,"AuthToken"));
	Info=HTTPInfoFromURL(Method, URL);
	SetVar(Info->CustomSendHeaders,"GData-Version", "2");
	if (StrLen(AuthToken))
	{
		SetVar(Info->CustomSendHeaders,"Authorization",AuthToken);
	}


	Curr=ListGetNext(Headers);
	while (Curr)
	{
	if (strcasecmp(Curr->Tag,"content-type")==0) Info->PostContentType=CopyStr(Info->PostContentType,(char *) Curr->Item);
	else SetVar(Info->CustomSendHeaders,Curr->Tag, (char *) Curr->Item);
	Curr=ListGetNext(Curr);
	}

	if (StrLen(PostData) > 0) Info->PostData=CopyStr(Info->PostData,PostData);
 	Info->S=HTTPTransact(Info);

 	if ((strcmp(Method,"POST")==0) && (StrLen(PostData)==0)) 
 	{
		if (Info->S) Info->S->Flags |= SF_WRONLY;
 	}


	if ((strcmp(Info->ResponseCode,"403")==0) && (FS->Settings & FS_OAUTH))
	{
  	GoogleOAuth(FS);
	
  	AuthToken=MCopyStr(AuthToken, "Bearer ", FS->Passwd, NULL);
		SetVar(FS->Vars,"AuthToken",AuthToken);
		Info=PicasaConnect(FS, Method, URL, PostData, Headers);
	}

	DestroyString(AuthToken);

	return(Info);
}



int PicasaChDir(TFileStore *FS, char *Dir)
{
char *Tempstr=NULL, *ptr;
int result=FALSE;
TFileInfo *FI;

//Only one level of album allowed in picasa, therefor '..' is '/'
if (strcmp(Dir,"..")==0) Tempstr=CopyStr(Tempstr,"/");
else
{
ptr=Dir;
if (*ptr=='/') ptr++;
Tempstr=CopyStr(Tempstr,ptr);
StripDirectorySlash(Tempstr);
}

	if (StrLen(Tempstr) > 1)
	{
		FI=FileStoreGetFileInfo(FS,Tempstr);

	  if (FI)
		{
			FS->CurrDir=MCopyStr(FS->CurrDir,"/",FI->Name,"/",NULL);
			FS->CurrDirPath=CopyStr(FS->CurrDirPath,FI->Path);
			result=TRUE;
		}
	}
	else
	{
			FS->CurrDir=MCopyStr(FS->CurrDir,"/",NULL);
			FS->CurrDirPath=CopyStr(FS->CurrDirPath,"");
			result=TRUE;
	}
	

DestroyString(Tempstr);
return(result);
}



void PicasaParseContent(TFileInfo *FI,char *TagData)
{
char *Name=NULL, *Value=NULL, *ptr;
char *Type=NULL, *URL=NULL;
char *ptr2;

ptr=GetNameValuePair(TagData, " ", "=", &Name, &Value);
while (ptr)
{
	if (
			(strcmp(Name,"src")==0) ||
			(strcmp(Name,"url")==0) 
		)
	{
		ptr2=strrchr(Value,'\'');
		if (ptr2) *ptr2='\0';

		if (strcmp(Name,"url")==0) 
		{
		URL=HtmlDeQuote(URL, Value);
		}
		else URL=CopyStr(URL, Value);
	}
	else if (strcmp(Name,"type")==0) Type=CopyStr(Type,Value);
	ptr=GetNameValuePair(ptr, " ", "=", &Name, &Value);
}

if (
			(strcmp(Type,"video/mpeg4")==0) ||
			(StrLen(FI->MediaType)==0)
	)
{
	FI->MediaType=CopyStr(FI->MediaType,Type);
	SetVar(FI->Vars,"DownloadURL",URL);
}

DestroyString(URL);
DestroyString(Name);
DestroyString(Type);
DestroyString(Value);
}



void PicasaParseLink(TFileInfo *FI,char *TagData)
{
char *Name=NULL, *Value=NULL, *ptr=NULL;
char *Type=NULL, *URL=NULL;

ptr=GetNameValuePair(TagData," ","=",&Name,&Value);
while (ptr)
{

if (strcmp(Name,"rel")==0) Type=CopyStr(Type,Value);
if (strcmp(Name,"href")==0) URL=CopyStr(URL,Value);

ptr=GetNameValuePair(ptr," ","=",&Name,&Value);
}

// rel='edit' type='application/atom+xml' href='http://picasaweb.google.com/data/entry/api/user/110641013609214344442/albumid/5811200235841478305?authkey=Gv1sRgCPP6yYLLge6V2QE'

if (StrLen(Type) && (strcmp(Type,"edit")==0)) FI->EditPath=CopyStr(FI->EditPath,URL);

DestroyString(Value);
DestroyString(Name);
DestroyString(Type);
DestroyString(URL);
}


TFileInfo *PicasaReadFileEntry(char **XML)
{
char *Token=NULL, *TagName=NULL, *TagData=NULL, *ptr;
char *gTags[]={"gphoto:id","gphoto:size","gphoto:width","gphoto:height","gphoto:access",NULL};
typedef enum {GPHOTO_ID, GPHOTO_SIZE,GPHOTO_WIDTH,GPHOTO_HEIGHT, GPHOTO_ACCESS};
char *Tags[]={"title","content","media:content","published","updated","link",NULL};
typedef enum {PICASA_TITLE,PICASA_CONTENT,PICASA_MEDIACONTENT,PICASA_PUBLISHED,PICASA_UPDATED,PICASA_LINK};
TFileInfo *FI=NULL;
int result, InEntry=TRUE;
int Width=0, Height=0;

FI=(TFileInfo *) calloc(1,sizeof(TFileInfo));
FI->Vars=ListCreate();
FI->Path=CopyStr(FI->Path,"????");
FI->Name=CopyStr(FI->Name,"????");
*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);

while (*XML && InEntry)
{
			switch (*TagName)
			{
			case 'g':
				result=MatchTokenFromList(TagName,gTags,0);
				switch (result)
				{
					case GPHOTO_ID:
						*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
						FI->Path=HTTPQuote(FI->Path,TagData);
						GetToken(TagData,":",&Token,0);
					break;

					case GPHOTO_SIZE:
						*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
						FI->Size=atoi(TagData);
					break;

					case GPHOTO_WIDTH:
						*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
						Width=atoi(TagData);
					break;

					case GPHOTO_HEIGHT:
						*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
						Height=atoi(TagData);
					break;

					case GPHOTO_ACCESS:
						*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
						FI->Permissions=CopyStr(FI->Permissions, TagData);
					break;

				}
			break;

			case '/':
				if (strcmp(TagName,"/entry")==0) InEntry=FALSE;
			break;
	
			case 't':			
			case 'c':			
			case 'l':			
			case 'm':			
			case 'p':			
			result=MatchTokenFromList(TagName,Tags,0);
			switch (result)
			{
				case PICASA_TITLE: 
					*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
					FI->Name=CopyStr(FI->Name,TagData);
				break;

				case PICASA_LINK: 
					if (FI) PicasaParseLink(FI,TagData);
				break;

				case PICASA_CONTENT:
				case PICASA_MEDIACONTENT:
					if (FI) PicasaParseContent(FI,TagData);
				break;
		/*
    <published>2012-11-16T00:24:51.000Z</published>
    <updated>2012-11-16T00:58:44.770Z</updated>
    <app:edited>2012-11-16T00:58:44.770Z</app:edited>
		*/
				case PICASA_PUBLISHED:
					*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);

					FI->Mtime=DateStrToSecs(PICASA_DATE_FMT,TagData,NULL);
				break;
			}
			break;

			}

if (InEntry) *XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
}

if (Width > 0)
{
Token=FormatStr(Token,"%dx%d",Width,Height);
SetVar(FI->Vars,"MediaDimensions",Token);
}

DestroyString(TagName);
DestroyString(TagData);
DestroyString(Token);

return(FI);
}


int PicasaLoadDir(TFileStore *FS, char *InPattern, ListNode *Items, int Flags)
{
int result;
char *Tempstr=NULL, *XML=NULL;
char *TagName=NULL, *TagData=NULL, *ptr;
TFileInfo *FI=NULL;
HTTPInfoStruct *Info;

	if (StrLen(FS->CurrDirPath)==0) Tempstr=MCopyStr(Tempstr,PICASA_FEED_URL,"?imgmax=d&prettyprint=true",NULL);
	else Tempstr=MCopyStr(Tempstr,PICASA_FEED_URL,"/albumid/",FS->CurrDirPath,"?imgmax=d&prettyprint=true",NULL);
	Info=PicasaConnect(FS, "GET", Tempstr, "", NULL);
	Tempstr=STREAMReadLine(Tempstr,Info->S);
	while (Tempstr)
	{
	XML=CatStr(XML,Tempstr);
	Tempstr=STREAMReadLine(Tempstr,Info->S);
	}


	ptr=XMLGetTag(XML,NULL,&TagName,&TagData);
	while (ptr)
	{
		if (strcmp(TagName,"entry")==0) 
		{
				FI=PicasaReadFileEntry(&ptr);

				if (Items) ListAddNamedItem(Items,FI->Name,FI);
				if ( (StrLen(FS->CurrDir)==0) || (strcmp(FS->CurrDir,"/")==0) )FI->Type=FTYPE_DIR;
				else FI->Type=FTYPE_FILE;
		}

		if (strcmp(TagName,"gphoto:quotalimit")==0)
		{
			ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
			FS->BytesAvailable=strtod(TagData,NULL);
		}

		if (strcmp(TagName,"gphoto:quotacurrent")==0)
		{
			ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
			FS->BytesUsed=strtod(TagData,NULL);
		}

		ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
	}


DestroyString(TagName);
DestroyString(TagData);
DestroyString(Tempstr);
DestroyString(XML);
return(TRUE);
}





int PicasaMkDir(TFileStore *FS, char *Dir)
{
char *Tempstr=NULL, *XML=NULL, *ptr;
HTTPInfoStruct *Info;
ListNode *Headers;
TFileInfo *FI;
int result=FALSE;


if (StrLen(FS->CurrDir) > 1) return(FALSE);

Headers=ListCreate();
XML=MCopyStr(XML,"<entry xmlns='http://www.w3.org/2005/Atom' ",
"xmlns:media='http://search.yahoo.com/mrss/' ",
"xmlns:gphoto='http://schemas.google.com/photos/2007'> ",
"<title type='text'>",Dir,"</title>",
"<summary type='text'></summary>",
"<gphoto:location></gphoto:location>",
"<gphoto:access>private</gphoto:access>",
"<media:group><media:keywords></media:keywords></media:group>",
"<category scheme='http://schemas.google.com/g/2005#kind' ",
"term='http://schemas.google.com/photos/2007#album'></category>",
"</entry>",NULL);

SetVar(Headers,"Content-type","application/atom+xml");
Tempstr=FormatStr(Tempstr,"%d",StrLen(XML));
SetVar(Headers,"Content-length",Tempstr);

Info=PicasaConnect(FS, "POST", PICASA_FEED_URL, XML,Headers);

if (Info && (*Info->ResponseCode=='2')) result=TRUE;

XML=CopyStr(XML,"");
Tempstr=STREAMReadLine(Tempstr,Info->S);
while (Tempstr)
{
XML=CatStr(XML,Tempstr);
Tempstr=STREAMReadLine(Tempstr,Info->S);
}

ptr=XML;
FI=PicasaReadFileEntry(&ptr);
if (FI) ListAddNamedItem(FS->DirListing,FI->Name,FI);

ListDestroy(Headers,DestroyString);
HTTPInfoDestroy(Info);
DestroyString(Tempstr);
DestroyString(XML);

return(result);
}




STREAM *PicasaOpenFile(TFileStore *FS, TFileInfo *OpenFI, int Flags)
{
char *Tempstr=NULL, *FileName=NULL, *TagData=NULL, *Boundary=NULL, *ptr;
TFileStore *SendFS=NULL;
ListNode *Headers=NULL, *Curr;
HTTPInfoStruct *Info=NULL;
TFileInfo *FI=NULL;

  if (Flags & OPEN_WRITE)
	{
		Headers=ListCreate();
		Boundary=FormatStr(Boundary,"END_OF_PART-%d-%d-%d",getpid(),time(NULL),rand());
		FileName=HTTPQuote(FileName,OpenFI->Path);
		Tempstr=MCopyStr(Tempstr,"multipart/related; boundary=\"",Boundary,"\"",NULL);
		SetVar(Headers,"Content-Type",Tempstr);
		//SetVar(Headers,"Slug",FileName);

		TagData=MCopyStr(TagData,"--",Boundary,"\r\nContent-Type: application/atom+xml\r\n\r\n<entry xmlns='http://www.w3.org/2005/Atom'><title>",FileName,"</title><summary>",FileName,"</summary><category scheme=\"http://schemas.google.com/g/2005#kind\" term=\"http://schemas.google.com/photos/2007#photo\"/></entry>\r\n--",Boundary,"\r\nContent-Type: ",OpenFI->MediaType,"\r\n\r\n",NULL),

		Tempstr=FormatStr(Tempstr,"%d",StrLen(TagData)+OpenFI->Size+StrLen(Boundary)+6);
		SetVar(Headers,"Content-Length",Tempstr);
		SetVar(Headers,"MIME-version","1.0");

		Tempstr=MCopyStr(Tempstr,PICASA_FEED_URL,"/albumid/",FS->CurrDirPath,NULL);
		Info=PicasaConnect(FS, "POST", Tempstr, NULL,Headers);
		STREAMSetValue(Info->S,"Boundary",Boundary);
		STREAMWriteLine(TagData,Info->S);
	}
	else
	{
			FI=FileStoreGetFileInfo(FS, OpenFI->Path);
			if (FI)
			{
				Info=PicasaConnect(FS, "GET", GetVar(FI->Vars,"DownloadURL"), NULL,NULL);
				//if we got a 'Content-Length' header, then take that as
				//the real size of the item
				if (Info->ContentLength > 0) OpenFI->Size=Info->ContentLength;
			}
	}

		ListDestroy(Headers,DestroyString);
		DestroyString(Boundary);
		DestroyString(TagData);
		DestroyString(FileName);
		DestroyString(Tempstr);

	 if (! Info) return(NULL);
	 STREAMSetItem(Info->S,"PicasaFileTransferHTTPInfo",Info);
   return(Info->S);
}


int PicasaCloseFile(TFileStore *FS, STREAM *S)
{ 
	char *Tempstr=NULL;
	HTTPInfoStruct *Info;
	int RetVal;
            
	Info=(HTTPInfoStruct *) STREAMGetItem(S,"PicasaFileTransferHTTPInfo");
	if (S->Flags & SF_WRONLY)
	{
		Tempstr=MCopyStr(Tempstr,"\r\n--",STREAMGetValue(S,"Boundary"),"--",NULL);
		STREAMWriteLine(Tempstr,S);
		STREAMFlush(S);
		HTTPTransact(Info);
		Tempstr=STREAMReadDocument(Tempstr,S,FALSE);
		sleep(1);
	}
	RetVal=HTTPParseResponse(Info->ResponseCode);
	
	STREAMClose(S);
	HTTPInfoDestroy(Info);

	return(RetVal);
}






int PicasaRenameFile(TFileStore *FS, char *FromPath, char *ToPath)
{
char *Tempstr=NULL;
char *QuotedTo=NULL, *QuotedFrom=NULL;

QuotedTo=QuoteCharsInStr(QuotedTo,ToPath,"'");
QuotedFrom=QuoteCharsInStr(QuotedFrom,FromPath,"'");


DestroyString(QuotedTo);
DestroyString(QuotedFrom);
DestroyString(Tempstr);
}


int PicasaDeleteFile(TFileStore *FS, TFileInfo *FI)
{
char *Tempstr=NULL;
HTTPInfoStruct *Info;
ListNode *Headers;
int result=FALSE;

Headers=ListCreate();
SetVar(Headers,"If-Match","*");

if (FI->Type==FTYPE_DIR) Tempstr=CopyStr(Tempstr,FI->EditPath);
else if ((StrLen(FS->CurrDir)==0) || (strcmp(FS->CurrDir,"/")==0)) Tempstr=MCopyStr(Tempstr,PICASA_ENTRY_URL,"/albumid/default/photoid/",FI->Path,NULL);
else Tempstr=MCopyStr(Tempstr,PICASA_ENTRY_URL,"/albumid/",FS->CurrDirPath,"/photoid/",FI->Path,NULL);

Info=PicasaConnect(FS, "DELETE", Tempstr, NULL,Headers);
Tempstr=STREAMReadDocument(Tempstr,Info->S,FALSE);
if (*Info->ResponseCode=='2') result=TRUE;

ListDestroy(Headers,DestroyString);
HTTPInfoDestroy(Info);
DestroyString(Tempstr);

return(result);
}


int PicasaOpen(TFileStore *FS)
{
char *Tempstr=NULL;
int result=FALSE;
HTTPInfoStruct *Info;

if (! FS) 
{
return(FALSE);
}

FS->S=STREAMCreate();

//if (! StrLen(FS->Passwd)) RequestAuthDetails(&FS->Logon,&FS->Passwd);
if (! StrLen(FS->Passwd))
{
  SetVar(FS->Vars,"OAuthScope","https://www.googleapis.com/auth/userinfo.profile http://picasaweb.google.com/data/");
  GoogleOAuth(FS);
  Tempstr=MCopyStr(Tempstr, "Bearer ", FS->Passwd, NULL);
}
else if (FS->Settings & FS_OAUTH) Tempstr=MCopyStr(Tempstr, "Bearer ", FS->Passwd, NULL);
else
{
  SetVar(FS->Vars,"ClientLoginScope","lh2");
  GoogleAuth(FS, &Tempstr);
}


if (StrLen(Tempstr))
{
SetVar(FS->Vars,"AuthToken",Tempstr);
result=TRUE;
}	


Info=PicasaConnect(FS, "GET", PICASA_FEED_URL, "",NULL);
PicasaChDir(FS,"/");
if (Info->S) PrintConnectionDetails(FS,Info->S);
HTTPInfoDestroy(Info);
	

DestroyString(Tempstr);

return(result);
}


int PicasaClose(TFileStore *FS)
{
int result;
char *Tempstr=NULL;


   DestroyString(Tempstr);
return(TRUE);
}



TFileStore *PicasaFileStoreCreate(char *Name, char *ConnectSetup)
{
TFileStore *FS;

FS=(TFileStore *) calloc(1,sizeof(TFileStore));
FS->Flags=FS_WRITEABLE|FS_NO_SUBFOLDERS;
FS->Features=FS_FILE_SIZE;
FS->Vars=ListCreate();
FS->CurrDir=CopyStr(FS->CurrDir,"");
FS->InitArg=CopyStr(FS->InitArg,ConnectSetup);
FS->Name=CopyStr(FS->Name,Name);

SetVar(FS->Vars,"LSFormat:Details","%s %P %m  %n ");


FS->Create=PicasaFileStoreCreate;
FS->Open=PicasaOpen;
FS->Close=PicasaClose;
FS->ChDir=PicasaChDir;
FS->MkDir=PicasaMkDir;
FS->LoadDir=PicasaLoadDir;
FS->OpenFile=PicasaOpenFile;
FS->CloseFile=PicasaCloseFile;
FS->ReadBytes=DefaultReadBytes;
FS->WriteBytes=DefaultWriteBytes;
FS->RenameFile=PicasaRenameFile;
FS->DeleteFile=PicasaDeleteFile;
FS->GetFileSize=FileStoreGetFileSize;

return(FS);
}
