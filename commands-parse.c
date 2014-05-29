//This file contains functions related to parsing the interactive file transfer/manipulation commands

#include "commands-parse.h"

char *Commands[]={
"cd","chdir","pwd","create","mkdir","del","rm","rmdir","mv","rename","chext","ls","dir", "md5","sha1","hash","chmod","info","show",
"lcd","lchdir","lpwd","lcreate","lmkdir","ldel","lrm","lrmdir","lmv","lrename","lchext","lls","ldir", "md5","lsha1","lhash","lchmod","linfo","lshow",
"get","put","cmp","hashcmp","set","time","help","quit","exit",
NULL};

char *HookTypes[]={"PreCopy","PostCopy",NULL};

void ParseCommandHook(ListNode *Vars, char *Text)
{
char *ptr, *Name=NULL;

ptr=strchr(Text,':');
if (ptr)
{
	Name=CopyStrLen(Name,Text,ptr-Text);
	if (MatchTokenFromList(Name,HookTypes,0)==-1) printf("ERROR: Unknown Hook Function Name: '%s'\n",Name);
	else
	{
		Name=CatStr(Name,"Hook");
		ptr++;
		SetVar(Vars,Name,ptr);
	}
}
else SetVar(Vars,"PostCopyHook",Text);

DestroyString(Name);
}


char *DefaultCommandParseArg(char *Args,int *CmdFlags, ListNode *Vars)
{
char *Tempstr=NULL, *Token=NULL, *ptr, *vptr;
int val;

ptr=GetToken(Args," ",&Token,GETTOKEN_QUOTES);
if (ptr)
{
switch (*Args)
{
	case '-': *CmdFlags |=FLAG_CMD_NOMORESWITCHES; break;
	case 'd': *CmdFlags |=FLAG_CMD_DEBUG; break;
	case 'r': *CmdFlags |=FLAG_CMD_RECURSE; break;
	case 'q': *CmdFlags |=FLAG_CMD_QUIET; break;
	case 'v': *CmdFlags |=FLAG_CMD_VERBOSE; break;

	case 'i':
		ptr=GetToken(ptr,"\\S",&Token,GETTOKEN_QUOTES);
		vptr=GetVar(Vars,"IncludePattern");
		if (! vptr) Tempstr=CopyStr(Tempstr,Token);
		else Tempstr=MCatStr(Tempstr,GetVar(Vars,"IncludePattern"),",",Token,NULL);
		SetVar(Vars,"IncludePattern",Tempstr);
	break;

	case 'x':
		ptr=GetToken(ptr,"\\S",&Token,GETTOKEN_QUOTES);
		vptr=GetVar(Vars,"ExcludePattern");
		if (! vptr) Tempstr=CopyStr(Tempstr,Token);
		else Tempstr=MCatStr(Tempstr,GetVar(Vars,"ExcludePattern"),",",Token,NULL);
		SetVar(Vars,"ExcludePattern",Tempstr);
	break;

	case 'h':
		ptr=GetToken(ptr,"\\S",&Token,GETTOKEN_QUOTES);
		ParseCommandHook(Vars,Token);
	break;
}
}

DestroyString(Tempstr);
DestroyString(Token);

return(ptr);
}



char *ParseMTimeArg(char *Data, int *CmdFlags, ListNode *Vars)
{
char *Tempstr=NULL;
char *eptr, *ptr;
int  val;

eptr=GetToken(Data,"\\S",&Tempstr,0);
ptr=Tempstr;

if (*ptr=='+')
{
	*CmdFlags|=FLAG_CMD_OLDERTHAN;
	ptr++;
}
else if (*ptr=='-') 
{
	*CmdFlags|=FLAG_CMD_YOUNGERTHAN;
	ptr++;
}
else *CmdFlags|=FLAG_CMD_MTIME;

val=strtol(ptr,&ptr,10);
if (*ptr=='m') val *= 60;
if (*ptr=='h') val *= 3600;
if (*ptr=='d') val *= 3600 * 24;

Tempstr=FormatStr(Tempstr,"%d",time(NULL) - val);
SetVar(Vars,"Since",Tempstr);

DestroyString(Tempstr);

return(eptr);
}



char *TransferCommandParseArg(char *Args,int *CmdFlags, ListNode *Vars)
{
char *Tempstr=NULL, *Token=NULL, *ptr, *vptr;
int val;


//the leading - has already been stepped past before this function
//is called, so arg will start with the first letter
ptr=GetToken(Args," ",&Token,GETTOKEN_QUOTES);

switch (*Token)
{
	case 's': *CmdFlags |=FLAG_CMD_SYNC; break;

	case 't':
	*CmdFlags |=FLAG_CMD_THROTTLE;
	ptr=GetToken(ptr,"\\S",&Token,0);
	val=atoi(Token) * 1024;
	Tempstr=FormatStr(Tempstr,"%d",val);
	SetVar(Vars,"Throttle",Tempstr);
	break;

	case 'p':
	if (strcmp(Token,"pub")==0) *CmdFlags |=FLAG_CMD_PUBLIC;
	else ptr=DefaultCommandParseArg(Args,CmdFlags,Vars);
	break;
	
	case 'c':
	if (strcmp(Token,"ct")==0)
	{
		 ptr=GetToken(ptr,"\\S",&Token,0);
		 SetVar(Vars,"ContentType",Token);
	}
	else ptr=DefaultCommandParseArg(Args,CmdFlags,Vars);
	break;

	case 'd':
	if (strcmp(Token,"dchmod")==0) 
	{
		*CmdFlags |=FLAG_CMD_CHMOD;
		ptr=GetToken(ptr,"\\S",&Token,0);
		SetVar(Vars,"DChMod",Token);
	}
	else ptr=DefaultCommandParseArg(Args,CmdFlags,Vars);
	break;


	case 'e':
	if (strcmp(Token,"et")==0)
	{
		 *CmdFlags |=FLAG_CMD_EXTN_TEMP;
		 ptr=GetToken(ptr,"\\S",&Token,0);
		 SetVar(Vars,"TransferTempExtn",Token);
	}
	else if (strcmp(Token,"ed")==0)
	{
		 *CmdFlags |=FLAG_CMD_EXTN_DEST;
		 ptr=GetToken(ptr,"\\S",&Token,0);
		 SetVar(Vars,"TransferDestExtn",Token);
	}
	else if (strcmp(Token,"es")==0)
	{
		 *CmdFlags |=FLAG_CMD_EXTN_SOURCE;
		 ptr=GetToken(ptr,"\\S",&Token,0);
		 SetVar(Vars,"TransferSourceExtn",Token);
	}
	else ptr=DefaultCommandParseArg(Args,CmdFlags,Vars);
	break;

	case 'f':
		if (StrLen(Token)==1) *CmdFlags |= FLAG_CMD_FULLPATH;
		else if (strcmp(Token,"fchmod")==0) 
		{
			*CmdFlags |=FLAG_CMD_CHMOD;
			ptr=GetToken(ptr,"\\S",&Token,0);
			SetVar(Vars,"FChMod",Token);
		}
		else ptr=DefaultCommandParseArg(Args,CmdFlags,Vars);
	break;


	case 'm':
		if (strcmp(Token,"mtime")==0) ptr=ParseMTimeArg(ptr, CmdFlags, Vars);
		else ptr=DefaultCommandParseArg(Args,CmdFlags,Vars);
	break;

	case 'v':
	if (strcmp(Token,"-ver")==0)
	{
		ptr=GetToken(ptr,"\\S",&Token,0);
		SetVar(Vars,"Version",Token);
	}
	else ptr=DefaultCommandParseArg(Args,CmdFlags,Vars);
	break;

	case 'L':
		*CmdFlags |= FLAG_CMD_LISTFROMFILE | FLAG_CMD_FULLPATH;
	break;

	case 'R':
		*CmdFlags |= FLAG_CMD_RESUME;
	break;

	default:
	ptr=DefaultCommandParseArg(Args,CmdFlags,Vars);
	break;
}

DestroyString(Tempstr);
DestroyString(Token);

return(ptr);
}



char *LSCommandParseArg(char *Args,int *CmdFlags, ListNode *Vars)
{
char *Tempstr=NULL, *Token=NULL, *ptr;

ptr=GetToken(Args," ",&Token,GETTOKEN_QUOTES);
if (ptr)
{
	if (strcmp(Token,"d")==0) *CmdFlags |=FLAG_CMD_DEBUG;
	else if (strcmp(Token,"s")==0) *CmdFlags |=FLAG_LS_SIZE;
	else if (strcmp(Token,"u")==0) *CmdFlags |=FLAG_LS_SHARELINK;
	else if (strcmp(Token,"l")==0) *CmdFlags |=FLAG_LS_DETAILS;
	else if (strcmp(Token,"ll")==0) *CmdFlags |=FLAG_LS_LONGDETAILS;
	else if (strcmp(Token,"lm")==0) *CmdFlags |=FLAG_LS_MEDIADETAILS;
	else if (strcmp(Token,"fr")==0) *CmdFlags |=FLAG_LS_REFRESH;
	else if (strcmp(Token,"n")==0)
	{
	 ptr=GetToken(ptr,"\\S",&Token,0);
	 Tempstr=FormatStr(Tempstr,"%d",atoi(Token));
	 SetVar(Vars,"LinesToList",Tempstr);
	}
}

DestroyString(Tempstr);
DestroyString(Token);

return(ptr);
}



char *CommandParseArg(char *Args,int Command, int *CmdFlags, ListNode *Vars)
{
switch (Command)
{
case CMD_GET:
case CMD_PUT:
case CMD_CREATE:
return(TransferCommandParseArg(Args,CmdFlags, Vars));
break;

case CMD_LS:
case CMD_LLS:
case CMD_DIR:
case CMD_LDIR:
return(LSCommandParseArg(Args,CmdFlags, Vars));
break;

default:
return(DefaultCommandParseArg(Args,CmdFlags, Vars));
break;
}


return(NULL);
}



char *ParseCommand(char *Line, int *Type, int *CmdFlags, ListNode *Vars)
{
char *Token=NULL, *ptr;

StripTrailingWhitespace(Line);
StripLeadingWhitespace(Line);
if (! StrLen(Line)) return(NULL);

ptr=GetToken(Line,"\\S",&Token,0);
*Type=MatchTokenFromList(Token,Commands,0);

while (ptr && (*ptr=='-')) 
{
	ptr++; //step past '-'

	ptr=CommandParseArg(ptr,*Type,CmdFlags,Vars);
	if (*CmdFlags & FLAG_CMD_NOMORESWITCHES) break;
}
StripDirectorySlash(ptr);

DestroyString(Token);

return(ptr);
}


void CommandPrintArgumentsHelp(int CmdID)
{

	switch (CmdID)
	{
		case CMD_CD:
		case CMD_CHDIR:
		case CMD_LCD:
		case CMD_LCHDIR:
		break;

		case CMD_PWD:
		break;

		case CMD_GET:
		case CMD_PUT:
			printf("	-r            Recursive Operation\n");
			printf("	-s            Sync (only transfer files that differ)\n");
			printf("	-x <pattern>  Exclude files matching <pattern> from transfer\n");
			printf("	-i <pattern>  Include files matching <pattern> (used to modify -x)\n");
			printf("	-ct <type>    Set content-type of file (differing uses in different filestores)\n");
			printf("	-et <extn>    Transfer file with temporary extension <extn>, then rename it to it's 'real' name\n");
			printf("	-es <extn>    Transfer file, then change the SOURCE file extension to <extn>\n");
			printf("	-ed <extn>    Transfer file, then change the DESTINATION file extension to <extn>\n");
			printf("	-f            Recreate full path of file at the DESTINATION\n");
			printf("	-q            Quiet mode, be less verbose, output less information while processing\n");
			printf("	-mtime <val>  Transfer files with modify time matching <val>\n              val will be +/-n(d|h|m) where d = days, h = hours, m = minutes and anything else is seconds\n             e.g. +20 more than 20 seconds old, -2d less than 2 days old\n");
			printf("	-h <script>   'Hook' script into transfer. Run this script when transfer complete\n");
			printf("	-L <list>     Transfer files listed in local file <list>\n");
			printf("	-v            Be more verbose, give more information while processing\n");
			printf("	-ver <ver>    Specify file version for Transfer (for filestores that support this)\n");
			printf("	--            Nothing after this is a switch, even if it starts with '-', it's a file\n");
		break;

		case CMD_CREATE:
		case CMD_LCREATE:
			printf("	-ct <content type>		Set content-type of file (differing uses in different filestores)\n");
		break;

		case CMD_MKDIR:
		case CMD_LMKDIR:
		break;

		case CMD_DEL:
		case CMD_RM:
		case CMD_LDEL:
		case CMD_LRM:
			printf("	-r		Recursive Delete\n");
		break;

		case CMD_RMDIR:
		case CMD_LRMDIR:
		break;


	case CMD_MOVE:
	case CMD_RENAME:
	case CMD_LMOVE:
	case CMD_LRENAME:
	break;
	
	case CMD_CHEXT:
	case CMD_LCHEXT:
			printf("	-r		Recursive Operation\n");
			printf("	-x <pattern>		Exclude files matching <pattern>\n");
			printf("	-i <pattern>		Include files matching <pattern> (used to modify -x)\n");
			printf("	-q		Quiet mode, be less verbose, output less information while processing\n");
			printf("	-h <script>		'Hook' script into transfer. Run this script when transfer complete\n");
			printf("	-L <list file>		Transfer files listed in local file <list file>\n");
			printf("	-v		Be more verbose, give more information while processing\n");
			printf("	-ver <version>		Specify file version for Transfer (for filestores that support this)\n");
			printf("	--		Nothing after this is a switch, even if it starts with '-', it's a file\n");

	break;
	
	case CMD_DIR:
	case CMD_LS:
	case CMD_LDIR:
	case CMD_LLS:
			printf("	-s		Show File Sizes\n");
			printf("	-l		Show File Details\n");
			printf("	-ll		Show More File Details\n");
			printf("	-lm		Show File Media Details\n");
			printf("	-n		Number of lines to list\n");
			printf("	-fr		Force refresh. Don't use cached listing.\n");
	break;

	case CMD_SHOW:
	case CMD_LSHOW:
	break;
	
	
	case CMD_MD5:
	case CMD_LMD5:
	case CMD_SHA1:
	case CMD_LSHA1:
			printf("	-r		Recursive Operation\n");
	break;
	
	case CMD_HASH:
	case CMD_LHASH:
			printf("	-r		Recursive Operation\n");
	break;
	
	case CMD_SET:
		printf("Available Settings:\n");
		printf("	compression  on/off	turn on/off compressed transfers\n");
		printf("	encoding     on/off	turn on/off transfer encoding\n");
		printf("	integrity    on/off/<hashtype>	turn on/off integrity checks\n");
		printf("               'on' will use an automatically selected hashtype, otherwise <hashtype> can be 'crc32' 'md5' 'sha1' or 'size'\n");
		printf("               Obviously the filestore must support one of the hashtypes (most support 'size', which is just the filesize).\n");
		printf("	comparetype  on/off/<type>	type of comparison to use when comparing files \n");
		printf("	dirmode      rwxrwxrwx	Permissions to set on transfered directories\n");
		printf("	filemode     rwxrwxrwx	Permissions to set on transfered files\n");
		printf("	exemode      rwxrwxrwx	Permissions to set on transfered executables\n");
	break;
	
	case CMD_INFO:
	case CMD_LINFO:
			printf("	-r		Recursive Operation\n");
	break;

	case CMD_CHMOD:
	case CMD_LCHMOD:
			printf("	-r		Recursive Operation\n");
	break;

	case CMD_CMP:
			printf("	-r		Recursive Operation\n");
			printf("	-q		Quiet mode. Only output fails\n");
	break;
	
	case CMD_HASHCMP:
			printf("	-r		Recursive Operation\n");
			printf("	-q		Quiet mode. Only output fails\n");
	break;

	case CMD_HELP:
	break;
	
	case CMD_TIME:
	break;

	case CMD_QUIT:
	case CMD_EXIT:
	break;
	
	}

}



void CommandPrintDetailedHelp(int CmdFlags, int CmdID)
{
	printf("%- 8s:  ",Commands[CmdID]);
	switch (CmdID)
	{
		case CMD_CD:
		case CMD_CHDIR:
			printf("Change REMOTE Directory\n");
		break;

		case CMD_LCD:
		case CMD_LCHDIR:
			printf("Change LOCAL Directory\n");
		break;

		case CMD_PWD:
			printf("Print REMOTE current Working Directory\n");
		break;

		case CMD_LPWD:
			printf("Print LOCAL current Working Directory\n");
		break;

		case CMD_GET:
			printf("Transfer files from remote to local\n");
		break;

		case CMD_PUT:
			printf("Transfer files from local to remote\n");
		break;

		case CMD_CREATE:
			printf("Create a REMOTE Empty File\n");
		break;

		case CMD_LCREATE:
			printf("Create a LOCAL Empty File\n");
		break;

		case CMD_MKDIR:
			printf("MaKe a REMOTE Directory\n");
		break;

		case CMD_LMKDIR:
			printf("MaKe a LOCAL Directory\n");
		break;

		case CMD_DEL:
		case CMD_RM:
			printf("Delete/ReMove a REMOTE File\n");
		break;

		case CMD_LDEL:
		case CMD_LRM:
			printf("Delete/ReMove a LOCAL File\n");
		break;

		case CMD_RMDIR:
			printf("Delete/ReMove a REMOTE Directory\n");
		break;

		case CMD_LRMDIR:
			printf("Delete/ReMove a LOCAL Directory\n");
		break;


	case CMD_MOVE:
	case CMD_RENAME:
			printf("Move/Rename a REMOTE item \n");
	break;
	
	case CMD_LMOVE:
	case CMD_LRENAME:
			printf("Move/Rename a LOCAL item\n");
	break;
	
	case CMD_CHEXT:
			printf("CHange EXTension of a REMOTE file\n");
	break;

	case CMD_LCHEXT:
			printf("CHange EXTension of a LOCAL file\n");
	break;
	
	case CMD_DIR:
	case CMD_LS:
			printf("List REMOTE Files and Directories\n");
	break;
	
	
	case CMD_LDIR:
	case CMD_LLS:
			printf("List LOCAL Files and Directories\n");
	break;

	case CMD_SHOW:
			printf("Display a REMOTE file\n");
	break;

	case CMD_LSHOW:
			printf("Display a LOCAL file\n");
	break;
	
	case CMD_CMP:
			printf("compare files between local and remote\n");
	break;
	
	case CMD_HASHCMP:
			printf("compare files between local and remote\n");
	break;
	
	case CMD_MD5:
			printf("Get an MD5 checksum of a REMOTE file\n");
	break;
	
	case CMD_LMD5:
			printf("Get an MD5 checksum of a LOCAL file\n");
	break;
	
	case CMD_SHA1:
			printf("Get an SHA1 checksum of a REMOTE file\n");
	break;
	
	case CMD_LSHA1:
			printf("Get an SHA1 checksum of a LOCAL file\n");
	break;
	
	case CMD_HASH:
			printf("Get a hash/checksum of a REMOTE file using any supported algorithm\n");
	break;
	
	case CMD_LHASH:
			printf("Get a hash/checksum of a LOCAL file using any supported algorithm\n");
	break;
	
	case CMD_SET:
			printf("Set values/variables\n");
	break;
	
	case CMD_INFO:
			printf("Get detailed info on a REMOTE file\n");
	break;

	case CMD_LINFO:
			printf("Get detailed info on a LOCAL file\n");
	break;

	case CMD_CHMOD:
			printf("CHange MODe (permissions etc) of REMOTE files\n");
	break;

	case CMD_LCHMOD:
			printf("CHange MODe (permissions etc) of LOCAL files\n");
	break;
	
	case CMD_HELP:
			printf("online help\n");
	break;
	
	case CMD_TIME:
			printf("Display local time\n");
	break;

	case CMD_QUIT:
	case CMD_EXIT:
			printf("EXIT program\n");
	break;
	
	}

if (CmdFlags & FLAG_CMD_VERBOSE) CommandPrintArgumentsHelp(CmdID);
}


void ProcessHelpCmd(int CmdFlags, char *Arg)
{
int i;

if (! StrLen(Arg))
{
	printf("Available Commands:\n\n");
	for (i=0; Commands[i] !=NULL; i++)
	{
		if (CmdFlags & FLAG_CMD_RECURSE) CommandPrintDetailedHelp(0,i);		
		else if (CmdFlags & FLAG_CMD_VERBOSE) CommandPrintDetailedHelp(FLAG_CMD_VERBOSE,i);		
		else 
		{
			printf("%- 8s",Commands[i]);
			if (((i+1) % 10)==0) printf("\n");
		}
	}
	printf("\n\nCommands starting with 'l' (except 'ls') are generally local versions of a command. So 'mkdir' will make a directory on the remote server, 'lmkdir' will make a local directory\n\n");
	printf("You can get more help about any command by typing 'help <command>'.\n\n");
}
else
{
	i=MatchTokenFromList(Arg,Commands,0);
	CommandPrintDetailedHelp(FLAG_CMD_VERBOSE,i);
}


}
