//-----------------------------------------------------------------------------
//   inputrpt.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/20/14 (Build 5.1.001)
//   Author:   L. Rossman
//
//   Report writing functions for input data summary.
//
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE


#include <string.h>
#include <time.h>
#include "headers.h"
#include "lid.h"

#define WRITE(x) (report_writeLine((x)))

//=============================================================================

void inputrpt_writeInput()
//
//  Input:   none
//  Output:  none
//  Purpose: writes summary of input data to report file.
//
{
    int m;
    int i, k;
    int lidCount = 0;
    if ( project->ErrorCode ) return;

    WRITE("");
    WRITE("*************");
    WRITE("Element Count");
    WRITE("*************");
    fprintf(project->Frpt.file, "\n  Number of rain gages ...... %d", project->Nobjects[GAGE]);
    fprintf(project->Frpt.file, "\n  Number of subcatchments ... %d", project->Nobjects[SUBCATCH]);
    fprintf(project->Frpt.file, "\n  Number of nodes ........... %d", project->Nobjects[NODE]);
    fprintf(project->Frpt.file, "\n  Number of links ........... %d", project->Nobjects[LINK]);
    fprintf(project->Frpt.file, "\n  Number of pollutants ...... %d", project->Nobjects[POLLUT]);
    fprintf(project->Frpt.file, "\n  Number of land uses ....... %d", project->Nobjects[LANDUSE]);

    if ( project->Nobjects[POLLUT] > 0 )
    {
        WRITE("");
        WRITE("");
        WRITE("*****************");
        WRITE("Pollutant Summary");
        WRITE("*****************");
        fprintf(project->Frpt.file,
    "\n                               Ppt.      GW         Kdecay");
        fprintf(project->Frpt.file,
    "\n  Name                 Units   Concen.   Concen.    1/days    CoPollutant");
        fprintf(project->Frpt.file,
    "\n  -----------------------------------------------------------------------");
        for (i = 0; i < project->Nobjects[POLLUT]; i++)
        {
            fprintf(project->Frpt.file, "\n  %-20s %5s%10.2f%10.2f%10.2f", project->Pollut[i].ID,
                QualUnitsWords[project->Pollut[i].units], Pollut[i].pptConcen,
                project->Pollut[i].gwConcen, Pollut[i].kDecay*SECperDAY);
            if ( project->Pollut[i].coPollut >= 0 )
                fprintf(project->Frpt.file, "    %-s  (%.2f)",
                    project->Pollut[Pollut[i].coPollut].ID, Pollut[i].coFraction);
        }
    }

    if ( project->Nobjects[LANDUSE] > 0 )
    {
        WRITE("");
        WRITE("");
        WRITE("***************");
        WRITE("project->Landuse Summary");
        WRITE("***************");
        fprintf(project->Frpt.file,
    "\n                         Sweeping   Maximum      Last");
        fprintf(project->Frpt.file,
    "\n  Name                   Interval   Removal     Swept");
        fprintf(project->Frpt.file,
    "\n  ---------------------------------------------------");
        for (i=0; i<project->Nobjects[LANDUSE]; i++)
        {
            fprintf(project->Frpt.file, "\n  %-20s %10.2f%10.2f%10.2f", project->Landuse[i].ID,
                project->Landuse[i].sweepInterval, Landuse[i].sweepRemoval,
                project->Landuse[i].sweepDays0);
        }
    }

    if ( project->Nobjects[GAGE] > 0 )
    {
        WRITE("");
        WRITE("");
        WRITE("****************");
        WRITE("Raingage Summary");
        WRITE("****************");
    fprintf(project->Frpt.file,
"\n                                                      Data       Recording");
    fprintf(project->Frpt.file,
"\n  Name                 Data Source                    Type       Interval ");
    fprintf(project->Frpt.file,
"\n  ------------------------------------------------------------------------");
        for (i = 0; i < project->Nobjects[GAGE]; i++)
        {
            if ( project->Gage[i].tSeries >= 0 )
            {
                fprintf(project->Frpt.file, "\n  %-20s %-30s ",
                    project->Gage[i].ID, project->Tseries[Gage[i].tSeries].ID);
                fprintf(project->Frpt.file, "%-10s %3d min.",
                    RainTypeWords[project->Gage[i].rainType],
                    (project->Gage[i].rainInterval)/60);
            }
            else fprintf(project->Frpt.file, "\n  %-20s %-30s",
                project->Gage[i].ID, Gage[i].fname);
        }
    }

    if ( project->Nobjects[SUBCATCH] > 0 )
    {
        WRITE("");
        WRITE("");
        WRITE("********************");
        WRITE("Subcatchment Summary");
        WRITE("********************");
        fprintf(project->Frpt.file,
"\n  Name                       Area     Width   %%Imperv    %%Slope Rain project->Gage            project->Outlet              ");
        fprintf(project->Frpt.file,
"\n  -----------------------------------------------------------------------------------------------------------");
        for (i = 0; i < project->Nobjects[SUBCATCH]; i++)
        {
            fprintf(project->Frpt.file,"\n  %-20s %10.2f%10.2f%10.2f%10.4f %-20s ",
                project->Subcatch[i].ID, Subcatch[i].area*UCF(LANDAREA),
                project->Subcatch[i].width*UCF(LENGTH),  Subcatch[i].fracImperv*100.0,
                project->Subcatch[i].slope*100.0, project->Gage[Subcatch[i].gage].ID);
            if ( project->Subcatch[i].outNode >= 0 )
            {
                fprintf(project->Frpt.file, "%-20s", project->Node[project->Subcatch[i].outNode].ID);
            }
            else if ( project->Subcatch[i].outSubcatch >= 0 )
            {
                fprintf(project->Frpt.file, "%-20s", project->Subcatch[Subcatch[i].outSubcatch].ID);
            }
            if ( project->Subcatch[i].lidArea ) lidCount++;
        }
    }
    if ( lidCount > 0 ) lid_writeSummary();

    if ( project->Nobjects[NODE] > 0 )
    {
        WRITE("");
        WRITE("");
        WRITE("************");
        WRITE("project->Node Summary");
        WRITE("************");
        fprintf(project->Frpt.file,
"\n                                           Invert      Max.    Ponded    External");
        fprintf(project->Frpt.file,
"\n  Name                 Type                 Elev.     Depth      Area    Inflow  ");
        fprintf(project->Frpt.file,
"\n  -------------------------------------------------------------------------------");
        for (i = 0; i < project->Nobjects[NODE]; i++)
        {
            fprintf(project->Frpt.file, "\n  %-20s %-16s%10.2f%10.2f%10.1f", project->Node[i].ID,
                NodeTypeWords[project->Node[i].type-JUNCTION],
                project->Node[i].invertElev*UCF(LENGTH),
                project->Node[i].fullDepth*UCF(LENGTH),
                project->Node[i].pondedArea*UCF(LENGTH)*UCF(LENGTH));
            if ( project->Node[i].extInflow || Node[i].dwfInflow || Node[i].rdiiInflow )
            {
                fprintf(project->Frpt.file, "    Yes");
            }
        }
    }

    if ( project->Nobjects[LINK] > 0 )
    {
        WRITE("");
        WRITE("");
        WRITE("************");
        WRITE("project->Link Summary");
        WRITE("************");
        fprintf(project->Frpt.file,
"\n  Name             From project->Node        To Node          Type            Length    %%Slope Roughness");
        fprintf(project->Frpt.file,
"\n  ---------------------------------------------------------------------------------------------");
        for (i = 0; i < project->Nobjects[LINK]; i++)
        {
            // --- list end nodes in their original orientation
            if ( project->Link[i].direction == 1 )
                fprintf(project->Frpt.file, "\n  %-16s %-16s %-16s ",
                    project->Link[i].ID, project->Node[Link[i].node1].ID, Node[Link[i].node2].ID);
            else
                fprintf(project->Frpt.file, "\n  %-16s %-16s %-16s ",
                    project->Link[i].ID, project->Node[Link[i].node2].ID, Node[Link[i].node1].ID);

            // --- list link type
            if ( project->Link[i].type == PUMP )
            {
                k = project->Link[i].subIndex;
                fprintf(project->Frpt.file, "%-5s PUMP  ",
                    PumpTypeWords[project->Pump[k].type]);
            }
            else fprintf(project->Frpt.file, "%-12s",
                LinkTypeWords[project->Link[i].type-CONDUIT]);

            // --- list length, slope and roughness for conduit links
            if (project->Link[i].type == CONDUIT)
            {
                k = project->Link[i].subIndex;
                fprintf(project->Frpt.file, "%10.1f%10.4f%10.4f",
                    project->Conduit[k].length*UCF(LENGTH),
                    project->Conduit[k].slope*100.0*project->Link[i].direction,
                    project->Conduit[k].roughness);
            }
        }

        WRITE("");
        WRITE("");
        WRITE("*********************");
        WRITE("Cross Section Summary");
        WRITE("*********************");
        fprintf(project->Frpt.file,
"\n                                        Full     Full     Hyd.     Max.   No. of     Full");
        fprintf(project->Frpt.file,    
"\n  project->Conduit          project->Shape               Depth     Area     Rad.    Width  Barrels     Flow");
        fprintf(project->Frpt.file,
"\n  ---------------------------------------------------------------------------------------");
        for (i = 0; i < project->Nobjects[LINK]; i++)
        {
            if (project->Link[i].type == CONDUIT)
            {
                k = project->Link[i].subIndex;
                fprintf(project->Frpt.file, "\n  %-16s ", project->Link[i].ID);
                if ( project->Link[i].xsect.type == CUSTOM )
                    fprintf(project->Frpt.file, "%-16s ", project->Curve[project->Link[i].xsect.transect].ID);
                else if ( project->Link[i].xsect.type == IRREGULAR )
                    fprintf(project->Frpt.file, "%-16s ",
                    project->Transect[project->Link[i].xsect.transect].ID);
                else fprintf(project->Frpt.file, "%-16s ",
                    XsectTypeWords[project->Link[i].xsect.type]);
                fprintf(project->Frpt.file, "%8.2f %8.2f %8.2f %8.2f      %3d %8.2f",
                    project->Link[i].xsect.yFull*UCF(LENGTH),
                    project->Link[i].xsect.aFull*UCF(LENGTH)*UCF(LENGTH),
                    project->Link[i].xsect.rFull*UCF(LENGTH),
                    project->Link[i].xsect.wMax*UCF(LENGTH),
                    project->Conduit[k].barrels,
                    project->Link[i].qFull*UCF(FLOW));
            }
        }
    }

    if (project->Nobjects[SHAPE] > 0)
    {
        WRITE("");
        WRITE("");
        WRITE("*************");
        WRITE("project->Shape Summary");
        WRITE("*************");
        for (i = 0; i < project->Nobjects[SHAPE]; i++)
        {
            k = project->Shape[i].curve;
            fprintf(project->Frpt.file, "\n\n  project->Shape %s", project->Curve[k].ID);
            fprintf(project->Frpt.file, "\n  Area:  ");
            for ( m = 1; m < N_SHAPE_TBL; m++)
            {
                 if ( m % 5 == 1 ) fprintf(project->Frpt.file,"\n          ");
                 fprintf(project->Frpt.file, "%10.4f ", project->Shape[i].areaTbl[m]);
            }
            fprintf(project->Frpt.file, "\n  Hrad:  ");
            for ( m = 1; m < N_SHAPE_TBL; m++)
            {
                 if ( m % 5 == 1 ) fprintf(project->Frpt.file,"\n          ");
                 fprintf(project->Frpt.file, "%10.4f ", project->Shape[i].hradTbl[m]);
            }
            fprintf(project->Frpt.file, "\n  Width: ");
            for ( m = 1; m < N_SHAPE_TBL; m++)
            {
                 if ( m % 5 == 1 ) fprintf(project->Frpt.file,"\n          ");
                 fprintf(project->Frpt.file, "%10.4f ", project->Shape[i].widthTbl[m]);
            }
        }
    }
    WRITE("");

    if (project->Nobjects[TRANSECT] > 0)
    {
        WRITE("");
        WRITE("");
        WRITE("****************");
        WRITE("project->Transect Summary");
        WRITE("****************");
        for (i = 0; i < project->Nobjects[TRANSECT]; i++)
        {
            fprintf(project->Frpt.file, "\n\n  project->Transect %s", Transect[i].ID);
            fprintf(project->Frpt.file, "\n  Area:  ");
            for ( m = 1; m < N_TRANSECT_TBL; m++)
            {
                 if ( m % 5 == 1 ) fprintf(project->Frpt.file,"\n          ");
                 fprintf(project->Frpt.file, "%10.4f ", project->Transect[i].areaTbl[m]);
            }
            fprintf(project->Frpt.file, "\n  Hrad:  ");
            for ( m = 1; m < N_TRANSECT_TBL; m++)
            {
                 if ( m % 5 == 1 ) fprintf(project->Frpt.file,"\n          ");
                 fprintf(project->Frpt.file, "%10.4f ", project->Transect[i].hradTbl[m]);
            }
            fprintf(project->Frpt.file, "\n  Width: ");
            for ( m = 1; m < N_TRANSECT_TBL; m++)
            {
                 if ( m % 5 == 1 ) fprintf(project->Frpt.file,"\n          ");
                 fprintf(project->Frpt.file, "%10.4f ", project->Transect[i].widthTbl[m]);
            }
        }
    }
    WRITE("");
}
