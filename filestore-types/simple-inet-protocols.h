#ifndef INET_H
#define INET_H

#include "includes.h"


#define INET_CONTINUE 1
#define INET_OKAY 2
#define INET_MORE 4
#define INET_GOOD 7


int InetReadResponse(STREAM *S, char **ResponseCode, char **Verbiage, int RequiredResponse);

#endif
