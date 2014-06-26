
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
using Oatc.OpenMI.Sdk.Buffer;

namespace SWMMOpenMIComponent
{
    public class SWMMOpenMIComponent : Identifiable, ITimeSpaceComponent
    {
        # region variables

        LinkableComponentStatus status = LinkableComponentStatus.Created;
        List<SWMMInputExchangeItem> inputs;
        List<SWMMOutputExchangeItem> outputs;
        List<IAdaptedOutputFactory> adaptedOutputFactories;
        List<string> validationErrorMessages = null;
        Dictionary<string, IArgument> arguments;
        String[] requiredArguments;
        TimeSet timeExtent;
        bool disposed = false;
        FileInfo SWMMLibrary, projectFile, reportFile, outputFile;
        SWMM model;
        bool hasBeenInitialized = false, hasBeenPrepared = false;
        List<IAdaptedOutputFactory> factories;

        # endregion

        # region constructors

        public SWMMOpenMIComponent()
        {

            Id = "SWMM OpenMI 2.0 Model Component";
            Caption = "SWMM OpenMI 2.0 Model Component";
            Description = "SWMM OpenMI 2.0 Component";

            //initialize lists
            inputs = new List<SWMMInputExchangeItem>();
            outputs = new List<SWMMOutputExchangeItem>();
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

            factories = new List<IAdaptedOutputFactory>();
            factories.Add(new TimeBufferFactory("Time Interpolation and Extrapolation Adapter"));
            factories.Add(new SpatialAdaptedOutputFactory("Spatial Adaptor Factory"));

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
                return (IList<IBaseInput>)inputs;
            }
        }

        public IList<IBaseOutput> Outputs
        {
            get
            {
                return (IList<IBaseOutput>)outputs;
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
            InitializeInputExchangeItems();
            InitializeOutputExchangeItems();
            InitializeSpace();

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

            if (!hasBeenInitialized)
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

        #region initialization functions

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
                            Id = (string)argument.Value;
                            break;
                        case "Caption":
                            Caption = (string)argument.Value;
                            break;
                        case "Description":
                            Description = (string)argument.Value;
                            break;
                        case "SWMMLibrary":
                            {
                                FileInfo temp = (FileInfo)argument.Value;

                                if (File.Exists(temp.FullName))
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

                                if (!File.Exists(temp.FullName))
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
            timeExtent.HasDurations = true;
            timeExtent.Times.Add(new Time(model.StartDateTime) );
            timeExtent.TimeHorizon = new Time(model.StartDateTime) 
            {
                DurationInDays = (model.EndDateTime - model.StartDateTime).TotalDays
            };
        }

        void InitializeSpace()
        {

            #region read coordinates

            string currentFlag = "";
            string[] delim = new string[] { "\t"," ",","};

            Dictionary<ObjectType, Dictionary<string, List<Coordinate>>> coordinates = new Dictionary<ObjectType, Dictionary<string, List<Coordinate>>>();
            coordinates.Add(ObjectType.SUBCATCH, new Dictionary<string, List<Coordinate>>());
            coordinates.Add(ObjectType.NODE, new Dictionary<string, List<Coordinate>>());
            coordinates.Add(ObjectType.LINK, new Dictionary<string, List<Coordinate>>());
            coordinates.Add(ObjectType.GAGE, new Dictionary<string, List<Coordinate>>());

            using (StreamReader reader = new StreamReader(projectFile.FullName))
            {
                string line;
                Dictionary<string,List<Coordinate>> value = null; 
                while((line = reader.ReadLine()) != null)
                {
                    line = line.Trim();

                    switch(line)
                    {
                        case "[POLYGONS]":
                            currentFlag = line;
                            value = coordinates[ObjectType.SUBCATCH];

                            break;
                        case "[COORDINATES]":
                            currentFlag = line;
                            value = coordinates[ObjectType.NODE];

                            break;
                        case "[VERTICES]":
                            currentFlag = line;
                            value = coordinates[ObjectType.LINK];

                            break;
                        case "[SYMBOLS]":
                            currentFlag = line;
                            value = coordinates[ObjectType.GAGE];

                            break;
                        default:
                            if(!string.IsNullOrEmpty(line) && !string.IsNullOrWhiteSpace(line) && line.Length > 0 && line[0] != ';' && value != null)
                            {
                                string[] columns = line.Split(delim, StringSplitOptions.RemoveEmptyEntries);

                                if(columns.Length == 3)
                                {
                                    string name = columns[0];
                                    double x, y;


                                    if(double.TryParse(columns[1], out x) && double.TryParse(columns[2], out y))
                                    {
                                        Coordinate coordinate = new Coordinate(x, y, 0);

                                        if (value.ContainsKey(name))
                                        {
                                            if (value[name] == null)
                                            {
                                                List<Coordinate> ccs = new List<Coordinate>();
                                                ccs.Add(coordinate);
                                                value[name] = ccs;
                                            }
                                            else
                                            {
                                                value[name].Add(coordinate);
                                            }
                                        }
                                    }
                                }
                            }
                            break;
                    }
                }
            }

            # endregion

            foreach (SWMMInputExchangeItem input in inputs)
            {
                Dictionary<string, List<Coordinate>> value = coordinates[input.ObjectType];

                if (value.Count > 0)
                {
                    switch(input.ObjectType)
                    {
                        case ObjectType.NODE:

                            for (int i = 0 ; i < input.SWMMObjects.Count ; i++) 
                            {
                                if(i== 0)
                                {
                                    input.ElementSet.ElementType = ElementType.Point;
                                }

                                SWMMObjectIdentifier id = input.SWMMObjects[i];

                                if(value.ContainsKey(id.ObjectId))
                                {
                                    List<Coordinate> coords = value[id.ObjectId];
                                    
                                    input.ElementSet.Elements[i].Vertices = value[id.ObjectId].ToArray();
                                }
                            }

                            break;
                    }
                }
            }

            foreach (SWMMOutputExchangeItem output in outputs)
            {
                Dictionary<string, List<Coordinate>> value = coordinates[output.ObjectType];

                if (value.Count > 0)
                {
                    switch (output.ObjectType)
                    {
                        case ObjectType.NODE:

                            for (int i = 0; i < output.SWMMObjects.Count; i++)
                            {
                                if (i == 0)
                                {
                                    output.ElementSet.ElementType = ElementType.Point;
                                }

                                SWMMObjectIdentifier id = output.SWMMObjects[i];

                                if (value.ContainsKey(id.ObjectId))
                                {
                                    List<Coordinate> coords = value[id.ObjectId];
                                    output.ElementSet.Elements[i].Vertices = value[id.ObjectId].ToArray();
                                }
                            }

                            break;
                    }
                }
            }
        }

        void InitializeInputExchangeItems()
        {
            inputs.Clear();
            InitializeInputNodes();

        }

        void InitializeInputNodes()
        {
            //Update input item requestedTime and 
            Dictionary<string, TNode> nodes = model.Nodes;
            List<string> keys = nodes.Keys.ToList<string>();
            List<SWMMObjectIdentifier> identifiers = new List<SWMMObjectIdentifier>();

            for (int i = 0; i < keys.Count; i++)
            {
                TNode node = nodes[keys[i]];
                SWMMObjectIdentifier identifier = new SWMMObjectIdentifier()
                {
                    ObjectId = node.ID,
                    ObjectIndex = i,
                    ObjectType = ObjectType.NODE,
                };

                identifiers.Add(identifier);
            }

            KeyValuePair<string, KeyValuePair<string, IValueDefinition>>[] parameters = SWMM.SWMMVariableTypes[ObjectType.NODE];


            for (int i = 0; i < parameters.Length; i++)
            {
                KeyValuePair<string, KeyValuePair<string, IValueDefinition>> parameter = parameters[i];

                SWMMInputExchangeItem allInput = new SWMMInputExchangeItem()
                {
                    Id = parameter.Value.Key + " |  All Nodes",
                    Caption = parameter.Value.Key + " |  All Nodes",
                    PropertyName = parameter.Key,
                    ObjectType = ObjectType.NODE,
                    SWMMObjects = identifiers,
                    Description = parameter.Value.Key + " for all nodes",
                    CacheValues = false,
                    ValueDefinition = parameter.Value.Value,
                    TimeSet = new TimeSet(timeExtent),
                    Component = this
                };

                allInput.ElementSet.ElementType = ElementType.IdBased;
                inputs.Add(allInput);

                for (int j = 0; j < identifiers.Count; j++)
                {

                    SWMMObjectIdentifier id = identifiers[j];
                    List<SWMMObjectIdentifier> ids = new List<SWMMObjectIdentifier>(new SWMMObjectIdentifier[] { id });

                    SWMMInputExchangeItem input = new SWMMInputExchangeItem()
                    {
                        Id = parameter.Value.Key + " | Node = " + id.ObjectId,
                        Caption = parameter.Value.Key + " | Node = " + id.ObjectId,
                        PropertyName = parameter.Key,
                        ObjectType = ObjectType.NODE,
                        SWMMObjects = ids,
                        Description = parameter.Value.Key + " for node " + id.ObjectId,
                        CacheValues = false,
                        ValueDefinition = parameter.Value.Value,
                        TimeSet = new TimeSet(timeExtent),
                        Component = this,
                    };

                    input.ElementSet.ElementType = ElementType.IdBased;

                    inputs.Add(input);
                }
            }
        }

        void InitializeOutputExchangeItems()
        {
            outputs.Clear();
            InitializeOutputNodes();
        }

        void InitializeOutputNodes()
        {
            //Update input item requestedTime and 
            Dictionary<string, TNode> nodes = model.Nodes;
            List<string> keys = nodes.Keys.ToList<string>();
            List<SWMMObjectIdentifier> identifiers = new List<SWMMObjectIdentifier>();

            for (int i = 0; i < keys.Count; i++)
            {
                TNode node = nodes[keys[i]];
                SWMMObjectIdentifier identifier = new SWMMObjectIdentifier()
                {
                    ObjectId = node.ID,
                    ObjectIndex = i,
                    ObjectType = ObjectType.NODE,
                };

                identifiers.Add(identifier);
            }

            KeyValuePair<string, KeyValuePair<string, IValueDefinition>>[] parameters = SWMM.SWMMVariableTypes[ObjectType.NODE];


            for (int i = 0; i < parameters.Length; i++)
            {
                KeyValuePair<string, KeyValuePair<string, IValueDefinition>> parameter = parameters[i];

                SWMMOutputExchangeItem all = new SWMMOutputExchangeItem()
                {
                    Id = parameter.Value.Key + " |  All Nodes",
                    Caption = parameter.Value.Key + " |  All Nodes",
                    PropertyName = parameter.Key,
                    ObjectType = ObjectType.NODE,
                    SWMMObjects = identifiers,
                    Description = parameter.Value.Key + " for all nodes",
                    CacheValues = false,
                    ValueDefinition = parameter.Value.Value,
                    TimeSet = timeExtent,
                    Component = this
                };

                all.ElementSet.ElementType = ElementType.IdBased;
                outputs.Add(all);

                for (int j = 0; j < identifiers.Count; j++)
                {

                    SWMMObjectIdentifier id = identifiers[j];
                    List<SWMMObjectIdentifier> ids = new List<SWMMObjectIdentifier>(new SWMMObjectIdentifier[] { id });

                    SWMMOutputExchangeItem output = new SWMMOutputExchangeItem()
                    {
                        Id = parameter.Value.Key + " | Node = " + id.ObjectId,
                        Caption = parameter.Value.Key + " | Node = " + id.ObjectId,
                        PropertyName = parameter.Key,
                        ObjectType = ObjectType.NODE,
                        SWMMObjects = ids,
                        Description = parameter.Value.Key + " for node " + id.ObjectId,
                        CacheValues = false,
                        ValueDefinition = parameter.Value.Value,
                        TimeSet = timeExtent,
                        Component = this,
                    };
                    output.ElementSet.ElementType = ElementType.IdBased;
                    outputs.Add(output);
                }
            }
        }

        #endregion

        #region update functions

        void UpdateRequiredInputExchangeItems()
        {
            //parallel
            foreach (SWMMInputExchangeItem inputItem in inputs)
            {
                int lastIndex =  inputItem.TimeSet.Times.Count -1;
                ITime time  = timeExtent[lastIndex];
                inputItem.TimeSet.Times[lastIndex] = new Time(time);

                if (inputItem.Provider != null)
                {
                    // get and store input
                    inputItem.Values = (ITimeSpaceValueSet)inputItem.Provider.GetValues(inputItem);
                    inputItem.UpdateModel(ref model);
                }
            }
        }

        void UpdateRequiredOutputExchangeItems()
        {
            foreach (SWMMOutputExchangeItem output in outputs)
            {
                if (output.Consumers.Count > 0 || output.AdaptedOutputs.Count > 0)
                {
                    output.RetrieveFromModel(ref model);

                    foreach (ITimeSpaceAdaptedOutput adaptedOutput in output.AdaptedOutputs)
                    {
                        // Only update adaptedOutputs that are actually active
                        if (adaptedOutput.Consumers.Count > 0 || adaptedOutput.AdaptedOutputs.Count > 0)
                        {
                            adaptedOutput.Refresh();
                        }
                    }

                }
            }
        }

        #endregion

        bool OutputItemsStillRequireData()
        {
            bool item = true;

            return item;
        }

        #endregion

        # endregion

        #region eventhandlers

        public event EventHandler<LinkableComponentStatusChangeEventArgs> StatusChanged;

        #endregion
    }
}
