#ifndef SWMMOPENMI_H
#define SWMMOPENMI_H

//#include <QObject>
//#include "swmmqopenmi_global.h"
//	
//
//class SWMMQOPENMI_EXPORT  SWMMQOpenMI
//{
//
//public:
//	static void addNodeLateralInflow(int index, double value);
//	static int  containsNodeLateralInflow(int index, double* const value);
//	
//	static QMap<int, double> NodeLateralInflows;
//	static QMap<int, double> NodeDepths;
//	static QMap<int, double> LinkFlows;
//};


#ifdef __cplusplus
extern "C" {
#endif

	void addNodeLateralInflow(int index, double value);
	int  containsNodeLateralInflow(int index, double* const value);

	void addNodeDepth(int index, double value);
	int containsNodeDepth(int index, double* const value);

	void addSubcatchRain(int index, double value);
	int containsSubcatchRain(int index, double* const value);

#ifdef __cplusplus
};
#endif

#endif // SWMMQOPENMI_H
