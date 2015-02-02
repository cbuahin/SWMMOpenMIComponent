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
#include  "infil.h"
#include <controls.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_STATS 5
	// Definition of 4-byte integer, 4-byte real and 8-byte real types
#define INT4  int
#define REAL4 float
#define REAL8 double

	typedef struct TXnode TXnode;

	//-----------------------------------------------------------------------------
	//  Constants from transect.c
	//-----------------------------------------------------------------------------
#define MAXSTATION 1500                // max. number of stations in a transect

	struct Project
	{

		int J1;
		int P1;
		int J2;
		double RT;


		TFile Finp;                     // Input file
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


		char Msg[MAXMSG + 1];            // Text of output message
		char Title[MAXTITLE][MAXMSG + 1];// Project title
		char TempDir[MAXFNAME + 1];      // Temporary file directory


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


		//-----------------------------------------------------------------------------
		//  Shared Variables from lid.c
		//-----------------------------------------------------------------------------
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
		//typedef int (*swmm_retrieve_openmi_items)(int, char*, char* , double* );
		//
		//  swmm_retrieve_openmi_items retrieve_openmi_exchangeItems;

		//-----------------------------------------------------------------------------
		//  Local Variables
		//-----------------------------------------------------------------------------
		//to do set to null
		THorton*   HortInfil;
		TGrnAmpt*  GAInfil;
		TCurveNum* CNInfil;


		//-----------------------------------------------------------------------------
		//  Shared variables controls.c
		//-----------------------------------------------------------------------------
		TRule*  Rules;             // Array of control rules
		TActionList* ActionList;        // Linked list of control actions
		int    InputState;                     // State of rule interpreter
		int    RuleCount;                      // Total number of rules
		double ControlValue;                   // Value of controller variable
		double SetPoint;                       // Value of controller setpoint


		//-----------------------------------------------------------------------------
		//  Shared variables from  input.c
		//-----------------------------------------------------------------------------
		char *Tok[MAXTOKS];             // String tokens from line of input
		int  Ntokens;                   // Number of tokens in line of input
		int  Mobjects[MAX_OBJ_TYPES];   // Working number of objects of each type
		int  Mnodes[MAX_NODE_TYPES];    // Working number of node objects
		int  Mlinks[MAX_LINK_TYPES];    // Working number of link objects


		//-----------------------------------------------------------------------------
		//  Shared variables from swmm.c
		//-----------------------------------------------------------------------------
		int  IsOpenFlag;           // TRUE if a project has been opened
		int  IsStartedFlag;        // TRUE if a simulation has been started
		int  SaveResultsFlag;      // TRUE if output to be saved to binary file
		int  ExceptionCount;       // number of exceptions handled
		int  DoRunoff;             // TRUE if runoff is computed
		int  DoRouting;            // TRUE if flow routing is computed


		//-----------------------------------------------------------------------------
		//  Shared variables from climate.c
		//-----------------------------------------------------------------------------
		// Temperature variables
		double    Tmin;                 // min. daily temperature (deg F)
		double    Tmax;                 // max. daily temperature (deg F)
		double    Trng;                 // 1/2 range of daily temperatures
		double    Trng1;                // prev. max - current min. temp.
		double    Tave;                 // average daily temperature (deg F)
		double    Hrsr;                 // time of min. temp. (hrs)
		double    Hrss;                 // time of max. temp (hrs) 
		double    Hrday;                // avg. of min/max temp times
		double    Dhrdy;                // hrs. between min. & max. temp. times
		double    Dydif;                // hrs. between max. & min. temp. times
		DateTime  LastDay;              // date of last day with temp. data

		// Evaporation variables
		DateTime  NextEvapDate;         // next date when evap. rate changes
		double    NextEvapRate;         // next evaporation rate (user units)

		// Climate file variables
		int      FileFormat;            // file format (see ClimateFileFormats)
		int      FileYear;              // current year of file data
		int      FileMonth;             // current month of year of file data
		int      FileDay;               // current day of month of file data
		int      FileLastDay;           // last day of current month of file data
		int      FileElapsedDays;       // number of days read from file
		double   FileValue[4];          // current day's values of climate data
		double   FileData[4][32];       // month's worth of daily climate data
		char     FileLine[MAXLINE + 1];   // line from climate data file


		//-----------------------------------------------------------------------------
		//  Shared variables
		//-----------------------------------------------------------------------------
		//  NOTE: all flux rates are in ft/sec, all depths are in ft.
		double    Infil;           // infiltration rate from surface
		double    MaxEvap;         // max. evaporation rate
		double    AvailEvap;       // available evaporation rate
		double    UpperEvap;       // evaporation rate from upper GW zone
		double    LowerEvap;       // evaporation rate from lower GW zone
		double    UpperPerc;       // percolation rate from upper to lower zone
		double    LowerLoss;       // loss rate from lower GW zone
		double    GWFlow;          // flow rate from lower zone to conveyance node
		double    MaxUpperPerc;    // upper limit on UpperPerc
		double    MaxGWFlowPos;    // upper limit on GWFlow when its positve
		double    MaxGWFlowNeg;    // upper limit on GWFlow when its negative
		double    FracPerv;        // fraction of surface that is pervious
		double    TotalDepth;      // total depth of GW aquifer
		double    Hgw;             // ht. of saturated zone
		double    Hstar;           // ht. from aquifer bottom to node invert
		double    Hsw;             // ht. from aquifer bottom to water surface
		double    Tstep;           // current time step (sec)
		TAquifer  A;               // aquifer being analyzed
		TGroundwater* GW;          // groundwater object being analyzed
		MathExpr* FlowExpr;        // user-supplied GW flow expression 


		//-----------------------------------------------------------------------------
		//  Shared variables from treatment.c
		//-----------------------------------------------------------------------------
		int     ErrCode;                // treatment error code
		int     J;                      // index of node being analyzed
		double  Dt;                     // curent time step (sec)
		double  Q;                      // node inflow (cfs)
		double  V;                      // node volume (ft3)
		double* R;                      // array of pollut. removals
		double* Cin;                    // node inflow concentrations
		TTreatment* Treatment;          // pointer to Treatment object


		//-----------------------------------------------------------------------------
		//  Exportable variables (shared with statsrpt.c) from stats.c
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
		//  Shared variables   from massbalc.
		//-----------------------------------------------------------------------------
		TRunoffTotals    RunoffTotals;    // overall surface runoff continuity totals
		TLoadingTotals*  LoadingTotals;   // overall WQ washoff continuity totals
		TGwaterTotals    GwaterTotals;    // overall groundwater continuity totals 
		TRoutingTotals   FlowTotals;      // overall routed flow continuity totals 
		TRoutingTotals*  QualTotals;      // overall routed WQ continuity totals 
		TRoutingTotals   StepFlowTotals;  // routed flow totals over time step
		TRoutingTotals   OldStepFlowTotals;
		TRoutingTotals*  StepQualTotals;  // routed WQ totals over time step


		//-----------------------------------------------------------------------------
		//  Imported variables from massbal.c
		//-----------------------------------------------------------------------------
		double*  NodeInflow;              // total inflow volume to each node (ft3)
		double*  NodeOutflow;             // total outflow volume from each node (ft3)
		double   TotalArea;               // total drainage area (ft2)


		//-----------------------------------------------------------------------------
		//  Shared variables from stats.c
		//-----------------------------------------------------------------------------
		TSysStats       SysStats;
		TMaxStats       MaxMassBalErrs[MAX_STATS];
		TMaxStats       MaxCourantCrit[MAX_STATS];
		TMaxStats       MaxFlowTurns[MAX_STATS];
		double          SysOutfallFlow;

		//-----------------------------------------------------------------------------
		// Shared variables   from subcatch.c
		//-----------------------------------------------------------------------------
		double     Losses;        // subcatch evap. + infil. loss rate (ft/sec)
		double     Outflow;       // subcatch outflow rate (ft/sec)

		// Volumes as either total (ft3) or per unit area (ft) depending on context
		double     Vrain;         // subcatch rain volume over a time step
		double     Vevap;         // subcatch evap. volume over a time step
		double     Vinfil;        // subcatch infil. volume over a time step

		double     Vrunon;        // subcatch runon volume over a time step (ft3)
		double     Vponded;       // volume of ponded water over subcatch (ft3)
		double     Voutflow;      // subcatch outflow depth (ft3)

		TSubarea* theSubarea;     // subarea to which getDdDt() is applied


		//-----------------------------------------------------------------------------
		//  Shared variables    from output.c
		//-----------------------------------------------------------------------------
		INT4      IDStartPos;           // starting file position of ID names
		INT4      InputStartPos;        // starting file position of input data
		INT4      OutputStartPos;       // starting file position of output data
		INT4      BytesPerPeriod;       // bytes saved per simulation time period
		INT4      NsubcatchResults;     // number of subcatchment output variables
		INT4      NnodeResults;         // number of node output variables
		INT4      NlinkResults;         // number of link output variables
		INT4      NumSubcatch;          // number of subcatchments reported on
		INT4      NumNodes;             // number of nodes reported on
		INT4      NumLinks;             // number of links reported on
		INT4      NumPolluts;           // number of pollutants reported on
		REAL4     SysResults[MAX_SYS_RESULTS];    // values of system output vars.

		//-----------------------------------------------------------------------------
		//  Exportable variables (shared with report.c) from output.c
		//-----------------------------------------------------------------------------
		REAL4*           SubcatchResults;
		REAL4*           NodeResults;
		REAL4*           LinkResults;


		//-----------------------------------------------------------------------------
		// Shared variables from runoff.c
		//-----------------------------------------------------------------------------
		char  IsRaining;                // TRUE if precip.falls on study area
		char  HasRunoff;                // TRUE if study area generates runoff
		char  HasSnow;                  // TRUE if any snow cover on study area
		int   Nsteps;                   // number of runoff time steps taken
		int   MaxSteps;                 // final number of runoff time steps
		long  MaxStepsPos;              // position in Runoff interface file
		//    where MaxSteps is saved

		//-----------------------------------------------------------------------------
		//  Exportable variables (shared with subcatch.c) from runoff.c
		//-----------------------------------------------------------------------------
		double* OutflowLoad;         // outflow pollutant mass from a subcatchment 
		double* WashoffLoad;         // washoff pollutant mass from landuses

		//-----------------------------------------------------------------------------                  
		// Shared Variables from rdii.c
		//-----------------------------------------------------------------------------                  
		TUHGroup*  UHGroup;             // processing data for each UH group
		int        RdiiStep;            // RDII time step (sec)
		int        NumRdiiNodes;        // number of nodes w/ RDII data
		int*       RdiiNodeIndex;       // indexes of nodes w/ RDII data
		REAL4*     RdiiNodeFlow;        // inflows for nodes with RDII          //(5.1.003)
		int        RdiiFlowUnits;       // RDII flow units code
		DateTime   RdiiStartDate;       // start date of RDII inflow period
		DateTime   RdiiEndDate;         // end date of RDII inflow period 
		double     TotalRainVol;        // total rainfall volume (ft3)
		double     TotalRdiiVol;        // total RDII volume (ft3)
		int        RdiiFileType;        // type (binary/text) of RDII file

		//-----------------------------------------------------------------------------                  
		//  Shared variables ifil.c
		//-----------------------------------------------------------------------------                  
		int      IfaceFlowUnits;        // flow units for routing interface file
		int      IfaceStep;             // interface file time step (sec)
		int      NumIfacePolluts;       // number of pollutants in interface file
		int*     IfacePolluts;          // indexes of interface file pollutants
		int      NumIfaceNodes;         // number of nodes on interface file
		int*     IfaceNodes;            // indexes of nodes on interface file
		double** OldIfaceValues;        // interface flows & WQ at previous time
		double** NewIfaceValues;        // interface flows & WQ at next time
		double   IfaceFrac;             // fraction of interface file time step
		DateTime OldIfaceDate;          // previous date of interface values
		DateTime NewIfaceDate;          // next date of interface values



		//-----------------------------------------------------------------------------
		//  Shared variables from transect.c
		//-----------------------------------------------------------------------------
		 int    Ntransects;              // total number of transects
		 int    Nstations;               // number of stations in current transect
		 double  Station[MAXSTATION + 1];  // x-coordinate of each station
		 double  Elev[MAXSTATION + 1];     // elevation of each station
		 double  Nleft;                  // Manning's n for left overbank
		 double  Nright;                 // Manning's n for right overbank
		 double  Nchannel;               // Manning's n for main channel
		 double  Xleftbank;              // station where left overbank ends
		 double  Xrightbank;             // station where right overbank begins
		 double  Xfactor;                // multiplier for station spacing
		 double  Yfactor;                // factor added to station elevations
		 double  Lfactor;                // main channel/flood plain length


		 //-----------------------------------------------------------------------------
		 //  Shared variables
		 //-----------------------------------------------------------------------------
		 double Atotal;
		 double Ptotal;


		 //-----------------------------------------------------------------------------
		 //  Shared variables from kinwave;c
		 //-----------------------------------------------------------------------------
		 double   Beta1;
		 double   C1;
		 double   C2;
		 double   Afull;
		 double   Qfull;
		 TXsect*  pXsect;

		 //-----------------------------------------------------------------------------
		 //  Shared Variables from dynwave.c
		 //-----------------------------------------------------------------------------
		 double  VariableStep;           // size of variable time step (sec)
		 TXnode* Xnode;                  // extended nodal information
		 double  Omega;                  // actual under-relaxation parameter
		 int     Steps;                  // number of Picard iterations

		 //-----------------------------------------------------------------------------
		 // Shared variables from routing.c
		 //-----------------------------------------------------------------------------
		 int* SortedLinks;

		 //-----------------------------------------------------------------------------
		 //  Shared variables from toposort.c
		 //-----------------------------------------------------------------------------
		  int* InDegree;                  // number of incoming links to each node
		  int* StartPos;                  // start of a node's outlinks in AdjList
		  int* AdjList;                   // list of outlink indexes for each node
		  int* Stack;                     // array of nodes "reached" during sorting
		  int  First;                     // position of first node in stack
		  int  Last;                      // position of last node added to stack

		  char* Examined;                 // TRUE if node included in spanning tree
		  char* InTree;                   // state of each link in spanning tree:
		 // 0 = unexamined,
		 // 1 = in spanning tree,
		 // 2 = chord of spanning tree
		  int*  LoopLinks;                // list of links which forms a loop
		  int   LoopLinksLast;            // number of links in a loop


		  //-----------------------------------------------------------------------------
		  //  Shared variables from rain.c
		  //-----------------------------------------------------------------------------
		  TRainStats RainStats;                  // see objects.h for definition
		  int        Condition;                  // rainfall condition code
		  int        TimeOffset;                 // time offset of rainfall reading (sec)
		  int        DataOffset;                 // start of data on line of input
		  int        ValueOffset;                // start of rain value on input line
		  int        RainType;                   // rain measurement type code
		  int        Interval;                   // rain measurement interval (sec)
		  double     UnitsFactor;                // units conversion factor
		  float      RainAccum;                  // rainfall depth accumulation
		  char       *StationID;                 // station ID appearing in rain file
		  DateTime   AccumStartDate;             // date when accumulation begins
		  DateTime   PreviousDate;               // date of previous rainfall record 
		  int        GageIndex;                  // index of rain gage analyzed
		  int        hasStationName;             // true if data contains station name
	};


	typedef struct  Project Project;
#ifdef __cplusplus
}   // matches the linkage specification from above */ 
#endif
#endif
