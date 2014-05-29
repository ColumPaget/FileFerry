#define _GNU_SOURCE
#include <fnmatch.h>

#include "remote_shell.h"
#include "../commands-parse.h"


#define LIST_CMD_3 "dir "
#define LIST_CMD_2 "ls -a -l -d --time-style=full-iso "
#define LIST_CMD_1 "ls -a -l -d "
#define ECHO_CMD "echo "
#define CHMOD_CMD "chmod 2>&1"
#define CHDIR_CMD "cd  2>&1"
#define MKDIR_CMD "mkdir  2>&1"
#define PWD_CMD "pwd "
#define RENAME_CMD "mv -f -b 2>&1 "
#define LINK_CMD "ln  2>&1"
#define DELETE_CMD "rm -f 2>&1 "
#define SU_CMD "su 2>&1"

#define QUOTE_CHARS "\" \t()'`|&<>;$!:#"

#define FLAG_SYNC 1


char *ListCommand=NULL;

char *WritePostfix=NULL;
int WritePostfixLen=0;
int total=0;

void ShellWriteLine(char *Line, STREAM *S)
{
STREAMWriteBytes(S,Line,StrLen(Line));
if (WritePostfixLen > 0) STREAMWriteBytes(S,WritePostfix,WritePostfixLen);
STREAMFlush(S);
usleep(100000);
}


void ShellConsumeInput(TFileStore *FS)
{
char *Tempstr=NULL;

Tempstr=SetStrLen(Tempstr,BUFSIZ);

while (STREAMCheckForBytes(FS->S))
{
  if (STREAMReadBytes(FS->S, Tempstr,BUFSIZ) < 1) break;
}


DestroyString(Tempstr);
}



//finds helper program for a feature like compression or encoding
char *ShellGetHelper(TFileStore *FS, char *Type, char *Feature)
{
char *Tempstr=NULL, *ptr;

Tempstr=MCopyStr(Tempstr,Type,":",Feature,":Program",NULL);
ptr=GetVar(FS->Vars,Tempstr);

DestroyString(Tempstr);

return(ptr);
}


int CalcEncodedLength(int DataLen, char *Encoding)
{
int val, pad;

if (! StrLen(Encoding)) return(DataLen);
val = ((DataLen * 4) / 3);

return(val);
}


//This 'builds' a command line, and is mostly used for file transfers (get and put)
//It considers factors like the use of compression or the need for transfer encoding
//and builds the appropriate command line for them.

char *ShellBuildTransferCommand(char *RetStr, TFileStore *FS, int Cmd, int Flags, TFileInfo *FI)
{
char *p_Compressor=NULL, *p_Encoder=NULL, *Tempstr=NULL, *SafePath=NULL;
int val;

SafePath=QuoteCharsInStr(SafePath,FI->Path,QUOTE_CHARS);
p_Compressor=ShellGetHelper(FS,"CompressionTypes",GetVar(FS->Vars,"Compression"));
if (FS->Flags & FS_NEEDS_ENCODING) p_Encoder=ShellGetHelper(FS,"EncodingTypes",GetVar(FS->Vars,"Encoding"));

	if (Cmd == CMD_GET)
	{
		if ((Flags & OPEN_RESUME) && (FI->ResumePoint > 0)) 
		{
			RetStr=FormatStr(RetStr,"tail -c +%d %s ",FI->ResumePoint+1, SafePath);
		}


		if (StrLen(p_Compressor))
		{
			if (StrLen(RetStr)) RetStr=MCatStr(RetStr," | ", p_Compressor,NULL);
			else RetStr=MCopyStr(RetStr,p_Compressor," ",SafePath,NULL);
		}

		if (StrLen(p_Encoder))
		{
			if (StrLen(RetStr)) RetStr=MCatStr(RetStr," | ", p_Encoder,NULL);
			else RetStr=MCopyStr(RetStr,p_Encoder," ",SafePath,NULL);
		}

		if (! StrLen(RetStr)) RetStr=MCopyStr(RetStr,"cat ",SafePath,NULL);
	}
	else
	{
		val=CalcEncodedLength(FI->Size, GetVar(FS->Vars,"Encoding"));
		RetStr=FormatStr(RetStr,"head -c %d",val);
		if (StrLen(p_Compressor) && StrLen(p_Encoder)) RetStr=MCatStr(RetStr," | ", p_Encoder," -d | ", p_Compressor, " -d ", NULL);
		else if (StrLen(p_Compressor)) RetStr=MCatStr(RetStr," | ",p_Compressor," -d ",NULL);
		else if (StrLen(p_Encoder)) RetStr=MCatStr(RetStr," | ",p_Encoder," -d ",NULL);

		if ((Flags & OPEN_RESUME) && (FI->ResumePoint > 0)) RetStr=MCatStr(RetStr," >> ",SafePath,NULL);
		else RetStr=MCatStr(RetStr," > ",SafePath,NULL);
	}

Tempstr=FormatStr(Tempstr,"SENT-%d-%d-%d",getpid(),time(NULL),rand());
STREAMSetValue(FS->S,"TermStr",Tempstr);
RetStr=MCatStr(RetStr,"; echo ",Tempstr,NULL);

DestroyString(SafePath);
DestroyString(Tempstr);

return(RetStr);
}


int IsExpectedString(char *String, int ExpectedLen, char *ExpectedChars)
{
int len, i;

len=StrLen(String);
if ((ExpectedLen > 0) && (len != ExpectedLen)) return(FALSE);

for (i=0; i < ExpectedLen; i++) if (! strchr(ExpectedChars,String[i])) return(FALSE);

return(TRUE);
}



int ReadBytesToTerm(STREAM *S, char *Buffer, int BuffSize, char Term)
{
int inchar, pos=0;

inchar=STREAMReadChar(S);
while (inchar != EOF) 
{
	if (inchar > -1)
	{
		Buffer[pos]=inchar;
		pos++;
		if (inchar==Term) break;
		if (pos==BuffSize) break;
	}
	inchar=STREAMReadChar(S);
}

if ((pos==0) && (inchar==EOF)) return(EOF);
return(pos);
}



char *ShellSendTermStr(STREAM *S, char *Buff, char *CommandStr)
{
char *Tempstr=NULL, *TermStr=NULL, *Token=NULL;

 GetToken(CommandStr," ",&Token,0); 

 TermStr=FormatStr(Buff,"%s %x-%x-%x",Token,getpid(),time(NULL),rand());

 Tempstr=CopyStr(Tempstr,ECHO_CMD);
 Tempstr=CatStr(Tempstr,TermStr);
 ShellWriteLine(Tempstr,S);

DestroyString(Tempstr);
DestroyString(Token);

return(TermStr);
}



int ShellReadUntil(STREAM *S, char *Expected, char **Return, int Timeout)
{
char *Tempstr=NULL;
int result=FALSE, OldTimeout, StartTime;

 OldTimeout=S->Timeout;
	if (Timeout > 0) 
	{
		STREAMSetTimeout(S,Timeout);
		time(&StartTime);
	}

	
 Tempstr=STREAMReadLine(Tempstr,S);
 while (Tempstr)
 {
   StripTrailingWhitespace(Tempstr);
   StripLeadingWhitespace(Tempstr);

   if (strstr(Tempstr,Expected))
   {
       result=TRUE;
       break;
   }
   else if (Return !=NULL) *Return=MCatStr(*Return,Tempstr,"\n",NULL);
		if (Timeout > 0)
		{
			if ((time(NULL) - StartTime) >= Timeout) break;
			time(&StartTime);
		}
   Tempstr=STREAMReadLine(Tempstr,S);
 }

 STREAMSetTimeout(S,OldTimeout);
DestroyString(Tempstr);

return(result);
}


int ShellGetCommandResponse(STREAM *S, char *Command, char **Return, int Timeout)
{
char *EndLine=NULL;
int result=FALSE;

   EndLine=ShellSendTermStr(S,EndLine,Command);
   STREAMFlush(S);
   result=ShellReadUntil(S,EndLine,Return,Timeout);

DestroyString(EndLine);

return(result);
}

int ShellPerformFileCommand(TFileStore *FS, char *Command, char *Arg1, char *Arg2)
{
char *Tempstr=NULL, *Path1=NULL, *Path2=NULL;

ShellConsumeInput(FS);
Path1=QuoteCharsInStr(Path1,Arg1,QUOTE_CHARS);

if (Arg2)
{
 Path2=QuoteCharsInStr(Path2,Arg2,QUOTE_CHARS);
 Tempstr=FormatStr(Tempstr,"%s %s %s",Command, Path1, Path2);
}
else Tempstr=FormatStr(Tempstr,"%s %s",Command,Path1);

ShellWriteLine(Tempstr,FS->S);

DestroyString(Path1);
DestroyString(Path2);
DestroyString(Tempstr);

return(ShellGetCommandResponse(FS->S,Command,NULL,30));
}



void ShellRunTask(TFileStore *FS, char *Task)
{
char *EndLine=NULL;

/*
ShellWriteLine(Task,FS->S);

//EndLine=ShellSendTermStr(FS->S,EndLine,"CHDIR");
STREAMFlush(FS->S);
*/

DestroyString(EndLine);
}


int ShellSwitchUser(TFileStore *FS, char *User, char *Pass)
{
char *Tempstr=NULL;
int result=FALSE;
ListNode *Dialog=NULL;

ShellConsumeInput(FS);
Tempstr=FormatStr(Tempstr,"su %s",SU_CMD,User);
ShellWriteLine(Tempstr,FS->S);
STREAMFlush(FS->S);

Tempstr=CopyStr(Tempstr,Pass);

Dialog=ListCreate();
ExpectDialogAdd(Dialog, "assword:", Tempstr, DIALOG_END);
ExpectDialogAdd(Dialog, "", "", DIALOG_FAIL);
STREAMExpectDialog(FS->S, Dialog);
ListDestroy(Dialog,ExpectDialogDestroy);

ShellWriteLine("whoami",FS->S);
STREAMFlush(FS->S);

Tempstr=STREAMReadLine(Tempstr,FS->S);
StripTrailingWhitespace(Tempstr);
if (strcmp(Tempstr,Pass)==0) result=TRUE;

DestroyString(Tempstr);

return(result);
}


int ShellMkDir(TFileStore *FS, char *Dir)
{
return(ShellPerformFileCommand(FS, MKDIR_CMD, Dir, NULL));
}



int ShellRenameFile(TFileStore *FS, char *FromPath, char *ToPath)
{
return(ShellPerformFileCommand(FS, RENAME_CMD, FromPath, ToPath));
}


int ShellLinkFile(TFileStore *FS, char *FromPath, char *ToPath)
{
return(ShellPerformFileCommand(FS, LINK_CMD, FromPath, ToPath));
}


int ShellDeleteFile(TFileStore *FS, TFileInfo *FI)
{
return(ShellPerformFileCommand(FS, DELETE_CMD, FI->Path,NULL));
}


int ShellExtractDigest(char *String, char *Type, char **FName, char **Digest)
{
int result=FALSE;
char *ptr;

ptr=GetToken(String,"\\S",Digest,0);
if (! ptr) return(FALSE);

while (isspace(*ptr)) ptr++;

	if ((strcmp(Type,"md5")==0) && (IsExpectedString(*Digest, 32, "0123456789abcdefABCDEF"))) result=TRUE;
	else if ((strcmp(Type,"sha1")==0) && (IsExpectedString(*Digest, 40, "0123456789abcdefABCDEF"))) result=TRUE;
	else if ((strcmp(Type,"sha")==0) && (IsExpectedString(*Digest, 40, "0123456789abcdefABCDEF"))) result=TRUE;
	else 
	{
		*Digest=CopyStr(*Digest,String);
		ptr="";
	}

	if (FName) *FName=CopyStr(*FName,ptr);

return(result);
}


int ShellGetMultiDigest(TFileStore *FS, char *Path, char *Type, ListNode *Files)
{
char *Tempstr=NULL, *Cmd=NULL, *SafePath=NULL, *EndLine=NULL, *Digest=NULL, *ptr;
int result=FALSE;
ListNode *Node;
TFileInfo *FI;

SafePath=QuoteCharsInStr(SafePath,Path,QUOTE_CHARS);
ptr=ShellGetHelper(FS, "HashTypes", Type);
Tempstr=MCopyStr(Tempstr,ptr," ",SafePath," 2>&1",NULL);
ShellWriteLine(Tempstr,FS->S);
EndLine=ShellSendTermStr(FS->S,EndLine,"HASH");
STREAMFlush(FS->S);

Tempstr=STREAMReadLine(Tempstr,FS->S);
while (Tempstr)
{
		StripTrailingWhitespace(Tempstr);	
		//StripLeadingWhitespace(Tempstr);

		if (strcmp(Tempstr,EndLine)==0) break;

		if (ShellExtractDigest(Tempstr, Type, &Cmd, &Digest))
		{
			Node=ListFindNamedItem(Files,Cmd);
			if (Node) 
			{
				FI=(TFileInfo *) Node->Item;
				if (FI->Type != FTYPE_DIR) FI->Hash=CopyStr(FI->Hash,Digest);
			}
		}

  Tempstr=STREAMReadLine(Tempstr,FS->S);
}

/*
*/

DestroyString(EndLine);
DestroyString(Tempstr);
DestroyString(Digest);
DestroyString(Cmd);

return(result);
}




int ShellGetDigest(TFileStore *FS, char *Path, char *Type, char **Digest)
{
char *Tempstr=NULL, *Cmd=NULL, *SafePath=NULL, *ptr;
int result=FALSE;

SafePath=QuoteCharsInStr(SafePath,Path,QUOTE_CHARS);
ptr=ShellGetHelper(FS, "HashTypes", Type);
Tempstr=MCopyStr(Tempstr,ptr," ",SafePath," 2>&1",NULL);
ShellWriteLine(Tempstr,FS->S);
STREAMFlush(FS->S);


Tempstr=STREAMReadLine(Tempstr,FS->S);
StripTrailingWhitespace(Tempstr);

result=ShellExtractDigest(Tempstr, Type, &Cmd, Digest);

DestroyString(SafePath);
DestroyString(Tempstr);
DestroyString(Cmd);

return(result);
}

int ShellInternalChMod(TFileStore *FS, char *Mode, char *Path)
{
return(ShellPerformFileCommand(FS, CHMOD_CMD, Mode,Path));
}


int ShellChMod(TFileStore *FS, TFileInfo *FI, char *Mode)
{
return(ShellInternalChMod(FS, Mode, FI->Path));
}


char *ShellGetCurrDir(char *RetStr, TFileStore *FS)
{
char *Tempstr=NULL;

//ShellConsumeInput(FS);

//Bit naughty, we use 'RetStr' for now
Tempstr=CopyStr(RetStr,PWD_CMD);
ShellWriteLine(Tempstr,FS->S);
STREAMFlush(FS->S);

//Now tempstr/RetStr is being used for data we will return
Tempstr=STREAMReadLine(Tempstr,FS->S);
StripTrailingWhitespace(Tempstr);

if (StrLen(Tempstr)==0)
{
	Tempstr=STREAMReadLine(Tempstr,FS->S);
	StripTrailingWhitespace(Tempstr);
}

Tempstr=TerminateDirectoryPath(Tempstr);


//Don't destroy tempstr
return(Tempstr);
}




int ShellChDir(TFileStore *FS, char *Dir)
{
char *Tempstr=NULL, *SafePath=NULL, *ExpectedDir=NULL;
int result;

ShellConsumeInput(FS);

ExpectedDir=FormatDirPath(ExpectedDir,FS->CurrDir,Dir);

SafePath=QuoteCharsInStr(SafePath,Dir,QUOTE_CHARS);
Tempstr=FormatStr(Tempstr,"%s %s",CHDIR_CMD,SafePath);
ShellWriteLine(Tempstr,FS->S);

result=ShellGetCommandResponse(FS->S,"CHDIR",NULL,30);
Tempstr=ShellGetCurrDir(Tempstr,FS);

if (strcmp(Tempstr,ExpectedDir)==0)
{
	FS->CurrDir=CopyStr(FS->CurrDir,Tempstr);
	result=TRUE;
}
else
{
	result=FALSE;
	printf("ERROR: NOT IN EXPECTED DIR: [%s] [%s]\n",Tempstr,ExpectedDir);
}

DestroyString(Tempstr);
DestroyString(SafePath);

return(result);
}



int ShellLoadDir(TFileStore *FS, char *InPattern, ListNode *Items, int Flags)
{
int result;
char *Tempstr=NULL, *FullPath=NULL, *EndLine=NULL;
TFileInfo *FI;

   FullPath=QuoteCharsInStr(FullPath,InPattern,QUOTE_CHARS);
   Tempstr=FormatStr(Tempstr,"%s %s",ListCommand,FullPath);

   ShellWriteLine(Tempstr,FS->S);

   EndLine=ShellSendTermStr(FS->S,EndLine,"LIST");
   STREAMFlush(FS->S);

   Tempstr=STREAMReadLine(Tempstr,FS->S);
   while (Tempstr)
   {
		StripTrailingWhitespace(Tempstr);	
		StripLeadingWhitespace(Tempstr);

		if (strcmp(Tempstr,EndLine)==0) break;

		if (StrLen(Tempstr))
		{
			FI=Decode_LS_Output(FS->CurrDir,Tempstr);
			if (FI) ListAddNamedItem(Items,FI->Name,FI);
		}

   Tempstr=STREAMReadLine(Tempstr,FS->S);
   }

	if (Flags & CMP_MD5) ShellGetMultiDigest(FS, FullPath, "md5", Items);
	if (Flags & CMP_SHA1) ShellGetMultiDigest(FS, FullPath, "sha1", Items);

DestroyString(Tempstr);
DestroyString(FullPath);
DestroyString(EndLine);
return(TRUE);
}




STREAM *ShellOpenFile(TFileStore *FS, TFileInfo *FI, int Flags)
{
char *Tempstr=NULL;


//As remote-shell transfers use external programs it's a good idea
//to gather any exited processes here.
waitpid(-1,NULL,WNOHANG);


if (Flags & OPEN_WRITE)
{
	STREAMResizeBuffer(FS->S,8192);
	Tempstr=ShellBuildTransferCommand(Tempstr, FS, CMD_PUT, Flags, FI);
	FS->S->Flags |= SF_WRONLY;
}
else
{
	Tempstr=ShellBuildTransferCommand(Tempstr, FS, CMD_GET, Flags, FI);
	//S->Flags |= SF_RDONLY;
}

		ShellWriteLine(Tempstr,FS->S);
		FS->S->BytesRead=0;
		FS->S->BytesWritten=0;

	
	 STREAMSetItem(FS->S,"FileInfo",FI);

   DestroyString(Tempstr);

   return(FS->S);
}


int ShellCloseFile(TFileStore *FS, STREAM *S)
{
char *Tempstr=NULL, *TermStr=NULL;
TFileInfo *FI;
int result;

  //if the transfer was a write 
  if (S->Flags & SF_WRONLY)
  {
		if (FS->Settings & FS_COMPRESSION) 
		{
			//printf("Write! == %d\n",total);
			STREAMWriteLine("==\r\n",S);
		}
		STREAMFlush(S);

		FI=(TFileInfo *) STREAMGetItem(S,"FileInfo");

		TermStr=CopyStr(TermStr,STREAMGetValue(S,"TermStr"));
		if (StrLen(TermStr))
		{
		Tempstr=STREAMReadLine(Tempstr,S);
		while (Tempstr)
		{
			StripTrailingWhitespace(Tempstr);
			if (strcmp(Tempstr,TermStr)==0) break;
			Tempstr=STREAMReadLine(Tempstr,S);
		}
		}
	
		Tempstr=FormatStr(Tempstr,"%o",ConvertFModeToNum(FI->Permissions));
		ShellInternalChMod(FS,Tempstr,FI->Name);
	}

	S->Flags &= ~SF_WRONLY;

DestroyString(Tempstr);
DestroyString(TermStr);

return(TRUE);
}



int ShellReadBytes(TFileStore *FS, STREAM *S, char *Buffer, int BuffLen)
{
TProcessingModule *PMod=NULL;
char *Tempstr=NULL;
int result=0, len;
char *ptr;

ptr=STREAMGetValue(S,"TermStr");
if (StrLen(ptr)==0) 
{
return(EOF);
}

Tempstr=CopyStr(Tempstr,ptr);
result=ReadBytesToTerm(S, Buffer, BuffLen, '\n');
if (result > 0)
{
	Buffer[result]='\0';

	ptr=Buffer+(result-StrLen(Tempstr)-1);
	if (strncmp(ptr,Tempstr,StrLen(Tempstr))==0) 
	{
		result=ptr-Buffer;
		STREAMSetValue(S,"TermStr","");
	}
	else 
	{
		ptr--;
		if (strncmp(ptr,Tempstr,StrLen(Tempstr))==0) 
		{
			result=ptr-Buffer;
			STREAMSetValue(S,"TermStr","");
			*ptr=0;
		}
	
		//At the moment only base64 encoding is supported
		if (FS->Settings & FS_COMPRESSION)
		{
			Tempstr=CopyStr(Tempstr,Buffer);
			StripTrailingWhitespace(Tempstr);		
			result=from64tobits(Buffer,Tempstr);
		}
	}
}

if (result < 0) result=0;

DestroyString(Tempstr);

return(result);
}


char *Base64Encode(char *RetStr, char *Data, int DataLen)
{
char *Tempstr=NULL;
int pos;

			RetStr=CopyStr(RetStr,"");
			Tempstr=SetStrLen(Tempstr,DataLen * 2);
			to64frombits(Tempstr,Data,DataLen);
	
			for (pos=0; pos < StrLen(Tempstr); pos+=76)
			{
				RetStr=CatStrLen(RetStr,Tempstr+pos,76);
				RetStr=CatStr(RetStr,"\r\n");
			}

	DestroyString(Tempstr);
	return(RetStr);
}



int ShellWriteBytes(TFileStore *FS, STREAM *S, char *Data, int DataLen)
{
char *Tempstr=NULL;
int result, towrite;

	
		if ((FS->Settings & FS_COMPRESSION) && (DataLen > 0))
		{
			//integer divide by three and then multiply back up, will give us
			//a number that divides by three (so it will be less than DataLen
			//if DataLen doesn't divide by three)
			towrite=(DataLen / 3) * 3;
			if (towrite==0) towrite=DataLen;

			Tempstr=Base64Encode(Tempstr, Data, DataLen);
			result=STREAMWriteBytes(S, Tempstr, StrLen(Tempstr));
			total+=result;
			result=towrite;
		}
		else result=STREAMWriteBytes(S, Data, DataLen);

//printf("SWB: %d %d\n",result,DataLen);

DestroyString(Tempstr);
return(result);
}



int ShellCheckConnected(STREAM *RS)
{
char *Tempstr=NULL;
int result=FALSE;

result=ShellGetCommandResponse(RS, "CONNECT", &Tempstr,5);

//STREAMSetTimeout(RS,30);

DestroyString(Tempstr);

return(result);
}

void ShellCheckFeatures(TFileStore *FS)
{
int FeatureVals[]={FS_FILE_HASHES,FS_FILE_HASHES,FS_FILE_HASHES,FS_TRANSFER_TYPES,FS_TRANSFER_TYPES,FS_TRANSFER_TYPES,FS_TRANSFER_TYPES,0,0,FS_RESUME_TRANSFERS};
char *FeatureVars[]={"HashTypes","HashTypes","HashTypes","CompressionTypes","CompressionTypes","CompressionTypes","CompressionTypes","EncodingTypes","ListTypes","ResumeTransfers",NULL};
char *FeatureArgs[]={"md5","sha1","sha1","gzip","bzip2","xz","lzop","base64","full-iso","resume",NULL};
char *FeatureTests[]={"echo 'Hello World' | md5sum 2>&1","echo 'Hello World' | sha1sum 2>&1","echo 'Hello World' | shasum -a 1 2>&1","gzip --help 2>&1","bzip2 --help 2>&1","xz --help 2>&1","lzop --help 2>&1","echo 'Hello World' | base64 2>&1","ls --help 2>&1","echo 'Hello World' | tail -c 6 2>&1",NULL};
char *FeatureCmds[]={"md5sum","sha1sum","shasum -a 1","gzip","bzip2","xz","lzop","base64","ls",NULL};
char *FeatureResponses[]={"e59ff97941044f85df5297e1c302d260  -","648a6a6ffffdaa0badb23b8baf90b6168dd16b3a  -","648a6a6ffffdaa0badb23b8baf90b6168dd16b3a  -","*sage: gzip *","*sage: bzip2 *","*sage: xz *","*sage: lzop *","SGVsbG8gV29ybGQK","*--time-style=*full-iso*","World",NULL};
int i;
char *Tempstr=NULL, *Cmd=NULL, *ptr;
int result=FALSE;

for (i=0; FeatureTests[i] !=NULL; i++)
{
	ShellWriteLine(FeatureTests[i],FS->S);

	Tempstr=CopyStr(Tempstr,"");
	Cmd=MCopyStr(Cmd,"RESP: ",FeatureArgs[i],NULL);
	result=ShellGetCommandResponse(FS->S, Cmd, &Tempstr,5);
	StripTrailingWhitespace(Tempstr);

	if (result && (fnmatch(FeatureResponses[i],Tempstr,0)==0))
	{
		FS->Features |= FeatureVals[i];
		Tempstr=CopyStr(Tempstr,GetVar(FS->Vars,FeatureVars[i]));
		Tempstr=MCatStr(Tempstr,FeatureArgs[i]," ",NULL);
		SetVar(FS->Vars,FeatureVars[i],Tempstr);

		Tempstr=MCopyStr(Tempstr,FeatureVars[i],":",FeatureArgs[i],":Program",NULL);
		SetVar(FS->Vars,Tempstr,FeatureCmds[i]);
	}
}

ListCommand=CopyStr(ListCommand,LIST_CMD_1);
ptr=GetVar(FS->Vars,"ListTypes");
if (StrLen(ptr))
{
	if (strstr(ptr,"full-iso")) ListCommand=CopyStr(ListCommand,LIST_CMD_2);
}

ptr=GetVar(FS->Vars,"EncodingTypes");
if (StrLen(ptr))
{
	FS->Features |= FS_COMPRESSION;
	if (FS->Flags & FS_NEEDS_ENCODING)
	{
		GetToken(ptr," ",&Tempstr,0);
		ChangeFilestoreSetting(FS, "Encoding", Tempstr);
	}
}

ptr=GetVar(FS->Vars,"HashTypes");

DestroyString(Tempstr);
DestroyString(Cmd);
}

#define SHELL_LOGON_DONE 1

int ShellFinalizeConnection(TFileStore *FS, int Flags)
{
char *Tempstr=NULL;
ListNode *Dialog=NULL;
int result;

	STREAMSetFlushType(FS->S,FLUSH_ALWAYS,0);
	if (! StrLen(FS->Passwd)) RequestAuthDetails(&FS->Logon,&FS->Passwd);

  if (! (FS->Flags & FS_NOLOGON))
  {
  Dialog=ListCreate();
  ExpectDialogAdd(Dialog, "Are you sure you want to continue connecting (yes/no)?", "yes\n", DIALOG_OPTIONAL);
  ExpectDialogAdd(Dialog, "Permission denied", "", DIALOG_OPTIONAL | DIALOG_FAIL);
  Tempstr=MCopyStr(Tempstr,FS->Logon,"\n",NULL);
  ExpectDialogAdd(Dialog, "ogin:", Tempstr, DIALOG_OPTIONAL);
  Tempstr=MCopyStr(Tempstr,FS->Passwd,"\n",NULL);
  ExpectDialogAdd(Dialog, "assword:", Tempstr, DIALOG_END);
  STREAMExpectDialog(FS->S, Dialog);
  ListDestroy(Dialog,ExpectDialogDestroy);
  }


	//STREAMSetFlushType(FS->S,FLUSH_LINE,0);
	//allow time for stty to take effect!
	ShellWriteLine("PS1=",FS->S); 
	ShellWriteLine("PS2=",FS->S);
	ShellWriteLine("stty -echo",FS->S);
	result=ShellCheckConnected(FS->S);
	sleep(1);

	if (result)
	{
		//ShellPerformFileCommand(FS, "export", "LC=POSIX", NULL);
		ShellCheckFeatures(FS);
		FS->CurrDir=ShellGetCurrDir(FS->CurrDir,FS);
		PrintConnectionDetails(FS,FS->S);
	}

DestroyString(Tempstr);
return(result);
}



int SSHShellOpen(TFileStore *FS)
{
char *Tempstr=NULL;
int result=FALSE;

if (! FS) 
{
return(FALSE);
}
FS->S=STREAMCreate();

if (! StrLen(FS->Passwd)) RequestAuthDetails(&FS->Logon,&FS->Passwd);

if (FS->Port > 0) Tempstr=FormatStr(Tempstr,"/usr/bin/ssh -2 -T %s@%s -p %d",FS->Logon,FS->Host,FS->Port);
else Tempstr=MCopyStr(Tempstr,"/usr/bin/ssh -2 -T ",FS->Logon,"@",FS->Host,NULL);

if (strncmp(FS->Passwd,"keyfile:",8)==0)
{
  Tempstr=MCatStr(Tempstr," -i ",FS->Passwd+8,NULL);
  FS->Flags |= FS_NOLOGON;
}


//Never use TTYFLAG_CANON here
//FS->S=STREAMSpawnCommand(Tempstr,COMMS_BY_PTY|TTYFLAG_CRLF|TTYFLAG_IGNSIG);
FS->S=STREAMSpawnCommand(Tempstr,COMMS_BY_PTY|TTYFLAG_IGNSIG);
result=ShellFinalizeConnection(FS, SHELL_LOGON_DONE);

DestroyString(Tempstr);

return(result);
}



int ShellClose(TFileStore *FS)
{

int result;
char *Tempstr=NULL;

   Tempstr=CopyStr(Tempstr,"exit ");
   ShellWriteLine(Tempstr,FS->S);

   STREAMClose(FS->S);
   DestroyString(Tempstr);

return(TRUE);
}











TFileStore *ShellFileStoreCreate(char *Name, char *ConnectSetup)
{
TFileStore *FS;

FS=(TFileStore *) calloc(1,sizeof(TFileStore));
FS->Flags=FS_REQ_CLONE | FS_WRITEABLE | FS_CHDIR_FULLPATH;
FS->Features=FS_FILE_SIZE;
FS->Vars=ListCreate();
FS->InitArg=CopyStr(FS->InitArg,ConnectSetup);
FS->Name=CopyStr(FS->Name,Name);
FS->CurrDir=CopyStr(FS->CurrDir,"");

WritePostfix=CopyStr(WritePostfix,"\n");
WritePostfixLen=1;

SetVar(FS->Vars,"GetCmd","cat");

FS->Create=ShellFileStoreCreate;
//FS->Open=SerialShellOpen;
FS->Close=ShellClose;
FS->ChDir=ShellChDir;
FS->MkDir=ShellMkDir;
FS->LoadDir=ShellLoadDir;
FS->OpenFile=ShellOpenFile;
FS->CloseFile=ShellCloseFile;
FS->ReadBytes=ShellReadBytes;
FS->WriteBytes=ShellWriteBytes;
FS->RenameFile=ShellRenameFile;
FS->DeleteFile=ShellDeleteFile;
FS->LinkFile=ShellLinkFile;
FS->RunTask=ShellRunTask;
FS->GetFileSize=FileStoreGetFileSize;
FS->GetDigest=ShellGetDigest;
FS->ChMod=ShellChMod;
return(FS);
}

/*********************** Shell over serial functions *******************/

TFileStore *SSHShellFileStoreCreate(char *Name, char *ConnectSetup)
{
TFileStore *FS;

FS=ShellFileStoreCreate(Name,ConnectSetup);
FS->Create=SSHShellFileStoreCreate;
FS->Open=SSHShellOpen;
//FS->SwitchUser=ShellSwitchUser;

return(FS);
}


/*
	IsACommand 255
  WILL (option code)  251 
  WON'T (option code) 252  
  DO (option code)    253 
  DON'T (option code) 254  
*/
#define TELNET_IAC  0xFF
#define TELNET_DONT 0xFE
#define TELNET_DO   0xFD
#define TELNET_WONT 0xFC
#define TELNET_WILL 0xFB
#define TELNET_STARTSN	0xFA
#define TELNET_ENDSN		0xF0

#define TELNET_BINARY 0
#define TELNET_ECHO 1
#define TELNET_FULLDUPLEX 3
#define TELNET_TERMTYPE 24
#define TELNET_TERMSPD 32
#define TELNET_FLOWCTL 33
#define TELNET_LINEMODE 34
#define TELNET_ENVVAR 36

char *TelOptName(int val)
{
switch (val)
{
	case TELNET_BINARY: return("binary"); break;
	case TELNET_ECHO: return("echo"); break;
	case TELNET_FULLDUPLEX: return("full duplex"); break;
	case TELNET_TERMTYPE: return("termtype"); break;
	case TELNET_TERMSPD: return("termspd"); break;
	case TELNET_FLOWCTL: return("flowctl"); break;
	case TELNET_LINEMODE: return("linemode"); break;
	case TELNET_ENVVAR: return("envvar"); break;

	default: return("?"); break;
}

}

void SendTelnetCommand(TFileStore *FS, char WillWont, char Cmd)
{
char Tempstr[4];

	Tempstr[0]=TELNET_IAC;
	Tempstr[1]=WillWont;
	Tempstr[2]=Cmd;
	Tempstr[3]='\0';
	if (Cmd==TELNET_ENDSN) STREAMWriteBytes(FS->S,Tempstr,2); 
	else STREAMWriteBytes(FS->S,Tempstr,3); 
	if (Cmd != TELNET_STARTSN) STREAMFlush(FS->S);
}


void TelnetHandleOption(TFileStore *FS)
{
int inchar;
char *Tempstr=NULL;

inchar=STREAMReadChar(FS->S);
switch (inchar)
{
	case TELNET_STARTSN:
			inchar=STREAMReadChar(FS->S);
			printf("SSN %d %s\n",inchar & 0xFF, TelOptName(inchar));
			while (inchar != 240)
			{
				printf("%d %c ",inchar, inchar);
				Tempstr=AddCharToStr(Tempstr,inchar);	
			inchar=STREAMReadChar(FS->S);
			}
		printf("SSN: %s\n",Tempstr);
	break;

	case TELNET_WILL:
			inchar=STREAMReadChar(FS->S);
			printf("WILL %d %s\n",inchar & 0xFF, TelOptName(inchar));

			switch (inchar)
			{
			case TELNET_FULLDUPLEX:
				SendTelnetCommand(FS, TELNET_DO, inchar);
			break;
			
			case TELNET_BINARY:
			case TELNET_ECHO:
				printf("DONT ECHO\n");
			default:
			SendTelnetCommand(FS, TELNET_DONT, inchar);
			break;
			}
	break;

	case TELNET_WONT:
			inchar=STREAMReadChar(FS->S);
			printf("WONT %d %s\n",inchar & 0xFF, TelOptName(inchar));
	break;

	case TELNET_DO:
			inchar=STREAMReadChar(FS->S);
			printf("DO %d %s\n",inchar & 0xFF, TelOptName(inchar));
			switch (inchar)
			{
			case TELNET_FULLDUPLEX:
				SendTelnetCommand(FS, TELNET_WILL, inchar);
			break;

			case TELNET_ECHO:
			case TELNET_BINARY:
			case TELNET_TERMTYPE:
			case TELNET_TERMSPD:
			case TELNET_FLOWCTL:
			case TELNET_LINEMODE:
			case TELNET_ENVVAR:

			default:
				SendTelnetCommand(FS, TELNET_WONT, inchar);
			break;
			}
	break;

	case TELNET_DONT:
			inchar=STREAMReadChar(FS->S);
			printf("DONT %d %s\n",inchar * 0xFF, TelOptName(inchar));
			SendTelnetCommand(FS, TELNET_WONT, inchar);
	break;
}

DestroyString(Tempstr);
}



//strip NULLs (NOOP) from telnet stream
int TelnetProcessRead(TProcessingModule *Mod, const char *InBuff, int InLen, char **OutBuff, int *OutLen)
{
char *iptr, *optr;

if (*OutLen < InLen)
{
	*OutLen=InLen;
	*OutBuff=SetStrLen(*OutBuff,*OutLen);
}

optr=*OutBuff;
for (iptr=InBuff; iptr < (InBuff+InLen); iptr++)
{
	if (*iptr !='\0') 
	{
		*optr=*iptr;
		optr++;
	}
}


return(optr - *OutBuff);
}



void TelnetAddReadProcessor(STREAM *S)
{
TProcessingModule *Mod=NULL;

   Mod=(TProcessingModule *) calloc(1,sizeof(TProcessingModule));
   Mod->Name=CopyStr(Mod->Name,"Telnet:Read");
   Mod->Read=TelnetProcessRead;

   STREAMAddDataProcessor(S,Mod,"");
}



TFileStore *TelnetShellOpen(TFileStore *FS)
{
int result=FALSE;
char *Tempstr=NULL;
int i;

if (! FS) 
{
return(FALSE);
}


FS->S=STREAMCreate();
result=STREAMConnectToHost(FS->S,FS->Host,FS->Port,0);
if (result)
{
	STREAMSetFlushType(FS->S,FLUSH_ALWAYS,0);
	Tempstr=SetStrLen(Tempstr,3);


//SendTelnetCommand(FS, TELNET_DO, TELNET_FULLDUPLEX);
//SendTelnetCommand(FS, TELNET_DO, TELNET_BINARY);
//SendTelnetCommand(FS, TELNET_DONT, TELNET_ECHO);
//SendTelnetCommand(FS, TELNET_WILL, TELNET_ECHO);
	sleep(1);
	Tempstr=SetStrLen(Tempstr,200);


	STREAMFlush(FS->S);
	result=STREAMReadChar(FS->S);
	while (result == 0xFF)
	{
		 TelnetHandleOption(FS);
		result=STREAMReadChar(FS->S);
	}

		printf("FIN OPTS! %d %c\n",result,result);
	
	TelnetAddReadProcessor(FS->S);
	//STREAMWriteChar(FS->S,"\n");
	result=ShellFinalizeConnection(FS,0);
}

DestroyString(Tempstr);
return(result);
}



TFileStore *TelnetShellFileStoreCreate(char *Name, char *ConnectSetup)
{
TFileStore *FS;

FS=ShellFileStoreCreate(Name,ConnectSetup);
if (FS->Port==0) FS->Port=23;
FS->Create=TelnetShellFileStoreCreate;
FS->Open=TelnetShellOpen;

WritePostfix=CopyStr(WritePostfix,"\r");
WritePostfixLen=1;
FS->Flags |= FS_NEEDS_ENCODING;


return(FS);
}



/*********************** Shell over serial functions *******************/

int SerialShellOpen(TFileStore *FS)
{
int result=FALSE, fd, TtyFlags=0;
char *Tempstr=NULL, *ptr;

if (! FS) 
{
return(FALSE);
}

Tempstr=MCopyStr(Tempstr,"/dev/",FS->Host,NULL);

fd=open(Tempstr,O_RDWR);

if (fd > -1)
{
	ptr=GetVar(FS->Vars,"ConnectSettings");
	if (ptr && isalpha(*ptr))
	{
		//set hardware flow control
		if (*ptr=='H') TtyFlags |= TTYFLAG_HARDWARE_FLOW;
		if (*ptr=='S') TtyFlags |= TTYFLAG_SOFTWARE_FLOW;
		 ptr++;
	}
	InitTTY(fd,FS->Port,TtyFlags);
	FS->S=STREAMFromFD(fd);

result=ShellFinalizeConnection(FS,0);
}

DestroyString(Tempstr);

return(result);
}



int SerialShellClose(TFileStore *FS)
{

int result;
char *Tempstr=NULL;

   Tempstr=CopyStr(Tempstr,"exit");
   ShellWriteLine(Tempstr,FS->S);
	 HangUpLine(FS->S->in_fd);
   STREAMClose(FS->S);
   DestroyString(Tempstr);

return(TRUE);
}


TFileStore *SerialShellFileStoreCreate(char *Name, char *ConnectSetup)
{
TFileStore *FS;

FS=ShellFileStoreCreate(Name,ConnectSetup);
FS->Create=SerialShellFileStoreCreate;
FS->Open=SerialShellOpen;
FS->Close=SerialShellClose;

//FS->SwitchUser=ShellSwitchUser;

return(FS);
}





