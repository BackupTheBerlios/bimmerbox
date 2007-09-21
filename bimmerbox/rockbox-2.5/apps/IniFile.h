

////////// MODIFY CODE IN THIS BLOCK TO ADD A PARAM
// Add your global variables for settings here.

typedef struct {
	int iDebugLevel;
	int iIntroTime;
	int iSkipDelay;
	int iScrollSpeed;
	int iScrollDelay;
	int iDisplayWidth;
	int iAnnouncePeriod;
	int iIbusIdleTime;
	int iTicksPerSecond;
	int iLeadingChar;
	bool bAlternateChanger;
	bool bAlternateStatus;
//	bool bAlternateStart;
	bool bUsePhoneButtons;
	bool bSkipMagazine;
	bool bMuteHack;
	bool bMuteOnSkip;
	bool bDisplayRAD;
	bool bDisplayMID;
	bool bDisplayOBC;
	bool bDisplayIRIS;
	bool bDisplayNAV;
	bool bDisplayPlaylistName;
	bool bLowCluster;
	bool bPollsFirst;
	bool bStatusFirst;
	char *sTitleLine;
} ConfigParameters;

extern ConfigParameters gConf;

// Add Entry to table for each setting in IniFile.c
//      NOTE: Names must be uppercase
////////// END BLOCK


//
// Call this function to read the ini file.
//
void loadSetting(void);


