#include "filestore.h"
#include "common.h"

char *DirectoryListingGetFormat(char *RetStr, TFileStore *FS, int Flags)
{
char *Format=NULL, *ptr;

Format=CopyStr(RetStr,"");

//if (FS->Flags & FS_NOLS_DATA) return("%n %p");
//if (Flags & FLAG_LS_DEBUG) return("%n %p");


if (Flags & FLAG_LS_LONGDETAILS) 
{
	ptr=GetVar(FS->Vars,"LSFormat:Long");
	if (StrLen(ptr)) Format=CatStr(Format,ptr);
	else Format=CatStr(Format,"%S %P %m  %v %u %n");
}
else if (Flags & FLAG_LS_DETAILS) 
{
	ptr=GetVar(FS->Vars,"LSFormat:Details");
	if (StrLen(ptr)) Format=CatStr(Format,ptr);
	else Format=CatStr(Format,"%s %m  %n ");
}
else if (Flags & FLAG_LS_MEDIADETAILS) 
{
	ptr=GetVar(FS->Vars,"LSFormat:Media");
	if (StrLen(ptr)) Format=CatStr(Format,ptr);
	else Format=CatStr(Format,"%s %m  %t %x %d %n ");
}
else
{	
	if (Flags & FLAG_LS_SIZE) Format=CatStr(Format,"%s ");

	if (Flags & FLAG_LS_VERSION) Format=CatStr(Format,"%ver ");

	Format=CatStr(Format,"%n ");

	if (Flags & FLAG_LS_SHARELINK) Format=CatStr(Format,"%l ");
}

return(Format);
}


#define LS_STRING 0
#define LS_INT 1
#define LS_FLOAT 2

char *DirectoryListingSizeItem(char *RetStr, int Type, char *Item, int MinLen, int MaxLen)
{
int len;
char *Output=NULL;

if (Type==LS_INT) Output=FormatStr(RetStr,"%d",*(int *) Item);
else if (Type==LS_FLOAT) Output=FormatStr(RetStr,"%f",*(float *) Item);
else Output=CopyStr(RetStr,Item);

len=StrLen(Output);

if ((MaxLen > 0) && (len > MaxLen)) Output[MaxLen]='\0';
else while (len < MinLen) Output=AddCharToBuffer(Output,len++,' ');

return(Output);
}

#define LS_MEDIATYPE_LEN 15

void DirectoryListingPrintItemOutput(TFileStore *FS, int Flags, TFileInfo *FI)
{
char *ptr, *fmtptr;
char *Format=NULL, *Line=NULL, *Tempstr=NULL;
int len;
	
Format=DirectoryListingGetFormat(Format,FS, Flags);

fmtptr=Format;
while (fmtptr && *fmtptr)
{
if (*fmtptr=='%')
{
fmtptr++;

switch (*fmtptr)
{
	case '%': Line=AddCharToStr(Line,'%'); break;

	//File Name
	case 'n': Line=CatStr(Line,FI->Name); break;

	//Full Path
	case 'p': Line=CatStr(Line,FI->Path); break;

	//Media Type
	case 't':
		if (StrLen(FI->MediaType) > LS_MEDIATYPE_LEN) 
		{
			ptr=strrchr(FI->MediaType,'/');
			if (ptr) ptr++;
			else ptr=FI->MediaType;
		}
		else ptr=FI->MediaType;

		Tempstr=DirectoryListingSizeItem(Tempstr, LS_STRING, ptr, LS_MEDIATYPE_LEN, LS_MEDIATYPE_LEN);
		Line=CatStr(Line,Tempstr); 
	break;
	
	//File Size
	case 's':
		if (FI->Type==FTYPE_DIR) ptr="DIR";
		else ptr=GetHumanReadableDataQty(FI->Size,0);
		Tempstr=DirectoryListingSizeItem(Tempstr, LS_STRING, ptr,7,0);
		Line=CatStr(Line,Tempstr); 
	break;

	//Full file size (in bytes)
	case 'S':
		if (FI->Type==FTYPE_DIR)
		{
			ptr="DIR";
			Tempstr=DirectoryListingSizeItem(Tempstr, LS_STRING, ptr,7,0);
		}
		else 
		{
			Tempstr=DirectoryListingSizeItem(Tempstr, LS_INT, &FI->Size,7,0);
		}
		Line=CatStr(Line,Tempstr); 
	break;


	//Modify Time
	case 'm':
		Line=CatStr(Line,GetDateStrFromSecs("%d/%m/%Y %H:%M:%S",FI->Mtime,NULL));
	break;

	//Permissions
	case 'P':
		Tempstr=DirectoryListingSizeItem(Tempstr, LS_STRING, FI->Permissions,9,9);
		Line=CatStr(Line,Tempstr);
	break;


	//Sharing Link
	case 'l':
		ptr=GetVar(FI->Vars,"DownloadURL");
		Line=CatStr(Line,ptr); 
	break;

	//owner/author/user
	case 'u':
		Tempstr=DirectoryListingSizeItem(Tempstr, LS_STRING, FI->User,15,15);
		Line=CatStr(Line,Tempstr); 
	break;

	//File Version
	case 'v':
		if (FI->Type!=FTYPE_DIR) Tempstr=FormatStr(Tempstr, "ver:%d",FI->Version);
		else Tempstr=CopyStr(Tempstr,"");
		Tempstr=DirectoryListingSizeItem(Tempstr, LS_STRING, Tempstr,6,0);
		Line=CatStr(Line,Tempstr); 
	break;

	case 'd':
		ptr=GetVar(FI->Vars,"Dimensions");
		if (ptr) ptr=GetVar(FI->Vars,"Duration");
		Tempstr=DirectoryListingSizeItem(Tempstr, LS_STRING, ptr,9,0);
		Line=CatStr(Line,Tempstr); 
	break;

	case 'x':
		ptr=GetVar(FI->Vars,"x-rated");
		Tempstr=DirectoryListingSizeItem(Tempstr, LS_STRING, ptr,8,0);
		Line=CatStr(Line,Tempstr); 
	break;

}

}
else Line=AddCharToStr(Line,*fmtptr);

fmtptr++;
}


printf("%s\n",Line);

DestroyString(Line);
DestroyString(Tempstr);
DestroyString(Format);
}


int DirectoryListingPrintOutput(TFileStore *FS, int CmdFlags, int LinesToList, ListNode *Items)
{
ListNode *Curr;
TFileInfo *FI;
int count, total_bytes=0;

   Curr=ListGetNext(Items);
   while (Curr)
   {
       FI=(TFileInfo *) Curr->Item;
       DirectoryListingPrintItemOutput(FS,CmdFlags,FI);
       total_bytes+=FI->Size;
       if ((LinesToList > 0) && (count > LinesToList)) break;
       count++;
       Curr=ListGetNext(Curr);
   }

return(total_bytes);
}
