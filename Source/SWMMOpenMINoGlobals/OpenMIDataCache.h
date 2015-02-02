#ifndef OpenMIDataCache_H
#define OpenMIDataCache_H

typedef struct Project Project;

#ifdef __cplusplus
extern "C" {
#endif

	void addNodeLateralInflow(Project* project, int index, double value);
	int  containsNodeLateralInflow(Project* project, int index, double* const value);

	void addNodeDepth(Project* project, int index, double value);
	int containsNodeDepth(Project* project, int index, double* const value);

	void addSubcatchRain(Project* project, int index, double value);
	int containsSubcatchRain(Project* project, int index, double* const value);


#ifdef __cplusplus 
}   // matches the linkage specification from above */ 
#endif

#endif