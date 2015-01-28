#include "stdafx.h"
#include "swmmopenmi.h"
#include <map>

using namespace std;

static map<int, double> NodeLateralInflows = map<int, double>();
static map<int, double> NodeDepths = map<int, double>();
static map<int, double> SubcatchRainfall = map<int, double>();

//Lateral Inflow
void addNodeLateralInflow(int index, double value)
{
	NodeLateralInflows[index] = value;
}

int containsNodeLateralInflow(int index, double* const  value)
{
	map<int, double>::iterator it = NodeLateralInflows.find(index);

	if (it != NodeLateralInflows.end())
	{
		*value = NodeLateralInflows[index];
		return 1;
	}

	return 0;
}


//Node Depths
void addNodeDepth(int index, double value)
{
	NodeDepths[index] = value;
}

int containsNodeDepth(int index, double* const value)
{
	map<int, double>::iterator it = NodeDepths.find(index);

	if (it != NodeDepths.end())
	{
		*value = NodeDepths[index];
		return 1;
	}

	return 0;
}


//SubcatchRainfall
void addSubcatchRain(int index, double value)
{
	SubcatchRainfall[index] = value;
}

int containsSubcatchRain(int index, double* const value)
{
	map<int, double>::iterator it = SubcatchRainfall.find(index);

	if (it != SubcatchRainfall.end())
	{
		*value = SubcatchRainfall[index];
		return 1;
	}

	return 0;
}