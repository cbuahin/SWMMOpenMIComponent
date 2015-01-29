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


// Definition of 4-byte integer, 4-byte real and 8-byte real types
#define INT4  int
#define REAL4 float
#define REAL8 double

enum InputDataType {INPUT_TYPE_CODE, INPUT_AREA, INPUT_INVERT, INPUT_MAX_DEPTH,
                    INPUT_OFFSET, INPUT_LENGTH};

//-----------------------------------------------------------------------------
//  Shared variables    
//-----------------------------------------------------------------------------
static INT4      IDStartPos;           // starting file position of ID names
static INT4      InputStartPos;        // starting file position of input data
static INT4      OutputStartPos;       // starting file position of output data
static INT4      BytesPerPeriod;       // bytes saved per simulation time period
static INT4      NsubcatchResults;     // number of subcatchment output variables
static INT4      NnodeResults;         // number of node output variables
static INT4      NlinkResults;         // number of link output variables
static INT4      NumSubcatch;          // number of subcatchments reported on
static INT4      NumNodes;             // number of nodes reported on
static INT4      NumLinks;             // number of links reported on
static INT4      NumPolluts;           // number of pollutants reported on
static REAL4     SysResults[MAX_SYS_RESULTS];    // values of system output vars.

//-----------------------------------------------------------------------------
//  Exportable variables (shared with report.c)
//-----------------------------------------------------------------------------
REAL4*           SubcatchResults;
REAL4*           NodeResults;
REAL4*           LinkResults;


//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------
static void output_openOutFile(Project *project);
static void output_saveID(char* id, FILE* file);
static void output_saveSubcatchResults(Project *project, double reportTime, FILE* file);
static void output_saveNodeResults(Project *project, double reportTime, FILE* file);
static void output_saveLinkResults(Project *project, double reportTime, FILE* file);

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

int output_open(Project *project)
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
    if ( project->IgnoreQuality ) NumPolluts = 0;
    else NumPolluts = project->Nobjects[POLLUT];

    // --- subcatchment results consist of Rainfall, Snowdepth, project->Evap, 
    //     Infil, Runoff, GW Flow, GW Elev, GW Sat, and Washoff
    NsubcatchResults = MAX_SUBCATCH_RESULTS - 1 + NumPolluts;

    // --- node results consist of Depth, Head, Volume, Lateral Inflow,
    //     Total Inflow, Overflow and Quality
    NnodeResults = MAX_NODE_RESULTS - 1 + NumPolluts;

    // --- link results consist of Depth, Flow, Velocity, Froude No.,
    //     Capacity and Quality
    NlinkResults = MAX_LINK_RESULTS - 1 + NumPolluts;

    // --- get number of objects reported on
    NumSubcatch = 0;
    NumNodes = 0;
    NumLinks = 0;
    for (j=0; j<project->Nobjects[SUBCATCH]; j++) if (project->Subcatch[j].rptFlag) NumSubcatch++;
    for (j=0; j<project->Nobjects[NODE]; j++) if (project->Node[j].rptFlag) NumNodes++;
    for (j=0; j<project->Nobjects[LINK]; j++) if (project->Link[j].rptFlag) NumLinks++;

    BytesPerPeriod = sizeof(REAL8)
        + NumSubcatch * NsubcatchResults * sizeof(REAL4)
        + NumNodes * NnodeResults * sizeof(REAL4)
        + NumLinks * NlinkResults * sizeof(REAL4)
        + MAX_SYS_RESULTS * sizeof(REAL4);
    project->Nperiods = 0;

    SubcatchResults = NULL;
    NodeResults = NULL;
    LinkResults = NULL;
    SubcatchResults = (REAL4 *) calloc(NsubcatchResults, sizeof(REAL4));
    NodeResults = (REAL4 *) calloc(NnodeResults, sizeof(REAL4));
    LinkResults = (REAL4 *) calloc(NlinkResults, sizeof(REAL4));
    if ( !SubcatchResults || !NodeResults || !LinkResults )
    {
        report_writeErrorMsg(ERR_MEMORY, "");
        return project->ErrorCode;
    }

    fseek(project->Fout.file, 0, SEEK_SET);
    k = MAGICNUMBER;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);   // Magic number
    k = VERSION;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);   // Version number
    k = project->FlowUnits;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);   // Flow units
    k = NumSubcatch;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);   // # subcatchments
    k = NumNodes;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);   // # nodes
    k = NumLinks;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);   // # links
    k = NumPolluts;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);   // # pollutants

    // --- save ID names of subcatchments, nodes, links, & pollutants 
    IDStartPos = ftell(project->Fout.file);
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
    for (j=0; j<NumPolluts; j++) output_saveID(project->Pollut[j].ID, project->Fout.file);

    // --- save codes of pollutant concentration units
    for (j=0; j<NumPolluts; j++)
    {
        k = project->Pollut[j].units;
        fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    }

    InputStartPos = ftell(project->Fout.file);

    // --- save subcatchment area
    k = 1;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = INPUT_AREA;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    for (j=0; j<project->Nobjects[SUBCATCH]; j++)
    {
         if ( !project->Subcatch[j].rptFlag ) continue;
         SubcatchResults[0] = (REAL4)(project->Subcatch[j].area * UCF(LANDAREA));
         fwrite(&SubcatchResults[0], sizeof(REAL4), 1, project->Fout.file);
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
        NodeResults[0] = (REAL4)(project->Node[j].invertElev * UCF(LENGTH));
        NodeResults[1] = (REAL4)(project->Node[j].fullDepth * UCF(LENGTH));
        fwrite(&k, sizeof(INT4), 1, project->Fout.file);
        fwrite(NodeResults, sizeof(REAL4), 2, project->Fout.file);
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
            for (m=0; m<4; m++) LinkResults[m] = 0.0f;
        }
        else
        {
            LinkResults[0] = (REAL4)(project->Link[j].offset1 * UCF(LENGTH));
            LinkResults[1] = (REAL4)(project->Link[j].offset2 * UCF(LENGTH));
            if ( project->Link[j].direction < 0 )
            {
                x = LinkResults[0];
                LinkResults[0] = LinkResults[1];
                LinkResults[1] = x;
            }
            if ( k == OUTLET ) LinkResults[2] = 0.0f;
            else LinkResults[2] = (REAL4)(project->Link[j].xsect.yFull * UCF(LENGTH));
            if ( k == CONDUIT )
            {
                m = project->Link[j].subIndex;
                LinkResults[3] = (REAL4)(project->Conduit[m].length * UCF(LENGTH));
            }
            else LinkResults[3] = 0.0f;
        }
        fwrite(&k, sizeof(INT4), 1, project->Fout.file);
        fwrite(LinkResults, sizeof(REAL4), 4, project->Fout.file);
    }

    // --- save number & codes of subcatchment result variables
    k = NsubcatchResults;
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

    for (j=0; j<NumPolluts; j++) 
    {
        k = SUBCATCH_WASHOFF + j;
        fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    }

    // --- save number & codes of node result variables
    k = NnodeResults;
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
    for (j=0; j<NumPolluts; j++)
    {
        k = NODE_QUAL + j;
        fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    }

    // --- save number & codes of link result variables
    k = NlinkResults;
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
    for (j=0; j<NumPolluts; j++)
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
        report_writeErrorMsg(ERR_OUT_WRITE, "");
        return project->ErrorCode;
    }
    OutputStartPos = ftell(project->Fout.file);
    if ( project->Fout.mode == SCRATCH_FILE ) output_checkFileSize(project);
    return project->ErrorCode;
}

//=============================================================================

void  output_checkFileSize(Project *project)
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
        if ( (double)OutputStartPos + (double)BytesPerPeriod * project->TotalDuration
             / 1000.0 / (double)project->ReportStep >= (double)MAXFILESIZE )
        {
            report_writeErrorMsg(ERR_FILE_SIZE, "");
        }
    }
}


//=============================================================================

void output_openOutFile(Project *project)
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
        getTempFileName(project->Fout.name);
    }

    // --- try to open the file
    if ( (project->Fout.file = fopen(project->Fout.name, "w+b")) == NULL)
    {
        writecon(FMT14);
        project->ErrorCode = ERR_OUT_FILE;
    }
}

//=============================================================================

void output_saveResults(Project *project, double reportTime)
//
//  Input:   reportTime = elapsed simulation time (millisec)
//  Output:  none
//  Purpose: writes computed results for current report time to binary file.
//
{
    int i;
    DateTime reportDate = getDateTime(reportTime);
    REAL8 date;

    if ( reportDate < project->ReportStart ) return;
    for (i=0; i<MAX_SYS_RESULTS; i++) SysResults[i] = 0.0f;
    date = reportDate;
    fwrite(&date, sizeof(REAL8), 1, project->Fout.file);
    if (project->Nobjects[SUBCATCH] > 0)
        output_saveSubcatchResults(project, reportTime, project->Fout.file);
    if (project->Nobjects[NODE] > 0)
        output_saveNodeResults(project, reportTime, project->Fout.file);
    if (project->Nobjects[LINK] > 0)
        output_saveLinkResults(project, reportTime, project->Fout.file);
    fwrite(SysResults, sizeof(REAL4), MAX_SYS_RESULTS, project->Fout.file);
    if ( project->Foutflows.mode == SAVE_FILE && !project->IgnoreRouting ) 
        iface_saveOutletResults(project, reportDate, project->Foutflows.file);
    project->Nperiods++;
}

//=============================================================================

void output_end(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: writes closing records to binary file.
//
{
    INT4 k;
    fwrite(&IDStartPos, sizeof(INT4), 1, project->Fout.file);
    fwrite(&InputStartPos, sizeof(INT4), 1, project->Fout.file);
    fwrite(&OutputStartPos, sizeof(INT4), 1, project->Fout.file);
    k = project->Nperiods;
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = (INT4)error_getCode(project->ErrorCode);
    fwrite(&k, sizeof(INT4), 1, project->Fout.file);
    k = MAGICNUMBER;
    if (fwrite(&k, sizeof(INT4), 1, project->Fout.file) < 1)
    {
        report_writeErrorMsg(ERR_OUT_WRITE, "");
    }
}

//=============================================================================

void output_close()
//
//  Input:   none
//  Output:  none
//  Purpose: frees memory used for accessing the binary file.
//
{
    FREE(SubcatchResults);
    FREE(NodeResults);
    FREE(LinkResults);
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

void output_saveSubcatchResults(Project *project, double reportTime, FILE* file)
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
    DateTime reportDate = getDateTime(reportTime);

    // --- update reported rainfall at each rain gage
    for ( j=0; j<project->Nobjects[GAGE]; j++ )
    {
        gage_setReportRainfall(project, j, reportDate);
    }

    // --- find where current reporting time lies between latest runoff times
    f = (reportTime - project->OldRunoffTime) / (project->NewRunoffTime - project->OldRunoffTime);

    // --- write subcatchment results to file
    for ( j=0; j<project->Nobjects[SUBCATCH]; j++)
    {
        // --- retrieve interpolated results for reporting time & write to file
        subcatch_getResults(project, j, f, SubcatchResults);
        if ( project->Subcatch[j].rptFlag )
            fwrite(SubcatchResults, sizeof(REAL4), NsubcatchResults, file);

        // --- update system-wide results
        area = project->Subcatch[j].area * UCF(LANDAREA);
        totalArea += (REAL4)area;
        SysResults[SYS_RAINFALL] +=
            (REAL4)(SubcatchResults[SUBCATCH_RAINFALL] * area);
        SysResults[SYS_SNOWDEPTH] +=
            (REAL4)(SubcatchResults[SUBCATCH_SNOWDEPTH] * area);
        SysResults[SYS_EVAP] +=
            (REAL4)(SubcatchResults[SUBCATCH_EVAP] * area);
        if ( project->Subcatch[j].groundwater ) SysResults[SYS_EVAP] += 
            (REAL4)(project->Subcatch[j].groundwater->evapLoss * UCF(EVAPRATE) * area);
        SysResults[SYS_INFIL] +=
            (REAL4)(SubcatchResults[SUBCATCH_INFIL] * area);
        SysResults[SYS_RUNOFF] += (REAL4)SubcatchResults[SUBCATCH_RUNOFF];
    }

    // --- normalize system-wide results to catchment area
    if ( project->UnitSystem == SI ) f = (5./9.) * (project->Temp.ta - 32.0);
    else f = project->Temp.ta;
    SysResults[SYS_TEMPERATURE] = (REAL4)f;
    SysResults[SYS_EVAP]      /= totalArea;
    SysResults[SYS_RAINFALL]  /= totalArea;
    SysResults[SYS_SNOWDEPTH] /= totalArea;
    SysResults[SYS_INFIL]     /= totalArea;
}

//=============================================================================

void output_saveNodeResults(Project *project, double reportTime, FILE* file)
//
//  Input:   reportTime = elapsed simulation time (millisec)
//           file = ptr. to binary output file
//  Output:  none
//  Purpose: writes computed node results to binary file.
//
{
    extern TRoutingTotals StepFlowTotals;  // defined in massbal.c
    int j;

    // --- find where current reporting time lies between latest routing times
    double f = (reportTime - project->OldRoutingTime) /
               (project->NewRoutingTime - project->OldRoutingTime);

    // --- write node results to file
    for (j=0; j<project->Nobjects[NODE]; j++)
    {
        // --- retrieve interpolated results for reporting time & write to file
        node_getResults(project, j, f, NodeResults);
        if ( project->Node[j].rptFlag )
            fwrite(NodeResults, sizeof(REAL4), NnodeResults, file);

        // --- update system-wide storage volume 
        SysResults[SYS_STORAGE] += NodeResults[NODE_VOLUME];
    }

    // --- update system-wide flows 
    SysResults[SYS_FLOODING] = (REAL4) (StepFlowTotals.flooding * UCF(FLOW));
    SysResults[SYS_OUTFLOW]  = (REAL4) (StepFlowTotals.outflow * UCF(FLOW));
    SysResults[SYS_DWFLOW] = (REAL4)(StepFlowTotals.dwInflow * UCF(FLOW));
    SysResults[SYS_GWFLOW] = (REAL4)(StepFlowTotals.gwInflow * UCF(FLOW));
    SysResults[SYS_IIFLOW] = (REAL4)(StepFlowTotals.iiInflow * UCF(FLOW));
    SysResults[SYS_EXFLOW] = (REAL4)(StepFlowTotals.exInflow * UCF(FLOW));
    SysResults[SYS_INFLOW] = SysResults[SYS_RUNOFF] +
                             SysResults[SYS_DWFLOW] +
                             SysResults[SYS_GWFLOW] +
                             SysResults[SYS_IIFLOW] +
                             SysResults[SYS_EXFLOW];
}

//=============================================================================

void output_saveLinkResults(Project *project, double reportTime, FILE* file)
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
        link_getResults(project, j, f, LinkResults);
        if ( project->Link[j].rptFlag ) 
            fwrite(LinkResults, sizeof(REAL4), NlinkResults, file);

        // --- update system-wide results
        z = ((1.0-f)*project->Link[j].oldVolume + f*project->Link[j].newVolume) * UCF(VOLUME);
        SysResults[SYS_STORAGE] += (REAL4)z;
    }
}

//=============================================================================

void output_readDateTime(Project *project, int period, DateTime* days)
//
//  Input:   period = index of reporting time period
//  Output:  days = date/time value
//  Purpose: retrieves the date/time for a specific reporting period
//           from the binary output file.
//
{
    INT4 bytePos = OutputStartPos + (period-1)*BytesPerPeriod;
    fseek(project->Fout.file, bytePos, SEEK_SET);
    *days = NO_DATE;
    fread(days, sizeof(REAL8), 1, project->Fout.file);
}

//=============================================================================

void output_readSubcatchResults(Project *project, int period, int index)
//
//  Input:   period = index of reporting time period
//           index = subcatchment index
//  Output:  none
//  Purpose: reads computed results for a subcatchment at a specific time
//           period.
//
{
    INT4 bytePos = OutputStartPos + (period-1)*BytesPerPeriod;
    bytePos += sizeof(REAL8) + index*NsubcatchResults*sizeof(REAL4);
    fseek(project->Fout.file, bytePos, SEEK_SET);
    fread(SubcatchResults, sizeof(REAL4), NsubcatchResults, project->Fout.file);
}

//=============================================================================

void output_readNodeResults(Project *project, int period, int index)
//
//  Input:   period = index of reporting time period
//           index = node index
//  Output:  none
//  Purpose: reads computed results for a node at a specific time period.
//
{
    INT4 bytePos = OutputStartPos + (period-1)*BytesPerPeriod;
    bytePos += sizeof(REAL8) + NumSubcatch*NsubcatchResults*sizeof(REAL4);
    bytePos += index*NnodeResults*sizeof(REAL4);
    fseek(project->Fout.file, bytePos, SEEK_SET);
    fread(NodeResults, sizeof(REAL4), NnodeResults, project->Fout.file);
}

//=============================================================================

void output_readLinkResults(Project *project, int period, int index)
//
//  Input:   period = index of reporting time period
//           index = link index
//  Output:  none
//  Purpose: reads computed results for a link at a specific time period.
//
{
    INT4 bytePos = OutputStartPos + (period-1)*BytesPerPeriod;
    bytePos += sizeof(REAL8) + NumSubcatch*NsubcatchResults*sizeof(REAL4);
    bytePos += NumNodes*NnodeResults*sizeof(REAL4);
    bytePos += index*NlinkResults*sizeof(REAL4);
    fseek(project->Fout.file, bytePos, SEEK_SET);
    fread(LinkResults, sizeof(REAL4), NlinkResults, project->Fout.file);
    fread(SysResults, sizeof(REAL4), MAX_SYS_RESULTS, project->Fout.file);
}

//=============================================================================
