#include "ConfigFile.h"

ListNode *FileStores=NULL;

TFileStore *ConfigFileReadFileStore(STREAM *S, char *Name)
{
char *Tempstr=NULL, *Token=NULL, *ptr;
TFileStore *FS=NULL;
char *Type=NULL, *Host=NULL, *Login=NULL, *Password=NULL, *RefreshToken=NULL;
int Flags=0;

Tempstr=STREAMReadLine(Tempstr,S);
while (Tempstr)
{
StripTrailingWhitespace(Tempstr);
StripTrailingWhitespace(Tempstr);

if (strcmp(Tempstr,"}")==0) break;

ptr=GetToken(Tempstr,":",&Token,0);
while (ptr && isspace(*ptr)) ptr++;

if (strcmp(Token,"Type")==0) Type=CopyStr(Type,ptr);
if (strcmp(Token,"Host")==0) Host=CopyStr(Host,ptr);
if (strcmp(Token,"Login")==0) Login=CopyStr(Login,ptr);
if (strcmp(Token,"Password")==0) Password=CopyStr(Password,ptr);
if (strcmp(Token,"RefreshToken")==0) RefreshToken=CopyStr(RefreshToken,ptr);
if (strcmp(Token,"OAuth")==0) Flags |= FS_OAUTH;

Tempstr=STREAMReadLine(Tempstr,S);
}

if (StrLen(Type) && StrLen(Host))
{
	if (StrLen(Login)) Tempstr=MCopyStr(Tempstr,Type,":",	Login,"@",Host,NULL);
	else Tempstr=MCopyStr(Tempstr,Type,":",	Host,NULL);

	if (StrLen(Password)) Tempstr=MCatStr(Tempstr," -password ",Password,NULL);

	FS=FileStoreCreate(Name,Tempstr);
	if (StrLen(RefreshToken)) SetVar(FS->Vars,"OAuthRefreshToken",RefreshToken);
	FS->Settings |= Flags;

}

DestroyString(Host);
DestroyString(Type);
DestroyString(Login);
DestroyString(Tempstr);
DestroyString(Password);
DestroyString(RefreshToken);

return(FS);
}


ListNode *ConfigFileLoadFileStores(char *Path)
{
STREAM *S;
TFileStore *FS=NULL;
char *Tempstr=NULL, *Token=NULL, *ptr;

if (! FileStores) FileStores=ListCreate();
S=STREAMOpenFile(Path,O_RDONLY);
if (S)
{
	Tempstr=STREAMReadLine(Tempstr,S);
	while (Tempstr)
	{
		StripTrailingWhitespace(Tempstr);
		ptr=GetToken(Tempstr," ",&Token,0);

		if (strcmp(Token,"FileStore")==0)
		{
			FS=ConfigFileReadFileStore(S, ptr);
			ListAddNamedItem(FileStores,FS->Name,FS);
		}

		Tempstr=STREAMReadLine(Tempstr,S);
	}

STREAMClose(S);
}

DestroyString(Tempstr);
DestroyString(Token);

return(FileStores);
}



void ConfigFileSaveFileStores()
{
STREAM *S;
char *Tempstr=NULL, *PortStr=NULL;
ListNode *Curr;
TFileStore *FS;


Tempstr=MCopyStr(Tempstr,GetCurrUserHomeDir(),"/.fileferry.conf",NULL);
S=STREAMOpenFile(Tempstr,O_CREAT | O_TRUNC | O_WRONLY);
if (S)
{
	Curr=ListGetNext(FileStores);
	while (Curr)
	{	
		FS=(TFileStore *) Curr->Item;	
		PortStr=FormatStr(PortStr,"%d",FS->Port);
		if (! StrLen(FS->Name)) Tempstr=MCopyStr(Tempstr,"FileStore ",FS->Type,":",FS->Logon,"@",FS->Host,"\n{\n",NULL);
		else Tempstr=MCopyStr(Tempstr,"FileStore ",FS->Name,"\n{\n",NULL);
		Tempstr=MCatStr(Tempstr, "Type: ",FS->Type,"\n","Host: ",FS->Host,"\n","Port: ",PortStr,"\n","Login: ",FS->Logon,"\n","Password: ",FS->Passwd,"\n",NULL);
		if (FS->Settings & FS_OAUTH) 
		{
			Tempstr=MCatStr(Tempstr, "OAuth\nRefreshToken: ",GetVar(FS->Vars,"OAuthRefreshToken"),"\n",NULL);
		}
		Tempstr=MCatStr(Tempstr, "}\n\n",NULL);
		STREAMWriteLine(Tempstr,S);
		Curr=ListGetNext(Curr);
	}
	STREAMClose(S);
}
else printf("ERROR: Failed to open config file\n");

DestroyString(Tempstr);
DestroyString(PortStr);
}

void ConfigFileSaveFileStore(TFileStore *FS)
{
ListNode *Curr;
TFileStore *Old;

Curr=ListFindNamedItem(FileStores,FS->Name);
if (Curr) 
{
	Old=(TFileStore *) Curr->Item;
	DestroyFileStore(Old);
	Curr->Item=FileStoreClone(FS);	
}
else ListAddNamedItem(FileStores,FS->Name,FileStoreClone(FS));


ConfigFileSaveFileStores();
}

