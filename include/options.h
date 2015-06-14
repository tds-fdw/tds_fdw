
#ifndef OPTIONS_H
#define OPTIONS_H

#include "postgres.h"

/* valid options follow this format */

typedef struct TdsFdwOption
{
	const char *optname;
	Oid optcontext;
} TdsFdwOption;

/* option values will be put here */

typedef struct TdsFdwOptionSet
{
	char *servername;
	char *language;
	char *character_set;
	int port;
	char *database;
	int dbuse;
	char* tds_version;
	char* msg_handler;
	char *username;
	char *password;
	char *table_database;
	char *query;
	char *table;
	char* row_estimate_method;
} TdsFdwOptionSet;

void tdsValidateOptions(List *options_list, Oid context, TdsFdwOptionSet* option_set);
void tdsGetForeignTableOptionsFromCatalog(Oid foreigntableid, TdsFdwOptionSet* option_set);
void tdsValidateOptionSet(TdsFdwOptionSet* option_set);

#endif
