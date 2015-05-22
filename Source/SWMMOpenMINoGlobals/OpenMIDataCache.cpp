#include <stdafx.h>
#include "OpenMIDataCache.h"
#include <map>


using namespace std;


map<Project*, map<int, double>> NodeLateralInflows;
map<Project*, map<int, double>> NodeDepths;
map<Project*, map<int, double>> SubcatchRainfall;

typedef struct OpenMIDataCache OpenMIDataCache;

//node lateral inflow
void addNodeLateralInflow(Project* project, int index, double value)
{
	NodeLateralInflows[project][index] = value;
}

int containsNodeLateralInflow(Project* project, int index, double* const  value)
{
	map<Project*,map<int, double>>::iterator it = NodeLateralInflows.find(project);

	if (it != NodeLateralInflows.end())
	{
		map<int, double > foundProject = NodeLateralInflows[project];
		map<int, double > ::iterator it1 = foundProject.find(index);
		
		if (it1 != foundProject.end())
		{
			*value = foundProject[index];
			return 1;
		}
	}

	return 0;
}


//Node Depths
void addNodeDepth(Project* project, int index, double value)
{
	NodeDepths[project][index] = value;
}

int containsNodeDepth(Project* project, int index, double* const value)
{

	map<Project*, map<int, double>>::iterator it = NodeDepths.find(project);

	if (it != NodeDepths.end())
	{
		map<int, double > foundProject = NodeDepths[project];
		map<int, double > ::iterator it1 = foundProject.find(index);

		if (it1 != foundProject.end())
		{
			*value = foundProject[index];
			return 1;
		}
	}

	return 0;
}


//SubcatchRainfall
void addSubcatchRain(Project* project, int index, double value)
{
	SubcatchRainfall[project][index] = value;
}

int containsSubcatchRain(Project* project, int index, double* const value)
{

	map<Project*, map<int, double>>::iterator it = SubcatchRainfall.find(project);

	if (it != SubcatchRainfall.end())
	{
		map<int, double > foundProject = SubcatchRainfall[project];
		map<int, double > ::iterator it1 = foundProject.find(index);

		if (it1 != foundProject.end())
		{
			*value = foundProject[index];
			return 1;
		}
	}

	return 0;
}