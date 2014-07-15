#include "google-data.h"



/*
OA: {
OA:   "access_token" : "ya29.AHES6ZTYN4vNboTWhDbouZ5p_bBAxgqfmosM-OwheUWkAZuDmI-0",
OA:   "token_type" : "Bearer",
OA:   "expires_in" : 3600,
OA:   "id_token" : "eyJhbGciOiJSUzI1NiIsImtpZCI6ImJjN2M4ZjU1ZGZiYzRkY2JiNjQ3N2RmZGM1OGJlNzQ3ZWRkNjE0ZmUifQ.eyJpc3MiOiJhY2NvdW50cy5nb29nbGUuY29tIiwiYXVkIjoiNDMzNTAyNDEyMDU2LmFwcHMuZ29vZ2xldXNlcmNvbnRlbnQuY29tIiwiYXRfaGFzaCI6Il9WNFVFQjN0cU4tcnJkREg0djFCb3ciLCJhenAiOiI0MzM1MDI0MTIwNTYuYXBwcy5nb29nbGV1c2VyY29udGVudC5jb20iLCJzdWIiOiIxMTA2NDEwMTM2MDkyMTQzNDQ0NDIiLCJpYXQiOjEzODAxMzM5NjUsImV4cCI6MTM4MDEzNzg2NX0.6l2JQaH685pA3pptPwlEP7WLr1ZHvPo9iNaFpS44If7WvK6xapVlDY_Uc9-dtT6pZUwFz1THwDfPcVGA-9IQMc8pexbqlYtirwlAoQYberx9mK_Y2hmK0YdRwqba14eomvPoj9e65PuYYBz67nOTfllCdqxcBQm3iAO6sw1zdqI",
OA:   "refresh_token" : "1/q4LDRuWLwAqOpe2VfhqeO3bTVg8AWQDx5ELy8DZU2RQ"
OA: }
*/



#define GOOGLE_CLIENT_ID "596195748702.apps.googleusercontent.com" 
#define GOOGLE_CLIENT_SECRET "YDYv4JZMI3umD80S7Xh_WNJV"


int GoogleOAuth(TFileStore *FS)
{
char *RefreshToken=NULL, *URL=NULL;
char *Tempstr=NULL;


HTTPSetFlags(HTTP_DEBUG);

RefreshToken=CopyStr(RefreshToken,GetVar(FS->Vars,"OAuthRefreshToken"));

if (StrLen(RefreshToken))
{
	OAuthDeviceRefreshToken("https://accounts.google.com/o/oauth2/token", GOOGLE_CLIENT_ID, GOOGLE_CLIENT_SECRET, RefreshToken, &FS->Passwd, &RefreshToken);
	printf("REFRESH: %s\n",FS->Passwd);
	SetVar(FS->Vars,"OAuthRefreshToken",RefreshToken);
}
else
{
	OAuthInstalledAppURL("https://accounts.google.com/o/oauth2/auth", GOOGLE_CLIENT_ID, GetVar(FS->Vars,"OAuthScope"), "urn:ietf:wg:oauth:2.0:oob", &URL);

	printf("CUT AND PASTE THE FOLLOWING URL INTO A WEBBROWSER:\n\n %s\n\n",URL);
	printf("Login and/or grant access, then cut and past the access code back to this program.\n\nAccess Code: ");
	fflush(NULL);

	Tempstr=SetStrLen(Tempstr,1024);
	read(0,Tempstr,1024);
	StripTrailingWhitespace(Tempstr);

	OAuthInstalledAppGetAccessToken("https://accounts.google.com/o/oauth2/token", GOOGLE_CLIENT_ID, GOOGLE_CLIENT_SECRET, Tempstr, "urn:ietf:wg:oauth:2.0:oob", &FS->Passwd, &RefreshToken);

	SetVar(FS->Vars,"OAuthRefreshToken",RefreshToken);
	printf("ACCESS:  %s\n",FS->Passwd);
	printf("REFRESH: %s\n",RefreshToken);
}


FS->Settings |= FS_OAUTH;
ConfigFileSaveFileStore(FS);


DestroyString(RefreshToken);
DestroyString(Tempstr);
DestroyString(URL);

return(TRUE);
}



int GoogleAuth(TFileStore *FS, char **AuthToken)
{
STREAM *S;
char *Tempstr=NULL;
int result=FALSE;

HTTPSetFlags(HTTP_DEBUG);
Tempstr=FormatStr(Tempstr,"https://www.google.com/accounts/ClientLogin?accountType=GOOGLE&Email=%s&Passwd=%s&service=%s&source=colum-autoget-0.1",FS->Logon,FS->Passwd,GetVar(FS->Vars,"ClientLoginScope"));

S=HTTPMethod("POST",Tempstr,"","","","",0);

if (S)
{
  Tempstr=STREAMReadLine(Tempstr,S);
  while (Tempstr)
  {
    StripTrailingWhitespace(Tempstr);
printf("GA: %s\n",Tempstr);
    if (strncmp(Tempstr,"Auth=",5)==0) 
		{
				*AuthToken=MCopyStr(*AuthToken,"GoogleLogin auth=",Tempstr+5,NULL);
				result=TRUE;
		}
    Tempstr=STREAMReadLine(Tempstr,S);
  }
STREAMClose(S);
}

DestroyString(Tempstr);

return(result);
}



HTTPInfoStruct *GDataConnect(TFileStore *FS, char *Method, char *URL, char *PostData, ListNode *Headers, int HttpFlags)
{
HTTPInfoStruct *Info;
char *Tempstr=NULL, *ptr;
ListNode *Curr;

	Info=(HTTPInfoStruct *) FS->Extra;

	if (Info)
	{
		ptr=ParseURL(URL, &Tempstr, &Info->Host, &Info->Port, NULL, NULL, NULL, NULL);
		Info->Doc=MCopyStr(Info->Doc,URL,NULL);
		Info->Method=CopyStr(Info->Method,Method);
	}
	else
	{
		Info=HTTPInfoFromURL(Method, URL);
		Info->Doc=MCopyStr(Info->Doc,URL,NULL);
		Info->Flags |= HTTP_SSL;
		Info->Flags |= HttpFlags;
	}

	Info->ResponseCode=CopyStr(Info->ResponseCode,"");
	ListClear(Info->CustomSendHeaders,DestroyString);
	Tempstr=CopyStr(Tempstr,GetVar(FS->Vars,"GData-Version"));
	SetVar(Info->CustomSendHeaders,"GData-Version", Tempstr);

	SetVar(Info->CustomSendHeaders,"Authorization", GetVar(FS->Vars,"AuthToken"));

	fflush(NULL);

	Curr=ListGetNext(Headers);
	while (Curr)
	{
		if (strcasecmp(Curr->Tag,"content-type")==0) Info->PostContentType=CopyStr(Info->PostContentType,(char *) Curr->Item);
		else SetVar(Info->CustomSendHeaders,Curr->Tag, (char *) Curr->Item);
		Curr=ListGetNext(Curr);
	}

	Info->PostData=CopyStr(Info->PostData,PostData);
	Info->PostContentLength=StrLen(PostData);
	
	HTTPTransact(Info);

	if (
				(FS->Settings & FS_OAUTH) &&
				(
				(strcmp(Info->ResponseCode,"401")==0) ||
				(strcmp(Info->ResponseCode,"403")==0) 
				)
	)
  {
		sleep(2);
    GoogleOAuth(FS);

    Tempstr=MCopyStr(Tempstr, "Bearer ", FS->Passwd, NULL);
    SetVar(FS->Vars,"AuthToken",Tempstr);
		Info=GDataConnect(FS, Method, URL, PostData, Headers, HttpFlags);
  }

	if ((strcmp(Method,"POST")==0) && Info->S) Info->S->Flags |= SF_WRONLY;


	DestroyString(Tempstr);

	return(Info);
}


void GDataParseContent(TFileInfo *FI,char *TagData)
{
char *Name=NULL, *Value=NULL, *ptr;

ptr=GetNameValuePair(TagData, " ", "=", &Name, &Value);
while (ptr)
{
	if (strcmp(Name,"src")==0) SetVar(FI->Vars,"content-path",Value);
	if ((strcmp(Name,"type")==0) && (! StrLen(FI->MediaType))) FI->MediaType=CopyStr(FI->MediaType,Value);
	ptr=GetNameValuePair(ptr, " ", "=", &Name, &Value);
}

DestroyString(Name);
DestroyString(Value);
}


void GDataParseCategory(TFileInfo *FI,char *TagData)
{
char *Name=NULL, *Value=NULL, *ptr;

ptr=GetNameValuePair(TagData, " ", "=", &Name, &Value);
while (ptr)
{
	//if (strcmp(Name,"src")==0) FI->Path=CopyStr(FI->Path,Value);
	if (strcmp(Name,"label")==0) 
	{
		if (strcmp(Value,"webpage")==0) FI->MediaType=CopyStr(FI->MediaType,Value);
		if (strcmp(Value,"folder")==0) 
		{
			FI->MediaType=CopyStr(FI->MediaType,Value);
			FI->Type=FTYPE_DIR;
		}
	}

	ptr=GetNameValuePair(ptr, " ", "=", &Name, &Value);
}


DestroyString(Name);
DestroyString(Value);
}

void GDataParseLink(char *TagData, char **Type, char **URL)
{
char *Name=NULL, *Value=NULL, *ptr;

ptr=GetNameValuePair(TagData, " ", "=", &Name, &Value);
while (ptr)
{
	if (strcmp(Name,"rel")==0) *Type=CopyStr(*Type,Value);
	if (strcmp(Name,"href")==0) *URL=CopyStr(*URL,Value);

	ptr=GetNameValuePair(ptr, " ", "=", &Name, &Value);
}

DestroyString(Name);
DestroyString(Value);
}

void GDataParseFileInfoLink(TFileInfo *FI,char *TagData)
{
char *Type=NULL, *URL=NULL, *ptr;

GDataParseLink(TagData, &Type, &URL);

if (StrLen(URL))
{
	ptr=strrchr(URL,'\'');
	if (ptr) *ptr='\0';
	if (strcasecmp(Type,"edit")==0) FI->EditPath=CopyStr(FI->EditPath,URL);
	else SetVar(FI->Vars,Type,URL);
}

DestroyString(Type);
DestroyString(URL);
}



TFileInfo *GDataReadFileEntry(char **XML)
{
char *Token=NULL, *TagName=NULL, *TagData=NULL, *ptr;
char *Tags[]={"title","content","category","id","link","published","updated","/entry","gd:quotaBytesUsed","docs:md5checksum",NULL};
typedef enum {GDATA_TITLE,GDATA_CONTENT,GDATA_CATEGORY,GDATA_ID,GDATA_LINK,GDATA_PUBLISHED,GDATA_UPDATED,GDATA_ENTRY_END,GDATA_BYTES_USED,GDATA_MD5};
TFileInfo *FI=NULL;
int result;

		FI=FileInfoCreate("", "", 0, 0, 0);
		*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
		while (*XML)
		{
			ptr=TagData+StrLen(TagData)-1;
			*ptr='\0';
			result=MatchTokenFromList(TagName,Tags,0);
			if (result==GDATA_ENTRY_END) break;
			switch (result)
			{
				case GDATA_TITLE: 
					*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
					FI->Name=CopyStr(FI->Name,TagData);
				break;

				case GDATA_CONTENT:
					if (FI) GDataParseContent(FI,TagData);
				break;

				case GDATA_CATEGORY:
					if (FI) GDataParseCategory(FI,TagData);
				break;

				case GDATA_LINK:
					if (FI) GDataParseFileInfoLink(FI,TagData);
				break;


				case GDATA_ID:
					*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
					FI->Path=CopyStr(FI->Path,TagData);
					ptr=GetBasename(FI->Path);
				break;

				case GDATA_BYTES_USED:
					*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
					FI->Size=atoi(TagData);
				break;

				case GDATA_MD5:
					*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
					SetVar(FI->Vars,"md5",TagData);
				break;


				case GDATA_UPDATED:
					*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
					FI->Mtime=DateStrToSecs("%Y-%m-%dT%H:%M:%S.",TagData,NULL);
				break;

			}

		*XML=XMLGetTag(*XML,NULL,&TagName,&TagData);
		}

ptr=GetVar(FI->Vars,"content-path");
if ((FI->Type==FTYPE_DIR))
{
 FI->Path=CopyStr(FI->Path,ptr);
}

DestroyString(TagName);
DestroyString(TagData);
DestroyString(Token);

return(FI);
}


/*For some gdata APIs we only get a few entries fed to us at a time, we have to follow a 'next' link */
/*to get the rest, so this is a partial feed*/
void GDataLoadPartialDir(TFileStore *FS, char *URL, char *InPattern, ListNode *Items, char **Next)
{
int result;
char *Tempstr=NULL, *XML=NULL;
char *TagName=NULL, *TagData=NULL, *ptr;
TFileInfo *FI;
HTTPInfoStruct *Info;

	Tempstr=CopyStr(Tempstr,GetVar(FS->Vars,"GDataVersion"));
	Info=GDataConnect(FS, "GET",URL, "",NULL,0);


	if (Info && Info->S)
	{
		Tempstr=STREAMReadLine(Tempstr,Info->S);
		while (Tempstr)
		{
			XML=CatStr(XML,Tempstr);
			Tempstr=STREAMReadLine(Tempstr,Info->S);
		}


	//Can safely blank 'Next' now, even if it's the same object as 'URL'
	*Next=CopyStr(*Next,"");

	ptr=XMLGetTag(XML,NULL,&TagName,&TagData);
	while (ptr)
	{
		if (strcmp(TagName,"entry")==0) 
		{
				FI=GDataReadFileEntry(&ptr);
				ListAddNamedItem(Items,FI->Name,FI);
		}
		else if (strcmp(TagName,"link")==0)
		{
			GDataParseLink(TagData, &TagName, &Tempstr);
			if (strcmp(TagName,"next")==0) *Next=CopyStr(*Next,Tempstr);
		}

		ptr=XMLGetTag(ptr,NULL,&TagName,&TagData);
	}
	}

DestroyString(TagName);
DestroyString(TagData);
DestroyString(Tempstr);
DestroyString(XML);
}


int GDataLoadDir(TFileStore *FS, char *URL, char *InPattern, ListNode *Items, int Flags)
{
char *Next=NULL;

Next=CopyStr(Next,URL);

while (StrLen(Next))
{
	GDataLoadPartialDir(FS, Next, InPattern, Items, &Next);
}

DestroyString(Next);

return(TRUE);
}


int GDataDeleteFile(TFileStore *FS, TFileInfo *FI)
{
char *Tempstr=NULL;
HTTPInfoStruct *Info;
ListNode *Headers;
int result=FALSE;

Headers=ListCreate();
SetVar(Headers,"If-Match","*");
Tempstr=MCopyStr(Tempstr,FI->Path,"?delete=true",NULL);
Info=GDataConnect(FS,"DELETE", Tempstr, NULL, Headers,0);
if (Info->ResponseCode && (*Info->ResponseCode=='2')) result=TRUE;

Tempstr=STREAMReadLine(Tempstr,Info->S);
while (Tempstr)
{
Tempstr=STREAMReadLine(Tempstr,Info->S);
}

ListDestroy(Headers,DestroyString);
HTTPInfoDestroy(Info);
DestroyString(Tempstr);

return(result);
}



