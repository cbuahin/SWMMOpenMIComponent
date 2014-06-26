using Oatc.OpenMI.Sdk.Backbone;
using OpenMI.Standard2;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace SWMMOpenMIComponent
{
    public class SWMM : IDisposable
    {

        # region SWMMDelegates

        delegate int OpenDelegate(string inputFile, string reportFile, string outFile);

        delegate int StartModelDelegate(int saveResults);

        delegate int PerformTimeStepDelegate(ref double elapsedTime);

        delegate int EndRunDelegate();

        delegate int CloseModelDelegate();

        delegate string GetErrorMessageDelegate(ref string message, int errorCode);

        delegate void DecodeDateTimeDelegate(double dateTime, ref int year, ref int month, ref int day, ref int hour, ref int minute, ref int second);

        delegate double GetDateTimeDelegate(string name);

        public delegate int GetObjectTypeCountDelegate(ObjectType type);

        delegate IntPtr GetObjectDelegate(int index);

        delegate IntPtr GetObjectByIdDelegate(string id);

        delegate void SetObjectValue(IntPtr ptrToObject, string propertyName);

        # endregion    
    
        # region Variables

        FileInfo library;
        String inputFile, outPutFile, reportFile;
        bool disposed = false;
        DateTime startDateTime, endDateTime, currentDateTime;
        double startDateTimeD, endDateTimeD;

        //Library
        IntPtr hModule;

        //Internal delegates
        OpenDelegate open;
        StartModelDelegate startModel;
        PerformTimeStepDelegate performTimeStep;
        EndRunDelegate endRun;
        CloseModelDelegate closeModel;
        GetErrorMessageDelegate getErrorMessage;
        DecodeDateTimeDelegate decodeDateTime;
        GetDateTimeDelegate getDateTime;
        GetObjectDelegate getNode;
        GetObjectByIdDelegate getNodeById;
        GetObjectDelegate getLink;
        SetObjectValue setNodeObjectValue;
        SetObjectValue setLinkObjectValue;


        //Direct delegate calls
        public GetObjectTypeCountDelegate GetObjectTypeCount;

        //Variables that can be exchanged
        public static Dictionary<ObjectType, KeyValuePair<string, KeyValuePair<string,IValueDefinition>>[]> SWMMVariableTypes;


        //Marshalld structs
        Dictionary<string,TNode> nodes;
        Dictionary<string,TLink> links;


        # endregion

        #region Constructor
        static SWMM()
        {
             IUnit lengthU = new Unit("Length (ft)", 1.0, 0.0)
             {
                 Dimension = new Dimension(PredefinedDimensions.Length),
             };

             IUnit areaU = new Unit("Area (ft^2)", 1.0, 0.0)
             {
                 Dimension = new Dimension(PredefinedDimensions.Area) 
             };

             IUnit flowU = new Unit("Flow (ft^3/s)", 1.0, 0.0)
             {
                 Dimension = new Dimension(PredefinedDimensions.VolumePerTime)
             };

             IUnit coeffU = new Unit("Coefficient", 1.0, 0.0)
             {
                 Dimension = new Dimension()
             };

             IValueDefinition elevationV = new Quantity("Elevation")
              {
                   Unit = lengthU,
                   MissingDataValue = -9999,
                   Caption = "Elevation (ft)",
                   Description = "",
                   ValueType = typeof(double)
              };

             IValueDefinition depthV = new Quantity("Depth")
             {
                 Unit = lengthU,
                 MissingDataValue = -9999,
                 Caption = "Depth (ft)",
                 Description = "",
                 ValueType = typeof(double)
             };

             IValueDefinition heightV = new Quantity("Height")
             {
                 Unit = lengthU,
                 MissingDataValue = -9999,
                 Caption = "Height (ft)",
                 Description = "",
                 ValueType = typeof(double)
             };

             IValueDefinition areaV = new Quantity("Area")
             {
                 Unit = areaU,
                 MissingDataValue = -9999,
                 Caption = "Area (ft^2)",
                 Description = "",
                 ValueType = typeof(double)
             };

             IValueDefinition flowV = new Quantity("Flow")
             {
                 Unit = flowU,
                 MissingDataValue = -9999,
                 Caption = "Flow (ft^3/s)",
                 Description = "",
                 ValueType = typeof(double)
             };

             IValueDefinition coeffV = new Quantity("Coefficients")
             {
                 Unit = flowU,
                 MissingDataValue = -9999,
                 Caption = "Coefficients",
                 Description = "",
                 ValueType = typeof(double)
             };

             SWMMVariableTypes = new Dictionary<ObjectType, KeyValuePair<string, KeyValuePair<string, IValueDefinition>>[]>();
             SWMMVariableTypes.Add(ObjectType.NODE, new KeyValuePair<string, KeyValuePair<string, IValueDefinition>>[] 
            {
               new KeyValuePair<string, KeyValuePair<string, IValueDefinition>>( "invertElev" , new KeyValuePair<string,IValueDefinition>("Invert elevation (ft)",elevationV)),
               new KeyValuePair<string, KeyValuePair<string, IValueDefinition>>( "crownElev" , new KeyValuePair<string,IValueDefinition>("Top of highest connecting conduit (ft)",elevationV)),
               new KeyValuePair<string, KeyValuePair<string, IValueDefinition>>( "initDepth" , new KeyValuePair<string,IValueDefinition>("Initial storage level (ft)",depthV)),
               new KeyValuePair<string, KeyValuePair<string, IValueDefinition>>( "surDepth" , new KeyValuePair<string,IValueDefinition>("Added depth under surcharge (ft)",depthV)),
               new KeyValuePair<string, KeyValuePair<string, IValueDefinition>>( "newDepth" , new KeyValuePair<string,IValueDefinition>("Water depth (ft)",depthV)),
               new KeyValuePair<string, KeyValuePair<string, IValueDefinition>>( "inflow" , new KeyValuePair<string,IValueDefinition>("Total inflow (cfs)",flowV)),
               new KeyValuePair<string, KeyValuePair<string, IValueDefinition>>( "outflow" , new KeyValuePair<string,IValueDefinition>("Total outflow (cfs)",flowV)),
               new KeyValuePair<string, KeyValuePair<string, IValueDefinition>>( "newLatFlow" , new KeyValuePair<string,IValueDefinition>("Lateral inflow (cfs)",flowV)),
            });
        }

        public SWMM(FileInfo library, string inputFile,string outputFile, string reportFile = "")
        {
            //dictionary
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

            AssignFunctionsToDelegates();
        }

        #endregion

        # region Properties

        public DateTime StartDateTime
        {
            get
            {
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

        public Dictionary<string,TNode> Nodes
        {
            get { return nodes; }
        }

        public Dictionary<string, TLink> Links
        {
            get { return links; }
        }

        #endregion

        # region Functions

        private void AssignFunctionsToDelegates()
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

            GetObjectTypeCount = WinLibraryLoader.LoadFunction<GetObjectTypeCountDelegate>(ref hModule, "getObjectTypeCount");
            CheckIfLibraryError();

            getNode = WinLibraryLoader.LoadFunction<GetObjectDelegate>(ref hModule, "getNode");
            CheckIfLibraryError();

            getNodeById = WinLibraryLoader.LoadFunction<GetObjectByIdDelegate>(ref hModule, "getNodeById");
            CheckIfLibraryError();

            getLink = WinLibraryLoader.LoadFunction<GetObjectDelegate>(ref hModule, "getLink");
            CheckIfLibraryError();

            setNodeObjectValue = WinLibraryLoader.LoadFunction<SetObjectValue>(ref hModule, "setNode");
            CheckIfLibraryError();

            setLinkObjectValue = WinLibraryLoader.LoadFunction<SetObjectValue>(ref hModule, "setLink");
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


            //Initialize Objects
           InitializeNodes();
           InitializeLinks();
           InitializeCatchments();


        }


        void InitializeNodes()
        {
            nodes = new Dictionary<string, TNode>();

            int nodeCount = GetObjectTypeCount(ObjectType.NODE);

            for(int i = 0 ; i < nodeCount ; i++)
            {
                TNode node = GetNode(i);
                nodes.Add(node.ID, node);
            }

        }

        void InitializeLinks()
        {
            links = new Dictionary<string, TLink>();

            int linkCount = GetObjectTypeCount(ObjectType.LINK);

            for(int i = 0 ; i < linkCount ; i++)
            {
                TLink link = GetLink(i);
                links.Add(link.ID, link);
            }
        }

        void InitializeCatchments()
        {

        }

        public bool PerformTimeStep()
        {
            double timeElapsed = -10000;
            int error = performTimeStep(ref timeElapsed);
            SetError(error);

            if (timeElapsed == 0)
            {
                currentDateTime = endDateTime;
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
        
        public TNode GetNode(int index)
        {
            IntPtr nodePtr = getNode(index);
            TNode node = (TNode)Marshal.PtrToStructure(nodePtr, typeof(TNode));
            return node;
        }

        public TNode GetNodeById(string id)
        {
            IntPtr nodePtr = getNodeById(id);
            TNode node = (TNode)Marshal.PtrToStructure(nodePtr, typeof(TNode));
            return node;
        }

        public TLink GetLink(int index)
        {
            IntPtr linkPtr = getLink(index);
            TLink link = (TLink)Marshal.PtrToStructure(linkPtr, typeof(TLink));
            return link;
        }

        public void UpdateValue(ObjectType type , string name, string property, double value)
        {
            switch(type)
            {
                case ObjectType.NODE:
                    {
                        FieldInfo field = typeof(TNode).GetField(property);
                        field.SetValue(nodes[name], value);

                        IntPtr nodePtr = IntPtr.Zero;
                        Marshal.StructureToPtr(nodes[name], nodePtr, false);
                        setNodeObjectValue(nodePtr, property);
                    }
                    break;
                case ObjectType.LINK:
                    {
                        FieldInfo field = typeof(TLink).GetField(property);
                        field.SetValue(links[name], value);

                        IntPtr nodePtr = IntPtr.Zero;
                        Marshal.StructureToPtr(links[name], nodePtr, false);
                        setLinkObjectValue(nodePtr, property);
                    }
                    break;
            }
        }

        public double GetValue(ObjectType type , string name, string property)
        {
            switch (type)
            {
                case ObjectType.NODE:
                    {
                        FieldInfo field = typeof(TNode).GetField(property);
                        TNode node = nodes[name];
                        node = GetNodeById(node.ID);
                        return (double)field.GetValue(nodes[name]);
                    }
                case ObjectType.LINK:
                    {
                        FieldInfo field = typeof(TLink).GetField(property);
                        return (double)field.GetValue(links[name]);
                    }
            }

            return 0.0;
        }
       
        # endregion
    }
}
