//-----------------------------------------------------------------------------
//   swmm5.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/19/14  (Build 5.1.001)
//   Author:   L. Rossman
//
//   This is the main module of the computational engine for Version 5 of
//   the U.S. Environmental Protection Agency's Storm Water Management Model
//   (SWMM). It contains functions that control the flow of computations.
//
//   Depending on how it is compiled, this engine can be executed either as
//   a command line executable or through a series of calls made to functions
//   in a dynamic link library.
//
//-----------------------------------------------------------------------------

#define _CRT_SECURE_NO_DEPRECATE

#include "swmmopenmi.h"


//**********************************************************
//  Leave only one of the following 3 lines un-commented,
//  depending on the choice of compilation target
//**********************************************************
//#define CLE     /* Compile as a command line executable */
//#define SOL     /* Compile as a shared object library */
#define DLL     /* Compile as a Windows DLL */

// --- define WINDOWS
#undef WINDOWS
#ifdef _WIN32
#define WINDOWS
#endif
#ifdef __WIN32__
#define WINDOWS
#endif

// --- headers for exception handling
#ifdef WINDOWS
#include <windows.h>
#include <excpt.h>
#endif

//#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <float.h>
//-----------------------------------------------------------------------------
//  SWMM's header files
//
//  Note: the directives listed below are also contained in headers.h which
//        is included at the start of most of SWMM's other code modules.
//-----------------------------------------------------------------------------
#include "consts.h"                    // defined constants
#include "macros.h"                    // macros used throughout SWMM
#include "enums.h"                     // enumerated variables
#include "error.h"                     // error message codes
#include "datetime.h"                  // date/time functions
#include "objects.h"                   // definitions of SWMM's data objects
#include "funcs.h"                     // declaration of all global functions
#include "text.h"                      // listing of all text strings


#define  EXTERN                        // defined as 'extern' in headers.h
#include "globals.h"                   // declaration of all global variables

#include "swmm5.h"                     // declaration of exportable functions
//   callable from other programs
#define  MAX_EXCEPTIONS 100            // max. number of exceptions handled

//-----------------------------------------------------------------------------
//  Unit conversion factors
//-----------------------------------------------------------------------------
const double Ucf[10][2] =
{//  US      SI
    {43200.0,   1097280.0 },         // RAINFALL (in/hr, mm/hr --> ft/sec)
    {12.0,      304.8     },         // RAINDEPTH (in, mm --> ft)
    {1036800.0, 26334720.0},         // EVAPRATE (in/day, mm/day --> ft/sec)
    {1.0,       0.3048    },         // LENGTH (ft, m --> ft)
    {2.2956e-5, 0.92903e-5},         // LANDAREA (ac, ha --> ft2)
    {1.0,       0.02832   },         // VOLUME (ft3, m3 --> ft3)
    {1.0,       1.608     },         // WINDSPEED (mph, km/hr --> mph)
    {1.0,       1.8       },         // TEMPERATURE (deg F, deg C --> deg F)
    {2.203e-6,  1.0e-6    },         // MASS (lb, kg --> mg)
    {43560.0,   3048.0    }          // GWFLOW (cfs/ac, cms/ha --> ft/sec)
};
const double Qcf[6] =                  // Flow Conversion Factors:
{ 1.0,     448.831, 0.64632,     // cfs, gpm, mgd --> cfs
    0.02832, 28.317,  2.4466 };    // cms, lps, mld --> cfs

//-----------------------------------------------------------------------------
//  Shared variables
//-----------------------------------------------------------------------------
static int  IsOpenFlag;           // TRUE if a project has been opened
static int  IsStartedFlag;        // TRUE if a simulation has been started
static int  SaveResultsFlag;      // TRUE if output to be saved to binary file
static int  ExceptionCount;       // number of exceptions handled
static int  DoRunoff;             // TRUE if runoff is computed
static int  DoRouting;            // TRUE if flow routing is computed

//-----------------------------------------------------------------------------
//  External functions (prototyped in swmm5.h)
//-----------------------------------------------------------------------------
//  swmm_run
//  swmm_open
//  swmm_start
//  swmm_step
//  swmm_end
//  swmm_report
//  swmm_close
//  swmm_getMassBalErr
//  swmm_getVersion

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------
static void execRouting(Project* project, DateTime elapsedTime);

// Exception filtering function
#ifdef WINDOWS
static int  xfilter(int xc, DateTime elapsedTime, long step);
#endif

//-----------------------------------------------------------------------------
//  Entry point used to compile a stand-alone executable.
//-----------------------------------------------------------------------------
#ifdef CLE
int  main(int argc, char *argv[])
//
//  Input:   argc = number of command line arguments
//           argv = array of command line arguments
//  Output:  returns error status
//  Purpose: processes command line arguments.
//
//  Command line for stand-alone operation is: swmm5 f1  f2  f3
//  where f1 = name of input file, f2 = name of report file, and
//  f3 = name of binary output file if saved (or blank if not saved).
//
{
    char *inputFile;
    char *reportFile;
    char *binaryFile;
    char blank[] = "";
    time_t start;
    double runTime;
    
    // --- initialize flags
    IsOpenFlag = FALSE;
    IsStartedFlag = FALSE;
    SaveResultsFlag = TRUE;
    
    // --- check for proper number of command line arguments
    start = time(0);
    if (argc < 3) writecon(FMT01);
    else
    {
        // --- extract file names from command line arguments
        inputFile = argv[1];
        reportFile = argv[2];
        if (argc > 3) binaryFile = argv[3];
        else          binaryFile = blank;
        writecon(FMT02);
        
        // --- run SWMM
        swmm_run(inputFile, reportFile, binaryFile);
        
        // Display closing status on console
        runTime = difftime(time(0), start);
        sprintf(project->Msg, "\n\n... EPA-SWMM completed in %.2f seconds.", runTime);
        writecon(project->Msg);
        if      ( project->ErrorCode   ) writecon(FMT03);
        else if ( project->WarningCode ) writecon(FMT04);
        else                    writecon(FMT05);
    }
    
    // --- Use the code below if you need to keep the console window visible
    /*
     writecon("    Press Enter to continue...");
     getchar();
     */
    
    return 0;
}                                      /* End of main */
#endif

//=============================================================================

int DLLEXPORT  swmm_run(Project* project, char* f1, char* f2, char* f3)
//
//  Input:   f1 = name of input file
//           f2 = name of report file
//           f3 = name of binary output file
//  Output:  returns error code
//  Purpose: runs a SWMM simulation.
//
{
    long newHour, oldHour = 0;
    long theDay, theHour;
    DateTime elapsedTime = 0.0;
    
    // --- open the files & read input data
    project->ErrorCode = 0;
    swmm_open(project, f1, f2, f3);
    
    // --- run the simulation if input data OK
    if ( !project->ErrorCode )
    {
        // --- initialize values
        swmm_start(project, TRUE);
        
        // --- execute each time step until elapsed time is re-set to 0
        if ( !project->ErrorCode )
        {
            writecon("\n o  Simulating day: 0     hour:  0");
            do
            {
                swmm_step(project, &elapsedTime);
                newHour = (long)(elapsedTime * 24.0);
                if ( newHour > oldHour )
                {
                    theDay = (long)elapsedTime;
                    theHour = (long)((elapsedTime - floor(elapsedTime)) * 24.0);
                    writecon("\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
                    sprintf(project->Msg, "%-5d hour: %-2d", theDay, theHour);
                    writecon(project->Msg);
                    oldHour = newHour;
                }
            } while ( elapsedTime > 0.0 && !project->ErrorCode );
            writecon("\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
                     "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
            writecon("Simulation complete           ");
        }
        
        // --- clean up
        swmm_end(project);
    }
    
    // --- report results
    if ( project->Fout.mode == SCRATCH_FILE ) swmm_report(project);
    
    // --- close the system
    swmm_close(project);
    return project->ErrorCode;
}

//=============================================================================

int DLLEXPORT swmm_open(Project* project, char* f1, char* f2, char* f3)
//
//  Input:   f1 = name of input file
//           f2 = name of report file
//           f3 = name of binary output file
//  Output:  returns error code
//  Purpose: opens a SWMM project.
//
{
#ifdef DLL
    // _fpreset();
#endif
    
#ifdef WINDOWS
    // --- begin exception handling here
    __try
#endif
    {
        // --- initialize error & warning codes
        datetime_setDateFormat(M_D_Y);
        project->ErrorCode = 0;
        project->WarningCode = 0;
        IsOpenFlag = FALSE;
        IsStartedFlag = FALSE;
        ExceptionCount = 0;
        
        // --- open a SWMM project
        project_open(f1, f2, f3);
        if ( project->ErrorCode ) return project->ErrorCode;
        IsOpenFlag = TRUE;
        report_writeLogo(project);
        writecon(FMT06);
        
        // --- retrieve project data from input file
        project_readInput(project);
        if ( project->ErrorCode ) return project->ErrorCode;
        
        // --- write project title to report file & validate data
        report_writeTitle(project);
        project_validate(project);
        report_writeOptions(project);
        
        // --- write input summary to report file if requested
        if ( project->RptFlags.input )
            inputrpt_writeInput(project);
    }
    
#ifdef WINDOWS
    // --- end of try loop; handle exception here
    __except(xfilter(GetExceptionCode(), 0.0, 0))
    {
        project->ErrorCode = ERR_SYSTEM;
    }
#endif
    return project->ErrorCode;
}

//=============================================================================

int DLLEXPORT swmm_start(Project* project, int saveResults)
//
//  Input:   saveResults = TRUE if simulation results saved to binary file
//  Output:  returns an error code
//  Purpose: starts a SWMM simulation.
//
{
    // --- check that a project is open & no run started
    if ( project->ErrorCode ) return project->ErrorCode;
    if ( !IsOpenFlag || IsStartedFlag )
    {
        report_writeErrorMsg(project, ERR_NOT_OPEN, "");
        return project->ErrorCode;
    }
    ExceptionCount = 0;
    
#ifdef WINDOWS
    // --- begin exception handling loop here
    __try
#endif
    {
        
        // --- initialize runoff, routing & reporting time (in milliseconds)
        project->NewRunoffTime = 0.0;
        project->NewRoutingTime = 0.0;
        project->ReportTime =   (double)(1000 * project->ReportStep);
        project->StepCount = 0;
        project->NonConvergeCount = 0;
        IsStartedFlag = TRUE;
        
        // --- initialize global continuity errors
        project->RunoffError = 0.0;
        project->GwaterError = 0.0;
        project->FlowError = 0.0;
        project->QualError = 0.0;
        
        // --- open rainfall processor (creates/opens a rainfall
        //     interface file and generates any RDII flows)
        if ( !project->IgnoreRainfall ) rain_open(project);
        if ( project->ErrorCode ) return project->ErrorCode;
        
        // --- initialize state of each major system component
        project_init(project);
        
        // --- see if runoff & routing needs to be computed
        if ( project->Nobjects[SUBCATCH] > 0 ) DoRunoff = TRUE;
        else DoRunoff = FALSE;
        if ( project->Nobjects[NODE] > 0 && !project->IgnoreRouting ) DoRouting = TRUE;
        else DoRouting = FALSE;
        
        // --- open all computing systems (order is important!)
        output_open(project);
        if ( DoRunoff ) runoff_open(project);
        if ( DoRouting ) routing_open(project);
        if ( !hotstart_open(project) ) return project->ErrorCode;
        
        // --- initialize flow and quality routing
        if ( DoRouting )
        {
            flowrout_init(project, project->RouteModel);
            qualrout_init(project);
        }
        
        // --- initialize mass balance and statistics processors
        massbal_open(project);
        stats_open(project);
        
        // --- write Control Actions heading to report file
        if ( project->RptFlags.controls ) report_writeControlActionsHeading(project);
    }
    
#ifdef WINDOWS
    // --- end of try loop; handle exception here
    __except(xfilter(GetExceptionCode(), 0.0, 0))
    {
        project->ErrorCode = ERR_SYSTEM;
    }
#endif
    
    // --- save saveResults flag to global variable
    SaveResultsFlag = saveResults;
    return project->ErrorCode;
}
//=============================================================================

int DLLEXPORT swmm_step(Project* project, DateTime* elapsedTime)
//
//  Input:   elapsedTime = current elapsed time in decimal days

//  Output:  updated value of elapsedTime,
//           returns error code
//  Purpose: advances the simulation by one routing time step.
//
{
    // --- check that simulation can proceed
    if ( project->ErrorCode )
        return project->ErrorCode;
    
    if ( !IsOpenFlag || !IsStartedFlag  )
    {
        report_writeErrorMsg(project, ERR_NOT_OPEN, "");
        return project->ErrorCode;
    }
    
#ifdef WINDOWS
    // --- begin exception handling loop here
    __try
#endif
    {
        // --- if routing time has not exceeded total duration
        if ( project->NewRoutingTime < project->TotalDuration )
        {
            // --- route flow & WQ through drainage system
            //     (runoff will be calculated as needed)
            //     (project->NewRoutingTime is updated)
            execRouting(project, *elapsedTime);
        }
        
        // --- save results at next reporting time
        if ( project->NewRoutingTime >= project->ReportTime )
        {
            if ( SaveResultsFlag )
                output_saveResults(project, project->ReportTime);
            
            project->ReportTime = project->ReportTime + (double)(1000 * project->ReportStep);
        }
        
        // --- update elapsed time (days)
        if ( project->NewRoutingTime < project->TotalDuration )
        {
            *elapsedTime = project->NewRoutingTime / MSECperDAY;
        }
        
        // --- otherwise end the simulation
        else *elapsedTime = 0.0;
    }
    
#ifdef WINDOWS
    // --- end of try loop; handle exception here
    __except(xfilter(GetExceptionCode(), *elapsedTime, project->StepCount))
    {
        project->ErrorCode = ERR_SYSTEM;
    }
#endif
    return project->ErrorCode;
}

//=============================================================================

void execRouting(Project* project, DateTime elapsedTime)
//
//  Input:   elapsedTime = current elapsed time in decimal days
//  Output:  none
//  Purpose: routes flow & WQ through drainage system over a single time step.
//
{
    double   nextRoutingTime;          // updated elapsed routing time (msec)
    double   routingStep;              // routing time step (sec)
    
#ifdef WINDOWS
    // --- begin exception handling loop here
    __try
#endif
    {
        // --- determine when next routing time occurs
        project->StepCount++;
        
        if (!DoRouting)
        {
            routingStep = MIN(project->WetStep, project->ReportStep);
        }
        else
        {
            routingStep = routing_getRoutingStep(project, project->RouteModel, project->RouteStep);
        }
        
        if ( routingStep <= 0.0 )
        {
            project->ErrorCode = ERR_TIMESTEP;
            return;
        }
        
        nextRoutingTime = project->NewRoutingTime + 1000.0 * routingStep;
        
        // --- compute runoff until next routing time reached or exceeded
        if (DoRunoff)
        {
            while (project->NewRunoffTime < nextRoutingTime)
            {
                runoff_execute(project);
                
                if (project->ErrorCode)
                {
                    return;
                }
            }
        }
        else
        {
            // --- if no runoff analysis, update climate state (for evaporation)
            climate_setState(project, getDateTime(project, project->NewRoutingTime));
        }
        
        // --- route flows through drainage system over current time step
        if (DoRouting)
        {
            routing_execute(project, project->RouteModel, routingStep);
        }
        else
        {
            project->NewRoutingTime = nextRoutingTime;
        }
    }
    
#ifdef WINDOWS
    // --- end of try loop; handle exception here
    __except(xfilter(GetExceptionCode(), elapsedTime, project->StepCount))
    {
        project->ErrorCode = ERR_SYSTEM;
        return;
    }
#endif
}

//=============================================================================

int DLLEXPORT swmm_end(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: ends a SWMM simulation.
//
{
    // --- check that project opened and run started
    if ( !IsOpenFlag )
    {
        report_writeErrorMsg(project, ERR_NOT_OPEN, "");
        return project->ErrorCode;
    }
    
    if ( IsStartedFlag )
    {
        // --- write ending records to binary output file
        if ( project->Fout.file ) output_end(project);
        
        // --- report mass balance results and system statistics
        if ( !project->ErrorCode )
        {
            massbal_report(project);
            stats_report(project);
        }
        
        // --- close all computing systems
        stats_close(project);
        massbal_close(project);
        if ( !project->IgnoreRainfall ) rain_close(project);
        if ( DoRunoff ) runoff_close(project);
        if ( DoRouting ) routing_close(project, project->RouteModel);
        hotstart_close(project);
        IsStartedFlag = FALSE;
    }
    
    
    
    return project->ErrorCode;
}

//=============================================================================

int DLLEXPORT swmm_report(Project* project)
//
//  Input:   none
//  Output:  returns an error code
//  Purpose: writes simulation results to report file.
//
{
    if ( project->Fout.mode == SCRATCH_FILE ) output_checkFileSize(project);
    if ( project->ErrorCode ) report_writeErrorCode (project);
    else
    {
        writecon(FMT07);
        report_writeReport(project );
    }
    return project->ErrorCode;
}

//=============================================================================

int DLLEXPORT swmm_close(Project* project)
//
//  Input:   none
//  Output:  returns an error code
//  Purpose: closes a SWMM project.
//
{
    if ( project->Fout.file ) output_close();
    if ( IsOpenFlag ) project_close(project);
    report_writeSysTime(project);
    if ( project->Finp.file != NULL ) fclose(project->Finp.file);
    if ( project->Frpt.file != NULL ) fclose(project->Frpt.file);
    if ( project->Fout.file != NULL )
    {
        fclose(project->Fout.file);
        if ( project->Fout.mode == SCRATCH_FILE ) remove(project->Fout.name);
    }
    IsOpenFlag = FALSE;
    IsStartedFlag = FALSE;
    return 0;
}

//=============================================================================

int  DLLEXPORT swmm_getMassBalErr(Project* project, float* runoffErr, float* flowErr,float* qualErr)
//
//  Input:   none
//  Output:  runoffErr = runoff mass balance error (percent)
//           flowErr   = flow routing mass balance error (percent)
//           qualErr   = quality routing mass balance error (percent)
//           returns an error code
//  Purpose: reports a simulation's mass balance errors.
//
{
    *runoffErr = 0.0;
    *flowErr   = 0.0;
    *qualErr   = 0.0;
    
    if ( IsOpenFlag && !IsStartedFlag)
    {
        *runoffErr = (float)project->RunoffError;
        *flowErr   = (float)project->FlowError;
        *qualErr   = (float)project->QualError;
    }
    return 0;
}

//=============================================================================

int  DLLEXPORT swmm_getVersion(Project* project)
//
//  Input:   none
//  Output:  returns SWMM engine version number
//  Purpose: retrieves version number of current SWMM engine which
//           uses a format of xyzzz where x = major version number,
//           y = minor version number, and zzz = build number.
//
{
    return VERSION;
}

double DLLEXPORT swmm_getDateTime(Project* project,char* beginorend)
{
    if (strcomp(beginorend, "begin"))
    {
        return project->StartDateTime;
    }
    else
    {
        return project->EndDateTime;
    }
}

void DLLEXPORT datetime_decodeDateTime(DateTime date, int* y, int* m, int* d, int* h, int* mm, int* s)
{
    datetime_decodeDate(date, y, m, d);
    datetime_decodeTime(date, h, mm, s);
}

SDLLEXPORT char * getErrorMsg(int i)
{
    return error_getMsg(i);
}

int DLLEXPORT getObjectTypeCount(Project* project, int type)
{
    return project->Nobjects[type];
}

//=============================================================================

SDLLEXPORT TNode* STDCALL  getNode(Project* project, int index)
{
    return &project->Node[index];
}

SDLLEXPORT TNode* STDCALL  getNodeById(Project* project, char* id)
{
    int index = project_findObject(project, NODE, id);
    return &project->Node[index];
}

void DLLEXPORT setNode(Project* project, TNode* node, char* propertyName)
{
    int index = project_findObject(project, NODE, node->ID);
    TNode* n1 = &project->Node[index];
    
    if (strcomp(propertyName, "invertElev"))
    {
        n1->invertElev = node->invertElev;
    }
    else if (strcomp(propertyName, "crownElev"))
    {
        n1->crownElev = node->crownElev;
    }
    else if (strcomp(propertyName, "initDepth"))
    {
        n1->initDepth = node->initDepth;
    }
    else if (strcomp(propertyName, "surDepth"))
    {
        n1->surDepth = node->surDepth;
    }
    else if (strcomp(propertyName, "newDepth"))
    {
        //to be applied later
        addNodeDepth(index, node->newDepth);
    }
    else if (strcomp(propertyName, "pondedArea"))
    {
        n1->pondedArea = node->pondedArea;
    }
    else if (strcomp(propertyName, "inflow"))
    {
        n1->inflow = node->inflow;
    }
    else if (strcomp(propertyName, "outflow"))
    {
        n1->outflow = node->outflow;
    }
    else if (strcomp(propertyName, "newLatFlow"))
    {
        //to be applied later
        addNodeLateralInflow(index, node->newLatFlow);
    }
    
}

//=============================================================================

SDLLEXPORT TLink* STDCALL getLink(Project* project,int index)
{
    return &project->Link[index];
}

SDLLEXPORT TLink* STDCALL  getLinkById(Project* project,char* id)
{
    int index = project_findObject(project, LINK, id);
    return &project->Link[index];
}

void DLLEXPORT setLink(Project* project, TLink* link, char* propertyName)
{
    
    TLink* l = &project->Link[project_findObject(project, LINK, link->ID)];
    
    if (strcomp(propertyName, "offset1"))
    {
        l->offset1 = link->offset1;
    }
    else if (strcomp(propertyName, "offset2"))
    {
        l->offset2 = link->offset2;
    }
    else if (strcomp(propertyName, "q0"))
    {
        l->q0 = link->q0;
    }
    else if (strcomp(propertyName, "cLossInlet"))
    {
        l->cLossInlet = link->cLossInlet;
    }
    else if (strcomp(propertyName, "cLossOutlet"))
    {
        l->cLossOutlet = link->cLossOutlet;
    }
    else if (strcomp(propertyName, "cLossAvg"))
    {
        l->cLossAvg = link->cLossAvg;
    }
    else if (strcomp(propertyName, "seepRate"))
    {
        l->seepRate = link->seepRate;
    }
    else if (strcomp(propertyName, "newFlow"))
    {
        l->newFlow = link->newFlow;
    }
    
}

//=============================================================================

SDLLEXPORT TSubcatch*  STDCALL getSubcatch(Project* project, int index)
{
    return &project->Subcatch[index];
}

SDLLEXPORT TSubcatch* STDCALL getSubcatchById(Project* project, char* id)
{
    int index = project_findObject(project, SUBCATCH, id);
    return &project->Subcatch[index];
}

void DLLEXPORT setSubcatch(Project* project, TSubcatch* subCatch, char* propertyName)
{
    int index = project_findObject(project, SUBCATCH, subCatch->ID);
    TSubcatch* subcatch = &project->Subcatch[index];
    
    if (strcomp(propertyName, "newRunoff"))
    {
        subcatch->newRunoff = subCatch->newRunoff;
    }
    else if(strcomp(propertyName, "rainfall"))
    {
        addSubcatchRain(index, subcatch->rainfall);
    }
}

//=============================================================================
//   General purpose functions
//=============================================================================

double UCF(Project *project, int u)
//
//  Input:   u = integer code of quantity being converetd
//  Output:  returns a units conversion factor
//  Purpose: computes a conversion factor from SWMM's internal
//           units to user's units
//
{
    if ( u < FLOW ) return Ucf[u][project->UnitSystem];
    else            return Qcf[project->FlowUnits];
}

//=============================================================================

char* sstrncpy(char *dest, const char *src, size_t maxlen)
//
//  Input:   dest = string to be copied to
//           src = string to be copied from
//           maxlen = number of characters to copy
//  Output:  returns a pointer to dest
//  Purpose: safe version of standard strncpy function
//
{
    strncpy(dest, src, maxlen);
    dest[maxlen] = '\0';
    return dest;
}

//=============================================================================

int  strcomp(char *s1, char *s2)
//
//  Input:   s1 = a character string
//           s2 = a character string
//  Output:  returns 1 if s1 is same as s2, 0 otherwise
//  Purpose: does a case insensitive comparison of two strings.
//
{
    int i;
    for (i = 0; UCHAR(s1[i]) == UCHAR(s2[i]); i++)
    {
        if (!s1[i+1] && !s2[i+1]) return(1);
    }
    return(0);
}

//=============================================================================


char* concat(char *s1, char *s2)
//
//  Input:   s1 = a character string
//           s2 = a character string
//  Output:  returns a concatenated string of s1 and s2
//  Purpose: does a case insensitive comparison of two strings.
//
{
    char *result = malloc(strlen(s1) + strlen(s2) + 1);//+1 for the zero-terminator
    //in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

//=============================================================================


char* getTempFileName(char* fname)
//
//  Input:   fname = file name string (with max size of MAXFNAME)
//  Output:  returns pointer to file name
//  Purpose: creates a temporary file name with path prepended to it.
//
{
    // For Windows systems:
#ifdef WINDOWS
    
    char* name = NULL;
    char* dir = NULL;
    
    // --- set dir to user's choice of a temporary directory
    if (strlen(project->TempDir) > 0)
    {
        _mkdir(project->TempDir);
        dir = project->TempDir;
    }
    
    // --- use _tempnam to get a pointer to an unused file name
    name = _tempnam(dir, "swmm");
    if (name == NULL) return NULL;
    
    // --- copy the file name to fname
    if (strlen(name) < MAXFNAME) strncpy(fname, name, MAXFNAME);
    else fname = NULL;
    
    // --- free the pointer returned by _tempnam
    free(name);
    
    // --- return the new contents of fname
    return fname;
    
    // For non-Windows systems:
#else
    
    // --- use system function mkstemp() to create a temporary file name
    strcpy(fname, "swmmXXXXXX");
    mkstemp(fname);
    return fname;
    
#endif
}

//=============================================================================

void getElapsedTime(Project *project, DateTime aDate, int* days, int* hrs, int* mins)
//
//  Input:   aDate = simulation calendar date + time
//  Output:  days, hrs, mins = elapsed days, hours & minutes for aDate
//  Purpose: finds elapsed simulation time for a given calendar date
//
{
    DateTime x;
    int secs;
    x = aDate - project->StartDateTime;
    if ( x <= 0.0 )
    {
        *days = 0;
        *hrs  = 0;
        *mins = 0;
    }
    else
    {
        *days = (int)x;
        datetime_decodeTime(x, hrs, mins, &secs);
    }
}

//=============================================================================

DateTime getDateTime(Project *project, double elapsedMsec)
//
//  Input:   elapsedMsec = elapsed milliseconds
//  Output:  returns date/time value
//  Purpose: finds calendar date/time value for elapsed milliseconds of
//           simulation time.
//
{
    return datetime_addSeconds(project->StartDateTime, (elapsedMsec+1)/1000.0);
}

//=============================================================================

void  writecon(char *s)
//
//  Input:   s = a character string
//  Output:  none
//  Purpose: writes string of characters to the console.
//
{
#ifdef CLE
    fprintf(stdout,s);
    fflush(stdout);
#endif
}

//=============================================================================

#ifdef WINDOWS

int xfilter(int xc, DateTime elapsedTime, long step)
//
//  Input:   xc          = exception code
//           elapsedTime = simulation time when exception occurred (days)
//           step        = step count at time when exception occurred
//  Output:  returns an exception handling code
//  Purpose: exception filtering routine for operating system exceptions
//           under Windows.
//
{
    int  rc;                           // result code
    long hour;                         // current hour of simulation
    char msg[40];                      // exception type text
    char xmsg[120];                    // error message text
    switch (xc)
    {
        case EXCEPTION_ACCESS_VIOLATION:
            sprintf(msg, "\n  Access violation ");
            rc = EXCEPTION_EXECUTE_HANDLER;
            break;
        case EXCEPTION_FLT_DENORMAL_OPERAND:
            sprintf(msg, "\n  Illegal floating point operand ");
            rc = EXCEPTION_CONTINUE_EXECUTION;
            break;
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            sprintf(msg, "\n  Floating point divide by zero ");
            rc = EXCEPTION_CONTINUE_EXECUTION;
            break;
        case EXCEPTION_FLT_INVALID_OPERATION:
            sprintf(msg, "\n  Illegal floating point operation ");
            rc = EXCEPTION_CONTINUE_EXECUTION;
            break;
        case EXCEPTION_FLT_OVERFLOW:
            sprintf(msg, "\n  Floating point overflow ");
            rc = EXCEPTION_CONTINUE_EXECUTION;
            break;
        case EXCEPTION_FLT_STACK_CHECK:
            sprintf(msg, "\n  Floating point stack violation ");
            rc = EXCEPTION_EXECUTE_HANDLER;
            break;
        case EXCEPTION_FLT_UNDERFLOW:
            sprintf(msg, "\n  Floating point underflow ");
            rc = EXCEPTION_CONTINUE_EXECUTION;
            break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            sprintf(msg, "\n  Integer divide by zero ");
            rc = EXCEPTION_CONTINUE_EXECUTION;
            break;
        case EXCEPTION_INT_OVERFLOW:
            sprintf(msg, "\n  Integer overflow ");
            rc = EXCEPTION_CONTINUE_EXECUTION;
            break;
        default:
            sprintf(msg, "\n  Exception %d", xc);
            rc = EXCEPTION_EXECUTE_HANDLER;
    }
    hour = (long)(elapsedTime / 1000.0 / 3600.0);
    sprintf(xmsg, "%s at step %d, hour %d", msg, step, hour);
    if ( rc == EXCEPTION_EXECUTE_HANDLER ||
        ++ExceptionCount >= MAX_EXCEPTIONS )
    {
        strcat(xmsg, " --- execution halted.");
        rc = EXCEPTION_EXECUTE_HANDLER;
    }
    report_writeLine(xmsg);
    return rc;
}

#endif

//=============================================================================
