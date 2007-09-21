/*

	Ini file processor. 
	
	  Comments welcome, ajkle35467 at hotmail.com (Steve Lavelli)
	  Not copyrighted. Forget about it, use it as you want!
*/

//#include "stdafx.h"
#include "system.h"
#include "kernel.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "file.h"
#include "atoi.h"
#include "IniFile.h"
#include "ctype.h"

/*
#define _U 01
#define _L 02
extern char _ctype_[];
#define islower(c) ((_ctype_+1)[(unsigned)(c)]&_L)
#define toupper(c) \
	__extension__ (( int __x = (c); islower(__x) ? (__x-'a' +'A') : __X;))
*/


// Name of the ini file (include path)
#define INI_FILE_NAME "/bmw/ibus.ini"

// Max size of a line in the ini file (other character are tossed)
#define MAX_LINE_LENGTH 30
   
#define MAX_DYNAMIC_MEM		(MAX_LINE_LENGTH*10)

char local_heap[MAX_DYNAMIC_MEM];
int heap_used = 0;

char *local_malloc(int size) {
	int m_size = size > MAX_LINE_LENGTH+1 ? MAX_LINE_LENGTH+1: size;
	char *result = local_heap + heap_used;
	heap_used+=m_size;
	return result;
}

// Types we can accept for params in ini file
#define INI_TYPE_EOL  0		// marker for last param
#define INI_TYPE_BOOL 1		// bool type: YES/NO, TRUE/FALSE, 1/0, etc
#define INI_TYPE_INT  2		// int type: int number 
#define INI_TYPE_STR  3		// String type: Text 


typedef struct {
	char * cpName;
	int iType;
	void *vpValue;
	char * cpDefault;
} iniparams;


////////// MODIFY CODE IN THIS BLOCK TO ADD A PARAM
// Add your global variables for settings to IniFile.h

ConfigParameters gConf;

// Add Entry in this table for each setting
//      NOTE: Names must be uppercase
const iniparams iniparm[] = {
			{"DEBUGLEVEL", INI_TYPE_INT, &gConf.iDebugLevel,"0"},
			{"USEPHONEBUTTONS", INI_TYPE_BOOL, &gConf.bUsePhoneButtons,"1"},
			{"ALTERNATECHANGER", INI_TYPE_BOOL, &gConf.bAlternateChanger, "0"},
			{"ALTERNATESTATUS", INI_TYPE_BOOL, &gConf.bAlternateStatus, "0"},
//			{"ALTERNATESTART", INI_TYPE_BOOL, &gConf.bAlternateStart, "0"},
			{"MUTEHACK", INI_TYPE_BOOL, &gConf.bMuteHack, "0"},
			{"MUTEONSKIP", INI_TYPE_BOOL, &gConf.bMuteOnSkip, "0"},
			{"INTROTIME", INI_TYPE_INT, &gConf.iIntroTime, "10"},
			{"DOUBLECLICKTIME", INI_TYPE_INT, &gConf.iDoubleClickTime, "4"},
			{"SKIPDELAY", INI_TYPE_INT, &gConf.iSkipDelay, "2"},
			{"IBUSIDLETIME", INI_TYPE_INT, &gConf.iIbusIdleTime, "300"},
			{"ANNOUNCEPERIOD", INI_TYPE_INT, &gConf.iAnnouncePeriod, "10"},
			{"LEADINGCHAR", INI_TYPE_INT, &gConf.iLeadingChar, "46"},
			{"DISPLAYWIDTH", INI_TYPE_INT, &gConf.iDisplayWidth, "11"},
			{"DISPLAYRAD", INI_TYPE_BOOL, &gConf.bDisplayRAD, "1"},
			{"DISPLAYMID", INI_TYPE_BOOL, &gConf.bDisplayMID, "0"},
			{"DISPLAYOBC", INI_TYPE_BOOL, &gConf.bDisplayOBC, "0"},
			{"DISPLAYIRIS", INI_TYPE_BOOL, &gConf.bDisplayIRIS, "0"},
			{"DISPLAYNAV", INI_TYPE_BOOL, &gConf.bDisplayNAV, "0"},
			{"DISPLAYPLAYLISTNAME", INI_TYPE_BOOL, &gConf.bDisplayPlaylistName, "0"},
			{"SKIPMAGAZINE", INI_TYPE_BOOL, &gConf.bSkipMagazine, "0"},
			{"SCROLLSPEED", INI_TYPE_INT, &gConf.iScrollSpeed, "4"},
			{"SCROLLDELAY", INI_TYPE_INT, &gConf.iScrollDelay, "10"},
			{"TICKSPERSECOND", INI_TYPE_INT, &gConf.iTicksPerSecond, "4"},
			{"LOWCLUSTER", INI_TYPE_BOOL, &gConf.bLowCluster, "0"},
			{"POLLSFIRST", INI_TYPE_BOOL, &gConf.bPollsFirst, "0"},
			{"STATUSFIRST", INI_TYPE_BOOL, &gConf.bStatusFirst, "0"},
			{"TITLELINE", INI_TYPE_STR, &gConf.sTitleLine, "--- Now Playing: ---"},
			{"",INI_TYPE_EOL,NULL,""}};
////////// END BLOCK

// same as strncmp except don't case about case.
int strncmpnocase(const char * cp1,const char *cp2, int iLen) {
	while (iLen-- > 0) {
		if (toupper(cp1[iLen]) != toupper(cp2[iLen])) 
			return -1;
	}
 	return 0;  // match
}


// returns true/false given value in string
bool getBoolValue(const char *cpValue) {
	const char *bools[] = {"0","1","OFF","ON","FALSE","TRUE","NO","YES",""};
	
	int x = 0;
	while (bools[x] != '\0') {
		if (strncmpnocase(bools[x],cpValue,strlen(bools[x])) == 0)
			return (x & 1) == 1;
		x++;
	}
	return false;
}


// returns int given value in string
int getIntValue(const char *cpValue) {

// if atoi not available try (int) strtol(string, (char**) 0,10)
	return atoi(cpValue);
}


// returns Str given value in string
char *getStrValue(const char *cpValue) {
// I bet we could lose some memory here...
// strdup is not define....
//	return (char *)strdup(cpValue);
	int len = strlen(cpValue);
	char *str = local_malloc(len+1);
	strcpy(str, cpValue);

//	return "SORRY NOT WORKING";
	return str;
}


// Set a value 
void setValue(int iType, void * vpValue, const char *cpInput) {
	switch (iType) {
	case INI_TYPE_BOOL:
		*((bool *) vpValue) = getBoolValue(cpInput);
		break;
	case INI_TYPE_INT:
		*((int *) vpValue) = getIntValue(cpInput);
		break;
	case INI_TYPE_STR:
		*((char **) vpValue) = getStrValue(cpInput);
		break;
/*		default: 
			printf("Unknown type %n for %s\n",iType,cpInput);
*/
	}
}


// Set all the params to the default values acording to the table
void resetIniValues(void) {
	int iIdx = 0;
	while (iniparm[iIdx].iType != 0) {
		setValue(iniparm[iIdx].iType, iniparm[iIdx].vpValue, iniparm[iIdx].cpDefault);
		iIdx++;
	}
}


//  Read line removes leading spaces
bool readLine(char * cpBuf, int fp) {
	int iLen = 0;
	char cTempBuf[2];

	while (read(fp, &cTempBuf,1) == 1) {
		if ((cTempBuf[0] == '\x0d') || (cTempBuf[0] == '\x0a') ) {
			cpBuf[iLen] = '\0';
			return true;
		} else {
			if (!((iLen == 0) && (cTempBuf[0] == ' ' || cTempBuf[0] == '\t' )) &&
					iLen < (MAX_LINE_LENGTH - 1)) {
				cpBuf[iLen] = cTempBuf[0];
				iLen++;
			}
		}
	} 
	cpBuf[iLen] = '\0';
	if (iLen != 0)
		return true;
	return false;
}


// Compare to string (using length of first)
//   and if matched move second string to point after match.
bool tokenMatch(char *cpToken, char **cpBuffer) {
	int nTokLen = strlen(cpToken);
	if (strncmpnocase(cpToken,*cpBuffer,nTokLen) == 0) {
		*cpBuffer = &(*cpBuffer)[nTokLen];
		while (**cpBuffer == ' ' || **cpBuffer == '\t'|| **cpBuffer == '=') 
			(*cpBuffer)++;
		return true;
	}
	return false;
}


// Set the params to their defaults, then load them from Ini
void loadSetting() {
	char cLine[MAX_LINE_LENGTH];
	int fIni = open(INI_FILE_NAME,O_RDONLY);
	int iIdx;
	char *cpLinePtr;

	resetIniValues();

	if (fIni > -1) {
		while (readLine(cLine, fIni)) {
			if (!(cLine[0] == '\0' || cLine[0] == ';')) {
				cpLinePtr = cLine;
				iIdx = 0;
				while (iniparm[iIdx].iType != 0) {
					if (tokenMatch(iniparm[iIdx].cpName,&cpLinePtr)) {
						setValue(iniparm[iIdx].iType, iniparm[iIdx].vpValue,cpLinePtr);
						break;
					}
					iIdx++;
				}
			}
		}
		close(fIni);
	}
}

/*
// Test code 
int main(int argc, char* argv[])
{
	loadSetting();
	printf("Hello World!\n");
	return 0;
}
*/
