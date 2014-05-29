#include "filestore.h"
#include "wait.h"
#include "common.h"
#include "commands-parse.h"
#include "commands-file-transfer.h"
#include "commands-file-info.h"
#include "Settings.h"
#include "ConfigFile.h"
#include "locale.h"

#define VERSION "0.0.1"
#define DEFAULT_SSL_PATH "/usr/share/ssl:/usr/lib/ssl:/etc/ssl:/etc"

extern ListNode *FileStores;

void HandleSignal(int sig)
{
if (Settings.Flags & FLAG_INTERRUPT) exit(1);
Settings.Flags |= FLAG_INTERRUPT;
}


int ProcessCommandAgainstPatternList(TFileStore *FromFS, TFileStore *ToFS, int Command, char *PatternList, char *Arg, int CmdFlags, ListNode *Vars, void *Functor)
{
char *Pattern=NULL, *OldDir=NULL, *ptr, *target_dir;
int result;

ptr=GetToken(PatternList," ",&Pattern,GETTOKEN_QUOTES);
while (ptr)
{
	OldDir=CopyStr(OldDir,FromFS->CurrDir);
	switch (Command)
	{
	case CMD_GET:
	case CMD_PUT:
	case CMD_SHOW:
	case CMD_LSHOW:
	case CMD_CMP:
	case CMD_HASHCMP:
		result=((TRANSFER_COMMAND_FUNCTOR) Functor)(FromFS,ToFS,Command,Pattern,CmdFlags,Vars); 
	break;

	default:

		result=((INFO_COMMAND_FUNCTOR) Functor)(FromFS,Pattern,Arg,CmdFlags,Vars);
	break;
	}

	//Go Back to old dir when done
	FileStoreChDir(FromFS,OldDir);

	ptr=GetToken(ptr," ",&Pattern,GETTOKEN_QUOTES);
}

DestroyString(OldDir);
DestroyString(Pattern);
return(result);
}


//This reads file paths out of a file in a manner similar to the '-T' command in tar
int ProcessTransferListFile(TFileStore *FromFS, TFileStore *ToFS, int Command, char *ListPath, int CmdFlags, ListNode *Vars, void *Functor)
{
char *Tempstr=NULL, *Pattern=NULL, *OldFromDir=NULL, *OldToDir=NULL, *ptr, *target_dir;
int result;
STREAM *ListFile;

signal(SIGINT, HandleSignal);
ListFile=STREAMOpenFile(ListPath,O_RDONLY);
if (! ListFile) return(FALSE);

Pattern=STREAMReadLine(Pattern,ListFile);
while (Pattern)
{
	StripTrailingWhitespace(Pattern);

	OldFromDir=CopyStr(OldFromDir,FromFS->CurrDir);
	OldToDir=CopyStr(OldToDir,ToFS->CurrDir);

	if (ToFS) result=((TRANSFER_COMMAND_FUNCTOR) Functor)(FromFS,ToFS,Command,Pattern,CmdFlags,Vars);
	else result=((INFO_COMMAND_FUNCTOR) Functor)(FromFS,Command,Pattern,CmdFlags,Vars);

	//Go Back to old dir when done
	FileStoreChDir(FromFS,OldFromDir);
	FileStoreChDir(ToFS,OldToDir);

Pattern=STREAMReadLine(Pattern,ListFile);
}

STREAMClose(ListFile);
signal(SIGINT, NULL);

DestroyString(OldFromDir);
DestroyString(OldToDir);
DestroyString(Pattern);
DestroyString(Tempstr);

return(result);
}



int ProcessCommand(TFileStore *FS, TFileStore *DiskFS, char *Command)
{
char *Token=NULL, *Tempstr=NULL, *FileName1=NULL, *FileName2=NULL, *ptr;
int val;
int result=TRUE;
int LinesToList=0, Throttle=0;
TFileStore *p_FS;
int CmdFlags=0;
int total_bytes=0;
ListNode *Vars;

if (! StrLen(Command)) return(0);
Vars=ListCreate();

SetVar(Vars,"CompareLevel",GetVar(Settings.Vars,"CompareLevel"));
ptr=ParseCommand(Command, &val, &CmdFlags, Vars);
if (Settings.Flags & FLAG_VERBOSE) CmdFlags |= FLAG_CMD_VERBOSE;

switch (val)
{
	case CMD_LCD:
	case CMD_LCHDIR:
		if (DiskFS->ChDir(DiskFS,ptr))
		{
		FileStoreLoadDir(DiskFS,"",NULL, LIST_REFRESH|LIST_INCLUDE_DIRS);
		HandleEventMessage(Settings.Flags,"OKAY: Local Directory is %s",DiskFS->CurrDir);
		}
		else HandleEventMessage(Settings.Flags,"FAIL: Can't CD to Local Directory %s",DiskFS->CurrDir);
		break;

	case CMD_PWD:
		HandleEventMessage(Settings.Flags,"OKAY: Remote Directory is %s",FS->CurrDir);
		break;

	case CMD_LPWD:
		HandleEventMessage(Settings.Flags,"OKAY: Local Directory is %s",DiskFS->CurrDir);
		break;


	case CMD_CD:
	case CMD_CHDIR:
		if (FS->ChDir)
		{
			if (FileStoreChDir(FS,ptr)) HandleEventMessage(Settings.Flags,"OKAY: Remote Directory is %s",ptr);
			else HandleEventMessage(Settings.Flags,"FAIL: Can't CD to Remote Directory %s",ptr);
		}
		else HandleEventMessage(Settings.Flags,"FAIL: Can't CD to Remote Directory %s. Filestore does not support directories",ptr);
	break;

	case CMD_MKDIR:
		if(FS->MkDir) 
		{
			if (FileStoreMkDir(FS,ptr,0)) HandleEventMessage(Settings.Flags,"OKAY: Remote Directory %s created",ptr);
			else HandleEventMessage(Settings.Flags,"FAIL: Can't make remote directory %s.",ptr);
		}
		else HandleEventMessage(Settings.Flags,"FAIL: Can't make remote directory %s. Filestore does not support making directories",ptr);
	break;

	case CMD_LMKDIR:
		if(DiskFS->MkDir) 
		{
			DiskFS->MkDir(DiskFS,ptr);
			HandleEventMessage(Settings.Flags,"OKAY: Local Directory %s created",ptr);
		}
		else HandleEventMessage(Settings.Flags,"FAIL: Can't make local directory %s. Filestore does not support making directories",ptr);
	break;


	case CMD_RM:
	case CMD_DEL:
	case CMD_LRM:
	case CMD_LDEL:
	if ((val==CMD_LDEL) || (val==CMD_LRM))  p_FS=DiskFS;
	else p_FS=FS;


  if (! FileStoreDelete(p_FS,ptr,CmdFlags)) 
	{
		if (! (CmdFlags & FLAG_CMD_QUIET)) 
		{
				HandleEventMessage(Settings.Flags,"FAIL: Delete failed for %s",ptr);
		}
	}
	else HandleEventMessage(Settings.Flags,"OKAY: %s Deleted",ptr);
	break;

	case CMD_INFO:
	printf("\n");
	result=ProcessCommandAgainstPatternList(FS, NULL, val, ptr, Token, CmdFlags, Vars, ProcessGetFileInfo);
	break;

	case CMD_LINFO:
	printf("\n");
	result=ProcessCommandAgainstPatternList(DiskFS, NULL, val, ptr, Token, CmdFlags, Vars, ProcessGetFileInfo);
	break;


	case CMD_CHMOD:
	ptr=GetToken(ptr,"\\S",&Token,0);
	if (FS->ChMod) result=ProcessCommandAgainstPatternList(FS, NULL, val, ptr, Token, CmdFlags, Vars, ProcessChMod);
	break;

	case CMD_LCHMOD:
	ptr=GetToken(ptr,"\\S",&Token,0);
	if (DiskFS->ChMod) result=ProcessCommandAgainstPatternList(DiskFS, NULL, val, ptr, Token, CmdFlags, Vars, ProcessChMod);
	break;

	/*
	case CMD_SU:
	ptr=GetToken(ptr,"\\S",&Token,0);
	if (FS->SwitchUser)
	{
		result=FS->SwitchUser(FS, Token, ptr);
		if (! result) printf("ERROR: Switch User Failed\n");
		else printf("OKAY: User now %s\n",Token);
	}
	else printf("ERROR: Filestore does not support switching Users\n");
	break;
	*/


	case CMD_CREATE:
	ProcessCreateFile(FS, ptr, Vars);
	//Get a new listing, to be sure all facts about the file are retrived
	FileStoreLoadDir(FS,"*",NULL, LIST_REFRESH);
	HandleEventMessage(Settings.Flags,"DONE: %s Created",ptr);
	break;

	case CMD_LCREATE:
	ProcessCreateFile(DiskFS, ptr, Vars);
	//Get a new listing, to be sure all facts about the file are retrived
	FileStoreLoadDir(DiskFS,"*",NULL, LIST_REFRESH);
	HandleEventMessage(Settings.Flags,"DONE: %s Created",ptr);
	break;


	case CMD_GET:
	SetVar(Vars,"Transferred","0");
	SetVar(Vars,"Total","0");
	SetVar(Vars,"Failed","0");
	SetVar(Vars,"TotalBytes","0");

	if (CmdFlags & FLAG_CMD_LISTFROMFILE) ProcessTransferListFile(FS, DiskFS, val, ptr, CmdFlags, Vars, ProcessTransferFiles);
	else result=ProcessCommandAgainstPatternList(FS, DiskFS, val, ptr, "", CmdFlags, Vars, ProcessTransferFiles);

	Token=CopyStr(Token,GetVar(Vars,"TotalBytes"));
	HandleEventMessage(Settings.Flags,"DONE: Transferred %s files of %s. %s Failures.  ",GetVar(Vars,"Transferred"),GetVar(Vars,"Total"),GetVar(Vars,"Failed"), GetHumanReadableDataQty(atof(Token),0));
	break;

	case CMD_PUT:
	SetVar(Vars,"Transferred","0");
	SetVar(Vars,"Total","0");
	SetVar(Vars,"Failed","0");
	SetVar(Vars,"TotalBytes","0");

	if (CmdFlags & FLAG_CMD_LISTFROMFILE) ProcessTransferListFile(DiskFS, FS, val, ptr, CmdFlags, Vars, ProcessTransferFiles);
	else result=ProcessCommandAgainstPatternList(DiskFS, FS, val, ptr, "", CmdFlags, Vars, ProcessTransferFiles);

	//Get a new listing, to be sure all facts about the file are retrived
	FileStoreLoadDir(FS,"*",NULL, LIST_REFRESH);
	HandleEventMessage(Settings.Flags,"DONE: Transferred %s files of %s. %s Failures. Bytes Transferred",GetVar(Vars,"Transferred"),GetVar(Vars,"Total"),GetVar(Vars,"Failed"), GetVar(Vars,"TotalBytes"));
	break;

	case CMD_SHOW:
	printf("\n");
	result=ProcessCommandAgainstPatternList(FS, NULL, val, ptr, "", CmdFlags, Vars, ProcessTransferFiles);
	break;

	case CMD_LSHOW:
	printf("\n");
	result=ProcessCommandAgainstPatternList(DiskFS, NULL, val, ptr, "", CmdFlags, Vars, ProcessTransferFiles);
	break;

	case CMD_HASH:
		ptr=GetToken(ptr," ",&Token,0);
		if (FS->GetDigest) ProcessCommandAgainstPatternList(FS, NULL, val, ptr, Token, CmdFlags, Vars, ProcessGetDigest);
	break;

	case CMD_LHASH:
		ptr=GetToken(ptr," ",&Token,0);
		if (DiskFS->GetDigest) ProcessCommandAgainstPatternList(DiskFS, NULL, val, ptr, Token, CmdFlags, Vars, ProcessGetDigest);
	break;

	case CMD_MD5:
		if (FS->GetDigest) ProcessCommandAgainstPatternList(FS, NULL, val, ptr, "md5", CmdFlags, Vars, ProcessGetDigest);
	break;

	case CMD_LMD5:
		if (DiskFS->GetDigest) ProcessCommandAgainstPatternList(DiskFS, NULL, val, ptr, "md5", CmdFlags, Vars, ProcessGetDigest);
	break;

	case CMD_SHA1:
		if (FS->GetDigest) ProcessCommandAgainstPatternList(FS, NULL, val, ptr, "sha1", CmdFlags, Vars, ProcessGetDigest);
	break;

	case CMD_LSHA1:
		if (DiskFS->GetDigest) ProcessCommandAgainstPatternList(DiskFS, NULL, val, ptr, "sha1", CmdFlags, Vars, ProcessGetDigest);
	break;

	case CMD_MOVE:
	case CMD_LMOVE:
	case CMD_RENAME:
	case CMD_LRENAME:
	if ((val==CMD_LMOVE) || (val==CMD_LRENAME)) p_FS=DiskFS;
	else p_FS=FS;

	Tempstr=CopyStr(Tempstr,ptr);
	ptr=strrchr(Tempstr,' ');
	if (ptr)
	{
		*ptr='\0';
		ptr++;
	
		result=ProcessCommandAgainstPatternList(p_FS, NULL, val, Tempstr, ptr, 0, Vars, ProcessRename);
		if (! (CmdFlags & FLAG_CMD_QUIET))
		{ 
			if (result==TRANSFER_OKAY) HandleEventMessage(Settings.Flags,"OKAY: All files renamed");
			else if (result==ERR_NOFILES) HandleEventMessage(Settings.Flags,"FAIL: No matching files to rename");
			else HandleEventMessage(Settings.Flags,"FAIL: Some files failed to rename");
		}
	}
	break;

	case CMD_CHEXT:
	case CMD_LCHEXT:
	if (val==CMD_LCHEXT) p_FS=DiskFS;
	else p_FS=FS;
		ptr=GetToken(ptr," ",&Tempstr,GETTOKEN_QUOTES);
		result=ProcessCommandAgainstPatternList(p_FS, NULL, val, ptr, Tempstr, 1, Vars, ProcessRename);
		if (! (CmdFlags & FLAG_CMD_QUIET))
		{ 
			if (result==TRANSFER_OKAY) HandleEventMessage(Settings.Flags,"OKAY: All files renamed");
			else if (result==ERR_NOFILES) HandleEventMessage(Settings.Flags,"FAIL: No matching files to rename");
			else HandleEventMessage(Settings.Flags,"FAIL: Some files failed to rename");
		}
	break;

	case CMD_LLS:
	case CMD_LDIR:
	case CMD_LS:
	case CMD_DIR:
		if ((val==CMD_LLS) || (val==CMD_LDIR)) p_FS=DiskFS;
		else p_FS=FS;
		ProcessLSCmd(p_FS, DiskFS, CmdFlags, ptr, LinesToList);
	break;


	case CMD_CMP:
	SetVar(Vars,"CMP:Checked","0");
	SetVar(Vars,"CMP:Differs","0");
	Tempstr=FormatStr(Tempstr,"%d",FileStoreSelectCompareLevel(DiskFS, FS, GetVar(Vars,"CompareLevel")));
	SetVar(Vars,"CMP:CompareLevel",Tempstr);

	if (CmdFlags & FLAG_CMD_LISTFROMFILE) ProcessTransferListFile(DiskFS, FS, val, ptr, CmdFlags, Vars, ProcessCompare);
	else result=ProcessCommandAgainstPatternList(DiskFS, FS, val, ptr, "", CmdFlags, Vars, ProcessCompare);

	HandleEventMessage(Settings.Flags,"DONE: %s files checked %s files differ\n",GetVar(Vars,"CMP:Checked"),GetVar(Vars,"CMP:Differs"));
	break;

	case CMD_HASHCMP:
		result=ProcessCommandAgainstPatternList(DiskFS, FS, val, ptr, "", CmdFlags, Vars, ProcessHashCompare);
	break;


	case CMD_SET:
		ProcessSetting(FS,ptr);
	break;

	case CMD_TIME:
		printf("TIME: %s\n",GetDateStr(ptr,NULL));
	break;

	case CMD_HELP:
		if (CmdFlags & FLAG_CMD_VERBOSE) ProcessHelpCmd(FLAG_CMD_RECURSE,ptr);
		else ProcessHelpCmd(0,ptr);
	break;

	case CMD_EXIT:
	case CMD_QUIT:
	FS->Close(FS);
	exit(0);
	break;

	default:
		printf("ERROR: Unknown Command\n");
	break;
}

fflush(NULL);

DestroyString(Token);
DestroyString(Tempstr);
DestroyString(FileName1);
DestroyString(FileName2);

return(result);
}


int ProcessScript(TFileStore *FS, TFileStore *DiskFS, char *CmdStr)
{
char *Command=NULL, *cptr;
int result=TRUE;

cptr=GetToken(CmdStr,";",&Command,0);
while (cptr)
{
ProcessCommand(FS,DiskFS,Command);
cptr=GetToken(cptr,";",&Command,0);
}

DestroyString(Command);
return(result);
}


char *GetNextCommand(char *InStr, char *Prompt, STREAM *S)
{
char *RetStr=NULL;

if (Settings.Flags & FLAG_CONSOLE)
{
	RetStr=MCopyStr(InStr,Prompt,"> ",NULL);
	printf("%s",RetStr);
	fflush(NULL);
}

return(STREAMReadLine(RetStr,S));
}


int ProcessCommands(TFileStore *FS, TFileStore *DiskFS)
{
STREAM *S;
char *Command=NULL;
int result=TRUE;

S=STREAMFromFD(0);
STREAMSetTimeout(S,0);

if (S)
{
	Command=GetNextCommand(Command,FS->Host,S);
	while (Command)
	{
		ProcessCommand(FS,DiskFS,Command);
		Command=GetNextCommand(Command,FS->Host,S);
	}
}
STREAMClose(S);

DestroyString(Command);
return(result);
}


#define ACT_ERROR 0
#define ACT_CONNECT 1
#define ACT_ADDSERVICE 2
#define ACT_HELP 3
#define ACT_VERSION 4
#define ACT_COMMANDS 5

int ParseArgs(int argc, char *argv[], char **SaveName, char **ConnectStr, char **CmdStr)
{
int i, j, len, Action=ACT_CONNECT;
char *Password=NULL;

for (i=1; i < argc; i++)
{
	if (strcmp(argv[i],"-c")==0) *CmdStr=CopyStr(*CmdStr,argv[++i]);
	else if (strcmp(argv[i],"-l")==0) LogFile=STREAMOpenFile(argv[++i],O_WRONLY | O_CREAT | O_APPEND);
	else if (strcmp(argv[i],"-save")==0)
	{
		*SaveName=CopyStr(*SaveName,argv[++i]);	
		if (StrLen(*SaveName) > 0) Action = ACT_ADDSERVICE;
		else 
		{
			*ConnectStr=CopyStr(*ConnectStr,"ERROR: Service Name argument missing. Use 'fileferry <protocol>:<user>@<host> -p password -save <service name>");
			Action=ACT_ERROR;
			break;
		}
	}
	else if (strcmp(argv[i],"-h")==0) Action=ACT_HELP;
	else if (strcmp(argv[i],"-?")==0) Action=ACT_HELP;
	else if (strcmp(argv[i],"-help")==0) Action=ACT_HELP;
	else if (strcmp(argv[i],"--help")==0) Action=ACT_HELP;
	else if (strcmp(argv[i],"-commands")==0) Action=ACT_COMMANDS;
	else if (strcmp(argv[i],"--commands")==0) Action=ACT_COMMANDS;
	else if (strcmp(argv[i],"-q")==0) Settings.Flags |= FLAG_QUIET;
	else if (strcmp(argv[i],"-b")==0) Settings.Flags |= FLAG_QUIET | FLAG_BACKGROUND;
 	else if (strcmp(argv[i],"-i")==0) Password=MCopyStr(Password,"keyfile:",argv[++i],NULL);
 	else if (strcmp(argv[i],"-p")==0) 
 	{
 		Password=CopyStr(Password,argv[++i]);
 
 		len=StrLen(argv[i]);
 		for (j=0; j < len; j++)
 		{
 			if (j < 4) argv[i][j]='*';
 			else argv[i][j]='\0';
 		}
 	}
 	else 
 	{
 		if (StrLen(*ConnectStr)) *ConnectStr=MCatStr(*ConnectStr,",",argv[i],NULL);
 		else *ConnectStr=CopyStr(*ConnectStr,argv[i]);
 	}
}

if (StrLen(Password)) *ConnectStr=MCatStr(*ConnectStr ," -password ",Password,NULL);


DestroyString(Password);

return(Action);
}


int ConnectToService(TFileStore *FS)
{
char *ptr;

if (! FS->Open(FS)) 
{
	return(FALSE);
}

SetDefaultHashType(GetVar(FS->Vars,"HashTypes"));

if (! FileStoreLoadDir(FS,"",NULL, LIST_INCLUDE_DIRS)) 
{
	return(FALSE);
}

return(TRUE);
}


TFileStore *LoadOrCreateRemoteFS(char *ConnectStr)
{
ListNode *Curr;
char *Tempstr=NULL;
TFileStore *FS=NULL, *tmpFS=NULL;

FS=FileStoreCreate("",ConnectStr);

if (! FS)
{
	Curr=ListFindNamedItem(FileStores,ConnectStr);
	if (Curr) FS=FileStoreClone(Curr->Item);
}
else if (! StrLen(FS->Passwd))
{
	Curr=ListGetNext(FileStores);
	while (Curr)
	{
		tmpFS=(TFileStore *) Curr->Item;
		if (
					(strcmp(FS->Type,tmpFS->Type)==0) &&
					(strcmp(FS->Host,tmpFS->Host)==0) &&
					(strcmp(FS->Logon,tmpFS->Logon)==0)
			) 
			{
				FS->Settings |= tmpFS->Settings;
				FS->Passwd=CopyStr(FS->Passwd,tmpFS->Passwd);
				SetVar(FS->Vars,"OAuthRefreshToken",GetVar(tmpFS->Vars,"OAuthRefreshToken"));
			}
		Curr=ListGetNext(Curr);
	}
}

DestroyString(Tempstr);

return(FS);
}



int ConnectAndProcess(char *ConnectStr, char *CmdStr)
{
char *Tempstr=NULL, *ptr;
TFileStore *FS=NULL, *DiskFS=NULL;
int i;
int result=FALSE, RetVal=-1;
time_t Now, diff;

if (isatty(1)) Settings.Flags |= FLAG_CONSOLE;

LoadFileMagics("/etc/mime.types",NULL);

Tempstr=FindFileInPath(Tempstr,"cacert.pem",DEFAULT_SSL_PATH);
if (StrLen(Tempstr)) LibUsefulSetValue("SSL_VERIFY_CERTFILE",Tempstr);

DiskFS=DiskFileStoreCreate("",".");
DiskFS->Open(DiskFS);


FS=LoadOrCreateRemoteFS(ConnectStr);


if (FS)
{
	if (StrLen(FS->Logon)) HandleEventMessage(Settings.Flags,"CONNECT: %s:%s@%s:%d",FS->Type,FS->Logon,FS->Host,FS->Port);
	else HandleEventMessage(Settings.Flags,"CONNECT: %s:%s:%d",FS->Type,FS->Host,FS->Port);
	if (StrLen(CmdStr)) HandleEventMessage(Settings.Flags,"Processing commands: %s",CmdStr);

	if (ConnectToService(FS))
	{
		if (StrLen(FS->Name)) HandleEventMessage(Settings.Flags,"Connected to %s:%d",FS->Host,FS->Port);	
		else HandleEventMessage(Settings.Flags,"Connected to %s@%s:%d",FS->Logon,FS->Host,FS->Port);	

		RetVal=0;
		if (StrLen(CmdStr))
		{
			 if (Settings.Flags & FLAG_BACKGROUND) demonize();
			 result=ProcessScript(FS,DiskFS,CmdStr);
		}
		else result=ProcessCommands(FS,DiskFS);
	}
	else HandleEventMessage(Settings.Flags,"FAILED to connect to %s@%s:%d",FS->Logon,FS->Host,FS->Port);	
}
else HandleEventMessage(Settings.Flags,"Unknown File store type");;

diff=time(NULL) - StartupTime;
HandleEventMessage(Settings.Flags,"INFO: Processing Ends after %d mins %d secs",diff / 60, diff % 60);

DestroyString(Tempstr);

return(RetVal);
}


void InitSettings()
{
SetVar(Settings.Vars,"filemode","-rw-------");
SetVar(Settings.Vars,"dirmode","-rwx------");
SetVar(Settings.Vars,"exemode","-rwx------");
}

void PrintProgramHelp()
{

printf("usage:\n	fileferry <type>:<username>@<host> [-p <password>] [-c <commands>]\n\n");

printf("-p		Specify password for service (insecure, use -save to save password in config file instead)\n");
printf("-i		path to keyfile for ssh connections.\n");
printf("-c		Specify commands to run.\n");
printf("-l		Specify path to a logfile.\n");
printf("-q		Quiet mode.\n");
printf("-b		Fork into background after connect.\n");
printf("-h		Help.\n");
printf("-?		Help.\n");
printf("--help		Help.\n");
printf("-save		Add service to config file (don't connect to service)\n\n");
}

void PrintProgramVersion()
{
printf("fileferry (File Ferry) %s\n",VERSION);

}


main(int argc, char *argv[])
{
char *SaveName=NULL, *ConnectStr=NULL, *CmdStr=NULL, *Tempstr=NULL;
int result, RetVal=0;
TFileStore *FS;

time(&StartupTime);
Settings.Vars=ListCreate();
InitSettings();

result=ParseArgs(argc, argv, &SaveName, &ConnectStr, &CmdStr);
HTTPSetUserAgent("FileFerry 0.1");

Tempstr=MCopyStr(Tempstr,GetCurrUserHomeDir(),"/.fileferry.conf",NULL);
FileStores=ConfigFileLoadFileStores(Tempstr);

if (result == ACT_ERROR) printf("%s\n",ConnectStr);
else if (result == ACT_ADDSERVICE) 
{
	FS=FileStoreCreate(SaveName,ConnectStr);
	ConfigFileSaveFileStore(FS);
}
else if (result == ACT_HELP) PrintProgramHelp();
else if (result == ACT_VERSION) PrintProgramVersion();
else if (result == ACT_COMMANDS) ProcessHelpCmd(FLAG_CMD_VERBOSE,"");
else RetVal=ConnectAndProcess(ConnectStr, CmdStr);

exit(RetVal);
}
