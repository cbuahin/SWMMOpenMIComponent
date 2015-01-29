//-----------------------------------------------------------------------------
//   link.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/20/14   (Build 5.1.001)
//   Author:   L. Rossman (EPA)
//             M. Tryby (EPA)
//
//   Conveyance system link functions
//
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE


#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "headers.h"

//-----------------------------------------------------------------------------
//  Constants
//-----------------------------------------------------------------------------
static const double MIN_DELTA_Z = 0.001; // minimum elevation change for conduit
                                         // slopes (ft)

//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)
//-----------------------------------------------------------------------------
//  link_readParams        (called by parseLine in input.c)
//  link_readXsectParams   (called by parseLine in input.c)
//  link_readLossParams    (called by parseLine in input.c)
//  link_validate          (called by project_validate in project.c)
//  link_initState         (called by initObjects in swmm5.c)
//  link_setOldHydState    (called by routing_execute in routing.c)
//  link_setOldQualState   (called by routing_execute in routing.c)            
//  link_setTargetSetting  (called by routing_execute in routing.c)
//  link_setSetting        (called by routing_execute in routing.c)
//  link_getResults        (called by output_saveLinkResults)
//  link_getLength
//  link_getFroude
//  link_getInflow
//  link_setOutfallDepth
//  link_getYcrit
//  link_getYnorm
//  link_getVelocity
//  link_getPower          (called by stats_updateLinkStats in stats.c) 
//  link_getLossRate

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------
static void   link_setParams(int j, int type, int n1, int n2, int k, double x[]);
static void   link_convertOffsets(int j);
static double link_getOffsetHeight(int j, double offset, double elev);

static int    conduit_readParams(int j, int k, char* tok[], int ntoks);
static void   conduit_validate(int j, int k);
static void   conduit_initState(int j, int k);
static void   conduit_reverse(int j, int k);
static double conduit_getLength(int j);
static double conduit_getLengthFactor(int j, int k, double roughness);
static double conduit_getSlope(int j);
static double conduit_getInflow(int j);
static void   conduit_updateStats(int j, double dt, DateTime aDate);
static double conduit_getLossRate(int j, double tstep);

static int    pump_readParams(int j, int k, char* tok[], int ntoks);
static void   pump_validate(int j, int k);
static void   pump_initState(int j, int k);
static double pump_getInflow(int j);

static int    orifice_readParams(int j, int k, char* tok[], int ntoks);
static void   orifice_validate(int j, int k);
static void   orifice_setSetting(int j, double tstep);
static double orifice_getWeirCoeff(int j, int k, double h);
static double orifice_getInflow(int j);
static double orifice_getFlow(int j, int k, double head, double f,
              int hasFlapGate);

static int    weir_readParams(int j, int k, char* tok[], int ntoks);
static void   weir_validate(int j, int k);
static double weir_getInflow(int j);
static double weir_getOpenArea(int j, double y);
static void   weir_getFlow(int j, int k, double head, double dir,
              int hasFlapGate, double* q1, double* q2);
static double weir_getdqdh(int k, double dir, double h, double q1, double q2);

static int    outlet_readParams(int j, int k, char* tok[], int ntoks);
static double outlet_getFlow(int k, double head);
static double outlet_getInflow(int j);


//=============================================================================

int link_readParams(int j, int type, int k, char* tok[], int ntoks)
//
//  Input:   j     = link index
//           type  = link type code
//           k     = link type index
//           tok[] = array of string tokens
//           ntoks = number of tokens   
//  Output:  returns an error code
//  Purpose: reads parameters for a specific type of link from a 
//           tokenized line of input data.
//
{
    switch ( type )
    {
      case CONDUIT: return conduit_readParams(j, k, tok, ntoks);
      case PUMP:    return pump_readParams(j, k, tok, ntoks);
      case ORIFICE: return orifice_readParams(j, k, tok, ntoks);
      case WEIR:    return weir_readParams(j, k, tok, ntoks);
      case OUTLET:  return outlet_readParams(j, k, tok, ntoks);
      default: return 0;
    }
}

//=============================================================================

int link_readXsectParams(char* tok[], int ntoks)
//
//  Input:   tok[] = array of string tokens
//           ntoks = number of tokens   
//  Output:  returns an error code
//  Purpose: reads a link's cross section parameters from a tokenized
//           line of input data.
//
{
    int    i, j, k;
    double x[4];

    // --- get index of link
    if ( ntoks < 6 ) return error_setInpError(ERR_ITEMS, "");
    j = project_findObject(LINK, tok[0]);
    if ( j < 0 ) return error_setInpError(ERR_NAME, tok[0]);

    // --- get code of xsection shape
    k = findmatch(tok[1], XsectTypeWords);
    if ( k < 0 ) return error_setInpError(ERR_KEYWORD, tok[1]);

    // --- assign default number of barrels to conduit
    if ( project->Link[j].type == CONDUIT ) project->Conduit[Link[j].subIndex].barrels = 1;
    
    // --- assume link is not a culvert 
    project->Link[j].xsect.culvertCode = 0;

    // --- for irregular shape, find index of transect object
    if ( k == IRREGULAR )
    {
        i = project_findObject(TRANSECT, tok[2]);
        if ( i < 0 ) return error_setInpError(ERR_NAME, tok[2]);
        project->Link[j].xsect.type = k;
        project->Link[j].xsect.transect = i;
    }
    else
    {
        // --- parse max. depth & shape curve for a custom shape 
        if ( k == CUSTOM )
        {
            if ( !getDouble(tok[2], &x[0]) || x[0] <= 0.0 )
               return error_setInpError(ERR_NUMBER, tok[2]);
            i = project_findObject(CURVE, tok[3]);
            if ( i < 0 ) return error_setInpError(ERR_NAME, tok[3]);
            project->Link[j].xsect.type = k;
            project->Link[j].xsect.transect = i;
            project->Link[j].xsect.yFull = x[0] / UCF(LENGTH);
        }

        // --- parse and save geometric parameters
        else for (i = 2; i <= 5; i++)
        {
            if ( !getDouble(tok[i], &x[i-2]) )
                return error_setInpError(ERR_NUMBER, tok[i]);
        }
        if ( !xsect_setParams(&project->Link[j].xsect, k, x, UCF(LENGTH)) )
        {
            return error_setInpError(ERR_NUMBER, "");
        }

        // --- parse number of barrels if present
        if ( project->Link[j].type == CONDUIT && ntoks >= 7 )
        {
            i = atoi(tok[6]);
            if ( i <= 0 ) return error_setInpError(ERR_NUMBER, tok[6]);
            else project->Conduit[project->Link[j].subIndex].barrels = (char)i;
        }

        // --- parse culvert code if present
        if ( project->Link[j].type == CONDUIT && ntoks >= 8 )
        {
            i = atoi(tok[7]);
            if ( i < 0 ) return error_setInpError(ERR_NUMBER, tok[7]);
            else project->Link[j].xsect.culvertCode = i;
        }

    }
    return 0;
}

//=============================================================================

int link_readLossParams(char* tok[], int ntoks)
//
//  Input:   tok[] = array of string tokens
//           ntoks = number of tokens   
//  Output:  returns an error code
//  Purpose: reads local loss parameters for a link from a tokenized
//           line of input data.
//
{
    int    i, j, k;
    double x[3];
    double seepRate = 0.0;

    if ( ntoks < 4 ) return error_setInpError(ERR_ITEMS, "");
    j = project_findObject(LINK, tok[0]);
    if ( j < 0 ) return error_setInpError(ERR_NAME, tok[0]);
    for (i=1; i<=3; i++)
    {
        if ( ! getDouble(tok[i], &x[i-1]) || x[i-1] < 0.0 )
        return error_setInpError(ERR_NUMBER, tok[i]);
    }
    k = 0;
    if ( ntoks >= 5 )
    {
        k = findmatch(tok[4], NoYesWords);             
        if ( k < 0 ) return error_setInpError(ERR_KEYWORD, tok[4]);
    }
    if ( ntoks >= 6 )
    {
        if ( ! getDouble(tok[5], &seepRate) )
        return error_setInpError(ERR_NUMBER, tok[5]);
    }
    project->Link[j].cLossInlet   = x[0];
    project->Link[j].cLossOutlet  = x[1];
    project->Link[j].cLossAvg     = x[2];
    project->Link[j].hasFlapGate  = k;
    project->Link[j].seepRate     = seepRate / UCF(RAINFALL);
    return 0;
}

//=============================================================================

void  link_setParams(int j, int type, int n1, int n2, int k, double x[])
//
//  Input:   j   = link index
//           type = link type code
//           n1   = index of upstream node
//           n2   = index of downstream node
//           k    = index of link's sub-type
//           x    = array of parameter values
//  Output:  none
//  Purpose: sets parameters for a link.
//
{
    project->Link[j].node1       = n1;
    project->Link[j].node2       = n2;
    project->Link[j].type        = type;
    project->Link[j].subIndex    = k;
    project->Link[j].offset1     = 0.0;
    project->Link[j].offset2     = 0.0;
    project->Link[j].q0          = 0.0;
    project->Link[j].qFull       = 0.0;
    project->Link[j].setting     = 1.0;
    project->Link[j].targetSetting = 1.0;
    project->Link[j].hasFlapGate = 0;
    project->Link[j].qLimit      = 0.0;         // 0 means that no limit is defined
    project->Link[j].direction   = 1;

    switch (type)
    {
      case CONDUIT:
        project->Conduit[k].length    = x[0] / UCF(LENGTH);
        project->Conduit[k].modLength = Conduit[k].length;
        project->Conduit[k].roughness = x[1];
        project->Link[j].offset1      = x[2] / UCF(LENGTH);
        project->Link[j].offset2      = x[3] / UCF(LENGTH);
        project->Link[j].q0           = x[4] / UCF(FLOW);
        project->Link[j].qLimit       = x[5] / UCF(FLOW);
        break;

      case PUMP:
        project->Pump[k].pumpCurve    = (int)x[0];
        project->Link[j].hasFlapGate  = FALSE;
        project->Pump[k].initSetting  = x[1];
        project->Pump[k].yOn          = x[2] / UCF(LENGTH);
        project->Pump[k].yOff         = x[3] / UCF(LENGTH);
        project->Pump[k].xMin         = 0.0;
        project->Pump[k].xMax         = 0.0;
        break;

      case ORIFICE:
        project->Orifice[k].type      = (int)x[0];
        project->Link[j].offset1      = x[1] / UCF(LENGTH);
        project->Link[j].offset2      = Link[j].offset1;
        project->Orifice[k].cDisch    = x[2];
        project->Link[j].hasFlapGate  = (x[3] > 0.0) ? 1 : 0;
        project->Orifice[k].orate     = x[4] * 3600.0;
        break;

      case WEIR:
        project->Weir[k].type         = (int)x[0];
        project->Link[j].offset1      = x[1] / UCF(LENGTH);
        project->Link[j].offset2      = Link[j].offset1;
        project->Weir[k].cDisch1      = x[2];
        project->Link[j].hasFlapGate  = (x[3] > 0.0) ? 1 : 0;
        project->Weir[k].endCon       = x[4];
        project->Weir[k].cDisch2      = x[5];
        break;

      case OUTLET:
        project->Link[j].offset1      = x[0] / UCF(LENGTH);
        project->Link[j].offset2      = Link[j].offset1;
        project->Outlet[k].qCoeff     = x[1];
        project->Outlet[k].qExpon     = x[2];
        project->Outlet[k].qCurve     = (int)x[3];
        project->Link[j].hasFlapGate  = (x[4] > 0.0) ? 1 : 0;
        project->Outlet[k].curveType  = (int)x[5];

        xsect_setParams(&project->Link[j].xsect, DUMMY, NULL, 0.0);
        break;

    }
}

//=============================================================================

void  link_validate(int j)
//
//  Input:   j = link index
//  Output:  none
//  Purpose: validates a link's properties.
//
{
    int   n;

    if ( project->LinkOffsets == ELEV_OFFSET ) link_convertOffsets(j);
    switch ( project->Link[j].type )
    {
      case CONDUIT: conduit_validate(j, project->Link[j].subIndex); break;
      case PUMP:    pump_validate(j, project->Link[j].subIndex);    break;
      case ORIFICE: orifice_validate(j, project->Link[j].subIndex); break;
      case WEIR:    weir_validate(j, project->Link[j].subIndex);    break;
    }

    // --- check if crest of regulator opening < invert of downstream node
    switch ( project->Link[j].type )
    {
      case ORIFICE: 
      case WEIR:
      case OUTLET:
          if ( project->Node[project->Link[j].node1].invertElev + Link[j].offset1 <
               project->Node[project->Link[j].node2].invertElev )
               report_writeWarningMsg(WARN10, project->Link[j].ID);
    }

    // --- force max. depth of end nodes to be >= link crown height
    //     at non-storage nodes

    // --- skip pumps and bottom orifices
    if ( project->Link[j].type == PUMP ||
         (project->Link[j].type == ORIFICE && 
          project->Orifice[project->Link[j].subIndex].type == BOTTOM_ORIFICE) ) return;

    // --- extend upstream node's full depth to link's crown elevation
    n = project->Link[j].node1;
    if ( project->Node[n].type != STORAGE )
    {
        project->Node[n].fullDepth = MAX(Node[n].fullDepth,
                            project->Link[j].offset1 + Link[j].xsect.yFull);
    }

    // --- do same for downstream node only for conduit links
    n = project->Link[j].node2;
    if ( project->Node[n].type != STORAGE && project->Link[j].type == CONDUIT )
    {
        project->Node[n].fullDepth = MAX(Node[n].fullDepth,
                            project->Link[j].offset2 + Link[j].xsect.yFull);
    }
}

//=============================================================================

void link_convertOffsets(int j)
//
//  Input:   j = link index
//  Output:  none
//  Purpose: converts offset elevations to offset heights for a link.
//
{
    double elev;
    
    elev = project->Node[project->Link[j].node1].invertElev;
    project->Link[j].offset1 = link_getOffsetHeight(j, Link[j].offset1, elev);
    if ( project->Link[j].type == CONDUIT )
    {
        elev = project->Node[project->Link[j].node2].invertElev;
        project->Link[j].offset2 = link_getOffsetHeight(j, Link[j].offset2, elev);
    }
    else project->Link[j].offset2 = Link[j].offset1;
}

//=============================================================================

double link_getOffsetHeight(int j, double offset, double elev)
//
//  Input:   j = link index
//           offset = link elevation offset (ft)
//           elev   = node invert elevation (ft)
//  Output:  returns offset distance above node invert (ft)
//  Purpose: finds offset height for one end of a link.
//
{
    if ( offset <= MISSING || project->Link[j].type == PUMP) return 0.0;
    offset -= elev;
    if ( offset >= 0.0 ) return offset;
    if ( offset >= -MIN_DELTA_Z ) return 0.0;
    report_writeWarningMsg(WARN03, project->Link[j].ID);
    return 0.0;
}

//=============================================================================

void link_initState(int j)
//
//  Input:   j = link index
//  Output:  none
//  Purpose: initializes a link's state variables at start of simulation.
//
{
    int   p;

    // --- initialize hydraulic state
    project->Link[j].oldFlow   = Link[j].q0;
    project->Link[j].newFlow   = Link[j].q0;
    project->Link[j].oldDepth  = 0.0;
    project->Link[j].newDepth  = 0.0;
    project->Link[j].oldVolume = 0.0;
    project->Link[j].newVolume = 0.0;
    project->Link[j].setting   = 1.0;
    project->Link[j].targetSetting = 1.0;
    project->Link[j].inletControl  = FALSE;
    project->Link[j].normalFlow    = FALSE;
    if ( project->Link[j].type == CONDUIT ) conduit_initState(j, Link[j].subIndex);
    if ( project->Link[j].type == PUMP    ) pump_initState(j, Link[j].subIndex);
    
    // --- initialize water quality state
    for (p = 0; p < project->Nobjects[POLLUT]; p++)
    {
        project->Link[j].oldQual[p] = 0.0;
        project->Link[j].newQual[p] = 0.0;
		project->Link[j].totalLoad[p] = 0.0;
    }
}

//=============================================================================

double  link_getInflow(int j)
//
//  Input:   j = link index
//  Output:  returns link flow rate (cfs)
//  Purpose: finds total flow entering a link during current time step.
//
{
    if ( project->Link[j].setting == 0 ) return 0.0;
    switch ( project->Link[j].type )
    {
      case CONDUIT: return conduit_getInflow(j);
      case PUMP:    return pump_getInflow(j);
      case ORIFICE: return orifice_getInflow(j);
      case WEIR:    return weir_getInflow(j);
      case OUTLET:  return outlet_getInflow(j);
      default:      return node_getOutflow(project->Link[j].node1, j);
    }
}

//=============================================================================

void link_setOldHydState(int j)
//
//  Input:   j = link index
//  Output:  none
//  Purpose: replaces link's old hydraulic state values with current ones.
//
{
    int k;
    
	project->Link[j].oldDepth  = Link[j].newDepth;
    project->Link[j].oldFlow   = Link[j].newFlow;
    project->Link[j].oldVolume = Link[j].newVolume;

    if ( project->Link[j].type == CONDUIT )
    {
        k = project->Link[j].subIndex;
        project->Conduit[k].q1Old = Conduit[k].q1;
        project->Conduit[k].q2Old = Conduit[k].q2;
    }
}

//=============================================================================

void link_setOldQualState(int j)
//
//  Input:   j = link index
//  Output:  none
//  Purpose: replaces link's old water quality state values with current ones.
//
{
    int p;
    for (p = 0; p < project->Nobjects[POLLUT]; p++)
    {
        project->Link[j].oldQual[p] = Link[j].newQual[p];
        project->Link[j].newQual[p] = 0.0;
    }
}

//=============================================================================

void link_setTargetSetting(int j)
// 
//  Input:   j = link index
//  Output:  none
//  Purpose: updates a link's target setting.
//
{
    int k, n1;
    if ( project->Link[j].type == PUMP )
    {
        k = project->Link[j].subIndex;
        n1 = project->Link[j].node1;
        project->Link[j].targetSetting = Link[j].setting;
        if ( project->Pump[k].yOff > 0.0 &&
             project->Link[j].setting > 0.0 &&
             project->Node[n1].newDepth < project->Pump[k].yOff ) project->Link[j].targetSetting = 0.0;
        if ( project->Pump[k].yOn > 0.0 &&
             project->Link[j].setting == 0.0 &&
             project->Node[n1].newDepth > project->Pump[k].yOn )  project->Link[j].targetSetting = 1.0;
    }
}

//=============================================================================

void link_setSetting(int j, double tstep)
// 
//  Input:   j = link index
//           tstep = time step over which setting is adjusted 
//  Output:  none
//  Purpose: updates a link's setting as a result of a control action.
//
{
    if ( project->Link[j].type == ORIFICE ) orifice_setSetting(j, tstep);
    else project->Link[j].setting = Link[j].targetSetting; 
}

//=============================================================================

int link_setFlapGate(int j, int n1, int n2, double q)
//
//  Input:   j = link index
//           n1 = index of node on upstream end of link
//           n2 = index of node on downstream end of link 
//           q = signed flow value (value and units don't matter) 
//  Output:  returns TRUE if there is reverse flow through a flap gate
//           associated with the link. 
//  Purpose: based on the sign of the flow, determines if a flap gate
//           associated with the link should close or not.
//
{
    int    n = -1;

    // --- check for reverse flow through link's flap gate
    if ( project->Link[j].hasFlapGate )
    {
        if ( q * (double)project->Link[j].direction < 0.0 ) return TRUE;
    }

    // --- check for project->Outfall with flap gate node on inflow end of link
    if ( q < 0.0 ) n = n2;
    if ( q > 0.0 ) n = n1;
    if ( n >= 0 &&
         project->Node[n].type == OUTFALL &&
         project->Outfall[project->Node[n].subIndex].hasFlapGate ) return TRUE;
    return FALSE;
}

//=============================================================================

void link_getResults(int j, double f, float x[])
//
//  Input:   j = link index
//           f = time weighting factor
//  Output:  x = array of weighted results
//  Purpose: retrieves time-weighted average of old and new results for a link.
//
{
    int    p;                     // pollutant index
    double y,                     // depth
           q,                     // flow
           u,                     // velocity
           v,                     // volume
           c;                     // capacity, setting or concentration
    double f1 = 1.0 - f;

    y = f1*project->Link[j].oldDepth + f*Link[j].newDepth;
    q = f1*project->Link[j].oldFlow + f*Link[j].newFlow;
    v = f1*project->Link[j].oldVolume + f*Link[j].newVolume;
    u = link_getVelocity(j, q, y);
    c = 0.0;
    if (project->Link[j].type == CONDUIT)
    {
        if (project->Link[j].xsect.type != DUMMY)
            c = xsect_getAofY(&project->Link[j].xsect, y) / Link[j].xsect.aFull;
    }
    else c = project->Link[j].setting;

    // --- override time weighting for pump flow between on/off states
    if (project->Link[j].type == PUMP && Link[j].oldFlow*Link[j].newFlow == 0.0)
    {
        if ( f >= f1 ) q = project->Link[j].newFlow;
        else           q = project->Link[j].oldFlow;
    }

    y *= UCF(LENGTH);
    v *= UCF(VOLUME);
    q *= UCF(FLOW) * (double)project->Link[j].direction;
    u *= UCF(LENGTH) * (double)project->Link[j].direction;
    x[LINK_DEPTH]    = (float)y;
    x[LINK_FLOW]     = (float)q;
    x[LINK_VELOCITY] = (float)u;
    x[LINK_VOLUME]   = (float)v;
    x[LINK_CAPACITY] = (float)c;

    if ( !project->IgnoreQuality ) for (p = 0; p < project->Nobjects[POLLUT]; p++)
    {
        c = f1*project->Link[j].oldQual[p] + f*Link[j].newQual[p];
        x[LINK_QUAL+p] = (float)c;
    }
}

//=============================================================================

void link_setOutfallDepth(int j)
//
//  Input:   j = link index
//  Output:  none
//  Purpose: sets depth at outfall node connected to link j.
//
{
    int     k;                         // conduit index
    int     n;                         // outfall node index
    double  z;                         // invert offset height (ft)
    double  q;                         // flow rate (cfs)
    double  yCrit = 0.0;               // critical flow depth (ft)
    double  yNorm = 0.0;               // normal flow depth (ft)

    // --- find which end node of link is an outfall
    if ( project->Node[project->Link[j].node2].type == OUTFALL )
    {
        n = project->Link[j].node2;
        z = project->Link[j].offset2;
    }
    else if ( project->Node[project->Link[j].node1].type == OUTFALL )
    {
        n = project->Link[j].node1;
        z = project->Link[j].offset1;
    }
    else return;
    
    // --- find both normal & critical depth for current flow
    if ( project->Link[j].type == CONDUIT )
    {
        k = project->Link[j].subIndex;
        q = fabs(project->Link[j].newFlow / project->Conduit[k].barrels);
        yNorm = link_getYnorm(j, q);
        yCrit = link_getYcrit(j, q);
    }

    // --- set new depth at node
    node_setOutletDepth(n, yNorm, yCrit, z);
}

//=============================================================================

double link_getYcrit(int j, double q)
//
//  Input:   j = link index
//           q = link flow rate (cfs)
//  Output:  returns critical depth (ft)
//  Purpose: computes critical depth for given flow rate.
//
{
    return xsect_getYcrit(&project->Link[j].xsect, q);
}

//=============================================================================

double  link_getYnorm(int j, double q)
//
//  Input:   j = link index
//           q = link flow rate (cfs)
//  Output:  returns normal depth (ft)
//  Purpose: computes normal depth for given flow rate.
//
{
    int    k;
    double s, a, y;

    if ( project->Link[j].type != CONDUIT ) return 0.0;
    if ( project->Link[j].xsect.type == DUMMY ) return 0.0;
    q = fabs(q);
    k = project->Link[j].subIndex;
    if ( q > project->Conduit[k].qMax ) q = Conduit[k].qMax;
    if ( q <= 0.0 ) return 0.0;
    s = q / project->Conduit[k].beta;
    a = xsect_getAofS(&project->Link[j].xsect, s);
    y = xsect_getYofA(&project->Link[j].xsect, a);
    return y;
}

//=============================================================================

double link_getLength(int j)
//
//  Input:   j = link index
//  Output:  returns length (ft)
//  Purpose: finds true length of a link.
//
{
    if ( project->Link[j].type == CONDUIT ) return conduit_getLength(j);
    return 0.0;
}

//=============================================================================

double link_getVelocity(int j, double flow, double depth)
//
//  Input:   j     = link index
//           flow  = link flow rate (cfs)
//           depth = link flow depth (ft)
//  Output:  returns flow velocity (fps)
//  Purpose: finds flow velocity given flow and depth.
//
{
    double area;
    double veloc = 0.0;
    int    k;

    if ( depth <= 0.01 ) return 0.0;
    if ( project->Link[j].type == CONDUIT )
    {
        k = project->Link[j].subIndex;
        flow /= project->Conduit[k].barrels;
        area = xsect_getAofY(&project->Link[j].xsect, depth);
        if (area > FUDGE ) veloc = flow / area;
    }
    return veloc;
}

//=============================================================================

double link_getFroude(int j, double v, double y)
//
//  Input:   j = link index
//           v = flow velocity (fps)
//           y = flow depth (ft)
//  Output:  returns Froude Number
//  Purpose: computes Froude Number for given velocity and flow depth
//
{
    TXsect*  xsect = &project->Link[j].xsect;

    // --- return 0 if link is not a conduit
    if ( project->Link[j].type != CONDUIT ) return 0.0;
    
    // --- return 0 if link empty or closed conduit is full
    if ( y <= FUDGE ) return 0.0;
    if ( !xsect_isOpen(xsect->type) &&
         xsect->yFull - y <= FUDGE ) return 0.0;

    // --- compute hydraulic depth
    y = xsect_getAofY(xsect, y) / xsect_getWofY(xsect, y);

    // --- compute Froude No.
    return fabs(v) / sqrt(GRAVITY * y);
}

//=============================================================================

double link_getPower(int j)
//
//  Input:   j = link index
//  Output:  returns power consumed by link in kwatts
//  Purpose: computes power consumed by head loss (or head gain) of
//           water flowing through a link
//
{
    int    n1 = project->Link[j].node1;
    int    n2 = project->Link[j].node2;
    double dh = (project->Node[n1].invertElev + Node[n1].newDepth) -
                (project->Node[n2].invertElev + Node[n2].newDepth);
    double q =  fabs(project->Link[j].newFlow);
    return fabs(dh) * q / 8.814 * KWperHP;
}

//=============================================================================

double link_getLossRate(int j, double tStep)
//
//  Input:   j = link index
//           tstep = time step (sec)
//  Output:  returns uniform loss rate in link (ft3/sec)
//  Purpose: computes volume lost in link due to evaporation and infiltration
//           over a time step.
//
{
    if ( project->Link[j].type == CONDUIT ) return conduit_getLossRate(j, tStep);
    else return 0.0;
}


//=============================================================================
//                    C O N D U I T   M E T H O D S
//=============================================================================

int  conduit_readParams(int j, int k, char* tok[], int ntoks)
//
//  Input:   j = link index
//           k = conduit index
//           tok[] = array of string tokens
//           ntoks = number of tokens
//  Output:  returns an error code
//  Purpose: reads conduit parameters from a tokenzed line of input.
//
{
    int    n1, n2;
    double x[6];
    char*  id;

    // --- check for valid ID and end node IDs
    if ( ntoks < 7 ) return error_setInpError(ERR_ITEMS, "");
    id = project_findID(LINK, tok[0]);                // link ID
    if ( id == NULL ) return error_setInpError(ERR_NAME, tok[0]);
    n1 = project_findObject(NODE, tok[1]);            // upstrm. node
    if ( n1 < 0 ) return error_setInpError(ERR_NAME, tok[1]);
    n2 = project_findObject(NODE, tok[2]);            // dwnstrm. node
    if ( n2 < 0 ) return error_setInpError(ERR_NAME, tok[2]);

    // --- parse length & Mannings N
    if ( !getDouble(tok[3], &x[0]) )
        return error_setInpError(ERR_NUMBER, tok[3]);
    if ( !getDouble(tok[4], &x[1]) )
        return error_setInpError(ERR_NUMBER, tok[4]);

    // --- parse offsets
    if ( project->LinkOffsets == ELEV_OFFSET && *tok[5] == '*' ) x[2] = MISSING;
    else if ( !getDouble(tok[5], &x[2]) )
        return error_setInpError(ERR_NUMBER, tok[5]);
    if ( project->LinkOffsets == ELEV_OFFSET && *tok[6] == '*' ) x[3] = MISSING;
    else if ( !getDouble(tok[6], &x[3]) )
        return error_setInpError(ERR_NUMBER, tok[6]);

   // --- parse optional parameters
    x[4] = 0.0;                                       // init. flow
    if ( ntoks >= 8 )
    {
        if ( !getDouble(tok[7], &x[4]) )
        return error_setInpError(ERR_NUMBER, tok[7]);
    }
    x[5] = 0.0;
    if ( ntoks >= 9 )
    {
        if ( !getDouble(tok[8], &x[5]) )
        return error_setInpError(ERR_NUMBER, tok[8]);
    }

    // --- add parameters to data base
    project->Link[j].ID = id;
    link_setParams(j, CONDUIT, n1, n2, k, x);
    return 0;
}

//=============================================================================

void  conduit_validate(int j, int k)
//
//  Input:   j = link index
//           k = conduit index
//  Output:  none
//  Purpose: validates a conduit's properties.
//
{
    double aa;
    double lengthFactor, roughness, slope;

    // --- a storage node cannot have a dummy outflow link
    if ( project->Link[j].xsect.type == DUMMY )
    {
        if ( project->Node[project->Link[j].node1].type == STORAGE )
        {
            report_writeErrorMsg(ERR_DUMMY_LINK, project->Node[project->Link[j].node1].ID);
            return;
        }
    }

    // --- if custom xsection, then set its parameters
    if ( project->Link[j].xsect.type == CUSTOM )
        xsect_setCustomXsectParams(&project->Link[j].xsect);

    // --- if irreg. xsection, assign transect roughness to conduit
    if ( project->Link[j].xsect.type == IRREGULAR )
    {
        xsect_setIrregXsectParams(&project->Link[j].xsect);
        project->Conduit[k].roughness = project->Transect[project->Link[j].xsect.transect].roughness;
    }

    // --- if force main xsection, adjust units on D-W roughness height
    if ( project->Link[j].xsect.type == FORCE_MAIN )
    {
        if ( project->ForceMainEqn == D_W ) project->Link[j].xsect.rBot /= UCF(RAINDEPTH);
        if ( project->Link[j].xsect.rBot <= 0.0 )
            report_writeErrorMsg(ERR_XSECT, project->Link[j].ID);
    }

    // --- check for valid length & roughness
    if ( project->Conduit[k].length <= 0.0 )
        report_writeErrorMsg(ERR_LENGTH, project->Link[j].ID);
    if ( project->Conduit[k].roughness <= 0.0 )
        report_writeErrorMsg(ERR_ROUGHNESS, project->Link[j].ID);
    if ( project->Conduit[k].barrels <= 0 )
        report_writeErrorMsg(ERR_BARRELS, project->Link[j].ID);

    // --- check for valid xsection
    if ( project->Link[j].xsect.type != DUMMY )
    {
        if ( project->Link[j].xsect.type < 0 )
            report_writeErrorMsg(ERR_NO_XSECT, project->Link[j].ID);
        else if ( project->Link[j].xsect.aFull <= 0.0 )
            report_writeErrorMsg(ERR_XSECT, project->Link[j].ID);
    }
    if ( project->ErrorCode ) return;

    // --- check for negative offsets
    if ( project->Link[j].offset1 < 0.0 )
    {
        report_writeWarningMsg(WARN03, project->Link[j].ID);
        project->Link[j].offset1 = 0.0;
    }
	if ( project->Link[j].offset2 < 0.0 )
    {
        report_writeWarningMsg(WARN03, project->Link[j].ID);
        project->Link[j].offset2 = 0.0;
    }

    // --- adjust conduit offsets for partly filled circular xsection
    if ( project->Link[j].xsect.type == FILLED_CIRCULAR )
    {
        project->Link[j].offset1 += Link[j].xsect.yBot;
        project->Link[j].offset2 += Link[j].xsect.yBot;
    }

    // --- compute conduit slope 
    slope = conduit_getSlope(j);
    project->Conduit[k].slope = slope;

    // --- reverse orientation of conduit if using dynamic wave routing 
    //     and slope is negative
    if ( project->RouteModel == DW &&
         slope < 0.0 &&
         project->Link[j].xsect.type != DUMMY )
    {
        conduit_reverse(j, k);
    }

    // --- get equivalent Manning roughness for Force Mains
    //     for use when pipe is partly full
    roughness = project->Conduit[k].roughness;
    if ( project->RouteModel == DW && project->Link[j].xsect.type == FORCE_MAIN )
    {
        roughness = forcemain_getEquivN(j, k);
    }

    // --- adjust roughness for meandering natural channels
    if ( project->Link[j].xsect.type == IRREGULAR )
    {
        lengthFactor = project->Transect[project->Link[j].xsect.transect].lengthFactor;
        roughness *= sqrt(lengthFactor);
    }

    // --- lengthen conduit if lengthening option is in effect
    lengthFactor = 1.0;
    if ( project->RouteModel == DW &&
         project->LengtheningStep > 0.0 &&
         project->Link[j].xsect.type != DUMMY )
    {
        lengthFactor = conduit_getLengthFactor(j, k, roughness);
    }
        
    if ( lengthFactor != 1.0 )
    {
        project->Conduit[k].modLength = lengthFactor * conduit_getLength(j);
        slope /= lengthFactor;
        roughness = roughness / sqrt(lengthFactor);
    }

    // --- compute roughness factor used when computing friction
    //     slope term in Dynamic Wave flow routing

    // --- special case for non-Manning Force Mains
    //     (roughness factor for full flow is saved in xsect.sBot)  
    if ( project->RouteModel == DW && project->Link[j].xsect.type == FORCE_MAIN )
    {
        project->Link[j].xsect.sBot =
            forcemain_getRoughFactor(j, lengthFactor);
    }
    project->Conduit[k].roughFactor = GRAVITY * SQR(roughness/PHI);

    // --- compute full flow through cross section
    if ( project->Link[j].xsect.type == DUMMY ) project->Conduit[k].beta = 0.0;
    else project->Conduit[k].beta = PHI * sqrt(fabs(slope)) / roughness;
    project->Link[j].qFull = Link[j].xsect.sFull * project->Conduit[k].beta;
    project->Conduit[k].qMax = project->Link[j].xsect.sMax * Conduit[k].beta;

    // --- see if flow is supercritical most of time
    //     by comparing normal & critical velocities.
    //     (factor of 0.3 is for circular pipe 95% full)
    // NOTE: this factor was used in the past for a modified version of
    //       Kinematic Wave routing but is now deprecated.
    aa = project->Conduit[k].beta / sqrt(32.2) *
         pow(project->Link[j].xsect.yFull, 0.1666667) * 0.3;
    if ( aa >= 1.0 ) project->Conduit[k].superCritical = TRUE;
    else             project->Conduit[k].superCritical = FALSE;

    // --- set value of hasLosses flag
    if ( project->Link[j].cLossInlet  == 0.0 &&
         project->Link[j].cLossOutlet == 0.0 &&
         project->Link[j].cLossAvg    == 0.0
       ) project->Conduit[k].hasLosses = FALSE;
    else project->Conduit[k].hasLosses = TRUE;
}

//=============================================================================

void conduit_reverse(int j, int k)
//
//  Input:   j = link index
//           k = conduit index
//  Output:  none
//  Purpose: reverses direction of a conduit
//
{
    int    i;
    double z;
    double cLoss;

    // --- reverse end nodes
    i = project->Link[j].node1;
    project->Link[j].node1 = Link[j].node2;
    project->Link[j].node2 = i;

    // --- reverse node offsets
    z = project->Link[j].offset1;
    project->Link[j].offset1 = Link[j].offset2;
    project->Link[j].offset2 = z;

    // --- reverse loss coeffs.
    cLoss = project->Link[j].cLossInlet;
    project->Link[j].cLossInlet = Link[j].cLossOutlet;
    project->Link[j].cLossOutlet = cLoss;

    // --- reverse direction & slope
    project->Conduit[k].slope = -Conduit[k].slope;
    project->Link[j].direction *= (signed char)-1;

    // --- reverse initial flow value
    project->Link[j].q0 = -Link[j].q0;
}

//=============================================================================

double conduit_getLength(int j)
//
//  Input:   j = link index
//  Output:  returns conduit's length (ft)
//  Purpose: finds true length of a conduit.
//
//  Note: for irregular natural channels, user inputs length of main
//        channel (for FEMA purposes) but program should use length
//        associated with entire flood plain. project->Transect.lengthFactor
//        is the ratio of these two lengths.
//
{
    int k = project->Link[j].subIndex;
    int t;
    if ( project->Link[j].xsect.type != IRREGULAR ) return project->Conduit[k].length;
    t = project->Link[j].xsect.transect;
    if ( t < 0 || t >= project->Nobjects[TRANSECT] ) return project->Conduit[k].length;
    return project->Conduit[k].length / project->Transect[t].lengthFactor;
}

//=============================================================================

double conduit_getLengthFactor(int j, int k, double roughness)
//
//  Input:   j = link index
//           k = conduit index
//           roughness = conduit Manning's n
//  Output:  returns factor by which a conduit should be lengthened
//  Purpose: computes amount of conduit lengthing to improve numerical stability.
//
//  The following form of the Courant criterion is used:
//      L = t * v * (1 + Fr) / Fr
//  where L = conduit length, t = time step, v = velocity, & Fr = Froude No.
//  After substituting Fr = v / sqrt(gy), where y = flow depth, we get:
//    L = t * ( sqrt(gy) + v )
//
{
    double ratio;
    double yFull;
    double vFull;
    double tStep;

    // --- evaluate flow depth and velocity at full normal flow condition
    yFull = project->Link[j].xsect.yFull;
    if ( xsect_isOpen(project->Link[j].xsect.type) )
    {
        yFull = project->Link[j].xsect.aFull / xsect_getWofY(&Link[j].xsect, yFull);
    }
    vFull = PHI / roughness * project->Link[j].xsect.sFull *
            sqrt(fabs(project->Conduit[k].slope)) / project->Link[j].xsect.aFull;

    // --- determine ratio of Courant length to actual length
    if ( project->LengtheningStep == 0.0 ) tStep = project->RouteStep;
    else                          tStep = MIN(project->RouteStep, project->LengtheningStep);
    ratio = (sqrt(GRAVITY*yFull) + vFull) * tStep / conduit_getLength(j);

    // --- return max. of 1.0 and ratio
    if ( ratio > 1.0 ) return ratio;
    else return 1.0;
}

//=============================================================================

double conduit_getSlope(int j) 
//
//  Input:   j = link index
//  Output:  returns conduit slope
//  Purpose: computes conduit slope.
//
{
    double elev1, elev2, delta, slope;
    double length = conduit_getLength(j);

    // --- check that elevation drop > minimum allowable drop
    elev1 = project->Link[j].offset1 + project->Node[Link[j].node1].invertElev;
    elev2 = project->Link[j].offset2 + project->Node[Link[j].node2].invertElev;
    delta = fabs(elev1 - elev2);
    if ( delta < MIN_DELTA_Z )
    {
        report_writeWarningMsg(WARN04, project->Link[j].ID);
        delta = MIN_DELTA_Z;
    }

    // --- elevation drop cannot exceed conduit length
    if ( delta >= length )
    {
        report_writeWarningMsg(WARN08, project->Link[j].ID);
        slope = delta / length;
    }

    // --- slope = elev. drop / horizontal distance
    else slope = delta / sqrt(SQR(length) - SQR(delta));

    // -- check that slope exceeds minimum allowable slope
    if ( project->MinSlope > 0.0 && slope < MinSlope )
    {
        report_writeWarningMsg(WARN05, project->Link[j].ID);
        slope = project->MinSlope;
        // keep min. slope positive for SF or KW routing 
        if (project->RouteModel == SF || RouteModel == KW) return slope;
    }

    // --- change sign for adverse slope
    if ( elev1 < elev2 ) slope = -slope;
    return slope;
}

//=============================================================================

void  conduit_initState(int j, int k)
//
//  Input:   j = link index
//           k = conduit index
//  Output:  none
//  Purpose: sets initial conduit depth to normal depth of initial flow
//
{
    project->Link[j].newDepth = link_getYnorm(j, Link[j].q0 / project->Conduit[k].barrels);
    project->Link[j].oldDepth = Link[j].newDepth;
}

//=============================================================================

double conduit_getInflow(int j)
//
//  Input:   j = link index
//  Output:  returns flow in link (cfs)
//  Purpose: finds inflow to conduit from upstream node.
//
{
    double qIn = node_getOutflow(project->Link[j].node1, j);
    if ( project->Link[j].qLimit > 0.0 ) qIn = MIN(qIn, Link[j].qLimit);
    return qIn;
}

//=============================================================================

double conduit_getLossRate(int j, double tStep)
//
//  Input:   j = link index
//           tStep = time step (sec)
//  Output:  returns rate of evaporation & infiltration losses (ft3/sec)
//  Purpose: computes rate of water evaporation & infiltration from a conduit.
//
{
	TXsect *xsect;
	double depth = 0.5 * (project->Link[j].oldDepth + Link[j].newDepth);
    double length;
    double topWidth;
    double wettedPerimeter;
    double maxLossRate;
	double evapLossRate = 0.0,
           seepLossRate = 0.0, 
           totalLossRate = 0.0;

    if ( depth > FUDGE )
    {
	    xsect = &project->Link[j].xsect;
        length = conduit_getLength(j);

        // --- find evaporation rate for open conduits
        if ( xsect_isOpen(xsect->type) && project->Evap.rate > 0.0 )
        {
            topWidth = xsect_getWofY(xsect, depth);
            evapLossRate = topWidth * length * project->Evap.rate;
        }

        // --- compute seepage loss rate
        if ( project->Link[j].seepRate > 0.0 )
        {
            // limit depth to depth at max width
            if ( depth >= xsect->ywMax ) depth = xsect->ywMax;

            // get wetted perimeter
            wettedPerimeter = 0.0;
            if ( depth > 0.0 )
            {
                wettedPerimeter = xsect_getAofY(xsect, depth) / 
                                  xsect_getRofY(xsect, depth);
            }

            // compute seepage loss rate across length of conduit
            seepLossRate = project->Link[j].seepRate * wettedPerimeter * length;
        }

        // --- compute total loss rate
        totalLossRate = evapLossRate + seepLossRate;

        // --- total loss rate cannot exceed volume stored in conduit
        if ( totalLossRate > 0.0 )
        {
            maxLossRate = 0.5 * (project->Link[j].oldVolume + Link[j].newVolume) / tStep;
            if ( totalLossRate > maxLossRate )
            {
                evapLossRate = evapLossRate * maxLossRate / totalLossRate;
                seepLossRate = seepLossRate * maxLossRate / totalLossRate;
                totalLossRate = maxLossRate;
            }
        }
    }
    project->Conduit[project->Link[j].subIndex].evapLossRate = evapLossRate;
    project->Conduit[project->Link[j].subIndex].seepLossRate = seepLossRate;
    return totalLossRate;
}


//=============================================================================
//                        P U M P   M E T H O D S
//=============================================================================

int  pump_readParams(int j, int k, char* tok[], int ntoks)
//
//  Input:   j = link index
//           k = pump index
//           tok[] = array of string tokens
//           ntoks = number of tokens
//  Output:  returns an error code
//  Purpose: reads pump parameters from a tokenized line of input.
//
{
    int    m;
    int    n1, n2;
    double x[4]; 
    char*  id;

    // --- check for valid ID and end node IDs
    if ( ntoks < 3 ) return error_setInpError(ERR_ITEMS, ""); 
    id = project_findID(LINK, tok[0]);
    if ( id == NULL ) return error_setInpError(ERR_NAME, tok[0]);
    n1 = project_findObject(NODE, tok[1]);
    if ( n1 < 0 ) return error_setInpError(ERR_NAME, tok[1]);
    n2 = project_findObject(NODE, tok[2]);
    if ( n2 < 0 ) return error_setInpError(ERR_NAME, tok[2]);

    // --- parse curve name
    x[0] = -1.; 
    if ( ntoks >= 4 )
    {
        if ( !strcomp(tok[3],"*") )
        {
            m = project_findObject(CURVE, tok[3]);
            if ( m < 0 ) return error_setInpError(ERR_NAME, tok[3]);
            x[0] = m;
        }
    }

    // --- parse init. status if present
    x[1] = 1.0;
    if ( ntoks >= 5 )
    {
        m = findmatch(tok[4], OffOnWords);
        if ( m < 0 ) return error_setInpError(ERR_KEYWORD, tok[4]);
        x[1] = m;
    }

    // --- parse startup/shutoff depths if present 
    x[2] = 0.0; 
    if ( ntoks >= 6 )
    {
        if ( !getDouble(tok[5], &x[2]) || x[2] < 0.0)
        return error_setInpError(ERR_NUMBER, tok[5]);
    }
    x[3] = 0.0;
    if ( ntoks >= 7 )
    {
        if ( !getDouble(tok[6], &x[3]) || x[3] < 0.0 )
        return error_setInpError(ERR_NUMBER, tok[6]);
    }

    // --- add parameters to pump object
    project->Link[j].ID = id;
    link_setParams(j, PUMP, n1, n2, k, x);
    return 0;
}

//=============================================================================

void  pump_validate(int j, int k)
//
//  Input:   j = link index
//           k = pump index
//  Output:  none
//  Purpose: validates a pump's properties
//
{
    int    m, n1;
    double x, y;

    project->Link[j].xsect.yFull = 0.0;

    // --- check for valid curve type
    m = project->Pump[k].pumpCurve;
    if ( m < 0 )
    {
        project->Pump[k].type = IDEAL_PUMP;
    }
    else 
    {
        if ( project->Curve[m].curveType < PUMP1_CURVE ||
             project->Curve[m].curveType > PUMP4_CURVE )
            report_writeErrorMsg(ERR_NO_CURVE, project->Link[j].ID);

        // --- store pump curve type with pump's parameters
        else 
        {
            project->Pump[k].type = project->Curve[m].curveType - PUMP1_CURVE;
            if ( table_getFirstEntry(&project->Curve[m], &x, &y) )
            {
                project->Link[j].qFull = y;
                project->Pump[k].xMin = x;
                project->Pump[k].xMax = x;
                while ( table_getNextEntry(&project->Curve[m], &x, &y) )
                {
                    project->Link[j].qFull = MAX(y, Link[j].qFull);
                    project->Pump[k].xMax = x;
                }
            }
            project->Link[j].qFull /= UCF(FLOW);
       }
    }

    // --- check that shutoff depth < startup depth
    if ( project->Pump[k].yOn > 0.0 && Pump[k].yOn <= Pump[k].yOff )
        report_writeErrorMsg(ERR_PUMP_LIMITS, project->Link[j].ID);

    // --- assign wet well volume to inlet node of Type 1 pump 
    if ( project->Pump[k].type == TYPE1_PUMP )
    {
        n1 = project->Link[j].node1;
        if ( project->Node[n1].type != STORAGE )
            project->Node[n1].fullVolume = MAX(Node[n1].fullVolume,
                                      project->Pump[k].xMax / UCF(VOLUME));
    }

}

//=============================================================================

void  pump_initState(int j, int k)
//
//  Input:   j = link index
//           k = pump index
//  Output:  none
//  Purpose: initializes pump conditions at start of a simulation
//
{
    project->Link[j].setting = project->Pump[k].initSetting;
    project->Link[j].targetSetting = project->Pump[k].initSetting;
}

//=============================================================================

double pump_getInflow(int j)
//
//  Input:   j = link index
//  Output:  returns pump flow (cfs)
//  Purpose: finds flow produced by a pump.
//
{
    int     k, m;
    int     n1, n2;
    double  vol, depth, head;
    double  qIn, qIn1, dh = 0.001;

    k = project->Link[j].subIndex;
    m = project->Pump[k].pumpCurve;
    n1 = project->Link[j].node1;
    n2 = project->Link[j].node2;

    // --- no flow if setting is closed 
    project->Link[j].flowClass = NO;
    project->Link[j].setting = Link[j].targetSetting;
    if ( project->Link[j].setting == 0.0 ) return 0.0;

    // --- pump flow = node inflow for IDEAL_PUMP
    if ( project->Pump[k].type == IDEAL_PUMP )
        qIn = project->Node[n1].inflow + Node[n1].overflow;

    // --- pumping rate depends on pump curve type
    else switch(project->Curve[m].curveType)
    {
      case PUMP1_CURVE:
        vol = project->Node[n1].newVolume * UCF(VOLUME);
        qIn = table_intervalLookup(&project->Curve[m], vol) / UCF(FLOW);

        // --- check if off of pump curve
        if ( vol < project->Pump[k].xMin || vol > Pump[k].xMax )    
            project->Link[j].flowClass = YES;
        break;

      case PUMP2_CURVE:
        depth = project->Node[n1].newDepth * UCF(LENGTH);
        qIn = table_intervalLookup(&project->Curve[m], depth) / UCF(FLOW);

        // --- check if off of pump curve
        if ( depth < project->Pump[k].xMin || depth > Pump[k].xMax )  
            project->Link[j].flowClass = YES;
        break;

      case PUMP3_CURVE:
        head = ( (project->Node[n2].newDepth + Node[n2].invertElev) -
                 (project->Node[n1].newDepth + Node[n1].invertElev) );

		head = MAX(head, 0.0);

        qIn = table_lookup(&project->Curve[m], head*UCF(LENGTH)) / UCF(FLOW);

        // --- compute dQ/dh (slope of pump curve) and
        //     reverse sign since flow decreases with increasing head
    	project->Link[j].dqdh = -table_getSlope(&project->Curve[m], head*UCF(LENGTH)) * 
                       UCF(LENGTH) / UCF(FLOW);

        // --- check if off of pump curve
        head *= UCF(LENGTH);
        if ( head < project->Pump[k].xMin || head > Pump[k].xMax )   
            project->Link[j].flowClass = YES;
        break;

      case PUMP4_CURVE:
        depth = project->Node[n1].newDepth;
        qIn = table_lookup(&project->Curve[m], depth*UCF(LENGTH)) / UCF(FLOW);

        // --- compute dQ/dh (slope of pump curve)
        qIn1 = table_lookup(&project->Curve[m], (depth+dh)*UCF(LENGTH)) / UCF(FLOW);
        project->Link[j].dqdh = (qIn1 - qIn) / dh;

        // --- check if off of pump curve
        depth *= UCF(LENGTH);
        if ( depth < project->Pump[k].xMin ) project->Link[j].flowClass = DN_DRY;
        if ( depth > project->Pump[k].xMax ) project->Link[j].flowClass = UP_DRY;
        break;

      default: qIn = 0.0;
    }

    // --- do not allow reverse flow through pump
    if ( qIn < 0.0 )  qIn = 0.0;
    return qIn * project->Link[j].setting; 
}


//=============================================================================
//                    O R I F I C E   M E T H O D S
//=============================================================================

int  orifice_readParams(int j, int k, char* tok[], int ntoks)
//
//  Input:   j = link index
//           k = orifice index
//           tok[] = array of string tokens
//           ntoks = number of tokens
//  Output:  returns an error code
//  Purpose: reads orifice parameters from a tokenized line of input.
//
{
    int    m;
    int    n1, n2;
    double x[5];
    char*  id;

    // --- check for valid ID and end node IDs
    if ( ntoks < 6 ) return error_setInpError(ERR_ITEMS, "");
    id = project_findID(LINK, tok[0]);
    if ( id == NULL ) return error_setInpError(ERR_NAME, tok[0]);
    n1 = project_findObject(NODE, tok[1]);
    if ( n1 < 0 ) return error_setInpError(ERR_NAME, tok[1]);
    n2 = project_findObject(NODE, tok[2]);
    if ( n2 < 0 ) return error_setInpError(ERR_NAME, tok[2]);

    // --- parse orifice parameters
    m = findmatch(tok[3], OrificeTypeWords);
    if ( m < 0 ) return error_setInpError(ERR_KEYWORD, tok[3]);
    x[0] = m;                                              // type
    if ( project->LinkOffsets == ELEV_OFFSET && *tok[4] == '*' ) x[1] = MISSING;
    else if ( ! getDouble(tok[4], &x[1]) )                 // crest height
        return error_setInpError(ERR_NUMBER, tok[4]);
    if ( ! getDouble(tok[5], &x[2]) || x[2] < 0.0 )        // cDisch
        return error_setInpError(ERR_NUMBER, tok[5]);
    x[3] = 0.0;
    if ( ntoks >= 7 )
    {
        m = findmatch(tok[6], NoYesWords);               
        if ( m < 0 ) return error_setInpError(ERR_KEYWORD, tok[6]);
        x[3] = m;                                          // flap gate
    }
    x[4] = 0.0;
    if ( ntoks >= 8 )
    {
        if ( ! getDouble(tok[7], &x[4]) || x[4] < 0.0 )    // orate
            return error_setInpError(ERR_NUMBER, tok[7]);
    }

    // --- add parameters to orifice object
    project->Link[j].ID = id;
    link_setParams(j, ORIFICE, n1, n2, k, x);
    return 0;
}

//=============================================================================

void  orifice_validate(int j, int k)
//
//  Input:   j = link index
//           k = orifice index
//  Output:  none
//  Purpose: validates an orifice's properties
//
{
    int    err = 0;

    // --- check for valid xsection
    if ( project->Link[j].xsect.type != RECT_CLOSED
    &&   project->Link[j].xsect.type != CIRCULAR ) err = ERR_REGULATOR_SHAPE;
    if ( err > 0 )
    {
        report_writeErrorMsg(err, project->Link[j].ID);
        return;
    }

    // --- check for negative offset
    if ( project->Link[j].offset1 < 0.0 ) Link[j].offset1 = 0.0;

    // --- compute partial flow adjustment
    orifice_setSetting(j, 0.0);

    // --- compute an equivalent length
    project->Orifice[k].length = 2.0 * project->RouteStep * sqrt(GRAVITY * project->Link[j].xsect.yFull);
    project->Orifice[k].length = MAX(200.0, Orifice[k].length);
    project->Orifice[k].surfArea = 0.0;
}

//=============================================================================

void  orifice_setSetting(int j, double tstep)
//
//  Input:   j = link index
//           tstep = time step over which setting is adjusted (sec)
//  Output:  none
//  Purpose: updates an orifice's setting as a result of a control action.
//
{
    int    k = project->Link[j].subIndex;
    double delta, step;
    double h, f;

    // --- case where adjustment rate is instantaneous
    if ( project->Orifice[k].orate == 0.0 || tstep == 0.0)
        project->Link[j].setting = Link[j].targetSetting;

    // --- case where orifice setting depends on time step
    else
    {
        delta = project->Link[j].targetSetting - Link[j].setting;
        step = tstep / project->Orifice[k].orate;
        if ( step + 0.001 >= fabs(delta) )
            project->Link[j].setting = Link[j].targetSetting;
        else project->Link[j].setting += SGN(delta) * step;
    }

    // --- find effective orifice discharge coeff.
    h = project->Link[j].setting * Link[j].xsect.yFull;
    f = xsect_getAofY(&project->Link[j].xsect, h) * sqrt(2.0 * GRAVITY);
    project->Orifice[k].cOrif = Orifice[k].cDisch * f;

    // --- find equiv. discharge coeff. for when weir flow occurs
    project->Orifice[k].cWeir = orifice_getWeirCoeff(j, k, h) * f;
}

//=============================================================================

double orifice_getWeirCoeff(int j, int k, double h)
//
//  Input:   j = link index
//           k = orifice index
//           h = height of orifice opening (ft)
//  Output:  returns a discharge coefficient (ft^1/2)
//  Purpose: computes the discharge coefficient for an orifice
//           at the critical depth where weir flow begins.
//
{
    double w, aOverL;

    // --- this is for bottom orifices
    if ( project->Orifice[k].type == BOTTOM_ORIFICE )
    {
        // --- find critical height above opening where orifice flow
        //     turns into weir flow. It equals (Co/Cw)*(Area/Length)
        //     where Co is the orifice coeff., Cw is the weir coeff/sqrt(2g),
        //     Area is the area of the opening, and Length = circumference
        //     of the opening. For a basic sharp crested weir, Cw = 0.414.
        if (project->Link[j].xsect.type == CIRCULAR) aOverL = h / 4.0;
        else
        {
            w = project->Link[j].xsect.wMax;
            aOverL = (h*w) / (2.0*(h+w));
        }
        h = project->Orifice[k].cDisch / 0.414 * aOverL;
        project->Orifice[k].hCrit = h;
    }

    // --- this is for side orifices
    else
    {
        // --- critical height is simply height of opening
        project->Orifice[k].hCrit = h;

        // --- head on orifice is distance to center line
        h = h / 2.0;
    }

    // --- return a coefficient for the critical depth
    return project->Orifice[k].cDisch * sqrt(h);
}

//=============================================================================

double orifice_getInflow(int j)
//
//  Input:   j = link index
//  Output:  returns orifice flow rate (cfs)
//  Purpose: finds the flow through an orifice.
//
{
    int    k, n1, n2;
    double head, h1, h2, y1, dir;
    double f;
    double hcrest = 0.0;
    double hcrown = 0.0;
    double hmidpt;
    double q, ratio;

    // --- get indexes of end nodes and link's orifice
    n1 = project->Link[j].node1;
    n2 = project->Link[j].node2;
    k  = project->Link[j].subIndex;

    // --- find heads at upstream & downstream nodes
    if ( project->RouteModel == DW )
    {
        h1 = project->Node[n1].newDepth + Node[n1].invertElev;
        h2 = project->Node[n2].newDepth + Node[n2].invertElev;
    }
    else
    {
        h1 = project->Node[n1].newDepth + Node[n1].invertElev;
        h2 = project->Node[n1].invertElev;
    }
    dir = (h1 >= h2) ? +1.0 : -1.0; 
           
    // --- exchange h1 and h2 for reverse flow
    y1 = project->Node[n1].newDepth;
    if ( dir < 0.0 )
    {
        head = h1;
        h1 = h2;
        h2 = head;
        y1 = project->Node[n2].newDepth;
    }

    // --- orifice is a bottom orifice (oriented in horizontal plane)
    if ( project->Orifice[k].type == BOTTOM_ORIFICE )
    {
        // --- compute crest elevation
        hcrest = project->Node[n1].invertElev + project->Link[j].offset1;

        // --- compute head on orifice
        if (h1 < hcrest) head = 0.0;
        else if (h2 > hcrest) head = h1 - h2;
        else head = h1 - hcrest;

        // --- find fraction of critical height for which weir flow occurs
        f = head / project->Orifice[k].hCrit;
        f = MIN(f, 1.0);
    }

    // --- otherwise orifice is a side orifice (oriented in vertical plane)
    else
    {
        // --- compute elevations of orifice crest and crown
        hcrest = project->Node[n1].invertElev + project->Link[j].offset1;
        hcrown = hcrest + project->Link[j].xsect.yFull * Link[j].setting;
        hmidpt = (hcrest + hcrown) / 2.0;
    
        // --- compute degree of inlet submergence
        if ( h1 < hcrown && hcrown > hcrest )
            f = (h1 - hcrest) / (hcrown - hcrest);
        else f = 1.0;

        // --- compute head on orifice
        if ( f < 1.0 )          head = h1 - hcrest;
        else if ( h2 < hmidpt ) head = h1 - hmidpt;
        else                    head = h1 - h2;
    }

    // --- return if head is negligible or flap gate closed
    if ( head <= FUDGE || y1 <= FUDGE || 
         link_setFlapGate(j, n1, n2, dir) )
    {
        project->Link[j].newDepth = 0.0;
        project->Link[j].flowClass = DRY;
        project->Orifice[k].surfArea = FUDGE * Orifice[k].length;
        project->Link[j].dqdh = 0.0;
        return 0.0;
    }

    // --- determine flow class
    project->Link[j].flowClass = SUBCRITICAL;
    if ( hcrest > h2 )
    {
        if ( dir == 1.0 ) project->Link[j].flowClass = DN_CRITICAL;
        else              project->Link[j].flowClass = UP_CRITICAL;
    }

    // --- compute flow depth and surface area
    y1 = project->Link[j].xsect.yFull * Link[j].setting;
    if ( project->Orifice[k].type == SIDE_ORIFICE )
    {
        project->Link[j].newDepth = y1 * f;
        project->Orifice[k].surfArea =
            xsect_getWofY(&project->Link[j].xsect, Link[j].newDepth) *
            project->Orifice[k].length;
    }
    else
    {
        project->Link[j].newDepth = y1;
        project->Orifice[k].surfArea = xsect_getAofY(&project->Link[j].xsect, y1);
    }

    // --- find flow through the orifice
    q = dir * orifice_getFlow(j, k, head, f, project->Link[j].hasFlapGate);

    // --- apply Villemonte eqn. to correct for submergence
    if ( f < 1.0 && h2 > hcrest )
    {
        ratio = (h2 - hcrest) / (h1 - hcrest);
        q *= pow( (1.0 - pow(ratio, 1.5)), 0.385);
    }
    return q;
}

//=============================================================================

double orifice_getFlow(int j, int k,  double head, double f, int hasFlapGate)
//
//  Input:   j = link index
//           k = orifice index
//           head = head across orifice
//           f = fraction of critical depth filled
//           hasFlapGate = flap gate indicator
//  Output:  returns flow through an orifice
//  Purpose: computes flow through an orifice as a function of head.
//
{
    double area, q;
    double veloc, hLoss;

    // --- case where orifice is closed
    if ( head == 0.0 || f <= 0.0  )
    {
        project->Link[j].dqdh = 0.0;
        return 0.0;
    }

    // --- case where inlet depth is below critical depth;
    //     orifice behaves as a weir
    else if ( f < 1.0 )
    {
        q = project->Orifice[k].cWeir * pow(f, 1.5);
        project->Link[j].dqdh = 1.5 * q / (f * project->Orifice[k].hCrit);
    }

    // --- case where normal orifice flow applies
    else
    {
        q = project->Orifice[k].cOrif * sqrt(head);
        project->Link[j].dqdh = q / (2.0 * head);
    }

    // --- apply ARMCO adjustment for headloss from flap gate
    if ( hasFlapGate )
    {
        // --- compute velocity for current orifice flow
        area = xsect_getAofY(&project->Link[j].xsect,
                             project->Link[j].setting * Link[j].xsect.yFull);
        veloc = q / area;

        // --- compute head loss from gate
        hLoss = (4.0 / GRAVITY) * veloc * veloc *
                 exp(-1.15 * veloc / sqrt(head) );

        // --- update head (for orifice flow) 
        //     or critical depth fraction (for weir flow)
        if ( f < 1.0 )
        {
            f = f - hLoss/project->Orifice[k].hCrit;
            if ( f < 0.0 ) f = 0.0;
        }
        else
        {
            head = head - hLoss;
            if ( head < 0.0 ) head = 0.0;
        }

        // --- make recursive call to this function, with hasFlapGate
        //     set to false, to find flow values at adjusted head value
        q = orifice_getFlow(j, k, head, f, FALSE);
    }
    return q;
}

//=============================================================================
//                           W E I R   M E T H O D S
//=============================================================================

int   weir_readParams(int j, int k, char* tok[], int ntoks)
//
//  Input:   j = link index
//           k = weir index
//           tok[] = array of string tokens
//           ntoks = number of tokens
//  Output:  returns an error code
//  Purpose: reads weir parameters from a tokenized line of input.
//
{
    int    m;
    int    n1, n2;
    double x[6];
    char*  id;

    // --- check for valid ID and end node IDs
    if ( ntoks < 6 ) return error_setInpError(ERR_ITEMS, "");
    id = project_findID(LINK, tok[0]);
    if ( id == NULL ) return error_setInpError(ERR_NAME, tok[0]);
    n1 = project_findObject(NODE, tok[1]);
    if ( n1 < 0 ) return error_setInpError(ERR_NAME, tok[1]);
    n2 = project_findObject(NODE, tok[2]);
    if ( n2 < 0 ) return error_setInpError(ERR_NAME, tok[2]);

    // --- parse weir parameters
    m = findmatch(tok[3], WeirTypeWords);
    if ( m < 0 ) return error_setInpError(ERR_KEYWORD, tok[3]);
    x[0] = m;                                              // type
    if ( project->LinkOffsets == ELEV_OFFSET && *tok[4] == '*' ) x[1] = MISSING;
    else if ( ! getDouble(tok[4], &x[1]) )                 // height
        return error_setInpError(ERR_NUMBER, tok[4]);
    if ( ! getDouble(tok[5], &x[2]) || x[2] < 0.0 )        // cDisch1
        return error_setInpError(ERR_NUMBER, tok[5]);
    x[3] = 0.0;
    x[4] = 0.0;
    x[5] = 0.0;
    if ( ntoks >= 7 )
    {
        m = findmatch(tok[6], NoYesWords);             
        if ( m < 0 ) return error_setInpError(ERR_KEYWORD, tok[6]);
        x[3] = m;                                          // flap gate
    }
    if ( ntoks >= 8 )
    {
        if ( ! getDouble(tok[7], &x[4]) || x[4] < 0.0 )     // endCon
            return error_setInpError(ERR_NUMBER, tok[7]);
    }
    if ( ntoks >= 9 )
    {
        if ( ! getDouble(tok[8], &x[5]) || x[5] < 0.0 )     // cDisch2
            return error_setInpError(ERR_NUMBER, tok[8]);
    }

    // --- add parameters to weir object
    project->Link[j].ID = id;
    link_setParams(j, WEIR, n1, n2, k, x);
    return 0;
}

//=============================================================================

void  weir_validate(int j, int k)
//
//  Input:   j = link index
//           k = weir index
//  Output:  none
//  Purpose: validates a weir's properties
//
{
    int    err = 0;
 
    // --- check for valid cross section
    switch ( project->Weir[k].type)
    {
      case TRANSVERSE_WEIR:
      case SIDEFLOW_WEIR:
        if ( project->Link[j].xsect.type != RECT_OPEN ) err = ERR_REGULATOR_SHAPE;
        project->Weir[k].slope = 0.0;
        break;
        
      case VNOTCH_WEIR:
        if ( project->Link[j].xsect.type != TRIANGULAR ) err = ERR_REGULATOR_SHAPE;
        else
        {
            project->Weir[k].slope = project->Link[j].xsect.sBot;
        }
        break;

      case TRAPEZOIDAL_WEIR:
        if ( project->Link[j].xsect.type != TRAPEZOIDAL ) err = ERR_REGULATOR_SHAPE;
        else
        {
            project->Weir[k].slope = project->Link[j].xsect.sBot;
        }
        break;
    }
    if ( err > 0 )
    {
        report_writeErrorMsg(err, project->Link[j].ID);
        return;
    }

    // --- check for negative offset 
    if ( project->Link[j].offset1 < 0.0 ) Link[j].offset1 = 0.0;

    // --- compute an equivalent length
    project->Weir[k].length = 2.0 * project->RouteStep * sqrt(GRAVITY * project->Link[j].xsect.yFull);
    project->Weir[k].length = MAX(200.0, Weir[k].length);
    project->Weir[k].surfArea = 0.0;
}

//=============================================================================

double weir_getInflow(int j)
//
//  Input:   j = link index
//  Output:  returns weir flow rate (cfs)
//  Purpose: finds the flow over a weir.
//
{
    int    n1;          // index of upstream node
    int    n2;          // index of downstream node
    int    k;           // index of weir
    double q1;          // flow through central part of weir (cfs)
    double q2;          // flow through end sections of weir (cfs)
    double head;        // head on weir (ft)
    double h1;          // upstrm nodal head (ft)
    double h2;          // downstrm nodal head (ft)
    double hcrest;      // head at weir crest (ft)
    double hcrown;      // head at weir crown (ft)
    double y;           // water depth in weir (ft)
    double dir;         // direction multiplier
    double ratio;
    double weirPower[] = {1.5,       // transverse weir
                          5./3.,     // side flow weir
                          2.5,       // v-notch weir
                          1.5};      // trapezoidal weir

    n1 = project->Link[j].node1;
    n2 = project->Link[j].node2;
    k  = project->Link[j].subIndex;
    if ( project->RouteModel == DW )
    {
        h1 = project->Node[n1].newDepth + Node[n1].invertElev;
        h2 = project->Node[n2].newDepth + Node[n2].invertElev;
    }
    else
    {
        h1 = project->Node[n1].newDepth + Node[n1].invertElev;
        h2 = project->Node[n1].invertElev;
    }
    dir = (h1 > h2) ? +1.0 : -1.0;            

    // --- exchange h1 and h2 for reverse flow
    if ( dir < 0.0 )
    {
        head = h1;
        h1 = h2;
        h2 = head;
    }

    // --- find head of weir's crest and crown
    hcrest = project->Node[n1].invertElev + project->Link[j].offset1;
    hcrown = hcrest + project->Link[j].xsect.yFull;

    // --- adjust crest ht. for partially open weir
    hcrest += (1.0 - project->Link[j].setting) * Link[j].xsect.yFull;

    // --- compute head relative to weir crest
    head = h1 - hcrest;

    // --- return if head is negligible or flap gate closed
    project->Link[j].dqdh = 0.0;
    if ( head <= FUDGE || hcrest >= hcrown ||
         link_setFlapGate(j, n1, n2, dir) )
    {
        project->Link[j].newDepth = 0.0;
        project->Link[j].flowClass = DRY;
        return 0.0;
    }

    // --- determine flow class
    project->Link[j].flowClass = SUBCRITICAL;
    if ( hcrest > h2 )
    {
        if ( dir == 1.0 ) project->Link[j].flowClass = DN_CRITICAL;
        else              project->Link[j].flowClass = UP_CRITICAL;
    }

    // --- compute new equivalent surface area
    y = project->Link[j].xsect.yFull - (hcrown - MIN(h1, hcrown));
    project->Weir[k].surfArea = xsect_getWofY(&project->Link[j].xsect, y) * Weir[k].length;

//// Since weirs can't physically surcharge (because they have open tops)
///  the 5.0 code that applied an equivalent orifice eqn. when h1 > hcrown
///  was removed.

    // --- use weir eqn. to find flows through central (q1)
    //     and end sections (q2) of weir
    weir_getFlow(j, k, head, dir, project->Link[j].hasFlapGate, &q1, &q2);

    // --- apply Villemonte eqn. to correct for submergence
    if ( h2 > hcrest )
    {
        ratio = (h2 - hcrest) / (h1 - hcrest);
        q1 *= pow( (1.0 - pow(ratio, weirPower[project->Weir[k].type])), 0.385);
        if ( q2 > 0.0 )
            q2 *= pow( (1.0 - pow(ratio, weirPower[VNOTCH_WEIR])), 0.385);
    }

    // --- return total flow through weir
    project->Link[j].newDepth = MIN((h1 - hcrest), Link[j].xsect.yFull);
    return dir * (q1 + q2);
}

//=============================================================================

void weir_getFlow(int j, int k,  double head, double dir, int hasFlapGate,
                  double* q1, double* q2)
//
//  Input:   j    = link index
//           k    = weir index
//           head = head across weir (ft)
//           dir  = flow direction indicator
//           hasFlapGate = flap gate indicator
//  Output:  q1 = flow through central portion of weir (cfs)
//           q2 = flow through end sections of weir (cfs)
//  Purpose: computes flow over weir given head.
//
{
    double length;
    double h;
    double y;
    double hLoss;
    double area;
    double veloc;
    int    wType;

    // --- q1 = flow through central portion of weir,
    //     q2 = flow through end sections of trapezoidal weir
    *q1 = 0.0;
    *q2 = 0.0;
    project->Link[j].dqdh = 0.0;
    if ( head <= 0.0 ) return;

    // --- convert weir length & head to original units
    length = project->Link[j].xsect.wMax * UCF(LENGTH);
    h = head * UCF(LENGTH);

    // --- reduce length when end contractions present
    length -= 0.1 * project->Weir[k].endCon * h;
    length = MAX(length, 0.0);

    // --- use appropriate formula for weir flow
    wType = project->Weir[k].type;
    if ( wType == VNOTCH_WEIR &&
         project->Link[j].setting < 1.0 ) wType = TRAPEZOIDAL_WEIR;
    switch (wType)
    {
      case TRANSVERSE_WEIR:
        *q1 = project->Weir[k].cDisch1 * length * pow(h, 1.5);
        break;

      case SIDEFLOW_WEIR:
        // --- weir behaves as a transverse weir under reverse flow
        if ( dir < 0.0 )
            *q1 = project->Weir[k].cDisch1 * length * pow(h, 1.5);
        else
            *q1 = project->Weir[k].cDisch1 * length * pow(h, 5./3.);
        break;

      case VNOTCH_WEIR:
        *q1 = project->Weir[k].cDisch1 * Weir[k].slope * pow(h, 2.5);
        break;

      case TRAPEZOIDAL_WEIR:
        y = (1.0 - project->Link[j].setting) * Link[j].xsect.yFull;
        length = xsect_getWofY(&project->Link[j].xsect, y) * UCF(LENGTH);
        length -= 0.1 * project->Weir[k].endCon * h;
        length = MAX(length, 0.0);
        *q1 = project->Weir[k].cDisch1 * length * pow(h, 1.5);
        *q2 = project->Weir[k].cDisch2 * Weir[k].slope * pow(h, 2.5);
    }

    // --- convert CMS flows to CFS
    if ( project->UnitSystem == SI )
    {
        *q1 /= M3perFT3;
        *q2 /= M3perFT3;
    }

    // --- apply ARMCO adjustment for headloss from flap gate
    if ( hasFlapGate )
    {
        // --- compute flow area & velocity for current weir flow
        area = weir_getOpenArea(j, head);
        if ( area > TINY )
        {
            veloc = (*q1 + *q2) / area;

            // --- compute headloss and subtract from original head
            hLoss = (4.0 / GRAVITY) * veloc * veloc *
                     exp(-1.15 * veloc / sqrt(head) );
            head = head - hLoss;
            if ( head < 0.0 ) head = 0.0;

            // --- make recursive call to this function, with hasFlapGate
            //     set to false, to find flow values at adjusted head value
            weir_getFlow(j, k, head, dir, FALSE, q1, q2);
        }
    }
    project->Link[j].dqdh = weir_getdqdh(k, dir, head, *q1, *q2);
}


//=============================================================================

double weir_getOpenArea(int j, double y)
//
//  Input:   j = link index
//           y = depth of water above weir crest (ft)
//  Output:  returns area between weir crest and y (ft2)
//  Purpose: finds flow area through a weir.
//
{
    double z, zy;

    // --- find offset of weir crest due to control setting
    z = (1.0 - project->Link[j].setting) * Link[j].xsect.yFull;

    // --- ht. of crest + ht of water above crest
    zy = z + y;
    zy = MIN(zy, project->Link[j].xsect.yFull);

    // --- return difference between area of offset + water depth
    //     and area of just the offset
    return xsect_getAofY(&project->Link[j].xsect, zy) -
           xsect_getAofY(&project->Link[j].xsect, z);
}

//=============================================================================

double  weir_getdqdh(int k, double dir, double h, double q1, double q2)
{
    double q1h;
    double q2h;

    if ( fabs(h) < FUDGE ) return 0.0;
    q1h = fabs(q1/h);
    q2h = fabs(q2/h);

    switch (project->Weir[k].type)
    {
      case TRANSVERSE_WEIR: return 1.5 * q1h;

      case SIDEFLOW_WEIR:
        // --- weir behaves as a transverse weir under reverse flow
        if ( dir < 0.0 ) return 1.5 * q1h;
        else return 5./3. * q1h;

      case VNOTCH_WEIR: 
        if ( q2h == 0.0 ) return 2.5 * q1h;  // Fully open
        else return 1.5 * q1h + 2.5 * q2h;   // Partly open

      case TRAPEZOIDAL_WEIR: return 1.5 * q1h + 2.5 * q2h;
    }
    return 0.0;
}
 

//=============================================================================
//               O U T L E T    D E V I C E    M E T H O D S
//=============================================================================

int outlet_readParams(int j, int k, char* tok[], int ntoks)
//
//  Input:   j = link index
//           k = outlet index
//           tok[] = array of string tokens
//           ntoks = number of tokens
//  Output:  returns an error code
//  Purpose: reads outlet parameters from a tokenized  line of input.
//
{
    int    i, m, n;
    int    n1, n2;
    double x[6];
    char*  id;
    char*  s;

    // --- check for valid ID and end node IDs
    if ( ntoks < 6 ) return error_setInpError(ERR_ITEMS, "");
    id = project_findID(LINK, tok[0]);
    if ( id == NULL ) return error_setInpError(ERR_NAME, tok[0]);
    n1 = project_findObject(NODE, tok[1]);
    if ( n1 < 0 ) return error_setInpError(ERR_NAME, tok[1]);
    n2 = project_findObject(NODE, tok[2]);
    if ( n2 < 0 ) return error_setInpError(ERR_NAME, tok[2]);

    // --- get height above invert
    if ( project->LinkOffsets == ELEV_OFFSET && *tok[3] == '*' ) x[0] = MISSING;
    else
    {
        if ( ! getDouble(tok[3], &x[0]) )
            return error_setInpError(ERR_NUMBER, tok[3]);
	if ( project->LinkOffsets == DEPTH_OFFSET && x[0] < 0.0 ) x[0] = 0.0;
    }

    // --- see if outlet flow relation is tabular or functional
    m = findmatch(tok[4], RelationWords);
    if ( m < 0 ) return error_setInpError(ERR_KEYWORD, tok[4]);
    x[1] = 0.0;
    x[2] = 0.0;
    x[3] = -1.0;
    x[4] = 0.0;

    // --- see if rating curve is head or depth based
    x[5] = NODE_DEPTH;                                //default is depth-based
    s = strtok(tok[4], "/");                          //parse token for
    s = strtok(NULL, "/");                            //  qualifier term
    if ( strcomp(s, w_HEAD) ) x[5] = NODE_HEAD;       //check if its "HEAD"

    // --- get params. for functional outlet device
    if ( m == FUNCTIONAL )
    {
        if ( ntoks < 7 ) return error_setInpError(ERR_ITEMS, "");
        if ( ! getDouble(tok[5], &x[1]) )
            return error_setInpError(ERR_NUMBER, tok[5]);
        if ( ! getDouble(tok[6], &x[2]) )
            return error_setInpError(ERR_NUMBER, tok[6]);
        n = 7;
    }

    // --- get name of outlet rating curve
    else
    {
        i = project_findObject(CURVE, tok[5]);
        if ( i < 0 ) return error_setInpError(ERR_NAME, tok[5]);
        x[3] = i;
        n = 6;
    }

    // --- check if flap gate specified
    if ( ntoks > n)
    {
        i = findmatch(tok[n], NoYesWords);               
        if ( i < 0 ) return error_setInpError(ERR_KEYWORD, tok[n]);
        x[4] = i;
    }

    // --- add parameters to outlet object
    project->Link[j].ID = id;
    link_setParams(j, OUTLET, n1, n2, k, x);
    return 0;
}

//=============================================================================

double outlet_getInflow(int j)
//
//  Input:   j = link index
//  Output:  outlet flow rate (cfs)
//  Purpose: finds the flow through an outlet.
//
{
    int    k, n1, n2;
    double head, hcrest, h1, h2, y1, dir;

    // --- get indexes of end nodes
    n1 = project->Link[j].node1;
    n2 = project->Link[j].node2;
    k  = project->Link[j].subIndex;

    // --- find heads at upstream & downstream nodes
    if ( project->RouteModel == DW )
    {
        h1 = project->Node[n1].newDepth + Node[n1].invertElev;
        h2 = project->Node[n2].newDepth + Node[n2].invertElev;
    }
    else
    {
        h1 = project->Node[n1].newDepth + Node[n1].invertElev;
        h2 = project->Node[n1].invertElev;
    }
    dir = (h1 >= h2) ? +1.0 : -1.0; 

    // --- exchange h1 and h2 for reverse flow
    y1 = project->Node[n1].newDepth;
    if ( dir < 0.0 )
    {
        y1 = h1;
        h1 = h2;
        h2 = y1;
        y1 = project->Node[n2].newDepth;
    }

    // --- for a NODE_DEPTH rating curve the effective head across the
    //     outlet is the depth above the crest elev. while for a NODE_HEAD
    //     curve it is the difference between upstream & downstream heads
    hcrest = project->Node[n1].invertElev + project->Link[j].offset1;
    if ( project->Outlet[k].curveType == NODE_HEAD && project->RouteModel == DW )
        head = h1 - MAX(h2, hcrest);
    else head = h1 - hcrest;

    // --- no flow if either no effective head difference,
    //     no upstream water available, or closed flap gate
    if ( head <= FUDGE || y1 <= FUDGE ||
         link_setFlapGate(j, n1, n2, dir) )
    {
        project->Link[j].newDepth = 0.0;
        project->Link[j].flowClass = DRY;
        return 0.0;
    }

    // --- otherwise use rating curve to compute flow
    project->Link[j].newDepth = head;
    project->Link[j].flowClass = SUBCRITICAL;
    return dir * project->Link[j].setting * outlet_getFlow(k, head);
}

//=============================================================================

double outlet_getFlow(int k, double head)
//
//  Input:   k    = outlet index
//           head = head across outlet (ft)
//  Output:  returns outlet flow rate (cfs)
//  Purpose: computes flow rate through an outlet given head.
//
{
    int    m;
    double h;

    // --- convert head to original units
    h = head * UCF(LENGTH);

    // --- look-up flow in rating curve table if provided
    m = project->Outlet[k].qCurve;
    if ( m >= 0 ) return table_lookup(&project->Curve[m], h) / UCF(FLOW);
    
    // --- otherwise use function to find flow
    else return project->Outlet[k].qCoeff * pow(h, Outlet[k].qExpon) / UCF(FLOW);
}
