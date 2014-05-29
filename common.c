#include "common.h"

char *CacheDir=NULL;
time_t StartupTime;
STREAM *LogFile=NULL;
TSettings Settings;

int Flags=0;

char *CompareLevelNames[]={"none","exists","size","size+time","crc32","md5","sha1","sha",NULL};
int CompareLevelTypes[]={CMP_FAIL,CMP_EXISTS,CMP_SIZE,CMP_SIZE_TIME,CMP_CRC,CMP_MD5,CMP_SHA1,CMP_SHA1};


char *GetCompareLevelName(int Type)
{
int i;

for (i=0; CompareLevelNames[i] !=NULL; i++)
{
	if (CompareLevelTypes[i]==Type) return(CompareLevelNames[i]);
}

return("?");
}


int GetCompareLevelTypes(char *TypeList)
{
char *Token=NULL, *ptr;
int val, result=0;

ptr=GetToken(TypeList," ",&Token,0);
while (ptr)
{
val=MatchTokenFromList(Token,CompareLevelNames,0);
if (val > -1) result |= CompareLevelTypes[val];
ptr=GetToken(ptr," ",&Token,0);
}

DestroyString(Token);

return(result);
}

void WriteLog(char *Msg)
{
char *Tempstr=NULL;

    if (LogFile)
    {
      Tempstr=MCopyStr(Tempstr,GetDateStr("%Y/%m/%d %H:%M:%S: ",NULL),Msg,"\n",NULL);
      STREAMWriteLine(Tempstr,LogFile);
      STREAMFlush(LogFile);
    }

DestroyString(Tempstr);
}


void HandleEventMessage(int Flags, char *FmtStr, ...)
{
char *Tempstr=NULL;
va_list args;

va_start(args,FmtStr);
Tempstr=VFormatStr(Tempstr,FmtStr,args);
va_end(args);

if (! (Flags & FLAG_QUIET)) printf("%s\n",Tempstr);
//if (Flags & FLAG_LOG)
 WriteLog(Tempstr);

DestroyString(Tempstr);
}



char *TerminateDirectoryPath(char *DirPath)
{
char *ptr, *RetStr=NULL;

if (! DirPath) return(CopyStr(DirPath,"/"));
RetStr=DirPath;
ptr=RetStr+StrLen(RetStr)-1;
if (*ptr != '/') RetStr=AddCharToStr(RetStr,'/');

return(RetStr);
}



void SetIntegerVar(ListNode *Vars, char *Name, int Value)
{
char *Buffer=NULL;

Buffer=FormatStr(Buffer,"%d",Value);
SetVar(Vars,Name,Buffer);
DestroyString(Buffer);
}





TFileInfo *FileInfoCreate(char *Name, char *Path, int Type, int Size, int Mtime)
{
TFileInfo *FI;
char *ptr;

FI=(TFileInfo *) calloc(1,sizeof(TFileInfo));
if (StrLen(Name) && (*Name != '/')) FI->Name=CopyStr(FI->Name,Name);
else FI->Name=CopyStr(FI->Name,GetBasename(Path));

FI->Path=CopyStr(FI->Path,Path);
FI->Type=Type;
FI->Size=Size;
FI->Mtime=Mtime;
FI->Vars=ListCreate();
return(FI);
}


void FileInfoDestroy(TFileInfo *FI)
{
if (! FI) return;

DestroyString(FI->Name);
DestroyString(FI->Path);
DestroyString(FI->EditPath);
DestroyString(FI->Permissions);
DestroyString(FI->User);
DestroyString(FI->Group);
DestroyString(FI->MediaType);
DestroyString(FI->ID);
DestroyString(FI->Hash);

ListDestroy(FI->Vars,DestroyString);
free(FI);
}

TFileInfo *FileInfoClone(TFileInfo *FI)
{
TFileInfo *NewFI;
ListNode *Curr;
char *ptr;

NewFI=(TFileInfo *) calloc(1,sizeof(TFileInfo));
NewFI->Name=CopyStr(NewFI->Name,FI->Name);
NewFI->Path=CopyStr(NewFI->Path,FI->Path);
NewFI->EditPath=CopyStr(NewFI->EditPath,FI->EditPath);
NewFI->Permissions=CopyStr(NewFI->Permissions,FI->Permissions);
NewFI->User=CopyStr(NewFI->User,FI->User);
NewFI->MediaType=CopyStr(NewFI->MediaType,FI->MediaType);
NewFI->ID=CopyStr(NewFI->ID,FI->ID);
NewFI->Hash=CopyStr(NewFI->Hash,FI->Hash);
NewFI->Type=FI->Type;
NewFI->Size=FI->Size;
NewFI->Mtime=FI->Mtime;
NewFI->Version=FI->Version;

NewFI->Vars=ListCreate();
Curr=ListGetNext(FI->Vars);
while (Curr)
{
	SetVar(NewFI->Vars,Curr->Tag,Curr->Item);
	Curr=ListGetNext(Curr);
}

return(NewFI);
}








char *CleanBadChars(char *OutBuff, char *InStr)
{
char *ptr, *Tempstr=NULL;

Tempstr=CopyStr(OutBuff,"");
if (StrLen(InStr)==0) return(Tempstr);

ptr=InStr;
while (*ptr != '\0')
{
	switch (*ptr)
	{
		case '\'':
		case '\"':
		break;

		default:
		Tempstr=AddCharToStr(Tempstr,*ptr);
	}
	ptr++;
}

return(Tempstr);
}


char *PathChangeExtn(char *RetStr, char *Path, char *NewExtn)
{
char *NewName=NULL, *ptr;

 NewName=CopyStr(RetStr,Path);
 ptr=strrchr(NewName,'.');
 if (ptr) *ptr='\0';

 ptr=NewExtn;
 if (ptr && (*ptr=='.')) ptr++;
 NewName=MCatStr(NewName,".",ptr,NULL);
return(NewName);
}


char *STREAMReadDocument(char *Buffer, STREAM *S, int StripCR)
{
char *Tempstr=NULL, *Doc=NULL;
int result=FALSE;

/*
Token=SetStrLen(Token,BufSiz);
result=STREAMReadBytes(FS->S,Token,BufSiz);
while (result > EOF)
{
Tempstr=AddBytesToBuffer(Tempstr,len,Token,result);
len+=result;
result=STREAMReadBytes(FS->S,Token,BufSiz);
}
*/


Doc=CopyStr(Buffer,"");
Tempstr=STREAMReadLine(Tempstr,S);
while (Tempstr)
{
	if (StripCR) StripTrailingWhitespace(Tempstr);
	Doc=CatStr(Doc,Tempstr);

	Tempstr=STREAMReadLine(Tempstr,S);
}

DestroyString(Tempstr);
return(Doc);
}


int CheckFileMtime(TFileInfo *FI, int CmdFlags, ListNode *Vars)
{
time_t Since=0;
int result=TRUE;
char *ptr;

if (CmdFlags & FLAG_CMD_YOUNGERTHAN)
{
	ptr=GetVar(Vars,"Since");
	if (ptr) 
	{
		Since=atoi(ptr);
		//unset result if file is NOT YOUNGERTHAN
		if (FI->Mtime < Since) result=FALSE;
	}
}


if (CmdFlags & FLAG_CMD_OLDERTHAN)
{
	ptr=GetVar(Vars,"Since");
	if (ptr) 
	{
		Since=atoi(ptr);
		//unset result if file is NOT OLDERTHAN
		if (FI->Mtime > Since) result=FALSE;
	}
}

return(result);
}



int FileIncluded(TFileInfo *FI, char *IncludePattern, char *ExcludePattern, int CmdFlags, ListNode *Vars)
{
int result=FALSE;
char *Token=NULL, *ptr;

//For now we aren't allowing these file types
switch (FI->Type)
{
case FTYPE_LINK:
case FTYPE_CHARDEV:
case FTYPE_BLKDEV:
case FTYPE_SOCKET:
	return(FALSE);
break;

case FTYPE_DIR:
	if (! (CmdFlags & FLAG_CMD_RECURSE)) return(FALSE);
break;

default:
break;
}

if (StrLen(IncludePattern)==0) result=TRUE;
ptr=GetToken(ExcludePattern,",",&Token,0);
while (ptr)
{
	if (StrLen(Token) && (fnmatch(Token,FI->Name,FNM_CASEFOLD)==0)) result=FALSE;
	ptr=GetToken(ptr,",",&Token,0);
}

//Includes override excludes
ptr=GetToken(IncludePattern,",",&Token,0);
while (ptr)
{
		if (StrLen(Token) && (fnmatch(Token,FI->Name,FNM_CASEFOLD)==0)) result=TRUE;
		ptr=GetToken(ptr,",",&Token,0);
}

DestroyString(Token);

return(result);
}




void RequestAuthDetails(char **Logon, char **Passwd)
{
STREAM *S;
char inchar;

S=STREAMFromFD(0);
if (! StrLen(*Logon))
{
printf("Logon: "); fflush(NULL);
*Logon=STREAMReadLine(*Logon,S);
StripCRLF(*Logon);
}

if (! StrLen(*Passwd))
{
//Turn off echo (and other things)
InitTTY(0,0,0);
printf("Password: "); fflush(NULL);
inchar=STREAMReadChar(S);
while ((inchar != EOF) && (inchar != '\n') && (inchar != '\r')) 
{
*Passwd=AddCharToStr(*Passwd,inchar);
write(1,"*",1);
inchar=STREAMReadChar(S);
}
StripCRLF(*Passwd);
//turn echo back on
ResetTTY(0);
}
printf("\n");
STREAMDisassociateFromFD(S);
}


int ConvertFModeToNum(char *Mode)
{
char *ptr;
int Val=0;

if (! Mode) return(0600);

ptr=Mode;

//1st letter will be 'd' or '-'. If it's not, then
//abandon trying to decode FMode
if ((*ptr != 'd') && (*ptr != '-')) 
{
	if (strcmp(ptr,"public")==0) return(0666);
	return(0600);
}
ptr++;

if (*ptr=='r') Val |=S_IRUSR;
ptr++;
if (*ptr=='w') Val |=S_IWUSR;
ptr++;
if (*ptr=='x') Val |=S_IXUSR;
ptr++;

if (*ptr=='r') Val |=S_IRGRP;
ptr++;
if (*ptr=='w') Val |=S_IWGRP;
ptr++;
if (*ptr=='x') Val |=S_IXGRP;
ptr++;

if (*ptr=='r') Val |=S_IROTH;
ptr++;
if (*ptr=='w') Val |=S_IWOTH;
ptr++;
if (*ptr=='x') Val |=S_IXOTH;
ptr++;

return(Val);
}

char *ConvertFModeItem(char *ModeStr, int *pos, int Mode, int Flag, char Symbol)
{
char *RetStr=NULL;


if (Mode & Flag) RetStr=AddCharToBuffer(ModeStr,*pos,Symbol);
else RetStr=AddCharToBuffer(ModeStr,*pos,'-');


(*pos)++;

return(RetStr);
}

char* ConvertNumToFMode(char *ModeStr, int Mode)
{
char *RetStr=NULL;
int pos=0;

RetStr=ConvertFModeItem(ModeStr, &pos, Mode, 0, 'd');

RetStr=ConvertFModeItem(RetStr, &pos, Mode, S_IRUSR, 'r');
RetStr=ConvertFModeItem(RetStr, &pos, Mode, S_IWUSR, 'w');
RetStr=ConvertFModeItem(RetStr, &pos, Mode, S_IXUSR, 'x');

RetStr=ConvertFModeItem(RetStr, &pos, Mode, S_IRGRP, 'r');
RetStr=ConvertFModeItem(RetStr, &pos, Mode, S_IWGRP, 'w');
RetStr=ConvertFModeItem(RetStr, &pos, Mode, S_IXGRP, 'x');

RetStr=ConvertFModeItem(RetStr, &pos, Mode, S_IROTH, 'r');
RetStr=ConvertFModeItem(RetStr, &pos, Mode, S_IWOTH, 'w');
RetStr=ConvertFModeItem(RetStr, &pos, Mode, S_IXOTH, 'x');

return(RetStr);
}



TFileInfo *OpenFileParseArgs(char *Path, char *Args)
{
TFileInfo *FI;
char *Name=NULL, *Value=NULL, *ptr;

FI=FileInfoCreate(Path, Path, FTYPE_FILE, 0, 0);
FI->MediaType=CopyStr(FI->MediaType,"application/octet-stream");
ptr=GetNameValuePair(Args," ","=",&Name,&Value);

while (ptr)
{
	if (strcmp(Name,"Name")==0) FI->Name=CopyStr(FI->Name,Value);
	else if (strcmp(Name,"ContentType")==0) FI->MediaType=CopyStr(FI->MediaType,Value);
	else if (strcmp(Name,"Version")==0) FI->Version=atoi(Value);
	else if (strcmp(Name,"Size")==0) FI->Size=atoi(Value);
	else if (strcmp(Name,"Mtime")==0) FI->Mtime=atoi(Value);
	else if (strcmp(Name,"Permissions")==0) FI->Permissions=CopyStr(FI->Permissions,Value);
	ptr=GetNameValuePair(ptr," ","=",&Name,&Value);
}

DestroyString(Name);
DestroyString(Value);

return(FI);
}



