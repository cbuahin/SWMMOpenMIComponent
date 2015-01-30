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
#include "lid.h"
#include "infil.h"


#ifdef __cplusplus
 extern "C" {
#endif 

    struct Project
    {

        int J1;
        int P1;
        int J2;
        double RT;


        TFile Finp ;                     // Input file
        TFile Fout;                     // Output file
        TFile Frpt;                     // Report file
        TFile Fclimate;                 // Climate file
        TFile Frain;                    // Rainfall file
        TFile Frunoff;                  // Runoff file
        TFile Frdii;                    // RDII inflow file
        TFile Fhotstart1;               // Hot start input file
        TFile Fhotstart2;               // Hot start output file
        TFile Finflows;                 // Inflows routing file
        TFile Foutflows;                // Outflows routing file


        long Nperiods;                 // Number of reporting periods
        long StepCount;                // Number of routing steps used
        long NonConvergeCount;         // Number of non-converging steps


        char Msg[MAXMSG+1];            // Text of output message
        char Title[MAXTITLE][MAXMSG+1];// Project title
        char TempDir[MAXFNAME+1];      // Temporary file directory


        TRptFlags RptFlags;                    // Reporting options


        int Nobjects[MAX_OBJ_TYPES];  // Number of each object type
        int Nnodes[MAX_NODE_TYPES];   // Number of each node sub-type
        int Nlinks[MAX_LINK_TYPES];   // Number of each link sub-type
        int UnitSystem;               // Unit system
        int FlowUnits;                // Flow units
        int InfilModel;               // Infiltration method
        int RouteModel;               // Flow routing method
        int ForceMainEqn;             // Flow equation for force mains
        int LinkOffsets;              // Link offset convention
        int AllowPonding;             // Allow water to pond at nodes
        int InertDamping;             // Degree of inertial damping
        int NormalFlowLtd;            // Normal flow limited
        int SlopeWeighting;           // Use slope weighting
        int Compatibility;            // SWMM 5/3/4 compatibility
        int SkipSteadyState;          // Skip over steady state periods
        int IgnoreRainfall;           // Ignore rainfall/runoff
        int IgnoreRDII;               // Ignore RDII                     //(5.1.004)
        int IgnoreSnowmelt;           // Ignore snowmelt
        int IgnoreGwater;             // Ignore groundwater
        int IgnoreRouting;            // Ignore flow routing
        int IgnoreQuality;            // Ignore water quality
        int ErrorCode;                // Error code number
        int WarningCode;              // Warning code number
        int WetStep;                  // Runoff wet time step (sec)
        int DryStep;                  // Runoff dry time step (sec)
        int ReportStep;               // Reporting time step (sec)
        int SweepStart;               // Day of year when sweeping starts
        int SweepEnd;                 // Day of year when sweeping ends
        int MaxTrials;                // Max. trials for DW routing


        double RouteStep;                // Routing time step (sec)
        double LengtheningStep;          // Time step for lengthening (sec)
        double StartDryDays;             // Antecedent dry days
        double CourantFactor;            // Courant time step factor
        double MinSurfArea;              // Minimum nodal surface area
        double MinSlope;                 // Minimum conduit slope
        double RunoffError;              // Runoff continuity error
        double GwaterError;              // Groundwater continuity error
        double FlowError;                // Flow routing error
        double QualError;                // Quality routing error
        double HeadTol;                  // DW routing head tolerance (ft)
        double SysFlowTol;               // Tolerance for steady system flow
        double LatFlowTol;               // Tolerance for steady nodal inflow


        DateTime StartDate;                // Starting date
        DateTime StartTime;                // Starting time
        DateTime StartDateTime;            // Starting Date+Time
        DateTime EndDate;                  // Ending date
        DateTime EndTime;                  // Ending time
        DateTime EndDateTime;              // Ending Date+Time
        DateTime ReportStartDate;          // Report start date
        DateTime ReportStartTime;          // Report start time
        DateTime ReportStart;              // Report start Date+Time


        double ReportTime;               // Current reporting time (msec)
        double OldRunoffTime;            // Previous runoff time (msec)
        double NewRunoffTime;            // Current runoff time (msec)
        double OldRoutingTime;           // Previous routing time (msec)
        double NewRoutingTime;           // Current routing time (msec)
        double TotalDuration;            // Simulation duration (msec)

        TTemp      Temp;                     // Temperature data
        TEvap      Evap;                     // Evaporation data
        TWind      Wind;                     // Wind speed data
        TSnow      Snow;                     // Snow melt data
        TSnowmelt* Snowmelt;                 // Array of snow melt objects
        TGage*     Gage;                     // Array of rain gages
        TSubcatch* Subcatch;                 // Array of subcatchments
        TAquifer*  Aquifer;                  // Array of groundwater aquifers
        TUnitHyd*  UnitHyd;                  // Array of unit hydrographs
        TNode*     Node;                     // Array of nodes
        TOutfall*  Outfall;                  // Array of outfall nodes
        TDivider*  Divider;                  // Array of divider nodes
        TStorage*  Storage;                  // Array of storage nodes
        TLink*     Link;                     // Array of links
        TConduit*  Conduit;                  // Array of conduit links
        TPump*     Pump;                     // Array of pump links
        TOrifice*  Orifice;                  // Array of orifice links
        TWeir*     Weir;                     // Array of weir links
        TOutlet*   Outlet;                   // Array of outlet device links
        TPollut*   Pollut;                   // Array of pollutants
        TLanduse*  Landuse;                  // Array of landuses
        TPattern*  Pattern;                  // Array of time patterns
        TTable*    Curve;                    // Array of curve tables
        TTable*    Tseries;                  // Array of time series tables
        TTransect* Transect;                 // Array of transect data
        TShape*    Shape;                    // Array of custom conduit shapes
        
        HTtable* Htable[MAX_OBJ_TYPES]; // Hash tables for object ID names
        char     MemPoolAllocated;      // TRUE if memory pool allocated
        
        TLidProc*  LidProcs;            // array of LID processes
        int        LidCount;            // number of LID processes
        TLidGroup* LidGroups;           // array of LID process groups
        int        GroupCount;          // number of LID groups (subcatchments)
        
        double     EvapRate;            // evaporation rate (ft/s)
        double     NativeInfil;         // native soil infil. rate (ft/s)
        double     MaxNativeInfil;      // native soil infil. rate limit (ft/s)
        
        double     TotalEvapVol;        // subcatch. evap loss (ft3)
        double     TotalPervEvapVol;    // evap loss over pervious area (ft3)
        double     TotalInfilVol;       // subcatch infiltration loss (ft3)
        double     NextReportTime;
        int        SaveResults;         // = 1 if detailed results to be saved


        //to do set to null
        THorton*   HortInfil;
        TGrnAmpt*  GAInfil;
        TCurveNum* CNInfil ;
        

    };
     
     typedef struct Project Project;
    
#ifdef __cplusplus
}   // matches the linkage specification from above */ 
#endif
#endif
