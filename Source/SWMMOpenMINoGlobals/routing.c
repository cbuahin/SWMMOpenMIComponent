//-----------------------------------------------------------------------------
//   routing.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/19/14  (Build 5.1.000)
//   Author:   L. Rossman (EPA)
//             M. Tryby (EPA)
//
//   Conveyance system routing functions.
//
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "headers.h"
#include <swmm5.h>

//-----------------------------------------------------------------------------
// Shared variables
//-----------------------------------------------------------------------------
static int* SortedLinks;

//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)
//-----------------------------------------------------------------------------
// routing_open            (called by swmm_start in swmm5.c)
// routing_getRoutingStep  (called by swmm_step in swmm5.c)
// routing_execute         (called by swmm_step in swmm5.c)
// routing_close           (called by swmm_end in swmm5.c)

//-----------------------------------------------------------------------------
// Function declarations
//-----------------------------------------------------------------------------
static void addExternalInflows(DateTime currentDate);
static void addDryWeatherInflows(DateTime currentDate);
static void addWetWeatherInflows(double routingTime);
static void addGroundwaterInflows(double routingTime);
static void addRdiiInflows(DateTime currentDate);
static void addIfaceInflows(DateTime currentDate);
static void removeStorageLosses(double tStep);
static void removeOutflows(void);
static void removeConduitLosses(void); 
static int  inflowHasChanged(void);

//=============================================================================

int routing_open()
//
//  Input:   none
//  Output:  returns an error code
//  Purpose: initializes the routing analyzer.
//
{
    // --- open treatment system
    if ( !treatmnt_open() ) 
		return project->ErrorCode;

    // --- topologically sort the links
    SortedLinks = NULL;

    if ( project->Nobjects[LINK] > 0 )
    {
		int nim = project->Nobjects[LINK];

        SortedLinks = (int *) calloc(project->Nobjects[LINK], sizeof(int));
        if ( !SortedLinks )
        {
            report_writeErrorMsg(ERR_MEMORY, "");
            return project->ErrorCode;
        }
        toposort_sortLinks(SortedLinks);
        if ( project->ErrorCode ) return ErrorCode;
    }

    // --- open any routing interface files
    iface_openRoutingFiles();
   
    return project->ErrorCode;
}

//=============================================================================

void routing_close(int routingModel)
//
//  Input:   routingModel = routing method code
//  Output:  none
//  Purpose: closes down the routing analyzer.
//
{
    // --- close any routing interface files
    iface_closeRoutingFiles();

    // --- free allocated memory
    flowrout_close(routingModel);
    treatmnt_close();
    FREE(SortedLinks);
}

//=============================================================================

double routing_getRoutingStep(int routingModel, double fixedStep)
//
//  Input:   routingModel = routing method code
//           fixedStep = user-supplied time step (sec)
//  Output:  returns a routing time step (sec)
//  Purpose: determines time step used for flow routing at current time period.
//
{
    if ( project->Nobjects[LINK] == 0 ) return fixedStep;
    else return flowrout_getRoutingStep(routingModel, fixedStep);
}

//=============================================================================

void routing_execute(int routingModel, double routingStep)
//
//  Input:   routingModel = routing method code
//           routingStep = routing time step (sec)
//  Output:  none
//  Purpose: executes the routing process at the current time period.
//
{
    int      j;
    int      stepCount = 1;
    int      actionCount = 0;
    int      inSteadyState = FALSE;
    DateTime currentDate;
    double   stepFlowError;

    // --- update continuity with current state
    //     applied over 1/2 of time step
    if ( project->ErrorCode ) 
		return;

    massbal_updateRoutingTotals(routingStep/2.);

    // --- evaluate control rules at current date and elapsed time
    currentDate = getDateTime(project->NewRoutingTime);

	for (j = 0; j < project->Nobjects[LINK]; j++)
	{
		link_setTargetSetting(j);
	}

    controls_evaluate(currentDate, currentDate - project->StartDateTime, routingStep/SECperDAY);

    for (j=0; j<project->Nobjects[LINK]; j++)
    {
        if ( project->Link[j].targetSetting != Link[j].setting )
        {
            link_setSetting(j, routingStep);
            actionCount++;
        } 
    }

    // --- update value of elapsed routing time (in milliseconds)
    project->OldRoutingTime = project->NewRoutingTime;
    project->NewRoutingTime = NewRoutingTime + 1000.0 * routingStep;
    currentDate = getDateTime(project->NewRoutingTime);

    // --- initialize mass balance totals for time step
    stepFlowError = massbal_getStepFlowError();
    massbal_initTimeStepTotals();

    // --- replace old water quality state with new state
    if ( project->Nobjects[POLLUT] > 0 )
    {
		for (j = 0; j < project->Nobjects[NODE]; j++)
		{
			node_setOldQualState(j);
		}

		for (j = 0; j < project->Nobjects[LINK]; j++)
		{
			link_setOldQualState(j);
		}
    }

    // --- add lateral inflows to nodes
    for (j = 0; j < project->Nobjects[NODE]; j++)
    {
        project->Node[j].oldLatFlow  = Node[j].newLatFlow;
        project->Node[j].newLatFlow  = 0.0;
    }


    addExternalInflows(currentDate);
    addDryWeatherInflows(currentDate);
    addWetWeatherInflows(project->NewRoutingTime);
    addGroundwaterInflows(project->NewRoutingTime);
    addRdiiInflows(currentDate);
    addIfaceInflows(currentDate);
	
	// add openmi lateral inflows
	setOpenMILateralInflows();

    // --- check if can skip steady state periods
    if ( project->SkipSteadyState )
    {
        if ( project->OldRoutingTime == 0.0
        ||   actionCount > 0
        ||   fabs(stepFlowError) > project->SysFlowTol
        ||   inflowHasChanged() ) inSteadyState = FALSE;
        else inSteadyState = TRUE;
    }

    // --- find new hydraulic state if system has changed
    if ( inSteadyState == FALSE )
    {
        // --- replace old hydraulic state values with current ones
		for (j = 0; j < project->Nobjects[LINK]; j++)
		{
			link_setOldHydState(j);
		}

        for (j = 0; j < project->Nobjects[NODE]; j++)
        {
            node_setOldHydState(j);
            node_initInflow(j, routingStep);
        }

        // --- route flow through the drainage network
        if ( project->Nobjects[LINK] > 0 )
        {
            stepCount = flowrout_execute(SortedLinks, routingModel, routingStep);
        }
    }

    // --- route quality through the drainage network
    if ( project->Nobjects[POLLUT] > 0 && !project->IgnoreQuality ) 
    {
        qualrout_execute(routingStep);
    }

    // --- remove evaporation, infiltration & outflows from system
    removeStorageLosses(routingStep);
    removeConduitLosses();
    removeOutflows();
	
    // --- update continuity with new totals
    //     applied over 1/2 of routing step
    massbal_updateRoutingTotals(routingStep/2.);

    // --- update summary statistics
    if ( project->RptFlags.flowStats && project->Nobjects[LINK] > 0 )
    {
        stats_updateFlowStats(routingStep, currentDate, stepCount, inSteadyState);
    }
}

//=============================================================================

void addExternalInflows(DateTime currentDate)
//
//  Input:   currentDate = current date/time
//  Output:  none
//  Purpose: adds direct external inflows to nodes at current date.
//
{
    int     j, p;
    double  q, w;
    TExtInflow* inflow;

    // --- for each node with a defined external inflow
    for (j = 0; j < project->Nobjects[NODE]; j++)
    {
        inflow = project->Node[j].extInflow;
        if ( !inflow ) continue;

        // --- get flow inflow
        q = 0.0;
        while ( inflow )
        {
            if ( inflow->type == FLOW_INFLOW )
            {
                q = inflow_getExtInflow(inflow, currentDate);
                break;
            }
            else inflow = inflow->next;
        }
        if ( fabs(q) < FLOW_TOL ) q = 0.0;

        // --- add flow inflow to node's lateral inflow
        project->Node[j].newLatFlow += q;
        massbal_addInflowFlow(EXTERNAL_INFLOW, q);

        // --- add on any inflow (i.e., reverse flow) through an outfall
        if ( project->Node[j].type == OUTFALL && Node[j].oldNetInflow < 0.0 ) 
        {
            q = q - project->Node[j].oldNetInflow;
        }

        // --- get pollutant mass inflows
        inflow = project->Node[j].extInflow;
        while ( inflow )
        {
            if ( inflow->type != FLOW_INFLOW )
            {
                p = inflow->param;
                w = inflow_getExtInflow(inflow, currentDate);
                if ( inflow->type == CONCEN_INFLOW ) w *= q;
                project->Node[j].newQual[p] += w;
                massbal_addInflowQual(EXTERNAL_INFLOW, p, w);
            }
            inflow = inflow->next;
        }
    }
}

//=============================================================================

void addDryWeatherInflows(DateTime currentDate)
//
//  Input:   currentDate = current date/time
//  Output:  none
//  Purpose: adds dry weather inflows to nodes at current date.
//
{
    int      j, p;
    int      month, day, hour;
    double   q, w;
    TDwfInflow* inflow;

    // --- get month (zero-based), day-of-week (zero-based),
    //     & hour-of-day for routing date/time
    month = datetime_monthOfYear(currentDate) - 1;
    day   = datetime_dayOfWeek(currentDate) - 1;
    hour  = datetime_hourOfDay(currentDate);

    // --- for each node with a defined dry weather inflow
    for (j = 0; j < project->Nobjects[NODE]; j++)
    {
        inflow = project->Node[j].dwfInflow;
        if ( !inflow ) continue;

        // --- get flow inflow (i.e., the inflow whose param code is -1)
        q = 0.0;
        while ( inflow )
        {
            if ( inflow->param < 0 )
            {
                q = inflow_getDwfInflow(inflow, month, day, hour);
                break;
            }
            inflow = inflow->next;
        }
        if ( fabs(q) < FLOW_TOL ) q = 0.0;

        // --- add flow inflow to node's lateral inflow
        project->Node[j].newLatFlow += q;
        massbal_addInflowFlow(DRY_WEATHER_INFLOW, q);

        // --- add default DWF pollutant inflows
        for ( p = 0; p < project->Nobjects[POLLUT]; p++)
        {
            if ( project->Pollut[p].dwfConcen > 0.0 )
            {
                w = q * project->Pollut[p].dwfConcen;
                project->Node[j].newQual[p] += w;
                massbal_addInflowQual(DRY_WEATHER_INFLOW, p, w);
            }
        }

        // --- get pollutant mass inflows
        inflow = project->Node[j].dwfInflow;
        while ( inflow )
        {
            if ( inflow->param >= 0 )
            {
                p = inflow->param;
                w = q * inflow_getDwfInflow(inflow, month, day, hour);
                project->Node[j].newQual[p] += w;
                massbal_addInflowQual(DRY_WEATHER_INFLOW, p, w);

                // --- subtract off any default inflow
                if ( project->Pollut[p].dwfConcen > 0.0 )
                {
                    w = q * project->Pollut[p].dwfConcen;
                    project->Node[j].newQual[p] -= w;
                    massbal_addInflowQual(DRY_WEATHER_INFLOW, p, -w);
                }
            }
            inflow = inflow->next;
        }
    }
}

//=============================================================================

void addWetWeatherInflows(double routingTime)
//
//  Input:   routingTime = elasped time (millisec)
//  Output:  none
//  Purpose: adds runoff inflows to nodes at current elapsed time.
//
{
    int    i, j, p;
    double q, w;
    double f;

    // --- find where current routing time lies between latest runoff times
    if ( project->Nobjects[SUBCATCH] == 0 ) return;
    f = (routingTime - project->OldRunoffTime) / (project->NewRunoffTime - OldRunoffTime);
    if ( f < 0.0 ) f = 0.0;
    if ( f > 1.0 ) f = 1.0;

    // for each subcatchment outlet node,
    // add interpolated runoff flow & pollutant load to node's inflow
    for (i = 0; i < project->Nobjects[SUBCATCH]; i++)
    {
        j = project->Subcatch[i].outNode;
        if ( j >= 0)
        {
            // add runoff flow to lateral inflow
            q = subcatch_getWtdOutflow(i, f);     // current runoff flow
            project->Node[j].newLatFlow += q;
            massbal_addInflowFlow(WET_WEATHER_INFLOW, q);

            // add pollutant load
            for (p = 0; p < project->Nobjects[POLLUT]; p++)
            {
                w = subcatch_getWtdWashoff(i, p, f);
                project->Node[j].newQual[p] += w;
                massbal_addInflowQual(WET_WEATHER_INFLOW, p, w);
            }
        }
    }
}

//=============================================================================

void addGroundwaterInflows(double routingTime)
//
//  Input:   routingTime = elasped time (millisec)
//  Output:  none
//  Purpose: adds groundwater inflows to nodes at current elapsed time.
//
{
    int    i, j, p;
    double q, w;
    double f;
    TGroundwater* gw;

    // --- find where current routing time lies between latest runoff times
    if ( project->Nobjects[SUBCATCH] == 0 ) return;
    f = (routingTime - project->OldRunoffTime) / (project->NewRunoffTime - OldRunoffTime);
    if ( f < 0.0 ) f = 0.0;
    if ( f > 1.0 ) f = 1.0;

    // --- for each subcatchment
    for (i = 0; i < project->Nobjects[SUBCATCH]; i++)
    {
        // --- see if subcatch contains groundwater
        gw = project->Subcatch[i].groundwater;

        if ( gw )
        {
            // --- identify node receiving groundwater flow
            j = gw->node;

            if ( j >= 0 )
            {
                // add groundwater flow to lateral inflow
                q = ( (1.0 - f)*(gw->oldFlow) + f*(gw->newFlow) )
                    * project->Subcatch[i].area;

                if ( fabs(q) < FLOW_TOL )
					continue;

                project->Node[j].newLatFlow += q;
                massbal_addInflowFlow(GROUNDWATER_INFLOW, q);

                // add pollutant load (for positive inflow)
                if ( q > 0.0 )
                {
                    for (p = 0; p < project->Nobjects[POLLUT]; p++)
                    {
                        w = q * project->Pollut[p].gwConcen;
                        project->Node[j].newQual[p] += w;
                        massbal_addInflowQual(GROUNDWATER_INFLOW, p, w);
                    }
                }
            }
        }
    }
}

//=============================================================================

void addRdiiInflows(DateTime currentDate)
//
//  Input:   currentDate = current date/time
//  Output:  none
//  Purpose: adds RDII inflows to nodes at current date.
//
{
    int    i, j, p;
    double q, w;
    int    numRdiiNodes;

    // --- see if any nodes have RDII at current date
    numRdiiNodes = rdii_getNumRdiiFlows(currentDate);

    // --- add RDII flow to each node's lateral inflow
    for (i=0; i<numRdiiNodes; i++)
    {
        rdii_getRdiiFlow(i, &j, &q);
        if ( j < 0 ) continue;
        if ( fabs(q) < FLOW_TOL ) continue;
        project->Node[j].newLatFlow += q;
        massbal_addInflowFlow(RDII_INFLOW, q);

        // add pollutant load (for positive inflow)
        if ( q > 0.0 )
        {
            for (p = 0; p < project->Nobjects[POLLUT]; p++)
            {
                w = q * project->Pollut[p].rdiiConcen;
                project->Node[j].newQual[p] += w;
                massbal_addInflowQual(RDII_INFLOW, p, w);
            }
        }
    }
}

//=============================================================================

void addIfaceInflows(DateTime currentDate)
//
//  Input:   currentDate = current date/time
//  Output:  none
//  Purpose: adds inflows from routing interface file to nodes at current date.
//
{
    int    i, j, p;
    double q, w;
    int    numIfaceNodes;

    // --- see if any nodes have interface inflows at current date
    if ( project->Finflows.mode != USE_FILE ) return;
    numIfaceNodes = iface_getNumIfaceNodes(currentDate);

    // --- add interface flow to each node's lateral inflow
    for (i=0; i<numIfaceNodes; i++)
    {
        j = iface_getIfaceNode(i);
        if ( j < 0 ) continue;
        q = iface_getIfaceFlow(i);
        if ( fabs(q) < FLOW_TOL ) continue;
        project->Node[j].newLatFlow += q;
        massbal_addInflowFlow(EXTERNAL_INFLOW, q);

        // add pollutant load (for positive inflow)
        if ( q > 0.0 )
        {
            for (p = 0; p < project->Nobjects[POLLUT]; p++)
            {
                w = q * iface_getIfaceQual(i, p);
                project->Node[j].newQual[p] += w;
                massbal_addInflowQual(EXTERNAL_INFLOW, p, w);
            }
        }
    }
}

//=============================================================================

int  inflowHasChanged()
//
//  Input:   none
//  Output:  returns TRUE if external inflows or outfall flows have changed
//           from the previous time step
//  Purpose: checks if the hydraulic state of the system has changed from
//           the previous time step.
//
{
    int    j;
    double diff, qOld, qNew;

    // --- check if external inflows or outfall flows have changed 
    for (j = 0; j < project->Nobjects[NODE]; j++)
    {
        qOld = project->Node[j].oldLatFlow;
        qNew = project->Node[j].newLatFlow;
        if      ( fabs(qOld) > TINY ) diff = (qNew / qOld) - 1.0;
        else if ( fabs(qNew) > TINY ) diff = 1.0;
        else                    diff = 0.0;
        if ( fabs(diff) > project->LatFlowTol ) return TRUE;
        if ( project->Node[j].type == OUTFALL || Node[j].degree == 0 )
        {
            qOld = project->Node[j].oldFlowInflow;
            qNew = project->Node[j].inflow;
            if      ( fabs(qOld) > TINY ) diff = (qNew / qOld) - 1.0;
            else if ( fabs(qNew) > TINY ) diff = 1.0;
            else                          diff = 0.0;
            if ( fabs(diff) > project->LatFlowTol ) return TRUE;
        }
    }
    return FALSE;
}

//=============================================================================

void removeStorageLosses(double tStep)
//
//  Input:   tStep = routing time step (sec)
//  Output:  none
//  Purpose: adds rate of mass lost from all storage nodes due to evaporation
//           & seepage in current time step to overall mass balance.
//
{
    int    i, j, p;
 	double evapLoss = 0.0,
		   seepLoss = 0.0;
    double vRatio;

    // --- check each storage node
    for ( i = 0; i < project->Nobjects[NODE]; i++ )
    {
        if (project->Node[i].type == STORAGE)
        {
            // --- update total system storage losses
            evapLoss += project->Storage[project->Node[i].subIndex].evapLoss;
            seepLoss += project->Storage[project->Node[i].subIndex].seepLoss;
  
            // --- adjust storage concentrations for any evaporation loss
            if ( project->Nobjects[POLLUT] > 0 && project->Node[i].newVolume > FUDGE )
            {
                j = project->Node[i].subIndex;
                vRatio = 1.0 + (project->Storage[j].evapLoss / project->Node[i].newVolume);
                for ( p = 0; p < project->Nobjects[POLLUT]; p++ )
                {
                    project->Node[i].newQual[p] *= vRatio;
                }
            }
        }
    }

    // --- add loss rates (ft3/sec) to time step's mass balance 
    massbal_addNodeLosses(evapLoss/tStep, seepLoss/tStep);
}

//=============================================================================

void removeConduitLosses()
//
//  Input:   none
//  Output:  none
//  Purpose: adds rate of mass lost from all conduits due to evaporation
//           & seepage over current time step to overall mass balance.
//
{
	int i;
	double evapLoss = 0.0,
		   seepLoss = 0.0;

	for ( i = 0; i < project->Nobjects[LINK]; i++ )
	{
		if (project->Link[i].type == CONDUIT)
        {
			// --- update conduit losses
			evapLoss += project->Conduit[project->Link[i].subIndex].evapLossRate;
            seepLoss += project->Conduit[project->Link[i].subIndex].seepLossRate;
		}
	}
    massbal_addLinkLosses(evapLoss, seepLoss);
}

//=============================================================================

void removeOutflows()
//
//  Input:   none
//  Output:  none
//  Purpose: finds flows that leave the system and adds these to mass
//           balance totals.
//
{
    int    i, p;
    int    isFlooded;
    double q, w;

    for ( i = 0; i < project->Nobjects[NODE]; i++ )
    {
        // --- determine flows leaving the system
        q = node_getSystemOutflow(i, &isFlooded);
        if ( q != 0.0 )
        {
            massbal_addOutflowFlow(q, isFlooded);
            for ( p = 0; p < project->Nobjects[POLLUT]; p++ )
            {
                w = q * project->Node[i].newQual[p];
                massbal_addOutflowQual(p, w, isFlooded);
            }
        }
    }
}

//=============================================================================

void setOpenMINodeDepths()
//
//  Input:   none
//  Output:  none
//  Purpose: sets openmi node depths
//           
//
{
	int max = project->Nobjects[NODE];

	for (int j = 0; j < max; j++)
	{
		double value = 0;
		if (containsNodeDepth(j, &value))
		{
			TNode* node = &project->Node[j];
			node->newDepth = value;
		}
	}
}

//=============================================================================

void setOpenMINodeDepth(int index)
//
//  Input:   none
//  Output:  none
//  Purpose: sets openmi node depth
//           
//
{
	double value = 0;

	if (containsNodeDepth(index, &value))
	{
		TNode* node = &project->Node[index];
		node->newDepth = value;
	}
}
//=============================================================================

void setOpenMILateralInflows()
//
//  Input:   none
//  Output:  none
//  Purpose: sets openmi node lateral inflows
//           
//
{
	int max = project->Nobjects[NODE];

	for (int j = 0; j < max; j++)
	{
		double value = 0;

		if (containsNodeLateralInflow(j, &value))
		{
			TNode* node = &project->Node[j];
			node->newLatFlow += value;
			massbal_addInflowFlow(EXTERNAL_INFLOW, value);
		}
	}
}

//=============================================================================

void setOpenMILateralInflow(int index)
//
//  Input:   none
//  Output:  none
//  Purpose: sets openmi node lateral inflow
//           
//
{
	double value = 0;

	if (containsNodeLateralInflow(index, &value))
	{
		TNode* node = &project->Node[index];
		node->newLatFlow += value;
		massbal_addInflowFlow(EXTERNAL_INFLOW, value);
	}
}

//=============================================================================
