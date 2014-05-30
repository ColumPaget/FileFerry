#define _GNU_SOURCE
#include <fnmatch.h>

#include "google-data.h"
#include "picasa.h"

time_t StartTime;

void IDriveParseResponseItem(char *Data, ListNode *Vars)
{
char *Name=NULL, *Value=NULL, *ptr;

ptr=Data+StrLen(Data)-1;
if (*ptr=='/') *ptr='\0';

ptr=GetNameValuePair(Data," ","=",&Name,&Value);
while (ptr)
{
	SetVar(Vars,Name,Value);
	ptr=GetNameValuePair(ptr," ","=",&Name,&Value);
}

DestroyString(Name);
DestroyString(Value);
}


void IDriveParseResponse(char *Data, ListNode *Vars)
{
char *TagName=NULL, *TagData=NULL, *ptr;

ptr=XMLGetTag(Data,NULL,&TagName,&TagData);
while (ptr)
{
IDriveParseResponseItem(TagData, Vars);
ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
}

DestroyString(TagName);
DestroyString(TagData);
}

int IDriveParseStatusResponse(char *Data, char **Error)
{
char *TagName=NULL, *TagData=NULL, *ptr;
int result=FALSE;
ListNode *Vars=NULL;

Vars=ListCreate();
IDriveParseResponse(Data, Vars);

if (strcmp(GetVar(Vars,"message"),"SUCCESS")==0) result=TRUE;
else *Error=CopyStr(*Error,GetVar(Vars,"desc"));

ListDestroy(Vars,DestroyString);
DestroyString(TagName);
DestroyString(TagData);

return(result);
}



TFileInfo *IDriveReadFileEntry(char *Data)
{
char *Name=NULL, *Value=NULL, *ptr;
TFileInfo *FI=NULL;

FI=(TFileInfo *) calloc(1,sizeof(TFileInfo));
ptr=GetNameValuePair(Data," ","=",&Name,&Value);
while (ptr)
{
	if (strcmp(Name,"restype")==0) 
	{
		if (atoi(Value)==0) FI->Type=FTYPE_DIR;
	}
	if (strcmp(Name,"resname")==0) 
	{
		FI->Name=CopyStr(FI->Name,Value);
		FI->Path=CopyStr(FI->Path,Value);
	}

	if (strcmp(Name,"size")==0) FI->Size=atoi(Value);
	if (strcmp(Name,"ver")==0) FI->Version=atoi(Value);
	if (strcmp(Name,"lmd")==0) FI->Mtime=DateStrToSecs("%Y/%m/%d %H:%M:%S",Value,NULL);

ptr=GetNameValuePair(ptr," ","=",&Name,&Value);
}

DestroyString(Value);
DestroyString(Name);

return(FI);
}


int IDriveLoadDir(TFileStore *FS, char *InPattern, ListNode *Items, int Flags)
{
int result;
char *Tempstr=NULL, *XML=NULL;
char *TagName=NULL, *TagData=NULL, *ptr;
TFileInfo *FI;
HTTPInfoStruct *Info;
ListNode *Vars;

	Tempstr=MCopyStr(Tempstr,"https://",FS->Host,"/evs/browseFolder?uid=",FS->Logon,"&pwd=",FS->Passwd,"&p=",FS->CurrDir,NULL);

	FS->S=HTTPMethod("POST",Tempstr,"","");
	Tempstr=STREAMReadDocument(Tempstr, FS->S, TRUE);
	
	ptr=XMLGetTag(Tempstr,NULL,&TagName,&TagData);
	while (ptr)
	{
		if (strcmp(TagName,"item")==0) 
		{
				FI=IDriveReadFileEntry(TagData);
				if (Items) ListAddNamedItem(Items,FI->Name,FI);
		}

		ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
	}

STREAMClose(FS->S);
FS->S=NULL;

Tempstr=MCopyStr(Tempstr,"https://",FS->Host,"/evs/getAccountQuota?uid=",FS->Logon,"&pwd=",FS->Passwd,NULL);
FS->S=HTTPMethod("POST",Tempstr,"","");
Tempstr=STREAMReadDocument(Tempstr, FS->S, TRUE);

Vars=ListCreate();
IDriveParseResponse(Tempstr,Vars);

FS->BytesAvailable=strtod(GetVar(Vars,"totalquota"),NULL);
FS->BytesUsed=strtod(GetVar(Vars,"usedquota"),NULL);

ListDestroy(Vars,DestroyString);

DestroyString(TagName);
DestroyString(TagData);
DestroyString(Tempstr);
DestroyString(XML);

return(TRUE);
}




STREAM *IDriveOpenFile(TFileStore *FS, TFileInfo *FI, int Flags)
{
char *URL=NULL, *Tempstr=NULL, *Boundary=NULL, *FullPath=NULL, *ptr;
HTTPInfoStruct *Info;
TFileInfo *tmpFI;

  if (Flags & OPEN_WRITE)
	{
  URL=MCopyStr(URL,"https://",FS->Host,"/evs/uploadFile",NULL);
	Info=HTTPInfoFromURL("POST",URL);
	
	Boundary=FormatStr(Boundary,"%x-%x-%x",getpid(),time(NULL),rand());
	Info->PostContentType=MCopyStr(Info->ContentType,"multipart/form-data; boundary=",Boundary,NULL);

	Tempstr=MCopyStr(Tempstr,"--",Boundary,"\r\n","Content-disposition: form-data; name=uid\r\n\r\n",FS->Logon,"\r\n",NULL);
	Tempstr=MCatStr(Tempstr,"--",Boundary,"\r\n","Content-disposition: form-data; name=pwd\r\n\r\n",FS->Passwd,"\r\n",NULL);
	Tempstr=MCatStr(Tempstr,"--",Boundary,"\r\n","Content-disposition: form-data; name=p\r\n\r\n",FS->CurrDir,"\r\n",NULL);
	//Tempstr=MCatStr(Tempstr,"--",Boundary,"\r\n","Content-disposition: form-data; name=myfiles\r\n\r\n",Path,"\r\n",NULL);
	Tempstr=MCatStr(Tempstr,"--",Boundary,"\r\n","Content-disposition: form-data; charset=UTF-8; name=definition; filename=",FI->Path,"\r\nContent-type: image/jpeg\r\n\r\n",NULL);

	Info->PostContentLength=FI->Size+StrLen(Tempstr)+StrLen(Boundary)+8;
	FS->S=HTTPTransact(Info);
	FS->Extra=Info;

	STREAMWriteLine(Tempstr,FS->S);
	STREAMSetValue(FS->S,"Boundary",Boundary);
	Tempstr=FormatStr(Tempstr,"%d",FI->Size);
	STREAMSetValue(FS->S,"Transfer-Size",Tempstr);
	}
	else
	{
			if (*FI->Path=='/') FullPath=CopyStr(FullPath,FI->Path);
			else FullPath=MCopyStr(FullPath,FS->CurrDir,FI->Path,NULL);

			Tempstr=HTTPQuote(Tempstr,FullPath);

//		if (OpenFI->Version) FI=OpenFI;
//		else FI=FileStoreGetFileInfo(FS, FI->Path);

		  URL=FormatStr(URL,"https://%s/evs/downloadFile?uid=%s&pwd=%s&p=%s&version=%d",FS->Host,FS->Logon,FS->Passwd,Tempstr,FI->Version);
			FS->S=HTTPMethod("POST",URL,"","");
			FS->Extra=NULL;
//		if (FI != OpenFI) FileInfoDestroy(FI);
	}

	DestroyString(URL);
	DestroyString(FullPath);
	DestroyString(Tempstr);
	DestroyString(Boundary);

   return(FS->S);
}



int IDriveCloseFileWrite(TFileStore *FS, STREAM *S)
{
	char *Tempstr=NULL, *Error=NULL, *ptr;
	ListNode *Vars=NULL;
	HTTPInfoStruct *Info;
	int result=FALSE, val;

	Info=(HTTPInfoStruct *) FS->Extra;
	if (Info)
	{
	Tempstr=MCopyStr(Tempstr,"\r\n--",STREAMGetValue(S,"Boundary"),"--\r\n",NULL);
	STREAMWriteLine(Tempstr,S);
	STREAMFlush(S);

	HTTPTransact(Info);
			

	
/*
<?xml version="1.0" encoding="UTF-8"?>
<tree message="SUCCESS">
    <item filename="autoget.c" filesize="27620"
        lmd="1969/12/31 16:00:00" message="SUCCESS"/>
</tree>
*/

		val=HTTPReadDocument(S, &Tempstr);
		if (Settings.Flags & FLAG_VERBOSE) printf("\n%s\n",Tempstr);

		Vars=ListCreate();
		IDriveParseResponse(Tempstr, Vars);
		Tempstr=CopyStr(Tempstr,GetVar(Vars,"message"));
		if (strcmp(Tempstr,"SUCCESS")==0) 
		{
				val=atoi(STREAMGetValue(S,"Transfer-Size"));

				if (val==atoi(GetVar(Vars,"filesize"))) result=TRUE;	
				else result=ERR_INTERRUPTED;
		}
		else 
		{
			SetVar(FS->Vars,"Error",GetVar(Vars,"desc"));
			result=ERR_CUSTOM;
		}
	}

DestroyString(Tempstr);
return(result);
}


int IDriveCloseFile(TFileStore *FS, STREAM *S)
{ 
	char *Tempstr=NULL, *Error=NULL, *ptr;
	ListNode *Vars=NULL;
	int result=FALSE, val;
            
		if (S)
		{

			//if there's a boundary we must be doing a WriteFile
			ptr=STREAMGetValue(S,"Boundary");
			if (StrLen(ptr)) result=IDriveCloseFileWrite(FS, S);
			else result=TRUE;

			STREAMClose(S);
			FS->S=NULL;
		}

	DestroyString(Tempstr);
	DestroyString(Error);

	return(result);
}






int IDriveMkDir(TFileStore *FS, char *Dir)
{
char *Tempstr=NULL;
int result=TRUE, val;

Tempstr=MCopyStr(Tempstr,"https://",FS->Host,"/evs/createFolder?uid=",FS->Logon,"&pwd=",FS->Passwd,"&p=",FS->CurrDir,"&foldername=",Dir,NULL);
FS->S=HTTPMethod("POST",Tempstr,"","");
val=HTTPReadDocument(FS->S, &Tempstr);
if (Settings.Flags & FLAG_VERBOSE) printf("\n%s\n",Tempstr);

STREAMClose(FS->S);
FS->S=NULL;

DestroyString(Tempstr);

return(result);
}





int IDriveRenameFile(TFileStore *FS, char *FromArg, char *ToArg)
{
char *Tempstr=NULL, *Error=NULL, *FromPath=NULL, *ToPath=NULL;
HTTPInfoStruct *Info;
ListNode *Headers;
int result, val;
STREAM *S;


FromPath=MakeFullPath(FromPath,FS, FromArg);
ToPath=MakeFullPath(ToPath,FS, ToArg);

Tempstr=MCopyStr(Tempstr,"https://",FS->Host,"/evs/renameFileFolder?uid=",FS->Logon,"&pwd=",FS->Passwd,"&oldpath=",FromPath,"&newpath=",ToPath,NULL);
S=HTTPMethod("POST",Tempstr,"","");

val=HTTPReadDocument(S, &Tempstr);
if (Settings.Flags & FLAG_VERBOSE) printf("\n%s\n",Tempstr);

Error=CopyStr(Error,"");
result=IDriveParseStatusResponse(Tempstr, &Error);

DestroyString(Tempstr);
DestroyString(Error);
DestroyString(FromPath);
DestroyString(ToPath);


return(result);
}


int IDriveDeleteFile(TFileStore *FS, TFileInfo *FI)
{
char *Tempstr=NULL, *Error=NULL, *Path=NULL;
HTTPInfoStruct *Info;
ListNode *Headers;
int result, val;
STREAM *S;

Path=MakeFullPath(Path,FS, FI->Name);
Tempstr=MCopyStr(Tempstr,"https://",FS->Host,"/evs/deleteFile?uid=",FS->Logon,"&pwd=",FS->Passwd,"&p=",Path,NULL);
S=HTTPMethod("POST",Tempstr,"","");

val=HTTPReadDocument(S, &Tempstr);
if (Settings.Flags & FLAG_VERBOSE) printf("\n%s\n",Tempstr);
result=IDriveParseStatusResponse(Tempstr, &Error);

DestroyString(Tempstr);
DestroyString(Error);
DestroyString(Path);

return(result);
}





int IDriveOpen(TFileStore *FS)
{
char *Tempstr=NULL, *TagName=NULL, *TagData=NULL, *ptr;
ListNode *Vars;
STREAM *S;
int result=FALSE;

if (! FS) 
{
return(FALSE);
}

//Always require login for IDrive
if (! StrLen(FS->Passwd)) RequestAuthDetails(&FS->Logon,&FS->Passwd);

time(&StartTime);
HTTPSetFlags(HTTP_NODECODE);

//Get Address of the server that stores my data

FS->CurrDir=CopyStr(FS->CurrDir,"/");
if (StrLen(FS->InitDir)) FS->ChDir(FS,FS->InitDir);

Tempstr=MCopyStr(Tempstr,"https://evs.idrive.com/evs/getServerAddress?uid=",FS->Logon,"&pwd=",FS->Passwd,NULL);
S=HTTPMethod("POST",Tempstr,"","");

if (S)
{
Tempstr=STREAMReadDocument(Tempstr, S, TRUE);

Vars=ListCreate();
IDriveParseResponse(Tempstr, Vars);
FS->Host=CopyStr(FS->Host,GetVar(Vars,"webApiServer"));

PrintConnectionDetails(FS,S);
STREAMClose(S);
result=TRUE;
}


ListDestroy(Vars,DestroyString);
DestroyString(Tempstr);
DestroyString(TagData);
DestroyString(TagName);

return(result);
}


int IDriveClose(TFileStore *FS)
{
int result;
char *Tempstr=NULL;

		
   DestroyString(Tempstr);
return(TRUE);
}



TFileStore *IDriveFileStoreCreate(char *Name, char *ConnectSetup)
{
TFileStore *FS;

FS=(TFileStore *) calloc(1,sizeof(TFileStore));
FS->Flags=FS_SSL | FS_WRITEABLE | FS_CHDIR_FULLPATH;
FS->Features=FS_FILE_SIZE;
FS->Vars=ListCreate();
FS->CurrDir=CopyStr(FS->CurrDir,"");
FS->InitArg=CopyStr(FS->InitArg,ConnectSetup);
FS->Name=CopyStr(FS->Name,Name);

SetVar(FS->Vars,"LSFormat:Details","%s %m %v %n ");
SetVar(FS->Vars,"LSFormat:Long","%S %m %v %n ");


FS->Create=IDriveFileStoreCreate;
FS->Open=IDriveOpen;
FS->Close=IDriveClose;
FS->ChDir=DefaultChDir;
FS->MkDir=IDriveMkDir;
FS->LoadDir=IDriveLoadDir;
FS->OpenFile=IDriveOpenFile;
FS->CloseFile=IDriveCloseFile;
FS->ReadBytes=DefaultReadBytes;
FS->WriteBytes=DefaultWriteBytes;
FS->RenameFile=IDriveRenameFile;
FS->DeleteFile=IDriveDeleteFile;
FS->GetFileSize=FileStoreGetFileSize;

return(FS);
}
