//-----------------------------------------------------------------------------
//   stats.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/20/14   (Build 5.1.001)
//   Author:   L. Rossman (EPA)
//             R. Dickinson (CDM)
//
//   Simulation statistics functions.
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE


#include <stdlib.h>
#include <math.h>
#include "headers.h"

//-----------------------------------------------------------------------------
//  Shared variables
//-----------------------------------------------------------------------------
#define MAX_STATS 5
static TSysStats       SysStats;
static TMaxStats       MaxMassBalErrs[MAX_STATS];
static TMaxStats       MaxCourantCrit[MAX_STATS];
static TMaxStats       MaxFlowTurns[MAX_STATS];
static double          SysOutfallFlow;

//-----------------------------------------------------------------------------
//  Exportable variables (shared with statsrpt.c)
//-----------------------------------------------------------------------------
TSubcatchStats* SubcatchStats;
TNodeStats*     NodeStats;
TLinkStats*     LinkStats;
TStorageStats*  StorageStats;
TOutfallStats*  OutfallStats;
TPumpStats*     PumpStats;
double          MaxOutfallFlow;
double          MaxRunoffFlow;

//-----------------------------------------------------------------------------
//  Imported variables
//-----------------------------------------------------------------------------
extern double*         NodeInflow;     // defined in massbal.c
extern double*         NodeOutflow;    // defined in massbal.c

//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)
//-----------------------------------------------------------------------------
//  stats_open                    (called from swmm_start in swmm5.c)
//  stats_close                   (called from swmm_end in swmm5.c)
//  stats_report                  (called from swmm_end in swmm5.c)
//  stats_updateSubcatchStats     (called from subcatch_getRunoff)
//  stats_updateFlowStats         (called from routing_execute)
//  stats_updateCriticalTimeCount (called from getVariableStep in dynwave.c)

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------
static void stats_updateNodeStats(Project *project, int node, double tStep, DateTime aDate);
static void stats_updateLinkStats(Project *project, int link, double tStep, DateTime aDate);
static void stats_findMaxStats(Project *project);
static void stats_updateMaxStats(TMaxStats maxStats[], int i, int j, double x);

//=============================================================================

int  stats_open(Project *project)
//
//  Input:   none
//  Output:  returns an error code
//  Purpose: opens the simulation statistics system.
//
{
    int j, k;

    // --- set all pointers to NULL
    NodeStats = NULL;
    LinkStats = NULL;
    StorageStats = NULL;
    OutfallStats = NULL;
    PumpStats = NULL;

    // --- allocate memory for & initialize subcatchment statistics
    SubcatchStats = NULL;
    if ( project->Nobjects[SUBCATCH] > 0 )
    {
        SubcatchStats = (TSubcatchStats *) calloc(project->Nobjects[SUBCATCH],
                                               sizeof(TSubcatchStats));
        if ( !SubcatchStats )
        {
            report_writeErrorMsg(project, ERR_MEMORY, "");
            return project->ErrorCode;
        }
        for (j=0; j<project->Nobjects[SUBCATCH]; j++)
        {
            SubcatchStats[j].precip  = 0.0;
            SubcatchStats[j].runon   = 0.0;
            SubcatchStats[j].evap    = 0.0;
            SubcatchStats[j].infil   = 0.0;
            SubcatchStats[j].runoff  = 0.0;
            SubcatchStats[j].maxFlow = 0.0;
        }
    }

    // --- allocate memory for node & link stats
    if ( project->Nobjects[LINK] > 0 )
    {
        NodeStats = (TNodeStats *) calloc(project->Nobjects[NODE], sizeof(TNodeStats));
        LinkStats = (TLinkStats *) calloc(project->Nobjects[LINK], sizeof(TLinkStats));
        if ( !NodeStats || !LinkStats )
        {
            report_writeErrorMsg(project, ERR_MEMORY, "");
            return project->ErrorCode;
        }
    }

    // --- initialize node stats
    if ( NodeStats ) for ( j = 0; j < project->Nobjects[NODE]; j++ )
    {
        NodeStats[j].avgDepth = 0.0;
        NodeStats[j].maxDepth = 0.0;
        NodeStats[j].maxDepthDate = project->StartDateTime;
        NodeStats[j].maxDepthChange = 0.0;
        NodeStats[j].volFlooded = 0.0;
        NodeStats[j].timeFlooded = 0.0;
        NodeStats[j].timeSurcharged = 0.0;
        NodeStats[j].timeCourantCritical = 0.0;
        NodeStats[j].totLatFlow = 0.0;
        NodeStats[j].maxLatFlow = 0.0;
        NodeStats[j].maxInflow = 0.0;
        NodeStats[j].maxOverflow = 0.0;
        NodeStats[j].maxPondedVol = 0.0;
        NodeStats[j].maxInflowDate = project->StartDateTime;
        NodeStats[j].maxOverflowDate = project->StartDateTime;
    }

    // --- initialize link stats
    if ( LinkStats ) for ( j = 0; j < project->Nobjects[LINK]; j++ )
    {
        LinkStats[j].maxFlow = 0.0;
        LinkStats[j].maxVeloc = 0.0;
        LinkStats[j].maxDepth = 0.0;
        LinkStats[j].timeSurcharged = 0.0;
        LinkStats[j].timeFullUpstream = 0.0;
        LinkStats[j].timeFullDnstream = 0.0;
        LinkStats[j].timeFullFlow = 0.0;
        LinkStats[j].timeCapacityLimited = 0.0;
        LinkStats[j].timeCourantCritical = 0.0;
        for (k=0; k<MAX_FLOW_CLASSES; k++)
            LinkStats[j].timeInFlowClass[k] = 0.0;
        LinkStats[j].flowTurns = 0;
        LinkStats[j].flowTurnSign = 0;
    }

    // --- allocate memory for & initialize storage unit statistics
    if ( project->Nnodes[STORAGE] > 0 )
    {
        StorageStats = (TStorageStats *) calloc(project->Nnodes[STORAGE],
                           sizeof(TStorageStats));
        if ( !StorageStats )
        {
            report_writeErrorMsg(project, ERR_MEMORY, "");
            return project->ErrorCode;
        }
        else for ( k = 0; k < project->Nobjects[NODE]; k++ )
        {
            if ( project->Node[k].type != STORAGE ) continue;
            j = project->Node[k].subIndex;
            StorageStats[j].initVol = project->Node[k].newVolume;
            StorageStats[j].avgVol = 0.0;
            StorageStats[j].maxVol = 0.0;
            StorageStats[j].maxFlow = 0.0;
            StorageStats[j].evapLosses = 0.0;
            StorageStats[j].seepLosses = 0.0;
            StorageStats[j].maxVolDate = project->StartDateTime;
        }
    }

    // --- allocate memory for & initialize outfall statistics
    if ( project->Nnodes[OUTFALL] > 0 )
    {
        OutfallStats = (TOutfallStats *) calloc(project->Nnodes[OUTFALL],
                           sizeof(TOutfallStats));
        if ( !OutfallStats )
        {
            report_writeErrorMsg(project, ERR_MEMORY, "");
            return project->ErrorCode;
        }
        else for ( j = 0; j < project->Nnodes[OUTFALL]; j++ )
        {
            OutfallStats[j].avgFlow = 0.0;
            OutfallStats[j].maxFlow = 0.0;
            OutfallStats[j].totalPeriods = 0;
            if ( project->Nobjects[POLLUT] > 0 )
            {
                OutfallStats[j].totalLoad =
                    (double *) calloc(project->Nobjects[POLLUT], sizeof(double));
                if ( !OutfallStats[j].totalLoad )
                {
                    report_writeErrorMsg(project, ERR_MEMORY, "");
                    return project->ErrorCode;
                }
                for (k=0; k<project->Nobjects[POLLUT]; k++)
                    OutfallStats[j].totalLoad[k] = 0.0;
            }
            else OutfallStats[j].totalLoad = NULL;
        }
    }

    // --- allocate memory & initialize pumping statistics
    if ( project->Nlinks[PUMP] > 0 ) 
    { 
        PumpStats = (TPumpStats *) calloc(project->Nlinks[PUMP], sizeof(TPumpStats));
        if ( !PumpStats ) 
        {
            report_writeErrorMsg(project, ERR_MEMORY, "");
            return project->ErrorCode;
        }
        else for ( j = 0; j < project->Nlinks[PUMP]; j++ )
        {
            PumpStats[j].utilized = 0.0;
            PumpStats[j].minFlow  = 0.0;
            PumpStats[j].avgFlow  = 0.0;
            PumpStats[j].maxFlow  = 0.0; 
            PumpStats[j].volume   = 0.0;
            PumpStats[j].energy   = 0.0;
            PumpStats[j].startUps = 0;
            PumpStats[j].offCurveLow = 0.0; 
            PumpStats[j].offCurveHigh = 0.0;
        } 
    } 

    // --- initialize system stats
    MaxRunoffFlow = 0.0;
    MaxOutfallFlow = 0.0;
    SysStats.maxTimeStep = 0.0;
    SysStats.minTimeStep = project->RouteStep;
    SysStats.avgTimeStep = 0.0;
    SysStats.avgStepCount = 0.0;
    SysStats.steadyStateCount = 0.0;
    return 0;
}

//=============================================================================

void  stats_close(Project *project)
//
//  Input:   none
//  Output:  
//  Purpose: closes the simulation statistics system.
//
{
    int j;

    FREE(SubcatchStats);
    FREE(NodeStats);
    FREE(LinkStats);
    FREE(StorageStats); 
    if ( OutfallStats )
    {
        for ( j=0; j<project->Nnodes[OUTFALL]; j++ )
            FREE(OutfallStats[j].totalLoad);
        FREE(OutfallStats);
    }
    FREE(PumpStats);
}

//=============================================================================

void  stats_report(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: reports simulation statistics.
//
{

    // --- report flow routing accuracy statistics
    if ( project->Nobjects[LINK] > 0 && project->RouteModel != NO_ROUTING )
    {
        stats_findMaxStats(project);
        report_writeMaxStats(project, MaxMassBalErrs, MaxCourantCrit, MAX_STATS);
        report_writeMaxFlowTurns(project, MaxFlowTurns, MAX_STATS);
        report_writeSysStats(project, &SysStats);
    }

    // --- report summary statistics
    statsrpt_writeReport(project);
}

//=============================================================================

void   stats_updateSubcatchStats(int j, double rainVol, double runonVol,
                                 double evapVol, double infilVol,
                                 double runoffVol, double runoff)
//
//  Input:   j = subcatchment index
//           rainVol   = rainfall + snowfall volume (ft3)
//           runonVol  = runon volume from other subcatchments (ft3)
//           evapVol   = evaporation volume (ft3)
//           infilVol  = infiltration volume (ft3)
//           runoffVol = runoff volume (ft3)
//           runoff    = runoff rate (cfs)
//  Output:  none
//  Purpose: updates totals of runoff components for a specific subcatchment.
//
{
    SubcatchStats[j].precip += rainVol;
    SubcatchStats[j].runon  += runonVol;
    SubcatchStats[j].evap   += evapVol;
    SubcatchStats[j].infil  += infilVol;
    SubcatchStats[j].runoff += runoffVol;
    SubcatchStats[j].maxFlow = MAX(SubcatchStats[j].maxFlow, runoff);
}

//=============================================================================

void  stats_updateMaxRunoff(Project *project)
//
//   Input:   none
//   Output:  updates global variable MaxRunoffFlow
//   Purpose: updates value of maximum system runoff rate.
//
{
    int j;
    double sysRunoff = 0.0;
    
    for (j=0; j<project->Nobjects[SUBCATCH]; j++) sysRunoff += project->Subcatch[j].newRunoff;
    MaxRunoffFlow = MAX(MaxRunoffFlow, sysRunoff);
}    

//=============================================================================

void   stats_updateFlowStats(Project *project, double tStep, DateTime aDate, int stepCount,
                             int steadyState)
//
//  Input:   tStep = routing time step (sec)
//           aDate = current date/time
//           stepCount = # steps required to solve routing at current time period
//           steadyState = TRUE if steady flow conditions exist
//  Output:  none
//  Purpose: updates various flow routing statistics at current time period.
//
{
    int   j;

    // --- update stats only after reporting period begins
    if ( aDate < project->ReportStart ) return;
    SysOutfallFlow = 0.0;

    // --- update node & link stats
    for ( j=0; j<project->Nobjects[NODE]; j++ )
        stats_updateNodeStats(project, j, tStep, aDate);
    for ( j=0; j<project->Nobjects[LINK]; j++ )
        stats_updateLinkStats(project, j, tStep, aDate);

    // --- update time step stats
    //     (skip initial time step for min. value)
    if ( project->StepCount > 1 )
    {
        SysStats.minTimeStep = MIN(SysStats.minTimeStep, tStep);
    }
    SysStats.avgTimeStep += tStep;
    SysStats.maxTimeStep = MAX(SysStats.maxTimeStep, tStep);

    // --- update iteration step count stats
    SysStats.avgStepCount += stepCount;

    // --- update count of times in steady state
    SysStats.steadyStateCount += steadyState;

    // --- update max. system outfall flow
    MaxOutfallFlow = MAX(MaxOutfallFlow, SysOutfallFlow);
}

//=============================================================================
   
void stats_updateCriticalTimeCount(int node, int link)
//
//  Input:   node = node index
//           link = link index
//  Output:  none
//  Purpose: updates count of times a node or link was time step-critical.
//
{
    if      ( node >= 0 ) NodeStats[node].timeCourantCritical += 1.0;
    else if ( link >= 0 ) LinkStats[link].timeCourantCritical += 1.0;
}

//=============================================================================

void stats_updateNodeStats(Project *project, int j, double tStep, DateTime aDate)
//
//  Input:   j = node index
//           tStep = routing time step (sec)
//           aDate = current date/time
//  Output:  none
//  Purpose: updates flow statistics for a node.
//
{
    int    k, p;
    double newVolume = project->Node[j].newVolume;
    double newDepth = project->Node[j].newDepth;
    double delta;
    int    canPond = (project->AllowPonding && project->Node[j].pondedArea > 0.0);

    // --- update depth statistics 
    NodeStats[j].avgDepth += newDepth;
    if ( newDepth > NodeStats[j].maxDepth )
    {
        NodeStats[j].maxDepth = newDepth;
        NodeStats[j].maxDepthDate = aDate;
    }
    delta = fabs(newDepth - project->Node[j].oldDepth);
    if ( delta > NodeStats[j].maxDepthChange )
        NodeStats[j].maxDepthChange = delta;
    
    // --- update flooding, ponding, and surcharge statistics
    if ( project->Node[j].type != OUTFALL )
    {
        if ( newVolume > project->Node[j].fullVolume || project->Node[j].overflow > 0.0 )
        {
            NodeStats[j].timeFlooded += tStep;
            NodeStats[j].volFlooded += project->Node[j].overflow * tStep;
            if ( canPond ) NodeStats[j].maxPondedVol =
                MAX(NodeStats[j].maxPondedVol,
                    (newVolume - project->Node[j].fullVolume));
        }
        if ( newDepth + project->Node[j].invertElev + FUDGE >= project->Node[j].crownElev )
        {
                NodeStats[j].timeSurcharged += tStep;
        }
    }

    // --- update storage statistics
    if ( project->Node[j].type == STORAGE )
    {
        k = project->Node[j].subIndex;
        StorageStats[k].avgVol += newVolume;
        StorageStats[k].evapLosses += 
            project->Storage[project->Node[j].subIndex].evapLoss; 
        StorageStats[k].seepLosses +=
            project->Storage[project->Node[j].subIndex].seepLoss;

        newVolume = MIN(newVolume, project->Node[j].fullVolume);
        if ( newVolume > StorageStats[k].maxVol )
        {
            StorageStats[k].maxVol = newVolume;
            StorageStats[k].maxVolDate = aDate;
        }
        StorageStats[k].maxFlow = MAX(StorageStats[k].maxFlow, project->Node[j].outflow);
    }

    // --- update outfall statistics
    if ( project->Node[j].type == OUTFALL ) 
    {
        k = project->Node[j].subIndex;
		if ( project->Node[j].inflow >= MIN_RUNOFF_FLOW )
		{
            OutfallStats[k].avgFlow += project->Node[j].inflow;
            OutfallStats[k].maxFlow = MAX(OutfallStats[k].maxFlow, project->Node[j].inflow);
            OutfallStats[k].totalPeriods++;
		}
        for (p=0; p<project->Nobjects[POLLUT]; p++)
        {
            OutfallStats[k].totalLoad[p] += project->Node[j].inflow * 
				project->Node[j].newQual[p] * tStep;
        }
        SysOutfallFlow += project->Node[j].inflow;
    }

    // --- update inflow statistics
    NodeStats[j].totLatFlow += ( (project->Node[j].oldLatFlow + project->Node[j].newLatFlow) *
                                 0.5 * tStep );
    NodeStats[j].maxLatFlow = MAX(project->Node[j].newLatFlow, NodeStats[j].maxLatFlow);
    if ( project->Node[j].inflow > NodeStats[j].maxInflow )
    {
        NodeStats[j].maxInflow = project->Node[j].inflow;
        NodeStats[j].maxInflowDate = aDate;
    }

    // --- update overflow statistics
    if ( project->Node[j].overflow > NodeStats[j].maxOverflow )
    {
        NodeStats[j].maxOverflow = project->Node[j].overflow;
        NodeStats[j].maxOverflowDate = aDate;
    }
}

//=============================================================================

void  stats_updateLinkStats(Project *project, int j, double tStep, DateTime aDate)
//
//  Input:   j = link index
//           tStep = routing time step (sec)
//           aDate = current date/time
//  Output:  none
//  Purpose: updates flow statistics for a link.
//
{
    int    k;
    double q, v;
    double dq;

    // --- update max. flow
    dq = project->Link[j].newFlow - project->Link[j].oldFlow;
    q = fabs(project->Link[j].newFlow);
    if ( q > LinkStats[j].maxFlow )
    {
        LinkStats[j].maxFlow = q;
        LinkStats[j].maxFlowDate = aDate;
    }

    // --- update max. velocity
    v = link_getVelocity(project, j, q, project->Link[j].newDepth);
    if ( v > LinkStats[j].maxVeloc )
    {
        LinkStats[j].maxVeloc = v;
        LinkStats[j].maxVelocDate = aDate;
    }

    // --- update max. depth
    if ( project->Link[j].newDepth > LinkStats[j].maxDepth )
    {
        LinkStats[j].maxDepth = project->Link[j].newDepth;
    }

    if ( project->Link[j].type == PUMP )
    {
        if ( q >= project->Link[j].qFull )
            LinkStats[j].timeFullFlow += tStep;
        if ( q > MIN_RUNOFF_FLOW )
        {
            k = project->Link[j].subIndex;
            PumpStats[k].minFlow = MIN(PumpStats[k].minFlow, q);
            PumpStats[k].maxFlow = LinkStats[j].maxFlow;
            PumpStats[k].avgFlow += q;
            PumpStats[k].volume += q*tStep;
            PumpStats[k].utilized += tStep;
            PumpStats[k].energy += link_getPower(project, j)*tStep/3600.0;
            if ( project->Link[j].flowClass == DN_DRY )
                PumpStats[k].offCurveLow += tStep;
            if ( project->Link[j].flowClass == UP_DRY )
                PumpStats[k].offCurveHigh += tStep;
            if ( project->Link[j].oldFlow < MIN_RUNOFF_FLOW )
                PumpStats[k].startUps++;
            PumpStats[k].totalPeriods++;
            LinkStats[j].timeSurcharged += tStep;
            LinkStats[j].timeFullUpstream += tStep;
            LinkStats[j].timeFullDnstream += tStep;
        }
    }
    else if ( project->Link[j].type == CONDUIT )
    {

        // --- update time under normal flow & inlet control 
        if ( project->Link[j].normalFlow ) LinkStats[j].timeNormalFlow += tStep;
        if ( project->Link[j].inletControl ) LinkStats[j].timeInletControl += tStep;
    
        // --- update flow classification distribution
        k = project->Link[j].flowClass;
        if ( k >= 0 && k < MAX_FLOW_CLASSES )
        {
            ++LinkStats[j].timeInFlowClass[k];
        }

        // --- update time conduit is full
        k = project->Link[j].subIndex;
        if ( q >= project->Link[j].qFull ) LinkStats[j].timeFullFlow += tStep; 
        if ( project->Conduit[k].capacityLimited )
            LinkStats[j].timeCapacityLimited += tStep;
        if ( project->Link[j].newDepth >= project->Link[j].xsect.yFull )
        {
            LinkStats[j].timeSurcharged += tStep;
            LinkStats[j].timeFullUpstream += tStep;
            LinkStats[j].timeFullDnstream += tStep;
        }
        else if ( project->Conduit[k].a1 >= project->Link[j].xsect.aFull )
            LinkStats[j].timeFullUpstream += tStep;
        else if ( project->Conduit[k].a2 >= project->Link[j].xsect.aFull )
            LinkStats[j].timeFullDnstream += tStep;
    }

    // --- update flow turn count
    k = LinkStats[j].flowTurnSign;
    LinkStats[j].flowTurnSign = SGN(dq);
    if ( fabs(dq) > 0.001 &&  k * LinkStats[j].flowTurnSign < 0 )
            LinkStats[j].flowTurns++;
}

//=============================================================================

void  stats_findMaxStats(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: finds nodes & links with highest mass balance errors
//           & highest times Courant time-step critical.
//
{
    int    j;
    double x;

    // --- initialize max. stats arrays
    for (j=0; j<MAX_STATS; j++)
    {
        MaxMassBalErrs[j].objType = NODE;
        MaxMassBalErrs[j].index   = -1;
        MaxMassBalErrs[j].value   = -1.0;
        MaxCourantCrit[j].index   = -1;
        MaxCourantCrit[j].value   = -1.0;
        MaxFlowTurns[j].index     = -1; 
        MaxFlowTurns[j].value     = -1.0;
    }

    // --- find links with most flow turns 
    if ( project->StepCount > 2 )
    {
        for (j=0; j<project->Nobjects[LINK]; j++)
        {
            x = 100.0 * LinkStats[j].flowTurns / (2./3.*(project->StepCount-2));
            stats_updateMaxStats(MaxFlowTurns, LINK, j, x);
        }
    }

    // --- find nodes with largest mass balance errors
    for (j=0; j<project->Nobjects[NODE]; j++)
    {
        // --- skip terminal nodes and nodes with negligible inflow
        if ( project->Node[j].degree <= 0  ) continue;
        if ( NodeInflow[j] <= 0.1 ) continue;

        // --- evaluate mass balance error
        //     (Note: NodeInflow & NodeOutflow include any initial and final
        //            stored volumes, respectively).
        if ( NodeInflow[j]  > 0.0 )
            x = 1.0 - NodeOutflow[j] / NodeInflow[j];
        else if ( NodeOutflow[j] > 0.0 ) x = -1.0;
        else                             x = 0.0;
        stats_updateMaxStats(MaxMassBalErrs, NODE, j, 100.0*x);
    }

    // --- stop if not using a variable time step
    if ( project->RouteModel != DW || project->CourantFactor == 0.0 ) return;

    // --- find nodes most frequently Courant critical
    for (j=0; j<project->Nobjects[NODE]; j++)
    {
        x = NodeStats[j].timeCourantCritical / project->StepCount;
        stats_updateMaxStats(MaxCourantCrit, NODE, j, 100.0*x);
    }

    // --- find links most frequently Courant critical
    for (j=0; j<project->Nobjects[LINK]; j++)
    {
        x = LinkStats[j].timeCourantCritical / project->StepCount;
        stats_updateMaxStats(MaxCourantCrit, LINK, j, 100.0*x);
    }
}

//=============================================================================

void  stats_updateMaxStats(TMaxStats maxStats[], int i, int j, double x)
//
//  Input:   maxStats[] = array of critical statistics values
//           i = object category (NODE or LINK)
//           j = object index
//           x = value of statistic for the object
//  Output:  none
//  Purpose: updates the collection of most critical statistics
//
{
    int   k;
    TMaxStats maxStats1, maxStats2;
    maxStats1.objType = i;
    maxStats1.index   = j;
    maxStats1.value   = x;
    for (k=0; k<MAX_STATS; k++)
    {
        if ( fabs(maxStats1.value) > fabs(maxStats[k].value) )
        {
            maxStats2 = maxStats[k];
            maxStats[k] = maxStats1;
            maxStats1 = maxStats2;
        }
    }
}

//=============================================================================
