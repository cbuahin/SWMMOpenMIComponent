//-----------------------------------------------------------------------------
//   globals.h
//
//   Project: EPA SWMM5
//   Version: 5.1
//   Date:    03/19/14  (Build 5.1.000)
//            04/14/14  (Build 5.1.004)
//   Author:  L. Rossman
//
//   Global Variables
//-----------------------------------------------------------------------------
#ifndef GLOBALS_H
#define GLOBALS_H

#include "hash.h"

#ifdef __cplusplus
 extern "C" {
#endif 

    struct Project
    {

        int J1;
        int P1;
        int J2;
        double RT;


  
                TFile project->Finp;                     // Input file
                TFile  project->Fout;                     // Output file
               TFile   project->Frpt;                     // Report file
               TFile   project->Fclimate;                 // Climate file
               TFile   project->Frain;                    // Rainfall file
               TFile   project->Frunoff;                  // Runoff file
               TFile   project->Frdii;                    // RDII inflow file
               TFile   project->Fhotstart1;               // Hot start input file
               TFile   project->Fhotstart2;               // Hot start output file
               TFile   project->Finflows;                 // Inflows routing file
               TFile   project->Foutflows;                // Outflows routing file

  
                 long project->Nperiods;                 // Number of reporting periods
                 long project->StepCount;                // Number of routing steps used
                 long project->NonConvergeCount;         // Number of non-converging steps

  
                 char project->Msg[MAXMSG+1];            // Text of output message
                 char project->Title[MAXTITLE][MAXMSG+1];// Project title
                 char project->TempDir[MAXFNAME+1];      // Temporary file directory

  
                 TRptFlags project->RptFlags;                 // Reporting options

  
                 int project->Nobjects[MAX_OBJ_TYPES];  // Number of each object type
                 int project->Nnodes[MAX_NODE_TYPES];   // Number of each node sub-type
                 int project->Nlinks[MAX_LINK_TYPES];   // Number of each link sub-type
                 int project->UnitSystem;               // Unit system
                 int project->FlowUnits;                // Flow units
                 int project->InfilModel;               // Infiltration method
                 int project->RouteModel;               // Flow routing method
                 int project->ForceMainEqn;             // Flow equation for force mains
                 int project->LinkOffsets;              // project->Link offset convention
                 int project->AllowPonding;             // Allow water to pond at nodes
                 int project->InertDamping;             // Degree of inertial damping
                 int project->NormalFlowLtd;            // Normal flow limited
                int  project->SlopeWeighting;           // Use slope weighting
                 int project->Compatibility;            // SWMM 5/3/4 compatibility
                 int project->SkipSteadyState;          // Skip over steady state periods
                 int project->IgnoreRainfall;           // Ignore rainfall/runoff
                 int project->IgnoreRDII;               // Ignore RDII                     //(5.1.004)
                 int project->IgnoreSnowmelt;           // Ignore snowmelt
                 int project->IgnoreGwater;             // Ignore groundwater
                 int project->IgnoreRouting;            // Ignore flow routing
                 int project->IgnoreQuality;            // Ignore water quality
                 int project->ErrorCode;                // Error code number
                 int project->WarningCode;              // Warning code number
                 int project->WetStep;                  // Runoff wet time step (sec)
                 int project->DryStep;                  // Runoff dry time step (sec)
                 int project->ReportStep;               // Reporting time step (sec)
                 int project->SweepStart;               // Day of year when sweeping starts
                 int project->SweepEnd;                 // Day of year when sweeping ends
                 int project->MaxTrials;                // Max. trials for DW routing

  
                 double project->RouteStep;                // Routing time step (sec)
                 double project->LengtheningStep;          // Time step for lengthening (sec)
                 double project->StartDryDays;             // Antecedent dry days
                 double project->CourantFactor;            // Courant time step factor
                 double project->MinSurfArea;              // Minimum nodal surface area
                 double project->MinSlope;                 // Minimum conduit slope
                 double project->RunoffError;              // Runoff continuity error
                 double project->GwaterError;              // Groundwater continuity error
                 double project->FlowError;                // Flow routing error
                 double project->QualError;                // Quality routing error
                 double project->HeadTol;                  // DW routing head tolerance (ft)
                 double project->SysFlowTol;               // Tolerance for steady system flow
                 double project->LatFlowTol;               // Tolerance for steady nodal inflow

  
                 DateTime project->StartDate;                // Starting date
                 DateTime project->StartTime;                // Starting time
                 DateTime project->StartDateTime;            // Starting Date+Time
                 DateTime project->EndDate;                  // Ending date
                 DateTime project->EndTime;                  // Ending time
                 DateTime project->EndDateTime;              // Ending Date+Time
                 DateTime project->ReportStartDate;          // Report start date
                 DateTime project->ReportStartTime;          // Report start time
                 DateTime project->ReportStart;              // Report start Date+Time

  
                 double project->ReportTime;               // Current reporting time (msec)
                 double project->OldRunoffTime;            // Previous runoff time (msec)
                 double project->NewRunoffTime;            // Current runoff time (msec)
                 double project->OldRoutingTime;           // Previous routing time (msec)
                 double project->NewRoutingTime;           // Current routing time (msec)
                 double project->TotalDuration;            // Simulation duration (msec)

        TTemp      project->Temp;                     // Temperature data
        TEvap      project->Evap;                     // Evaporation data
        TWind      project->Wind;                     // Wind speed data
        TSnow      project->Snow;                     // Snow melt data
        TSnowmelt* project->Snowmelt;                 // Array of snow melt objects
        TGage*     project->Gage;                     // Array of rain gages
        TSubcatch* project->Subcatch;                 // Array of subcatchments
        TAquifer*  project->Aquifer;                  // Array of groundwater aquifers
        TUnitHyd*  project->UnitHyd;                  // Array of unit hydrographs
        TNode*     project->Node;                     // Array of nodes
        TOutfall*  project->Outfall;                  // Array of outfall nodes
        TDivider*  project->Divider;                  // Array of divider nodes
        TStorage*  project->Storage;                  // Array of storage nodes
        TLink*     project->Link;                     // Array of links
        TConduit*  project->Conduit;                  // Array of conduit links
        TPump*     project->Pump;                     // Array of pump links
        TOrifice*  project->Orifice;                  // Array of orifice links
        TWeir*     project->Weir;                     // Array of weir links
        TOutlet*   project->Outlet;                   // Array of outlet device links
        TPollut*   project->Pollut;                   // Array of pollutants
        TLanduse*  project->Landuse;                  // Array of landuses
        TPattern*  project->Pattern;                  // Array of time patterns
        TTable*    project->Curve;                    // Array of curve tables
        TTable*    project->Tseries;                  // Array of time series tables
        TTransect* project->Transect;                 // Array of transect data
        TShape*    project->Shape;                    // Array of custom conduit shapes

//typedef int (*swmm_retrieve_openmi_items)(int, char*, char* , double* );
//
//  swmm_retrieve_openmi_items retrieve_openmi_exchangeItems;

    };
    
#ifdef __cplusplus
}   // matches the linkage specification from above */ 
#endif
#endif
