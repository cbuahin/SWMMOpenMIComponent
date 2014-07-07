
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
using SWMMOpenMIComponent;

namespace SWMMOpenMIComponent
{
    public class SWMMOpenMIComponent : Identifiable, ITimeSpaceComponent
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
        FileInfo SWMMLibrary, projectFile, reportFile, outputFile;
        SWMM model;
        bool hasBeenInitialized = false;
        bool hasBeenPrepared = false;
        List<IBaseInput> consumers;
        IBaseOutput[] requiredOutputs;
        IBaseInput[] requiredInputs;
        SWMM.CallbackFunction routingCallback;

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

            adaptedOutputFactories = new List<IAdaptedOutputFactory>();
            
            adaptedOutputFactories.Add(new TimeBufferFactory("Time Interpolation and Extrapolation Adapter") 
            {
                Caption = "Time Interpolation and Extrapolation Adapter",
                Description = "Time Interpolation and Extrapolation Adapter"
                
            });
            
            adaptedOutputFactories.Add(new SpatialAdaptedOutputFactory("Spatial Adaptor Factory") 
            {
                Caption = "Time Interpolation and Extrapolation Adapter",
                Description = "Time Interpolation and Extrapolation Adapter"
            });

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
            InitializeInputExchangeItems();
            InitializeOutputExchangeItems();
            InitializeSpace();
            
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

            if (!hasBeenInitialized)
            {
                throw new Exception("Prepare method in the LinkableEngine cannot be invoked before the Initialize method has been invoked");
            }

            Status = LinkableComponentStatus.Preparing;

 
            RetrieveAllConsumers();
            RetrieveRequiredOutputs();
            RetrieveRequiredInputs();

            foreach (IBaseOutput output in requiredOutputs)
            {
                foreach (IBaseAdaptedOutput adaptedOutput in output.AdaptedOutputs)
                {
                    adaptedOutput.Initialize();
                }
            }


            // Prepare the engine
            model.StartModel();
            routingCallback = UpdateRequiredInputExchangeItems;
            model.SetRoutingCallback(routingCallback);



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

            if (Status != LinkableComponentStatus.Done)
            {
                DateTime earliestTimeDesired = GetEarliestTimeRequiredByOutput();

                if (earliestTimeDesired > model.CurrentDateTime && earliestTimeDesired < model.EndDateTime)
                {
                    while (model.CurrentDateTime <= earliestTimeDesired)
                    {
                        model.PerformTimeStep();
                    }
                }
                else
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
            }


            Status = model.CurrentDateTime >= timeExtent.TimeHorizon.End().ToDateTime() ?
                (OutputItemsStillRequireData() ? LinkableComponentStatus.Done : LinkableComponentStatus.Finishing)
                : LinkableComponentStatus.Updated;
        }

        public void Finish()
        {
            Status = LinkableComponentStatus.Finishing;

            model.EndRun();
            model.CloseModel();
            model.Dispose();

            hasBeenInitialized = false;
            hasBeenPrepared = false;


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
            timeExtent.Times.Add(new Time(model.StartDateTime));
            timeExtent.TimeHorizon = new Time(model.StartDateTime)
            {
                DurationInDays = (model.EndDateTime - model.StartDateTime).TotalDays
            };
        }

        void InitializeSpace()
        {

            #region read coordinates

            string currentFlag = "";
            string[] delim = new string[] { "\t", " ", "," };

            Dictionary<ObjectType, Dictionary<string, List<Coordinate>>> coordinates = new Dictionary<ObjectType, Dictionary<string, List<Coordinate>>>();
            coordinates.Add(ObjectType.SUBCATCH, new Dictionary<string, List<Coordinate>>());
            coordinates.Add(ObjectType.NODE, new Dictionary<string, List<Coordinate>>());
            coordinates.Add(ObjectType.LINK, new Dictionary<string, List<Coordinate>>());
            coordinates.Add(ObjectType.GAGE, new Dictionary<string, List<Coordinate>>());

            using (StreamReader reader = new StreamReader(projectFile.FullName))
            {
                string line;
                Dictionary<string, List<Coordinate>> value = null;
                while ((line = reader.ReadLine()) != null)
                {
                    line = line.Trim();

                    switch (line)
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
                            if (!string.IsNullOrEmpty(line) && !string.IsNullOrWhiteSpace(line) && line.Length > 0 && line[0] != ';' && value != null)
                            {
                                string[] columns = line.Split(delim, StringSplitOptions.RemoveEmptyEntries);

                                if (columns.Length == 3)
                                {
                                    string name = columns[0];
                                    double x, y;


                                    if (double.TryParse(columns[1], out x) && double.TryParse(columns[2], out y))
                                    {
                                        Coordinate coordinate = new Coordinate(x, y, 0);

                                        if (value.ContainsKey(name))
                                        {
                                            value[name].Add(coordinate);
                                        }
                                        else
                                        {
                                            List<Coordinate> coords = new List<Coordinate>();
                                            coords.Add(coordinate);


                                            value.Add(name, coords);
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

            
                    switch (input.ObjectType)
                    {
                        case ObjectType.NODE:

                          
                                for (int i = 0; i < input.SWMMObjects.Count; i++)
                                {
                                    if (i == 0)
                                    {
                                        input.ElementSet.ElementType = ElementType.Point;
                                    }

                                    SWMMObjectIdentifier id = input.SWMMObjects[i];

                                    if (value.ContainsKey(id.ObjectId))
                                    {
                                        input.ElementSet.Elements[i].Vertices = value[id.ObjectId].ToArray();
                                    }
                                }

                            break;

                        case ObjectType.LINK:

                            for (int i = 0; i < input.SWMMObjects.Count; i++)
                            {
                                if (i == 0)
                                {
                                    input.ElementSet.ElementType = ElementType.PolyLine;
                                }

                                SWMMObjectIdentifier id = input.SWMMObjects[i];

                                int n1 = model.Links[id.ObjectId].node1;
                                int n2 = model.Links[id.ObjectId].node2;

                                TNode node1 = model.GetNode(n1);
                                TNode node2 = model.GetNode(n2);

                                Coordinate c1 = coordinates[ObjectType.NODE][node1.ID][0];
                                Coordinate c2 = coordinates[ObjectType.NODE][node2.ID][0];

                                if (value.ContainsKey(id.ObjectId))
                                {
                                    List<Coordinate> vertices = value[id.ObjectId];
                                    Coordinate[] verts = new Coordinate[vertices.Count + 2];

                                    int last = verts.Length - 1;

                                    for (int v = 0; v < verts.Length; v++)
                                    {
                                        if(v == 0)
                                        {
                                            verts[0] = c1; 
                                        }
                                        else if(v == last)
                                        {
                                            verts[last] = c2;   
                                        }
                                        else
                                        {
                                            verts[v] = vertices[v - 1];
                                        }
                                    }

                                    input.ElementSet.Elements[i].Vertices = verts;
                                }
                                else
                                {
                                    input.ElementSet.Elements[i].Vertices = new Coordinate[] {c1,c2 };
                                }
                            }

                            break;
                    }
            }

            foreach (SWMMOutputExchangeItem output in outputs)
            {
                Dictionary<string, List<Coordinate>> value = coordinates[output.ObjectType];

                    switch (output.ObjectType)
                    {
                        case ObjectType.NODE:
                            if (value.Count > 0)
                            {
                                for (int i = 0; i < output.SWMMObjects.Count; i++)
                                {
                                    if (i == 0)
                                    {
                                        output.ElementSet.ElementType = ElementType.Point;
                                        output.ElementSet.HasZ = false;
                                    }

                                    SWMMObjectIdentifier id = output.SWMMObjects[i];

                                    if (value.ContainsKey(id.ObjectId))
                                    {
                                        output.ElementSet.Elements[i].Vertices = value[id.ObjectId].ToArray();
                                    }
                                }
                            }
                            break;

                        case ObjectType.LINK:
                            for (int i = 0; i < output.SWMMObjects.Count; i++)
                            {
                                if (i == 0)
                                {
                                    output.ElementSet.ElementType = ElementType.PolyLine;
                                    output.ElementSet.HasZ = false;
                                }

                                SWMMObjectIdentifier id = output.SWMMObjects[i];

                                int n1 = model.Links[id.ObjectId].node1;
                                int n2 = model.Links[id.ObjectId].node2;

                                TNode node1 = model.GetNode(n1);
                                TNode node2 = model.GetNode(n2);

                                Coordinate c1 = coordinates[ObjectType.NODE][node1.ID][0];
                                Coordinate c2 = coordinates[ObjectType.NODE][node2.ID][0];

                                if (value.ContainsKey(id.ObjectId))
                                {
                                    List<Coordinate> vertices = value[id.ObjectId];
                                    Coordinate[] verts = new Coordinate[vertices.Count + 2];

                                    int last = verts.Length - 1;

                                    for (int v = 0; v < verts.Length; v++)
                                    {
                                        if (v == 0)
                                        {
                                            verts[0] = c1;
                                        }
                                        else if (v == last)
                                        {
                                            verts[last] = c2;
                                        }
                                        else
                                        {
                                            verts[v] = vertices[v - 1];
                                        }
                                    }

                                    output.ElementSet.Elements[i].Vertices = verts;
                                }
                                else
                                {
                                    output.ElementSet.Elements[i].Vertices = new Coordinate[] { c1, c2 };
                                }
                            }

                            break;
                    }
            }
        }

        void InitializeInputExchangeItems()
        {
            inputs.Clear();
            InitializeInputNodes();
            InitializeInputLinks();
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

            KeyValuePair<string, SWMMVariableDefinition>[] parameters = SWMM.SWMMExposedVariables[ObjectType.NODE];


            for (int i = 0; i < parameters.Length; i++)
            {
                KeyValuePair<string, SWMMVariableDefinition> parameter = parameters[i];

                SWMMInputExchangeItem allInput = new SWMMInputExchangeItem()
                {
                    Id = parameter.Value.Description + " |  All Nodes",
                    Caption = parameter.Value.Description + " |  All Nodes",
                    PropertyName = parameter.Key,
                    ObjectType = ObjectType.NODE,
                    SWMMObjects = identifiers,
                    Description = parameter.Value.Description + " for all nodes",
                    CacheValues = false,
                    ValueDefinition = parameter.Value.ValueDefinition,
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
                        Id = parameter.Value.Description + " | Node = " + id.ObjectId,
                        Caption = parameter.Value.Description + " | Node = " + id.ObjectId,
                        PropertyName = parameter.Key,
                        ObjectType = ObjectType.NODE,
                        SWMMObjects = ids,
                        Description = parameter.Value.Description + " for node " + id.ObjectId,
                        CacheValues = false,
                        ValueDefinition = parameter.Value.ValueDefinition,
                        TimeSet = new TimeSet(timeExtent),
                        Component = this,
                    };

                    input.ElementSet.ElementType = ElementType.IdBased;
                    input.ElementSet.HasZ = false;

                    inputs.Add(input);
                }
            }
        }

        void InitializeInputLinks()
        {
            //Update input item requestedTime and 
            Dictionary<string, TLink> links = model.Links;
            List<string> keys = links.Keys.ToList<string>();
            List<SWMMObjectIdentifier> identifiers = new List<SWMMObjectIdentifier>();

            for (int i = 0; i < keys.Count; i++)
            {
                TLink link = links[keys[i]];
                SWMMObjectIdentifier identifier = new SWMMObjectIdentifier()
                {
                    ObjectId = link.ID,
                    ObjectIndex = i,
                    ObjectType = ObjectType.LINK,
                };

                identifiers.Add(identifier);
            }

            KeyValuePair<string, SWMMVariableDefinition>[] parameters = SWMM.SWMMExposedVariables[ObjectType.LINK];


            for (int i = 0; i < parameters.Length; i++)
            {
                KeyValuePair<string, SWMMVariableDefinition> parameter = parameters[i];

                SWMMInputExchangeItem allInput = new SWMMInputExchangeItem()
                {
                    Id = parameter.Value.Description + " |  All Links",
                    Caption = parameter.Value.Description + " |  All Links",
                    PropertyName = parameter.Key,
                    ObjectType = ObjectType.LINK,
                    SWMMObjects = identifiers,
                    Description = parameter.Value.Description + " for all Links",
                    CacheValues = false,
                    ValueDefinition = parameter.Value.ValueDefinition,
                    TimeSet = new TimeSet(timeExtent),
                    Component = this
                };

                allInput.ElementSet.ElementType = ElementType.IdBased;
                allInput.ElementSet.HasZ = false;
                inputs.Add(allInput);

                for (int j = 0; j < identifiers.Count; j++)
                {

                    SWMMObjectIdentifier id = identifiers[j];
                    List<SWMMObjectIdentifier> ids = new List<SWMMObjectIdentifier>(new SWMMObjectIdentifier[] { id });

                    SWMMInputExchangeItem input = new SWMMInputExchangeItem()
                    {
                        Id = parameter.Value.Description + " | Link = " + id.ObjectId,
                        Caption = parameter.Value.Description + " | Link = " + id.ObjectId,
                        PropertyName = parameter.Key,
                        ObjectType = ObjectType.LINK,
                        SWMMObjects = ids,
                        Description = parameter.Value.Description + " for Link " + id.ObjectId,
                        CacheValues = false,
                        ValueDefinition = parameter.Value.ValueDefinition,
                        TimeSet = new TimeSet(timeExtent),
                        Component = this,
                    };

                    input.ElementSet.ElementType = ElementType.IdBased;
                    input.ElementSet.HasZ = false;

                    inputs.Add(input);
                }
            }
        }
       
        void InitializeOutputExchangeItems()
        {
            outputs.Clear();
            InitializeOutputNodes();
            InitializeOutputLinks();
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

            KeyValuePair<string, SWMMVariableDefinition>[] parameters = SWMM.SWMMExposedVariables[ObjectType.NODE];


            for (int i = 0; i < parameters.Length; i++)
            {
                KeyValuePair<string, SWMMVariableDefinition> parameter = parameters[i];

                SWMMOutputExchangeItem all = new SWMMOutputExchangeItem()
                {
                    Id = parameter.Value.Description + " |  All Nodes",
                    Caption = parameter.Value.Description + " |  All Nodes",
                    PropertyName = parameter.Key,
                    ObjectType = ObjectType.NODE,
                    SWMMObjects = identifiers,
                    Description = parameter.Value.Description + " for all nodes",
                    CacheValues = false,
                    ValueDefinition = parameter.Value.ValueDefinition,
                    TimeSet = new TimeSet(timeExtent),
                    Component = this
                };

                all.ElementSet.ElementType = ElementType.IdBased;
                all.ElementSet.HasZ = false;

                outputs.Add(all);

                for (int j = 0; j < identifiers.Count; j++)
                {

                    SWMMObjectIdentifier id = identifiers[j];
                    List<SWMMObjectIdentifier> ids = new List<SWMMObjectIdentifier>(new SWMMObjectIdentifier[] { id });

                    SWMMOutputExchangeItem output = new SWMMOutputExchangeItem()
                    {
                        Id = parameter.Value.Description + " | Node = " + id.ObjectId,
                        Caption = parameter.Value.Description + " | Node = " + id.ObjectId,
                        PropertyName = parameter.Key,
                        ObjectType = ObjectType.NODE,
                        SWMMObjects = ids,
                        Description = parameter.Value.Description + " for node " + id.ObjectId,
                        CacheValues = false,
                        ValueDefinition = parameter.Value.ValueDefinition,
                        TimeSet = new TimeSet(timeExtent),
                        Component = this,
                    };
                    output.ElementSet.ElementType = ElementType.IdBased;
                    output.ElementSet.HasZ = false;
                    outputs.Add(output);
                }
            }
        }

        void InitializeOutputLinks()
        {
            //Update input item requestedTime and 
            Dictionary<string, TLink> links = model.Links;
            List<string> keys = links.Keys.ToList<string>();
            List<SWMMObjectIdentifier> identifiers = new List<SWMMObjectIdentifier>();

            for (int i = 0; i < keys.Count; i++)
            {
                TLink link = links[keys[i]];
                SWMMObjectIdentifier identifier = new SWMMObjectIdentifier()
                {
                    ObjectId = link.ID,
                    ObjectIndex = i,
                    ObjectType = ObjectType.LINK,
                };

                identifiers.Add(identifier);
            }

            KeyValuePair<string, SWMMVariableDefinition>[] parameters = SWMM.SWMMExposedVariables[ObjectType.LINK];


            for (int i = 0; i < parameters.Length; i++)
            {
                KeyValuePair<string, SWMMVariableDefinition> parameter = parameters[i];

                SWMMOutputExchangeItem all = new SWMMOutputExchangeItem()
                {
                    Id = parameter.Value.Description + " |  All Links",
                    Caption = parameter.Value.Description + " |  All Links",
                    PropertyName = parameter.Key,
                    ObjectType = ObjectType.LINK,
                    SWMMObjects = identifiers,
                    Description = parameter.Value.Description + " for all Links",
                    CacheValues = false,
                    ValueDefinition = parameter.Value.ValueDefinition,
                    TimeSet = new TimeSet(timeExtent),
                    Component = this
                };

                all.ElementSet.ElementType = ElementType.IdBased;
                all.ElementSet.HasZ = false;
                outputs.Add(all);

                for (int j = 0; j < identifiers.Count; j++)
                {

                    SWMMObjectIdentifier id = identifiers[j];
                    List<SWMMObjectIdentifier> ids = new List<SWMMObjectIdentifier>(new SWMMObjectIdentifier[] { id });

                    SWMMOutputExchangeItem output = new SWMMOutputExchangeItem()
                    {
                        Id = parameter.Value.Description + " | Link = " + id.ObjectId,
                        Caption = parameter.Value.Description + " | Link = " + id.ObjectId,
                        PropertyName = parameter.Key,
                        ObjectType = ObjectType.LINK,
                        SWMMObjects = ids,
                        Description = parameter.Value.Description + " for Link " + id.ObjectId,
                        CacheValues = false,
                        ValueDefinition = parameter.Value.ValueDefinition,
                        TimeSet = new TimeSet(timeExtent),
                        Component = this,
                    };
                    output.ElementSet.ElementType = ElementType.IdBased;
                    output.ElementSet.HasZ = false;
                    outputs.Add(output);
                }
            }
        }

        #endregion

        #region update functions

        void UpdateRequiredInputExchangeItems()
        {
            //parallel
            foreach (SWMMInputExchangeItem inputItem in requiredInputs)
            {
                int lastIndex = inputItem.TimeSet.Times.Count - 1;
                ITime time = timeExtent[lastIndex];
                inputItem.TimeSet.Times[lastIndex] = new Time(time);

                // get and store input
                inputItem.Values = (ITimeSpaceValueSet)inputItem.Provider.GetValues(inputItem);
                inputItem.UpdateModel(ref model);

            }
        }

        void UpdateRequiredOutputExchangeItems()
        {
            ITime current =  new Time(timeExtent.Times[timeExtent.Times.Count - 1]);

            for (int i = 0; i < requiredOutputs.Length; i++)
            {
                SWMMOutputExchangeItem output = (SWMMOutputExchangeItem)requiredOutputs[i];

                int lastIndex = output.TimeSet.Times.Count - 1;

                output.TimeSet.Times[lastIndex] = current;

                output.RetrieveFromModel(ref model);

                for (int j = 0; j < output.AdaptedOutputs.Count; j++)
                {
                    ITimeSpaceAdaptedOutput adaptedOutput = (ITimeSpaceAdaptedOutput)output.AdaptedOutputs[j];

                    // Only update adaptedOutputs that are actually active

                    lastIndex = adaptedOutput.TimeSet.Times.Count - 1;
                    adaptedOutput.TimeSet.Times[lastIndex] = current;
                    adaptedOutput.Refresh();
                }
            }
        }

        bool OutputItemsStillRequireData()
        {
            if (consumers != null)
            {
                foreach (IBaseInput consumer in consumers)
                {
                    if (consumer.Component.Status != LinkableComponentStatus.Done &&
                       consumer.Component.Status != LinkableComponentStatus.Finished &&
                       consumer.Component.Status != LinkableComponentStatus.Finishing
                       )
                    {
                        return true;
                    }

                }
            }

           return false;
        }

        DateTime GetEarliestTimeRequiredByOutput()
        {
            
            DateTime dateTimeTest = DateTime.MaxValue;

            foreach(ITimeSpaceInput input in consumers)
            {
                IList<ITime> times = input.TimeSet.Times;

                DateTime time = times[times.Count - 1].End().ToDateTime();

                if (time < dateTimeTest)
                    dateTimeTest = time;
            }

            return dateTimeTest;

        }

        #endregion

        void RetrieveAllConsumers()
        {
            consumers = new List<IBaseInput>();

            foreach(IBaseOutput output in outputs)
            {
                if(output.Consumers.Count > 0)
                {
                    consumers.AddRange(output.Consumers);
                }

                if(output.AdaptedOutputs.Count > 0)
                {
                    foreach(IBaseAdaptedOutput adaptedOutput in output.AdaptedOutputs)
                    {
                        GetAdaptedOutputsConsumers(adaptedOutput,ref consumers);
                    }
                }
            }
        }

        void RetrieveRequiredOutputs()
        {

            this.requiredOutputs = (from n in outputs
                                    where n.Consumers.Count > 0 || n.AdaptedOutputs.Count > 0
                                    select n).ToArray();
        }

        void RetrieveRequiredInputs()
        {
            requiredInputs = (from n in inputs
                             where n.Provider != null
                                  select n).ToArray();
        }

        void GetAdaptedOutputsConsumers(IBaseAdaptedOutput adaptedOutput , ref List<IBaseInput> consumers)
        {
            if(adaptedOutput.Consumers.Count > 0)
            {
                consumers.AddRange(adaptedOutput.Consumers);
            }

            foreach (IBaseAdaptedOutput adaptedOutputT in adaptedOutput.AdaptedOutputs)
            {
                GetAdaptedOutputsConsumers(adaptedOutputT, ref consumers);
            }
        }

        #endregion

        # endregion

        #region eventhandlers

        public event EventHandler<LinkableComponentStatusChangeEventArgs> StatusChanged;

        #endregion
    }
}
