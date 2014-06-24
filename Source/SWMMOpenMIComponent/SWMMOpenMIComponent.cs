
/****************************************************************************
**
**  Copyright (C) 2014 Caleb Amoa Buahin
**  Contact: calebgh@gmail.com
** 
**  This file is part of SWMMOpenMIComponent.dll, a SWMM OpenMI 2.0 TimeSpaceLinkableCompont
**
**  SWMMOpenMIComponent.dll and its associated files is free software; you can redistribute it and/or modify
**  it under the terms of the Lesser GNU General Public License as published by
**  the Free Software Foundation; either version 3 of the License, or
**  (at your option) any later version.
**
**  SWMMOpenMIComponent.dll and its associated files is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  Lesser GNU General Public License for more details.
**
**  You should have received a copy of the Lesser GNU General Public License
**  along with this program.  If not, see <http://www.gnu.org/licenses/>
**
****************************************************************************/

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Oatc.OpenMI.Sdk;
using Oatc.OpenMI.Sdk.Backbone;
using OpenMI.Standard2;
using OpenMI.Standard2.TimeSpace;
using System.IO;
using Oatc.OpenMI.Sdk.Spatial;
using System.Threading;

namespace SWMMOpenMIComponent
{
    public class SWMMOpenMIComponent : Identifiable , ITimeSpaceComponent
    {
        # region variables

        LinkableComponentStatus status = LinkableComponentStatus.Created;
        List<IBaseInput> inputs;
        List<IBaseOutput> outputs;
        List<IAdaptedOutputFactory> adaptedOutputFactories;
        List<string> validationErrorMessages = null;
        Dictionary<string, IArgument> arguments;
        String[] requiredArguments;
        TimeSet timeExtent;
        bool disposed = false;
        FileInfo SWMMLibrary, projectFile, reportFile, outputFile;
        SWMM model;
        bool hasBeenInitialized = false, hasBeenPrepared = false;

        # endregion

        # region constructors

        public SWMMOpenMIComponent()
        {

            Id = "SWMM OpenMI 2.0 Model Component";
            Caption = "SWMM OpenMI 2.0 Model Component";
            Description = "SWMM OpenMI 2.0 Component";

            //initialize lists
            inputs = new List<IBaseInput>();
            outputs = new List<IBaseOutput>();
            adaptedOutputFactories = new List<IAdaptedOutputFactory>();
            validationErrorMessages = new List<string>();
            arguments = new Dictionary<string, IArgument>();

            //Initialize simulation duration
            timeExtent = new TimeSet();
            DateTime date = DateTime.Now;
            timeExtent.TimeHorizon = new Time(date)
            {
                DurationInDays = 5
            };

            timeExtent.Times.Add(new Time(date));

            requiredArguments = new String[] { "ModelID", "Caption", "Description", "SWMMLibrary", "ProjectFile", "ReportFile", "OutputFile" };
            //Initialize arguments
            arguments.Add("ModelID", new ArgumentString("ModelID") { Caption = "ModelID", Description = "SWMM Model Component Identifier", DefaultValue = "SWMM Model Component", IsOptional = false, IsReadOnly = true, Value = "SWMM Model " + Guid.NewGuid() });
            arguments.Add("Caption", new ArgumentString("Caption") { Caption = "ModelCaption", Description = "SWMM Model Component Caption", DefaultValue = "SWMM OpenMI 2.0 Component", IsOptional = false, IsReadOnly = true, Value = "SWMM OpenMI 2.0 Component" });
            arguments.Add("Description", new ArgumentString("Description") { Caption = "ModelDescription", Description = "SWMM Model Description", DefaultValue = "SWMM OpenMI 2.0 Component Model", IsOptional = false, IsReadOnly = true, Value = "SWMM OpenMI 2.0 Component Model" });
            arguments.Add("SWMMLibrary", new ArgumentFileInfo("SWMMLibrary") { Caption = "SWMM Custom Library File", Description = "Path to custom SWMM Library Developed to Expose More of the internal functions of the SWMM Model", IsOptional = false, IsReadOnly = true });
            arguments.Add("ProjectFile", new ArgumentFileInfo("ProjectFile") { Caption = "SWMM Project File", Description = "SWMM project file usually with .inp extension", IsOptional = false, IsReadOnly = true });
            arguments.Add("ReportFile", new ArgumentFileInfo("ReportFile") { Caption = "SWMM Report File", Description = "SWMM report file usually with .rpt extension", IsOptional = false, IsReadOnly = true });
            arguments.Add("OutputFile", new ArgumentFileInfo("OutputFile") { Caption = "SWMM Output File", Description = "SWMM output file usually with .out extension", IsOptional = false, IsReadOnly = true });
        }

        #endregion

        # region properties

        #region baselinkablecomponent

        public IList<IArgument> Arguments
        {
            get
            {
                return arguments.Values.ToList();
            }
            set
            {
                for (int i = 0; i < value.Count(); i++)
                {
                    IArgument argument = value[i];

                    if (arguments.ContainsKey(argument.Id))
                    {
                        arguments[argument.Id] = argument;
                    }
                    else
                    {
                        throw new ArgumentException(argument.Id + " is not a required input for " + GetType().Name, argument.Id);
                    }
                }
            }
        }

        public LinkableComponentStatus Status
        {
            get
            {
                return status;
            }

            set
            {
                LinkableComponentStatus oldStatus = status;
                status = value;

                if (StatusChanged != null)
                {
                    LinkableComponentStatusChangeEventArgs args = new LinkableComponentStatusChangeEventArgs { LinkableComponent = this, OldStatus = oldStatus, NewStatus = value };
                    StatusChanged(this, args);
                }
            }
        }
        public IList<IBaseInput> Inputs
        {
            get
            {
                return inputs;
            }
        }

        public IList<IBaseOutput> Outputs
        {
            get
            {
                return outputs;
            }
        }

        public List<IAdaptedOutputFactory> AdaptedOutputFactories
        {
            get
            {
                return adaptedOutputFactories;
            }
        }


        #endregion

        #region timespacecomponent

        public ITimeSet TimeExtent
        {
            get
            {
                return timeExtent;
            }
        }

        #endregion

        #region swmm
        internal SWMM Model
        {
            get { return model; }
        }

        #endregion

        #endregion

        # region functions

        #region baselinkablecomponent

        public void Initialize()
        {

            //Initialize
            Status = LinkableComponentStatus.Initializing;

            //initialize arguments
            InitializeArguments();
            InitializeSWMMLibrary();
            InitializeTime();


            Quantity flow = new Quantity("TestValue");

            LineString fullRiverLineString = new LineString
            {
                Coordinates = GeometryFactory.CreateCoordinateList(new double[] { 1.0, 2.0 }, new double[] { 1.0, 2.0 }),
                Caption = "WholeRiver",
                Description = "WholeRiver",
                IsClosed = false,
                IsNodeBased = false,
            };

            IElementSet fullRiverElementSet = new LineStringWrapper(fullRiverLineString);


            Output p = new Output("TestOutput", flow, fullRiverElementSet);
            p.TimeSet = this.timeExtent;
            outputs.Add(p);

            hasBeenInitialized = true;
            Status = LinkableComponentStatus.Initialized;
        }

        public string[] Validate()
        {
            if (!hasBeenInitialized)
            {
                throw new Exception("Prepare method in the LinkableEngine cannot be invoked before the Initialize method has been invoked");
            }

            // catch internal swmm validation errors
            return new String[] { };
        }

        public void Prepare()
        {
            
            if(!hasBeenInitialized)
            {
                throw new Exception("Prepare method in the LinkableEngine cannot be invoked before the Initialize method has been invoked");
            }

            Status = LinkableComponentStatus.Preparing;

            // Prepare the engine
            model.StartModel();


            hasBeenPrepared = true;

            Status = LinkableComponentStatus.Updated;
        }

        public void Update(params IBaseOutput[] requiredOutput)
        {
            if (Status == LinkableComponentStatus.Updating)
            {
                return;
            }

            Status = LinkableComponentStatus.Updating;

            UpdateRequiredInputExchangeItems();


            if (Status != LinkableComponentStatus.Done)
            {
                model.PerformTimeStep();
            }

            ITime time = new Time(model.CurrentDateTime) { DurationInDays = 0 };

            lock (timeExtent.Times)
            {
                if (timeExtent.Times.Count == 0)
                {
                    timeExtent.Times.Add(time);
                }
                else if (timeExtent.Count == 1)
                {
                    timeExtent.Times[0] = time;
                }
                else
                {
                    timeExtent.Times.Clear();
                    timeExtent.Times.Add(time);
                }
            }


            UpdateRequiredOutputExchangeItems();

            Status = model.CurrentDateTime >= timeExtent.TimeHorizon.End().ToDateTime() ? 
                OutputItemsStillRequireData() ? LinkableComponentStatus.Finishing : LinkableComponentStatus.Done 
                : LinkableComponentStatus.Updated;
        }

        public void Finish()
        {
            Status = LinkableComponentStatus.Finishing;
            
            model.EndRun();
            model.CloseModel();

            Status = LinkableComponentStatus.Finished;
        }

        #endregion


        #region swmm

        void InitializeArguments()
        {
            for (int i = 0; i < requiredArguments.Length; i++)
            {
                string kid = requiredArguments[i];

                if (arguments.ContainsKey(kid))
                {
                    IArgument argument = arguments[kid];

                    switch (kid)
                    {
                        case "ModelID":
                            id = (string)argument.Value;
                            break;
                        case "Caption":
                            caption = (string)argument.Value;
                            break;
                        case "Description":
                            description = (string)argument.Value;
                            break;
                        case "SWMMLibrary":
                            {
                                FileInfo temp = (FileInfo)argument.Value;
                                
                                if(File.Exists(temp.FullName))
                                {
                                    Guid tempID = Guid.NewGuid();
                                    string newFileName = temp.FullName.Replace(temp.Extension, tempID.ToString() + temp.Extension);
                                    File.Copy(temp.FullName, newFileName);
                                    SWMMLibrary = new FileInfo(newFileName);
                                }
                                else
                                {
                                    throw new FileNotFoundException("Native SWMM Library was not found", temp.FullName);
                                }
                            }
                            break;
                        case "ProjectFile":
                            {
                                FileInfo temp = (FileInfo)argument.Value;

                                if(!File.Exists(temp.FullName))
                                {
                                    throw new FileNotFoundException("SWMM Input project file is required", temp.FullName);
                                }

                                projectFile = temp;
                            }
                            break;
                        case "ReportFile":
                            {
                                FileInfo temp = (FileInfo)argument.Value;
                                reportFile = temp;

                            }
                            break;
                        case "OutputFile":
                            {
                                FileInfo temp = (FileInfo)argument.Value;

                                if (string.IsNullOrEmpty(temp.FullName))
                                {
                                    throw new Exception("Output file cannot be invalid");
                                }

                                outputFile = temp;
                            }
                            break;
                    }
                }
            }

        }

        void InitializeSWMMLibrary()
        {
            //make copy of library
            model = new SWMM(SWMMLibrary, projectFile.FullName, outputFile.FullName, reportFile.FullName);
            model.Initialize();
        }

        void InitializeTime()
        {
            timeExtent.Times.Clear();
            timeExtent.Times.Add(new Time(model.StartDateTime) { DurationInDays = 0 });
            timeExtent.TimeHorizon = new Time(model.StartDateTime) { DurationInDays = (model.EndDateTime - model.StartDateTime).TotalDays };
        }

        void InitializeSpace()
        {

        }

        void initializeExchangeItems()
        {

        }

        void InitializeInputExchangeItems()
        {
            //Update input item requestedTime and 
        }

        void UpdateRequiredInputExchangeItems()
        {
            foreach (ITimeSpaceInput inputItem in Inputs)
            {
                if (inputItem.Provider != null)
                {
                    // get and store input
                    inputItem.Values = (ITimeSpaceValueSet)inputItem.Provider.GetValues(inputItem);
                }
            }
        }

        void InitializeOutputExchangeItems()
        {


        }

        void UpdateRequiredOutputExchangeItems()
        {

        }

        bool OutputItemsStillRequireData()
        {
            return false;
        }

        #endregion

        # endregion

        #region eventhandlers

        public event EventHandler<LinkableComponentStatusChangeEventArgs> StatusChanged;

        #endregion

    }
}
