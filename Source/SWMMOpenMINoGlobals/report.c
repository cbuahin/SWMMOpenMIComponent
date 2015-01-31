//-----------------------------------------------------------------------------
//   report.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/21/2014  (Build 5.1.001)
//             04/14/14    (Build 5.1.004)
//   Author:   L. Rossman (EPA)
//
//   Report writing functions.
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE


//#include <malloc.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "headers.h"

#define WRITE(x,y) (report_writeLine((x),(y)))
#define LINE_10 "----------"
#define LINE_12 "------------"
#define LINE_51 \
"---------------------------------------------------"
#define LINE_64 \
"----------------------------------------------------------------"


//-----------------------------------------------------------------------------
//  Shared variables   
//-----------------------------------------------------------------------------
static time_t SysTime;

//-----------------------------------------------------------------------------
//  Imported variables
//-----------------------------------------------------------------------------
#define REAL4 float
extern REAL4* SubcatchResults;         // Results vectors defined in OUTPUT.C
extern REAL4* NodeResults;             //  "
extern REAL4* LinkResults;             //  " 
extern char   ErrString[81];           // defined in ERROR.C

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------
static void report_LoadingErrors(Project *project, int p1, int p2, TLoadingTotals* totals);
static void report_QualErrors(Project *project, int p1, int p2, TRoutingTotals* totals);
static void report_Subcatchments(Project *project);
static void report_SubcatchHeader(Project *project, char *id);
static void report_Nodes(Project *project);
static void report_NodeHeader(Project *project, char *id);
static void report_Links(Project *project);
static void report_LinkHeader(Project *project, char *id);


//=============================================================================

int report_readOptions(Project *project, char* tok[], int ntoks)
//
//  Input:   tok[] = array of string tokens
//           ntoks = number of tokens
//  Output:  returns an error code
//  Purpose: reads reporting options from a line of input
//
{
    char  k;
    int   j, m, t;
    if ( ntoks < 2 ) return error_setInpError(ERR_ITEMS, "");
    k = (char)findmatch(tok[0], ReportWords);
    if ( k < 0 ) return error_setInpError(ERR_KEYWORD, tok[0]);
    switch ( k )
    {
      case 0: // Input
        m = findmatch(tok[1], NoYesWords);
        if      ( m == YES ) project->RptFlags.input = TRUE;
        else if ( m == NO )  project->RptFlags.input = FALSE;
        else                 return error_setInpError(ERR_KEYWORD, tok[1]);
        return 0;

      case 1: // Continuity
        m = findmatch(tok[1], NoYesWords);
        if      ( m == YES ) project->RptFlags.continuity = TRUE;
        else if ( m == NO )  project->RptFlags.continuity = FALSE;
        else                 return error_setInpError(ERR_KEYWORD, tok[1]);
        return 0;

      case 2: // Flow Statistics
        m = findmatch(tok[1], NoYesWords);
        if      ( m == YES ) project->RptFlags.flowStats = TRUE;
        else if ( m == NO )  project->RptFlags.flowStats = FALSE;
        else                 return error_setInpError(ERR_KEYWORD, tok[1]);
        return 0;

      case 3: // Controls
        m = findmatch(tok[1], NoYesWords);
        if      ( m == YES ) project->RptFlags.controls = TRUE;
        else if ( m == NO )  project->RptFlags.controls = FALSE;
        else                 return error_setInpError(ERR_KEYWORD, tok[1]);
        return 0;

      case 4:  m = SUBCATCH;  break;  // Subcatchments
      case 5:  m = NODE;      break;  // Nodes
      case 6:  m = LINK;      break;  // Links

      case 7: // project->Node Statistics
        m = findmatch(tok[1], NoYesWords);
        if      ( m == YES ) project->RptFlags.nodeStats = TRUE;
        else if ( m == NO )  project->RptFlags.nodeStats = FALSE;
        else                 return error_setInpError(ERR_KEYWORD, tok[1]);
        return 0;

      default: return error_setInpError(ERR_KEYWORD, tok[1]);
    }
    k = (char)findmatch(tok[1], NoneAllWords);
    if ( k < 0 )
    {
        k = SOME;
        for (t = 1; t < ntoks; t++)
        {
            j = project_findObject(project, m, tok[t]);
            if ( j < 0 ) return error_setInpError(ERR_NAME, tok[t]);
            switch ( m )
            {
              case SUBCATCH:  project->Subcatch[j].rptFlag = TRUE;  break;
              case NODE:      project->Node[j].rptFlag = TRUE;  break;
              case LINK:      project->Link[j].rptFlag = TRUE;  break;
            }
        }
    }
    switch ( m )
    {
      case SUBCATCH: project->RptFlags.subcatchments = k;  break;
      case NODE:     project->RptFlags.nodes = k;  break;
      case LINK:     project->RptFlags.links = k;  break;
    }
    return 0;
}

//=============================================================================

void report_writeLine(Project *project, char *line)
//
//  Input:   line = line of text
//  Output:  none
//  Purpose: writes line of text to report file.
//
{
    if ( project->Frpt.file ) fprintf(project->Frpt.file, "\n  %s", line);
}

//=============================================================================

void report_writeSysTime(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: writes starting/ending processing times to report file.
//
{
    char    theTime[9];
    double  elapsedTime;
    time_t  endTime;
    if ( project->Frpt.file )
    {
        fprintf(project->Frpt.file, FMT20, ctime(&SysTime));
        time(&endTime);
        fprintf(project->Frpt.file, FMT20a, ctime(&endTime));
        elapsedTime = difftime(endTime, SysTime);
        fprintf(project->Frpt.file, FMT21);
        if ( elapsedTime < 1.0 ) fprintf(project->Frpt.file, "< 1 sec");
        else
        {
            elapsedTime /= SECperDAY;                                      
            if (elapsedTime >= 1.0)
            {
                fprintf(project->Frpt.file, "%d.", (int)floor(elapsedTime));
                elapsedTime -= floor(elapsedTime);
            }
            datetime_timeToStr(elapsedTime, theTime);
            fprintf(project->Frpt.file, "%s", theTime);
        }
    }
}


//=============================================================================
//      SIMULATION OPTIONS REPORTING
//=============================================================================

void report_writeLogo(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: writes report header lines to report file.
//
{
    fprintf(project->Frpt.file, FMT08);
    fprintf(project->Frpt.file, FMT09);
    fprintf(project->Frpt.file, FMT10);
    time(&SysTime);                    // Save starting wall clock time
}

//=============================================================================

void report_writeTitle(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: writes project title to report file.
//
{
    int i;
    if ( project->ErrorCode ) return;
    for (i=0; i<MAXTITLE; i++) if ( strlen(project->Title[i]) > 0 )
    {
        WRITE(project, project->Title[i]);
    }
}

//=============================================================================

void report_writeOptions(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: writes analysis options in use to report file.
//
{
    char str[80];
    WRITE(project, "");
    WRITE(project, "*********************************************************");
    WRITE(project, "NOTE: The summary statistics displayed in this report are");
    WRITE(project, "based on results found at every computational time step,  ");
    WRITE(project, "not just on results from each reporting time step.");
    WRITE(project, "*********************************************************");
    WRITE(project, "");
    WRITE(project, "****************");
    WRITE(project, "Analysis Options");
    WRITE(project, "****************");
    fprintf(project->Frpt.file, "\n  Flow Units ............... %s",
        FlowUnitWords[project->FlowUnits]);
    fprintf(project->Frpt.file, "\n  Process Models:");
    fprintf(project->Frpt.file, "\n    Rainfall/Runoff ........ ");
    if ( project->IgnoreRainfall || project->Nobjects[GAGE] == 0 )
        fprintf(project->Frpt.file, "NO");
    else fprintf(project->Frpt.file, "YES");

    fprintf(project->Frpt.file, "\n    RDII ................... ");                     //(5.1.004)
    if ( project->IgnoreRDII || project->Nobjects[UNITHYD] == 0 )
        fprintf(project->Frpt.file, "NO");
    else fprintf(project->Frpt.file, "YES");

    fprintf(project->Frpt.file, "\n    project->Snowmelt ............... ");
    if ( project->IgnoreSnowmelt || project->Nobjects[SNOWMELT] == 0 )
        fprintf(project->Frpt.file, "NO");
    else fprintf(project->Frpt.file, "YES");
    fprintf(project->Frpt.file, "\n    Groundwater ............ ");
    if ( project->IgnoreGwater || project->Nobjects[AQUIFER] == 0 )
        fprintf(project->Frpt.file, "NO");
    else fprintf(project->Frpt.file, "YES");
    fprintf(project->Frpt.file, "\n    Flow Routing ........... ");
    if ( project->IgnoreRouting || project->Nobjects[LINK] == 0 )
        fprintf(project->Frpt.file, "NO");
    else
    {
        fprintf(project->Frpt.file, "YES");
        fprintf(project->Frpt.file, "\n    Ponding Allowed ........ ");
        if ( project->AllowPonding ) fprintf(project->Frpt.file, "YES");
        else                fprintf(project->Frpt.file, "NO");
    }
    fprintf(project->Frpt.file, "\n    Water Quality .......... ");
    if ( project->IgnoreQuality || project->Nobjects[POLLUT] == 0 )
        fprintf(project->Frpt.file, "NO");
    else fprintf(project->Frpt.file, "YES");

    if ( project->Nobjects[SUBCATCH] > 0 )
    fprintf(project->Frpt.file, "\n  Infiltration Method ...... %s", 
        InfilModelWords[project->InfilModel]);
    if ( project->Nobjects[LINK] > 0 )
    fprintf(project->Frpt.file, "\n  Flow Routing Method ...... %s",
        RouteModelWords[project->RouteModel]);
    datetime_dateToStr(project->StartDate, str);
    fprintf(project->Frpt.file, "\n  Starting Date ............ %s", str);
    datetime_timeToStr(project->StartTime, str);
    fprintf(project->Frpt.file, " %s", str);
    datetime_dateToStr(project->EndDate, str);
    fprintf(project->Frpt.file, "\n  Ending Date .............. %s", str);
    datetime_timeToStr(project->EndTime, str);
    fprintf(project->Frpt.file, " %s", str);
    fprintf(project->Frpt.file, "\n  Antecedent Dry Days ...... %.1f", project->StartDryDays);
    datetime_timeToStr(datetime_encodeTime(0, 0, project->ReportStep), str);
    fprintf(project->Frpt.file, "\n  Report Time Step ......... %s", str);
    if ( project->Nobjects[SUBCATCH] > 0 )
    {
        datetime_timeToStr(datetime_encodeTime(0, 0, project->WetStep), str);
        fprintf(project->Frpt.file, "\n  Wet Time Step ............ %s", str);
        datetime_timeToStr(datetime_encodeTime(0, 0, project->DryStep), str);
        fprintf(project->Frpt.file, "\n  Dry Time Step ............ %s", str);
    }
    if ( project->Nobjects[LINK] > 0 )
    {
        fprintf(project->Frpt.file, "\n  Routing Time Step ........ %.2f sec", project->RouteStep);
		if ( project->RouteModel == DW )
		{
		fprintf(project->Frpt.file, "\n  Variable Time Step ....... ");
		if ( project->CourantFactor > 0.0 ) fprintf(project->Frpt.file, "YES");
		else                       fprintf(project->Frpt.file, "NO");
		fprintf(project->Frpt.file, "\n  Maximum Trials ........... %d", project->MaxTrials);
		fprintf(project->Frpt.file, "\n  Head Tolerance ........... %.6f ", project->HeadTol);
		if ( project->UnitSystem == US ) fprintf(project->Frpt.file, "ft");
		else                    fprintf(project->Frpt.file, "m");
		}
    }
    WRITE(project, "");
}


//=============================================================================
//      RAINFALL FILE REPORTING
//=============================================================================

void report_writeRainStats(Project *project, int i, TRainStats* r)
//
//  Input:   i = rain gage index
//           r = rain file summary statistics
//  Output:  none
//  Purpose: writes summary of rain data read from file to report file.
//
{
    char date1[] = "***********";
    char date2[] = "***********";
    if ( i < 0 )
    {
        WRITE(project, "");
        WRITE(project, "*********************");
        WRITE(project, "Rainfall File Summary");
        WRITE(project, "*********************");
        fprintf(project->Frpt.file,
"\n  Station    First        Last         Recording   Periods    Periods    Periods");
        fprintf(project->Frpt.file,
"\n  ID         Date         Date         Frequency  w/Precip    Missing    Malfunc.");
        fprintf(project->Frpt.file,
"\n  -------------------------------------------------------------------------------\n");
    }
    else
    {
        if ( r->startDate != NO_DATE ) datetime_dateToStr(r->startDate, date1);
        if ( r->endDate   != NO_DATE ) datetime_dateToStr(r->endDate, date2);
        fprintf(project->Frpt.file, "  %-10s %-11s  %-11s  %5d min    %6d     %6d     %6d\n",
            project->Gage[i].staID, date1, date2, project->Gage[i].rainInterval/60,
            r->periodsRain, r->periodsMissing, r->periodsMalfunc);
    }        
}


//=============================================================================
//      RDII REPORTING
//=============================================================================

void report_writeRdiiStats(Project *project, double rainVol, double rdiiVol)
//
//  Input:   rainVol = total rainfall volume over sewershed
//           rdiiVol = total RDII volume produced
//  Output:  none
//  Purpose: writes summary of RDII inflow to report file.
//
{
    double ratio;
    double ucf1, ucf2;

    ucf1 = UCF(project, LENGTH) * UCF(project, LANDAREA);
    if ( project->UnitSystem == US) ucf2 = MGDperCFS / SECperDAY;
    else                   ucf2 = MLDperCFS / SECperDAY;

    WRITE(project, "");
    fprintf(project->Frpt.file,
    "\n  **********************           Volume        Volume");
    if ( project->UnitSystem == US) fprintf(project->Frpt.file,
    "\n  Rainfall Dependent I/I        acre-feet      10^6 gal");
    else fprintf(project->Frpt.file,
    "\n  Rainfall Dependent I/I        hectare-m      10^6 ltr");
    fprintf(project->Frpt.file,
    "\n  **********************        ---------     ---------");

    fprintf(project->Frpt.file, "\n  Sewershed Rainfall ......%14.3f%14.3f",
            rainVol * ucf1, rainVol * ucf2);

    fprintf(project->Frpt.file, "\n  RDII Produced ...........%14.3f%14.3f",
            rdiiVol * ucf1, rdiiVol * ucf2);

    if ( rainVol == 0.0 ) ratio = 0.0;
    else ratio = rdiiVol / rainVol;
    fprintf(project->Frpt.file, "\n  RDII Ratio ..............%14.3f", ratio);
    WRITE(project, "");
}


//=============================================================================
//      CONTROL ACTIONS REPORTING
//=============================================================================

void   report_writeControlActionsHeading(Project *project)
{
    WRITE(project, "");
    WRITE(project, "*********************");
    WRITE(project, "Control Actions Taken");
    WRITE(project, "*********************");
    fprintf(project->Frpt.file, "\n");
}

//=============================================================================

void   report_writeControlAction(Project *project, DateTime aDate, char* linkID, double value,
                                 char* ruleID)
//
//  Input:   aDate  = date/time of rule action
//           linkID = ID of link being controlled
//           value  = new status value of link
//           ruleID = ID of rule implementing the action
//  Output:  none
//  Purpose: reports action taken by a control rule.
//
{
    char     theDate[12];
    char     theTime[9];
    datetime_dateToStr(aDate, theDate);
    datetime_timeToStr(aDate, theTime);
    fprintf(project->Frpt.file,
            "  %11s: %8s project->Link %s setting changed to %6.2f by Control %s\n",
            theDate, theTime, linkID, value, ruleID);
}


//=============================================================================
//      CONTINUITY ERROR REPORTING
//=============================================================================

void report_writeRunoffError(Project *project, TRunoffTotals* totals, double totalArea)
//
//  Input:  totals = accumulated runoff totals
//          totalArea = total area of all subcatchments
//  Output:  none
//  Purpose: writes runoff continuity error to report file. 
//
{

    if ( project->Frunoff.mode == USE_FILE )
    {
        WRITE(project, "");
        fprintf(project->Frpt.file,
        "\n  **************************"
        "\n  Runoff Quantity Continuity"
        "\n  **************************"
        "\n  Runoff supplied by interface file %s", project->Frunoff.name);
        WRITE(project, "");
        return;
    }

    if ( totalArea == 0.0 ) return;
    WRITE(project, "");

    fprintf(project->Frpt.file,
    "\n  **************************        Volume         Depth");
    if ( project->UnitSystem == US) fprintf(project->Frpt.file,
    "\n  Runoff Quantity Continuity     acre-feet        inches");
    else fprintf(project->Frpt.file,
    "\n  Runoff Quantity Continuity     hectare-m            mm");
    fprintf(project->Frpt.file,
    "\n  **************************     ---------       -------");

    if ( totals->initStorage > 0.0 )
    {
        fprintf(project->Frpt.file, "\n  Initial LID project->Storage ......%14.3f%14.3f",
            totals->initStorage * UCF(project, LENGTH) * UCF(project, LANDAREA),
            totals->initStorage / totalArea * UCF(project, RAINDEPTH));
    }

    if ( project->Nobjects[SNOWMELT] > 0 )
    {
        fprintf(project->Frpt.file, "\n  Initial project->Snow Cover .......%14.3f%14.3f",
            totals->initSnowCover * UCF(project, LENGTH) * UCF(project, LANDAREA),
            totals->initSnowCover / totalArea * UCF(project, RAINDEPTH));
    }

    fprintf(project->Frpt.file, "\n  Total Precipitation ......%14.3f%14.3f",
            totals->rainfall * UCF(project, LENGTH) * UCF(project, LANDAREA),
            totals->rainfall / totalArea * UCF(project, RAINDEPTH));

    fprintf(project->Frpt.file, "\n  Evaporation Loss .........%14.3f%14.3f",
            totals->evap * UCF(project, LENGTH) * UCF(project, LANDAREA),
            totals->evap / totalArea * UCF(project, RAINDEPTH));

    fprintf(project->Frpt.file, "\n  Infiltration Loss ........%14.3f%14.3f",
            totals->infil * UCF(project, LENGTH) * UCF(project, LANDAREA),
            totals->infil / totalArea * UCF(project, RAINDEPTH));

    fprintf(project->Frpt.file, "\n  Surface Runoff ...........%14.3f%14.3f",
            totals->runoff * UCF(project, LENGTH) * UCF(project, LANDAREA),
            totals->runoff / totalArea * UCF(project, RAINDEPTH));

    if ( project->Nobjects[SNOWMELT] > 0 )
    {
        fprintf(project->Frpt.file, "\n  project->Snow Removed .............%14.3f%14.3f",
            totals->snowRemoved * UCF(project, LENGTH) * UCF(project, LANDAREA),
            totals->snowRemoved / totalArea * UCF(project, RAINDEPTH));
        fprintf(project->Frpt.file, "\n  Final project->Snow Cover .........%14.3f%14.3f",
            totals->finalSnowCover * UCF(project, LENGTH) * UCF(project, LANDAREA),
            totals->finalSnowCover / totalArea * UCF(project, RAINDEPTH));
    }

    fprintf(project->Frpt.file, "\n  Final Surface project->Storage ....%14.3f%14.3f",
            totals->finalStorage * UCF(project, LENGTH) * UCF(project, LANDAREA),
            totals->finalStorage / totalArea * UCF(project, RAINDEPTH));

    fprintf(project->Frpt.file, "\n  Continuity Error (%%) .....%14.3f",
            totals->pctError);
    WRITE(project, "");
}

//=============================================================================

void report_writeLoadingError(Project *project, TLoadingTotals* totals)
//
//  Input:   totals = accumulated pollutant loading totals
//           area = total area of all subcatchments
//  Output:  none
//  Purpose: writes runoff loading continuity error to report file. 
//
{
    int p1, p2;
    p1 = 1;
    p2 = MIN(5, project->Nobjects[POLLUT]);
    while ( p1 <= project->Nobjects[POLLUT] )
    {
        report_LoadingErrors(project, p1-1, p2-1, totals);
        p1 = p2 + 1;
        p2 = p1 + 4;
        p2 = MIN(p2, project->Nobjects[POLLUT]);
    }
}

//=============================================================================

void report_LoadingErrors(Project *project, int p1, int p2, TLoadingTotals* totals)
//
//  Input:   p1 = index of first pollutant to report
//           p2 = index of last pollutant to report
//           totals = accumulated pollutant loading totals
//           area = total area of all subcatchments
//  Output:  none
//  Purpose: writes runoff loading continuity error to report file for
//           up to 5 pollutants at a time. 
//
{
    int    i;
    int    p;
    double cf = 1.0;
    char   units[15];

    WRITE(project, "");
    fprintf(project->Frpt.file, "\n  **************************");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14s", project->Pollut[p].ID);
    }
    fprintf(project->Frpt.file, "\n  Runoff Quality Continuity ");
    for (p = p1; p <= p2; p++)
    {
        i = project->UnitSystem;
        if ( project->Pollut[p].units == COUNT ) i = 2;
        strcpy(units, LoadUnitsWords[i]);
        fprintf(project->Frpt.file, "%14s", units);
    }
    fprintf(project->Frpt.file, "\n  **************************");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "    ----------");
    }

    fprintf(project->Frpt.file, "\n  Initial Buildup ..........");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", totals[p].initLoad*cf);
    }
    fprintf(project->Frpt.file, "\n  Surface Buildup ..........");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", totals[p].buildup*cf);
    }
    fprintf(project->Frpt.file, "\n  Wet Deposition ...........");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", totals[p].deposition*cf);
    }
    fprintf(project->Frpt.file, "\n  Sweeping Removal .........");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", totals[p].sweeping*cf);
    }
    fprintf(project->Frpt.file, "\n  Infiltration Loss ........");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", totals[p].infil*cf);
    }
    fprintf(project->Frpt.file, "\n  BMP Removal ..............");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", totals[p].bmpRemoval*cf);
    }
    fprintf(project->Frpt.file, "\n  Surface Runoff ...........");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", totals[p].runoff*cf);
    }
    fprintf(project->Frpt.file, "\n  Remaining Buildup ........");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", totals[p].finalLoad*cf);
    }
    fprintf(project->Frpt.file, "\n  Continuity Error (%%) .....");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", totals[p].pctError);
    }
    WRITE(project, "");
}

//=============================================================================

void report_writeGwaterError(Project *project, TGwaterTotals* totals, double gwArea)
//
//  Input:   totals = accumulated groundwater totals
//           gwArea = total area of all subcatchments with groundwater
//  Output:  none
//  Purpose: writes groundwater continuity error to report file. 
//
{
    WRITE(project, "");
    fprintf(project->Frpt.file,
    "\n  **************************        Volume         Depth");
    if ( project->UnitSystem == US) fprintf(project->Frpt.file,
    "\n  Groundwater Continuity         acre-feet        inches");
    else fprintf(project->Frpt.file,
    "\n  Groundwater Continuity         hectare-m            mm");
    fprintf(project->Frpt.file,
    "\n  **************************     ---------       -------");
    fprintf(project->Frpt.file, "\n  Initial project->Storage ..........%14.3f%14.3f",
            totals->initStorage * UCF(project, LENGTH) * UCF(project, LANDAREA),
            totals->initStorage / gwArea * UCF(project, RAINDEPTH));

    fprintf(project->Frpt.file, "\n  Infiltration .............%14.3f%14.3f", 
            totals->infil * UCF(project, LENGTH) * UCF(project, LANDAREA),
            totals->infil / gwArea * UCF(project, RAINDEPTH));

    fprintf(project->Frpt.file, "\n  Upper Zone ET ............%14.3f%14.3f", 
            totals->upperEvap * UCF(project, LENGTH) * UCF(project, LANDAREA),
            totals->upperEvap / gwArea * UCF(project, RAINDEPTH));

    fprintf(project->Frpt.file, "\n  Lower Zone ET ............%14.3f%14.3f", 
            totals->lowerEvap * UCF(project, LENGTH) * UCF(project, LANDAREA),
            totals->lowerEvap / gwArea * UCF(project, RAINDEPTH));

    fprintf(project->Frpt.file, "\n  Deep Percolation .........%14.3f%14.3f", 
            totals->lowerPerc * UCF(project, LENGTH) * UCF(project, LANDAREA),
            totals->lowerPerc / gwArea * UCF(project, RAINDEPTH));

    fprintf(project->Frpt.file, "\n  Groundwater Flow .........%14.3f%14.3f",
            totals->gwater * UCF(project, LENGTH) * UCF(project, LANDAREA),
            totals->gwater / gwArea * UCF(project, RAINDEPTH));

    fprintf(project->Frpt.file, "\n  Final project->Storage ............%14.3f%14.3f",
            totals->finalStorage * UCF(project, LENGTH) * UCF(project, LANDAREA),
            totals->finalStorage / gwArea * UCF(project, RAINDEPTH));

    fprintf(project->Frpt.file, "\n  Continuity Error (%%) .....%14.3f",
            totals->pctError);
    WRITE(project, "");
}

//=============================================================================

void report_writeFlowError(Project *project, TRoutingTotals *totals)
//
//  Input:  totals = accumulated flow routing totals
//  Output:  none
//  Purpose: writes flow routing continuity error to report file. 
//
{
    double ucf1, ucf2;

    ucf1 = UCF(project, LENGTH) * UCF(project, LANDAREA);
    if ( project->UnitSystem == US) ucf2 = MGDperCFS / SECperDAY;
    else                   ucf2 = MLDperCFS / SECperDAY;

    WRITE(project, "");
    fprintf(project->Frpt.file,
    "\n  **************************        Volume        Volume");
    if ( project->UnitSystem == US) fprintf(project->Frpt.file,
    "\n  Flow Routing Continuity        acre-feet      10^6 gal");
    else fprintf(project->Frpt.file,
    "\n  Flow Routing Continuity        hectare-m      10^6 ltr"); 
    fprintf(project->Frpt.file,
    "\n  **************************     ---------     ---------");

    fprintf(project->Frpt.file, "\n  Dry Weather Inflow .......%14.3f%14.3f",
            totals->dwInflow * ucf1, totals->dwInflow * ucf2);

    fprintf(project->Frpt.file, "\n  Wet Weather Inflow .......%14.3f%14.3f",
            totals->wwInflow * ucf1, totals->wwInflow * ucf2);

    fprintf(project->Frpt.file, "\n  Groundwater Inflow .......%14.3f%14.3f",
            totals->gwInflow * ucf1, totals->gwInflow * ucf2);

    fprintf(project->Frpt.file, "\n  RDII Inflow ..............%14.3f%14.3f",
            totals->iiInflow * ucf1, totals->iiInflow * ucf2);

    fprintf(project->Frpt.file, "\n  External Inflow ..........%14.3f%14.3f",
            totals->exInflow * ucf1, totals->exInflow * ucf2);

    fprintf(project->Frpt.file, "\n  External Outflow .........%14.3f%14.3f",
            totals->outflow * ucf1, totals->outflow * ucf2);

    fprintf(project->Frpt.file, "\n  Internal Outflow .........%14.3f%14.3f",
            totals->flooding * ucf1, totals->flooding * ucf2);

    fprintf(project->Frpt.file, "\n  Evaporation Loss .........%14.3f%14.3f",
            totals->evapLoss * ucf1, totals->evapLoss * ucf2);
    
    fprintf(project->Frpt.file, "\n  Seepage Loss .............%14.3f%14.3f",
            totals->seepLoss * ucf1, totals->seepLoss * ucf2);

    fprintf(project->Frpt.file, "\n  Initial Stored Volume ....%14.3f%14.3f",
            totals->initStorage * ucf1, totals->initStorage * ucf2);

    fprintf(project->Frpt.file, "\n  Final Stored Volume ......%14.3f%14.3f",
            totals->finalStorage * ucf1, totals->finalStorage * ucf2);

    fprintf(project->Frpt.file, "\n  Continuity Error (%%) .....%14.3f",
            totals->pctError);
    WRITE(project, "");
}

//=============================================================================

void report_writeQualError(Project *project, TRoutingTotals QualTotals[])
//
//  Input:   totals = accumulated quality routing totals for each pollutant
//  Output:  none
//  Purpose: writes quality routing continuity error to report file. 
//
{
    int p1, p2;
    p1 = 1;
    p2 = MIN(5, project->Nobjects[POLLUT]);
    while ( p1 <= project->Nobjects[POLLUT] )
    {
        report_QualErrors(project, p1-1, p2-1, QualTotals);
        p1 = p2 + 1;
        p2 = p1 + 4;
        p2 = MIN(p2, project->Nobjects[POLLUT]);
    }
}

//=============================================================================

void report_QualErrors(Project *project, int p1, int p2, TRoutingTotals QualTotals[])
{
    int   i;
    int   p;
    char  units[15];

    WRITE(project, "");
    fprintf(project->Frpt.file, "\n  **************************");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14s", project->Pollut[p].ID);
    }
    fprintf(project->Frpt.file, "\n  Quality Routing Continuity");
    for (p = p1; p <= p2; p++)
    {
        i = project->UnitSystem;
        if ( project->Pollut[p].units == COUNT ) i = 2;
        strcpy(units, LoadUnitsWords[i]);
        fprintf(project->Frpt.file, "%14s", units);
    }
    fprintf(project->Frpt.file, "\n  **************************");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "    ----------");
    }

    fprintf(project->Frpt.file, "\n  Dry Weather Inflow .......");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", QualTotals[p].dwInflow);
    }

    fprintf(project->Frpt.file, "\n  Wet Weather Inflow .......");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", QualTotals[p].wwInflow);
    }

    fprintf(project->Frpt.file, "\n  Groundwater Inflow .......");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", QualTotals[p].gwInflow);
    }

    fprintf(project->Frpt.file, "\n  RDII Inflow ..............");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", QualTotals[p].iiInflow);
    }

    fprintf(project->Frpt.file, "\n  External Inflow ..........");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", QualTotals[p].exInflow);
    }

    fprintf(project->Frpt.file, "\n  Internal Flooding ........");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", QualTotals[p].flooding);
    }

    fprintf(project->Frpt.file, "\n  External Outflow .........");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", QualTotals[p].outflow);
    }

    fprintf(project->Frpt.file, "\n  Mass Reacted .............");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", QualTotals[p].reacted);
    }

    fprintf(project->Frpt.file, "\n  Initial Stored Mass ......");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", QualTotals[p].initStorage);
    }

    fprintf(project->Frpt.file, "\n  Final Stored Mass ........");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", QualTotals[p].finalStorage);
    }

    fprintf(project->Frpt.file, "\n  Continuity Error (%%) .....");
    for (p = p1; p <= p2; p++)
    {
        fprintf(project->Frpt.file, "%14.3f", QualTotals[p].pctError);
    }
    WRITE(project, "");
}

//=============================================================================

void report_writeMaxStats(Project *project, TMaxStats maxMassBalErrs[], TMaxStats maxCourantCrit[],
                          int nMaxStats)
//
//  Input:   maxMassBal[] = nodes with highest mass balance errors
//           maxCourantCrit[] = nodes most often Courant time step critical
//           maxLinkTimes[] = links most often Courant time step critical
//           nMaxStats = number of most critical nodes/links saved
//  Output:  none
//  Purpose: lists nodes & links with highest mass balance errors and 
//           time Courant time step critical
//
{
    int i, j, k;

    if ( project->RouteModel != DW || project->Nobjects[LINK] == 0 ) return;
    if ( nMaxStats <= 0 ) return;
    if ( maxMassBalErrs[0].index >= 0 )
    {
        WRITE(project, "");
        WRITE(project, "*************************");
        WRITE(project, "Highest Continuity Errors");    
        WRITE(project, "*************************");
        for (i=0; i<nMaxStats; i++)
        {
            j = maxMassBalErrs[i].index;
            if ( j < 0 ) continue;
            fprintf(project->Frpt.file, "\n  project->Node %s (%.2f%%)",
                project->Node[j].ID, maxMassBalErrs[i].value);
        }
        WRITE(project, "");
    }

    if ( project->CourantFactor == 0.0 ) return;
    WRITE(project, "");
    WRITE(project, "***************************");
    WRITE(project, "Time-Step Critical Elements");    
    WRITE(project, "***************************");
    k = 0;
    for (i=0; i<nMaxStats; i++)
    {
        j = maxCourantCrit[i].index;
        if ( j < 0 ) continue;
        k++;
        if ( maxCourantCrit[i].objType == NODE )
             fprintf(project->Frpt.file, "\n  project->Node %s", project->Node[j].ID);
        else fprintf(project->Frpt.file, "\n  project->Link %s", project->Link[j].ID);
        fprintf(project->Frpt.file, " (%.2f%%)", maxCourantCrit[i].value);
    }
    if ( k == 0 ) fprintf(project->Frpt.file, "\n  None");
    WRITE(project, "");
}

//=============================================================================

void report_writeMaxFlowTurns(Project *project, TMaxStats flowTurns[], int nMaxStats)
//
//  Input:   flowTurns[] = links with highest number of flow turns
//           nMaxStats = number of links in flowTurns[]
//  Output:  none
//  Purpose: lists links with highest number of flow turns (i.e., fraction
//           of time periods where the flow is higher (or lower) than the
//           flows in the previous and following periods).
//
{
    int i, j;

    if ( project->Nobjects[LINK] == 0 ) return;
    WRITE(project, "");
    WRITE(project, "********************************");
    WRITE(project, "Highest Flow Instability Indexes"); 
    WRITE(project, "********************************");
    if ( nMaxStats <= 0 || flowTurns[0].index <= 0 )
        fprintf(project->Frpt.file, "\n  All links are stable.");
    else
    { 
        for (i=0; i<nMaxStats; i++)
        {
            j = flowTurns[i].index;
            if ( j < 0 ) continue;
            fprintf(project->Frpt.file, "\n  project->Link %s (%.0f)",
                project->Link[j].ID, flowTurns[i].value);
        }  
    }
    WRITE(project, "");
}

//=============================================================================

void report_writeSysStats(Project *project, TSysStats* sysStats)
//
//  Input:   sysStats = simulation statistics for overall system
//  Output:  none
//  Purpose: writes simulation statistics for overall system to report file.
//
{
    double x;

    if ( project->Nobjects[LINK] == 0 || project->StepCount == 0 ) return;
    WRITE(project, "");
    WRITE(project, "*************************");
    WRITE(project, "Routing Time Step Summary");
    WRITE(project, "*************************");
    fprintf(project->Frpt.file,
        "\n  Minimum Time Step           :  %7.2f sec",
        sysStats->minTimeStep);
    fprintf(project->Frpt.file,
        "\n  Average Time Step           :  %7.2f sec",
        sysStats->avgTimeStep / project->StepCount);
    fprintf(project->Frpt.file,
        "\n  Maximum Time Step           :  %7.2f sec",
        sysStats->maxTimeStep);
    x = sysStats->steadyStateCount / project->StepCount * 100.0;
    fprintf(project->Frpt.file,
        "\n  Percent in Steady State     :  %7.2f", MIN(x, 100.0));
    fprintf(project->Frpt.file,
        "\n  Average Iterations per Step :  %7.2f",
        sysStats->avgStepCount / project->StepCount);
    fprintf(project->Frpt.file,
        "\n  Percent Not Converging      :  %7.2f",
        100.0 * (double)project->NonConvergeCount / project->StepCount);
    WRITE(project, "");
}


//=============================================================================
//      SIMULATION RESULTS REPORTING
//=============================================================================

void report_writeReport(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: writes simulation results to report file.
//
{
    if ( project->ErrorCode ) return;
    if ( project->Nperiods == 0 ) return;
    if ( project->RptFlags.subcatchments != NONE
         && ( project->IgnoreRainfall == FALSE ||
              project->IgnoreSnowmelt == FALSE ||
              project->IgnoreGwater == FALSE)
       ) report_Subcatchments(project);

    if ( project->IgnoreRouting == TRUE && project->IgnoreQuality == TRUE ) return;
    if ( project->RptFlags.nodes != NONE ) report_Nodes(project);
    if ( project->RptFlags.links != NONE ) report_Links(project);
}

//=============================================================================

void report_Subcatchments(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: writes results for selected subcatchments to report file.
//
{
    int      j, p, k;
    int      period;
    DateTime days;
    char     theDate[12];
    char     theTime[9];
    int      hasSnowmelt = (project->Nobjects[SNOWMELT] > 0 && !project->IgnoreSnowmelt);
    int      hasGwater   = (project->Nobjects[AQUIFER] > 0  && !project->IgnoreGwater);
    int      hasQuality  = (project->Nobjects[POLLUT] > 0 && !project->IgnoreQuality);

    if ( project->Nobjects[SUBCATCH] == 0 ) return;
    WRITE(project, "");
    WRITE(project, "********************");
    WRITE(project, "Subcatchment Results");
    WRITE(project, "********************");
    k = 0;
    for (j = 0; j < project->Nobjects[SUBCATCH]; j++)
    {
        if ( project->Subcatch[j].rptFlag == TRUE ) 
        {
            report_SubcatchHeader(project, project->Subcatch[j].ID);
            for ( period = 1; period <= project->Nperiods; period++ )
            {
                output_readDateTime(project, period, &days);
                datetime_dateToStr(days, theDate);
                datetime_timeToStr(days, theTime);
                output_readSubcatchResults(project, period, k);
                fprintf(project->Frpt.file, "\n  %11s %8s %10.3f%10.3f%10.4f",
                    theDate, theTime, SubcatchResults[SUBCATCH_RAINFALL],
                    SubcatchResults[SUBCATCH_EVAP]/24.0 +
                    SubcatchResults[SUBCATCH_INFIL], 
                    SubcatchResults[SUBCATCH_RUNOFF]);
                if ( hasSnowmelt )
                    fprintf(project->Frpt.file, "  %10.3f",
                        SubcatchResults[SUBCATCH_SNOWDEPTH]);
                if ( hasGwater )
                    fprintf(project->Frpt.file, "%10.3f%10.4f",
                        SubcatchResults[SUBCATCH_GW_ELEV],
                        SubcatchResults[SUBCATCH_GW_FLOW]);
                if ( hasQuality )
                    for (p = 0; p < project->Nobjects[POLLUT]; p++)
                        fprintf(project->Frpt.file, "%10.3f",
                            SubcatchResults[SUBCATCH_WASHOFF+p]);
            }
            WRITE(project, "");
            k++;
        }
    }
}

//=============================================================================

void  report_SubcatchHeader(Project *project, char *id)
//
//  Input:   id = subcatchment ID name
//  Output:  none
//  Purpose: writes table headings for subcatchment results to report file.
//
{
    int i;
    int hasSnowmelt = (project->Nobjects[SNOWMELT] > 0 && !project->IgnoreSnowmelt);
    int hasGwater   = (project->Nobjects[AQUIFER] > 0  && !project->IgnoreGwater);
    int hasQuality  = (project->Nobjects[POLLUT] > 0 && !project->IgnoreQuality); 

    // --- print top border of header
    WRITE(project, "");
    fprintf(project->Frpt.file,"\n  <<< Subcatchment %s >>>", id);
    WRITE(project, LINE_51);
    if ( hasSnowmelt  > 0 ) fprintf(project->Frpt.file, LINE_12);
    if ( hasGwater )
    {
        fprintf(project->Frpt.file, LINE_10);
        fprintf(project->Frpt.file, LINE_10);
    }
    if ( hasQuality )
    {
        for (i = 0; i < project->Nobjects[POLLUT]; i++) fprintf(project->Frpt.file, LINE_10);
    }

    // --- print first line of column headings
    fprintf(project->Frpt.file,
    "\n  Date        Time        Precip.    Losses    Runoff");
    if ( hasSnowmelt ) fprintf(project->Frpt.file, "  project->Snow Depth");
    if ( hasGwater   ) fprintf(project->Frpt.file, "  GW Elev.   GW Flow");
    if ( hasQuality ) for (i = 0; i < project->Nobjects[POLLUT]; i++)
        fprintf(project->Frpt.file, "%10s", project->Pollut[i].ID);

    // --- print second line of column headings
    if ( project->UnitSystem == US ) fprintf(project->Frpt.file, 
    "\n                            in/hr     in/hr %9s", FlowUnitWords[project->FlowUnits]);
    else fprintf(project->Frpt.file, 
    "\n                            mm/hr     mm/hr %9s", FlowUnitWords[project->FlowUnits]);
    if ( hasSnowmelt )
    {
        if ( project->UnitSystem == US ) fprintf(project->Frpt.file, "      inches");
        else                    fprintf(project->Frpt.file, "     mmeters");
    }
    if ( hasGwater )
    {
        if ( project->UnitSystem == US )
            fprintf(project->Frpt.file, "      feet %9s", FlowUnitWords[project->FlowUnits]);
        else
            fprintf(project->Frpt.file, "    meters %9s", FlowUnitWords[project->FlowUnits]);
    }
    if ( hasQuality ) for (i = 0; i < project->Nobjects[POLLUT]; i++)
        fprintf(project->Frpt.file, "%10s", QualUnitsWords[project->Pollut[i].units]);

    // --- print lower border of header
    WRITE(project, LINE_51);
    if ( hasSnowmelt ) fprintf(project->Frpt.file, LINE_12);
    if ( hasGwater )
    {
        fprintf(project->Frpt.file, LINE_10);
        fprintf(project->Frpt.file, LINE_10);
    }
    if ( hasQuality ) for (i = 0; i < project->Nobjects[POLLUT]; i++)
        fprintf(project->Frpt.file, LINE_10);
}

//=============================================================================

void report_Nodes(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: writes results for selected nodes to report file.
//
{
    int      j, p, k;
    int      period;
    DateTime days;
    char     theDate[20];
    char     theTime[20];

    if ( project->Nobjects[NODE] == 0 ) return;
    WRITE(project, "");
    WRITE(project, "************");
    WRITE(project, "project->Node Results");
    WRITE(project, "************");
    k = 0;
    for (j = 0; j < project->Nobjects[NODE]; j++)
    {
        if ( project->Node[j].rptFlag == TRUE )
        {
            report_NodeHeader(project, project->Node[j].ID);
            for ( period = 1; period <= project->Nperiods; period++ )
            {
                output_readDateTime(project, period, &days);
                datetime_dateToStr(days, theDate);
                datetime_timeToStr(days, theTime);
                output_readNodeResults(project, period, k);
                fprintf(project->Frpt.file, "\n  %11s %8s  %9.3f %9.3f %9.3f %9.3f",
                    theDate, theTime, NodeResults[NODE_INFLOW],
                    NodeResults[NODE_OVERFLOW], NodeResults[NODE_DEPTH],
                    NodeResults[NODE_HEAD]);
                if ( !project->IgnoreQuality ) for (p = 0; p < project->Nobjects[POLLUT]; p++)
                    fprintf(project->Frpt.file, " %9.3f", NodeResults[NODE_QUAL + p]);
            }
            WRITE(project, "");
            k++;
        }
    }
}

//=============================================================================

void  report_NodeHeader(Project *project, char *id)
//
//  Input:   id = node ID name
//  Output:  none
//  Purpose: writes table headings for node results to report file.
//
{
    int i;
    char lengthUnits[9];
    WRITE(project, "");
    fprintf(project->Frpt.file,"\n  <<< project->Node %s >>>", id);
    WRITE(project, LINE_64);
    for (i = 0; i < project->Nobjects[POLLUT]; i++) fprintf(project->Frpt.file, LINE_10);

    fprintf(project->Frpt.file,
    "\n                           Inflow  Flooding     Depth      Head");
    if ( !project->IgnoreQuality ) for (i = 0; i < project->Nobjects[POLLUT]; i++)
        fprintf(project->Frpt.file, "%10s", project->Pollut[i].ID);
    if ( project->UnitSystem == US) strcpy(lengthUnits, "feet");
    else strcpy(lengthUnits, "meters");
    fprintf(project->Frpt.file,
    "\n  Date        Time      %9s %9s %9s %9s",
        FlowUnitWords[project->FlowUnits], FlowUnitWords[project->FlowUnits],
        lengthUnits, lengthUnits);
    if ( !project->IgnoreQuality ) for (i = 0; i < project->Nobjects[POLLUT]; i++)
        fprintf(project->Frpt.file, "%10s", QualUnitsWords[project->Pollut[i].units]);

    WRITE(project, LINE_64);
    if ( !project->IgnoreQuality )
        for (i = 0; i < project->Nobjects[POLLUT]; i++) fprintf(project->Frpt.file, LINE_10);
}

//=============================================================================

void report_Links(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: writes results for selected links to report file.
//
{
    int      j, p, k; 
    int      period;
    DateTime days;
    char     theDate[12];
    char     theTime[9];

    if ( project->Nobjects[LINK] == 0 ) return;
    WRITE(project, "");
    WRITE(project, "************");
    WRITE(project, "project->Link Results");
    WRITE(project, "************");
    k = 0;
    for (j = 0; j < project->Nobjects[LINK]; j++)
    {
        if ( project->Link[j].rptFlag == TRUE )
        {
            report_LinkHeader(project, project->Link[j].ID);
            for ( period = 1; period <= project->Nperiods; period++ )
            {
                output_readDateTime(project, period, &days);
                datetime_dateToStr(days, theDate);
                datetime_timeToStr(days, theTime);
                output_readLinkResults(project, period, k);
                fprintf(project->Frpt.file, "\n  %11s %8s  %9.3f %9.3f %9.3f %9.3f",
                    theDate, theTime, LinkResults[LINK_FLOW], 
                    LinkResults[LINK_VELOCITY], LinkResults[LINK_DEPTH],
                    LinkResults[LINK_CAPACITY]);
                if ( !project->IgnoreQuality ) for (p = 0; p < project->Nobjects[POLLUT]; p++)
                    fprintf(project->Frpt.file, " %9.3f", LinkResults[LINK_QUAL + p]);
            }
            WRITE(project, "");
            k++; 
        }
    }
}

//=============================================================================

void  report_LinkHeader(Project *project, char *id)
//
//  Input:   id = link ID name
//  Output:  none
//  Purpose: writes table headings for link results to report file.
//
{
    int i;
    WRITE(project, "");
    fprintf(project->Frpt.file,"\n  <<< project->Link %s >>>", id);
    WRITE(project, LINE_64); 
    for (i = 0; i < project->Nobjects[POLLUT]; i++) fprintf(project->Frpt.file, LINE_10);

    fprintf(project->Frpt.file, 
    "\n                             Flow  Velocity     Depth  Capacity/");
    if ( !project->IgnoreQuality ) for (i = 0; i < project->Nobjects[POLLUT]; i++)
        fprintf(project->Frpt.file, "%10s", project->Pollut[i].ID);

    if ( project->UnitSystem == US )
        fprintf(project->Frpt.file,
        "\n  Date        Time     %10s    ft/sec      feet   Setting ",
        FlowUnitWords[project->FlowUnits]);
    else
        fprintf(project->Frpt.file,
        "\n  Date        Time     %10s     m/sec    meters   Setting ",
        FlowUnitWords[project->FlowUnits]);
    if ( !project->IgnoreQuality ) for (i = 0; i < project->Nobjects[POLLUT]; i++)
        fprintf(project->Frpt.file, " %9s", QualUnitsWords[project->Pollut[i].units]);

    WRITE(project, LINE_64); 
    if ( !project->IgnoreQuality )
        for (i = 0; i < project->Nobjects[POLLUT]; i++) fprintf(project->Frpt.file, LINE_10);
}


//=============================================================================
//      ERROR REPORTING
//=============================================================================

void report_writeErrorMsg(Project *project, int code, char* s)
//
//  Input:   code = error code
//           s = error message text
//  Output:  none
//  Purpose: writes error message to report file.
//
{
    if ( project->Frpt.file )
    {
        WRITE(project, "");
        fprintf(project->Frpt.file, error_getMsg(code), s);
    }
    project->ErrorCode = code;
}

//=============================================================================

void report_writeErrorCode(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: writes error message to report file.
//
{
    if ( project->Frpt.file )
    {
        if ( (project->ErrorCode >= ERR_MEMORY && project->ErrorCode <= ERR_TIMESTEP)
        ||   (project->ErrorCode >= ERR_FILE_NAME && project->ErrorCode <= ERR_OUT_FILE)
        ||   (project->ErrorCode == ERR_SYSTEM) )
            fprintf(project->Frpt.file, error_getMsg(project->ErrorCode));
    }
}

//=============================================================================

void report_writeInputErrorMsg(Project *project, int k, int sect, char* line, long lineCount)
//
//  Input:   k = error code
//           sect = number of input data section where error occurred
//           line = line of data containing the error
//           lineCount = line number of data file containing the error
//  Output:  none
//  Purpose: writes input error message to report file.
//
{
    if ( project->Frpt.file )
    {
        report_writeErrorMsg(project, k, ErrString);
        if ( sect < 0 ) fprintf(project->Frpt.file, FMT17, lineCount);
        else            fprintf(project->Frpt.file, FMT18, lineCount, SectWords[sect]);
        fprintf(project->Frpt.file, "\n  %s", line);
    }
}

//=============================================================================

void report_writeWarningMsg(Project *project, char* msg, char* id)
//
//  Input:   msg = text of warning message
//           id = ID name of object that message refers to
//  Output:  none
//  Purpose: writes a warning message to the report file.
//
{
    fprintf(project->Frpt.file, "\n  %s %s", msg, id);
}

//=============================================================================

void report_writeTseriesErrorMsg(Project *project, int code, TTable *tseries)
//
//  Input:   tseries = pointer to a time series
//  Output:  none
//  Purpose: writes the date where a time series' data is out of order.
//
{
    char     theDate[20];
    char     theTime[20];
    DateTime x;

    if (code == ERR_CURVE_SEQUENCE)
    {
        x = tseries->x2;
        datetime_dateToStr(x, theDate);
        datetime_timeToStr(x, theTime);
        report_writeErrorMsg(project, ERR_TIMESERIES_SEQUENCE, tseries->ID);
        fprintf(project->Frpt.file, " at %s %s.", theDate, theTime);
    }
    else report_writeErrorMsg(project, code, tseries->ID);
} 
