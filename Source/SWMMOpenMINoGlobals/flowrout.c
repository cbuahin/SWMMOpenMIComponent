//-----------------------------------------------------------------------------
//   flowrout.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/19/14  (Build 5.1.00)
//   Author:   L. Rossman (EPA)
//             M. Tryby (EPA)
//
//   Flow routing functions.
//
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE


#include "headers.h"
#include <stdlib.h>
#include <math.h>

//-----------------------------------------------------------------------------
//  Constants
//-----------------------------------------------------------------------------
static const double OMEGA   = 0.55;    // under-relaxation parameter
static const int    MAXITER = 10;      // max. iterations for storage updating
static const double STOPTOL = 0.005;   // storage updating stopping tolerance

//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)
//-----------------------------------------------------------------------------
//  flowrout_init            (called by routing_open)
//  flowrout_close           (called by routing_close)
//  flowrout_getRoutingStep  (called routing_getRoutingStep)
//  flowrout_execute         (called routing_execute)

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------
static void   initLinkDepths(Project* project);
static void   initNodeDepths(Project* project);
static void   initNodes(Project* project);
static void   initLinks(Project* project);
static void   validateTreeLayout(Project* project);      
static void   validateGeneralLayout(Project* project);
static void   updateStorageState(Project* project, int i, int j, int links[], double dt);
static double getStorageOutflow(Project* project, int node, int j, int links[], double dt);
static double getLinkInflow(Project* project, int link, double dt);
static void   setNewNodeState(Project* project, int node, double dt);
static void   setNewLinkState(Project* project, int link);
static void   updateNodeDepth(Project* project, int node, double y);
static int    steadyflow_execute(Project* project, int link, double* qin, double* qout,
              double tStep);


//=============================================================================

void flowrout_init(Project* project, int routingModel)
//
//  Input:   routingModel = routing model code
//  Output:  none
//  Purpose: initializes flow routing system.
//
{
    // --- initialize for dynamic wave routing 
    if ( routingModel == DW )
    {
        // --- check for valid conveyance network layout
        validateGeneralLayout(project);
        dynwave_init(project);

        // --- initialize node & link depths if not using a hotstart file
        if ( project->Fhotstart1.mode == NO_FILE )
        {
            initNodeDepths(project);
            initLinkDepths(project);
        }
    }

    // --- validate network layout for kinematic wave routing
    else validateTreeLayout(project);

    // --- initialize node & link volumes
    initNodes(project);
    initLinks(project);
}

//=============================================================================

void  flowrout_close(Project* project, int routingModel)
//
//  Input:   routingModel = routing method code
//  Output:  none
//  Purpose: closes down routing method used.
//
{
    if ( routingModel == DW ) dynwave_close(project);
}

//=============================================================================

double flowrout_getRoutingStep(Project* project, int routingModel, double fixedStep)
//
//  Input:   routingModel = type of routing method used
//           fixedStep = user-assigned max. routing step (sec)
//  Output:  returns adjusted value of routing time step (sec)
//  Purpose: finds variable time step for dynamic wave routing.
//
{
    if ( routingModel == DW )
    {
        return dynwave_getRoutingStep(project,fixedStep);
    }
    return fixedStep;
}

//=============================================================================

int flowrout_execute(Project* project, int links[], int routingModel, double tStep)
//
//  Input:   links = array of link indexes in topo-sorted order
//           routingModel = type of routing method used
//           tStep = routing time step (sec)
//  Output:  returns number of computational steps taken
//  Purpose: routes flow through conveyance network over current time step.
//
{
    int   i, j;
    int   n1;                          // upstream node of link
    double qin;                        // link inflow (cfs)
    double qout;                       // link outflow (cfs)
    double steps;                      // computational step count

    // --- set overflows to drain any ponded water
    if ( project->ErrorCode )
		return 0;
    for (j = 0; j <project-> Nobjects[NODE]; j++)
    {
        project-> Node[j].updated = FALSE;
        project-> Node[j].overflow = 0.0;
        if ( project-> Node[j].type != STORAGE
        &&   project-> Node[j].newVolume > project-> Node[j].fullVolume )
        {
            project-> Node[j].overflow = (project-> Node[j].newVolume - project-> Node[j].fullVolume)/tStep;
        }
    }

    // --- execute dynamic wave routing if called for
    if ( routingModel == DW )
    {
        return dynwave_execute(project,tStep);
    }

    // --- otherwise examine each link, moving from upstream to downstream
    steps = 0.0;
    for (i = 0; i <project-> Nobjects[LINK]; i++)
    {
        // --- see if upstream node is a storage unit whose state needs updating
        j = links[i];
        n1 = project-> Link[j].node1;

        if ( project-> Node[n1].type == STORAGE ) 
			updateStorageState(project,n1, i, links, tStep);

        // --- retrieve inflow at upstream end of link
        qin  = getLinkInflow(project,j, tStep);

        // route flow through link
        if ( routingModel == SF )
            steps += steadyflow_execute(project,j, &qin, &qout, tStep);
        else 
			steps += kinwave_execute(project,j, &qin, &qout, tStep);

        project-> Link[j].newFlow = qout;
		

        // adjust outflow at upstream node and inflow at downstream node
        project-> Node[ project-> Link[j].node1 ].outflow += qin;
		project-> Node[project-> Link[j].node2].inflow += project-> Link[j].newFlow;
    }

    if (project-> Nobjects[LINK] > 0 ) steps /=project-> Nobjects[LINK];

    // --- update state of each non-updated node and link
	for (j = 0; j<project->Nobjects[NODE]; j++) setNewNodeState(project,j, tStep);
	for (j = 0; j<project->Nobjects[LINK]; j++) setNewLinkState(project,j);
    return (int)(steps+0.5);
}

//=============================================================================

void validateTreeLayout(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: validates tree-like conveyance system layout used for Steady
//           and Kinematic Wave flow routing
//
{
    int    j;

    // --- check nodes
    for ( j = 0; j <project-> Nobjects[NODE]; j++ )
    {
        switch ( project-> Node[j].type )
        {
          // --- dividers must have only 2 outlet links
          case DIVIDER:
            if ( project-> Node[j].degree > 2 )
            {
                report_writeErrorMsg(project,ERR_DIVIDER, project-> Node[j].ID);
            }
            break;

          // --- outfalls cannot have any outlet links
          case OUTFALL:
            if ( project-> Node[j].degree > 0 )
            {
                report_writeErrorMsg(project,ERR_OUTFALL, project-> Node[j].ID);
            }
            break;

          // --- storage nodes can have multiple outlets
          case STORAGE: break;

          // --- all other nodes allowed only one outlet link
          default:
            if ( project-> Node[j].degree > 1 )
            {
                report_writeErrorMsg(project,ERR_MULTI_OUTLET, project-> Node[j].ID);
            }
        }
    }

    // ---  check links 
    for (j=0; j<project->Nobjects[LINK]; j++)
    {
        switch ( project-> Link[j].type )
        {
          // --- non-dummy conduits cannot have adverse slope
          case CONDUIT:
              if ( project-> Conduit[project-> Link[j].subIndex].slope < 0.0 &&
                   project-> Link[j].xsect.type != DUMMY )
              {
                  report_writeErrorMsg(project,ERR_SLOPE, project-> Link[j].ID);
              }
              break;

          // --- regulator links must be outlets of storage nodes
          case ORIFICE:
          case WEIR:
          case OUTLET:
            if ( project-> Node[project-> Link[j].node1].type != STORAGE )
            {
                report_writeErrorMsg(project,ERR_REGULATOR, project-> Link[j].ID);
            }
        }
    }
}

//=============================================================================

void validateGeneralLayout(Project* project)
//
//  Input:   none
//  Output:  nonw
//  Purpose: validates general conveyance system layout.
//
{
    int i, j;
    int outletCount = 0;

    // --- use node inflow attribute to count inflow connections
    for ( i=0; i<project->Nobjects[NODE]; i++ ) project-> Node[i].inflow = 0.0;

    // --- examine each link
    for ( j = 0; j <project-> Nobjects[LINK]; j++ )
    {
        // --- update inflow link count of downstream node
        i = project-> Link[j].node1;
        if ( project-> Node[i].type != OUTFALL ) i = project-> Link[j].node2;
        project-> Node[i].inflow += 1.0;

        // --- if link is dummy link or ideal pump then it must
        //     be the only link exiting the upstream node 
        if ( (project-> Link[j].type == CONDUIT && project-> Link[j].xsect.type == DUMMY) ||
             (project-> Link[j].type == PUMP &&
              project->Pump[project-> Link[j].subIndex].type == IDEAL_PUMP) )
        {
            i = project-> Link[j].node1;
            if ( project-> Link[j].direction < 0 ) i = project-> Link[j].node2;
            if ( project-> Node[i].degree > 1 )
            {
                report_writeErrorMsg(project,ERR_DUMMY_LINK, project-> Node[i].ID);
            }
        }
    }

    // --- check each node to see if it qualifies as an outlet node
    //     (meaning that degree = 0)
    for ( i = 0; i <project-> Nobjects[NODE]; i++ )
    {
        // --- if node is of type Outfall, check that it has only 1
        //     connecting link (which can either be an outflow or inflow link)
        if ( project-> Node[i].type == OUTFALL )
        {
            if ( project-> Node[i].degree + (int)project-> Node[i].inflow > 1 )
            {
                report_writeErrorMsg(project,ERR_OUTFALL, project-> Node[i].ID);
            }
            else outletCount++;
        }
    }
    if ( outletCount == 0 ) report_writeErrorMsg(project,ERR_NO_OUTLETS, "");

    // --- reset node inflows back to zero
    for ( i = 0; i <project-> Nobjects[NODE]; i++ )
    {
        if ( project-> Node[i].inflow == 0.0 ) project-> Node[i].degree = -project-> Node[i].degree;
        project-> Node[i].inflow = 0.0;
    }
}

//=============================================================================

void initNodeDepths(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: sets initial depth at nodes for Dynamic Wave flow routing.
//
{
    int   i;                           // link or node index
    int   n;                           // node index
    double y;                          // node water depth (ft)

    // --- use project-> Node[].inflow as a temporary accumulator for depth in 
    //     connecting links and project-> Node[].outflow as a temporary counter
    //     for the number of connecting links
    for (i = 0; i <project-> Nobjects[NODE]; i++)
    {
        project-> Node[i].inflow  = 0.0;
        project-> Node[i].outflow = 0.0;
    }

    // --- total up flow depths in all connecting links into nodes
    for (i = 0; i <project-> Nobjects[LINK]; i++)
    {
        if ( project-> Link[i].newDepth > FUDGE ) y = project-> Link[i].newDepth + project-> Link[i].offset1;
        else y = 0.0;
        n = project-> Link[i].node1;
        project-> Node[n].inflow += y;
        project-> Node[n].outflow += 1.0;
        n = project-> Link[i].node2;
        project-> Node[n].inflow += y;
        project-> Node[n].outflow += 1.0;
    }

    // --- if no user-supplied depth then set initial depth at non-storage/
    //     non-outfall nodes to average of depths in connecting links
    for ( i = 0; i <project-> Nobjects[NODE]; i++ )
    {
        if ( project-> Node[i].type == OUTFALL ) continue;
        if ( project-> Node[i].type == STORAGE ) continue;
        if ( project-> Node[i].initDepth > 0.0 ) continue;
        if ( project-> Node[i].outflow > 0.0 )
        {
            project-> Node[i].newDepth = project-> Node[i].inflow / project-> Node[i].outflow;
        }
    }

    // --- compute initial depths at all outfall nodes
    for ( i = 0; i <project-> Nobjects[LINK]; i++ ) link_setOutfallDepth(project,i);
}

//=============================================================================
         
void initLinkDepths(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: sets initial flow depths in conduits under Dyn. Wave routing.
//
{
    int    i;                          // link index
    double y, y1, y2;                  // depths (ft)

    // --- examine each link
    for (i = 0; i <project-> Nobjects[LINK]; i++)
    {
        // --- examine each conduit
        if ( project-> Link[i].type == CONDUIT )
        {
            // --- skip conduits with user-assigned initial flows
            //     (their depths have already been set to normal depth)
            if ( project-> Link[i].q0 != 0.0 ) continue;

            // --- set depth to average of depths at end nodes
            y1 = project-> Node[project-> Link[i].node1].newDepth - project-> Link[i].offset1;
            y1 = MAX(y1, 0.0);
            y1 = MIN(y1, project-> Link[i].xsect.yFull);
            y2 = project-> Node[project-> Link[i].node2].newDepth - project-> Link[i].offset2;
            y2 = MAX(y2, 0.0);
            y2 = MIN(y2, project-> Link[i].xsect.yFull);
            y = 0.5 * (y1 + y2);
            y = MAX(y, FUDGE);
            project-> Link[i].newDepth = y;
        }
    }
}

//=============================================================================

void initNodes(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: sets initial inflow/outflow and volume for each node
//
{
    int i;

    for ( i = 0; i <project-> Nobjects[NODE]; i++ )
    {
        // --- set default crown elevations here
        project-> Node[i].crownElev = project-> Node[i].invertElev;

        // --- initialize node inflow and outflow
        project-> Node[i].inflow = project-> Node[i].newLatFlow;
        project-> Node[i].outflow = 0.0;

        // --- initialize node volume
        project-> Node[i].newVolume = 0.0;
        if ( project-> AllowPonding &&
             project-> Node[i].pondedArea > 0.0 &&
             project-> Node[i].newDepth > project-> Node[i].fullDepth )
        {
            project-> Node[i].newVolume = project-> Node[i].fullVolume +
                                (project-> Node[i].newDepth - project-> Node[i].fullDepth) *
                                project-> Node[i].pondedArea;
        }
        else project-> Node[i].newVolume = node_getVolume(project,i, project-> Node[i].newDepth);
    }

    // --- update nodal inflow/outflow at ends of each link
    //     (needed for Steady Flow & Kin. Wave routing)
    for ( i = 0; i <project-> Nobjects[LINK]; i++ )
    {
        if ( project-> Link[i].newFlow >= 0.0 )
        {
            project-> Node[project-> Link[i].node1].outflow += project-> Link[i].newFlow;
            project-> Node[project-> Link[i].node2].inflow  += project-> Link[i].newFlow;
        }
        else
        {
            project-> Node[project-> Link[i].node1].inflow   -= project-> Link[i].newFlow;
            project-> Node[project-> Link[i].node2].outflow  -= project-> Link[i].newFlow;
        }
    }
}

//=============================================================================

void initLinks(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: sets initial upstream/downstream conditions in links.
//
//  Note: initNodes() must have been called first to properly
//        initialize each node's crown elevation.
//
{
    int    i;                          // link index
    int    j;                          // node index
    int    k;                          // conduit or pump index
    double z;                          // crown elev. (ft)

    // --- examine each link
    for ( i = 0; i <project-> Nobjects[LINK]; i++ )
    {
        // --- examine each conduit
        if ( project-> Link[i].type == CONDUIT )
        {
            // --- assign initial flow to both ends of conduit
            k = project-> Link[i].subIndex;
            project-> Conduit[k].q1 = project-> Link[i].newFlow / project-> Conduit[k].barrels;
            project-> Conduit[k].q2 = project-> Conduit[k].q1;

            project-> Conduit[k].q1Old = project-> Conduit[k].q1;
            project-> Conduit[k].q2Old = project-> Conduit[k].q2;

            // --- find areas based on initial flow depth
            project-> Conduit[k].a1 = xsect_getAofY(project,&project-> Link[i].xsect, project-> Link[i].newDepth);
            project-> Conduit[k].a2 = project-> Conduit[k].a1;

            // --- compute initial volume from area
            project-> Link[i].newVolume = project-> Conduit[k].a1 * link_getLength(project,i) *
                                project-> Conduit[k].barrels;
            project-> Link[i].oldVolume = project-> Link[i].newVolume;
        }

        // --- update crown elev. of nodes at either end
        j = project-> Link[i].node1;
        z = project-> Node[j].invertElev + project-> Link[i].offset1 + project-> Link[i].xsect.yFull;
        project-> Node[j].crownElev = MAX(project-> Node[j].crownElev, z);
        j = project-> Link[i].node2;
        z = project-> Node[j].invertElev + project-> Link[i].offset2 + project-> Link[i].xsect.yFull;
        project-> Node[j].crownElev = MAX(project-> Node[j].crownElev, z);
    }
}

//=============================================================================

double getLinkInflow(Project* project, int j, double dt)
//
//  Input:   j  = link index
//           dt = routing time step (sec)
//  Output:  returns link inflow (cfs)
//  Purpose: finds flow into upstream end of link at current time step under
//           Steady or Kin. Wave routing.
//
{
    int   n1 = project-> Link[j].node1;
    double q;
    if ( project-> Link[j].type == CONDUIT ||
         project-> Link[j].type == PUMP ||
         project-> Node[n1].type == STORAGE ) q = link_getInflow(project,j);
    else q = 0.0;
    return node_getMaxOutflow(project,n1, q, dt);
}

//=============================================================================

void updateStorageState(Project* project, int i, int j, int links[], double dt)
//
//  Input:   i = index of storage node
//           j = current position in links array
//           links = array of topo-sorted link indexes
//           dt = routing time step (sec)
//  Output:  none
//  Purpose: updates depth and volume of a storage node using successive
//           approximation with under-relaxation for Steady or Kin. Wave
//           routing.
//
{
    int    iter;                       // iteration counter
    int    stopped;                    // TRUE when iterations stop
    double vFixed;                     // fixed terms of flow balance eqn.
    double v2;                         // new volume estimate (ft3)
    double d1;                         // initial value of storage depth (ft)
    double d2;                         // updated value of storage depth (ft)
    double outflow;                    // outflow rate from storage (cfs)

    // --- see if storage node needs updating
    if ( project-> Node[i].type != STORAGE ) return;
    if ( project-> Node[i].updated ) return;

    // --- compute terms of flow balance eqn.
    //       v2 = v1 + (inflow - outflow)*dt
    //     that do not depend on storage depth at end of time step
    vFixed = project-> Node[i].oldVolume + 
             0.5 * (project-> Node[i].oldNetInflow + project-> Node[i].inflow) * dt;
    d1 = project-> Node[i].newDepth;

    // --- iterate finding outflow (which depends on depth) and subsequent
    //     new volume and depth until negligible depth change occurs
    iter = 1;
    stopped = FALSE;
    while ( iter < MAXITER && !stopped )
    {
        // --- find total flow in all outflow links
        outflow = getStorageOutflow(project,i, j, links, dt);

        // --- find new volume from flow balance eqn.
        v2 = vFixed - 0.5 * outflow * dt - node_getLosses(project,i, dt);

        // --- limit volume to full volume if no ponding
        //     and compute overflow rate
        v2 = MAX(0.0, v2);
        project-> Node[i].overflow = 0.0;
        if ( v2 > project-> Node[i].fullVolume )
        {
            project-> Node[i].overflow = (v2 - MAX(project-> Node[i].oldVolume,
                                         project-> Node[i].fullVolume)) / dt;
            if ( !project-> AllowPonding || project-> Node[i].pondedArea == 0.0 )
                v2 = project-> Node[i].fullVolume;
        }

        // --- update node's volume & depth 
        project-> Node[i].newVolume = v2;
        if ( v2 > project-> Node[i].fullVolume ) d2 = node_getPondedDepth(project,i, v2);
        else d2 = node_getDepth(project,i, v2);
        project-> Node[i].newDepth = d2;

        // --- use under-relaxation to estimate new depth value
        //     and stop if close enough to previous value
        d2 = (1.0 - OMEGA)*d1 + OMEGA*d2;
        if ( fabs(d2 - d1) <= STOPTOL ) stopped = TRUE;

        // --- update old depth with new value and continue to iterate
        project-> Node[i].newDepth = d2;
        d1 = d2;
        iter++;
    }

    // --- mark node as being updated
    project-> Node[i].updated = TRUE;
}

//=============================================================================

double getStorageOutflow(Project* project, int i, int j, int links[], double dt)
//
//  Input:   i = index of storage node
//           j = current position in links array
//           links = array of topo-sorted link indexes
//           dt = routing time step (sec)
//  Output:  returns total outflow from storage node (cfs)
//  Purpose: computes total flow released from a storage node.
//
{
    int   k, m;
    double outflow = 0.0;

    for (k = j; k <project-> Nobjects[LINK]; k++)
    {
        m = links[k];
        if ( project-> Link[m].node1 != i ) break;
        outflow += getLinkInflow(project,m, dt);
    }
    return outflow;        
}

//=============================================================================

void setNewNodeState(Project* project, int j, double dt)
//
//  Input:   j  = node index
//           dt = time step (sec)
//  Output:  none
//  Purpose: updates state of node after current time step
//           for Steady Flow or Kinematic Wave flow routing.
//
{
    int   canPond;                     // TRUE if ponding can occur at node  
    double newNetInflow;               // inflow - outflow at node (cfs)

    // --- storage nodes have already been updated
    if ( project-> Node[j].type == STORAGE ) return; 

    // --- update stored volume using mid-point integration
    newNetInflow = project-> Node[j].inflow - project-> Node[j].outflow;
    project-> Node[j].newVolume = project-> Node[j].oldVolume +
                        0.5 * (project-> Node[j].oldNetInflow + newNetInflow) * dt;
    if ( project-> Node[j].newVolume < FUDGE ) project-> Node[j].newVolume = 0.0;

    // --- determine any overflow lost from system
    project-> Node[j].overflow = 0.0;
    canPond = (project-> AllowPonding && project-> Node[j].pondedArea > 0.0);
    if ( project-> Node[j].newVolume > project-> Node[j].fullVolume )
    {
        project-> Node[j].overflow = (project-> Node[j].newVolume - MAX(project-> Node[j].oldVolume,
                            project-> Node[j].fullVolume)) / dt;
        if ( project-> Node[j].overflow < FUDGE ) project-> Node[j].overflow = 0.0;
        if ( !canPond ) project-> Node[j].newVolume = project-> Node[j].fullVolume;
    }

    // --- compute a depth from volume
    //     (depths at upstream nodes are subsequently adjusted in
    //     setNewLinkState to reflect depths in connected conduit)
    project-> Node[j].newDepth = node_getDepth(project,j, project-> Node[j].newVolume);
}

//=============================================================================

void setNewLinkState(Project* project, int j)
//
//  Input:   j = link index
//  Output:  none
//  Purpose: updates state of link after current time step under
//           Steady Flow or Kinematic Wave flow routing
//
{
    int   k;
    double a, y1, y2;

    project-> Link[j].newDepth = 0.0;
    project-> Link[j].newVolume = 0.0;

    if ( project-> Link[j].type == CONDUIT )
    {
        // --- find avg. depth from entry/exit conditions
        k = project-> Link[j].subIndex;
        a = 0.5 * (project-> Conduit[k].a1 + project-> Conduit[k].a2);
        project-> Link[j].newVolume = a * link_getLength(project,j) * project-> Conduit[k].barrels;
        y1 = xsect_getYofA(project,&project-> Link[j].xsect, project-> Conduit[k].a1);
        y2 = xsect_getYofA(project,&project-> Link[j].xsect, project-> Conduit[k].a2);
        project-> Link[j].newDepth = 0.5 * (y1 + y2);

        // --- update depths at end nodes
        updateNodeDepth(project,project-> Link[j].node1, y1 + project-> Link[j].offset1);
        updateNodeDepth(project,project-> Link[j].node2, y2 + project-> Link[j].offset2);

        // --- check if capacity limited
        if ( project-> Conduit[k].a1 >= project-> Link[j].xsect.aFull )
             project-> Conduit[k].capacityLimited = TRUE;
        else project-> Conduit[k].capacityLimited = FALSE;
    }
}

//=============================================================================

void updateNodeDepth(Project* project, int i, double y)
//
//  Input:   i = node index
//           y = flow depth (ft)
//  Output:  none
//  Purpose: updates water depth at a node with a possibly higher value.
//
{
    // --- storage nodes were updated elsewhere
    if ( project-> Node[i].type == STORAGE ) return;

    // --- if non-outfall node is flooded, then use full depth
    if ( project-> Node[i].type != OUTFALL &&
         project-> Node[i].overflow > 0.0 ) y = project-> Node[i].fullDepth;

    // --- if current new depth below y
    if ( project-> Node[i].newDepth < y )
    {
        // --- update new depth
        project-> Node[i].newDepth = y;

        // --- depth cannot exceed full depth (if value exists)
        if ( project-> Node[i].fullDepth > 0.0 && y > project-> Node[i].fullDepth )
        {
            project-> Node[i].newDepth = project-> Node[i].fullDepth;
        }
    }
}

//=============================================================================

int steadyflow_execute(Project* project, int j, double* qin, double* qout, double tStep)
//
//  Input:   j = link index
//           qin = inflow to link (cfs)
//           tStep = time step (sec)
//  Output:  qin = adjusted inflow to link (limited by flow capacity) (cfs)
//           qout = link's outflow (cfs)
//           returns 1 if successful
//  Purpose: performs steady flow routing through a single link.
//
{
    int   k;
    double s;
    double q;

    // --- use Manning eqn. to compute flow area for conduits
    if ( project-> Link[j].type == CONDUIT )
    {
        k = project-> Link[j].subIndex;
        q = (*qin) / project-> Conduit[k].barrels;
        if ( project-> Link[j].xsect.type == DUMMY ) project-> Conduit[k].a1 = 0.0;
        else 
        {
            // --- subtract evap and infil losses from inflow
            q -= link_getLossRate(project,j, tStep);
            if ( q < 0.0 ) q = 0.0;

            // --- flow can't exceed full flow 
            if ( q > project-> Link[j].qFull )
            {
                q = project-> Link[j].qFull;
                project-> Conduit[k].a1 = project-> Link[j].xsect.aFull;
                (*qin) = q * project-> Conduit[k].barrels;
            }

            // --- infer flow area from flow rate 
            else
            {
                s = q / project-> Conduit[k].beta;
                project-> Conduit[k].a1 = xsect_getAofS(project,&project-> Link[j].xsect, s);
            }
        }
        project-> Conduit[k].a2 = project-> Conduit[k].a1;
        project-> Conduit[k].q1 = q;
        project-> Conduit[k].q2 = q;
        (*qout) = q * project-> Conduit[k].barrels;
    }
    else (*qout) = (*qin);
    return 1;
}

//=============================================================================
