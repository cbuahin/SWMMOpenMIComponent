//-----------------------------------------------------------------------------
//   qualrout.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/20/14   (Build 5.1.001)
//   Author:   L. Rossman
//
//   Water quality routing functions.
//
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "headers.h"

//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)
//-----------------------------------------------------------------------------
//  qualrout_init            (called by swmm_start)
//  qualrout_execute         (called by routing_execute)

//-----------------------------------------------------------------------------
//  Function declarations
//-----------------------------------------------------------------------------
static void  findLinkMassFlow(Project* project, int i, double tStep);
static void  findNodeQual(Project* project, int j);
static void  findLinkQual(Project* project, int i, double tStep);
static void  findSFLinkQual(Project* project, int i, double tStep);
static void  findStorageQual(Project* project, int j, double tStep);
static void  updateHRT(Project* project, int j, double v, double q, double tStep);
static double getReactedQual(Project* project, int p, double c, double v1, double tStep);
static double getMixedQual(double c, double v1, double wIn, double qIn,
              double tStep);


//=============================================================================

void    qualrout_init(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: initializes water quality concentrations in all nodes and links.
//
{
    int     i, p, isWet;
    double  c;

    for (i = 0; i < project->Nobjects[NODE]; i++)
    {
	    isWet = ( project->Node[i].newDepth > FUDGE );
        for (p = 0; p < project->Nobjects[POLLUT]; p++)
        {
            c = 0.0;
            if ( isWet ) c = project->Pollut[p].initConcen;
            project->Node[i].oldQual[p] = c;
            project->Node[i].newQual[p] = c;
        }
    }

    for (i = 0; i < project->Nobjects[LINK]; i++)
    {
        isWet = ( project->Link[i].newDepth > FUDGE );
        for (p = 0; p < project->Nobjects[POLLUT]; p++)
        {
            c = 0.0;
            if ( isWet ) c = project->Pollut[p].initConcen;
            project->Link[i].oldQual[p] = c;
            project->Link[i].newQual[p] = c;
        }
    }
}

//=============================================================================

void qualrout_execute(Project* project, double tStep)
//
//  Input:   tStep = routing time step (sec)
//  Output:  none
//  Purpose: routes water quality constituents through the drainage
//           network over the current time step.
//
{
    int    i, j;
    double qIn, vAvg;

    // --- find mass flow each link contributes to its downstream node
    for ( i = 0; i < project->Nobjects[LINK]; i++ ) findLinkMassFlow(project,i, tStep);

    // --- find new water quality concentration at each node  
    for (j = 0; j < project->Nobjects[NODE]; j++)
    {
        // --- get node inflow and average volume
        qIn = project->Node[j].inflow;
        vAvg = (project->Node[j].oldVolume + project->Node[j].newVolume) / 2.0;
        
        // --- save inflow concentrations if treatment applied
        if ( project->Node[j].treatment )
        {
            if ( qIn < ZERO ) qIn = 0.0;
            treatmnt_setInflow(project,qIn, project->Node[j].newQual);
        }
       
        // --- find new quality at the node 
        if ( project->Node[j].type == STORAGE || project->Node[j].oldVolume > FUDGE )
        {
            findStorageQual(project,j, tStep);
        }
        else findNodeQual(project,j);

        // --- apply treatment to new quality values
        if ( project->Node[j].treatment ) treatmnt_treat(project,j, qIn, vAvg, tStep);
    }

    // --- find new water quality in each link
    for ( i=0; i<project->Nobjects[LINK]; i++ ) findLinkQual(project,i, tStep);
}

//=============================================================================

double getMixedQual(double c, double v1, double wIn, double qIn, double tStep)
//
//  Input:   c = pollutant concentration at start of time step (mass/ft3)
//           v1 = volume at start of time step (ft3)
//           wIn = mass inflow rate (mass/sec)
//           qIn = flow inflow rate (cfs)
//           tStep = time step (sec)
//  Output:  returns pollutant concentration at end of time step (mass/ft3)
//  Purpose: finds pollutant concentration within a completely mixed volume
//
{
    double vIn, cIn, cMax;

    // --- compute concentration of any inflow
    if ( qIn <= ZERO ) return c;
    vIn = qIn * tStep;
    cIn = wIn * tStep / vIn;

    // --- find limit on final concentration
    cMax = MAX(c, cIn);

    // --- mix inflow with reacted contents
    c = (c*v1 + wIn*tStep) / (v1 + vIn);
    c = MIN(c, cMax);
    c = MAX(c, 0.0);
    return c;
}


//=============================================================================

void findLinkMassFlow(Project* project, int i, double tStep)
//
//  Input:   i = link index
//           tStep = time step (sec)
//  Output:  none
//  Purpose: adds constituent mass flow out of link to the total
//           accumulation at the link's downstream node.
//
//  Note:    project->Node[].newQual[], the accumulator variable, already contains
//           contributions from runoff and other external inflows from
//           calculations made in routing_execute().
{
    int    j, p;
    double qLink, w;

    // --- find inflow to downstream node
    qLink = project->Link[i].newFlow;

    // --- identify index of downstream node
    j = project->Link[i].node2;
    if ( qLink < 0.0 ) j = project->Link[i].node1;
    qLink = fabs(qLink);

    // --- add mass inflow from link to total at downstream node
    for (p = 0; p < project->Nobjects[POLLUT]; p++)
    {
	    w = qLink * project->Link[i].oldQual[p];
        project->Node[j].newQual[p] += w;
	    project->Link[i].totalLoad[p] += w * tStep;
    }
}

//=============================================================================

void findNodeQual(Project* project, int j)
//
//  Input:   j = node index
//  Output:  none
//  Purpose: finds new quality in a node with no storage volume.
//
{
    int    p;
    double qNode;

    // --- if there is flow into node then concen. = mass inflow/node flow
    qNode = project->Node[j].inflow;
    if ( qNode > ZERO )
    {
        for (p = 0; p < project->Nobjects[POLLUT]; p++)
        {
            project->Node[j].newQual[p] /= qNode;
        }
    }

    // --- otherwise concen. is 0
    else for (p = 0; p < project->Nobjects[POLLUT]; p++) project->Node[j].newQual[p] = 0.0;
}

//=============================================================================

void findLinkQual(Project* project, int i, double tStep)
//
//  Input:   i = link index
//           tStep = routing time step (sec)
//  Output:  none
//  Purpose: finds new quality in a link at end of the current time step.
//
{
    int    j,                // upstream node index
           k,                // conduit index
           p;                // pollutant index
    double wIn,              // pollutant mass inflow rate (mass/sec)
           qIn,              // inflow rate (cfs)
           qOut,             // outflow rate (cfs)
           v1,               // link volume at start of time step (ft3)
           v2,               // link volume at end of time step (ft3)
           c1,               // current concentration within link (mass/ft3)
           c2;               // new concentration within link (mass/ft3)

    // --- identify index of upstream node
    j = project->Link[i].node1;
    if ( project->Link[i].newFlow < 0.0 ) j = project->Link[i].node2;

    // --- link concentration equals that of upstream node when
    //     link is not a conduit or is a dummy link
    if ( project->Link[i].type != CONDUIT || project->Link[i].xsect.type == DUMMY )
    {
        for (p = 0; p < project->Nobjects[POLLUT]; p++)
        {
            project->Link[i].newQual[p] = project->Node[j].newQual[p];
        }
        return;
    }

    // --- concentrations are zero in an empty conduit
    if ( project->Link[i].newDepth <= FUDGE )
    {
        for (p = 0; p < project->Nobjects[POLLUT]; p++)
        {
            project->Link[i].newQual[p] = 0.0;
        }
        return;
    }

    // --- Steady Flow routing requires special treatment
    if ( project->RouteModel == SF )
    {
        findSFLinkQual(project,i, tStep);
        return;
    }

    // --- get inlet & outlet flow
    k = project->Link[i].subIndex;
    qIn  = fabs(project->Conduit[k].q1) * (double)project->Conduit[k].barrels;
    qOut = fabs(project->Conduit[k].q2) * (double)project->Conduit[k].barrels;

    // --- get starting and ending volumes
    v1 = project->Link[i].oldVolume;
    v2 = project->Link[i].newVolume;

    // --- adjust inflow to compensate for volume change when inflow = outflow
    if (qIn == qOut)                                        
    {
        qIn = qIn + (v2 - v1) / tStep; 
        qIn = MAX(qIn, 0.0);
    }

    // --- for each pollutant
    for (p = 0; p < project->Nobjects[POLLUT]; p++)
    {
        // --- determine mass lost to first order decay
        c1 = project->Link[i].oldQual[p];
        c2 = getReactedQual(project,p, c1, v1, tStep);

        // --- mix inflow to conduit with previous contents
        wIn = project->Node[j].newQual[p]*qIn;
        c2 = getMixedQual(c2, v1, wIn, qIn, tStep);

        // --- assign new concen. to link
        project->Link[i].newQual[p] = c2;
    }
}

//=============================================================================

void  findSFLinkQual(Project* project, int i, double tStep)
//
//  Input:   i = link index
//           tStep = routing time step (sec)
//  Output:  none
//  Purpose: finds new quality in a link at end of the current time step for
//           Steady Flow routing.
//
{
    int j = project->Link[i].node1;
    int p;
    double c1, c2;
    double lossRate;
    double t = tStep;

    // --- for each pollutant
    for (p = 0; p < project->Nobjects[POLLUT]; p++)
    {
        // --- conduit's quality equals upstream node quality
        c1 = project->Node[j].newQual[p];
        c2 = c1;

        // --- apply first-order decay over travel time
        if ( project->Pollut[p].kDecay > 0.0 )
        {
            c2 = c1 * exp(-project->Pollut[p].kDecay * t);
            c2 = MAX(0.0, c2);
            lossRate = (c1 - c2) * project->Link[i].newFlow;
            massbal_addReactedMass(project,p, lossRate);
        }
        project->Link[i].newQual[p] = c2;
    }
}

//=============================================================================

void  findStorageQual(Project* project, int j, double tStep)
//
//  Input:   j = node index
//           tStep = routing time step (sec)
//  Output:  none
//  Purpose: finds new quality in a node with storage volume.
//  
{
    int    p;                // pollutant index
    double qIn,              // inflow rate (cfs)
           wIn,              // pollutant mass inflow rate (mass)
           v1,               // volume at start of time step (ft3)
           c1,               // initial pollutant concentration (mass/ft3)
           c2;               // final pollutant concentration (mass/ft3)

    // --- get inflow rate & initial volume
    qIn = project->Node[j].inflow;
    v1 = project->Node[j].oldVolume;

    // --- update hydraulic residence time for storage nodes
    //     (HRT can be used in treatment functions)
    if ( project->Node[j].type == STORAGE )
    {    
        updateHRT(project,j, project->Node[j].oldVolume, qIn, tStep);
    }

    // --- for each pollutant
    for (p = 0; p < project->Nobjects[POLLUT]; p++)
    {
        // --- get current concentration 
        c1 = project->Node[j].oldQual[p];

        // --- apply first order decay only if no separate treatment function
        if ( project->Node[j].treatment == NULL ||
             project->Node[j].treatment[p].equation == NULL )
        {
            c1 = getReactedQual(project,p, c1, v1, tStep);
        }

        // --- mix inflow with current contents (mass inflow rate was
        //     temporarily saved in project->Node[j].newQual)
        wIn = project->Node[j].newQual[p];
        c2 = getMixedQual(c1, v1, wIn, qIn, tStep);

        // --- assign new concen. to node
        project->Node[j].newQual[p] = c2;
    }
}

//=============================================================================

void updateHRT(Project* project, int j, double v, double q, double tStep)
//
//  Input:   j = node index
//           v = storage volume (ft3)
//           q = inflow rate (cfs)
//           tStep = time step (sec)
//  Output:  none
//  Purpose: updates hydraulic residence time (i.e., water age) at a 
//           storage node.
//
{
    int    k = project->Node[j].subIndex;
    double hrt = project->Storage[k].hrt;
    if ( v < ZERO ) hrt = 0.0;
    else hrt = (hrt + tStep) * v / (v + q*tStep);
    project->Storage[k].hrt = MAX(hrt, 0.0);
}

//=============================================================================

double getReactedQual(Project* project, int p, double c, double v1, double tStep)
//
//  Input:   p = pollutant index
//           c = initial concentration (mass/ft3)
//           v1 = initial volume (ft3)
//           tStep = time step (sec)
//  Output:  none
//  Purpose: applies a first order reaction to a pollutant over a given
//           time step.
//
{
    double c2, lossRate;
    double kDecay = project->Pollut[p].kDecay;

    if ( kDecay == 0.0 ) return c;
    c2 = c * (1.0 - kDecay * tStep);
    c2 = MAX(0.0, c2);
    lossRate = (c - c2) * v1 / tStep;
    massbal_addReactedMass(project,p, lossRate);
    return c2;
}

//=============================================================================
