//-----------------------------------------------------------------------------
//   output.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/20/14  (Build 5.1.001)
//   Author:   L. Rossman (EPA)
//
//   Binary output file access functions.
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE


#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "headers.h"




enum InputDataType {INPUT_TYPE_CODE, INPUT_AREA, INPUT_INVERT, INPUT_MAX_DEPTH,
                    INPUT_OFFSET, INPUT_LENGTH};

//-----------------------------------------------------------------------------
//  Shared variables    
//-----------------------------------------------------------------------------
//static INT4      IDStartPos;           // starting file position of ID names
//static INT4      InputStartPos;        // starting file position of input data
//static INT4      OutputStartPos;       // starting file position of output data
//static INT4      BytesPerPeriod;       // bytes saved per simulation time period
//static INT4      NsubcatchResults;     // number of subcatchment output variables
//static INT4      NnodeResults;         // number of node output variables
//static INT4      NlinkResults;         // number of link output variables
//static INT4      NumSubcatch;          // number of subcatchments reported on
//static INT4      NumNodes;             // number of nodes reported on
//static INT4      NumLinks;             // number of links reported on
//static INT4      NumPolluts;           // number of pollutants reported on
//static REAL4     SysResults[MAX_SYS_RESULTS];    // values of system output vars.
//
////-----------------------------------------------------------------------------
////  Exportable variables (shared with report.c)
////-----------------------------------------------------------------------------
//REAL4*           SubcatchResults;
//REAL4*           NodeResults;
//REAL4*           LinkResults;


//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------
static void output_openOutFile(Project* project);
static void output_saveID(char* id, FILE* file);
static void output_saveSubcatchResults(Project* project, double reportTime, FILE* file);
static void output_saveNodeResults(Project* project, double reportTime, FILE* file);
static void output_saveLinkResults(Project* project ,double reportTime, FILE* file);

//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)
//-----------------------------------------------------------------------------
//  output_open                   (called by swmm_start in swmm5.c)
//  output_end                    (called by swmm_end in swmm5.c)
//  output_close                  (called by swmm_close in swmm5.c)
//  output_saveResults            (called by swmm_step in swmm5.c)
//  output_checkFileSize          (called by swmm_report)
//  output_readDateTime           (called by routines in report.c)
//  output_readSubcatchResults    (called by report_Subcatchments)
//  output_readNodeResults        (called by report_Nodes)
//  output_readLinkResults        (called by report_Links)


//=============================================================================

int output_open(Project* project)
//
//  Input:   none
//  Output:  returns an error code
//  Purpose: writes basic project data to binary output file.
//
{
    int   j;
    int   m;
    INT4  k;
    REAL4 x;
    REAL8 z;

    // --- open binary output file
    output_openOutFile(project);
    if ( project->ErrorCode ) return project->ErrorCode;

    // --- ignore pollutants if no water quality analsis performed
    if ( project->IgnoreQuality ) project->NumPolluts = 0;
    else project->NumPolluts = project->Nobjects[POLLUT];

    // --- subcatchment results consist of Rainfall, Snowdepth, project->Evap, 
    //     Infil, Runoff, GW Flow, GW Elev, GW Sat, and Washoff
    project->NsubcatchResults = MAX_SUBCATCH_RESULTS - 1 + project->NumPolluts;

    // --- node results consist of Depth, Head, Volume, Lateral Inflow,
    //     Total Inflow, Overflow and Quality
    project->NnodeResults = MAX_NODE_RESULTS - 1 + project->NumPolluts;

    // --- link results consist of Depth, Flow, Velocity, Froude No.,
    //     Capacity and Quality
    project->NlinkResults = MAX_LINK_RESULTS - 1 + project->NumPolluts;

    // --- get number of objects reported on
    project->NumSubcatch = 0;
    project->NumNodes = 0;
    project->NumLinks = 0;
    for (j=0; j<project->Nobjects[SUBCATCH]; j++) if (project->Subcatch[j].rptFlag) project->NumSubcatch++;
    for (j=0; j<project->Nobjects[NODE]; j++) if (project->Node[j].rptFlag) project->NumNodes++;
    for (j=0; j<project->Nobjects[LINK]; j++) if (project->Link[j].rptFlag) project->NumLinks++;

    project->BytesPerPeriod = sizeof(REAL8)
        + project->NumSubcatch * project->NsubcatchResults * sizeof(REAL4)
        + project->NumNodes * project->NnodeResults * sizeof(REAL4)
        + project->NumLinks * project->NlinkResults * sizeof(REAL4)
        + MAX_SYS_RESULTS * sizeof(REAL4);
    project->Nperiods = 0;

    project->SubcatchResults = NULL;
    project->NodeResults = NULL;
    project->LinkResults = NULL;
    project->SubcatchResults = (REAL4 *) calloc(project->NsubcatchResults, sizeof(REAL4));
    project->NodeResults = (REAL4 *) calloc(project->NnodeResults, sizeof(REAL4));
    project->LinkResults = (REAL4 *) calloc(project->NlinkResults, sizeof(REAL4));
    if ( !project->SubcatchResults || !project->NodeResults || !project->LinkResults )
    {
        report_writeErrorMsg(project,ERR_MEMORY, "");
        return project->ErrorCode;
    }

    fseek(project->Fout.file, 0, SEEK_SET);
    k = MAGICNUMBER;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);   // Magic number
    k = VERSION;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);   // Version number
    k = project->FlowUnits;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);   // Flow units
    k = project->NumSubcatch;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);   // # subcatchments
    k = project->NumNodes;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);   // # nodes
    k = project->NumLinks;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);   // # links
    k = project->NumPolluts;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);   // # pollutants

    // --- save ID names of subcatchments, nodes, links, & pollutants 
    project->IDStartPos = ftell(project->Fout.file);
    for (j=0; j<project->Nobjects[SUBCATCH]; j++)
    {
        if ( project->Subcatch[j].rptFlag ) output_saveID(project->Subcatch[j].ID, project->Fout.file);
    }
    for (j=0; j<project->Nobjects[NODE];     j++)
    {
        if ( project->Node[j].rptFlag ) output_saveID(project->Node[j].ID, project->Fout.file);
    }
    for (j=0; j<project->Nobjects[LINK];     j++)
    {
        if ( project->Link[j].rptFlag ) output_saveID(project->Link[j].ID, project->Fout.file);
    }
    for (j=0; j<project->NumPolluts; j++) output_saveID(project->Pollut[j].ID, project->Fout.file);

    // --- save codes of pollutant concentration units
    for (j=0; j<project->NumPolluts; j++)
    {
        k = project->Pollut[j].units;
        fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    }

    project->InputStartPos = ftell(project->Fout.file);

    // --- save subcatchment area
    k = 1;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = INPUT_AREA;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    for (j=0; j<project->Nobjects[SUBCATCH]; j++)
    {
         if ( !project->Subcatch[j].rptFlag ) continue;
         project->SubcatchResults[0] = (REAL4)(project->Subcatch[j].area * UCF(project, LANDAREA));
         fwrite(&project->SubcatchResults[0], sizeof(REAL4), 1, project->Fout.file);
    }

    // --- save node type, invert, & max. depth
    k = 3;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = INPUT_TYPE_CODE;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = INPUT_INVERT;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = INPUT_MAX_DEPTH;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    for (j=0; j<project->Nobjects[NODE]; j++)
    {
        if ( !project->Node[j].rptFlag ) continue;
        k = project->Node[j].type;
        project->NodeResults[0] = (REAL4)(project->Node[j].invertElev * UCF(project, LENGTH));
        project->NodeResults[1] = (REAL4)(project->Node[j].fullDepth * UCF(project, LENGTH));
        fwrite(&k, sizeof(INT4), 1, project->Fout.file);
        fwrite(project->NodeResults, sizeof(REAL4), 2, project->Fout.file);
    }

    // --- save link type, offsets, max. depth, & length
    k = 5;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = INPUT_TYPE_CODE;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = INPUT_OFFSET;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = INPUT_OFFSET;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = INPUT_MAX_DEPTH;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = INPUT_LENGTH;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);

    for (j=0; j<project->Nobjects[LINK]; j++)
    {
        if ( !project->Link[j].rptFlag ) continue;
        k = project->Link[j].type;
        if ( k == PUMP )
        {
            for (m=0; m<4; m++) project->LinkResults[m] = 0.0f;
        }
        else
        {
            project->LinkResults[0] = (REAL4)(project->Link[j].offset1 * UCF(project, LENGTH));
            project->LinkResults[1] = (REAL4)(project->Link[j].offset2 * UCF(project, LENGTH));
            if ( project->Link[j].direction < 0 )
            {
                x = project->LinkResults[0];
                project->LinkResults[0] = project->LinkResults[1];
                project->LinkResults[1] = x;
            }
            if ( k == OUTLET ) project->LinkResults[2] = 0.0f;
            else project->LinkResults[2] = (REAL4)(project->Link[j].xsect.yFull * UCF(project, LENGTH));
            if ( k == CONDUIT )
            {
                m = project->Link[j].subIndex;
                project->LinkResults[3] = (REAL4)(project->Conduit[m].length * UCF(project, LENGTH));
            }
            else project->LinkResults[3] = 0.0f;
        }
        fwrite(&k, sizeof(INT4), 1, project->Fout.file);
        fwrite(project->LinkResults, sizeof(REAL4), 4, project->Fout.file);
    }

    // --- save number & codes of subcatchment result variables
    k = project->NsubcatchResults;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = SUBCATCH_RAINFALL;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = SUBCATCH_SNOWDEPTH;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = SUBCATCH_EVAP;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = SUBCATCH_INFIL;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = SUBCATCH_RUNOFF;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = SUBCATCH_GW_FLOW;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = SUBCATCH_GW_ELEV;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = SUBCATCH_SOIL_MOIST;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);

    for (j=0; j<project->NumPolluts; j++) 
    {
        k = SUBCATCH_WASHOFF + j;
        fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    }

    // --- save number & codes of node result variables
    k = project->NnodeResults;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = NODE_DEPTH;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = NODE_HEAD;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = NODE_VOLUME;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = NODE_LATFLOW;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = NODE_INFLOW;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = NODE_OVERFLOW;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    for (j=0; j<project->NumPolluts; j++)
    {
        k = NODE_QUAL + j;
        fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    }

    // --- save number & codes of link result variables
    k = project->NlinkResults;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = LINK_FLOW;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = LINK_DEPTH;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = LINK_VELOCITY;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = LINK_VOLUME;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = LINK_CAPACITY;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    for (j=0; j<project->NumPolluts; j++)
    {
        k = LINK_QUAL + j;
        fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    }

    // --- save number & codes of system result variables
    k = MAX_SYS_RESULTS;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    for (k=0; k<MAX_SYS_RESULTS; k++) fwrite(&k, sizeof(INT4), 1, project->Fout.file);

    // --- save starting report date & report step
    //     (if reporting start date > simulation start date then
    //      make saved starting report date one reporting period
    //      prior to the date of the first reported result)
    z = (double)project->ReportStep/86400.0;
    if ( project->StartDateTime + z > project->ReportStart ) z = project->StartDateTime;
    else
    {
        z = floor((project->ReportStart - project->StartDateTime)/z) - 1.0;
        z = project->StartDateTime + z*(double)project->ReportStep/86400.0;
    }
    fwrite(&z, sizeof(REAL8), 1, project->Fout.file);
    k = project->ReportStep;
    if ( fwrite(&k, sizeof(INT4), 1, project->Fout.file) < 1)
    {
        report_writeErrorMsg(project, ERR_OUT_WRITE, "");
        return project->ErrorCode;
    }
    project->OutputStartPos = ftell(project->Fout.file);
    if ( project->Fout.mode == SCRATCH_FILE ) output_checkFileSize(project);
    return project->ErrorCode;
}

//=============================================================================

void  output_checkFileSize(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: checks if the size of the binary output file will be too big
//           to access using an integer file pointer variable.
//
{
    if ( project->RptFlags.subcatchments != NONE ||
         project->RptFlags.nodes != NONE ||
         project->RptFlags.links != NONE )
    {
        if ( (double)project->OutputStartPos + (double)project->BytesPerPeriod * project->TotalDuration
             / 1000.0 / (double)project->ReportStep >= (double)MAXFILESIZE )
        {
            report_writeErrorMsg(project ,ERR_FILE_SIZE, "");
        }
    }
}


//=============================================================================

void output_openOutFile(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: opens a project's binary output file.
//
{
    // --- close output file if already opened
    if (project->Fout.file != NULL) fclose(project->Fout.file); 

    // --- else if file name supplied then set file mode to SAVE
    else if (strlen(project->Fout.name) != 0) project->Fout.mode = SAVE_FILE;

    // --- otherwise set file mode to SCRATCH & generate a name
    else
    {
        project->Fout.mode = SCRATCH_FILE;
        getTempFileName(project,project->Fout.name);
    }

    // --- try to open the file
    if ( (project->Fout.file = fopen(project->Fout.name, "w+b")) == NULL)
    {
        writecon(FMT14);
        project->ErrorCode = ERR_OUT_FILE;
    }
}

//=============================================================================

void output_saveResults(Project* project,double reportTime)
//
//  Input:   reportTime = elapsed simulation time (millisec)
//  Output:  none
//  Purpose: writes computed results for current report time to binary file.
//
{
    int i;
    DateTime reportDate = getDateTime(project,reportTime);
    REAL8 date;

    if ( reportDate < project->ReportStart ) return;
    for (i=0; i<MAX_SYS_RESULTS; i++) project->SysResults[i] = 0.0f;
    date = reportDate;
    fwrite(&date, sizeof(REAL8), 1, project->Fout.file);
    if (project->Nobjects[SUBCATCH] > 0)
        output_saveSubcatchResults(project,reportTime, project->Fout.file);
    if (project->Nobjects[NODE] > 0)
        output_saveNodeResults(project , reportTime, project->Fout.file);
    if (project->Nobjects[LINK] > 0)
        output_saveLinkResults(project , reportTime, project->Fout.file);
    fwrite(project->SysResults, sizeof(REAL4), MAX_SYS_RESULTS, project->Fout.file);
    if ( project->Foutflows.mode == SAVE_FILE && !project->IgnoreRouting ) 
        iface_saveOutletResults(project,reportDate, project->Foutflows.file);
    project->Nperiods++;
}

//=============================================================================

void output_end(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: writes closing records to binary file.
//
{
    INT4 k;
    fwrite(&project->IDStartPos, sizeof(INT4), 1, project->Fout.file);
    fwrite(&project->InputStartPos, sizeof(INT4), 1, project->Fout.file);
    fwrite(&project->OutputStartPos, sizeof(INT4), 1, project->Fout.file);
    k = project->Nperiods;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = (INT4)error_getCode(project->ErrorCode);
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = MAGICNUMBER;
    if (fwrite(&k, sizeof(INT4), 1, project->Fout.file) < 1)
    {
        report_writeErrorMsg(project,ERR_OUT_WRITE, "");
    }
}

//=============================================================================

void output_close(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: frees memory used for accessing the binary file.
//
{
    FREE(project->SubcatchResults);
    FREE(project->NodeResults);
    FREE(project->LinkResults);
}

//=============================================================================

void output_saveID(char* id, FILE* file)
//
//  Input:   id = name of an object
//           file = ptr. to binary output file
//  Output:  none
//  Purpose: writes an object's name to the binary output file.
//
{
    INT4 n = strlen(id);
    fwrite(&n, sizeof(INT4), 1, file);
    fwrite(id, sizeof(char), n, file);
}

//=============================================================================

void output_saveSubcatchResults(Project* project, double reportTime, FILE* file)
//
//  Input:   reportTime = elapsed simulation time (millisec)
//           file = ptr. to binary output file
//  Output:  none
//  Purpose: writes computed subcatchment results to binary file.
//
{
    int      j;
    double   f;
    double   area;
    REAL4    totalArea = 0.0f; 
    DateTime reportDate = getDateTime(project, reportTime);

    // --- update reported rainfall at each rain gage
    for ( j=0; j<project->Nobjects[GAGE]; j++ )
    {
        gage_setReportRainfall(project,j, reportDate);
    }

    // --- find where current reporting time lies between latest runoff times
    f = (reportTime - project->OldRunoffTime) / (project->NewRunoffTime - project->OldRunoffTime);

    // --- write subcatchment results to file
    for ( j=0; j<project->Nobjects[SUBCATCH]; j++)
    {
        // --- retrieve interpolated results for reporting time & write to file
        subcatch_getResults(project,j, f, project->SubcatchResults);
        if ( project->Subcatch[j].rptFlag )
            fwrite(project->SubcatchResults, sizeof(REAL4), project->NsubcatchResults, file);

        // --- update system-wide results
        area = project->Subcatch[j].area * UCF(project, LANDAREA);
        totalArea += (REAL4)area;
        project->SysResults[SYS_RAINFALL] +=
            (REAL4)(project->SubcatchResults[SUBCATCH_RAINFALL] * area);
        project->SysResults[SYS_SNOWDEPTH] +=
            (REAL4)(project->SubcatchResults[SUBCATCH_SNOWDEPTH] * area);
        project->SysResults[SYS_EVAP] +=
            (REAL4)(project->SubcatchResults[SUBCATCH_EVAP] * area);
        if ( project->Subcatch[j].groundwater ) project->SysResults[SYS_EVAP] += 
            (REAL4)(project->Subcatch[j].groundwater->evapLoss * UCF(project, EVAPRATE) * area);
        project->SysResults[SYS_INFIL] +=
            (REAL4)(project->SubcatchResults[SUBCATCH_INFIL] * area);
        project->SysResults[SYS_RUNOFF] += (REAL4)project->SubcatchResults[SUBCATCH_RUNOFF];
    }

    // --- normalize system-wide results to catchment area
    if ( project->UnitSystem == SI ) f = (5./9.) * (project->Temp.ta - 32.0);
    else f = project->Temp.ta;
    project->SysResults[SYS_TEMPERATURE] = (REAL4)f;
    project->SysResults[SYS_EVAP]      /= totalArea;
    project->SysResults[SYS_RAINFALL]  /= totalArea;
    project->SysResults[SYS_SNOWDEPTH] /= totalArea;
    project->SysResults[SYS_INFIL]     /= totalArea;
}

//=============================================================================

void output_saveNodeResults(Project* project, double reportTime, FILE* file)
//
//  Input:   reportTime = elapsed simulation time (millisec)
//           file = ptr. to binary output file
//  Output:  none
//  Purpose: writes computed node results to binary file.
//
{
   // extern TRoutingTotals StepFlowTotals;  // defined in massbal.c
    int j;

    // --- find where current reporting time lies between latest routing times
    double f = (reportTime - project->OldRoutingTime) /
               (project->NewRoutingTime - project->OldRoutingTime);

    // --- write node results to file
    for (j=0; j<project->Nobjects[NODE]; j++)
    {
        // --- retrieve interpolated results for reporting time & write to file
        node_getResults(project,j, f, project->NodeResults);
        if ( project->Node[j].rptFlag )
            fwrite(project->NodeResults, sizeof(REAL4), project->NnodeResults, file);

        // --- update system-wide storage volume 
        project->SysResults[SYS_STORAGE] += project->NodeResults[NODE_VOLUME];
    }

    // --- update system-wide flows 
	project->SysResults[SYS_FLOODING] = (REAL4)(project->StepFlowTotals.flooding * UCF(project, FLOW));
	project->SysResults[SYS_OUTFLOW] = (REAL4)(project->StepFlowTotals.outflow * UCF(project, FLOW));
	project->SysResults[SYS_DWFLOW] = (REAL4)(project->StepFlowTotals.dwInflow * UCF(project, FLOW));
	project->SysResults[SYS_GWFLOW] = (REAL4)(project->StepFlowTotals.gwInflow * UCF(project, FLOW));
	project->SysResults[SYS_IIFLOW] = (REAL4)(project->StepFlowTotals.iiInflow * UCF(project, FLOW));
	project->SysResults[SYS_EXFLOW] = (REAL4)(project->StepFlowTotals.exInflow * UCF(project, FLOW));
    project->SysResults[SYS_INFLOW] = project->SysResults[SYS_RUNOFF] +
                             project->SysResults[SYS_DWFLOW] +
                             project->SysResults[SYS_GWFLOW] +
                             project->SysResults[SYS_IIFLOW] +
                             project->SysResults[SYS_EXFLOW];
}

//=============================================================================

void output_saveLinkResults(Project* project, double reportTime, FILE* file)
//
//  Input:   reportTime = elapsed simulation time (millisec)
//           file = ptr. to binary output file
//  Output:  none
//  Purpose: writes computed link results to binary file.
//
{
    int j;
    double f;
    double z;

    // --- find where current reporting time lies between latest routing times
    f = (reportTime - project->OldRoutingTime) / (project->NewRoutingTime - project->OldRoutingTime);

    // --- write link results to file
    for (j=0; j<project->Nobjects[LINK]; j++)
    {
        // --- retrieve interpolated results for reporting time & write to file
        link_getResults(project,j, f, project->LinkResults);
        if ( project->Link[j].rptFlag ) 
            fwrite(project->LinkResults, sizeof(REAL4), project->NlinkResults, file);

        // --- update system-wide results
        z = ((1.0-f)*project->Link[j].oldVolume + f*project->Link[j].newVolume) * UCF(project, VOLUME);
        project->SysResults[SYS_STORAGE] += (REAL4)z;
    }
}

//=============================================================================

void output_readDateTime(Project* project, int period, DateTime* days)
//
//  Input:   period = index of reporting time period
//  Output:  days = date/time value
//  Purpose: retrieves the date/time for a specific reporting period
//           from the binary output file.
//
{
    INT4 bytePos = project->OutputStartPos + (period-1)*project->BytesPerPeriod;
    fseek(project->Fout.file, bytePos, SEEK_SET);
    *days = NO_DATE;
    fread(days, sizeof(REAL8), 1, project->Fout.file);
}

//=============================================================================

void output_readSubcatchResults(Project* project, int period, int index)
//
//  Input:   period = index of reporting time period
//           index = subcatchment index
//  Output:  none
//  Purpose: reads computed results for a subcatchment at a specific time
//           period.
//
{
    INT4 bytePos = project->OutputStartPos + (period-1)*project->BytesPerPeriod;
    bytePos += sizeof(REAL8) + index*project->NsubcatchResults*sizeof(REAL4);
    fseek(project->Fout.file, bytePos, SEEK_SET);
    fread(project->SubcatchResults, sizeof(REAL4), project->NsubcatchResults, project->Fout.file);
}

//=============================================================================

void output_readNodeResults(Project* project, int period, int index)
//
//  Input:   period = index of reporting time period
//           index = node index
//  Output:  none
//  Purpose: reads computed results for a node at a specific time period.
//
{
    INT4 bytePos = project->OutputStartPos + (period-1)*project->BytesPerPeriod;
    bytePos += sizeof(REAL8) + project->NumSubcatch*project->NsubcatchResults*sizeof(REAL4);
    bytePos += index*project->NnodeResults*sizeof(REAL4);
    fseek(project->Fout.file, bytePos, SEEK_SET);
    fread(project->NodeResults, sizeof(REAL4), project->NnodeResults, project->Fout.file);
}

//=============================================================================

void output_readLinkResults(Project* project, int period, int index)
//
//  Input:   period = index of reporting time period
//           index = link index
//  Output:  none
//  Purpose: reads computed results for a link at a specific time period.
//
{
    INT4 bytePos = project->OutputStartPos + (period-1)*project->BytesPerPeriod;
    bytePos += sizeof(REAL8) + project->NumSubcatch*project->NsubcatchResults*sizeof(REAL4);
    bytePos += project->NumNodes*project->NnodeResults*sizeof(REAL4);
    bytePos += index*project->NlinkResults*sizeof(REAL4);
    fseek(project->Fout.file, bytePos, SEEK_SET);
    fread(project->LinkResults, sizeof(REAL4), project->NlinkResults, project->Fout.file);
    fread(project->SysResults, sizeof(REAL4), MAX_SYS_RESULTS, project->Fout.file);
}

//=============================================================================
