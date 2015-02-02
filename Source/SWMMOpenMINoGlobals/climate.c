//-----------------------------------------------------------------------------
//   climate.c
//
//   Project: EPA SWMM5
//   Version: 5.1
//   Date:    03/20/10 (Build 5.1.001)
//   Author:  L. Rossman
//
//   Climate related functions.
//-----------------------------------------------------------------------------

#define _CRT_SECURE_NO_DEPRECATE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "headers.h"

//-----------------------------------------------------------------------------
//  Constants
//-----------------------------------------------------------------------------
enum ClimateFileFormats {UNKNOWN_FORMAT,
                         USER_PREPARED,     // SWMM 5's own user format
                         TD3200,            // NCDC TD3200 format
                         DLY0204};          // Canadian DLY02 or DLY04 format
static const int   MAXCLIMATEVARS  = 4;
static const int   MAXDAYSPERMONTH = 32;

// These variables are used when processing climate files.
enum   ClimateVarType {TMIN, TMAX, EVAP, WIND};
static char* ClimateVarWords[] = {"TMIN", "TMAX", "EVAP", "WDMV", NULL};

//-----------------------------------------------------------------------------
//  Shared variables
//-----------------------------------------------------------------------------
// Temperature variables
//static double    Tmin;                 // min. daily temperature (deg F)
//static double    Tmax;                 // max. daily temperature (deg F)
//static double    Trng;                 // 1/2 range of daily temperatures
//static double    Trng1;                // prev. max - current min. temp.
//static double    Tave;                 // average daily temperature (deg F)
//static double    Hrsr;                 // time of min. temp. (hrs)
//static double    Hrss;                 // time of max. temp (hrs) 
//static double    Hrday;                // avg. of min/max temp times
//static double    Dhrdy;                // hrs. between min. & max. temp. times
//static double    Dydif;                // hrs. between max. & min. temp. times
//static DateTime  LastDay;              // date of last day with temp. data
//
//// Evaporation variables
//static DateTime  NextEvapDate;         // next date when evap. rate changes
//static double    NextEvapRate;         // next evaporation rate (user units)
//
//// Climate file variables
//static int      FileFormat;            // file format (see ClimateFileFormats)
//static int      FileYear;              // current year of file data
//static int      FileMonth;             // current month of year of file data
//static int      FileDay;               // current day of month of file data
//static int      FileLastDay;           // last day of current month of file data
//static int      FileElapsedDays;       // number of days read from file
//static double   FileValue[4];          // current day's values of climate data
//static double   FileData[4][32];       // month's worth of daily climate data
//static char     FileLine[MAXLINE+1];   // line from climate data file

//-----------------------------------------------------------------------------
//  External functions (defined in funcs.h)
//-----------------------------------------------------------------------------
//  climate_readParams                 // called by input_parseLine
//  climate_readEvapParams             // called by input_parseLine
//  climate_validate                   // called by project_validate
//  climate_openFile                   // called by runoff_open
//  climate_initState                  // called by project_init
//  climate_setState                   // called by runoff_execute
//  climate_getNextEvap                // called by runoff_getTimeStep

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------
static int  getFileFormat(Project* project);
static void readFileLine(Project* project, int *year, int *month);
static void readUserFileLine(Project* project, int *year, int *month);
static void readTD3200FileLine(Project* project, int *year, int *month);
static void readDLY0204FileLine(Project* project, int *year, int *month);
static void readFileValues(Project* project);
static void setFileValues(int param);
static void setEvap(Project* project, DateTime theDate);
static void setTemp(Project* project, DateTime theDate);
static void setWind(Project* project, DateTime theDate);
static void updateTempTimes(Project* project, int day);
static double getTempEvap(Project* project, int day);
static void updateFileValues(Project* project, DateTime theDate);
static void parseUserFileLine(Project* project);
static void parseTD3200FileLine(Project* project);
static void parseDLY0204FileLine(Project* project);
static void setTD3200FileValues(Project* project, int param);


//=============================================================================

int  climate_readParams(Project* project, char* tok[], int ntoks)
//
//  Input:   tok[] = array of string tokens
//           ntoks = number of tokens
//  Output:  returns error code
//  Purpose: reads climate/temperature parameters from input line of data
//
//  Format of data can be
//    TIMESERIES  name
//    FILE        name
//    WINDSPEED   MONTHLY  v1  v2  ...  v12
//    WINDSPEED   FILE 
//    SNOWMELT    v1  v2  ...  v6
//    ADC         IMPERV/PERV  v1  v2  ...  v10
//
{
    int      i, j, k;
    double   x[6], y;
    DateTime aDate;

    // --- identify keyword
    k = findmatch(tok[0], TempKeyWords);
    if ( k < 0 ) return error_setInpError(ERR_KEYWORD, tok[0]);
    switch (k)
    {
      case 0: // Time series name
        // --- check that time series name exists
        if ( ntoks < 2 ) return error_setInpError(ERR_ITEMS, "");
        i = project_findObject(project, TSERIES, tok[1]);
        if ( i < 0 ) return error_setInpError(ERR_NAME, tok[1]);

        // --- record the time series as being the data source for temperature
		project->Temp.dataSource = TSERIES_TEMP;
		project->Temp.tSeries = i;
		project->Tseries[i].refersTo = TSERIES_TEMP;
        break;

      case 1: // Climate file
        // --- record file as being source of temperature data
        if ( ntoks < 2 ) return error_setInpError(ERR_ITEMS, "");
		project->Temp.dataSource = FILE_TEMP;

        // --- save name and usage mode of external climate file
		project->Fclimate.mode = USE_FILE;
		sstrncpy(project->Fclimate.name, tok[1], MAXFNAME);

        // --- save starting date to read from file if one is provided
		project->Temp.fileStartDate = NO_DATE;
        if ( ntoks > 2 )
        {
            if ( *tok[2] != '*')
            {
                if ( !datetime_strToDate(tok[2], &aDate) )
                    return error_setInpError(ERR_DATETIME, tok[2]);
				project->Temp.fileStartDate = aDate;
            }
        }
        break;

      case 2: // Wind speeds
        // --- check if wind speeds will be supplied from climate file
        if ( strcomp(tok[1], w_FILE) )
        {
			project->Wind.type = FILE_WIND;
        }

        // --- otherwise read 12 monthly avg. wind speed values
        else
        {
            if ( ntoks < 14 ) return error_setInpError(ERR_ITEMS, "");
			project->Wind.type = MONTHLY_WIND;
            for (i=0; i<12; i++)
            {
                if ( !getDouble(tok[i+2], &y) )
                    return error_setInpError(ERR_NUMBER, tok[i+2]);
				project->Wind.aws[i] = y;
            }
        }
        break;

      case 3: // Snowmelt params
        if ( ntoks < 7 ) return error_setInpError(ERR_ITEMS, "");
        for (i=1; i<7; i++)
        {
            if ( !getDouble(tok[i], &x[i-1]) )
                return error_setInpError(ERR_NUMBER, tok[i]);
        }
        // --- convert deg. C to deg. F for snowfall temperature
		if (project->UnitSystem == SI) x[0] = 9. / 5.*x[0] + 32.0;
		project->Snow.snotmp = x[0];
		project->Snow.tipm = x[1];
		project->Snow.rnm = x[2];
		project->Temp.elev = x[3] / UCF(project,LENGTH);
		project->Temp.anglat = x[4];
		project->Temp.dtlong = x[5] / 60.0;
        break;

      case 4:  // Areal Depletion Curve data
        // --- check if data is for impervious or pervious areas
        if ( ntoks < 12 ) return error_setInpError(ERR_ITEMS, "");
        if      ( match(tok[1], w_IMPERV) ) i = 0;
        else if ( match(tok[1], w_PERV)   ) i = 1;
        else return error_setInpError(ERR_KEYWORD, tok[1]);

        // --- read 10 fractional values
        for (j=0; j<10; j++)
        {
            if ( !getDouble(tok[j+2], &y) || y < 0.0 || y > 1.0 ) 
                return error_setInpError(ERR_NUMBER, tok[j+2]);
			project->Snow.adc[i][j] = y;
        }
        break;
    }
    return 0;
}

//=============================================================================

int climate_readEvapParams(Project* project, char* tok[], int ntoks)
//
//  Input:   tok[] = array of string tokens
//           ntoks = number of tokens
//  Output:  returns error code
//  Purpose: reads evaporation parameters from input line of data.
//
//  Data formats are:
//    CONSTANT  value
//    MONTHLY   v1 ... v12
//    TIMESERIES name
//    TEMPERATURE 
//    FILE      (v1 ... v12)
//    RECOVERY   name
//    DRY_ONLY   YES/NO
//
{
    int i, k;
    double x;

    // --- find keyword indicating what form the evaporation data is in
    k = findmatch(tok[0], EvapTypeWords);
    if ( k < 0 ) return error_setInpError(ERR_KEYWORD, tok[0]);

    // --- check for RECOVERY pattern data
    if ( k == RECOVERY )
    {
        if ( ntoks < 2 ) return error_setInpError(ERR_ITEMS, "");
		i = project_findObject(project, TIMEPATTERN, tok[1]);
        if ( i < 0 ) return error_setInpError(ERR_NAME, tok[1]);
		project->Evap.recoveryPattern = i;
        return 0;
    }

    // --- check for no evaporation in wet periods
    if ( k == DRYONLY )
    {
        if ( ntoks < 2 ) return error_setInpError(ERR_ITEMS, "");
		if (strcomp(tok[1], w_NO))  project->Evap.dryOnly = FALSE;
		else if (strcomp(tok[1], w_YES)) project->Evap.dryOnly = TRUE;
        else return error_setInpError(ERR_KEYWORD, tok[1]);
        return 0;
    }

    // --- process data depending on its form
	project->Evap.type = k;
    if ( k != TEMPERATURE_EVAP && ntoks < 2 )
        return error_setInpError(ERR_ITEMS, "");
    switch ( k )
    {
      case CONSTANT_EVAP:
        // --- for constant evap., fill monthly avg. values with same number
        if ( !getDouble(tok[1], &x) )
            return error_setInpError(ERR_NUMBER, tok[1]);
		for (i = 0; i<12; i++) project->Evap.monthlyEvap[i] = x;
        break;

      case MONTHLY_EVAP:
        // --- for monthly evap., read a value for each month of year
        if ( ntoks < 13 ) return error_setInpError(ERR_ITEMS, "");
        for ( i=0; i<12; i++)
			if (!getDouble(tok[i + 1], &project->Evap.monthlyEvap[i]))
                return error_setInpError(ERR_NUMBER, tok[i+1]);
        break;           

      case TIMESERIES_EVAP:
        // --- for time series evap., read name of time series
		  i = project_findObject(project, TSERIES, tok[1]);
        if ( i < 0 ) return error_setInpError(ERR_NAME, tok[1]);
		project->Evap.tSeries = i;
		project->Tseries[i].refersTo = TIMESERIES_EVAP;
        break;

      case FILE_EVAP:
        // --- for evap. from climate file, read monthly pan coeffs.
        //     if they are provided (default values are 1.0)
        if ( ntoks > 1 )
        {
            if ( ntoks < 13 ) return error_setInpError(ERR_ITEMS, "");
            for (i=0; i<12; i++)
            {
				if (!getDouble(tok[i + 1], &project->Evap.panCoeff[i]))
                    return error_setInpError(ERR_NUMBER, tok[i+1]);
            }
        }
        break;
    }
    return 0;
}

//=============================================================================

void climate_validate(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: validates climatological variables
//
{
    double    a, z, pa;

    // --- check if climate file was specified when used 
    //     to supply wind speed or evap rates
	if (project->Wind.type == FILE_WIND || project->Evap.type == FILE_EVAP ||
		project->Evap.type == TEMPERATURE_EVAP)
    {
		if (project->Fclimate.mode == NO_FILE)
        {
            report_writeErrorMsg(project,ERR_NO_CLIMATE_FILE, "");
        }
    }

    // --- snow melt parameters tipm & rnm must be fractions
	if (project->Snow.tipm < 0.0 ||
		project->Snow.tipm > 1.0 ||
		project->Snow.rnm  < 0.0 ||
		project->Snow.rnm  > 1.0) report_writeErrorMsg(project,ERR_SNOWMELT_PARAMS, "");

    // --- latitude should be between -90 & 90 degrees
	a = project->Temp.anglat;
    if ( a <= -89.99 ||
         a >= 89.99  ) report_writeErrorMsg(project,ERR_SNOWMELT_PARAMS, "");
	else project->Temp.tanAnglat = tan(a * PI / 180.0);

    // --- compute psychrometric constant
	z = project->Temp.elev / 1000.0;
    if ( z <= 0.0 ) pa = 29.9;
    else  pa = 29.9 - 1.02*z + 0.0032*pow(z, 2.4); // atmos. pressure
	project->Temp.gamma = 0.000359 * pa;
}

//=============================================================================

void climate_openFile(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: opens a climate file and reads in first set of values.
//
{
    int i, m, y;

    // --- open the file
	if ((project->Fclimate.file = fopen(project->Fclimate.name, "rt")) == NULL)
    {
		report_writeErrorMsg(project,ERR_CLIMATE_FILE_OPEN, project->Fclimate.name);
        return;
    }

    // --- initialize values of file's climate variables
    //     (Temp.ta was previously initialized in project.c)
	project->FileValue[TMIN] = project->Temp.ta;
	project->FileValue[TMAX] = project->Temp.ta;
	project->FileValue[EVAP] = 0.0;
	project->FileValue[WIND] = 0.0;

    // --- find climate file's format
	project->FileFormat = getFileFormat(project);
	if (project->FileFormat == UNKNOWN_FORMAT)
    {
		report_writeErrorMsg(project,ERR_CLIMATE_FILE_READ, project->Fclimate.name);
        return;
    }

    // --- position file to begin reading climate file at either user-specified
    //     month/year or at start of simulation period.
	rewind(project->Fclimate.file);
	strcpy(project->FileLine, "");
	if (project->Temp.fileStartDate == NO_DATE)
		datetime_decodeDate(project->StartDate, &project->FileYear, &project->FileMonth, &project->FileDay);
    else
		datetime_decodeDate(project->Temp.fileStartDate, &project->FileYear, &project->FileMonth, &project->FileDay);
	while (!feof(project->Fclimate.file))
    {
		strcpy(project->FileLine, "");
        readFileLine(project,&y, &m);
		if (y == project->FileYear && m == project->FileMonth) break;
    }
	if (feof(project->Fclimate.file))
    {
		report_writeErrorMsg(project,ERR_CLIMATE_END_OF_FILE, project->Fclimate.name);
        return;
    }
    
    // --- initialize file dates and current climate variable values 
	if (!project->ErrorCode)
    {
		project->FileElapsedDays = 0;
		project->FileLastDay = datetime_daysPerMonth(project->FileYear, project->FileMonth);
        readFileValues(project);
        for (i=TMIN; i<=WIND; i++)
        {
			if (project->FileData[i][project->FileDay] == MISSING) continue;
			project->FileValue[i] = project->FileData[i][project->FileDay];
        }
    }
}

//=============================================================================

void climate_initState(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: initializes climate state variables.
//
{
	project->LastDay = NO_DATE;
	project->Temp.tmax = MISSING;
	project->Snow.removed = 0.0;

////  Following code added for release 5.0.019  ////
	project->NextEvapDate = project->StartDate;
	project->NextEvapRate = 0.0;
	if (project->Evap.type == TIMESERIES_EVAP && project->Evap.tSeries >= 0)
		project->NextEvapRate = table_intervalLookup(&project->Tseries[project->Evap.tSeries],
		project->StartDate - 1.0);
////////////////////////////////////////////////////
}

//=============================================================================

void climate_setState(Project* project, DateTime theDate)
//
//  Input:   theDate = simulation date
//  Output:  none
//  Purpose: sets climate variables for current date.
//
{
	if (project->Fclimate.mode == USE_FILE) updateFileValues(project, theDate);
	if (project->Temp.dataSource != NO_TEMP) setTemp(project, theDate);
	setEvap(project, theDate); 
	setWind(project, theDate);
}

//=============================================================================

DateTime climate_getNextEvap(Project* project, DateTime days)
//
//  Input:   days = current simulation date
//  Output:  returns date (in whole days) when evaporation rate next changes
//  Purpose: finds date for next change in evaporation.
//
{
    int    yr, mon, day, k;
    double d, e;

    days = floor(days);
	switch (project->Evap.type)
    {
      case CONSTANT_EVAP:
        return days + 365.;

      case MONTHLY_EVAP:
        datetime_decodeDate(days, &yr, &mon, &day);
        if ( mon == 12 )
        {
            mon = 1;
            yr++;
        }
        else mon++;
        return datetime_encodeDate(yr, mon, 1);

      case TIMESERIES_EVAP:
		  if (project->NextEvapDate > days) return project->NextEvapDate;
		  k = project->Evap.tSeries;
        if ( k >= 0 )
        {    
			while (table_getNextEntry(&project->Tseries[k], &d, &e) &&
				d <= project->EndDateTime)
            {
                if ( d > days )
                {
					project->NextEvapDate = d;
					project->NextEvapRate = e;
                    return d;
                }
            }
        }
        return days + 365.;

      case FILE_EVAP:
        return days + 1.0;

      default: return days + 365.;
    }
}

//=============================================================================

void updateFileValues(Project* project, DateTime theDate)
//
//  Input:   theDate = current simulation date
//  Output:  none
//  Purpose: updates daily climate variables for new day or reads in
//           another month worth of values if a new month begins.
//
//  NOTE:    counters FileElapsedDays, FileDay, FileMonth, FileYear and
//           FileLastDay were initialized in climate_openFile().
//
{
    int i;
    int deltaDays;

    // --- see if a new day has begun
	deltaDays = (int)(floor(theDate) - floor(project->StartDateTime));
	if (deltaDays >project->FileElapsedDays)
    {
        // --- advance day counters
		project->FileElapsedDays++;
		project->FileDay++;

        // --- see if new month of data needs to be read from file
		if (project->FileDay > project->FileLastDay)
        {
			project->FileMonth++;
			if (project->FileMonth > 12)
            {
				project->FileMonth = 1;
				project->FileYear++;
            }
            readFileValues(project);
			project->FileDay = 1;
			project->FileLastDay = datetime_daysPerMonth(project->FileYear, project->FileMonth);
        }

        // --- set climate variables for new day
        for (i=TMIN; i<=WIND; i++)
        {
            // --- no change in current value if its missing
			if (project->FileData[i][project->FileDay] == MISSING) continue;
			project->FileValue[i] = project->FileData[i][project->FileDay];
        }
    }
}

//=============================================================================

void setTemp(Project* project, DateTime theDate)
//
//  Input:   theDate = simulation date
//  Output:  none
//  Purpose: updates temperatures for new simulation date.
//
{
    int      j;                        // snow data object index
    int      k;                        // time series index
    int      day;                      // day of year
    DateTime theDay;                   // calendar day
    double   hour;                     // hour of day
    double   tmp;                      // temporary temperature

    // --- see if a new day has started
    theDay = floor(theDate);
	if (theDay > project->LastDay)
    {
        // --- update min. & max. temps & their time of day
        day = datetime_dayOfYear(theDate);
		if (project->Temp.dataSource == FILE_TEMP)
        {
			project->Tmin = project->FileValue[TMIN];
			project->Tmax = project->FileValue[TMAX];
			if (project->Tmin > project->Tmax)
            {
				tmp = project->Tmin;
				project->Tmin = project->Tmax;
				project->Tmax = tmp;
            } 
			updateTempTimes(project, day);
			if (project->Evap.type == TEMPERATURE_EVAP)
				project->FileValue[EVAP] = getTempEvap(project, day);
        }

        // --- compute snow melt coefficients based on day of year
		project->Snow.season = sin(0.0172615*(day - 81.0));
		for (j = 0; j<project->Nobjects[SNOWMELT]; j++)
        {
			snow_setMeltCoeffs(project, j, project->Snow.season);
        }

        // --- update date of last day analyzed
		project->LastDay = theDate;
    }

    // --- for min/max daily temps. from climate file,
    //     compute hourly temp. by sinusoidal interp.
	if (project->Temp.dataSource == FILE_TEMP)
    {
        hour = (theDate - theDay) * 24.0;
		if (hour < project->Hrsr)
			project->Temp.ta = project->Tmin + project->Trng1 / 2.0 * sin(PI / project->Dydif * (project->Hrsr - hour));
		else if (hour >= project->Hrsr && hour <= project->Hrss)
			project->Temp.ta = project->Tave + project->Trng * sin(PI / project->Dhrdy * (project->Hrday - hour));
        else
			project->Temp.ta = project->Tmax - project->Trng * sin(PI / project->Dydif * (hour - project->Hrss));
    }

    // --- for user-supplied temperature time series,
    //     get temperature value from time series
	if (project->Temp.dataSource == TSERIES_TEMP)
    {
		k = project->Temp.tSeries;
        if ( k >= 0)
        {
			project->Temp.ta = table_tseriesLookup(&project->Tseries[k], theDate, TRUE);

            // --- convert from deg. C to deg. F if need be
			if (project->UnitSystem == SI)
            {
				project->Temp.ta = (9. / 5.) * project->Temp.ta + 32.0;
            }
        }
    }

    // --- compute saturation vapor pressure
	project->Temp.ea = 8.1175e6 * exp(-7701.544 / (project->Temp.ta + 405.0265));
}

//=============================================================================

void setEvap(Project* project, DateTime theDate)
//
//  Input:   theDate = simulation date
//  Output:  none
//  Purpose: sets evaporation rate (ft/sec) for a specified date.
//
{
    int yr, mon, day, k;

	switch (project->Evap.type)
    {
      case CONSTANT_EVAP:
		  project->Evap.rate = project->Evap.monthlyEvap[0] / UCF(project,EVAPRATE);
        break;

      case MONTHLY_EVAP:
        datetime_decodeDate(theDate, &yr, &mon, &day);
		project->Evap.rate = project->Evap.monthlyEvap[mon - 1] / UCF(project, EVAPRATE);
        break;

      case TIMESERIES_EVAP:
		  if (theDate >= project->NextEvapDate)
			  project->Evap.rate = project->NextEvapRate / UCF(project, EVAPRATE);
        break;

      case FILE_EVAP:
		  project->Evap.rate = project->FileValue[EVAP] / UCF(project,EVAPRATE);
        datetime_decodeDate(theDate, &yr, &mon, &day);
		project->Evap.rate *= project->Evap.panCoeff[mon - 1];
        break;

      case TEMPERATURE_EVAP:
		  project->Evap.rate = project->FileValue[EVAP] / UCF(project,EVAPRATE);
        break;

	  default: project->Evap.rate = 0.0;
    }

    // --- set soil recovery factor
	project->Evap.recoveryFactor = 1.0;
	k = project->Evap.recoveryPattern;
	if (k >= 0 && project->Pattern[k].type == MONTHLY_PATTERN)
    {
        mon = datetime_monthOfYear(theDate) - 1;
		project->Evap.recoveryFactor = project->Pattern[k].factor[mon];
    }
}

//=============================================================================

void setWind(Project* project, DateTime theDate)
//
//  Input:   theDate = simulation date
//  Output:  none
//  Purpose: sets wind speed (mph) for a specified date.
//
{
    int yr, mon, day;

	switch (project->Wind.type)
    {
      case MONTHLY_WIND:
        datetime_decodeDate(theDate, &yr, &mon, &day);
		project->Wind.ws = project->Wind.aws[mon - 1] / UCF(project,WINDSPEED);
        break;

      case FILE_WIND:
		  project->Wind.ws = project->FileValue[WIND];
        break;

	  default: project->Wind.ws = 0.0;
    }
}

//=============================================================================

void updateTempTimes(Project* project, int day)
//
//  Input:   day = day of year
//  Output:  none
//  Purpose: computes time of day when min/max temperatures occur.
//           (min. temp occurs at sunrise, max. temp. at 3 hrs. < sunset)
//
{
    double decl;                       // earth's declination
    double hrang;                      // hour angle of sunrise/sunset
    double arg;

    decl  = 0.40928*cos(0.017202*(172.0-day));
	arg = -tan(decl)*project->Temp.tanAnglat;
    if      ( arg <= -1.0 ) arg = PI;
    else if ( arg >= 1.0 )  arg = 0.0;
    else                    arg = acos(arg);
    hrang = 3.8197 * arg;
	project->Hrsr = 12.0 - hrang + project->Temp.dtlong;
	project->Hrss = 12.0 + hrang + project->Temp.dtlong - 3.0;
	project->Dhrdy = project->Hrsr - project->Hrss;
	project->Dydif = 24.0 + project->Hrsr - project->Hrss;
	project->Hrday = (project->Hrsr + project->Hrss) / 2.0;
	project->Tave = (project->Tmin + project->Tmax) / 2.0;
	project->Trng = (project->Tmax - project->Tmin) / 2.0;
	if (project->Temp.tmax == MISSING)project->Trng1 = project->Tmax - project->Tmin;
	else                       project->Trng1 = project->Temp.tmax - project->Tmin;
	project->Temp.tmax = project->Tmax;
}

//=============================================================================

double getTempEvap(Project* project, int day)
//
//  Input:   day = day of year
//  Output:  returns evaporation rate in in/day
//  Purpose: uses Hargreaves method to compute daily evaporation rate
//           from daily min/max temperatures and Julian day.
//
{
    double a = 2.0*PI/365.0;
	double ta = (project->Tave - 32.0)*5.0 / 9.0;           //average temperature (deg C)
	double tr = (project->Tmax - project->Tmin)*5.0 / 9.0;           //temperature range (deg C)
    double lamda = 2.50 - 0.002361 * ta;         //latent heat of vaporization
    double dr = 1.0 + 0.033*cos(a*day);          //relative earth-sun distance
	double phi = project->Temp.anglat*2.0*PI / 360.0;       //latitude angle (rad)
    double del = 0.4093*sin(a*(284+day));        //solar declination angle (rad)
    double omega = acos(-tan(phi)*tan(del));     //sunset hour angle (rad)
    double ra = 37.6*dr*                         //extraterrestrial radiation
                (omega*sin(phi)*sin(del) +
                 cos(phi)*cos(del)*sin(omega));
    double e = 0.0023*ra/lamda*sqrt(tr)*(ta+17.8);    //evap. rate (mm/day)
    if ( e < 0.0 ) e = 0.0; 
	if (project->UnitSystem == US) e /= MMperINCH;
    return e;
}

//=============================================================================

int  getFileFormat(Project* project)
//
//  Input:   none
//  Output:  returns code number of climate file's format
//  Purpose: determines what format the climate file is in.
//
{
    char recdType[4] = "";
    char elemType[4] = "";
    char filler[5] = "";
    char staID[80];
    char s[80];
    char line[MAXLINE];
    int  y, m, d, n;

    // --- read first line of file
	if (fgets(line, MAXLINE, project->Fclimate.file) == NULL) return UNKNOWN_FORMAT;

    // --- check for TD3200 format
    sstrncpy(recdType, line, 3);
    sstrncpy(filler, &line[23], 4);
    if ( strcmp(recdType, "DLY") == 0 &&
         strcmp(filler, "9999")  == 0 ) return TD3200;

    // --- check for DLY0204 format
    if ( strlen(line) >= 233 )
    { 
        sstrncpy(elemType, &line[13], 3);
        n = atoi(elemType);
        if ( n == 1 || n == 2 || n == 151 ) return DLY0204;
    }
    
    // --- check for USER_PREPARED format
    n = sscanf(line, "%s %d %d %d %s", staID, &y, &m, &d, s);
    if ( n == 5 ) return USER_PREPARED;
    return UNKNOWN_FORMAT;
}

//=============================================================================

void readFileLine(Project* project, int *y, int *m)
//
//  Input:   none
//  Output:  y = year
//           m = month
//  Purpose: reads year & month from next line of climate file.
//
{
    // --- read next line from climate data file
	while (strlen(project->FileLine) == 0)
    {
		if (fgets(project->FileLine, MAXLINE, project->Fclimate.file) == NULL) return;
		if (project->FileLine[0] == '\n')project->FileLine[0] = '\0';
    }

    // --- parse year & month from line
	switch (project->FileFormat)
    {
	case  USER_PREPARED: readUserFileLine(project, y, m);   break;
    case  TD3200:        readTD3200FileLine(project,y,m);  break;
    case  DLY0204:       readDLY0204FileLine(project,y,m); break;
    }
}

//=============================================================================

void readUserFileLine(Project* project, int* y, int* m)
//
//  Input:   none
//  Output:  y = year
//           m = month
//  Purpose: reads year & month from line of User-Prepared climate file.
//
{
    int n;
    char staID[80];
	n = sscanf(project->FileLine, "%s %d %d", staID, y, m);
    if ( n < 3 )
    {
		report_writeErrorMsg(project,ERR_CLIMATE_FILE_READ, project->Fclimate.name);
    }
}

//=============================================================================

void readTD3200FileLine(Project* project, int* y, int* m)
//
//  Input:   none
//  Output:  y = year
//           m = month
//  Purpose: reads year & month from line of TD-3200 climate file.
//
{
    char recdType[4] = "";
    char year[5] = "";
    char month[3] = "";
    int  len;

    // --- check for minimum number of characters
	len = strlen(project->FileLine);
    if ( len < 30 )
    {
		report_writeErrorMsg(project,ERR_CLIMATE_FILE_READ, project->Fclimate.name);
        return;
    }

    // --- check for proper type of record
	sstrncpy(recdType, project->FileLine, 3);
    if ( strcmp(recdType, "DLY") != 0 )
    {
		report_writeErrorMsg(project,ERR_CLIMATE_FILE_READ, project->Fclimate.name);
        return;
    }

    // --- get record's date 
	sstrncpy(year, &project->FileLine[17], 4);
	sstrncpy(month, &project->FileLine[21], 2);
    *y = atoi(year);
    *m = atoi(month);
}

//=============================================================================

void readDLY0204FileLine(Project* project, int* y, int* m)
//
//  Input:   none
//  Output:  y = year
//           m = month
//  Purpose: reads year & month from line of DLY02 or DLY04 climate file.
//
{
    char year[5] = "";
    char month[3] = "";
    int  len;

    // --- check for minimum number of characters
	len = strlen(project->FileLine);
    if ( len < 16 )
    {
		report_writeErrorMsg(project,ERR_CLIMATE_FILE_READ, project->Fclimate.name);
        return;
    }

    // --- get record's date 
	sstrncpy(year, &project->FileLine[7], 4);
	sstrncpy(month, &project->FileLine[11], 2);
    *y = atoi(year);
    *m = atoi(month);
}

//=============================================================================

void readFileValues(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: reads next month's worth of data from climate file.
//
{
    int  i, j;
    int  y, m;

    // --- initialize FileData array to missing values
    for ( i=0; i<MAXCLIMATEVARS; i++)
    {
		for (j = 0; j<MAXDAYSPERMONTH; j++) project->FileData[i][j] = MISSING;
    }

	while (!project->ErrorCode)
    {
        // --- return when date on line is after current file date
		if (feof(project->Fclimate.file)) return;
        readFileLine(project,&y, &m);
		if (y > project->FileYear || m >project->FileMonth) return;

        // --- parse climate values from file line
		switch (project->FileFormat)
        {
        case  USER_PREPARED: parseUserFileLine(project);   break;
		case  TD3200:        parseTD3200FileLine(project);  break;
		case  DLY0204:       parseDLY0204FileLine(project); break;
        }
		strcpy(project->FileLine, "");
    }
}

//=============================================================================

void parseUserFileLine(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: parses climate variable values from a line of a user-prepared
//           climate file.
//
{
    int   n;
    int   y, m, d;
    char  staID[80];
    char  s0[80];
    char  s1[80];
    char  s2[80];
    char  s3[80];
    double x;

    // --- read day, Tmax, Tmin, Evap, & Wind from file line
	n = sscanf(project->FileLine, "%s %d %d %d %s %s %s %s",
        staID, &y, &m, &d, s0, s1, s2, s3);
    if ( n < 4 ) return;
    if ( d < 1 || d > 31 ) return;

    // --- process TMAX
    if ( strlen(s0) > 0 && *s0 != '*' )
    {
        x = atof(s0);
		if (project->UnitSystem == SI) x = 9. / 5.*x + 32.0;
		project->FileData[TMAX][d] = x;
    }
 
    // --- process TMIN
    if ( strlen(s1) > 0 && *s1 != '*' )
    {
        x = atof(s1);
        if ( project->UnitSystem == SI ) x = 9./5.*x + 32.0;
		project->FileData[TMIN][d] = x;
    }

    // --- process EVAP
	if (strlen(s2) > 0 && *s2 != '*') project->FileData[EVAP][d] = atof(s2);

    // --- process WIND
	if (strlen(s3) > 0 && *s3 != '*') project->FileData[WIND][d] = atof(s3);
}

//=============================================================================

void parseTD3200FileLine(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: parses climate variable values from a line of a TD3200 file.
//
{
    int  i;
    char param[5] = "";

    // --- parse parameter name
	sstrncpy(param, &project->FileLine[11], 4);

    // --- see if parameter is temperature, evaporation or wind speed
    for (i=0; i<MAXCLIMATEVARS; i++)
    {
		if (strcmp(param, ClimateVarWords[i]) == 0) setTD3200FileValues(project,i);
    }
}

//=============================================================================

void setTD3200FileValues(Project* project, int i)
//
//  Input:   i = climate variable code
//  Output:  none
//  Purpose: reads month worth of values for climate variable from TD-3200 file.
//
{
    char valCount[4] = "";
    char day[3] = "";
    char sign[2] = "";
    char value[6] = "";
    char flag2[2] = "";
    double x;
    int  nValues;
    int  j, k, d;
    int  lineLength;

    // --- parse number of days with data from cols. 27-29 of file line
	sstrncpy(valCount, &project->FileLine[27], 3);
    nValues = atoi(valCount);
	lineLength = strlen(project->FileLine);

    // --- check for enough characters on line
    if ( lineLength >= 12*nValues + 30 )
    {
        // --- for each day's value
        for (j=0; j<nValues; j++)
        {
            // --- parse day, value & flag from file line
            k = 30 + j*12;
			
			sstrncpy(day, &project->FileLine[k], 2);
			sstrncpy(sign, &project->FileLine[k + 4], 1);
			sstrncpy(value, &project->FileLine[k + 5], 5);
			sstrncpy(flag2, &project->FileLine[k + 11], 1);

            // --- if value is valid then store it in FileData array
            d = atoi(day);
            if ( strcmp(value, "99999") != 0 
                 && ( flag2[0] == '0' || flag2[0] == '1')
                 &&   d > 0
                 &&   d <= 31 )
            {
                // --- convert from string value to numerical value
                x = atof(value);
                if ( sign[0] == '-' ) x = -x;

                // --- convert evaporation from hundreths of inches
                if ( i == EVAP )
                {
                    x /= 100.0;

                    // --- convert to mm if using SI units
					if (project->UnitSystem == SI) x *= MMperINCH;
                }
                
                // --- convert wind speed from miles/day to miles/hour
                if ( i == WIND ) x /= 24.0;
 
                // --- store value
				project->FileData[i][d] = x;
            }
        }
    }
}

//=============================================================================

void parseDLY0204FileLine(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: parses a month's worth of climate variable values from a line of
//           a DLY02 or DLY04 climate file.
//
{
    int  j, k, p;
    char param[4] = "";
    char sign[2]  = "";
    char value[6] = "";
    char code[2]  = "";
    double x;

    // --- parse parameter name
	sstrncpy(param, &project->FileLine[13], 3);

    // --- see if parameter is min or max temperature
    p = atoi(param);
    if ( p == 1 ) p = TMAX;
    else if ( p == 2 ) p = TMIN;
    else if ( p == 151 ) p = EVAP;
    else return;

    // --- check for 233 characters on line
	if (strlen(project->FileLine) < 233) return;

    // --- for each of 31 days
    k = 16;
    for (j=1; j<=31; j++)
    {
        // --- parse value & flag from file line
		sstrncpy(sign, &project->FileLine[k], 1);
		sstrncpy(value, &project->FileLine[k + 1], 5);
		sstrncpy(code, &project->FileLine[k + 6], 1);
        k += 7;

        // --- if value is valid then store it in FileData array

        if ( strcmp(value, "99999") != 0 && strcmp(value, "     ") != 0 )
        {
            switch (p)
            {
            case TMAX:
            case TMIN:
                // --- convert from integer tenths of a degree C to degrees F
                x = atof(value) / 10.0;
                if ( sign[0] == '-' ) x = -x;
                x = 9./5.*x + 32.0;
                break;
            case EVAP:
                // --- convert from 0.1 mm to inches or mm
                x = atof(value) / 10.0;
				if (project->UnitSystem == US) x /= MMperINCH;
                break;
			default: return;
            }
			project->FileData[p][j] = x;
        }
    }
}

//=============================================================================
