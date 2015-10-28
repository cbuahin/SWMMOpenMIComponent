from ctypes import*
import math

lib = cdll.LoadLibrary("Z:\\Documents\Projects\\SWMMOpenMIComponent\\Source\\SWMMOpenMIComponent\\bin\\Debug\\SWMMComponent.dll")
print(lib)  
print("\n")

finp = b"Z:\\Documents\\Projects\\SWMMOpenMIComponent\\Source\\SWMMOpenMINoGlobalsPythonTest\\test.inp"
frpt = b"Z:\\Documents\\Projects\\SWMMOpenMIComponent\\Source\\SWMMOpenMINoGlobalsPythonTest\\test.rpt"
fout = b"Z:\\Documents\\Projects\\SWMMOpenMIComponent\\Source\\SWMMOpenMINoGlobalsPythonTest\\test.out"

project = lib.swmm_open(finp , frpt , fout)
print(project)
print("\n")

newHour = 0
oldHour = 0
theDay = 0
theHour = 0
elapsedTime = c_double()

if(lib.swmm_getErrorCode(project) == 0):
    lib.swmm_start(project, 1)
    
    if(lib.swmm_getErrorCode(project) == 0):
        print("Simulating day: 0  Hour: 0")
        print("\n")

        while True:
            lib.swmm_step(project, byref(elapsedTime))
            newHour = elapsedTime.value * 24     

            if(newHour > oldHour):
                theDay = int(elapsedTime.value)
                temp = math.floor(elapsedTime.value)
                temp = (elapsedTime.value - temp) * 24.0
                theHour = int(temp)

                #print("\b\b\b\b\b\b\b\b\b\b\b\b\b\b")
                #print("\n")
                print "Hour " ,  str(theHour) , " Day " , str(theDay) , '            \r',
                
                #print("\n")
                oldHour = newHour

            if(elapsedTime.value <= 0 or not lib.swmm_getErrorCode(project) == 0):
                break


    lib.swmm_end(project)
    lib.swmm_report(project)
    lib.swmm_close(project)
   