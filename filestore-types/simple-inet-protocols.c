#include "simple-inet-protocols.h"

//Responses must start with a 3-letter digit. If we don't even get that (so there's silence) we
//return FALSE here, and the app exits.
int InetReadResponse(STREAM *S, char **ResponseCode, char **Verbiage, int RequiredResponse)
{
char *Tempstr=NULL;
int result=FALSE;

Tempstr=STREAMReadLine(Tempstr,S);
if (! StrLen(Tempstr)) return(FALSE);

if (Settings.Flags & FLAG_VERBOSE) printf("%s\n",Tempstr);

if (StrLen(Tempstr) > 3)
{
*ResponseCode=CopyStrLen(*ResponseCode,Tempstr,3);
switch (**ResponseCode)
{
case '1': if (RequiredResponse & INET_CONTINUE) result=TRUE; break;
case '2': if (RequiredResponse & INET_OKAY) result=TRUE; break;
case '3': if (RequiredResponse & INET_MORE) result=TRUE; break;

//POP3 style
case '+': if ((strcmp(*ResponseCode,"+OK")==0) && (RequiredResponse & INET_OKAY)) result=TRUE; break;
}

if (Verbiage) *Verbiage=CopyStr(*Verbiage,Tempstr+4);
while ( (Tempstr[3]=='-') || (isspace(Tempstr[0])) )
{
	StripCRLF(Tempstr);

	if (Verbiage) 
	{
		if (Tempstr[3]=='-') *Verbiage=MCatStr(*Verbiage,"\n",Tempstr+4,NULL);
		else *Verbiage=MCatStr(*Verbiage,"\n",Tempstr,NULL);
	}
	Tempstr=STREAMReadLine(Tempstr,S);
}
}

DestroyString(Tempstr);

return(result);
}
