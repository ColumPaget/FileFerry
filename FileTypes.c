#include "FileTypes.h"

ListNode *MimeTypes=NULL, *MimeTypesByExtn=NULL;

TMimeType *AddMediaType(char *Extn, char *Type)
{
TMimeType *MT=NULL;


if (! MimeTypes) MimeTypes=ListCreate();
if (! MimeTypesByExtn) MimeTypesByExtn=ListCreate();

if (! (StrLen(Extn) &&  ListFindNamedItem(MimeTypesByExtn,Extn)) )
{
	MT=(TMimeType *) calloc(1,sizeof(TMimeType));
	MT->MimeType=CopyStr(MT->MimeType,Type);
	MT->Extn=CopyStr(MT->Extn,Extn);
	if (! ListFindNamedItem(MimeTypes,Type)) ListAddNamedItem(MimeTypes,Type,MT);
	ListAddNamedItem(MimeTypesByExtn,Extn,MT);
}
return(MT);
}


void AddDefaultFileMagics()
{
char *Extns[]={"jpg","png","gif","pnm","bmp","txt","html","ps","pdf","flv","mp3","mpeg","mp4","mpg",NULL};
char *Types[]={"image/jpeg","image/png","image/gif","image/pnm","image/bmp","text/plain","text/html","application/ps","application/pdf","video/flv","audio/mpeg","video/mpeg","video/mp4","video/mpeg",NULL};
int i;


for (i=0; Extns[i]!=NULL; i++)
{
	AddMediaType(Extns[i],Types[i]);
}

}

void LoadFileMagics(char *MimeTypesPath, char *MagicsPath)
{
STREAM *S;
char *Tempstr=NULL, *Token=NULL, *ContentType=NULL, *ptr;
int len;

if (! MimeTypes) MimeTypes=ListCreate();
if (! MimeTypesByExtn) MimeTypesByExtn=ListCreate();


AddDefaultFileMagics();
S=STREAMOpenFile(MimeTypesPath,O_RDONLY);
if (S)
{
	Tempstr=STREAMReadLine(Tempstr,S);
	while (Tempstr)
	{
		StripTrailingWhitespace(Tempstr);
		StripLeadingWhitespace(Tempstr);
	
		ptr=GetToken(Tempstr,"\\S",&ContentType,0);
		ptr=GetToken(ptr,"\\S",&Token,0);

		while (ptr)
		{
			AddMediaType(Token,ContentType);
			ptr=GetToken(ptr,"\\S",&Token,0);
		}
		Tempstr=STREAMReadLine(Tempstr,S);
	}
STREAMClose(S);
}


DestroyString(ContentType);
DestroyString(Tempstr);
DestroyString(Token);
}


void LoadIconsForMimeTypes(char *MimeTypesMapFile)
{
STREAM *S;
char *Tempstr=NULL, *Token=NULL, *ContentType=NULL, *ptr;
int len;
ListNode *Curr;
TMimeType *MT;

S=STREAMOpenFile(MimeTypesMapFile,O_RDONLY);
if (S)
{
Tempstr=STREAMReadLine(Tempstr,S);
while (Tempstr)
{
StripTrailingWhitespace(Tempstr);
StripLeadingWhitespace(Tempstr);

ptr=GetToken(Tempstr,"\\S",&ContentType,0);
ptr=GetToken(ptr,"\\S",&Token,0);

	Curr=ListFindNamedItem(MimeTypes,ContentType);
	if (Curr)
	{
		MT=(TMimeType *) Curr->Item;
		MT->IconPath=CopyStr(MT->IconPath,Token);
	}
	else
	{
		MT=AddMediaType("",ContentType);
		MT->IconPath=CopyStr(MT->IconPath,Token);
	}
Tempstr=STREAMReadLine(Tempstr,S);
}
STREAMClose(S);
}

}



char *GuessMediaTypeFromPath(char *OutBuff,char *Path)
{
char *ptr;
char *Tempstr=NULL;
ListNode *Curr;
TMimeType *MT;

ptr=strrchr(Path,'.');
if (ptr)
{
  Tempstr=CopyStr(Tempstr,ptr+1);
  strlwr(Tempstr);
  Curr=ListFindNamedItem(MimeTypesByExtn,Tempstr);

  if (Curr)
  {
	  MT=(TMimeType *) Curr->Item;
	  ptr=MT->MimeType;
  }
  else ptr="";
}
else ptr="";

DestroyString(Tempstr);
return(CopyStr(OutBuff,ptr));
}


char *GetExtensionForMediaType(char *OutBuff,char *MediaType)
{
char *ptr;
char *Tempstr=NULL, *RetStr=NULL;
ListNode *Curr;
TMimeType *MT;

  RetStr=CopyStr(OutBuff,"");
  Tempstr=CopyStr(Tempstr,MediaType);
  strlwr(Tempstr);

  Curr=ListFindNamedItem(MimeTypes,MediaType);
  if (Curr)
  {
	  MT=(TMimeType *) Curr->Item;
	  if (strcmp(MT->MimeType,MediaType)==0)
	  {
		RetStr=CopyStr(RetStr,MT->Extn);
	  }
  }

DestroyString(Tempstr);
return(RetStr);
}



int IsSoundFile(char *FileName)
{
char *Extns[]={".wav",".au",NULL};
char *ptr;

ptr=strrchr(FileName,'.');
if (StrLen(ptr) && (MatchTokenFromList(ptr,Extns,0) > -1)) return(TRUE);

return(FALSE);
}

int IsContainerFile(char *FileName)
{
char *Extns[]={".tar",".zip",".pdf",".ps",NULL};
char *ptr;

ptr=strrchr(FileName,'.');
if (StrLen(ptr) && (MatchTokenFromList(ptr,Extns,0) > -1)) return(TRUE);

return(FALSE);
}


int GetMediaType(char *Type)
{
if (strncmp(Type,"image/",6)==0) return(FTYPE_IMAGE);
if (strncmp(Type,"video/",6)==0) return(FTYPE_MEDIA);
if (strncmp(Type,"audio/",6)==0) return(FTYPE_MEDIA);

return(FTYPE_FILE);
}


int GetPathType(char *Path)
{
char *Tempstr=NULL;
int result=FALSE;

Tempstr=GuessMediaTypeFromPath(Tempstr,Path);
result=GetMediaType(Tempstr);

DestroyString(Tempstr);
return(result);
}



int GetItemType(TFileInfo *Item)
{
if (StrLen(Item->MediaType)==0) Item->MediaType=GuessMediaTypeFromPath(Item->MediaType,Item->Path);
return(GetMediaType(Item->MediaType));
}

