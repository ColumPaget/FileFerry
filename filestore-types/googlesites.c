#define _GNU_SOURCE
#include <fnmatch.h>

#include "google-data.h"
#include "googlesites.h"

#define GSITES_URL "https://sites.google.com/feeds/content/site/"

//this must not clash with any 'open' flags
#define GSITES_XML 4096



HTTPInfoStruct *GSitesOpenWrite(TFileStore *FS, TFileInfo *FI)
{
char *Tempstr=NULL, *XML=NULL;
int Len,BoundLen;
ListNode *Headers;
HTTPInfoStruct *Info;
STREAM *S;

Tempstr=FormatStr(Tempstr,"BOUNDARY-%d-%d-%d",getpid(),time(NULL),rand());
BoundLen=StrLen(Tempstr);
SetVar(FS->Vars,"HTTP-BOUNDARY",Tempstr);

XML=MCopyStr(XML,"--",GetVar(FS->Vars,"HTTP-BOUNDARY"),"\r\n","Content-Type: application/atom+xml\r\n\r\n",NULL);
XML=CatStr(XML,"<entry xmlns=\"http://www.w3.org/2005/Atom\">\n");
XML=CatStr(XML,"<category scheme=\"http://schemas.google.com/g/2005#kind\" term=\"http://schemas.google.com/sites/2008#attachment\" label=\"attachment\"/>\n");
XML=CatStr(XML, "<link rel=\"http://schemas.google.com/sites/2008#parent\" type=\"application/atom+xml\" href=\"http://sites.google.com/feeds/content/site/columscode/2875318601410855137\"/>");
XML=MCatStr(XML,"<title>",FI->Name,"</title></entry>\r\n\r\n",NULL);
//  <summary>HR packet</summary>

if (StrLen(FI->MediaType)) XML=MCatStr(XML,"--",GetVar(FS->Vars,"HTTP-BOUNDARY"),"\r\n","Content-Type: ",FI->MediaType,"\r\n\r\n",NULL);
else XML=MCatStr(XML,"--",GetVar(FS->Vars,"HTTP-BOUNDARY"),"\r\n","Content-Type: application/octet-stream\r\n\r\n",NULL);

Len=StrLen(XML)+ FI->Size + 4 + BoundLen + 4;
Tempstr=FormatStr(Tempstr,"%d",Len);

Headers=ListCreate();
SetVar(Headers,"Content-Length",Tempstr);
Tempstr=MCopyStr(Tempstr,"multipart/related; boundary=",GetVar(FS->Vars,"HTTP-BOUNDARY"),NULL);
SetVar(Headers,"Content-Type",Tempstr);

Tempstr=MCopyStr(Tempstr,GSITES_URL,FS->Host,NULL);
Info=GDataConnect(FS,"POST", Tempstr, NULL,Headers,0);
STREAMWriteLine(XML,Info->S);

ListDestroy(Headers,DestroyString);
DestroyString(Tempstr);

return(Info);
}


HTTPInfoStruct *GSitesOpenAtomPage(TFileStore *FS, char *Method, char *URL, int Size)
{
char *Tempstr=NULL;
int Len,BoundLen;
ListNode *Headers;
HTTPInfoStruct *Info;
STREAM *S;

Headers=ListCreate();
Tempstr=FormatStr(Tempstr,"%d",Size);
SetVar(Headers,"Content-Length",Tempstr);
Tempstr=MCopyStr(Tempstr,"application/atom+xml",NULL);
SetVar(Headers,"Content-Type",Tempstr);
if (strcmp(Method,"PUT")==0) SetVar(Headers,"If-Match","*");

Info=GDataConnect(FS,Method, URL, NULL,Headers,0);

ListDestroy(Headers,DestroyString);

DestroyString(Tempstr);

return(Info);
}


HTTPInfoStruct *GSitesOpenHtmlPage(TFileStore *FS, char *Method, char *URL, char *FileName, int DocSize)
{
HTTPInfoStruct *Info;
char *HeaderXML=NULL, *TrailerXML=NULL;
int Size;

HeaderXML=MCopyStr(HeaderXML,"<entry xmlns=\"http://www.w3.org/2005/Atom\">\n",
" <category scheme=\"http://schemas.google.com/g/2005#kind\"\n",
" term=\"http://schemas.google.com/sites/2008#webpage\" label=\"webpage\"/>\n",
" <title>", FileName, "</title>\n"
" <content type=\"xhtml\">\n"
" <div xmlns=\"http://www.w3.org/1999/xhtml\">\n\n<!-- ************ YOUR HTML GOES BELOW THIS LINE ************ -->\n\n",NULL);

TrailerXML=CopyStr(TrailerXML,"\n\n<!-- ************ YOUR HTML GOES BELOW THIS LINE ************ -->\n\n</div></content></entry>\n");

Size=StrLen(HeaderXML) + DocSize + StrLen(TrailerXML);
Info=GSitesOpenAtomPage(FS, Method, URL, Size);
STREAMWriteLine(HeaderXML,Info->S);

if (Info->S) STREAMSetValue(Info->S,"TrailerXML", TrailerXML);


DestroyString(TrailerXML);
DestroyString(HeaderXML);

return(Info);
}



HTTPInfoStruct *GSitesOpenWebPage(TFileStore *FS, char *Path, int Size, int Flags)
{
ListNode *Node;
HTTPInfoStruct *Info=NULL;
TFileInfo *FI;
char *URL=NULL;

		Node=ListFindNamedItem(FS->DirListing,Path);
		if (Node)
		{
			FI=(TFileInfo *) Node->Item;
			if (Flags & GSITES_XML) Info=GSitesOpenAtomPage(FS,"PUT",FI->EditPath,Size);
			else Info=GSitesOpenHtmlPage(FS,"PUT",FI->EditPath,Path,Size);
		}
		else 
		{
			URL=MCopyStr(URL,GSITES_URL,FS->Host,NULL);
			if (Flags & GSITES_XML) Info=GSitesOpenAtomPage(FS,"POST",URL,Size);
			else Info=GSitesOpenHtmlPage(FS,"POST",URL,Path,Size);

		}

DestroyString(URL);
return(Info);
}



STREAM *GSitesOpenFile(TFileStore *FS, TFileInfo *FI, int Flags)
{
char *Path=NULL;
ListNode *Node;
HTTPInfoStruct *Info=NULL;

	if (Flags & OPEN_WRITE) 
	{
		if (strcmp(FI->MediaType,"application/googlesites")==0) Info=GSitesOpenWebPage(FS,FI->Path,FI->Size,Flags | GSITES_XML);
		else if (strcmp(FI->MediaType,"text/html")==0) Info=GSitesOpenWebPage(FS,FI->Path,FI->Size,Flags);
		else Info=GSitesOpenWrite(FS, FI);
		if (Info && Info->S) Info->S->Flags |=SF_WRONLY;
	}
	else
	{
		Path=MCopyStr(Path,FI->Path,"?prettyprint=true",NULL);
		Info=GDataConnect(FS,"GET", Path, NULL,NULL,0);
	}

	FS->Extra=(void *) Info;

	DestroyString(Path);

   return(Info->S);
}



int GSitesLoadDir(TFileStore *FS, char *InPattern, ListNode *Items, int Flags)
{
int result;
char *Tempstr=NULL;

	Tempstr=MCopyStr(Tempstr,GSITES_URL,FS->Host,"?prettyprint=true",NULL);
	GDataLoadDir(FS, Tempstr, InPattern, Items, Flags);

DestroyString(Tempstr);
return(TRUE);
}



int GSitesCloseFile(TFileStore *FS, STREAM *S)
{ 
	char *Tempstr=NULL, *Response=NULL;
	HTTPInfoStruct *Info;
	int RetVal=FALSE;
            
	Tempstr=CopyStr(Tempstr,STREAMGetValue(S,"TrailerXML"));
	if (StrLen(Tempstr))
	{
		STREAMWriteLine(Tempstr, S);
		STREAMFlush(S);
	}

	Info=(HTTPInfoStruct *) FS->Extra;

	//if doing a post
	if (S->Flags & SF_WRONLY)
	{
		Tempstr=MCopyStr(Tempstr,"\r\n","--",GetVar(FS->Vars,"HTTP-BOUNDARY"),"--\r\n",NULL);
		STREAMWriteLine(Tempstr,Info->S);

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



int GSitesRenameFile(TFileStore *FS, char *FromPath, char *ToPath)
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




int GSitesOpen(TFileStore *FS)
{
HTTPInfoStruct *Info;
char *Tempstr=NULL;
int result=FALSE;

if (! FS) 
{
return(FALSE);
}

FS->S=STREAMCreate();

printf("PASS: [%s]\n",FS->Passwd);
//if (! StrLen(FS->Passwd)) RequestAuthDetails(&FS->Logon,&FS->Passwd);

if (! StrLen(FS->Passwd)) 
{
printf("OAuth!\n");
	SetVar(FS->Vars,"OAuthScope","https://sites.google.com/feeds");
	GoogleOAuth(FS);
	Tempstr=MCopyStr(Tempstr, "Bearer ", FS->Passwd, NULL);
}
else if (FS->Settings & FS_OAUTH) Tempstr=MCopyStr(Tempstr, "Bearer ", FS->Passwd, NULL);
else 
{
	SetVar(FS->Vars,"ClientLoginScope","jotspot");
	GoogleAuth(FS, &Tempstr);
}

if (StrLen(Tempstr))
{
SetVar(FS->Vars,"AuthToken",Tempstr);
result=TRUE;

Tempstr=MCopyStr(Tempstr,GSITES_URL,FS->Host,NULL);

Info=GDataConnect(FS,"HEAD", Tempstr,"", NULL, 0);
if (Info) 
{
	PrintConnectionDetails(FS,Info->S);
	HTTPInfoDestroy(Info);
}
}

DestroyString(Tempstr);

return(result);
}



int GSitesClose(TFileStore *FS)
{
int result;
char *Tempstr=NULL;


   DestroyString(Tempstr);
return(TRUE);
}




TFileStore *GSitesFileStoreCreate(char *Name, char *ConnectSetup)
{
TFileStore *FS;

FS=(TFileStore *) calloc(1,sizeof(TFileStore));
FS->Flags=FS_WRITEABLE | FS_NOEXTENSION | FS_SSL;
FS->Features=FS_FILE_SIZE;
FS->Vars=ListCreate();
SetVar(FS->Vars,"GData-Version", "1.4");
FS->CurrDir=CopyStr(FS->CurrDir,"");


FS->Create=GSitesFileStoreCreate;
FS->Open=GSitesOpen;
FS->Close=GSitesClose;
//FS->ChDir=GSitesChDir;
FS->LoadDir=GSitesLoadDir;
FS->DeleteFile=GDataDeleteFile;

FS->OpenFile=GSitesOpenFile;
FS->ReadBytes=DefaultReadBytes;
FS->WriteBytes=DefaultWriteBytes;
FS->CloseFile=GSitesCloseFile;

/*
FS->MkDir=GSitesMkDir;
FS->RenameFile=GSitesRenameFile;
FS->GetFileSize=FileStoreGetFileSize;
*/

FS->InitArg=CopyStr(FS->InitArg,ConnectSetup);
FS->Name=CopyStr(FS->Name,Name);

return(FS);
}
