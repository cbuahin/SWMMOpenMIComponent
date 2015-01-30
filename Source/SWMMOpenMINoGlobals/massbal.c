//-----------------------------------------------------------------------------
//   massbal.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/19/14  (Build 5.1.001)
//   Author:   L. Rossman (EPA)
//             M. Tryby (EPA)
//
//   Mass balance functions
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE


#include <stdlib.h>
#include <math.h>
#include "headers.h"

//-----------------------------------------------------------------------------
//  Constants   
//-----------------------------------------------------------------------------
static const double MAX_RUNOFF_BALANCE_ERR = 10.0;
static const double MAX_FLOW_BALANCE_ERR   = 10.0;

//-----------------------------------------------------------------------------
//  Shared variables   
//-----------------------------------------------------------------------------
//TRunoffTotals    project->RunoffTotals;    // overall surface runoff continuity totals
//TLoadingTotals*  project->LoadingTotals;   // overall WQ washoff continuity totals
//TGwaterTotals    project->GwaterTotals;    // overall groundwater continuity totals 
//TRoutingTotals   project->FlowTotals;      // overall routed flow continuity totals 
//TRoutingTotals*  project->QualTotals;      // overall routed WQ continuity totals 
//TRoutingTotals   project->StepFlowTotals;  // routed flow totals over time step
//TRoutingTotals   project->OldStepFlowTotals;
//TRoutingTotals*  project->StepQualTotals;  // routed WQ totals over time step

//-----------------------------------------------------------------------------
//  Exportable variables
//-----------------------------------------------------------------------------
double*  NodeInflow;              // total inflow volume to each node (ft3)
double*  NodeOutflow;             // total outflow volume from each node (ft3)
double   TotalArea;               // total drainage area (ft2)

//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)
//-----------------------------------------------------------------------------
//  massbal_open                (called from swmm_start in swmm5.c)
//  massbal_close               (called from swmm_end in swmm5.c)
//  massbal_report              (called from swmm_end in swmm5.c)
//  massbal_updateRunoffTotals  (called from subcatch_getRunoff)
//  massbal_updateLoadingTotals (called from subcatch_getBuildup)
//  massbal_updateGwaterTotals  (called from updateMassBal in gwater.c)
//  massbal_updateRoutingTotals (called from routing_execute)
//  massbal_initTimeStepTotals  (called from routing_execute)
//  massbal_addInflowFlow       (called from routing.c)
//  massbal_addInflowQual       (called from routing.c)
//  massbal_addOutflowFlow      (called from removeOutflows in routing.c)
//  massbal_addOutflowQual      (called from removeOutflows in routing.c)
//  massbal_addNodeLosses       (called from removeStorageLosses in routing.c)
//  massbal_addLinkLosses       (called from removeConduitLosses in routing.c)
//  massbal_addReactedMass      (called from qualrout.c & treatmnt.c)
//  massbal_getStepFlowError    (called from routing.c)

//-----------------------------------------------------------------------------
//  Local Functions   
//-----------------------------------------------------------------------------
double massbal_getBuildup(Project *project, int pollut);
double massbal_getStorage(Project *project, char isFinalStorage);
double massbal_getStoredMass(Project *project, int pollut);
double massbal_getRunoffError(Project *project);
double massbal_getLoadingError(Project *project);
double massbal_getGwaterError(Project *project);
double massbal_getFlowError(Project *project);
double massbal_getQualError(Project *project);


//=============================================================================

int massbal_open(Project *project)
//
//  Input:   none
//  Output:  returns error code
//  Purpose: opens and initializes mass balance continuity checking.
//
{
    int j, n;

    // --- initialize global continuity errors
    project->RunoffError = 0.0;
    project->GwaterError = 0.0;
    project->FlowError   = 0.0;
    project->QualError   = 0.0;

    // --- initialize runoff totals
    project->RunoffTotals.rainfall    = 0.0;
    project->RunoffTotals.evap        = 0.0;
    project->RunoffTotals.infil       = 0.0;
    project->RunoffTotals.runoff      = 0.0;
    project->RunoffTotals.snowRemoved = 0.0;
    project->RunoffTotals.initStorage = 0.0;
    project->RunoffTotals.initSnowCover = 0.0;
    TotalArea = 0.0;
    for (j = 0; j < project->Nobjects[SUBCATCH]; j++)
    {
        project->RunoffTotals.initStorage += subcatch_getStorage(project,j);
        project->RunoffTotals.initSnowCover += snow_getSnowCover(project, j);
        TotalArea += project->Subcatch[j].area;
    }

    // --- initialize groundwater totals
    project->GwaterTotals.infil        = 0.0;
    project->GwaterTotals.upperEvap    = 0.0;
    project->GwaterTotals.lowerEvap    = 0.0;
    project->GwaterTotals.lowerPerc    = 0.0;
    project->GwaterTotals.gwater       = 0.0;
    project->GwaterTotals.initStorage  = 0.0;
    project->GwaterTotals.finalStorage = 0.0;
    for ( j = 0; j < project->Nobjects[SUBCATCH]; j++ )
    {
        project->GwaterTotals.initStorage += gwater_getVolume(project, j) * project->Subcatch[j].area;
    }

    // --- initialize node flow & storage totals
    project->FlowTotals.dwInflow = 0.0;
    project->FlowTotals.wwInflow = 0.0;
    project->FlowTotals.gwInflow = 0.0;
    project->FlowTotals.iiInflow = 0.0;
    project->FlowTotals.exInflow = 0.0;
    project->FlowTotals.flooding = 0.0;
    project->FlowTotals.outflow  = 0.0;
    project->FlowTotals.evapLoss = 0.0; 
    project->FlowTotals.seepLoss = 0.0;
    project->FlowTotals.reacted  = 0.0;
    project->FlowTotals.initStorage = 0.0;
    for (j = 0; j < project->Nobjects[NODE]; j++)
        project->FlowTotals.initStorage += project->Node[j].newVolume;
    for (j = 0; j < project->Nobjects[LINK]; j++)
        project->FlowTotals.initStorage += project->Link[j].newVolume;
    project->StepFlowTotals = project->FlowTotals;

    // --- add contribution of minimum surface area to initial storage
    if ( project->RouteModel == DW )
    {
	    for (j = 0; j < project->Nobjects[NODE]; j++)
	    {
            if ( project->Node[j].type != STORAGE ) project->FlowTotals.initStorage +=
		        project->Node[j].initDepth * project->MinSurfArea;
	    }
    }

    // --- initialize arrays to null
    project->LoadingTotals = NULL;
    project->QualTotals = NULL;
    project->StepQualTotals = NULL;
    NodeInflow = NULL;
    NodeOutflow = NULL;

    // --- allocate memory for WQ washoff continuity totals
    n = project->Nobjects[POLLUT];
    if ( n > 0 )
    {
        project->LoadingTotals = (TLoadingTotals *) calloc(n, sizeof(TLoadingTotals));
        if ( project->LoadingTotals == NULL )
        {
             report_writeErrorMsg(project, ERR_MEMORY, "");
             return project->ErrorCode;
        }
        for (j = 0; j < n; j++)
        {
            project->LoadingTotals[j].initLoad      = massbal_getBuildup(project, j);
            project->LoadingTotals[j].buildup       = 0.0;
            project->LoadingTotals[j].deposition    = 0.0;
            project->LoadingTotals[j].sweeping      = 0.0;
            project->LoadingTotals[j].infil         = 0.0;
            project->LoadingTotals[j].bmpRemoval    = 0.0;
            project->LoadingTotals[j].runoff        = 0.0;
            project->LoadingTotals[j].finalLoad     = 0.0;
        }
    }

    // --- allocate memory for nodal WQ continuity totals
    if ( n > 0 )
    {
         project->QualTotals = (TRoutingTotals *) calloc(n, sizeof(TRoutingTotals));
         project->StepQualTotals = (TRoutingTotals *) calloc(n, sizeof(TRoutingTotals));
         if ( project->QualTotals == NULL || project->StepQualTotals == NULL )
         {
             report_writeErrorMsg(project, ERR_MEMORY, "");
             return project->ErrorCode;
         }
     }

    // --- initialize WQ totals
    for (j = 0; j < n; j++)
    {
        project->QualTotals[j].dwInflow = 0.0;
        project->QualTotals[j].wwInflow = 0.0;
        project->QualTotals[j].gwInflow = 0.0;
        project->QualTotals[j].exInflow = 0.0;
        project->QualTotals[j].flooding = 0.0;
        project->QualTotals[j].outflow  = 0.0;
        project->QualTotals[j].evapLoss = 0.0;
        project->QualTotals[j].seepLoss = 0.0; 
        project->QualTotals[j].reacted  = 0.0;
        project->QualTotals[j].initStorage = massbal_getStoredMass(project,j);
    }

    // --- initialize totals used over a single time step
    massbal_initTimeStepTotals(project);

    // --- allocate memory for nodal flow continuity
    if ( project->Nobjects[NODE] > 0 )
    {
        NodeInflow = (double *) calloc(project->Nobjects[NODE], sizeof(double));
        if ( NodeInflow == NULL )
        {
             report_writeErrorMsg(project, ERR_MEMORY, "");
             return project->ErrorCode;
        }
        NodeOutflow = (double *) calloc(project->Nobjects[NODE], sizeof(double));
        if ( NodeOutflow == NULL )
        {
             report_writeErrorMsg(project, ERR_MEMORY, "");
             return project->ErrorCode;
        }
        for (j = 0; j < project->Nobjects[NODE]; j++) NodeInflow[j] = project->Node[j].newVolume;
    }
    return project->ErrorCode;
}

//=============================================================================

void massbal_close(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: frees memory used by mass balance system.
//
{
    FREE(project->LoadingTotals);
    FREE(project->QualTotals);
    FREE(project->StepQualTotals);
    FREE(NodeInflow);
    FREE(NodeOutflow);
}

//=============================================================================

void massbal_report(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: reports mass balance results.
//
{
    int    j;
    double gwArea = 0.0;

    if ( project->Nobjects[SUBCATCH] > 0 )
    {
        if ( massbal_getRunoffError(project) > MAX_RUNOFF_BALANCE_ERR ||
             project->RptFlags.continuity == TRUE
           ) report_writeRunoffError(&project->RunoffTotals, TotalArea);

        if ( project->Nobjects[POLLUT] > 0 && !project->IgnoreQuality )
        {
            if ( massbal_getLoadingError(project) > MAX_RUNOFF_BALANCE_ERR ||
                 project->RptFlags.continuity == TRUE
               ) report_writeLoadingError(project->LoadingTotals);
        }
    }

    if ( project->Nobjects[AQUIFER] > 0  && !project->IgnoreGwater )
    {
        if ( massbal_getGwaterError(project) > MAX_RUNOFF_BALANCE_ERR ||
             project->RptFlags.continuity == TRUE )
        {
            for ( j = 0; j < project->Nobjects[SUBCATCH]; j++ )
            {
                if ( project->Subcatch[j].groundwater ) gwArea += project->Subcatch[j].area;
            }
            if ( gwArea > 0.0 ) report_writeGwaterError(&project->GwaterTotals, gwArea);
       }
    }

    if ( project->Nobjects[NODE] > 0 && !project->IgnoreRouting )
    {
        if ( massbal_getFlowError(project) > MAX_FLOW_BALANCE_ERR ||
             project->RptFlags.continuity == TRUE
           ) report_writeFlowError(&project->FlowTotals);
    
        if ( project->Nobjects[POLLUT] > 0 && !project->IgnoreQuality )
        {
            if ( massbal_getQualError(project) > MAX_FLOW_BALANCE_ERR ||
                 project->RptFlags.continuity == TRUE
               ) report_writeQualError(project->QualTotals);
        }
    }
}

//=============================================================================

double massbal_getBuildup(Project *project, int p)
//
//  Input:   p = pollutant index
//  Output:  returns total pollutant buildup (lbs or kg)
//  Purpose: computes current total buildup of a pollutant over study area.
//
{
    int    i, j;
    double load = 0.0;

    for (j = 0; j < project->Nobjects[SUBCATCH]; j++)
    {
        for (i = 0; i < project->Nobjects[LANDUSE]; i++)
        {
            load += project->Subcatch[j].landFactor[i].buildup[p];
        }
        load += project->Subcatch[j].pondedQual[p] * project->Pollut[p].mcf;
    }
    return load;
}

//=============================================================================

void massbal_updateRunoffTotals(Project *project, double vRainfall, double vEvap, double vInfil,
                                double vRunoff)
//
//  Input:   vRain   = rainfall volume (ft3)
//           vEvap   = evaporation volume (ft3)
//           vInfil  = infiltration volume (ft3)
//           vRunoff = runoff volume (ft3)
//  Output:  none
//  Purpose: updates runoff totals after current time step.
//
{
    project->RunoffTotals.rainfall += vRainfall;
    project->RunoffTotals.evap     += vEvap;
    project->RunoffTotals.infil    += vInfil;
    project->RunoffTotals.runoff   += vRunoff;
}

//=============================================================================

void massbal_updateGwaterTotals(Project *project, double vInfil, double vUpperEvap, double vLowerEvap,
                                double vLowerPerc, double vGwater)
//
//  Input:   vInfil = volume depth of infiltrated water (ft)
//           vUpperEvap = volume depth of upper evaporation (ft)
//           vLowerEvap = volume depth of lower evaporation (ft)
//           vLowerPerc = volume depth of percolation to deep GW (ft)
//           vGwater = volume depth of groundwater outflow (ft)
//  Output:  none
//  Purpose: updates groundwater totals after current time step.
//
{
    project->GwaterTotals.infil     += vInfil;
    project->GwaterTotals.upperEvap += vUpperEvap;
    project->GwaterTotals.lowerEvap += vLowerEvap;
    project->GwaterTotals.lowerPerc += vLowerPerc;
    project->GwaterTotals.gwater    += vGwater;
}

//=============================================================================

void massbal_initTimeStepTotals(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: initializes routing totals for current time step.
//
{
    int j;
    project->OldStepFlowTotals = project->StepFlowTotals;
    project->StepFlowTotals.dwInflow  = 0.0;
    project->StepFlowTotals.wwInflow  = 0.0;
    project->StepFlowTotals.gwInflow  = 0.0;
    project->StepFlowTotals.iiInflow  = 0.0;
    project->StepFlowTotals.exInflow  = 0.0;
    project->StepFlowTotals.flooding  = 0.0;
    project->StepFlowTotals.outflow   = 0.0;
    project->StepFlowTotals.evapLoss  = 0.0;
    project->StepFlowTotals.seepLoss  = 0.0;
    project->StepFlowTotals.reacted   = 0.0;
    for (j=0; j<project->Nobjects[POLLUT]; j++)
    {
        project->StepQualTotals[j].dwInflow  = 0.0;
        project->StepQualTotals[j].wwInflow  = 0.0;
        project->StepQualTotals[j].gwInflow  = 0.0;
        project->StepQualTotals[j].iiInflow  = 0.0;
        project->StepQualTotals[j].exInflow  = 0.0;
        project->StepQualTotals[j].flooding  = 0.0;
        project->StepQualTotals[j].outflow   = 0.0;
        project->StepQualTotals[j].reacted   = 0.0;
    }
}

//=============================================================================

void massbal_addInflowFlow(Project *project, int type, double q)
//
//  Input:   type = type of inflow
//           q    = inflow rate (cfs)
//  Output:  none
//  Purpose: adds flow inflow to routing totals for current time step.
//
{
    switch (type)
    {
      case DRY_WEATHER_INFLOW: project->StepFlowTotals.dwInflow += q; break;
      case WET_WEATHER_INFLOW: project->StepFlowTotals.wwInflow += q; break;
      case GROUNDWATER_INFLOW: project->StepFlowTotals.gwInflow += q; break;
      case RDII_INFLOW:        project->StepFlowTotals.iiInflow += q; break;
      case EXTERNAL_INFLOW:    project->StepFlowTotals.exInflow += q; break;
    }
}

//=============================================================================

void massbal_updateLoadingTotals(Project *project, int type, int p, double w)
//
//  Input:   type = type of inflow
//           p    = pollutant index
//           w    = mass loading
//  Output:  none
//  Purpose: adds inflow mass loading to loading totals for current time step.
//
{
    switch (type)
    {
      case BUILDUP_LOAD:     project->LoadingTotals[p].buildup    += w; break;
      case DEPOSITION_LOAD:  project->LoadingTotals[p].deposition += w; break;
      case SWEEPING_LOAD:    project->LoadingTotals[p].sweeping   += w; break;
      case INFIL_LOAD:       project->LoadingTotals[p].infil      += w; break;
      case BMP_REMOVAL_LOAD: project->LoadingTotals[p].bmpRemoval += w; break;
      case RUNOFF_LOAD:      project->LoadingTotals[p].runoff     += w; break;
      case FINAL_LOAD:       project->LoadingTotals[p].finalLoad  += w; break;
    }
}

//=============================================================================

void massbal_addInflowQual(Project *project, int type, int p, double w)
//
//  Input:   type = type of inflow
//           p    = pollutant index
//           w    = mass flow rate (mass/sec)
//  Output:  none
//  Purpose: adds quality inflow to routing totals for current time step.
//
{
    if ( p < 0 || p >= project->Nobjects[POLLUT] ) return;
    switch (type)
    {
      case DRY_WEATHER_INFLOW: project->StepQualTotals[p].dwInflow += w; break;
      case WET_WEATHER_INFLOW: project->StepQualTotals[p].wwInflow += w; break;
      case GROUNDWATER_INFLOW: project->StepQualTotals[p].gwInflow += w; break;
      case EXTERNAL_INFLOW:    project->StepQualTotals[p].exInflow += w; break;
      case RDII_INFLOW:        project->StepQualTotals[p].iiInflow += w; break;
   }
}

//=============================================================================

void massbal_addOutflowFlow(Project *project, double q, int isFlooded)
//
//  Input:   q = outflow flow rate (cfs)
//           isFlooded = TRUE if outflow represents internal flooding
//  Output:  none
//  Purpose: adds flow outflow over current time step to routing totals.
//
{
    if ( q >= 0.0 )
    {
        if ( isFlooded ) project->StepFlowTotals.flooding += q;
        else             project->StepFlowTotals.outflow += q;
    }
    else project->StepFlowTotals.exInflow -= q;
}

//=============================================================================

void massbal_addOutflowQual(Project *project, int p, double w, int isFlooded)
//
//  Input:   p = pollutant index
//           w = mass outflow rate (mass/sec)
//           isFlooded = TRUE if outflow represents internal flooding
//  Output:  none
//  Purpose: adds pollutant outflow over current time step to routing totals.
//
{
    if ( p < 0 || p >= project->Nobjects[POLLUT] ) return;
    if ( w >= 0.0 )
    {
        if ( isFlooded ) project->StepQualTotals[p].flooding += w;
        else             project->StepQualTotals[p].outflow += w;
    }
    else project->StepQualTotals[p].exInflow -= w;
}

//=============================================================================

void massbal_addReactedMass(Project *project, int p, double w)
//
//  Input:   p = pollutant index
//           w = rate of mass reacted (mass/sec)
//  Output:  none
//  Purpose: adds mass reacted during current time step to routing totals.
//
{
    if ( p < 0 || p >= project->Nobjects[POLLUT] ) return;
    project->StepQualTotals[p].reacted += w;
}

//=============================================================================

void massbal_addNodeLosses(Project *project, double evapLoss, double seepLoss)
//
//  Input:   evapLoss = evaporation loss from all nodes (ft3/sec)
//           seepLoss = seepage loss from all nodes (ft3/sec)
//  Output:  none
//  Purpose: adds node losses over current time step to routing totals.
//
{
    project->StepFlowTotals.evapLoss += evapLoss;
    project->StepFlowTotals.seepLoss += seepLoss;
}

//=============================================================================

void massbal_addLinkLosses(Project *project, double evapLoss, double seepLoss)
//
//  Input:   evapLoss = evaporation loss from all links (ft3/sec)
//           infilLoss = infiltration loss from all links (ft3/sec)
//  Output:  none
//  Purpose: adds link losses over current time step to routing totals.
//
{
    project->StepFlowTotals.evapLoss += evapLoss;
    project->StepFlowTotals.seepLoss += seepLoss;
}

//=============================================================================

void massbal_updateRoutingTotals(Project *project, double tStep)
//
//  Input:   tStep = time step (sec)
//  Output:  none
//  Purpose: updates overall routing totals with totals from current time step.
//
{
    int j;
    project->FlowTotals.dwInflow += project->StepFlowTotals.dwInflow * tStep;
    project->FlowTotals.wwInflow += project->StepFlowTotals.wwInflow * tStep;
    project->FlowTotals.gwInflow += project->StepFlowTotals.gwInflow * tStep;
    project->FlowTotals.iiInflow += project->StepFlowTotals.iiInflow * tStep;
    project->FlowTotals.exInflow += project->StepFlowTotals.exInflow * tStep;
    project->FlowTotals.flooding += project->StepFlowTotals.flooding * tStep;
    project->FlowTotals.outflow  += project->StepFlowTotals.outflow * tStep;
    project->FlowTotals.evapLoss += project->StepFlowTotals.evapLoss * tStep;
    project->FlowTotals.seepLoss += project->StepFlowTotals.seepLoss * tStep;

    for (j = 0; j < project->Nobjects[POLLUT]; j++)
    {
        project->QualTotals[j].dwInflow += project->StepQualTotals[j].dwInflow * tStep;
        project->QualTotals[j].wwInflow += project->StepQualTotals[j].wwInflow * tStep;
        project->QualTotals[j].gwInflow += project->StepQualTotals[j].gwInflow * tStep;
        project->QualTotals[j].iiInflow += project->StepQualTotals[j].iiInflow * tStep;
        project->QualTotals[j].exInflow += project->StepQualTotals[j].exInflow * tStep;
        project->QualTotals[j].flooding += project->StepQualTotals[j].flooding * tStep;
        project->QualTotals[j].outflow  += project->StepQualTotals[j].outflow * tStep;
        project->QualTotals[j].reacted  += project->StepQualTotals[j].reacted * tStep;
    }

    for ( j = 0; j < project->Nobjects[NODE]; j++)
    {
        NodeInflow[j] += project->Node[j].inflow * tStep;
        if ( project->Node[j].type == OUTFALL || project->Node[j].degree == 0 )
        {
            NodeOutflow[j] += project->Node[j].inflow * tStep;
        }
        else
        {
            NodeOutflow[j] += project->Node[j].outflow * tStep; 
            if ( project->Node[j].newVolume <= project->Node[j].fullVolume )
                NodeOutflow[j] += project->Node[j].overflow * tStep; 
        }
    }
}

//=============================================================================

double massbal_getStorage(Project *project, char isFinalStorage)
//
//  Input:   isFinalStorage = TRUE if at final time period
//  Output:  returns storage volume used (ft3)
//  Purpose: computes total system storage (nodes + links) filled
//
{
    int    j;
    double totalStorage = 0.0;
    double nodeStorage;

    // --- get volume in nodes
    for (j = 0; j < project->Nobjects[NODE]; j++)
    {
        nodeStorage = project->Node[j].newVolume;
        if ( isFinalStorage ) NodeOutflow[j] += nodeStorage;
        totalStorage += nodeStorage;
    }

    // --- add contribution from minimum surface area to final storage
    if ( isFinalStorage && project->RouteModel == DW )
    {
	    for (j = 0; j < project->Nobjects[NODE]; j++)
        {
	        if ( project->Node[j].type != STORAGE )
                totalStorage +=	project->Node[j].newDepth * project->MinSurfArea;
	    }
    }

    // --- skip final link storage for Steady Flow routing 
    if ( isFinalStorage && project->RouteModel == SF ) return totalStorage;

    // --- add on volume stored in links
    for (j = 0; j < project->Nobjects[LINK]; j++)
    {
        totalStorage += project->Link[j].newVolume;
    }
    return totalStorage;
}

//=============================================================================

void massbal_getSysFlows(Project *project, double f, double sysFlows[])
//
//  Input:   f = time weighting factor
//  Output:  sysFlows = array of total system flows
//  Purpose: retrieves time-weighted average of old and new system flows.
//
{
    double f1 = 1.0 - f;
    sysFlows[SYS_DWFLOW] = (f1 * project->OldStepFlowTotals.dwInflow +
                             f * project->StepFlowTotals.dwInflow) * UCF(FLOW);
    sysFlows[SYS_GWFLOW] = (f1 * project->OldStepFlowTotals.gwInflow +
                             f * project->StepFlowTotals.gwInflow) * UCF(FLOW);
    sysFlows[SYS_IIFLOW] = (f1 * project->OldStepFlowTotals.iiInflow +
                             f * project->StepFlowTotals.iiInflow) * UCF(FLOW);
    sysFlows[SYS_EXFLOW] = (f1 * project->OldStepFlowTotals.exInflow +
                             f * project->StepFlowTotals.exInflow) * UCF(FLOW);
    sysFlows[SYS_FLOODING] = (f1 * project->OldStepFlowTotals.flooding +
                               f * project->StepFlowTotals.flooding) * UCF(FLOW);
    sysFlows[SYS_OUTFLOW] = (f1 * project->OldStepFlowTotals.outflow +
                              f * project->StepFlowTotals.outflow) * UCF(FLOW);
    sysFlows[SYS_STORAGE] = (f1 * project->OldStepFlowTotals.finalStorage +
                              f * project->StepFlowTotals.finalStorage) * UCF(VOLUME);
}

//=============================================================================

double massbal_getRunoffError(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: computes runoff mass balance error.
//
{
    int    j;
    double totalInflow;
    double totalOutflow;

    // --- find final storage on all subcatchments
    project->RunoffTotals.finalStorage = 0.0;
    project->RunoffTotals.finalSnowCover = 0.0;
    for (j = 0; j < project->Nobjects[SUBCATCH]; j++)
    {
        project->RunoffTotals.finalStorage += subcatch_getStorage(project, j);
        project->RunoffTotals.finalSnowCover += snow_getSnowCover(project, j);
    }

    // --- get snow removed from system
    project->RunoffTotals.snowRemoved = project->Snow.removed;

    // --- compute % difference between total inflow and outflow
    totalInflow  = project->RunoffTotals.rainfall +
                   project->RunoffTotals.initStorage +
                   project->RunoffTotals.initSnowCover;
    totalOutflow = project->RunoffTotals.evap +
                   project->RunoffTotals.infil +
                   project->RunoffTotals.runoff +
                   project->RunoffTotals.snowRemoved +
                   project->RunoffTotals.finalStorage +
                   project->RunoffTotals.finalSnowCover;
    project->RunoffTotals.pctError = 0.0;
    if ( fabs(totalInflow - totalOutflow) < 1.0 )
    {
        project->RunoffTotals.pctError = TINY;
    }
    else if ( totalInflow > 0.0 )
    {
        project->RunoffTotals.pctError = 100.0 * (1.0 - totalOutflow / totalInflow);
    }
    else if ( totalOutflow > 0.0 )
    {
        project->RunoffTotals.pctError = 100.0 * (totalInflow / totalOutflow - 1.0);
    }
    project->RunoffError = project->RunoffTotals.pctError;
    return project->RunoffTotals.pctError;
}

//=============================================================================

double massbal_getLoadingError(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: computes runoff load mass balance error.
//
{
    int    j;
    double loadIn;
    double loadOut;
    double maxError = 0.0;

    for (j = 0; j < project->Nobjects[POLLUT]; j++)
    {
        // --- get final pollutant loading remaining on land surface
        project->LoadingTotals[j].finalLoad += massbal_getBuildup(project, j);

        // --- compute total load added to study area
        loadIn = project->LoadingTotals[j].initLoad +
                 project->LoadingTotals[j].buildup +
                 project->LoadingTotals[j].deposition;
    
        // --- compute total load removed from study area
        loadOut = project->LoadingTotals[j].sweeping +
                  project->LoadingTotals[j].infil +
                  project->LoadingTotals[j].bmpRemoval +
                  project->LoadingTotals[j].runoff +
                  project->LoadingTotals[j].finalLoad;

        // --- compute mass balance error
        project->LoadingTotals[j].pctError = 0.0;
        if ( fabs(loadIn - loadOut) < 0.001 )
        {
            project->LoadingTotals[j].pctError = TINY;
        }
        else if ( loadIn > 0.0 )
        {
            project->LoadingTotals[j].pctError = 100.0 * (1.0 - loadOut / loadIn);
        }
        else if ( loadOut > 0.0 )
        {
            project->LoadingTotals[j].pctError = 100.0 * (loadIn / loadOut - 1.0);
        }
        maxError = MAX(maxError, project->LoadingTotals[j].pctError);

        // --- report total counts as log10
        if ( project->Pollut[j].units == COUNT )
        {
            project->LoadingTotals[j].initLoad   = LOG10(project->LoadingTotals[j].initLoad);
            project->LoadingTotals[j].buildup    = LOG10(project->LoadingTotals[j].buildup);
            project->LoadingTotals[j].deposition = LOG10(project->LoadingTotals[j].deposition);
            project->LoadingTotals[j].sweeping   = LOG10(project->LoadingTotals[j].sweeping);
            project->LoadingTotals[j].infil      = LOG10(project->LoadingTotals[j].infil);
            project->LoadingTotals[j].bmpRemoval = LOG10(project->LoadingTotals[j].bmpRemoval);
            project->LoadingTotals[j].runoff     = LOG10(project->LoadingTotals[j].runoff);
            project->LoadingTotals[j].finalLoad  = LOG10(project->LoadingTotals[j].finalLoad);
        }
    }
    return maxError;
}

//=============================================================================

double massbal_getGwaterError(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: computes groundwater mass balance error.
//
{
    int    j;
    double totalInflow;
    double totalOutflow;

    // --- find final storage in groundwater
    project->GwaterTotals.finalStorage = 0.0;
    for ( j = 0; j < project->Nobjects[SUBCATCH]; j++ )
    {
        project->GwaterTotals.finalStorage += gwater_getVolume(project, j) * project->Subcatch[j].area;
    }

    // --- compute % difference between total inflow and outflow
    totalInflow  = project->GwaterTotals.infil +
                   project->GwaterTotals.initStorage;
    totalOutflow = project->GwaterTotals.upperEvap +
                   project->GwaterTotals.lowerEvap +
                   project->GwaterTotals.lowerPerc +
                   project->GwaterTotals.gwater +
                   project->GwaterTotals.finalStorage;
    project->GwaterTotals.pctError = 0.0;
    if ( fabs(totalInflow - totalOutflow) < 1.0 )
    {
        project->GwaterTotals.pctError = TINY;
    }
    else if ( totalInflow > 0.0 )
    {
        project->GwaterTotals.pctError = 100.0 * (1.0 - totalOutflow / totalInflow);
    }
    else if ( totalOutflow > 0.0 )
    {
        project->GwaterTotals.pctError = 100.0 * (totalInflow / totalOutflow - 1.0);
    }
    project->GwaterError = project->GwaterTotals.pctError;
    return project->GwaterTotals.pctError;
}

//=============================================================================

double massbal_getFlowError(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: computes flow routing mass balance error.
//
{
    double totalInflow;
    double totalOutflow;

    // --- get final volume of nodes and links
    project->FlowTotals.finalStorage = massbal_getStorage(project,TRUE);

    // --- compute % difference between total inflow and outflow
    totalInflow  = project->FlowTotals.dwInflow +
                   project->FlowTotals.wwInflow +
                   project->FlowTotals.gwInflow +
                   project->FlowTotals.iiInflow +
                   project->FlowTotals.exInflow +
                   project->FlowTotals.initStorage;
    totalOutflow = project->FlowTotals.flooding +
                   project->FlowTotals.outflow +
                   project->FlowTotals.evapLoss +
                   project->FlowTotals.seepLoss + 
                   project->FlowTotals.reacted + 
                   project->FlowTotals.finalStorage;
    project->FlowTotals.pctError = 0.0;
    if ( fabs(totalInflow - totalOutflow) < 1.0 )
    {
        project->FlowTotals.pctError = TINY;
    }
    else if ( totalInflow > 0.0 )
    {
        project->FlowTotals.pctError = 100.0 * (1.0 - totalOutflow / totalInflow);
    }
    else if ( totalOutflow > 0.0 )
    {
        project->FlowTotals.pctError = 100.0 * (totalInflow / totalOutflow - 1.0);
    }
    project->FlowError = project->FlowTotals.pctError;
    return project->FlowTotals.pctError;
}

//=============================================================================

double massbal_getQualError(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: computes water quality routing mass balance error.
//
{
    int    p;
    double maxQualError = 0.0;
    double totalInflow;
    double totalOutflow;
    double cf;

    // --- analyze each pollutant
    for (p = 0; p < project->Nobjects[POLLUT]; p++)
    {
        // --- get final mass stored in nodes and links
        project->QualTotals[p].finalStorage = massbal_getStoredMass(project, p);

        // --- compute % difference between total inflow and outflow
        totalInflow  = project->QualTotals[p].dwInflow +
                       project->QualTotals[p].wwInflow +
                       project->QualTotals[p].gwInflow +
                       project->QualTotals[p].iiInflow +
                       project->QualTotals[p].exInflow +
                       project->QualTotals[p].initStorage;
        totalOutflow = project->QualTotals[p].flooding +
                       project->QualTotals[p].outflow +
                       project->QualTotals[p].reacted +
                       project->QualTotals[p].finalStorage;
        project->QualTotals[p].pctError = 0.0;
        if ( fabs(totalInflow - totalOutflow) < 0.001 )
        {
            project->QualTotals[p].pctError = TINY;
        }
        else if ( totalInflow > 0.0 )
        {
            project->QualTotals[p].pctError = 100.0 * (1.0 - totalOutflow / totalInflow);
        }
        else if ( totalOutflow > 0.0 )
        {
            project->QualTotals[p].pctError = 100.0 * (totalInflow / totalOutflow - 1.0);
        }

        // --- update max. error among all pollutants
        if ( fabs(project->QualTotals[p].pctError) > fabs(maxQualError) )
        {
            maxQualError = project->QualTotals[p].pctError;
        }

        // --- convert totals to reporting units (lbs, kg, or Log(Count))
        cf = LperFT3;
        if ( project->Pollut[p].units == COUNT )
        {
            project->QualTotals[p].dwInflow     = LOG10(cf * project->QualTotals[p].dwInflow);
            project->QualTotals[p].wwInflow     = LOG10(cf * project->QualTotals[p].wwInflow);
            project->QualTotals[p].gwInflow     = LOG10(cf * project->QualTotals[p].gwInflow);
            project->QualTotals[p].iiInflow     = LOG10(cf * project->QualTotals[p].iiInflow);
            project->QualTotals[p].exInflow     = LOG10(cf * project->QualTotals[p].exInflow);
            project->QualTotals[p].flooding     = LOG10(cf * project->QualTotals[p].flooding);
            project->QualTotals[p].outflow      = LOG10(cf * project->QualTotals[p].outflow);
            project->QualTotals[p].reacted      = LOG10(cf * project->QualTotals[p].reacted);
            project->QualTotals[p].initStorage  = LOG10(cf * project->QualTotals[p].initStorage);
            project->QualTotals[p].finalStorage = LOG10(cf * project->QualTotals[p].finalStorage);
        }
        else
        {
            cf = cf * UCF(MASS);
            if ( project->Pollut[p].units == UG ) cf /= 1000.0;
            project->QualTotals[p].dwInflow     *= cf;
            project->QualTotals[p].wwInflow     *= cf; 
            project->QualTotals[p].gwInflow     *= cf; 
            project->QualTotals[p].iiInflow     *= cf; 
            project->QualTotals[p].exInflow     *= cf; 
            project->QualTotals[p].flooding     *= cf; 
            project->QualTotals[p].outflow      *= cf; 
            project->QualTotals[p].reacted      *= cf; 
            project->QualTotals[p].initStorage  *= cf; 
            project->QualTotals[p].finalStorage *= cf; 
        }
    }
    project->QualError = maxQualError;
    return maxQualError;
}
//=============================================================================

double massbal_getStepFlowError(Project *project)
//
//  Input:   none
//  Output:  returns fractional difference between total inflow and outflow.
//  Purpose: computes flow routing mass balance error at current time step.
//
{
    double totalInflow;
    double totalOutflow;

    // --- compute % difference between total inflow and outflow
    totalInflow  = project->StepFlowTotals.dwInflow +
                   project->StepFlowTotals.wwInflow +
                   project->StepFlowTotals.gwInflow +
                   project->StepFlowTotals.iiInflow +
                   project->StepFlowTotals.exInflow;
    totalOutflow = project->StepFlowTotals.flooding +
                   project->StepFlowTotals.outflow +
                   project->StepFlowTotals.evapLoss +
                   project->StepFlowTotals.seepLoss +
                   project->StepFlowTotals.reacted;
    if ( totalInflow > 0.0 )       return 1.0 - totalOutflow / totalInflow;
    else if ( totalOutflow > 0.0 ) return totalInflow / totalOutflow - 1.0;
    else return 0.0;
}

//=============================================================================

double massbal_getStoredMass(Project *project, int p)
//
//  Input:   p = pollutant index
//  Output:  returns mass of pollutant.
//  Purpose: computes mass of pollutant stored in conveyance network.
//
{
    int j;
    double storedMass = 0.0;

    // --- get mass stored in nodes
    for (j = 0; j < project->Nobjects[NODE]; j++)
        storedMass += project->Node[j].newVolume * project->Node[j].newQual[p];

    // --- get mass stored in links (except for Steady Flow routing)
    if ( project->RouteModel != SF )
    {
        for (j = 0; j < project->Nobjects[LINK]; j++)
            storedMass += project->Link[j].newVolume * project->Link[j].newQual[p];
    }
    return storedMass;
}

//=============================================================================
