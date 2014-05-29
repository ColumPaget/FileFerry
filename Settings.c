#include "Settings.h"

char *FSSettings[]={"compression","encoding","symlink",NULL};
int FSSettingsFlags[]={FS_TRANSFER_TYPES, FS_COMPRESSION,FS_SYMLINKS};
char *AppSettings[]={"integrity","comparetype","verbose","filemode","dirmode","exemode", NULL};
typedef enum {AS_INTEGRITY, AS_COMPARETYPE, AS_VERBOSE, AS_FILEMODE, AS_DIRMODE, AS_EXEMODE} TAppSettings;

void DisplayFilestoreSetting(TFileStore *FS, char *Name)
{
int i, Flag;

	for (i=0; FSSettings[i] !=NULL; i++)
	{
		if ( (StrLen(Name)==0) || (strcmp(Name,FSSettings[i])==0) )
		{
			Flag=FSSettingsFlags[i];
			if (! (FS->Features & Flag)) printf("%s: Not supported by filestore\n",FSSettings[i]);
			else if (FS->Settings & Flag) printf("%s: on\n",FSSettings[i]);
			else printf("%s: off\n",FSSettings[i]);
		}
	}
}


int ChangeFilestoreSetting(TFileStore *FS, char *Name, char *Value)
{
int i, Flag;

i=MatchTokenFromList(Name,FSSettings,0);
if (i==-1) return(FALSE);

Flag=FSSettingsFlags[i];

if (! (FS->Features & Flag)) return(FALSE);

if (strcasecmp(Value,"on")==0) FS->Settings |= Flag;
else if (strcasecmp(Value,"off")==0) FS->Settings &= ~Flag;
else if (strcasecmp(Value,"yes")==0) FS->Settings |= Flag;
else if (strcasecmp(Value,"no")==0) FS->Settings &= ~Flag;
else 
{
	FS->Settings |= Flag;
	SetVar(FS->Vars,FSSettings[i],Value);
}

if (FS->Flags & FS_RESTART_SETTINGS)
{
  FS->Close(FS);
  FS->Open(FS);
}

return(TRUE);
}


void DisplaySettings(TFileStore *FS)
{
int i;

for (i=0; FSSettings[i] != NULL; i++)
{
	DisplayFilestoreSetting(FS, FSSettings[i]);
}


}


void ProcessSetting(TFileStore *FS, char *Args)
{
char *ptr, *Token=NULL;
int val;

ptr=GetToken(Args," ",&Token,0);

if (StrLen(ptr)) 
{
	val=MatchTokenFromList(Token,FSSettings,0);
	if (val > -1)
	{
		ChangeFilestoreSetting(FS, Token, ptr);
		DisplayFilestoreSetting(FS, Token);
	}
	else 
	{
		val=MatchTokenFromList(Token,AppSettings,0);
		if (val > -1)
		{
		switch (val)
		{
		case AS_VERBOSE:
			ptr=GetToken(ptr," ",&Token,0);
			if (strcmp(Token,"on")==0) Settings.Flags |= FLAG_VERBOSE;
			else if (strcmp(Token,"off")==0) Settings.Flags &= ~FLAG_VERBOSE;
			printf("OKAY: Verbose output set to %s\n",Token);
		break;

		case AS_INTEGRITY:
			ptr=GetToken(ptr," ",&Token,0);
			if (strcmp(Token,"on")==0) Settings.Flags |= FLAG_INTEGRITY_CHECK;
			else if (strcmp(Token,"off")==0)Settings.Flags &= ~FLAG_INTEGRITY_CHECK;
			else if (GetCompareLevelTypes(Token))
			{
				Settings.Flags |= FLAG_INTEGRITY_CHECK;
				SetVar(Settings.Vars,"Integrity",Token);
				printf("OKAY: Integrity checking set to %s\n",Token);
			}
			else printf("ERROR: Unknown integrity check type '%s'\n",Token);
		break;

		case AS_COMPARETYPE:
			ptr=GetToken(ptr," ",&Token,0);
			if (GetCompareLevelTypes(Token))
			{
				SetVar(Settings.Vars,"CompareLevel",Token);
				printf("OKAY: File comparison function set to %s\n",Token);
			}
			else printf("ERROR: Unknown comparison type '%s'\n",Token);
		break;

		default:
			SetVar(FS->Vars,Token,ptr);
			printf("OKAY: %s %s\n",Token,ptr);
		break;
		}
		}
		else printf("ERROR: Unknown Setting: '%s'\n",Token);
	}
}
else
{
	if (! StrLen(Token)) DisplaySettings(FS);
	else printf("ERROR: No argument for setting.\n");
}



DestroyString(Token);
}


void SetDefaultHashType(char *HashTypes)
{
char *Token=NULL, *ptr;

if (! HashTypes) return;
if (strstr(HashTypes,"md5 ")) Settings.HashType=CopyStr(Settings.HashType,"md5");
else
{
	ptr=GetToken(HashTypes," ",&Token,0);
	Settings.HashType=CopyStr(Settings.HashType,Token);
}
DestroyString(Token);
}
