#include "commands-file-info.h"


char *GenerateQuotaString(char *RetStr, TFileStore *FS)
{
double Percent;
char *Tempstr=NULL;

Percent= (FS->BytesUsed * 100.0) / FS->BytesAvailable;

Tempstr=CopyStr(Tempstr, GetHumanReadableDataQty(FS->BytesUsed,0));
RetStr=FormatStr(RetStr,"%0.2f%% of quota used (%s / %s)",Percent,Tempstr, GetHumanReadableDataQty(FS->BytesAvailable,0));

DestroyString(Tempstr);

return(RetStr);
}


int InternalRecurseInfoCommand(TFileStore *FS, char *Dir, char *Arg, int CmdFlags, ListNode *Vars, INFO_COMMAND_FUNCTOR Func)
{
int result=FALSE;

      if (FS->ChDir(FS,Dir))
      {
        if (CmdFlags & FLAG_CMD_VERBOSE) printf("Entering %s\n",Dir);
        result=Func(FS, "*", Arg, CmdFlags,Vars);
        FS->ChDir(FS,"..");
      }
      else printf("Failed to enter remote dir %s\n",Dir);

return(result);
}


void ProcessLSCmd(TFileStore *FS, TFileStore *DiskFS, int CmdFlags, char *Pattern, int LinesToList)
{
 TFileInfo *FI;
 ListNode *Items, *Curr;
 int val, count=0, total_bytes=0;
 char *Tempstr=NULL, *ptr;
 
	Items=ListCreate();
	val=LIST_INCLUDE_DIRS;
	if (CmdFlags & FLAG_LS_REFRESH) val |=LIST_REFRESH;
	if (StrLen(Pattern)==0) FileStoreLoadDir(FS,"*",Items,val);
	else FileStoreLoadDir(FS,Pattern,Items,val);

	total_bytes=DirectoryListingPrintOutput(FS, CmdFlags, LinesToList, Items);

	printf(".\n");
	if (FS->BytesAvailable > 0) Tempstr=GenerateQuotaString(Tempstr,FS);
	else Tempstr=CopyStr(Tempstr,"");
 
	if (CmdFlags & (FLAG_LS_LONGDETAILS | FLAG_LS_MEDIADETAILS)) printf("% 8d bytes total  %s\n",total_bytes,Tempstr);
	else if (CmdFlags & FLAG_LS_DETAILS) printf("% 8s total  %s\n",GetHumanReadableDataQty(total_bytes,0),Tempstr);
 
	fflush(NULL); 
	ListDestroy(Items,FileInfoDestroy);
 
	DestroyString(Tempstr);
}



void HandleCompareResult(TFileInfo *FI, int CmdFlags, int CompareLevel, char *Hook, int result)
{
char *ResultStr=NULL, *Tempstr=NULL;

	if (result >= CompareLevel) 
	{
		if (CmdFlags & FLAG_CMD_QUIET) /*Do nothing in quiet mode */  ; 
		else if (result==CMP_DIR)  /*Do nothing with dirs */ ;
		else ResultStr=CopyStr(ResultStr,"MATCHES");
	}
	else switch (result)
	{
		case CMP_FAIL:
			ResultStr=CopyStr(ResultStr,"LOCAL ONLY");
		break;

		case CMP_REMOTE:
			ResultStr=CopyStr(ResultStr,"REMOTE ONLY");
		break;

		case CMP_DIR:
		break;

		case CMP_EXISTS:
			ResultStr=CopyStr(ResultStr,"DIFFERS");
		break;

		case CMP_SIZE:
			ResultStr=CopyStr(ResultStr,"Size Matches");
		break;

		case CMP_SIZE_TIME:
			ResultStr=CopyStr(ResultStr,"Size + Time Matches");
		break;

		case CMP_MD5:
			ResultStr=CopyStr(ResultStr,"MD5 Hash Matches");
		break;

		case CMP_SHA1:
			ResultStr=CopyStr(ResultStr,"SHA1 Hash Matches");
		break;

	}

if (StrLen(ResultStr))
{
	printf("%s: %s\n",ResultStr,FI->Path);
	if (StrLen(Hook)) 
	{
		Tempstr=FormatStr(Tempstr,"%s '%s' '%s'",Hook,FI->Path,ResultStr);
		system(Tempstr);
	}
}

DestroyString(Tempstr);
DestroyString(ResultStr);
}


int InternalProcessCompareDir(TFileStore *DiskFS, TFileStore *FS, char *Command, char *Dir, char *Pattern, int CmdFlags, ListNode *Vars)
{
int result=FALSE;

		if (DiskFS->ChDir(DiskFS,Dir))
		{
			if (FS->ChDir(FS,Dir))
			{
				if (CmdFlags & FLAG_CMD_VERBOSE) printf("Entering %s\n",Dir);
				result=ProcessCompare(DiskFS, FS, Command, Pattern, CmdFlags,Vars);
				FS->ChDir(FS,"..");
			}
			else printf("Failed to enter remote dir %s\n",Dir);
			DiskFS->ChDir(DiskFS,"..");
		}
		else printf("Failed to enter local dir %s\n",Dir);

return(result);
}


int ProcessCompare(TFileStore *DiskFS, TFileStore *FS, char *Command, char *Pattern, int CmdFlags, ListNode *Vars)
{
ListNode *Items=NULL, *DestinationItems=NULL, *Curr, *Node;
char *Tempstr=NULL, *ptr;
char *IncludePattern=NULL, *ExcludePattern=NULL;
int result=CMP_FAIL, Flags=0, Checked=0, Differs=0, val;
int CompareLevel=0;
TFileInfo *FI, *RemoteFI;


//if we've been given a path to something a few directories down, then do this
ptr=strrchr(Pattern,'/');
if (ptr)
{
  Tempstr=CopyStrLen(Tempstr,Pattern,ptr-Pattern);
  ptr++;
	result=InternalProcessCompareDir(DiskFS, FS, Command, Tempstr, ptr, CmdFlags, Vars);
  DestroyString(Tempstr);
  return(result);
}


DestinationItems=ListCreate();
FileStoreLoadDir(FS,Pattern,DestinationItems, LIST_REFRESH);
IncludePattern=CopyStr(IncludePattern,GetVar(Vars,"IncludePattern"));
ExcludePattern=CopyStr(ExcludePattern,GetVar(Vars,"ExcludePattern"));
CompareLevel=atoi(GetVar(Vars,"CMP:CompareLevel"));

if (CmdFlags & FLAG_CMD_VERBOSE) HandleEventMessage(Settings.Flags,"INFO: Comparing files by %s",GetCompareLevelName(CompareLevel));

Items=ListCreate();
DiskFS->LoadDir(DiskFS,Pattern,Items,CompareLevel);
Curr=ListGetNext(Items);

while (Curr)
{
	FI=(TFileInfo *) Curr->Item;

	if (FileIncluded(FI,IncludePattern,ExcludePattern,CmdFlags, Vars))
	{
	Node=ListFindNamedItem(DestinationItems,FI->Name);
	if (Node) RemoteFI=(TFileInfo *) Node->Item;
	else RemoteFI=NULL;

	result=FileStoreCompareFile(FS,DiskFS,RemoteFI,FI,CompareLevel);
	Tempstr=CopyStr(Tempstr,FI->Name);
	Checked++;
	
	if ((result == CMP_DIR) && (CmdFlags & FLAG_CMD_RECURSE)) InternalProcessCompareDir(DiskFS, FS, Command, FI->Name, "*", CmdFlags, Vars);
	else
	{
		HandleCompareResult(FI, CmdFlags, CompareLevel, GetVar(Vars,"Hook"), result);
		if (result < CompareLevel) Differs++;
	}
	}
Curr=ListGetNext(Curr);
}


Curr=ListGetNext(DestinationItems);
while (Curr)
{
	FI=(TFileInfo *) Curr->Item;

	if (FileIncluded(FI,IncludePattern,ExcludePattern,CmdFlags, Vars)) 
	{
		
		if (! ListFindNamedItem(Items,FI->Name)) 
		{
			Tempstr=CopyStr(Tempstr,"");
			HandleCompareResult(FI, CmdFlags, CompareLevel, GetVar(Vars,"Hook"), CMP_REMOTE);
			Differs++;
			Checked++;
		}
	}

Curr=ListGetNext(Curr);
}

val=atoi(GetVar(Vars,"CMP:Differs")) + Differs;
Tempstr=FormatStr(Tempstr,"%d",val);
SetVar(Vars,"CMP:Differs",Tempstr);

val=atoi(GetVar(Vars,"CMP:Checked")) + Checked;
Tempstr=FormatStr(Tempstr,"%d",val);
SetVar(Vars,"CMP:Checked",Tempstr);



ListDestroy(DestinationItems,FileInfoDestroy);
ListDestroy(Items,FileInfoDestroy);

DestroyString(ExcludePattern);
DestroyString(IncludePattern);
DestroyString(Tempstr);

return(result);
}



int ProcessHashCompare(TFileStore *DiskFS, TFileStore *FS, char *Command, char *Pattern, int CmdFlags, ListNode *Vars)
{
ListNode *Items=NULL, *DestinationItems=NULL, *Curr, *Node;
char *H1=NULL, *H2=NULL;
char *IncludePattern=NULL, *ExcludePattern=NULL;
int result=CMP_FAIL, Flags=0, Checked=0, Differs=0, val;
int CompareLevel=0;
TFileInfo *FI, *RemoteFI;

DestinationItems=ListCreate();

Flags=LIST_REFRESH;
if (CmdFlags & FLAG_CMD_RECURSE) Flags |= LIST_INCLUDE_DIRS;
FileStoreLoadDir(FS,Pattern,DestinationItems, Flags);
IncludePattern=CopyStr(IncludePattern,GetVar(Vars,"IncludePattern"));
ExcludePattern=CopyStr(ExcludePattern,GetVar(Vars,"ExcludePattern"));


Items=ListCreate();
DiskFS->LoadDir(DiskFS,Pattern,Items,0);
Curr=ListGetNext(Items);

while (Curr)
{
	FI=(TFileInfo *) Curr->Item;

	if (FileIncluded(FI,IncludePattern,ExcludePattern,CmdFlags, Vars))
	{
	Node=ListFindNamedItem(DestinationItems,FI->Name);
	if (Node) RemoteFI=(TFileInfo *) Node->Item;
	else RemoteFI=NULL;

	if ((FI->Type==FTYPE_DIR) && (CmdFlags & FLAG_CMD_RECURSE))
	{
		if (DiskFS->ChDir(DiskFS,FI->Name))
		{
			if (FS->ChDir(FS,FI->Name))
			{
				if (CmdFlags & FLAG_CMD_VERBOSE) printf("Entering %s\n",FI->Path);
				ProcessHashCompare(DiskFS, FS, Command, "*", CmdFlags,Vars);
				FS->ChDir(FS,"..");
			}
			else printf("Failed to enter remote dir %s\n",FI->Path);
			DiskFS->ChDir(DiskFS,"..");
		}
		else printf("Failed to enter local dir %s\n",FI->Path);
	}
	else
	{
		FS->GetDigest(FS,FI->Name,"md5",&H1);
		DiskFS->GetDigest(DiskFS,FI->Name,"md5",&H2);

		if (H1 && H2 && (strcmp(H1,H2)==0)) 
		{
				if (CmdFlags & FLAG_CMD_QUIET) /*Do nothing in quiet mode */  ; 
				else printf("OKAY: Remote=%s Local=%s %s\n",H1,H2,FI->Path);
		}
		else printf("FAIL: Remote=%s Local=%s %s\n",H1,H2,FI->Path);
	}
	}
Curr=ListGetNext(Curr);
}



ListDestroy(DestinationItems,FileInfoDestroy);
ListDestroy(Items,FileInfoDestroy);

DestroyString(ExcludePattern);
DestroyString(IncludePattern);
DestroyString(H1);
DestroyString(H2);

return(result);
}

int ProcessGetDigest(TFileStore *FS, char *Pattern, char *Type, int CmdFlags, ListNode *Vars)
{
ListNode *Items=NULL, *Curr;
char *Tempstr=NULL, *IncludePattern=NULL, *ExcludePattern=NULL;
TFileInfo *FI;
int result=FALSE, Flags;

if (! FS->GetDigest) return(FALSE);

IncludePattern=CopyStr(IncludePattern,GetVar(Vars,"IncludePattern"));
ExcludePattern=CopyStr(ExcludePattern,GetVar(Vars,"ExcludePattern"));

Items=ListCreate();

Flags=LIST_REFRESH;
//if (CmdFlags & FLAG_CMD_RECURSE) Flags |= LIST_INCLUDE_DIRS;
FileStoreLoadDir(FS,Pattern,Items, Flags);
Curr=ListGetNext(Items);

while (Curr)
{
	FI=(TFileInfo *) Curr->Item;

  if (FileIncluded(FI,IncludePattern,ExcludePattern,CmdFlags, Vars))
	{
		if (FI->Type==FTYPE_DIR) 
		{
			if (CmdFlags & FLAG_CMD_RECURSE) InternalRecurseInfoCommand(FS, FI->Name, Type, CmdFlags, Vars, ProcessGetDigest);
		}
		else 
		{
			result=FS->GetDigest(FS,FI->Name,Type,&Tempstr);
			if (result)
			{
				if (CmdFlags & FLAG_CMD_RECURSE) printf("OKAY: %s %s%s\n",Tempstr,FS->CurrDir,FI->Name);
				else printf("OKAY: %s %s\n",Tempstr,FI->Name);
			}
			else printf("ERROR: %s\n",Tempstr);
		}
	}

Curr=ListGetNext(Curr);
}

ListDestroy(Items,FileInfoDestroy);

DestroyString(IncludePattern);
DestroyString(ExcludePattern);
DestroyString(Tempstr);

return(result);
}


int ProcessGetFileInfo(TFileStore *FS, char *Pattern, char *Arg, int CmdFlags, ListNode *Vars)
{
ListNode *Items=NULL, *Curr;
char *Tempstr=NULL, *IncludePattern=NULL, *ExcludePattern=NULL, *ptr;
TFileInfo *FI;
int result=FALSE, Flags, i;
char *DisplayVars[]={"Category","Author","Subtitle","Duration","Dimensions",NULL};
//"Summary","Description",NULL};

IncludePattern=CopyStr(IncludePattern,GetVar(Vars,"IncludePattern"));
ExcludePattern=CopyStr(ExcludePattern,GetVar(Vars,"ExcludePattern"));

Items=ListCreate();

Flags=LIST_REFRESH;
//if (CmdFlags & FLAG_CMD_RECURSE) Flags |= LIST_INCLUDE_DIRS;
FileStoreLoadDir(FS,Pattern,Items, Flags);
Curr=ListGetNext(Items);

while (Curr)
{
	FI=(TFileInfo *) Curr->Item;

  if (FileIncluded(FI,IncludePattern,ExcludePattern,CmdFlags, Vars))
	{
		if (FI->Type==FTYPE_DIR) 
		{
			if (CmdFlags & FLAG_CMD_RECURSE) InternalRecurseInfoCommand(FS, FI->Name, Arg,  CmdFlags, Vars, ProcessGetFileInfo);
		}
		else 
		{
			printf("\nInfo For %s\n",FI->Path);
			printf("Size: %s (%d bytes)\n",GetHumanReadableDataQty(FI->Size,0),FI->Size);
			printf("ContentType: %s\n",FI->MediaType);

			printf("Timestamp: %s\n",GetDateStrFromSecs("%Y/%m/%d %H:%M:%S",FI->Mtime,NULL));
			for (i=0; DisplayVars[i]; i++)
			{
				ptr=GetVar(FI->Vars,DisplayVars[i]);
				if (StrLen(ptr)) printf("%s: %s\n",DisplayVars[i],ptr);
			}
		}
	}

Curr=ListGetNext(Curr);
}

ListDestroy(Items,FileInfoDestroy);

DestroyString(IncludePattern);
DestroyString(ExcludePattern);
DestroyString(Tempstr);
}



int ProcessChMod(TFileStore *FS, char *Pattern, char *Mode, int CmdFlags, ListNode *Vars)
{
ListNode *Items=NULL, *Curr;
char *Tempstr=NULL, *IncludePattern=NULL, *ExcludePattern=NULL, *ptr;
TFileInfo *FI;
int result=FALSE, Flags, i;
char *DisplayVars[]={"Category","Author","Subtitle","Duration","Dimensions",NULL};
//"Summary","Description",NULL};

IncludePattern=CopyStr(IncludePattern,GetVar(Vars,"IncludePattern"));
ExcludePattern=CopyStr(ExcludePattern,GetVar(Vars,"ExcludePattern"));

Items=ListCreate();

Flags=LIST_REFRESH;
//if (CmdFlags & FLAG_CMD_RECURSE) Flags |= LIST_INCLUDE_DIRS;
FileStoreLoadDir(FS,Pattern,Items, Flags);
Curr=ListGetNext(Items);

while (Curr)
{
	FI=(TFileInfo *) Curr->Item;

  if (FileIncluded(FI,IncludePattern,ExcludePattern,CmdFlags, Vars))
	{
	if (FI->Type==FTYPE_DIR) 
	{
		if (CmdFlags & FLAG_CMD_RECURSE) InternalRecurseInfoCommand(FS, FI->Name, Mode, CmdFlags, Vars, ProcessChMod);
	}
	FS->ChMod(FS,FI,Mode);
	}

Curr=ListGetNext(Curr);
}

ListDestroy(Items,FileInfoDestroy);

DestroyString(IncludePattern);
DestroyString(ExcludePattern);
DestroyString(Tempstr);
}



