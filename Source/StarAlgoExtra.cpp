#include "stdafx.h"
#include "StarAlgoExtra.h"

int LoadConfigInt(const char *pszKey, const char *pszItem, const int iDefault)
{
	return GetPrivateProfileIntA(pszKey, pszItem, iDefault, configPath.c_str());
}
