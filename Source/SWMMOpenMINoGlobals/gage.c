//-----------------------------------------------------------------------------
//   gage.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/20/10  (Build 5.1.001)
//   Author:   L. Rossman
//
//   Rain gage functions.
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE


#include <string.h>
#include <math.h>
#include "headers.h"

//-----------------------------------------------------------------------------
//  Constants
//-----------------------------------------------------------------------------
const double OneSecond = 1.1574074e-5;

//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)
//-----------------------------------------------------------------------------
//  gage_readParams        (called by input_readLine)
//  gage_validate          (called by project_validate)
//  gage_initState         (called by project_init)
//  gage_setState          (called by runoff_execute & getRainfall in rdii.c)
//  gage_getPrecip         (called by subcatch_getRunoff)
//  gage_getNextRainDate   (called by runoff_getTimeStep)

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------
static int    readGageSeriesFormat(Project* project, char* tok[], int ntoks, double x[]);
static int    readGageFileFormat(char* tok[], int ntoks, double x[]);
static int    getFirstRainfall(Project* project, int gage);
static int    getNextRainfall(Project* project, int gage);
static double convertRainfall(Project* project, int gage, double rain);


//=============================================================================

int gage_readParams(Project* project, int j, char* tok[], int ntoks)
//
//  Input:   j = rain gage index
//           tok[] = array of string tokens
//           ntoks = number of tokens
//  Output:  returns an error code
//  Purpose: reads rain gage parameters from a line of input data
//
//  Data formats are:
//    Name RainType RecdFreq SCF TIMESERIES SeriesName
//    Name RainType RecdFreq SCF FILE FileName Station Units StartDate
//
{
    int      k, err;
    char     *id;
    char     fname[MAXFNAME+1];
    char     staID[MAXMSG+1];
    double   x[7];

    // --- check that gage exists
    if ( ntoks < 2 ) return error_setInpError(ERR_ITEMS, "");
    id = project_findID(project,GAGE, tok[0]);
    if ( id == NULL ) return error_setInpError(ERR_NAME, tok[0]);

    // --- assign default parameter values
    x[0] = -1.0;         // No time series index
    x[1] = 1.0;          // Rain type is volume
    x[2] = 3600.0;       // Recording freq. is 3600 sec
    x[3] = 1.0;          // Snow catch deficiency factor
    x[4] = NO_DATE;      // Default is no start/end date
    x[5] = NO_DATE;
    x[6] = 0.0;          // US units
    strcpy(fname, "");
    strcpy(staID, "");

    if ( ntoks < 5 ) return error_setInpError(ERR_ITEMS, "");
    k = findmatch(tok[4], GageDataWords);
    if      ( k == RAIN_TSERIES )
    {
        err = readGageSeriesFormat(project,tok, ntoks, x);
    }
    else if ( k == RAIN_FILE    )
    {
        if ( ntoks < 8 ) return error_setInpError(ERR_ITEMS, "");
        sstrncpy(fname, tok[5], MAXFNAME);
        sstrncpy(staID, tok[6], MAXMSG);
        err = readGageFileFormat(tok, ntoks, x);
    }
    else return error_setInpError(ERR_KEYWORD, tok[4]);

    // --- save parameters to rain gage object
    if ( err > 0 ) return err;
    project->Gage[j].ID = id;
    project->Gage[j].tSeries      = (int)x[0];
    project->Gage[j].rainType     = (int)x[1];
    project->Gage[j].rainInterval = (int)x[2];
    project->Gage[j].snowFactor   = x[3];
    project->Gage[j].rainUnits    = (int)x[6];
    if ( project->Gage[j].tSeries >= 0 ) project->Gage[j].dataSource = RAIN_TSERIES;
    else                        project->Gage[j].dataSource = RAIN_FILE;
    if ( project->Gage[j].dataSource == RAIN_FILE )
    {
        sstrncpy(project->Gage[j].fname, fname, MAXFNAME);
        sstrncpy(project->Gage[j].staID, staID, MAXMSG);
        project->Gage[j].startFileDate = x[4];
        project->Gage[j].endFileDate = x[5];
    }
    project->Gage[j].unitsFactor = 1.0;
    project->Gage[j].coGage = -1;
    project->Gage[j].isUsed = FALSE;
    return 0;
}

//=============================================================================

int readGageSeriesFormat(Project* project, char* tok[], int ntoks, double x[])
{
    int m, ts;
    DateTime aTime;

    if ( ntoks < 6 ) return error_setInpError(ERR_ITEMS, "");

    // --- determine type of rain data
    m = findmatch(tok[1], RainTypeWords);
    if ( m < 0 ) return error_setInpError(ERR_KEYWORD, tok[1]);
    x[1] = (double)m;

    // --- get data time interval & convert to seconds
    if ( getDouble(tok[2], &x[2]) ) x[2] = floor(x[2]*3600 + 0.5);
    else if ( datetime_strToTime(tok[2], &aTime) )
    {
        x[2] = floor(aTime*SECperDAY + 0.5);
    }
    else return error_setInpError(ERR_DATETIME, tok[2]);
    if ( x[2] <= 0.0 ) return error_setInpError(ERR_DATETIME, tok[2]);

    // --- get snow catch deficiency factor
    if ( !getDouble(tok[3], &x[3]) )
        return error_setInpError(ERR_DATETIME, tok[3]);;

    // --- get time series index
    ts = project_findObject(project,TSERIES, tok[5]);
    if ( ts < 0 ) return error_setInpError(ERR_NAME, tok[5]);
    x[0] = (double)ts;
    strcpy(tok[2], "");
    return 0;
}

//=============================================================================

int readGageFileFormat(char* tok[], int ntoks, double x[])
{
    int   m, u;
    DateTime aDate;
    DateTime aTime;

    // --- determine type of rain data
    m = findmatch(tok[1], RainTypeWords);
    if ( m < 0 ) return error_setInpError(ERR_KEYWORD, tok[1]);
    x[1] = (double)m;

    // --- get data time interval & convert to seconds
    if ( getDouble(tok[2], &x[2]) ) x[2] *= 3600;
    else if ( datetime_strToTime(tok[2], &aTime) )
    {
        x[2] = floor(aTime*SECperDAY + 0.5);
    }
    else return error_setInpError(ERR_DATETIME, tok[2]);
    if ( x[2] <= 0.0 ) return error_setInpError(ERR_DATETIME, tok[2]);

    // --- get snow catch deficiency factor
    if ( !getDouble(tok[3], &x[3]) )
        return error_setInpError(ERR_NUMBER, tok[3]);
 
    // --- get rain depth units
    u = findmatch(tok[7], RainUnitsWords);
    if ( u < 0 ) return error_setInpError(ERR_KEYWORD, tok[7]);
    x[6] = (double)u;

    // --- get start date (if present)
    if ( ntoks > 8 && *tok[8] != '*')
    {
        if ( !datetime_strToDate(tok[8], &aDate) )
            return error_setInpError(ERR_DATETIME, tok[8]);
        x[4] = (float) aDate;
    }
    return 0;
}

//=============================================================================

void  gage_validate(Project* project, int j)
//
//  Input:   j = rain gage index
//  Output:  none
//  Purpose: checks for valid rain gage parameters
//
//  NOTE: assumes that any time series used by a rain gage has been
//        previously validated.
//
{
    int i, k;
    int gageInterval;

    // --- for gage with time series data:
    if ( project->Gage[j].dataSource == RAIN_TSERIES )
    {
        // --- check gage's recording interval against that of time series
        k = project->Gage[j].tSeries;
        if ( project->Tseries[k].refersTo >= 0 )
        {
            report_writeErrorMsg(project,ERR_RAIN_GAGE_TSERIES, project->Gage[j].ID);
        }
        gageInterval = (int)(floor(project->Tseries[k].dxMin*SECperDAY + 0.5));
        if ( gageInterval > 0 && project->Gage[j].rainInterval > gageInterval )
        {
            report_writeErrorMsg(project,ERR_RAIN_GAGE_INTERVAL, project->Gage[j].ID);
        } 
        if ( project->Gage[j].rainInterval < gageInterval )
        {
            report_writeWarningMsg(project,WARN09, project->Gage[j].ID);
        }
        if ( project->Gage[j].rainInterval < project->WetStep )
        {
            report_writeWarningMsg(project,WARN01, project->Gage[j].ID);
            project->WetStep = project->Gage[j].rainInterval;
        }

        // --- see if gage uses same time series as another gage
        for (i=0; i<j; i++)
        {
            if ( project->Gage[i].dataSource == RAIN_TSERIES && project->Gage[i].tSeries == k )
            {
                project->Gage[j].coGage = i;

                // --- check that both gages record same type of data
                if ( project->Gage[j].rainType != project->Gage[i].rainType )
                {
                    report_writeErrorMsg(project,ERR_RAIN_GAGE_FORMAT, project->Gage[j].ID);
                }
                return;
            }
        }
    }
}

//=============================================================================

void  gage_initState(Project* project, int j)
//
//  Input:   j = rain gage index
//  Output:  none
//  Purpose: initializes state of rain gage.
//
{
    // --- assume gage not used by any subcatchment
    //     (will be updated in subcatch_initState)
    project->Gage[j].isUsed = FALSE;
    project->Gage[j].rainfall = 0.0;
    project->Gage[j].reportRainfall = 0.0;
    if ( project->IgnoreRainfall ) return;

    // --- for gage with file data:
    if ( project->Gage[j].dataSource == RAIN_FILE )
    {
        // --- set current file position to start of period of record
        project->Gage[j].currentFilePos = project->Gage[j].startFilePos;

        // --- assign units conversion factor
        //     (rain depths on interface file are in inches)
        if ( project->UnitSystem == SI ) project->Gage[j].unitsFactor = MMperINCH;
    }

    // --- get first & next rainfall values
    if ( getFirstRainfall(project,j) )
    {
        // --- find date at end of starting rain interval
        project->Gage[j].endDate = datetime_addSeconds(
                          project->Gage[j].startDate, project->Gage[j].rainInterval);

        // --- if rainfall record begins after start of simulation,
        if ( project->Gage[j].startDate > project->StartDateTime )
        {
            // --- make next rainfall date the start of the rain record
            project->Gage[j].nextDate = project->Gage[j].startDate;
            project->Gage[j].nextRainfall = project->Gage[j].rainfall;

            // --- make start of current rain interval the simulation start
            project->Gage[j].startDate = project->StartDateTime;
            project->Gage[j].endDate = project->Gage[j].nextDate;
            project->Gage[j].rainfall = 0.0;
        }

        // --- otherwise find next recorded rainfall
        else if ( !getNextRainfall(project,j) ) project->Gage[j].nextDate = NO_DATE;
    }
    else project->Gage[j].startDate = NO_DATE;
}

//=============================================================================

void gage_setState(Project* project, int j, DateTime t)
//
//  Input:   j = rain gage index
//           t = a calendar date/time
//  Output:  none
//  Purpose: updates state of rain gage for specified date. 
//
{
    // --- return if gage not used by any subcatchment
    if ( project->Gage[j].isUsed == FALSE ) return;

    // --- set rainfall to zero if disabled
    if ( project->IgnoreRainfall )
    {
        project->Gage[j].rainfall = 0.0;
        return;
    }

    // --- use rainfall from co-gage (gage with lower index that uses
    //     same rainfall time series or file) if it exists
    if ( project->Gage[j].coGage >= 0)
    {
        project->Gage[j].rainfall = project->Gage[project->Gage[j].coGage].rainfall;
        return;
    }

    // --- otherwise march through rainfall record until date t is bracketed
    t += OneSecond;
    for (;;)
    {
        // --- no rainfall if no interval start date
        if ( project->Gage[j].startDate == NO_DATE )
        {
            project->Gage[j].rainfall = 0.0;
            return;
        }

        // --- no rainfall if time is before interval start date
        if ( t < project->Gage[j].startDate )
        {
            project->Gage[j].rainfall = 0.0;
            return;
        }

        // --- use current rainfall if time is before interval end date
        if ( t < project->Gage[j].endDate )
        {
            return;
        }

        // --- no rainfall if t >= interval end date & no next interval exists
        if ( project->Gage[j].nextDate == NO_DATE )
        {
            project->Gage[j].rainfall = 0.0;
            return;
        }

        // --- no rainfall if t > interval end date & <  next interval date
        if ( t < project->Gage[j].nextDate )
        {
            project->Gage[j].rainfall = 0.0;
            return;
        }

        // --- otherwise update next rainfall interval date
        project->Gage[j].startDate = project->Gage[j].nextDate;
        project->Gage[j].endDate = datetime_addSeconds(project->Gage[j].startDate,
                          project->Gage[j].rainInterval);
        project->Gage[j].rainfall = project->Gage[j].nextRainfall;
        if ( !getNextRainfall(project,j) ) project->Gage[j].nextDate = NO_DATE;
    }
}

//=============================================================================

DateTime gage_getNextRainDate(Project* project, int j, DateTime aDate)
//
//  Input:   j = rain gage index
//           aDate = calendar date/time
//  Output:  next date with rainfall occurring
//  Purpose: finds the next date from  specified date when rainfall occurs.
//
{
    if ( project->Gage[j].isUsed == FALSE ) return aDate;
    aDate += OneSecond;
    if ( aDate < project->Gage[j].startDate ) return project->Gage[j].startDate;
    if ( aDate < project->Gage[j].endDate   ) return project->Gage[j].endDate;
    return project->Gage[j].nextDate;
}

//=============================================================================

double gage_getPrecip(Project* project, int j, double *rainfall, double *snowfall)
//
//  Input:   j = rain gage index
//  Output:  rainfall = rainfall rate (ft/sec)
//           snowfall = snow fall rate (ft/sec)
//           returns total precipitation (ft/sec)
//  Purpose: determines whether gage's recorded rainfall is rain or snow.
//
{
    *rainfall = 0.0;
    *snowfall = 0.0;
    if ( !project->IgnoreSnowmelt && project->Temp.ta <= project->Snow.snotmp )
    {
       *snowfall = project->Gage[j].rainfall * project->Gage[j].snowFactor / UCF(project,RAINFALL);
    }
    else *rainfall = project->Gage[j].rainfall / UCF(project,RAINFALL);
    return (*rainfall) + (*snowfall);
} 

//=============================================================================

void gage_setReportRainfall(Project* project, int j, DateTime reportDate)
//
//  Input:   j = rain gage index
//           reportDate = date/time value of current reporting time
//  Output:  none
//  Purpose: sets the rainfall value reported at the current reporting time.
//
{
    double result;

    // --- use value from co-gage if it exists
    if ( project->Gage[j].coGage >= 0)
    {
        project->Gage[j].reportRainfall = project->Gage[project->Gage[j].coGage].reportRainfall;
        return;
    }

    // --- otherwise increase reporting time by 1 second to avoid
    //     roundoff problems
    reportDate += OneSecond;

    // --- use current rainfall if report date/time is before end
    //     of current rain interval
    if ( reportDate < project->Gage[j].endDate ) result = project->Gage[j].rainfall;

    // --- use 0.0 if report date/time is before start of next rain interval
    else if ( reportDate < project->Gage[j].nextDate ) result = 0.0;

    // --- otherwise report date/time falls right on end of current rain
    //     interval and start of next interval so use next interval's rainfall
    else result = project->Gage[j].nextRainfall;
    project->Gage[j].reportRainfall = result;
}

//=============================================================================

int getFirstRainfall(Project* project, int j)
//
//  Input:   j = rain gage index
//  Output:  returns TRUE if successful
//  Purpose: positions rainfall record to date with first rainfall.
//
{
    int    k;                          // time series index
    float  vFirst;                     // first rain volume (ft or m)
    double rFirst;                     // first rain intensity (in/hr or mm/hr)

    // --- assign default values to date & rainfall
    project->Gage[j].startDate = NO_DATE;
    project->Gage[j].rainfall = 0.0;

    // --- initialize internal cumulative rainfall value
    project->Gage[j].rainAccum = 0;

    // --- use rain interface file if applicable
    if ( project->Gage[j].dataSource == RAIN_FILE )
    {
        if ( project->Frain.file && project->Gage[j].endFilePos > project->Gage[j].startFilePos )
        {
            // --- retrieve 1st date & rainfall volume from file
            fseek(project->Frain.file, project->Gage[j].startFilePos, SEEK_SET);
            fread(&project->Gage[j].startDate, sizeof(DateTime), 1, project->Frain.file);
            fread(&vFirst, sizeof(float), 1, project->Frain.file);
            project->Gage[j].currentFilePos = ftell(project->Frain.file);

            // --- convert rainfall to intensity
            project->Gage[j].rainfall = convertRainfall(project,j, (double)vFirst);
            return 1;
        }
        return 0;
    }

    // --- otherwise access user-supplied rainfall time series
    else
    {
        k = project->Gage[j].tSeries;
        if ( k >= 0 )
        {
            // --- retrieve first rainfall value from time series
            if ( table_getFirstEntry(&project->Tseries[k], &project->Gage[j].startDate,
                                     &rFirst) )
            {
                // --- convert rainfall to intensity
                project->Gage[j].rainfall = convertRainfall(project,j, rFirst);
                return 1;
            }
        }
        return 0;
    }
}

//=============================================================================

int getNextRainfall(Project* project, int j)
//
//  Input:   j = rain gage index
//  Output:  returns 1 if successful; 0 if not
//  Purpose: positions rainfall record to date with next non-zero rainfall
//           while updating the gage's next rain intensity value.
//
//  Note: zero rainfall values explicitly entered into a rain file or
//        time series are skipped over so that a proper accounting of
//        wet and dry periods can be maintained.
//
{
    int    k;                          // time series index
    float  vNext;                      // next rain volume (ft or m)
    double rNext;                      // next rain intensity (in/hr or mm/hr)

    project->Gage[j].nextRainfall = 0.0;
    do
    {
        if ( project->Gage[j].dataSource == RAIN_FILE )
        {
            if ( project->Frain.file && project->Gage[j].currentFilePos < project->Gage[j].endFilePos )
            {
                fseek(project->Frain.file, project->Gage[j].currentFilePos, SEEK_SET);
                fread(&project->Gage[j].nextDate, sizeof(DateTime), 1, project->Frain.file);
                fread(&vNext, sizeof(float), 1, project->Frain.file);
                project->Gage[j].currentFilePos = ftell(project->Frain.file);
                rNext = convertRainfall(project,j, (double)vNext);
            }
            else return 0;
        }

        else
        {
            k = project->Gage[j].tSeries;
            if ( k >= 0 )
            {
                if ( !table_getNextEntry(&project->Tseries[k],
                        &project->Gage[j].nextDate, &rNext) ) return 0;
                rNext = convertRainfall(project,j, rNext);
            }
            else return 0;
        }
    } while (rNext == 0.0);
    project->Gage[j].nextRainfall = rNext;
    return 1;
}

//=============================================================================

double convertRainfall(Project* project, int j, double r)
//
//  Input:   j = rain gage index
//           r = rainfall value (user units)
//  Output:  returns rainfall intensity (user units)
//  Purpose: converts rainfall value to an intensity (depth per hour).
//
{
    double r1;
    switch ( project->Gage[j].rainType )
    {
      case RAINFALL_INTENSITY:
        r1 = r;
        break;

      case RAINFALL_VOLUME:
        r1 = r / project->Gage[j].rainInterval * 3600.0;
        break;

      case CUMULATIVE_RAINFALL:
        if ( r  < project->Gage[j].rainAccum )
             r1 = r / project->Gage[j].rainInterval * 3600.0;
        else r1 = (r - project->Gage[j].rainAccum) / project->Gage[j].rainInterval * 3600.0;
        project->Gage[j].rainAccum = r;
        break;

      default: r1 = r;
    }
    return r1 * project->Gage[j].unitsFactor;
}

//=============================================================================
