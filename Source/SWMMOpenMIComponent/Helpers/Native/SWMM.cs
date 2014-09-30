using Oatc.OpenMI.Sdk.Backbone;
using OpenMI.Standard2;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace SWMMOpenMIComponent
{
    /// <summary>
    /// SWMM Engine Model Wrapper
    /// </summary>
    /// <remarks>Responsible for managing the SWMM engine library. It marshalls SWMM objects via Platform invoke for updating</remarks>
    public class SWMM : IDisposable
    {

        # region SWMMDelegates


        delegate int OpenDelegate([MarshalAs(UnmanagedType.LPStr)]string inputFile, [MarshalAs(UnmanagedType.LPStr)]string reportFile, [MarshalAs(UnmanagedType.LPStr)]string outFile);

        delegate int StartModelDelegate(int saveResults);

        delegate int PerformTimeStepDelegate(out double elapsedTime);

        delegate int IntRetureDelegate();

        delegate string GetErrorMessageDelegate(int errorCode);

        delegate void DecodeDateTimeDelegate(double dateTime, out int year, out int month, out int day, out int hour, out int minute, out int second);

        delegate double GetDateTimeDelegate([MarshalAs(UnmanagedType.LPStr)] string name);

        public delegate int GetObjectTypeCountDelegate(ObjectType type);

        delegate IntPtr GetObjectDelegate(int index);

        delegate IntPtr GetObjectByIdDelegate([MarshalAs(UnmanagedType.LPStr)]string id);

        delegate void SetObjectValue(IntPtr ptrToObject, [MarshalAs(UnmanagedType.LPStr)]string propertyName);

       

        # endregion

        # region Variables

        FileInfo library;
        String inputFile, outPutFile, reportFile;
        bool disposed = false;
        DateTime startDateTime, endDateTime, currentDateTime;
        double startDateTimeD, endDateTimeD;

        //Library
        IntPtr hModule;

        # region Delegates

        //Internal delegates
        OpenDelegate open;
        StartModelDelegate startModel;
        PerformTimeStepDelegate performTimeStep;
        IntRetureDelegate endRun;
        IntRetureDelegate closeModel;
        IntRetureDelegate reportModelResults;
        GetErrorMessageDelegate getErrorMessage;
        DecodeDateTimeDelegate decodeDateTime;
        GetDateTimeDelegate getDateTime;

        GetObjectDelegate getNode;
        GetObjectByIdDelegate getNodeById;
        
        GetObjectDelegate getLink;
        GetObjectByIdDelegate getLinkById;

        GetObjectDelegate getSubCatchment;
        GetObjectByIdDelegate getSubCatchmentById;

        SetObjectValue setNodeObjectValue;
        SetObjectValue setLinkObjectValue;
        SetObjectValue setSubCatchmentObjectValue;
      
        //Direct delegate calls
        public GetObjectTypeCountDelegate GetObjectTypeCount;
        

        #endregion

        //Marshalld structs
        Dictionary<string, Node> nodes;
        Dictionary<int, string> nodeIndexes;
        Dictionary<string, Link> links;
        Dictionary<string, SubCatchment> subCatchments;

        # endregion

        #region Constructor
      
        public SWMM(FileInfo library, string inputFile, string outputFile, string reportFile = "")
        {
            //dictionary
            disposed = false;
            this.library = library;
            hModule = WinLibraryLoader.LoadLibrary(library.FullName);
            CheckIfLibraryError();
           
            if (hModule == IntPtr.Zero)
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

        public Dictionary<string, Node> Nodes
        {
            get { return nodes; }
        }

        public Dictionary<string, Link> Links
        {
            get { return links; }
        }

        public Dictionary<string, SubCatchment> SubCatchments
        {
            get { return subCatchments; }
          
        }

        #endregion

        # region Functions

        /// <summary>
        /// Assigns delegates to the SWMM engines functions
        /// </summary>
        private void AssignFunctionsToDelegates()
        {
            open = WinLibraryLoader.LoadFunction<OpenDelegate>(ref hModule, "swmm_open");
            CheckIfLibraryError();

            startModel = WinLibraryLoader.LoadFunction<StartModelDelegate>(ref hModule, "swmm_start");
            CheckIfLibraryError();

            performTimeStep = WinLibraryLoader.LoadFunction<PerformTimeStepDelegate>(ref hModule, "swmm_step");
            CheckIfLibraryError();

            endRun = WinLibraryLoader.LoadFunction<IntRetureDelegate>(ref hModule, "swmm_end");
            CheckIfLibraryError();

            reportModelResults = WinLibraryLoader.LoadFunction<IntRetureDelegate>(ref hModule, "swmm_report");
            CheckIfLibraryError();

            closeModel = WinLibraryLoader.LoadFunction<IntRetureDelegate>(ref hModule, "swmm_close");
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

            setNodeObjectValue = WinLibraryLoader.LoadFunction<SetObjectValue>(ref hModule, "setNode");
            CheckIfLibraryError();

            getNodeById = WinLibraryLoader.LoadFunction<GetObjectByIdDelegate>(ref hModule, "getNodeById");
            CheckIfLibraryError();


            getLink = WinLibraryLoader.LoadFunction<GetObjectDelegate>(ref hModule, "getLink");
            CheckIfLibraryError();

            setLinkObjectValue = WinLibraryLoader.LoadFunction<SetObjectValue>(ref hModule, "setLink");
            CheckIfLibraryError();
            
            getLinkById = WinLibraryLoader.LoadFunction<GetObjectByIdDelegate>(ref hModule, "getLinkById");
            CheckIfLibraryError();


            getSubCatchment = WinLibraryLoader.LoadFunction<GetObjectDelegate>(ref hModule, "getSubcatch");
            CheckIfLibraryError();

            setSubCatchmentObjectValue = WinLibraryLoader.LoadFunction<SetObjectValue>(ref hModule, "setSubcatch");
            CheckIfLibraryError();

            getSubCatchmentById = WinLibraryLoader.LoadFunction<GetObjectByIdDelegate>(ref hModule, "getSubcatchById");
            CheckIfLibraryError();

        }

        public void StartModel()
        {
            bool op = true;
            int error = startModel(Convert.ToInt32(op));
            SetError(error);
            
        }

        public void Initialize()
        {


            int error = open(inputFile, reportFile, outPutFile);
            SetError(error);

            int year, month, day, hour, minute, second;
            year = month = day = hour = minute = second = 0;


            endDateTimeD = getDateTime("end");
            decodeDateTime(endDateTimeD, out year, out month, out day, out hour, out minute, out second);
            endDateTime = new DateTime(year, month, day, hour, minute, second);

            startDateTimeD = getDateTime("begin");
            decodeDateTime(startDateTimeD, out year, out month, out day, out hour, out minute, out second);
            startDateTime = new DateTime(year, month, day, hour, minute, second);

            currentDateTime = startDateTime;


            //Initialize Objects
            InitializeNodes();
            InitializeLinks();
            InitializeCatchments();


        }

        void InitializeNodes()
        {
            nodes = new Dictionary<string, Node>();
            nodeIndexes = new Dictionary<int, string>();

            int nodeCount = GetObjectTypeCount(ObjectType.NODE);

            for (int i = 0; i < nodeCount; i++)
            {
                Node node = new Node()
                {
                    ObjectIndex = i,
                };

                TNode tnode = GetNode(i);
                node.NativeNode = tnode;
                node.ObjectIndex = i;
                string id =Marshal.PtrToStringAnsi(tnode.ID);
                nodes.Add(id, node);
                nodeIndexes.Add(i, id);
            }

        }

        void InitializeLinks()
        {
            links = new Dictionary<string, Link>();

            int linkCount = GetObjectTypeCount(ObjectType.LINK);

            for (int i = 0; i < linkCount; i++)
            {
                Link link = new Link()
                {
                    ObjectIndex = i,
                };

                TLink tlink = GetLink(i);
                link.NativeLink = tlink;
                link.ObjectIndex = i;
                link.UpstreamNode = nodes[nodeIndexes[tlink.node1]];
                link.DownstreamNode = nodes[nodeIndexes[tlink.node2]];
                links.Add(Marshal.PtrToStringAnsi(tlink.ID), link);
            }
        }

        void InitializeCatchments()
        {
            subCatchments = new Dictionary<string, SubCatchment>();

            int subCatchCount = GetObjectTypeCount(ObjectType.SUBCATCH);

            for (int i = 0; i < subCatchCount; i++)
            {
                SubCatchment subCatch = new SubCatchment()
                {
                    ObjectIndex = i,
                };

                TSubcatch tsub = GetSubCatchment (i);
                subCatch.NativeSubCatchment = tsub;
                subCatch.ObjectIndex = i;
                subCatchments.Add(Marshal.PtrToStringAnsi(tsub.ID), subCatch);
            }
        }

        public bool PerformTimeStep()
        {
            double timeElapsed = currentDateTime.ToOADate();
            int error = performTimeStep(out timeElapsed);
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
                decodeDateTime(timeElapsed, out year, out month, out day, out hour, out minute, out second);
                currentDateTime = new DateTime(year, month, day, hour, minute, second);
                return true;
            }

        }

        public void EndRun()
        {
            int error = endRun();
            SetError(error);

            error =  reportModelResults();
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
            if (!disposed)
            {
                if (disposing)
                {

                }

                endRun();
                closeModel();

                if (hModule != IntPtr.Zero && !WinLibraryLoader.FreeLibrary(hModule))
                {
                    Marshal.ThrowExceptionForHR(Marshal.GetHRForLastWin32Error());
                }


                if (File.Exists(library.FullName))
                {
                    File.Delete(library.FullName);
                }

                GC.Collect();
                GC.WaitForPendingFinalizers();
                disposed = true;
            }
        }

        private void SetError(int errorCode)
        {
            if (errorCode != 0)
            {
                string message = getErrorMessage(errorCode);
                throw new Exception("SWMM Error : " + message);
            }
        }

        /// <summary>
        /// Checks to see if SWMM engine has error
        /// </summary>
        private void CheckIfLibraryError()
        {
            int error = Marshal.GetLastWin32Error();

            if (error != 0)
            {
               Win32Exception except =  new Win32Exception(error);
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

        public TLink GetLinkById(string id)
        {
            IntPtr linkPtr = getLinkById(id);
            TLink link = (TLink)Marshal.PtrToStructure(linkPtr, typeof(TLink));
            return link;
        }

        public TSubcatch GetSubCatchment(int index)
        {
            IntPtr subCatchPtr  = getSubCatchment(index);
            TSubcatch subCatch = (TSubcatch)Marshal.PtrToStructure(subCatchPtr, typeof(TSubcatch));
            return subCatch;
        }

        public TSubcatch GetSubCatchmentById(string id)
        {
            IntPtr subCatchPtr = getSubCatchmentById(id);
            TSubcatch subCatch = (TSubcatch)Marshal.PtrToStructure(subCatchPtr, typeof(TSubcatch));
            return subCatch; ;
        }

        public void UpdateSWMMObject(SWMMObject swmmObject , string propertyName )
        {
            switch (swmmObject.ObjectType)
            {
                case ObjectType.NODE:
                    {
                        Node node = (Node)swmmObject;
                        IntPtr nodePtr = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(TNode)));
                        Marshal.StructureToPtr(node.NativeNode, nodePtr, false);
                        setNodeObjectValue(nodePtr, propertyName);

                    }
                    break;
                case ObjectType.LINK:
                    {
                        Link link = (Link)swmmObject;
                        IntPtr linkPtr = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(TLink)));
                        Marshal.StructureToPtr(link.NativeLink, linkPtr, false);
                        setLinkObjectValue(linkPtr, propertyName);
                    }
                    break;
                case ObjectType.SUBCATCH:
                    {
                        SubCatchment subCatch = (SubCatchment)swmmObject;
                        IntPtr subCatchPtr = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(TSubcatch)));
                        Marshal.StructureToPtr(subCatch.NativeSubCatchment, subCatchPtr, false);
                        setSubCatchmentObjectValue(subCatchPtr, propertyName);
                    }
                    break;
            }
        }

        public void GetSWMMObject(SWMMObject swmmObject)
        {
            switch (swmmObject.ObjectType)
            {
                case ObjectType.NODE:
                    {
                        Node node = (Node)swmmObject;
                        node.NativeNode = GetNodeById(node.ObjectId);
                    }
                    break;
                case ObjectType.LINK:
                    {
                        Link link = (Link)swmmObject;
                        link.NativeLink = GetLinkById(link.ObjectId);
                    }
                    break;
                case ObjectType.SUBCATCH:
                    {
                        SubCatchment subCatchment = (SubCatchment)swmmObject;
                        subCatchment.NativeSubCatchment = GetSubCatchmentById(subCatchment.ObjectId);
                    }
                    break;
            }
        }

        # endregion

    }
}
