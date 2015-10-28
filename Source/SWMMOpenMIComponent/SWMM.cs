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
    /// <remarks>Responsible for managing the SWMM engine library. It marshalls SWMM objects via Platform invoke</remarks>
    public class SWMM
    {
        # region SWMMDelegates


        delegate IntPtr OpenDelegate([MarshalAs(UnmanagedType.LPStr)]string inputFile, [MarshalAs(UnmanagedType.LPStr)]string reportFile, [MarshalAs(UnmanagedType.LPStr)]string outFile);

        delegate int StartModelDelegate(IntPtr project, int saveResults);

        delegate int PerformTimeStepDelegate(IntPtr project, out double elapsedTime);

        delegate int IntReturnDelegate(IntPtr project);

        delegate string GetErrorMessageDelegate(int errorCode);

        delegate void DecodeDateTimeDelegate(double dateTime, out int year, out int month, out int day, out int hour, out int minute, out int second);

        delegate double GetDateTimeDelegate(IntPtr project, [MarshalAs(UnmanagedType.LPStr)] string name);

        public delegate int GetObjectTypeCountDelegate(IntPtr project, ObjectType type);

        delegate IntPtr GetObjectDelegate(IntPtr project, int index);

        delegate IntPtr GetObjectByIdDelegate(IntPtr project, [MarshalAs(UnmanagedType.LPStr)]string id);

        delegate void SetObjectValue(IntPtr project, [MarshalAs(UnmanagedType.LPStr)]string objectId, [MarshalAs(UnmanagedType.LPStr)]string propertyName, double value);


        # endregion

        # region Variables

        FileInfo library;
        String inputFile, outPutFile, reportFile;
        bool disposed = false;
        DateTime startDateTime, endDateTime, currentDateTime;
        double startDateTimeD, endDateTimeD;

        //Library
        static IntPtr hModule;
        IntPtr projectPointer;

        # region Delegates

        //Internal delegates
        OpenDelegate open;
        StartModelDelegate startModel;
        PerformTimeStepDelegate performTimeStep;
        IntReturnDelegate endRun;
        IntReturnDelegate closeModel;
        IntReturnDelegate reportModelResults;
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

            endRun = WinLibraryLoader.LoadFunction<IntReturnDelegate>(ref hModule, "swmm_end");
            CheckIfLibraryError();

            reportModelResults = WinLibraryLoader.LoadFunction<IntReturnDelegate>(ref hModule, "swmm_report");
            CheckIfLibraryError();

            closeModel = WinLibraryLoader.LoadFunction<IntReturnDelegate>(ref hModule, "swmm_close");
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

            if (projectPointer != IntPtr.Zero)
            {
                bool op = true;
                int error = startModel(projectPointer, Convert.ToInt32(op));
                SetError(error);
            }
            else
            {
                throw new Exception("Current Project is Invalid");
            }
        }

        public void Initialize()
        {


            projectPointer = open(inputFile, reportFile, outPutFile);


            if (projectPointer != IntPtr.Zero)
            {

                StartModel();

                int year, month, day, hour, minute, second;
                year = month = day = hour = minute = second = 0;


                endDateTimeD = getDateTime(projectPointer, "end");
                decodeDateTime(endDateTimeD, out year, out month, out day, out hour, out minute, out second);
                endDateTime = new DateTime(year, month, day, hour, minute, second);

                startDateTimeD = getDateTime(projectPointer, "begin");
                decodeDateTime(startDateTimeD, out year, out month, out day, out hour, out minute, out second);
                startDateTime = new DateTime(year, month, day, hour, minute, second);

                currentDateTime = startDateTime;


                //Initialize Objects
                InitializeNodes();
                InitializeLinks();
                InitializeCatchments();
            }
            else
            {
                throw new Exception("Looding input files failed");
            }


        }

        void InitializeNodes()
        {
            nodes = new Dictionary<string, Node>();
            nodeIndexes = new Dictionary<int, string>();

            int nodeCount = GetObjectTypeCount(projectPointer, ObjectType.NODE);

            for (int i = 0; i < nodeCount; i++)
            {
                Node node = new Node()
                {
                    ObjectIndex = i,
                };

                TNode tnode = GetNode(i);
                node.NativeNode = tnode;
                node.ObjectIndex = i;
                string id = Marshal.PtrToStringAnsi(tnode.ID);
                nodes.Add(id, node);
                nodeIndexes.Add(i, id);
            }

        }

        void InitializeLinks()
        {
            links = new Dictionary<string, Link>();

            int linkCount = GetObjectTypeCount(projectPointer, ObjectType.LINK);

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

            int subCatchCount = GetObjectTypeCount(projectPointer, ObjectType.SUBCATCH);

            for (int i = 0; i < subCatchCount; i++)
            {
                SubCatchment subCatch = new SubCatchment()
                {
                    ObjectIndex = i,
                };

                TSubcatch tsub = GetSubCatchment(i);
                subCatch.NativeSubCatchment = tsub;
                subCatch.ObjectIndex = i;
                subCatchments.Add(Marshal.PtrToStringAnsi(tsub.ID), subCatch);
            }
        }

        public bool PerformTimeStep()
        {
            double timeElapsed = currentDateTime.ToOADate();
            int error = performTimeStep(projectPointer, out timeElapsed);
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
            
            int error = endRun(projectPointer);
            SetError(error);

            error = reportModelResults(projectPointer);
            SetError(error);
        }

        public void CloseModel()
        {

            int error = closeModel(projectPointer);
            SetError(error);
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
                Win32Exception except = new Win32Exception(error);
            }
        }

        public TNode GetNode(int index)
        {
            IntPtr nodePtr = getNode(projectPointer, index);
            TNode node = (TNode)Marshal.PtrToStructure(nodePtr, typeof(TNode));
            return node;
        }

        public TNode GetNodeById(string id)
        {
            IntPtr nodePtr = getNodeById(projectPointer, id);
            TNode node = (TNode)Marshal.PtrToStructure(nodePtr, typeof(TNode));
            return node;
        }

        public TLink GetLink(int index)
        {
            IntPtr linkPtr = getLink(projectPointer, index);
            TLink link = (TLink)Marshal.PtrToStructure(linkPtr, typeof(TLink));
            return link;
        }

        public TLink GetLinkById(string id)
        {
            IntPtr linkPtr = getLinkById(projectPointer, id);
            TLink link = (TLink)Marshal.PtrToStructure(linkPtr, typeof(TLink));
            return link;
        }

        public TSubcatch GetSubCatchment(int index)
        {
            IntPtr subCatchPtr = getSubCatchment(projectPointer, index);
            TSubcatch subCatch = (TSubcatch)Marshal.PtrToStructure(subCatchPtr, typeof(TSubcatch));
            return subCatch;
        }

        public TSubcatch GetSubCatchmentById(string id)
        {
            IntPtr subCatchPtr = getSubCatchmentById(projectPointer, id);
            TSubcatch subCatch = (TSubcatch)Marshal.PtrToStructure(subCatchPtr, typeof(TSubcatch));
            return subCatch; ;
        }

        public void UpdateSWMMObject(SWMMObject swmmObject, string propertyName)
        {
            switch (swmmObject.ObjectType)
            {
                case ObjectType.NODE:
                    {
                        Node node = (Node)swmmObject;
                        double value = (double)typeof(TNode).GetField(propertyName).GetValue(node.NativeNode);
                        setNodeObjectValue(projectPointer, node.ObjectId, propertyName, value);

                    }
                    break;
                case ObjectType.LINK:
                    {
                        Link link = (Link)swmmObject;
                        double value = (double)typeof(TLink).GetField(propertyName).GetValue(link.NativeLink);
                        setLinkObjectValue(projectPointer, link.ObjectId, propertyName, value);
                    }
                    break;
                case ObjectType.SUBCATCH:
                    {
                        SubCatchment subCatch = (SubCatchment)swmmObject;
                        double value = (double)typeof(TSubcatch).GetField(propertyName).GetValue(subCatch.NativeSubCatchment);
                        setSubCatchmentObjectValue(projectPointer, subCatch.ObjectId, propertyName, value);
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


    [CLSCompliant(true)]
    static class WinLibraryLoader
    {
        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern IntPtr LoadLibrary(string library);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern IntPtr GetProcAddress(IntPtr hModule, string functionName);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool FreeLibrary(IntPtr hModule);

        public static T LoadFunction<T>(ref IntPtr hModule, string functionName) where T : class
        {
            IntPtr address = GetProcAddress(hModule, functionName);
            System.Delegate functionPointer = Marshal.GetDelegateForFunctionPointer(address, typeof(T));
            return functionPointer as T;
        }

        public static void SetValueForValueType<T>(this FieldInfo field, ref T item, object value) where T : struct
        {
            field.SetValueDirect(__makeref(item), value);
        }
    }

}
