#pragma once

#define	MAX_ALIAS_NAME	32
#define MAX_COMMAND_LENGTH 1024

struct cmdalias_t
{
	cmdalias_t	*next;
	char		name[MAX_ALIAS_NAME];
	char		*value;
};