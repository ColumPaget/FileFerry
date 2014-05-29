#include "xml.h"
#include "http.h"

#define XML_DATE_FMT "%Y-%m-%dT%H:%M:%S"

//This function specifically reads RSS style 'media:content' links and 
//creates fileinfo items from them. Any details about the item that
//come from outside this link are passed in as 'Vars'

void RSSReadLink(char *TagData, ListNode *Vars, ListNode *Items)
{
char *Name=NULL, *Value=NULL, *ptr, *ptr2;
int Height=0, Width=0;
TFileInfo *FI;

FI=FileInfoCreate("", "", FTYPE_FILE, 0, 0);

//These vars can be overrriden
ptr=GetVar(Vars,"Width");
if (ptr)
{
	Width=atoi(ptr);
	UnsetVar(Vars,"Width");
}

ptr=GetVar(Vars,"Height");
if (ptr)
{
	Height=atoi(ptr);
	UnsetVar(Vars,"Height");
}

ptr=GetVar(Vars,"Size");
if (ptr)
{
	FI->Size=atoi(ptr);
	UnsetVar(Vars,"Size");
}

ptr=GetNameValuePair(TagData," ","=",&Name,&Value);
while (ptr)
{
	if (strcasecmp(Name,"url")==0) 
	{
	FI->Path=CopyStr(FI->Path,Value);
	FI->Name=CopyStr(FI->Name,GetBasename(Value));
	ptr2=strchr(FI->Name,'?');
	if (ptr2) *ptr2='\0';
	}
	else if (strcasecmp(Name,"length")==0) FI->Size=atoi(Value);
	else if (strcasecmp(Name,"fileSize")==0) FI->Size=atoi(Value);
	else if (strcasecmp(Name,"height")==0) Height=atoi(Value);
	else if (strcasecmp(Name,"width")==0) Width=atoi(Value);
	else if (strcasecmp(Name,"medium")==0) FI->MediaType=MCopyStr(FI->MediaType,Value,"/?",NULL);
	else if (strcasecmp(Name,"type")==0) FI->MediaType=CopyStr(FI->MediaType,Value);
  if (strcmp(Name,"src")==0) 
  {       
    ptr2=strrchr(Value,'\'');
    if (ptr2) *ptr2='\0';
    SetVar(FI->Vars,"DownloadURL",Value);
  }

ptr=GetNameValuePair(ptr," ","=",&Name,&Value);
}

CopyVars(FI->Vars,Vars);

if (Width > 0)
{
	Value=FormatStr(Value,"%dx%d",Width,Height);
	SetVar(FI->Vars,"Dimensions",Value);
}


ptr=GetVar(Vars,"published");
FI->Mtime=DateStrToSecs(TagData,XML_DATE_FMT,NULL);


if (FI->Mtime < 1)
{
ptr=GetVar(Vars,"pubDate");
if (StrLen(ptr))
{
	FI->Mtime=DateStrToSecs("%a, %d %b %Y %H:%M:%S %Z",ptr,NULL);
	//Handle broken RSS streams
	if (FI->Mtime < 1) FI->Mtime=DateStrToSecs("%d %b %Y %H:%M:%S %Z",ptr,NULL);
}
}

FI->User=CopyStr(FI->User,GetVar(Vars,"Author"));


if (! ListFindNamedItem(Items,FI->Name))
{
	ListAddNamedItem(Items,FI->Name,FI);
}
else FileInfoDestroy(FI);


DestroyString(Name);
DestroyString(Value);
}






char *XMLReadItemEntry(TFileStore *FS,char *XML, ListNode *Items)
{
char *gTags[]={"gphoto:id","gphoto:size","gphoto:width","gphoto:height",NULL};
typedef enum {GPHOTO_ID,GPHOTO_SIZE,GPHOTO_WIDTH,GPHOTO_HEIGHT} GTAGS;
char *iTags[]={"itunes:summary","itunes:subtitle","itunes:image","itunes:category","itunes:explicit","itunes:author","itunes:owner","itunes:duration","itunes:subtitle",NULL};
typedef enum {ITUNES_SUMMARY,ITUNES_SUBTITLE,ITUNES_IMAGE,ITUNES_CATEGORY,ITUNES_EXPLICIT,ITUNES_AUTHOR,ITUNES_OWNER,ITUNES_DURATION} ITAGS;
char *mTags[]={"media:content","media:credit","media:rating","media:price","media:licence","media:subtitle","media:hash",NULL};
typedef enum {MEDIA_CONTENT, MEDIA_CREDIT, MEDIA_RATING, MEDIA_PRICE, MEDIA_LICENCE, MEDIA_SUBTITLE, MEDIA_HASH};
char *RSSTags[]={"title","content","category","description","enclosure","published","pubDate","updated",NULL};
typedef enum {RSS_TITLE,RSS_CONTENT,RSS_CATEGORY,RSS_DESCRIPTION,RSS_ENCLOSURE,RSS_PUBLISHED,RSS_PUBDATE,RSS_UPDATED} RSS_TAGS;

char *Token=NULL, *TagName=NULL, *TagData=NULL, *Media=NULL, *mptr, *ptr;
int result, Width, Height, WithinItem=TRUE;
ListNode *Vars;

Vars=ListCreate();
ptr=XMLGetTag(XML,NULL,&TagName,&TagData);
while (ptr)
{
			switch (*TagName)
			{
			case 'a':
				if (strcmp(TagName,"a")==0) HTTPReadLink(FS, TagData, "*", Items);
			break;

			//this should prevent any strcmps for these unused tags
			case 'b':
			case 'f':
			case 'h':
			case 'j':
			case 'k':
			case 'l':
			case 'n':
			case 'o':
			case 'q':
			case 'x':
			break;

			case 'g':
			result=MatchTokenFromList(TagName,gTags,0);
			switch (result)
			{
				case GPHOTO_ID:
					ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
					SetVar(Vars,"Path",TagData);
				break;

				case GPHOTO_SIZE:
					ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
					SetVar(Vars,"Size",TagData);
				break;

				case GPHOTO_WIDTH:
					ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
					SetVar(Vars,"Width",TagData);
				break;

				case GPHOTO_HEIGHT:
					ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
					SetVar(Vars,"Height",TagData);
				break;
			}
			break;


			case 'i':
			result=MatchTokenFromList(TagName,iTags,0);
			switch (result)
			{
			case ITUNES_DURATION: 
				ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
				SetVar(Vars,"Duration",TagData);
			break;

			case ITUNES_EXPLICIT: 
				ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
				if (strcasecmp(TagData,"yes")==0) SetVar(Vars,"x-rated","Explicit");
			break;

			case ITUNES_AUTHOR:
				ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
				strrep(TagData,' ','_');
				SetVar(Vars,"Author",TagData);
			break;

			case ITUNES_SUMMARY:
				ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
				Token=HtmlDeQuote(Token,TagData);
				SetVar(Vars,"Summary",Token);
			break;

			case ITUNES_SUBTITLE:
				ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
				SetVar(Vars,"Subtitle",TagData);
			break;

			case ITUNES_IMAGE:
				ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
				SetVar(Vars,"Image",TagData);
			break;

			case ITUNES_CATEGORY:
				ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
				SetVar(Vars,"Category",TagData);
			break;
			}
			break;


			case 'm':
			result=MatchTokenFromList(TagName,mTags,0);
			switch (result)
			{
			case MEDIA_CONTENT: 
				Media=MCatStr(Media,TagData,",",NULL);
			break;

			case MEDIA_CREDIT:
			if (strstr(TagData,"role=\"author\""))
			{
				ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
				if (strncasecmp(TagData,"http:",5) !=0) 
				{
				strrep(TagData,' ','_');
				SetVar(Vars,"Author",TagData);
				}
			}
			break;

			case MEDIA_RATING:
			ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
			SetVar(Vars,"x-rated",TagData);
			break;

			case MEDIA_PRICE:
			break;

			case MEDIA_LICENCE:
			ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
			SetVar(Vars,"Licence",TagData);
			break;

			case MEDIA_SUBTITLE:
			ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
			SetVar(Vars,"SubTitle",TagData);
			break;

			case MEDIA_HASH:
			break;

			}
			break;

			case '/':
			if (
						(strcasecmp(TagName,"/item")==0) ||
						(strcasecmp(TagName,"/entry")==0)
				) WithinItem=FALSE;
			break;

			default:
			result=MatchTokenFromList(TagName,RSSTags,0);
			switch(result)
			{
				case RSS_TITLE: 
					ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
					SetVar(Vars,"Title",TagData);
				break;

				case RSS_CONTENT:
				Media=MCatStr(Media,TagData,",",NULL);
				break;

				case RSS_CATEGORY:
				ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
				SetVar(Vars,"Category",TagData);
				break;

				case RSS_DESCRIPTION:
				ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
				SetVar(Vars,"Description",TagData);
				break;

				case RSS_ENCLOSURE:
				Media=MCatStr(Media,TagData,",",NULL);
				break;

				case RSS_PUBLISHED:
					ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
					SetVar(Vars,"published",TagData);
				break;

				case RSS_PUBDATE:	
				ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
				SetVar(Vars,"pubDate",TagData);
				break;
			}
		}

//Must do this hear, before reading next tag!
if (! WithinItem) break;
ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
}


//must use another ptr here, as we return ptr
mptr=GetToken(Media,",",&TagData,0);
while (mptr)
{
RSSReadLink(TagData, Vars, Items);
mptr=GetToken(mptr,",",&TagData,0);
}

ListDestroy(Vars,DestroyString);
DestroyString(TagName);
DestroyString(TagData);
DestroyString(Token);

return(ptr);
}


