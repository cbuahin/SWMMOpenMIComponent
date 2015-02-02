//-----------------------------------------------------------------------------
//   kinwave.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/20/14  (Build 5.1.001)
//   Author:   L. Rossman (EPA)
//             M. Tryby (EPA)
//
//   Kinematic wave flow routing functions.
//
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE


#include <math.h>
#include "headers.h"
#include "findroot.h"

//-----------------------------------------------------------------------------
//  Constants 
//-----------------------------------------------------------------------------
static const double WX      = 0.6;     // distance weighting
static const double WT      = 0.6;     // time weighting
static const double EPSIL   = 0.001;   // convergence criterion

//-----------------------------------------------------------------------------
//  Shared variables
//-----------------------------------------------------------------------------
//static double   project-> Beta1;
//static double   project->C1;
//static double   project->C2;
//static double   project-> Afull;
//static double   project-> Qfull;
//static TXsect*  project-> pXsect;

//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)
//-----------------------------------------------------------------------------
//  kinwave_execute  (called by flowrout_execute)

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------
static int   solveContinuity(Project* project, double qin, double ain, double* aout);
static void  evalContinuity(Project* project, double a, double* f, double* df, void* p);

//=============================================================================

int kinwave_execute(Project* project, int j, double* qinflow, double* qoutflow, double tStep)
//
//  Input:   j = link index
//           qinflow = inflow at current time (cfs)
//           tStep = time step (sec)
//  Output:  qoutflow = outflow at current time (cfs),
//           returns number of iterations used
//  Purpose: finds outflow over time step tStep given flow entering a
//           conduit using Kinematic Wave flow routing.
//
//
//                               ^ q3 
//  t                            |   
//  |          qin, ain |-------------------| qout, aout
//  |                   |  Flow --->        |
//  |----> x     q1, a1 |-------------------| q2, a2
//
//
{
    int    k;
    int    result = 1;
    double dxdt, dq;
    double ain, aout;
    double qin, qout;
    double a1, a2, q1, q2, q3;

    // --- no routing for non-conduit link
    (*qoutflow) = (*qinflow); 
    if ( project-> Link[j].type != CONDUIT ) return result;

    // --- no routing for dummy xsection
    if ( project-> Link[j].xsect.type == DUMMY ) return result;

    // --- assign module-level variables
    project-> pXsect = &project-> Link[j].xsect;
    project-> Qfull = project-> Link[j].qFull;
    project-> Afull = project-> Link[j].xsect.aFull;
    k = project-> Link[j].subIndex;
    project-> Beta1 = project->Conduit[k].beta / project-> Qfull;
 
    // --- normalize previous flows
    q1 = project->Conduit[k].q1 / project-> Qfull;
    q2 = project->Conduit[k].q2 / project-> Qfull;

    // --- compute evaporation and infiltration loss rate
	q3 = link_getLossRate(project,j, tStep) / project-> Qfull;

    // --- normalize previous areas
    a1 = project->Conduit[k].a1 / project-> Afull;
    a2 = project->Conduit[k].a2 / project-> Afull;

    // --- normalize inflow 
    qin = (*qinflow) / project->Conduit[k].barrels / project-> Qfull;

    // --- use full area when inlet flow >= full flow
    if ( qin >= 1.0 ) ain = 1.0;

    // --- get normalized inlet area corresponding to inlet flow
    else ain = xsect_getAofS(project,project-> pXsect, qin/project-> Beta1) / project-> Afull;

    // --- check for no flow
    if ( qin <= TINY && q2 <= TINY )
    {
        qout = 0.0;
        aout = 0.0;
    }

    // --- otherwise solve finite difference form of continuity eqn.
    else
    {
        // --- compute constant factors
        dxdt = link_getLength(project,j) / tStep * project-> Afull / project-> Qfull;
        dq   = q2 - q1;
        project->C1   = dxdt * WT / WX;
        project->C2   = (1.0 - WT) * (ain - a1);
        project->C2   = project->C2 - WT * a2;
        project->C2   = project->C2 * dxdt / WX;
        project->C2   = project->C2 + (1.0 - WX) / WX * dq - qin;
        project->C2   = project->C2 + q3 / WX;

        // --- starting guess for aout is value from previous time step
        aout = a2;

        // --- solve continuity equation for aout
        result = solveContinuity(project,qin, ain, &aout);

        // --- report error if continuity eqn. not solved
        if ( result == -1 )
        {
            report_writeErrorMsg(project,ERR_KINWAVE, project-> Link[j].ID);
            return 1;
        }
        if ( result <= 0 ) result = 1;

        // --- compute normalized outlet flow from outlet area
        qout = project-> Beta1 * xsect_getSofA(project,project-> pXsect, aout*project-> Afull);
        if ( qin > 1.0 ) qin = 1.0;
    }

    // --- save new flows and areas
    project->Conduit[k].q1 = qin * project-> Qfull;
    project->Conduit[k].a1 = ain * project-> Afull;
    project->Conduit[k].q2 = qout * project-> Qfull;
    project->Conduit[k].a2 = aout * project-> Afull;
    (*qinflow)  = project->Conduit[k].q1 * project->Conduit[k].barrels;
    (*qoutflow) = project->Conduit[k].q2 * project->Conduit[k].barrels;
    return result;
}

//=============================================================================

int solveContinuity(Project* project, double qin, double ain, double* aout)
//
//  Input:   qin = upstream normalized flow
//           ain = upstream normalized area
//           aout = downstream normalized area
//  Output:  new value for aout; returns an error code
//  Purpose: solves continuity equation f(a) = project-> Beta1*S(a) + project->C1*a + project->C2 = 0
//           for 'a' using the Newton-Raphson root finder function.
//           Return code has the following meanings:
//           >= 0 number of function evaluations used
//           -1   Newton function failed
//           -2   flow always above max. flow
//           -3   flow always below zero
//
//     Note: project-> pXsect (pointer to conduit's cross-section), and constants project-> Beta1,
//           project->C1, and project->C2 are module-level shared variables assigned values
//           in kinwave_execute().
//
{
    int    n;                          // # evaluations or error code
    double aLo, aHi, aTmp;             // lower/upper bounds on a
    double fLo, fHi;                   // lower/upper bounds on f
    double tol = EPSIL;                // absolute convergence tol.

    // --- first determine bounds on 'a' so that f(a) passes through 0.

    // --- set upper bound to area at full flow
    aHi = 1.0;
    fHi = 1.0 + project->C1 + project->C2;

    // --- try setting lower bound to area where section factor is maximum
    aLo = xsect_getAmax(project-> pXsect) / project-> Afull;
    if ( aLo < aHi )
    {
        fLo = ( project-> Beta1 * project-> pXsect->sMax ) + (project->C1 * aLo) + project->C2;
    }
    else fLo = fHi;

    // --- if fLo and fHi have same sign then set lower bound to 0
    if ( fHi*fLo > 0.0 )
    {
        aHi = aLo;
        fHi = fLo;
        aLo = 0.0;
        fLo = project->C2;
    }

    // --- proceed with search for root if fLo and fHi have different signs
    if ( fHi*fLo <= 0.0 )
    {
        // --- start search at midpoint of lower/upper bounds
        //     if initial value outside of these bounds
		if ( *aout < aLo || *aout > aHi ) *aout = 0.5*(aLo + aHi);

        // --- if fLo > fHi then switch aLo and aHi
        if ( fLo > fHi )
        {
            aTmp = aLo;
            aLo  = aHi;
            aHi  = aTmp;
        }

        // --- call the Newton root finder method passing it the 
        //     evalContinuity function to evaluate the function
        //     and its derivatives
        n = findroot_Newton_added(aLo, aHi, aout, tol, evalContinuity, NULL,project);

        // --- check if root finder succeeded
        if ( n <= 0 ) n = -1;
    }

    // --- if lower/upper bound functions both negative then use full flow
    else if ( fLo < 0.0 )
    {
        if ( qin > 1.0 ) *aout = ain;
        else *aout = 1.0;
        n = -2;
    }

    // --- if lower/upper bound functions both positive then use no flow
    else if ( fLo > 0 )
    {
        *aout = 0.0;
        n = -3;
    }
    else n = -1;
    return n;
}

//=============================================================================

void evalContinuity(Project* project, double a, double* f, double* df, void* p)
//
//  Input:   a = outlet normalized area
//  Output:  f = value of continuity eqn.
//           df = derivative of continuity eqn.
//  Purpose: computes value of continuity equation (f) and its derivative (df)
//           w.r.t. normalized area for link with normalized outlet area 'a'.
//
{
    *f  = (project-> Beta1 * xsect_getSofA(project,project-> pXsect, a*project-> Afull)) + (project->C1 * a) + project->C2;
    *df = (project-> Beta1 * project-> Afull * xsect_getdSdA(project,project-> pXsect, a*project-> Afull)) + project->C1;
}

//=============================================================================
