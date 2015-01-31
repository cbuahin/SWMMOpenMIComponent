//-----------------------------------------------------------------------------
//   dynwave.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/20/14   (5.1.001)
//             03/28/14   (5.1.002)
//   Author:   L. Rossman (EPA)
//             M. Tryby (EPA)
//             R. Dickinson (CDM)
//
//   Dynamic wave flow routing functions.
//
//   This module solves the dynamic wave flow routing equations using
//   Picard Iterations (i.e., a method of successive approximations)
//   to solve the explicit form of the continuity and momentum equations
//   for conduits.
//
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE


#include "headers.h"
//#include <malloc.h>
#include <stdlib.h>
#include <math.h>
//#include <omp.h>
#include <swmm5.h>

//-----------------------------------------------------------------------------
//     Constants 
//-----------------------------------------------------------------------------
static const double MINTIMESTEP =  0.5;     // min. time step (sec)
static const double OMEGA       =  0.5;     // under-relaxation parameter

//-----------------------------------------------------------------------------
//  Data Structures
//-----------------------------------------------------------------------------
typedef struct 
{
    char    converged;                 // TRUE if iterations for a node done
    double  newSurfArea;               // current surface area (ft2)
    double  oldSurfArea;               // previous surface area (ft2)
    double  sumdqdh;                   // sum of dqdh from adjoining links
    double  dYdT;                      // change in depth w.r.t. time (ft/sec)
} TXnode;

//-----------------------------------------------------------------------------
//  Shared Variables
//-----------------------------------------------------------------------------
static double  VariableStep;           // size of variable time step (sec)
static TXnode* Xnode;                  // extended nodal information

static double  Omega;                  // actual under-relaxation parameter
static int     Steps;                  // number of Picard iterations

//-----------------------------------------------------------------------------
//  Function declarations
//-----------------------------------------------------------------------------
static void   initRoutingStep(Project *project);
static void   initNodeStates(Project *project);
static void   findBypassedLinks(Project *project);
static void   findLimitedLinks(Project *project);

static void   findLinkFlows(Project *project,  double dt);
static int    isTrueConduit(Project *project,  int link);
static void   findNonConduitFlow(Project *project, int link, double dt);
static void   findNonConduitSurfArea(Project *project, int link);
static double getModPumpFlow(Project *project, int link, double q, double dt);
static void   updateNodeFlows(Project *project,  int link);

static int    findNodeDepths(Project *project, double dt);
static void   setNodeDepth(Project *project, int node, double dt);
static double getFloodedDepth(Project *project, int node, int canPond, double dV, double yNew,
              double yMax, double dt);

static double getVariableStep(Project *project, double maxStep);
static double getLinkStep(Project *project, double tMin, int *minLink);
static double getNodeStep(Project *project, double tMin, int *minNode);



//=============================================================================

void dynwave_init(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: initializes dynamic wave routing method.
//
{
    int i;

    VariableStep = 0.0;
    Xnode = (TXnode *) calloc(project->Nobjects[NODE], sizeof(TXnode));

    // --- initialize node surface areas
    for (i = 0; i < project->Nobjects[NODE]; i++ )
    {
        Xnode[i].newSurfArea = 0.0;
        Xnode[i].oldSurfArea = 0.0;
    }
    for (i = 0; i < project->Nobjects[LINK]; i++)
    {
        project->Link[i].flowClass = DRY;
        project->Link[i].dqdh = 0.0;
    }
}

//=============================================================================

void  dynwave_close()
//
//  Input:   none
//  Output:  none
//  Purpose: frees memory allocated for dynamic wave routing method.
//
{
    FREE(Xnode);
}

//=============================================================================

double dynwave_getRoutingStep(Project *project, double fixedStep)
//
//  Input:   fixedStep = user-supplied fixed time step (sec)
//  Output:  returns routing time step (sec)
//  Purpose: computes variable routing time step if applicable.
//
{
    // --- use user-supplied fixed step if variable step option turned off
    //     or if its smaller than the min. allowable variable time step
    if ( project->CourantFactor == 0.0 ) return fixedStep;
    if ( fixedStep < MINTIMESTEP ) return fixedStep;

    // --- at start of simulation (when current variable step is zero)
    //     use the minimum allowable time step
    if ( VariableStep == 0.0 )
    {
        VariableStep = MINTIMESTEP;
    }

    // --- otherwise compute variable step based on current flow solution
    else VariableStep = getVariableStep(project, fixedStep);

    // --- adjust step to be a multiple of a millisecond
    VariableStep = floor(1000.0 * VariableStep) / 1000.0;
    return VariableStep;
}

//=============================================================================

int dynwave_execute(Project *project, double tStep)
//
//  Input:   links = array of topo sorted links indexes
//           tStep = time step (sec)
//  Output:  returns number of iterations used
//  Purpose: routes flows through drainage network over current time step.
//
{
    int converged;

    // --- initialize
    if ( project->ErrorCode ) return 0;
    Steps = 0;
    converged = FALSE;
    Omega = OMEGA;
    initRoutingStep(project);



	// --- keep iterating until convergence 
	while (Steps < project->MaxTrials)
	{
		// --- execute a routing step & check for nodal convergence
		initNodeStates(project);

		findLinkFlows(project, tStep);

		converged = findNodeDepths(project, tStep);

		setOpenMINodeDepths(project);

		Steps++;
		if (Steps > 1)
		{
			if (converged)
				break;

			// --- check if link calculations can be skipped in next step
			findBypassedLinks(project);
		}
	}


		

    if ( !converged ) project->NonConvergeCount++;

    //  --- identify any capacity-limited conduits
    findLimitedLinks(project);
    return Steps;
}

//=============================================================================

void   initRoutingStep(Project *project)
{
    int i;
    for (i = 0; i < project->Nobjects[NODE]; i++)
    {
        Xnode[i].converged = FALSE;
        Xnode[i].dYdT = 0.0;
    }
    for (i = 0; i < project->Nobjects[LINK]; i++)
    {
        project->Link[i].bypassed = FALSE;
        project->Link[i].surfArea1 = 0.0;
        project->Link[i].surfArea2 = 0.0;
    }

    // --- a2 preserves conduit area from solution at last time step
    for ( i = 0; i < project->Nlinks[CONDUIT]; i++) project->Conduit[i].a2 = project->Conduit[i].a1;
}

//=============================================================================

void initNodeStates(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: initializes node's surface area, inflow & outflow
//
{
    int i;

    for (i = 0; i < project->Nobjects[NODE]; i++)
    {
        // --- initialize nodal surface area
        if ( project->AllowPonding )
        {
            Xnode[i].newSurfArea = node_getPondedArea(project, i, project->Node[i].newDepth);
        }
        else
        {
            Xnode[i].newSurfArea = node_getSurfArea(project, i, project->Node[i].newDepth);
        }
        if ( Xnode[i].newSurfArea < project->MinSurfArea )
        {
            Xnode[i].newSurfArea = project->MinSurfArea;
        }

        // --- initialize nodal inflow & outflow
        project->Node[i].inflow = project->Node[i].newLatFlow;
        project->Node[i].outflow = 0.0;
        Xnode[i].sumdqdh = 0.0;
    }
}

//=============================================================================

void   findBypassedLinks(Project *project)
{
    int i;
    for (i = 0; i < project->Nobjects[LINK]; i++)
    {
        if ( Xnode[project->Link[i].node1].converged &&
             Xnode[project->Link[i].node2].converged )
             project->Link[i].bypassed = TRUE;
        else project->Link[i].bypassed = FALSE;
    }
}

//=============================================================================

void  findLimitedLinks(Project *project)
//
//  Input:   none
//  Output:  none
//  Purpose: determines if a conduit link is capacity limited.
//
{
    int    j, n1, n2, k;
    double h1, h2;

    for (j = 0; j < project->Nobjects[LINK]; j++)
    {
        // ---- check only non-dummy conduit links
        if ( !isTrueConduit(project, j) ) return;

        // --- check that upstream end is full
        k = project->Link[j].subIndex;
        project->Conduit[k].capacityLimited = FALSE;
        if ( project->Conduit[k].a1 >= project->Link[j].xsect.aFull )
        {
            // --- check if HGL slope > conduit slope
            n1 = project->Link[j].node1;
            n2 = project->Link[j].node2;
            h1 = project->Node[n1].newDepth + project->Node[n1].invertElev;
            h2 = project->Node[n2].newDepth + project->Node[n2].invertElev;
            if ( (h1 - h2) > fabs(project->Conduit[k].slope) * project->Conduit[k].length )
                project->Conduit[k].capacityLimited = TRUE;
        }
    }
}

//=============================================================================

void findLinkFlows(Project *project, double dt)
{
    int i;

    // --- find new flow in each non-dummy conduit
    for ( i = 0; i < project->Nobjects[LINK]; i++)
    {
        if ( isTrueConduit(project, i) && !project->Link[i].bypassed )
            dwflow_findConduitFlow(project, i, Steps, Omega, dt);
    }


    // --- update inflow/outflows for nodes attached to non-dummy conduits
    for ( i = 0; i < project->Nobjects[LINK]; i++)
    {
	    if ( isTrueConduit(project, i) )
			updateNodeFlows(project, i);
    }

    // --- find new flows for all dummy conduits, pumps & regulators
    for ( i = 0; i < project->Nobjects[LINK]; i++)
    {
	    if ( !isTrueConduit(project, i) )
	    {	
            if ( !project->Link[i].bypassed ) 
				findNonConduitFlow(project, i, dt);

            updateNodeFlows(project, i);
        }
    }
}

//=============================================================================

int isTrueConduit(Project *project, int j)
{
    return ( project->Link[j].type == CONDUIT && project->Link[j].xsect.type != DUMMY );
}

//=============================================================================

void findNonConduitFlow(Project *project, int i, double dt)
//
//  Input:   i = link index
//           dt = time step (sec)
//  Output:  none
//  Purpose: finds new flow in a non-conduit-type link
//
{
    double qLast;                      // previous link flow (cfs)
    double qNew;                       // new link flow (cfs)

    // --- get link flow from last iteration
    qLast = project->Link[i].newFlow;
    project->Link[i].dqdh = 0.0;

    // --- get new inflow to link from its upstream node
    //     (link_getInflow returns 0 if flap gate closed or pump is offline)
    qNew = link_getInflow(project, i);
    if ( project->Link[i].type == PUMP ) qNew = getModPumpFlow(project, i, qNew, dt);

    // --- find surface area at each end of link
    findNonConduitSurfArea(project, i);

    // --- apply under-relaxation with flow from previous iteration;
    // --- do not allow flow to change direction without first being 0
    if ( Steps > 0 && project->Link[i].type != PUMP ) 
    {
        qNew = (1.0 - Omega) * qLast + Omega * qNew;
        if ( qNew * qLast < 0.0 ) qNew = 0.001 * SGN(qNew);
    }
    project->Link[i].newFlow = qNew;
}

//=============================================================================

double getModPumpFlow(Project *project, int i, double q, double dt)
//
//  Input:   i = link index
//           q = pump flow from pump curve (cfs)
//           dt = time step (sec)
//  Output:  returns modified pump flow rate (cfs)
//  Purpose: modifies pump curve pumping rate depending on amount of water
//           available at pump's inlet node.
//
{
    int    j = project->Link[i].node1;          // pump's inlet node index
    int    k = project->Link[i].subIndex;       // pump's index
    double newNetInflow;               // inflow - outflow rate (cfs)
    double netFlowVolume;              // inflow - outflow volume (ft3)
    double y;                          // node depth (ft)

    if ( q == 0.0 ) return q;

    // --- case where inlet node is a storage node: 
    //     prevent node volume from going negative
    if ( project->Node[j].type == STORAGE ) return node_getMaxOutflow(project, j, q, dt);

    // --- case where inlet is a non-storage node
    switch ( project->Pump[k].type )
    {
      // --- for Type1 pump, a volume is computed for inlet node,
      //     so make sure it doesn't go negative
      case TYPE1_PUMP:
        return node_getMaxOutflow(project, j, q, dt);

      // --- for other types of pumps, if pumping rate would make depth
      //     at upstream node negative, then set pumping rate = inflow
      case TYPE2_PUMP:
      case TYPE4_PUMP:
      case TYPE3_PUMP:
         newNetInflow = project->Node[j].inflow - project->Node[j].outflow - q;
         netFlowVolume = 0.5 * (project->Node[j].oldNetInflow + newNetInflow ) * dt;
         y = project->Node[j].oldDepth + netFlowVolume / Xnode[j].newSurfArea;
         if ( y <= 0.0 ) return project->Node[j].inflow;
    }
    return q;
}

//=============================================================================

void  findNonConduitSurfArea(Project *project, int i)
//
//  Input:   i = link index
//  Output:  none
//  Purpose: finds the surface area contributed by a non-conduit
//           link to its upstream and downstream nodes.
//
{
    if ( project->Link[i].type == ORIFICE )
    {
        project->Link[i].surfArea1 = project->Orifice[project->Link[i].subIndex].surfArea / 2.;
    }

	// --- no surface area for weirs to maintain SWMM 4 compatibility
/*
    else if ( project->Link[i].type == WEIR )
    {
        Xlink[i].surfArea1 = project->Weir[project->Link[i].subIndex].surfArea / 2.;
    }
*/

    else project->Link[i].surfArea1 = 0.0;
    project->Link[i].surfArea2 = project->Link[i].surfArea1;
    if ( project->Link[i].flowClass == UP_CRITICAL ||
        project->Node[project->Link[i].node1].type == STORAGE ) project->Link[i].surfArea1 = 0.0;
    if ( project->Link[i].flowClass == DN_CRITICAL ||
        project->Node[project->Link[i].node2].type == STORAGE ) project->Link[i].surfArea2 = 0.0;
}

//=============================================================================

void updateNodeFlows(Project *project, int i)
//
//  Input:   i = link index
//           q = link flow rate (cfs)
//  Output:  none
//  Purpose: updates cumulative inflow & outflow at link's end nodes.
//
{
    int    k, m;
    int    barrels = 1;
    int    n1 = project->Link[i].node1;
    int    n2 = project->Link[i].node2;
    double q = project->Link[i].newFlow;
    double uniformLossRate = 0.0;

    // --- compute any uniform seepage loss from a conduit
    if ( project->Link[i].type == CONDUIT )
    {
        k = project->Link[i].subIndex;
        uniformLossRate = project->Conduit[k].evapLossRate + project->Conduit[k].seepLossRate;
        barrels = project->Conduit[k].barrels;
    }

    // --- update total inflow & outflow at upstream/downstream nodes
    if ( q >= 0.0 )
    {
        project->Node[n1].outflow += q + uniformLossRate;
        project->Node[n2].inflow  += q;
    }
    else
    {
        project->Node[n1].inflow   -= q;
        project->Node[n2].outflow  -= q - uniformLossRate;
    }

    // --- add surf. area contributions to upstream/downstream nodes
    Xnode[project->Link[i].node1].newSurfArea += project->Link[i].surfArea1 * barrels;
    Xnode[project->Link[i].node2].newSurfArea += project->Link[i].surfArea2 * barrels;

    // --- update summed value of dqdh at each end node
    Xnode[project->Link[i].node1].sumdqdh += project->Link[i].dqdh;
    if ( project->Link[i].type == PUMP )
    {
        k = project->Link[i].subIndex;
        m = project->Pump[k].pumpCurve;
        if ( project->Curve[m].curveType != PUMP4_CURVE )
        {
            Xnode[n2].sumdqdh += project->Link[i].dqdh;
        }
    }
    else Xnode[n2].sumdqdh += project->Link[i].dqdh;
}

//=============================================================================

int findNodeDepths(Project *project, double dt)
{
    int i;
    int converged;      // convergence flag
    double yOld;        // previous node depth (ft)

    // --- compute outfall depths based on flow in connecting link
    for ( i = 0; i < project->Nobjects[LINK]; i++ ) 
		link_setOutfallDepth(project, i);

    // --- compute new depth for all non-outfall nodes and determine if
    //     depth change from previous iteration is below tolerance
    converged = TRUE;
    for ( i = 0; i < project->Nobjects[NODE]; i++ )
    {
        if ( project->Node[i].type == OUTFALL ) 
			continue;

        yOld = project->Node[i].newDepth;
        setNodeDepth(project, i, dt);
        Xnode[i].converged = TRUE;

        if ( fabs(yOld - project->Node[i].newDepth) > project->HeadTol )
        {
            converged = FALSE;
            Xnode[i].converged = FALSE;
        }
    }
    return converged;
}

//=============================================================================

void setNodeDepth(Project *project, int i, double dt)
//
//  Input:   i  = node index
//           dt = time step (sec)
//  Output:  none
//  Purpose: sets depth at non-outfall node after current time step.
//
{
    int     canPond;                   // TRUE if node can pond overflows
    int     isPonded;                  // TRUE if node is currently ponded 
    double  dQ;                        // inflow minus outflow at node (cfs)
    double  dV;                        // change in node volume (ft3)
    double  dy;                        // change in node depth (ft)
    double  yMax;                      // max. depth at node (ft)
    double  yOld;                      // node depth at previous time step (ft)
    double  yLast;                     // previous node depth (ft)
    double  yNew;                      // new node depth (ft)
    double  yCrown;                    // depth to node crown (ft)
    double  surfArea;                  // node surface area (ft2)
    double  denom;                     // denominator term
    double  corr;                      // correction factor
    double  f;                         // relative surcharge depth

    // --- see if node can pond water above it
    canPond = (project->AllowPonding && project->Node[i].pondedArea > 0.0);
    isPonded = (canPond && project->Node[i].newDepth > project->Node[i].fullDepth);

    // --- initialize values
    yCrown = project->Node[i].crownElev - project->Node[i].invertElev;
    yOld = project->Node[i].oldDepth;
    yLast = project->Node[i].newDepth;
    project->Node[i].overflow = 0.0;
    surfArea = Xnode[i].newSurfArea;

    // --- determine average net flow volume into node over the time step
    dQ = project->Node[i].inflow - project->Node[i].outflow;
    dV = 0.5 * (project->Node[i].oldNetInflow + dQ) * dt - node_getLosses(project, i, dt);

    // --- if node not surcharged, base depth change on surface area        
    if ( yLast <= yCrown || project->Node[i].type == STORAGE || isPonded )
    {
        dy = dV / surfArea;
        yNew = yOld + dy;

        // --- save non-ponded surface area for use in surcharge algorithm     //(5.1.002)
        if ( !isPonded ) 
			Xnode[i].oldSurfArea = surfArea;                      //(5.1.002)

        // --- apply under-relaxation to new depth estimate
        if ( Steps > 0 )
        {
            yNew = (1.0 - Omega) * yLast + Omega * yNew;
        }

        // --- don't allow a ponded node to drop much below full depth
        if ( isPonded && yNew < project->Node[i].fullDepth )
            yNew = project->Node[i].fullDepth - FUDGE;
    }

    // --- if node surcharged, base depth change on dqdh
    //     NOTE: depth change is w.r.t depth from previous
    //     iteration; also, do not apply under-relaxation.
    else
    {
        // --- apply correction factor for upstream terminal nodes
        corr = 1.0;
        if ( project->Node[i].degree < 0 ) 
			corr = 0.6;

        // --- allow surface area from last non-surcharged condition
        //     to influence dqdh if depth close to crown depth
        denom = Xnode[i].sumdqdh;
        if ( yLast < 1.25 * yCrown )
        {
            f = (yLast - yCrown) / yCrown;
            denom += (Xnode[i].oldSurfArea/dt - Xnode[i].sumdqdh) * exp(-15.0 * f);
        }

        // --- compute new estimate of node depth
        if ( denom == 0.0 ) 
			dy = 0.0;
        else 
			dy = corr * dQ / denom;
        
		yNew = yLast + dy;
        
		if ( yNew < yCrown ) 
			yNew = yCrown - FUDGE;

        // --- don't allow a newly ponded node to rise much above full depth
        if ( canPond && yNew > project->Node[i].fullDepth )
            yNew = project->Node[i].fullDepth + FUDGE;
    }

    // --- depth cannot be negative
    if ( yNew < 0 )
		yNew = 0.0;

    // --- determine max. non-flooded depth
    yMax = project->Node[i].fullDepth;

    if ( canPond == FALSE )
		yMax += project->Node[i].surDepth;

    // --- find flooded depth & volume
    if ( yNew > yMax )
    {
        yNew = getFloodedDepth(project, i, canPond, dV, yNew, yMax, dt);
    }
	else
	{
		project->Node[i].newVolume = node_getVolume(project, i, yNew);
	}

    // --- compute change in depth w.r.t. time
    Xnode[i].dYdT = fabs(yNew - yOld) / dt;

    // --- save new depth for node
    project->Node[i].newDepth = yNew;

}

//=============================================================================

double getFloodedDepth(Project *project, int i, int canPond, double dV, double yNew,
                       double yMax, double dt)
//
//  Input:   i  = node index
//           canPond = TRUE if water can pond over node
//           isPonded = TRUE if water is currently ponded
//           dV = change in volume over time step (ft3)
//           yNew = current depth at node (ft)
//           yMax = max. depth at node before ponding (ft)
//           dt = time step (sec)
//  Output:  returns depth at node when flooded (ft)
//  Purpose: computes depth, volume and overflow for a flooded node.
//
{
    if ( canPond == FALSE )
    {
        project->Node[i].overflow = dV / dt;
        project->Node[i].newVolume = project->Node[i].fullVolume;
        yNew = yMax;
    }
    else
    {
        project->Node[i].newVolume = MAX((project->Node[i].oldVolume+dV), project->Node[i].fullVolume);
        project->Node[i].overflow = (project->Node[i].newVolume -
            MAX(project->Node[i].oldVolume, project->Node[i].fullVolume)) / dt;
    }
    if ( project->Node[i].overflow < FUDGE ) project->Node[i].overflow = 0.0;
    return yNew;

}

//=============================================================================

double getVariableStep(Project *project, double maxStep)
//
//  Input:   maxStep = user-supplied max. time step (sec)
//  Output:  returns time step (sec)
//  Purpose: finds time step that satisfies stability criterion but
//           is no greater than the user-supplied max. time step.
//
{
    int    minLink = -1;                // index of link w/ min. time step
    int    minNode = -1;                // index of node w/ min. time step
    double tMin;                        // allowable time step (sec)
    double tMinLink;                    // allowable time step for links (sec)
    double tMinNode;                    // allowable time step for nodes (sec)

    // --- find stable time step for links & then nodes
    tMin = maxStep;
    tMinLink = getLinkStep(project, tMin, &minLink);
    tMinNode = getNodeStep(project, tMinLink, &minNode);

    // --- use smaller of the link and node time step
    tMin = tMinLink;
    if ( tMinNode < tMin )
    {
        tMin = tMinNode ;
        minLink = -1;
    }

    // --- update count of times the minimum node or link was critical
    stats_updateCriticalTimeCount(minNode, minLink);

    // --- don't let time step go below an absolute minimum
    if ( tMin < MINTIMESTEP ) tMin = MINTIMESTEP;
    return tMin;
}

//=============================================================================

double getLinkStep(Project *project, double tMin, int *minLink)
//
//  Input:   tMin = critical time step found so far (sec)
//  Output:  minLink = index of link with critical time step;
//           returns critical time step (sec)
//  Purpose: finds critical time step for conduits based on Courant criterion.
//
{
    int    i;                           // link index
    int    k;                           // conduit index
    double q;                           // conduit flow (cfs)
    double t;                           // time step (sec)
    double tLink = tMin;                // critical link time step (sec)

    // --- examine each conduit link
    for ( i = 0; i < project->Nobjects[LINK]; i++ )
    {
        if ( project->Link[i].type == CONDUIT )
        {
           // --- skip conduits with negligible flow, area or Fr
            k = project->Link[i].subIndex;
            q = fabs(project->Link[i].newFlow) / project->Conduit[k].barrels;
            if ( q <= 0.05 * project->Link[i].qFull
            ||   project->Conduit[k].a1 <= FUDGE
            ||   project->Link[i].froude <= 0.01 
               ) continue;

            // --- compute time step to satisfy Courant condition
            t = project->Link[i].newVolume / project->Conduit[k].barrels / q;
            t = t * project->Conduit[k].modLength / link_getLength(project, i);
            t = t * project->Link[i].froude / (1.0 + project->Link[i].froude) * project->CourantFactor;

            // --- update critical link time step
            if ( t < tLink )
            {
                tLink = t;
                *minLink = i;
            }
        }
    }
    return tLink;
}

//=============================================================================

double getNodeStep(Project *project, double tMin, int *minNode)
//
//  Input:   tMin = critical time step found so far (sec)
//  Output:  minNode = index of node with critical time step;
//           returns critical time step (sec)
//  Purpose: finds critical time step for nodes based on max. allowable
//           projected change in depth.
//
{
    int    i;                           // node index
    double maxDepth;                    // max. depth allowed at node (ft)
    double dYdT;                        // change in depth per unit time (ft/sec)
    double t1;                          // time needed to reach depth limit (sec)
    double tNode = tMin;                // critical node time step (sec)

    // --- find smallest time so that estimated change in nodal depth
    //     does not exceed safety factor * maxdepth
    for ( i = 0; i < project->Nobjects[NODE]; i++ )
    {
        // --- see if node can be skipped
        if ( project->Node[i].type == OUTFALL ) continue;
        if ( project->Node[i].newDepth <= FUDGE) continue;
        if ( project->Node[i].newDepth  + FUDGE >=
             project->Node[i].crownElev - project->Node[i].invertElev ) continue;

        // --- define max. allowable depth change using crown elevation
        maxDepth = (project->Node[i].crownElev - project->Node[i].invertElev) * 0.25;
        if ( maxDepth < FUDGE ) continue;
        dYdT = Xnode[i].dYdT;
        if (dYdT < FUDGE ) continue;

        // --- compute time to reach max. depth & compare with critical time
        t1 = maxDepth / dYdT;
        if ( t1 < tNode )
        {
            tNode = t1;
            *minNode = i;
        }
    }
    return tNode;
}
