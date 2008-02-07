#include "relational.h"
#include <stdio.h>

typedef struct _DBProvider
{
	int (*Connect)(const char *database, const char *user, const char *password);
	void (*Disconnect)();
	RowSet *(*Select)(const char* str);
	int (*UpdateCommand)(const char* str);
	void (*Commit)();
	GList *(*GetDatabaseList)(const char *user, const char *password);
	int (*DeleteDatabase)(const char *database, const char *user, const char *password);

	const char *name;
	const char *desc;
	int HasUserDetails;
	const char *database;
	const char *username;
	const char *password;
} DBProvider;

typedef enum _DBProviderType {
	INVALID_PROVIDER = -1,
#if USE_PYTHON
	PYTHON_SQLITE, PYTHON_MYSQL, PYTHON_POSTGRE,
#endif
	SQLITE
} DBProviderType;
#define NUM_PROVIDERS (int)(SQLITE + 1)

extern DBProviderType dbProviderType;
extern DBProvider providers[NUM_PROVIDERS];
extern DBProvider* GetDBProvider(DBProviderType dbType);
extern const char *TestDB(DBProviderType dbType);
DBProvider *ConnectToDB(DBProviderType dbType);
void SetDBType(const char *type);
void SetDBSettings(DBProviderType dbType, const char *database, const char *user, const char *password);
void RelationalSaveSettings(FILE *pf);
void SetDBParam(const char *db, const char *key, const char *value);
int RunQueryValue(DBProvider *pdb, const char *query);
int CreateDatabase(DBProvider *pdb);
