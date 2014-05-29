#define _GNU_SOURCE
#include <fnmatch.h>

#include "http.h"


int RestBackupLoadDir(TFileStore *FS, char *InPattern, ListNode *Items, int Flags)
{
char *Tempstr=NULL, *Token=NULL;
TFileInfo *FI;
char *ptr;
int len, result=FALSE;
int IsRSS=FALSE;
HTTPInfoStruct *HTTPInfo;
STREAM *S;

if (! FS->CurrDir) FS->CurrDir=CopyStr(FS->CurrDir,"/");

HTTPInfo=HTTPInfoCreate(FS->Host,FS->Port,FS->Logon,FS->Passwd,"GET",FS->CurrDir,"",0);
if (FS->Flags & FS_SSL) HTTPInfo->Flags |= HTTP_SSL;
SetVar(HTTPInfo->CustomSendHeaders,"Accept","text/plain");
S=HTTPTransact(HTTPInfo);
if (S)
{
	Tempstr=STREAMReadLine(Tempstr,S);
	while (Tempstr)
	{
		StripTrailingWhitespace(Tempstr);
		FI=(TFileInfo *) calloc(1,sizeof(TFileInfo));
		FI->Type=FTYPE_FILE;
		ptr=GetToken(Tempstr,"	",&Token,0);
		FI->Mtime=DateStrToSecs("%Y-%m-%dT%H:%M:%SZ",Token,NULL);
		ptr=GetToken(ptr,"	",&Token,0);
		FI->Size=atoi(Token);
		ptr=GetToken(ptr,"	",&FI->Name,0);
		FI->Path=MCopyStr(FI->Path,FS->CurrDir,FI->Name,NULL);
	
		if (! ListFindNamedItem(Items,FI->Name)) ListAddNamedItem(Items,FI->Name,FI);
		else FileInfoDestroy(FI);
		Tempstr=STREAMReadLine(Tempstr,S);
	}
	STREAMClose(S);
	result=TRUE;
}



DestroyString(Tempstr);
DestroyString(Token);

return(result);
}





TFileStore *RestBackupFileStoreCreate(char *Name, char *ConnectSetup)
{
TFileStore *FS;

FS=HTTPFileStoreCreate(Name,ConnectSetup);
FS->Flags |= FS_NO_FOLDERS | FS_SSL;
FS->Features=FS_FILE_SIZE;
FS->Port=443;

FS->Create=RestBackupFileStoreCreate;
FS->LoadDir=RestBackupLoadDir;

return(FS);
}

