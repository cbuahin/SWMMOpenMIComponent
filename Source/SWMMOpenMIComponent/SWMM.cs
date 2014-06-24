using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace SWMMOpenMIComponent
{
    class SWMM : IDisposable
    {

        enum SWMMModelStatus
        {
           
        }

        # region SWMMDelegates

        public delegate int OpenDelegate(string inputFile, string reportFile, string outFile);

        public delegate int StartModelDelegate(int saveResults);

        public delegate int PerformTimeStepDelegate(ref double elapsedTime);

        public delegate int EndRunDelegate();

        public delegate int CloseModelDelegate();

        public delegate string GetErrorMessageDelegate(ref string message, int errorCode);

        public delegate void DecodeDateTimeDelegate(double dateTime, ref int year, ref int month, ref int day, ref int hour, ref int minute, ref int second);

        delegate double GetDateTimeDelegate(string name);

        # endregion    
    
        # region Variables

        FileInfo library;
        String inputFile, outPutFile, reportFile;
        bool disposed = false;
        DateTime startDateTime, endDateTime, currentDateTime;
        double startDateTimeD, endDateTimeD;

        IntPtr hModule;
        OpenDelegate open;
        StartModelDelegate startModel;
        PerformTimeStepDelegate performTimeStep;
        EndRunDelegate endRun;
        CloseModelDelegate closeModel;
        GetErrorMessageDelegate getErrorMessage;
        DecodeDateTimeDelegate decodeDateTime;
        GetDateTimeDelegate getDateTime;

        # endregion


        #region Constructor

        public SWMM(FileInfo library, string inputFile,string outputFile, string reportFile = "")
        {
            this.library = library;
            hModule = WinLibraryLoader.LoadLibrary(library.FullName);
            CheckIfLibraryError();

            if(hModule == IntPtr.Zero)
            {
                throw new FileLoadException("Unable to load library located at " + library.FullName, library.FullName);
            }

            this.inputFile = inputFile;
            this.outPutFile = outputFile;
            this.reportFile = reportFile;

            AssignFunctions();
        }

        #endregion

        # region Properties

        public DateTime StartDateTime
        {
            get
            {
                if (hModule != IntPtr.Zero)
                {
                    double value = getDateTime("begin");
                    int year, month, day, hour, minute, second;
                    year = month = day = hour = minute = second = 0;
                    decodeDateTime(value, ref year, ref month, ref day, ref hour, ref minute, ref second);
                    startDateTime = new DateTime(year, month, day, hour, minute, second);
                }

                return startDateTime; 
            }
        }

        public DateTime EndDateTime
        {
            get 
            {
                return endDateTime;
            }
        }

        public DateTime CurrentDateTime
        {
            get 
            {
                return currentDateTime; 
            }
        }

        #endregion

        # region Functions


        private void AssignFunctions()
        {
            open = WinLibraryLoader.LoadFunction<OpenDelegate>(ref hModule, "swmm_open");
            CheckIfLibraryError();
           
            startModel = WinLibraryLoader.LoadFunction<StartModelDelegate>(ref hModule, "swmm_start");
            CheckIfLibraryError();
            
            performTimeStep = WinLibraryLoader.LoadFunction<PerformTimeStepDelegate>(ref hModule, "swmm_step");
            CheckIfLibraryError();
           
            endRun = WinLibraryLoader.LoadFunction<EndRunDelegate>(ref hModule, "swmm_end");
            CheckIfLibraryError();

            closeModel = WinLibraryLoader.LoadFunction<CloseModelDelegate>(ref hModule, "swmm_close");
            CheckIfLibraryError();

            getErrorMessage = WinLibraryLoader.LoadFunction<GetErrorMessageDelegate>(ref hModule, "getErrorMsg");
            CheckIfLibraryError();

            decodeDateTime = WinLibraryLoader.LoadFunction<DecodeDateTimeDelegate>(ref hModule, "datetime_decodeDateTime");
            CheckIfLibraryError();

            getDateTime = WinLibraryLoader.LoadFunction<GetDateTimeDelegate>(ref hModule, "swmm_getDateTime");
            CheckIfLibraryError();

        }

        public void StartModel()
        {
            int error = startModel(1);
            SetError(error);
        }

        public void Initialize()
        {
           int error = open(inputFile, reportFile, outPutFile);
           SetError(error);

           int year, month, day, hour, minute, second;
           year = month = day = hour = minute = second = 0;


           endDateTimeD = getDateTime("end");
           decodeDateTime(endDateTimeD, ref year, ref month, ref day, ref hour, ref minute, ref second);
           endDateTime = new DateTime(year, month, day, hour, minute, second);

           startDateTimeD = getDateTime("begin");
           decodeDateTime(startDateTimeD, ref year, ref month, ref day, ref hour, ref minute, ref second);
           startDateTime = new DateTime(year, month, day, hour, minute, second);

           currentDateTime = startDateTime;
        }

        public bool PerformTimeStep()
        {
            double timeElapsed = -10000;
            int error = performTimeStep(ref timeElapsed);
            SetError(error);

            if (timeElapsed == 0)
            {
                currentDateTime = endDateTime;
                endRun();

                return false;
            }
            else
            {
                timeElapsed = timeElapsed + startDateTimeD;
                int year, month, day, hour, minute, second;
                year = month = day = hour = minute = second = 0;
                decodeDateTime(timeElapsed, ref year, ref month, ref day, ref hour, ref minute, ref second);
                currentDateTime = new DateTime(year, month, day, hour, minute, second);
                return true;
            }

        }

        public void EndRun()
        {
            int error = endRun();
            SetError(error);
        }

        public void CloseModel()
        {
            int error = closeModel();
            SetError(error);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected void Dispose(bool disposing)
        {
            if (disposed)
            {
                return;
            }

            if (disposing)
            {
                endRun();
                closeModel();

                if (hModule != IntPtr.Zero && !WinLibraryLoader.FreeLibrary(hModule))
                {
                     Marshal.ThrowExceptionForHR(Marshal.GetHRForLastWin32Error()); 
                }
            }

            if(File.Exists(library.FullName))
            {
                File.Delete(library.FullName);
            }

            //Delete Library
            disposed = true;
        }

        private void SetError(int errorCode)
        {
            if(errorCode != 0)
            {
                string message = "";
                getErrorMessage(ref message, errorCode);
                throw new Exception("SWMM Error : " + message);
            }
        }

        private void CheckIfLibraryError()
        {
            int error = Marshal.GetLastWin32Error();

            if(error != 0)
            {
                //throw new Win32Exception(error);
            }
        }

        # endregion
    }
}
