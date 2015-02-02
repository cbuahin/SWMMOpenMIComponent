//-----------------------------------------------------------------------------
//   statsrpt.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/20/14 (Build 5.1.001)
//   Author:   L. Rossman
//
//   Report writing functions for summary statistics.
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE



//#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "headers.h"
#include "lid.h"

//-----------------------------------------------------------------------------
//  Imported variables
//-----------------------------------------------------------------------------
//extern TSubcatchStats* SubcatchStats;          // defined in STATS.C
//extern TNodeStats*     NodeStats;
//extern TLinkStats*     LinkStats;
//extern TStorageStats*  project->StorageStats;
//extern TOutfallStats*  OutfallStats;
//extern TPumpStats*     PumpStats;
//extern double          MaxOutfallFlow;
//extern double          MaxRunoffFlow;
//extern double*         NodeInflow;             // defined in MASSBAL.C
//extern double*         NodeOutflow;            // defined in massbal.c

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------
void    writeSubcatchRunoff(Project* project);
void    writeSubcatchLoads(Project* project);
void    writeNodeDepths(Project* project);
void    writeNodeFlows(Project* project);
void    writeNodeSurcharge(Project* project);
void    writeNodeFlooding(Project* project);
void    writeStorageVolumes(Project* project);
void    writeOutfallLoads(Project* project);
void    writeLinkFlows(Project* project);
void    writeFlowClass(Project* project);
void    writeLinkSurcharge(Project* project);
void    writePumpFlows(Project* project);
void    writeLinkLoads(Project* project);

#define WRITE(x, y) (report_writeLine((x),(y)))

static char   FlowFmt[6];
static double Vcf;

//=============================================================================

void statsrpt_writeReport(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: reports simulation summary statistics.
//
{
    // --- set number of decimal places for reporting flow values
	if (project->FlowUnits == MGD || project->FlowUnits == CMS) strcpy(FlowFmt, "%9.3f");
    else strcpy(FlowFmt, "%9.2f");

    // --- volume conversion factor from ft3 to Mgal or Mliters
	if (project->UnitSystem == US) Vcf = 7.48 / 1.0e6;
    else                  Vcf = 28.317 / 1.0e6;

    // --- report summary results for subcatchment runoff 
	if (project->Nobjects[SUBCATCH] > 0)
    {
		if (!project->IgnoreRainfall ||
			(project->Nobjects[SNOWMELT] > 0 && !project->IgnoreSnowmelt) ||
			(project->Nobjects[AQUIFER] > 0 && !project->IgnoreGwater))
        {
            writeSubcatchRunoff(project);
            lid_writeWaterBalance(project);
			if (project->Nobjects[POLLUT] > 0 && !project->IgnoreQuality) writeSubcatchLoads(project);
        }
    }

    // --- report summary results for flow routing
	if (project->Nobjects[LINK] > 0 && !project->IgnoreRouting)
    {
        writeNodeDepths(project);
        writeNodeFlows(project);
        writeNodeSurcharge(project);
        writeNodeFlooding(project);
        writeStorageVolumes(project);
        writeOutfallLoads(project);
        writeLinkFlows(project);
        writeFlowClass(project);
        writeLinkSurcharge(project);
        writePumpFlows(project);
		if (project->Nobjects[POLLUT] > 0 && !project->IgnoreQuality) writeLinkLoads(project);
    }
}

//=============================================================================

void writeSubcatchRunoff(Project* project)
{
    int    j;
    double a, x, r;

	if (project->Nobjects[SUBCATCH] == 0) return;
    WRITE(project,"");
    WRITE(project,"***************************");
    WRITE(project,"Subcatchment Runoff Summary");
    WRITE(project,"***************************");
    WRITE(project,"");
	fprintf(project->Frpt.file,

"\n  --------------------------------------------------------------------------------------------------------"
"\n                            Total      Total      Total      Total      Total       Total     Peak  Runoff"
"\n                           Precip      Runon       Evap      Infil     Runoff      Runoff   Runoff   Coeff");
	if (project->UnitSystem == US) fprintf(project->Frpt.file,
"\n  Subcatchment                 in         in         in         in         in    %8s      %3s",
VolUnitsWords[project->UnitSystem], FlowUnitWords[project->FlowUnits]);
	else fprintf(project->Frpt.file,
"\n  Subcatchment                 mm         mm         mm         mm         mm    %8s      %3s",
VolUnitsWords[project->UnitSystem], FlowUnitWords[project->FlowUnits]);
	fprintf(project->Frpt.file,
"\n  --------------------------------------------------------------------------------------------------------");

	for (j = 0; j < project->Nobjects[SUBCATCH]; j++)
    {
		a = project->Subcatch[j].area;
        if ( a == 0.0 ) continue;
		fprintf(project->Frpt.file, "\n  %-20s", project->Subcatch[j].ID);
		x = project->SubcatchStats[j].precip * UCF(project,RAINDEPTH);
		fprintf(project->Frpt.file, " %10.2f", x / a);
		x = project->SubcatchStats[j].runon * UCF(project,RAINDEPTH);
		fprintf(project->Frpt.file, " %10.2f", x / a);
		x = project->SubcatchStats[j].evap * UCF(project,RAINDEPTH);
		fprintf(project->Frpt.file, " %10.2f", x / a);
		x = project->SubcatchStats[j].infil * UCF(project,RAINDEPTH);
		fprintf(project->Frpt.file, " %10.2f", x / a);
		x = project->SubcatchStats[j].runoff * UCF(project,RAINDEPTH);
		fprintf(project->Frpt.file, " %10.2f", x / a);
		x = project->SubcatchStats[j].runoff * Vcf;
		fprintf(project->Frpt.file, "%12.2f", x);
		x = project->SubcatchStats[j].maxFlow * UCF(project,FLOW);
		fprintf(project->Frpt.file, " %8.2f", x);
		r = project->SubcatchStats[j].precip + project->SubcatchStats[j].runon;
		if (r > 0.0) r = project->SubcatchStats[j].runoff / r;
		fprintf(project->Frpt.file, "%8.3f", r);
    }
    WRITE(project,"");
}

//=============================================================================

void writeSubcatchLoads(Project* project)
{
    int i, j, p;
    double x;
    double* totals; 
    char  units[15];
    char  subcatchLine[] = "--------------------";
    char  pollutLine[]   = "--------------";

    // --- create an array to hold total loads for each pollutant
	totals = (double *)calloc(project->Nobjects[POLLUT], sizeof(double));
    if ( totals )
    {
        // --- print the table headings 
        WRITE(project,"");
        WRITE(project,"****************************");
        WRITE(project,"Subcatchment Washoff Summary");
        WRITE(project,"****************************");
        WRITE(project,"");
		fprintf(project->Frpt.file, "\n  %s", subcatchLine);
		for (p = 0; p < project->Nobjects[POLLUT]; p++) fprintf(project->Frpt.file, "%s", pollutLine);
		fprintf(project->Frpt.file, "\n                      ");
		for (p = 0; p < project->Nobjects[POLLUT]; p++) fprintf(project->Frpt.file, "%14s", project->Pollut[p].ID);
		fprintf(project->Frpt.file, "\n  Subcatchment        ");
		for (p = 0; p < project->Nobjects[POLLUT]; p++)
        {
			i = project->UnitSystem;
			if (project->Pollut[p].units == COUNT) i = 2;
            strcpy(units, LoadUnitsWords[i]);
			fprintf(project->Frpt.file, "%14s", units);
            totals[p] = 0.0;
        }
		fprintf(project->Frpt.file, "\n  %s", subcatchLine);
		for (p = 0; p < project->Nobjects[POLLUT]; p++) fprintf(project->Frpt.file, "%s", pollutLine);

        // --- print the pollutant loadings from each subcatchment
		for (j = 0; j < project->Nobjects[SUBCATCH]; j++)
        {
			fprintf(project->Frpt.file, "\n  %-20s", project->Subcatch[j].ID);
			for (p = 0; p <project->Nobjects[POLLUT]; p++)
            {
				x = project->Subcatch[j].totalLoad[p];
                totals[p] += x;
				if (project->Pollut[p].units == COUNT) x = LOG10(x);
				fprintf(project->Frpt.file, "%14.3f", x);
            }
        }

        // --- print the total loading of each pollutant
		fprintf(project->Frpt.file, "\n  %s", subcatchLine);
		for (p = 0; p < project->Nobjects[POLLUT]; p++) fprintf(project->Frpt.file, "%s", pollutLine);
		fprintf(project->Frpt.file, "\n  System              ");
		for (p = 0; p <project->Nobjects[POLLUT]; p++)
        {
            x = totals[p];
			if (project->Pollut[p].units == COUNT) x = LOG10(x);
			fprintf(project->Frpt.file, "%14.3f", x);
        }
        free(totals);
        WRITE(project,"");
    }
}

//=============================================================================

void writeNodeDepths(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: writes simulation statistics for nodes to report file.
//
{
    int j, days, hrs, mins;
	if (project->Nobjects[LINK] == 0) return;

    WRITE(project,"");
    WRITE(project,"******************");
    WRITE(project,"Node Depth Summary");
    WRITE(project,"******************");
    WRITE(project,"");

	fprintf(project->Frpt.file,
"\n  ---------------------------------------------------------------------"
"\n                                 Average  Maximum  Maximum  Time of Max"
"\n                                   Depth    Depth      HGL   Occurrence");
	if (project->UnitSystem == US) fprintf(project->Frpt.file,
"\n  Node                 Type         Feet     Feet     Feet  days hr:min");
	else fprintf(project->Frpt.file,
"\n  Node                 Type       Meters   Meters   Meters  days hr:min");
	fprintf(project->Frpt.file,
"\n  ---------------------------------------------------------------------");

	for (j = 0; j < project->Nobjects[NODE]; j++)
    {
		fprintf(project->Frpt.file, "\n  %-20s", project->Node[j].ID);
		fprintf(project->Frpt.file, " %-9s ", NodeTypeWords[project->Node[j].type]);
		getElapsedTime(project,project->NodeStats[j].maxDepthDate, &days, &hrs, &mins);
		fprintf(project->Frpt.file, "%7.2f  %7.2f  %7.2f  %4d  %02d:%02d",
			project->NodeStats[j].avgDepth / project->StepCount * UCF(project,LENGTH),
			project->NodeStats[j].maxDepth * UCF(project,LENGTH),
			(project->NodeStats[j].maxDepth + project->Node[j].invertElev) * UCF(project, LENGTH),
            days, hrs, mins);
    }
    WRITE(project,"");
}

//=============================================================================

void writeNodeFlows(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: writes flow statistics for nodes to report file.
//
{
    int j;
    int days1, hrs1, mins1;

    WRITE(project,"");
	WRITE(project, "*******************");
	WRITE(project, "Node Inflow Summary");
	WRITE(project, "*******************");
	WRITE(project, "");

	fprintf(project->Frpt.file,
"\n  -------------------------------------------------------------------------------------------------"
"\n                                  Maximum  Maximum                  Lateral       Total        Flow"
"\n                                  Lateral    Total  Time of Max      Inflow      Inflow     Balance"
"\n                                   Inflow   Inflow   Occurrence      Volume      Volume       Error"
"\n  Node                 Type           %3s      %3s  days hr:min    %8s    %8s     Percent",
FlowUnitWords[project->FlowUnits], FlowUnitWords[project->FlowUnits], VolUnitsWords[project->UnitSystem],
VolUnitsWords[project->UnitSystem], VolUnitsWords2[project->UnitSystem]);
	fprintf(project->Frpt.file,
"\n  -------------------------------------------------------------------------------------------------");

	for (j = 0; j < project->Nobjects[NODE]; j++)
    {
		fprintf(project->Frpt.file, "\n  %-20s", project->Node[j].ID);
		fprintf(project->Frpt.file, " %-9s", NodeTypeWords[project->Node[j].type]);
		getElapsedTime(project,project->NodeStats[j].maxInflowDate, &days1, &hrs1, &mins1);
		fprintf(project->Frpt.file, FlowFmt, project->NodeStats[j].maxLatFlow * UCF(project,FLOW));
		fprintf(project->Frpt.file, FlowFmt, project->NodeStats[j].maxInflow * UCF(project, FLOW));
		fprintf(project->Frpt.file, "  %4d  %02d:%02d", days1, hrs1, mins1);
		fprintf(project->Frpt.file, "%12.3g", project->NodeStats[j].totLatFlow * Vcf);
		fprintf(project->Frpt.file, "%12.3g", project->NodeInflow[j] * Vcf);
		if (fabs(project->NodeOutflow[j]) < 1.0)
			fprintf(project->Frpt.file, "%12.3f %s",
			(project->NodeInflow[j] - project->NodeOutflow[j])*Vcf*1.0e6,
			VolUnitsWords2[project->UnitSystem]);
	    else
			fprintf(project->Frpt.file, "%12.3f", (project->NodeInflow[j] - project->NodeOutflow[j]) /
			project->NodeOutflow[j] * 100.);
    }
    WRITE(project,"");
}

//=============================================================================

void writeNodeSurcharge(Project* project)
{
    int    j, n = 0;
    double t, d1, d2;

	WRITE(project, "");
	WRITE(project, "**********************");
	WRITE(project, "Node Surcharge Summary");
	WRITE(project, "**********************");
	WRITE(project, "");

	for (j = 0; j < project->Nobjects[NODE]; j++)
    {
		if (project->Node[j].type == OUTFALL) continue;
		if (project->NodeStats[j].timeSurcharged == 0.0) continue;
		t = MAX(0.01, (project->NodeStats[j].timeSurcharged / 3600.0));
        if ( n == 0 )
        {
            WRITE(project,"Surcharging occurs when water rises above the top of the highest conduit."); 
			fprintf(project->Frpt.file,
"\n  ---------------------------------------------------------------------"
"\n                                               Max. Height   Min. Depth"
"\n                                   Hours       Above Crown    Below Rim");
			if (project->UnitSystem == US) fprintf(project->Frpt.file,
"\n  Node                 Type      Surcharged           Feet         Feet");
			else fprintf(project->Frpt.file,
"\n  Node                 Type      Surcharged         Meters       Meters");
			fprintf(project->Frpt.file,
"\n  ---------------------------------------------------------------------");
            n = 1;
        }
		fprintf(project->Frpt.file, "\n  %-20s", project->Node[j].ID);
		fprintf(project->Frpt.file, " %-9s", NodeTypeWords[project->Node[j].type]);
		d1 = project->NodeStats[j].maxDepth + project->Node[j].invertElev - project->Node[j].crownElev;
        if ( d1 < 0.0 ) d1 = 0.0;
		d2 = project->Node[j].fullDepth - project->NodeStats[j].maxDepth;
        if ( d2 < 0.0 ) d2 = 0.0;
		fprintf(project->Frpt.file, "  %9.2f      %9.3f    %9.3f",
			t, d1*UCF(project, LENGTH), d2*UCF(project, LENGTH));
    }
	if (n == 0) WRITE(project, "No nodes were surcharged.");
	WRITE(project, "");
}

//=============================================================================

void writeNodeFlooding(Project* project)
{
    int    j, n = 0;
    int    days, hrs, mins;
    double t;

	WRITE(project, "");
	WRITE(project, "*********************");
	WRITE(project, "Node Flooding Summary");
	WRITE(project, "*********************");
	WRITE(project, "");

	for (j = 0; j < project->Nobjects[NODE]; j++)
    {
		if (project->Node[j].type == OUTFALL) continue;
		if (project->NodeStats[j].timeFlooded == 0.0) continue;
		t = MAX(0.01, (project->NodeStats[j].timeFlooded / 3600.0));

        if ( n == 0 )
        {
            WRITE(project , "Flooding refers to all water that overflows a node, whether it ponds or not.");
			fprintf(project->Frpt.file,
"\n  --------------------------------------------------------------------------"
"\n                                                             Total   Maximum"
"\n                                 Maximum   Time of Max       Flood    Ponded"
"\n                        Hours       Rate    Occurrence      Volume");
			if (project->RouteModel == DW) fprintf(project->Frpt.file, "     Depth");
			else                    fprintf(project->Frpt.file, "    Volume");
			fprintf(project->Frpt.file,
"\n  Node                 Flooded       %3s   days hr:min    %8s",
FlowUnitWords[project->FlowUnits], VolUnitsWords[project->UnitSystem]);
			if (project->RouteModel == DW)      fprintf(project->Frpt.file, "    %6s",
				PondingUnitsWords[project->UnitSystem]);
			else if (project->UnitSystem == US) fprintf(project->Frpt.file, "  1000 ft3");
			else                         fprintf(project->Frpt.file, "   1000 m3");
			fprintf(project->Frpt.file,
"\n  --------------------------------------------------------------------------");
            n = 1;
        }
		fprintf(project->Frpt.file, "\n  %-20s", project->Node[j].ID);
		fprintf(project->Frpt.file, " %7.2f ", t);
		fprintf(project->Frpt.file, FlowFmt, project->NodeStats[j].maxOverflow * UCF(project, FLOW));
		getElapsedTime(project, project->NodeStats[j].maxOverflowDate, &days, &hrs, &mins);
		fprintf(project->Frpt.file, "   %4d  %02d:%02d", days, hrs, mins);
		fprintf(project->Frpt.file, "%12.3f", project->NodeStats[j].volFlooded * Vcf);
		if (project->RouteModel == DW)
			fprintf(project->Frpt.file, " %9.3f",
			(project->NodeStats[j].maxDepth - project->Node[j].fullDepth) * UCF(project, LENGTH));
        else
			fprintf(project->Frpt.file, " %9.3f", project->NodeStats[j].maxPondedVol /
			1000.0 * UCF(project, VOLUME));
    }

	if (n == 0) WRITE(project, "No nodes were flooded.");
	WRITE(project, "");
}

//=============================================================================

void writeStorageVolumes(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: writes simulation statistics for storage units to report file.
//
{
    int    j, k, days, hrs, mins;
    double avgVol, maxVol, pctAvgVol, pctMaxVol;
    double addedVol, pctEvapLoss, pctSeepLoss;

    if ( project->Nnodes[STORAGE] > 0 )
    {
        WRITE(project , "");
        WRITE(project , "**********************");
        WRITE(project , "Storage Volume Summary");
        WRITE(project , "**********************");
        WRITE(project , "");

        fprintf(project->Frpt.file,
"\n  --------------------------------------------------------------------------------------------------"
"\n                         Average     Avg  Evap Infil       Maximum     Max    Time of Max    Maximum"
"\n                          Volume    Pcnt  Pcnt  Pcnt        Volume    Pcnt     Occurrence    Outflow");
		if (project->UnitSystem == US) fprintf(project->Frpt.file,
"\n  Storage Unit          1000 ft3    Full  Loss  Loss      1000 ft3    Full    days hr:min        ");
        else fprintf(project->Frpt.file,
"\n  Storage Unit           1000 m3    Full  Loss  Loss       1000 m3    Full    days hr:min        ");
        fprintf(project->Frpt.file, "%3s", FlowUnitWords[project->FlowUnits]);
        fprintf(project->Frpt.file,
"\n  --------------------------------------------------------------------------------------------------");

        for ( j = 0; j < project->Nobjects[NODE]; j++ )
        {
            if ( project->Node[j].type != STORAGE ) continue;
            k = project->Node[j].subIndex;
            fprintf(project->Frpt.file, "\n  %-20s", project->Node[j].ID);
            avgVol = project->StorageStats[k].avgVol / project->StepCount;
            maxVol = project->StorageStats[k].maxVol;
            pctMaxVol = 0.0;
            pctAvgVol = 0.0;
            if ( project->Node[j].fullVolume > 0.0 )
            {
                pctAvgVol = avgVol / project->Node[j].fullVolume * 100.0;
                pctMaxVol = maxVol / project->Node[j].fullVolume * 100.0;
            }
            pctEvapLoss = 0.0;
            pctSeepLoss = 0.0;
            addedVol = project->NodeInflow[j] + project->StorageStats[k].initVol;
            if ( addedVol > 0.0 )
            {
                pctEvapLoss = project->StorageStats[k].evapLosses / addedVol * 100.0;
                pctSeepLoss = project->StorageStats[k].seepLosses / addedVol * 100.0;
            }

            fprintf(project->Frpt.file, "%10.3f    %4.0f  %4.0f  %4.0f    %10.3f    %4.0f",
                avgVol*UCF(project,VOLUME)/1000.0, pctAvgVol, pctEvapLoss, pctSeepLoss,
                maxVol*UCF(project,VOLUME)/1000.0, pctMaxVol);

            getElapsedTime(project,project->StorageStats[k].maxVolDate, &days, &hrs, &mins);
            fprintf(project->Frpt.file, "    %4d  %02d:%02d  ", days, hrs, mins);
            fprintf(project->Frpt.file, FlowFmt, project->StorageStats[k].maxFlow*UCF(project,FLOW));
        }
        WRITE(project , "");
    }
}

//=============================================================================

void writeOutfallLoads(Project* project)
//
//  Input:   node
//  Output:  none
//  Purpose: writes simulation statistics for outfall nodess to report file.
//
{
    char    units[15];
    int     i, j, k, p;
    double  x;
    double  outfallCount, flowCount;
    double  flowSum, freqSum, volSum;
    double* totals;

    if ( project->Nnodes[OUTFALL] > 0 )
    {
        // --- initial totals
        totals = (double *) calloc(project->Nobjects[POLLUT], sizeof(double));
        for (p=0; p<project->Nobjects[POLLUT]; p++) totals[p] = 0.0;
        flowSum = 0.0;
        freqSum = 0.0;
		volSum  = 0.0;

        // --- print table title
        WRITE(project , "");
        WRITE(project , "***********************");
        WRITE(project , "Outfall Loading Summary");
        WRITE(project , "***********************");
        WRITE(project , "");

        // --- print table column headers
        fprintf(project->Frpt.file,
 "\n  -----------------------------------------------------------"); 
        for (p = 0; p < project->Nobjects[POLLUT]; p++) fprintf(project->Frpt.file, "--------------");
        fprintf(project->Frpt.file,
 "\n                         Flow       Avg       Max       Total");
        for (p=0; p<project->Nobjects[POLLUT]; p++) fprintf(project->Frpt.file,"         Total");
        fprintf(project->Frpt.file,
 "\n                         Freq      Flow      Flow      Volume");
        for (p = 0; p < project->Nobjects[POLLUT]; p++) fprintf(project->Frpt.file, "%14s", project->Pollut[p].ID);
        fprintf(project->Frpt.file,
 "\n  Outfall Node           Pcnt       %3s       %3s    %8s",
            FlowUnitWords[project->FlowUnits], FlowUnitWords[project->FlowUnits],
			VolUnitsWords[project->UnitSystem]);
        for (p = 0; p < project->Nobjects[POLLUT]; p++)
        {
            i = project->UnitSystem;
            if ( project->Pollut[p].units == COUNT ) i = 2;
            strcpy(units, LoadUnitsWords[i]);
            fprintf(project->Frpt.file, "%14s", units);
        }
        fprintf(project->Frpt.file,
 "\n  -----------------------------------------------------------");
        for (p = 0; p < project->Nobjects[POLLUT]; p++) fprintf(project->Frpt.file, "--------------");

        // --- identify each outfall node
        for (j=0; j<project->Nobjects[NODE]; j++)
        {
            if ( project->Node[j].type != OUTFALL ) continue;
            k = project->Node[j].subIndex;
            flowCount = project->OutfallStats[k].totalPeriods;

            // --- print node ID, flow freq., avg. flow, max. flow & flow vol.
            fprintf(project->Frpt.file, "\n  %-20s", project->Node[j].ID);
            x = 100.*flowCount/(double)project->StepCount;
            fprintf(project->Frpt.file, "%7.2f", x);
            freqSum += x;
            if ( flowCount > 0 )
				x = project->OutfallStats[k].avgFlow*UCF(project, FLOW) / flowCount;
            else
                x = 0.0;
            flowSum += x;

            fprintf(project->Frpt.file, " ");
            fprintf(project->Frpt.file, FlowFmt, x);
            fprintf(project->Frpt.file, " ");
			fprintf(project->Frpt.file, FlowFmt, project->OutfallStats[k].maxFlow*UCF(project, FLOW));
			fprintf(project->Frpt.file, "%12.3f", project->NodeInflow[j] * Vcf);
			volSum += project->NodeInflow[j];

            // --- print load of each pollutant for outfall
            for (p=0; p<project->Nobjects[POLLUT]; p++)
            {
                x = project->OutfallStats[k].totalLoad[p] * LperFT3 * project->Pollut[p].mcf;
                totals[p] += x;
                if ( project->Pollut[p].units == COUNT ) x = LOG10(x);
				fprintf(project->Frpt.file, "%14.3f", x); 
            }
        }

        // --- print total outfall loads
        outfallCount = project->Nnodes[OUTFALL];
        fprintf(project->Frpt.file,
 "\n  -----------------------------------------------------------"); 
        for (p = 0; p < project->Nobjects[POLLUT]; p++) fprintf(project->Frpt.file, "--------------");

        fprintf(project->Frpt.file, "\n  System              %7.2f ",
            freqSum/outfallCount);
        fprintf(project->Frpt.file, FlowFmt, flowSum);
        fprintf(project->Frpt.file, " ");
		fprintf(project->Frpt.file, FlowFmt, project->MaxOutfallFlow*UCF(project, FLOW));
		fprintf(project->Frpt.file, "%12.3f", volSum * Vcf);

        for (p = 0; p < project->Nobjects[POLLUT]; p++)
        {
			x = totals[p];
            if ( project->Pollut[p].units == COUNT ) x = LOG10(x);
			fprintf(project->Frpt.file, "%14.3f", x); 
        }
        WRITE(project , "");
        free(totals);
    } 
}

//=============================================================================

void writeLinkFlows(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: writes simulation statistics for links to report file.
//
{
    int    j, k, days, hrs, mins;
    double v, fullDepth;

    if ( project->Nobjects[LINK] == 0 ) return;
    WRITE(project , "");
    WRITE(project , "********************");
    WRITE(project , "Link Flow Summary");
    WRITE(project , "********************");
    WRITE(project , "");

    fprintf(project->Frpt.file,
"\n  -----------------------------------------------------------------------------"
"\n                                 Maximum  Time of Max   Maximum    Max/    Max/"
"\n                                  |Flow|   Occurrence   |Veloc|    Full    Full");
    if ( project->UnitSystem == US ) fprintf(project->Frpt.file,
"\n  Link                 Type          %3s  days hr:min    ft/sec    Flow   Depth",
        FlowUnitWords[project->FlowUnits]);
    else fprintf(project->Frpt.file, 
"\n  Link                 Type          %3s  days hr:min     m/sec    Flow   Depth",
        FlowUnitWords[project->FlowUnits]);
    fprintf(project->Frpt.file,
"\n  -----------------------------------------------------------------------------");

    for ( j = 0; j < project->Nobjects[LINK]; j++ )
    {
        // --- print link ID
        k = project->Link[j].subIndex;
        fprintf(project->Frpt.file, "\n  %-20s", project->Link[j].ID);

        // --- print link type
        if ( project->Link[j].xsect.type == DUMMY ) fprintf(project->Frpt.file, " DUMMY   ");
        else if ( project->Link[j].xsect.type == IRREGULAR ) fprintf(project->Frpt.file, " CHANNEL ");
        else fprintf(project->Frpt.file, " %-7s ", LinkTypeWords[project->Link[j].type]);

        // --- print max. flow & time of occurrence
        getElapsedTime(project,project->LinkStats[j].maxFlowDate, &days, &hrs, &mins);
		fprintf(project->Frpt.file, FlowFmt, project->LinkStats[j].maxFlow*UCF(project, FLOW));
        fprintf(project->Frpt.file, "  %4d  %02d:%02d", days, hrs, mins);

        // --- print max flow / flow capacity for pumps
        if ( project->Link[j].type == PUMP && project->Link[j].qFull > 0.0)
        {
            fprintf(project->Frpt.file, "          ");
            fprintf(project->Frpt.file, "  %6.2f",
                project->LinkStats[j].maxFlow / project->Link[j].qFull);
            continue;
        }

        // --- stop printing for dummy conduits
        if ( project->Link[j].xsect.type == DUMMY ) continue;

        // --- stop printing for outlet links (since they don't have xsections)
        if ( project->Link[j].type == OUTLET ) continue;

        // --- print max velocity & max/full flow for conduits
        if ( project->Link[j].type == CONDUIT )
        {
			v = project->LinkStats[j].maxVeloc*UCF(project, LENGTH);
            if ( v > 50.0 ) fprintf(project->Frpt.file, "    >50.00");
            else fprintf(project->Frpt.file, "   %7.2f", v);
            fprintf(project->Frpt.file, "  %6.2f", project->LinkStats[j].maxFlow / project->Link[j].qFull /
                                          (double)project->Conduit[k].barrels);
        }
        else fprintf(project->Frpt.file, "                  ");

        // --- print max/full depth
        fullDepth = project->Link[j].xsect.yFull;
        if ( project->Link[j].type == ORIFICE &&
             project->Orifice[k].type == BOTTOM_ORIFICE ) fullDepth = 0.0;
        if ( fullDepth > 0.0 )
        {
            fprintf(project->Frpt.file, "  %6.2f", project->LinkStats[j].maxDepth / fullDepth); 
        }
        else fprintf(project->Frpt.file, "        ");
    }
    WRITE(project , "");
}

//=============================================================================

void writeFlowClass(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: writes flow classification fro each conduit to report file.
//
{
    int   i, j, k;

    if ( project->RouteModel != DW ) return;
    WRITE(project , "");
    WRITE(project , "***************************");
    WRITE(project , "Flow Classification Summary");
    WRITE(project , "***************************");
    WRITE(project , "");
    fprintf(project->Frpt.file,
"\n  -------------------------------------------------------------------------------------"
"\n                      Adjusted    ---------- Fraction of Time in Flow Class ---------- "
"\n                       /Actual         Up    Down  Sub   Sup   Up    Down  Norm  Inlet "
"\n  Conduit               Length    Dry  Dry   Dry   Crit  Crit  Crit  Crit  Ltd   Ctrl  "
"\n  -------------------------------------------------------------------------------------");
    for ( j = 0; j < project->Nobjects[LINK]; j++ )
    {
        if ( project->Link[j].type != CONDUIT ) continue;
        if ( project->Link[j].xsect.type == DUMMY ) continue;
        k = project->Link[j].subIndex;
        fprintf(project->Frpt.file, "\n  %-20s", project->Link[j].ID);
        fprintf(project->Frpt.file, "  %6.2f ", project->Conduit[k].modLength / project->Conduit[k].length);
        for ( i=0; i<MAX_FLOW_CLASSES; i++ )
        {
            fprintf(project->Frpt.file, "  %4.2f",
                project->LinkStats[j].timeInFlowClass[i] /= project->StepCount);
        }
        fprintf(project->Frpt.file, "  %4.2f", project->LinkStats[j].timeNormalFlow /
                                      (project->NewRoutingTime/1000.0));
        fprintf(project->Frpt.file, "  %4.2f", project->LinkStats[j].timeInletControl /
                                      (project->NewRoutingTime/1000.0)); 
    }
    WRITE(project , "");
}

//=============================================================================

void writeLinkSurcharge(Project* project)
{
    int    i, j, n = 0;
    double t[5];

    WRITE(project , "");
    WRITE(project , "*************************");
    WRITE(project , "Conduit Surcharge Summary");
    WRITE(project , "*************************");
    WRITE(project , "");
    for ( j = 0; j < project->Nobjects[LINK]; j++ )
    {
        if ( project->Link[j].type != CONDUIT ||
			 project->Link[j].xsect.type == DUMMY ) continue; 
        t[0] = project->LinkStats[j].timeSurcharged / 3600.0;
        t[1] = project->LinkStats[j].timeFullUpstream / 3600.0;
        t[2] = project->LinkStats[j].timeFullDnstream / 3600.0;
        t[3] = project->LinkStats[j].timeFullFlow / 3600.0;
        if ( t[0] + t[1] + t[2] + t[3] == 0.0 ) continue;
        t[4] = project->LinkStats[j].timeCapacityLimited / 3600.0;
        for (i=0; i<5; i++) t[i] = MAX(0.01, t[i]);
        if (n == 0)
        {
            fprintf(project->Frpt.file, 
"\n  ----------------------------------------------------------------------------"
"\n                                                           Hours        Hours "
"\n                         --------- Hours Full --------   Above Full   Capacity"
"\n  Conduit                Both Ends  Upstream  Dnstream   Normal Flow   Limited"
"\n  ----------------------------------------------------------------------------");
            n = 1;
        }
        fprintf(project->Frpt.file, "\n  %-20s", project->Link[j].ID);
        fprintf(project->Frpt.file, "    %8.2f  %8.2f  %8.2f  %8.2f     %8.2f",
                t[0], t[1], t[2], t[3], t[4]);
    }
    if ( n == 0 ) WRITE(project , "No conduits were surcharged.");
    WRITE(project , "");
}

//=============================================================================

void writePumpFlows(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: writes simulation statistics for pumps to report file.
//
{
    int    j, k;
    double avgFlow, pctUtilized, pctOffCurve1, pctOffCurve2, totalSeconds;

    if ( project->Nlinks[PUMP] == 0 ) return;

    WRITE(project , "");
    WRITE(project , "***************");
    WRITE(project , "Pumping Summary");
    WRITE(project , "***************");
    WRITE(project , "");

    fprintf(project->Frpt.file,
"\n  ---------------------------------------------------------------------------------------------------------"
"\n                                                  Min       Avg       Max     Total     Power    %% Time Off"
"\n                        Percent   Number of      Flow      Flow      Flow    Volume     Usage    Pump Curve"
"\n  Pump                 Utilized   Start-Ups       %3s       %3s       %3s  %8s     Kw-hr    Low   High"
"\n  ---------------------------------------------------------------------------------------------------------",
        FlowUnitWords[project->FlowUnits], FlowUnitWords[project->FlowUnits],
        FlowUnitWords[project->FlowUnits], VolUnitsWords[project->UnitSystem]);
    for ( j = 0; j < project->Nobjects[LINK]; j++ )
    {
        if ( project->Link[j].type != PUMP ) continue;
        k = project->Link[j].subIndex;
        fprintf(project->Frpt.file, "\n  %-20s", project->Link[j].ID);
        totalSeconds = project->NewRoutingTime / 1000.0;
        pctUtilized = project->PumpStats[k].utilized / totalSeconds * 100.0;
        avgFlow = project->PumpStats[k].avgFlow;
        if ( project->PumpStats[k].totalPeriods > 0 )
            avgFlow /=  project->PumpStats[k].totalPeriods;
        fprintf(project->Frpt.file, " %8.2f  %10d %9.2f %9.2f %9.2f %9.3f %9.2f",
            pctUtilized, project->PumpStats[k].startUps, project->PumpStats[k].minFlow*UCF(project,FLOW),
            avgFlow*UCF(project,FLOW), project->PumpStats[k].maxFlow*UCF(project,FLOW), 
            project->PumpStats[k].volume*Vcf, project->PumpStats[k].energy);
        pctOffCurve1 = project->PumpStats[k].offCurveLow;
        pctOffCurve2 = project->PumpStats[k].offCurveHigh;
        if ( project->PumpStats[k].utilized > 0.0 )
        {
            pctOffCurve1 = pctOffCurve1 / project->PumpStats[k].utilized * 100.0;
            pctOffCurve2 = pctOffCurve2 / project->PumpStats[k].utilized * 100.0;
        }
        fprintf(project->Frpt.file, " %6.1f %6.1f", pctOffCurve1, pctOffCurve2); 
    }
    WRITE(project , "");
}

//=============================================================================

void writeLinkLoads(Project* project)
{
    int i, j, p;
    double x;
    char  units[15];
    char  linkLine[] = "--------------------";
    char  pollutLine[]   = "--------------";

    // --- print the table headings 
    WRITE(project , "");
    WRITE(project , "***************************");
    WRITE(project , "Link Pollutant Load Summary");
    WRITE(project , "***************************");
    WRITE(project , "");
    fprintf(project->Frpt.file, "\n  %s", linkLine);
    for (p = 0; p < project->Nobjects[POLLUT]; p++) fprintf(project->Frpt.file, "%s", pollutLine);
    fprintf(project->Frpt.file, "\n                      ");
    for (p = 0; p < project->Nobjects[POLLUT]; p++) fprintf(project->Frpt.file, "%14s", project->Pollut[p].ID);
    fprintf(project->Frpt.file, "\n  Link                ");
    for (p = 0; p < project->Nobjects[POLLUT]; p++)
    {
        i = project->UnitSystem;
        if ( project->Pollut[p].units == COUNT ) i = 2;
        strcpy(units, LoadUnitsWords[i]);
        fprintf(project->Frpt.file, "%14s", units);
    }
    fprintf(project->Frpt.file, "\n  %s", linkLine);
    for (p = 0; p < project->Nobjects[POLLUT]; p++) fprintf(project->Frpt.file, "%s", pollutLine);

    // --- print the pollutant loadings carried by each link
    for ( j = 0; j < project->Nobjects[LINK]; j++ )
    {
        fprintf(project->Frpt.file, "\n  %-20s", project->Link[j].ID);
        for (p = 0; p < project->Nobjects[POLLUT]; p++)
        {
            x = project->Link[j].totalLoad[p] * LperFT3 * project->Pollut[p].mcf;
            if ( project->Pollut[p].units == COUNT ) x = LOG10(x);
            if ( x < 10000. ) fprintf(project->Frpt.file, "%14.3f", x);
		else fprintf(project->Frpt.file, "%14.3e", x);
        }
    }
    WRITE(project , "");
}
