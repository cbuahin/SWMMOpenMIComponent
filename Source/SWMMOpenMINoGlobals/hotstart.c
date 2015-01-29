//-----------------------------------------------------------------------------
//   hotstart.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/20/14  (Build 5.1.001)
//             03/28/14  (Build 5.1.002)
//             04/23/14  (Build 5.1.005)
//   Author:   L. Rossman (EPA)
//
//   Hot Start file functions.
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "headers.h"

//-----------------------------------------------------------------------------
//  Local Variables
//-----------------------------------------------------------------------------
static int fileVersion;

//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)
//-----------------------------------------------------------------------------
// hotstart_open                          (called by swmm_start in swmm5.c)
// hotstart_close                         (called by swmm_end in swmm5.c)      //(5.1.005)

//-----------------------------------------------------------------------------
// Function declarations
//-----------------------------------------------------------------------------
static int  openHotstartFile1(void); 
static int  openHotstartFile2(void);       
static void readRunoff(void);
static void saveRunoff(void);
static void readRouting(void);
static void saveRouting(void);
static int  readFloat(float *x, FILE* f);
static int  readDouble(double* x, FILE* f);

//=============================================================================

int hotstart_open()
{
    // --- open hot start files
    if ( !openHotstartFile1() ) return FALSE;       //input hot start file
    if ( !openHotstartFile2() ) return FALSE;       //output hot start file

    ////  Following lines removed. ////                                            //(5.1.005)
    //if ( project->Fhotstart1.file )
    //{
    //    readRunoff();
    //    readRouting();
    //    fclose(project->Fhotstart1.file);
    //}

    return TRUE;
}

//=============================================================================

void hotstart_close()
{
    if ( project->Fhotstart2.file )
    {
        saveRunoff();
        saveRouting();
        fclose(project->Fhotstart2.file);
    }
}

//=============================================================================

int openHotstartFile1()
//
//  Input:   none
//  Output:  none
//  Purpose: opens a previously saved routing hotstart file.
//
{
    int   nSubcatch;
    int   nNodes;
    int   nLinks;
    int   nPollut;
    int   nLandUses;
    int   flowUnits;
    char  fStamp[]     = "SWMM5-HOTSTART";
    char  fileStamp[]  = "SWMM5-HOTSTART";
    char  fStampx[]    = "SWMM5-HOTSTARTx";
    char  fileStamp2[] = "SWMM5-HOTSTART2";
    char  fileStamp3[] = "SWMM5-HOTSTART3";

    // --- try to open the file
    if ( project->Fhotstart1.mode != USE_FILE ) return TRUE;
    if ( (project->Fhotstart1.file = fopen(Fhotstart1.name, "r+b")) == NULL)
    {
        report_writeErrorMsg(ERR_HOTSTART_FILE_OPEN, project->Fhotstart1.name);
        return FALSE;
    }

    // --- check that file contains proper header records
    fread(fStampx, sizeof(char), strlen(fileStamp2), project->Fhotstart1.file);
    if      ( strcmp(fStampx, fileStamp3) == 0 ) fileVersion = 3;
    else if ( strcmp(fStampx, fileStamp2) == 0 ) fileVersion = 2;
    else
    {
        rewind(project->Fhotstart1.file);
        fread(fStamp, sizeof(char), strlen(fileStamp), project->Fhotstart1.file);
        if ( strcmp(fStamp, fileStamp) != 0 )
        {
            report_writeErrorMsg(ERR_HOTSTART_FILE_FORMAT, "");
            return FALSE;
        }
        fileVersion = 1;
    }

    nSubcatch = -1;
    nNodes = -1;
    nLinks = -1;
    nPollut = -1;
    nLandUses = -1;
    flowUnits = -1;
    if ( fileVersion >= 2 )                                                    //(5.1.002)
    {    
        fread(&nSubcatch, sizeof(int), 1, project->Fhotstart1.file);
    }
    else nSubcatch = project->Nobjects[SUBCATCH];
    if ( fileVersion == 3 )
    {
        fread(&nLandUses, sizeof(int), 1, project->Fhotstart1.file);
    }
    else nLandUses = project->Nobjects[LANDUSE];
    fread(&nNodes, sizeof(int), 1, project->Fhotstart1.file);
    fread(&nLinks, sizeof(int), 1, project->Fhotstart1.file);
    fread(&nPollut, sizeof(int), 1, project->Fhotstart1.file);
    fread(&flowUnits, sizeof(int), 1, project->Fhotstart1.file);
    if ( nSubcatch != project->Nobjects[SUBCATCH] 
    ||   nLandUses != project->Nobjects[LANDUSE]
    ||   nNodes    != project->Nobjects[NODE]
    ||   nLinks    != project->Nobjects[LINK]
    ||   nPollut   != project->Nobjects[POLLUT]
    ||   flowUnits != project->FlowUnits )
    {
         report_writeErrorMsg(ERR_HOTSTART_FILE_FORMAT, "");
         return FALSE;
    }

    // --- read contents of the file and close it
    if ( fileVersion == 3 ) readRunoff();
    readRouting();
    fclose(project->Fhotstart1.file);
    if ( project->ErrorCode ) return FALSE;
    else return TRUE;
}

//=============================================================================

int openHotstartFile2()
//
//  Input:   none
//  Output:  none
//  Purpose: opens a new routing hotstart file to save results to.
//
{
    int   nSubcatch;
    int   nLandUses;
    int   nNodes;
    int   nLinks;
    int   nPollut;
    int   flowUnits;
    char  fileStamp[] = "SWMM5-HOTSTART3";

    // --- try to open file
    if ( project->Fhotstart2.mode != SAVE_FILE ) return TRUE;
    if ( (project->Fhotstart2.file = fopen(Fhotstart2.name, "w+b")) == NULL)
    {
        report_writeErrorMsg(ERR_HOTSTART_FILE_OPEN, project->Fhotstart2.name);
        return FALSE;
    }

    // --- write file stamp & number of objects to file
    nSubcatch = project->Nobjects[SUBCATCH];
    nLandUses = project->Nobjects[LANDUSE];
    nNodes = project->Nobjects[NODE];
    nLinks = project->Nobjects[LINK];
    nPollut = project->Nobjects[POLLUT];
    flowUnits = project->FlowUnits;
    fwrite(fileStamp, sizeof(char), strlen(fileStamp), project->Fhotstart2.file);
    fwrite(&nSubcatch, sizeof(int), 1, project->Fhotstart2.file);
    fwrite(&nLandUses, sizeof(int), 1, project->Fhotstart2.file);
    fwrite(&nNodes, sizeof(int), 1, project->Fhotstart2.file);
    fwrite(&nLinks, sizeof(int), 1, project->Fhotstart2.file);
    fwrite(&nPollut, sizeof(int), 1, project->Fhotstart2.file);
    fwrite(&flowUnits, sizeof(int), 1, project->Fhotstart2.file);
    return TRUE;
}

//=============================================================================

void  saveRouting()
//
//  Input:   none
//  Output:  none
//  Purpose: saves current state of all nodes and links to hotstart file.
//
{
    int   i, j;
    float x[3];

    for (i = 0; i < project->Nobjects[NODE]; i++)
    {
        x[0] = (float)project->Node[i].newDepth;
        x[1] = (float)project->Node[i].newLatFlow;
        fwrite(x, sizeof(float), 2, project->Fhotstart2.file);
        for (j = 0; j < project->Nobjects[POLLUT]; j++)
        {
            x[0] = (float)project->Node[i].newQual[j];
            fwrite(&x[0], sizeof(float), 1, project->Fhotstart2.file);
        }
    }
    for (i = 0; i < project->Nobjects[LINK]; i++)
    {
        x[0] = (float)project->Link[i].newFlow;
        x[1] = (float)project->Link[i].newDepth;
        x[2] = (float)project->Link[i].setting;
        fwrite(x, sizeof(float), 3, project->Fhotstart2.file);
        for (j = 0; j < project->Nobjects[POLLUT]; j++)
        {
            x[0] = (float)project->Link[i].newQual[j];
            fwrite(&x[0], sizeof(float), 1, project->Fhotstart2.file);
        }
    }
}

//=============================================================================

void readRouting()
//
//  Input:   none 
//  Output:  none
//  Purpose: reads initial state of all nodes, links and groundwater objects
//           from hotstart file.
//
{
    int   i, j;
    float x;
    double xgw[4];
    FILE* f = project->Fhotstart1.file;

    // --- for file format 2, assign GW moisture content and lower depth
    if ( fileVersion == 2 )
    {
        // --- flow and available upper zone volume not used
        xgw[2] = 0.0;
        xgw[3] = MISSING;
        for (i = 0; i < project->Nobjects[SUBCATCH]; i++)
        {
            // --- read moisture content and water table elevation as floats
            if ( !readFloat(&x, f) ) return;
            xgw[0] = x;
            if ( !readFloat(&x, f) ) return;
            xgw[1] = x;

            // --- set GW state
            if ( project->Subcatch[i].groundwater != NULL ) gwater_setState(i, xgw);
        }
    }

    // --- read node states
    for (i = 0; i < project->Nobjects[NODE]; i++)
    {
        if ( !readFloat(&x, f) ) return;
        project->Node[i].newDepth = x;
        if ( !readFloat(&x, f) ) return;
        project->Node[i].newLatFlow = x;
        for (j = 0; j < project->Nobjects[POLLUT]; j++)
        {
            if ( !readFloat(&x, f) ) return;
            project->Node[i].newQual[j] = x;
        }

        // --- read in zero here for back compatibility
        if ( fileVersion <= 2 )
        {
            for (j = 0; j < project->Nobjects[POLLUT]; j++)
            {
                if ( !readFloat(&x, f) ) return;
            }
        }
    }

    // --- read link states
    for (i = 0; i < project->Nobjects[LINK]; i++)
    {
        if ( !readFloat(&x, f) ) return;
        project->Link[i].newFlow = x;
        if ( !readFloat(&x, f) ) return;
        project->Link[i].newDepth = x;
        if ( !readFloat(&x, f) ) return;
        project->Link[i].setting = x;
        for (j = 0; j < project->Nobjects[POLLUT]; j++)
        {
            if ( !readFloat(&x, f) ) return;
            project->Link[i].newQual[j] = x;
        }
    }
}

//=============================================================================

void  saveRunoff(void)
//
//  Input:   none
//  Output:  none
//  Purpose: saves current state of all subcatchments to hotstart file.
//
{
    int   i, j, k, sizeX;
    double* x;
    FILE*  f = project->Fhotstart2.file;

    sizeX = MAX(6, project->Nobjects[POLLUT]+1);
    x = (double *) calloc(sizeX, sizeof(double));

    for (i = 0; i < project->Nobjects[SUBCATCH]; i++)
    {
        // Ponded depths for each sub-area & total runoff (4 elements)
        for (j = 0; j < 3; j++) x[j] = project->Subcatch[i].subArea[j].depth;
        x[3] = project->Subcatch[i].newRunoff;
        fwrite(x, sizeof(double), 4, f);

        // Infiltration state (max. of 6 elements)
        for (j=0; j<sizeX; j++) x[j] = 0.0;
        infil_getState(i, project->InfilModel, x);
        fwrite(x, sizeof(double), 6, f);

        // Groundwater state (4 elements)
        if ( project->Subcatch[i].groundwater != NULL )
        {
            gwater_getState(i, x);
            fwrite(x, sizeof(double), 4, f);
        }

        // Snowpack state (5 elements for each of 3 snow surfaces)
        if ( project->Subcatch[i].snowpack != NULL )
        {
            for (j=0; j<3; j++)
            {
                snow_getState(i, j, x);
                fwrite(x, sizeof(double), 5, f);
            }
        }

        // Water quality
        if ( project->Nobjects[POLLUT] > 0 && Nobjects[LANDUSE] > 0 )
        {
            // Runoff quality
            for (j=0; j<project->Nobjects[POLLUT]; j++) x[j] = project->Subcatch[i].newQual[j];
            fwrite(x, sizeof(double), project->Nobjects[POLLUT], f);

            // Ponded quality
            for (j=0; j<project->Nobjects[POLLUT]; j++) x[j] = project->Subcatch[i].pondedQual[j];
            fwrite(x, sizeof(double), project->Nobjects[POLLUT], f);
            
            // Buildup and when streets were last swept
            for (k=0; k<project->Nobjects[LANDUSE]; k++)
            {
                for (j=0; j<project->Nobjects[POLLUT]; j++)
                    x[j] = project->Subcatch[i].landFactor[k].buildup[j];
                fwrite(x, sizeof(double), project->Nobjects[POLLUT], f);
                x[0] = project->Subcatch[i].landFactor[k].lastSwept;
                fwrite(x, sizeof(double), 1, f);
            }
        }
    }
    free(x);
}

//=============================================================================

void  readRunoff()
//
//  Input:   none
//  Output:  none
//  Purpose: reads saved state of all subcatchments from a hot start file.
//
{
    int    i, j, k;
    double x[6];
    FILE*  f = project->Fhotstart1.file;

    for (i = 0; i < project->Nobjects[SUBCATCH]; i++)
    {
        // Ponded depths & runoff (4 elements)
        for (j = 0; j < 3; j++)
        {
            if ( !readDouble(&project->Subcatch[i].subArea[j].depth, f) ) return;
        }
        if ( !readDouble(&project->Subcatch[i].oldRunoff, f) ) return;

        // Infiltration state (max. of 6 elements)
        for (j=0; j<6; j++) if ( !readDouble(&x[j], f) ) return;
        infil_setState(i, project->InfilModel, x);

        // Groundwater state (4 elements)
        if ( project->Subcatch[i].groundwater != NULL )
        {
            for (j=0; j<4; j++) if ( !readDouble(&x[j], f) ) return;
            gwater_setState(i, x);
        }

        // Snowpack state (5 elements for each of 3 snow surfaces)
        if ( project->Subcatch[i].snowpack != NULL )
        {
            for (j=0; j<3; j++) 
            {
                for (k=0; k<5; k++) if ( !readDouble(&x[j], f) ) return;
                snow_setState(i, j, x);
            }
        }

        // Water quality
        if ( project->Nobjects[POLLUT] > 0 && Nobjects[LANDUSE] > 0 )
        {
            // Runoff quality
            for (j=0; j<project->Nobjects[POLLUT]; j++)
                if ( ! readDouble(&project->Subcatch[i].oldQual[j], f) ) return;

            // Ponded quality
            for (j=0; j<project->Nobjects[POLLUT]; j++)
                if ( !readDouble(&project->Subcatch[i].pondedQual[j], f) ) return;
            
            // Buildup and when streets were last swept
            for (k=0; k<project->Nobjects[LANDUSE]; k++)
            {
                for (j=0; j<project->Nobjects[POLLUT]; j++)
                {
                    if ( !readDouble(
                        &project->Subcatch[i].landFactor[k].buildup[j], f) ) return;
                }
                if ( !readDouble(&project->Subcatch[i].landFactor[k].lastSwept, f) )
                    return;
            }
        }
    }
}

//=============================================================================

int  readFloat(float *x, FILE* f)
//
//  Input:   none
//  Output:  x  = pointer to a float variable
//  Purpose: reads a floating point value from a hot start file
//
{
    // --- read a value from the file
    fread(x, sizeof(float), 1, f);

    // --- test if the value is NaN (not a number)
    if ( *(x) != *(x) )
    {
        report_writeErrorMsg(ERR_HOTSTART_FILE_READ, "");
        *(x) = 0.0;
        return FALSE;
    }
    return TRUE;
}

//=============================================================================

int  readDouble(double* x, FILE* f)
//
//  Input:   none
//  Output:  x  = pointer to a double variable
//  Purpose: reads a floating point value from a hot start file
//
{
    // --- read a value from the file
    if ( feof(f) )
    {    
        *(x) = 0.0;
        report_writeErrorMsg(ERR_HOTSTART_FILE_READ, "");
        return FALSE;
    }
    fread(x, sizeof(double), 1, f);

    // --- test if the value is NaN (not a number)
    if ( *(x) != *(x) )
    {
        *(x) = 0.0;
        return FALSE;
    }
    return TRUE;
}
