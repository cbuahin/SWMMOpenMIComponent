
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
using System.ComponentModel;
using System.Reflection;
using System.Runtime.InteropServices;

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
        bool isInitialized = false, isPrepared = false, isDisposed = false;

        //caching objects
        List<IBaseInput> consumers;
        IBaseOutput[] requiredOutputs;
        IBaseInput[] requiredInputs;

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

            adaptedOutputFactories.Add(new SWMMInterpolationAdaptedOutputFactory());

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
                if (status != value)
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

        public SWMM Model
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

            isInitialized = true;

            Status = LinkableComponentStatus.Initialized;
        }

        public string[] Validate()
        {
            if (!isInitialized)
            {
                throw new Exception("Prepare method in the LinkableEngine cannot be invoked before the Initialize method has been invoked");
            }

            // catch internal swmm validation errors
            return new String[] { };
        }

        public void Prepare()
        {
            if (!isInitialized)
            {
                throw new Exception("Prepare method in the LinkableEngine cannot be invoked before the Initialize method has been invoked");
            }

            Status = LinkableComponentStatus.Preparing;

            RetrieveAllConsumers();
            RetrieveRequiredOutputs();
            RetrieveRequiredInputs();

            for (int i = 0; i < requiredOutputs.Length; i++)
            {
                IBaseOutput output = requiredOutputs[i];

                for (int j = 0; j < output.AdaptedOutputs.Count; j++)
                {
                    IBaseAdaptedOutput adaptedOutput = output.AdaptedOutputs[j];
                    adaptedOutput.Initialize();
                }
            }

            // Prepare the engine
            //model.StartModel();

            UpdateRequiredOutputExchangeItems();

            isPrepared = true;
            Status = LinkableComponentStatus.Updated;
        }

        public void Update(params IBaseOutput[] requiredOutput)
        {
            if (status == LinkableComponentStatus.Updating)
            {
                return;
            }

            if (status != LinkableComponentStatus.Done)
            {
                Status = LinkableComponentStatus.Updating;

                UpdateRequiredInputExchangeItems();

                //DateTime earliestTimeRequired = GetEarliestTimeRequiredConsumers(requiredOutput);


                //while (model.CurrentDateTime <= earliestTimeRequired)
                //{
                    model.PerformTimeStep();
                //}


                Time time = timeExtent.Times[0] as Time;
                time.StampAsModifiedJulianDay = model.CurrentDateTime.ToModifiedJulianDay();

                UpdateRequiredOutputExchangeItems(requiredOutput);
            }


            Status = model.CurrentDateTime >= model.EndDateTime ?   LinkableComponentStatus.Finishing : LinkableComponentStatus.Updated;

            //Status = model.CurrentDateTime >= model.EndDateTime ?
            //               (OutputItemsStillRequireData() ? LinkableComponentStatus.Done : LinkableComponentStatus.Finishing)
            //               : LinkableComponentStatus.Updated;
        }

        public void Finish()
        {
            Status = LinkableComponentStatus.Finishing;

            model.EndRun();

            model.CloseModel();

            isInitialized = false;
            isPrepared = false;

            Status = LinkableComponentStatus.Finished;
        }

        #endregion

        #region IDisposable


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
                                    SWMMLibrary = temp;

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

                    switch (line.ToUpper())
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

            # region input geometry

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

                            SWMMObject id = input.SWMMObjects[i];

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

                            SWMMObject id = input.SWMMObjects[i];

                            int n1 = model.Links[id.ObjectId].NativeLink.node1;
                            int n2 = model.Links[id.ObjectId].NativeLink.node2;

                            TNode node1 = model.GetNode(n1);
                            TNode node2 = model.GetNode(n2);

                            Coordinate c1 = coordinates[ObjectType.NODE][Marshal.PtrToStringAnsi(node1.ID)][0];
                            Coordinate c2 = coordinates[ObjectType.NODE][Marshal.PtrToStringAnsi(node2.ID)][0];

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

                                input.ElementSet.Elements[i].Vertices = verts;
                            }
                            else
                            {
                                input.ElementSet.Elements[i].Vertices = new Coordinate[] { c1, c2 };
                            }
                        }

                        break;

                    case ObjectType.SUBCATCH:


                        for (int i = 0; i < input.SWMMObjects.Count; i++)
                        {
                            if (i == 0)
                            {
                                input.ElementSet.ElementType = ElementType.Polygon;
                            }

                            SWMMObject id = input.SWMMObjects[i];

                            if (value.ContainsKey(id.ObjectId))
                            {
                                input.ElementSet.Elements[i].Vertices = value[id.ObjectId].ToArray();
                            }
                        }

                        break;
                }
            }


            #endregion

            #region output geometry

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

                                SWMMObject id = output.SWMMObjects[i];

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

                            SWMMObject id = output.SWMMObjects[i];

                            int n1 = model.Links[id.ObjectId].NativeLink.node1;
                            int n2 = model.Links[id.ObjectId].NativeLink.node2;

                            TNode node1 = model.GetNode(n1);
                            TNode node2 = model.GetNode(n2);

                            Coordinate c1 = coordinates[ObjectType.NODE][Marshal.PtrToStringAnsi(node1.ID)][0];
                            Coordinate c2 = coordinates[ObjectType.NODE][Marshal.PtrToStringAnsi(node2.ID)][0];

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
                    case ObjectType.SUBCATCH:

                        if (value.Count > 0)
                        {
                            for (int i = 0; i < output.SWMMObjects.Count; i++)
                            {
                                if (i == 0)
                                {
                                    output.ElementSet.ElementType = ElementType.Polygon;
                                    output.ElementSet.HasZ = false;
                                }

                                SWMMObject id = output.SWMMObjects[i];

                                if (value.ContainsKey(id.ObjectId))
                                {
                                    output.ElementSet.Elements[i].Vertices = value[id.ObjectId].ToArray();
                                }
                            }
                        }

                        break;
                }
            }

            # endregion
        }

        void InitializeInputExchangeItems()
        {
            inputs.Clear();
            InitializeInputNodes();
            InitializeInputLinks();
            InitializeInputSubcatchments();
        }

        void InitializeInputNodes()
        {

            Dictionary<string, Node> nodes = model.Nodes;
            List<string> keys = nodes.Keys.ToList<string>();
            List<PropertyInfo> properties = SWMMObject.GetAvailableProperties(typeof(Node));

            for (int j = 0; j < properties.Count; j++)
            {
                PropertyInfo property = properties[j];
                SWMMVariableDefinitionAttribute attribute = property.GetCustomAttribute<SWMMVariableDefinitionAttribute>();
                IValueDefinition valueDefinition = Node.GetValueDefinition(attribute.ValueDefinition);

                if (attribute.IsInput)
                {
                    SWMMInputExchangeItem allInput = null;

                    if (attribute.IsMultiInput)
                    {
                        allInput = new SWMMMultiInputExchangeItem()
                        {
                            Id = attribute.Description + " |  All Nodes",
                            Caption = attribute.Description + " |  All Nodes",
                            ObjectType = ObjectType.NODE,
                            Property = property,
                            SWMMObjects = nodes.Values.ToList<SWMMObject>(),
                            Description = attribute.Description + " for all nodes",
                            CacheValues = false,
                            ValueDefinition = valueDefinition,
                            TimeSet = new TimeSet(timeExtent),
                            Component = this,
                            Model = model,
                        };
                    }
                    else
                    {
                        allInput = new SWMMInputExchangeItem()
                        {
                            Id = attribute.Description + " |  All Nodes",
                            Caption = attribute.Description + " |  All Nodes",
                            ObjectType = ObjectType.NODE,
                            Property = property,
                            SWMMObjects = nodes.Values.ToList<SWMMObject>(),
                            Description = attribute.Description + " for all nodes",
                            CacheValues = false,
                            ValueDefinition = valueDefinition,
                            TimeSet = new TimeSet(timeExtent),
                            Component = this,
                            Model = model,
                        };
                    }

                    allInput.ElementSet.ElementType = ElementType.IdBased;
                    inputs.Add(allInput);

                    for (int i = 0; i < keys.Count; i++)
                    {
                        Node node = nodes[keys[i]];
                        List<SWMMObject> tempNodes = new List<SWMMObject>(new SWMMObject[] { node });

                        SWMMInputExchangeItem input = null;


                        if (attribute.IsMultiInput)
                        {
                            input = new SWMMMultiInputExchangeItem()
                            {
                                Id = attribute.Description + " | Node = " + node.ObjectId,
                                Caption = attribute.Description + " | Node = " + node.ObjectId,
                                ObjectType = ObjectType.NODE,
                                Property = property,
                                SWMMObjects = tempNodes,
                                Description = attribute.Description + " for node " + node.ObjectId,
                                CacheValues = false,
                                ValueDefinition = valueDefinition,
                                TimeSet = new TimeSet(timeExtent),
                                Component = this,
                                Model = model,
                            };
                        }
                        else
                        {
                            input = new SWMMInputExchangeItem()
                            {
                                Id = attribute.Description + " | Node = " + node.ObjectId,
                                Caption = attribute.Description + " | Node = " + node.ObjectId,
                                ObjectType = ObjectType.NODE,
                                Property = property,
                                SWMMObjects = tempNodes,
                                Description = attribute.Description + " for node " + node.ObjectId,
                                CacheValues = false,
                                ValueDefinition = valueDefinition,
                                TimeSet = new TimeSet(timeExtent),
                                Component = this,
                                Model = model,
                            };
                        }

                        input.ElementSet.ElementType = ElementType.IdBased;
                        input.ElementSet.HasZ = false;

                        inputs.Add(input);
                    }
                }
            }
        }

        void InitializeInputLinks()
        {

            Dictionary<string, Link> links = model.Links;
            List<string> keys = links.Keys.ToList<string>();
            List<PropertyInfo> properties = SWMMObject.GetAvailableProperties(typeof(Link));


            for (int i = 0; i < properties.Count; i++)
            {
                PropertyInfo property = properties[i];
                SWMMVariableDefinitionAttribute attribute = property.GetCustomAttribute<SWMMVariableDefinitionAttribute>();
                IValueDefinition valueDefinition = Link.GetValueDefinition(attribute.ValueDefinition);

                if (attribute.IsInput)
                {
                    SWMMInputExchangeItem allInput = null;

                    if (attribute.IsMultiInput)
                    {
                        allInput = new SWMMMultiInputExchangeItem()
                        {
                            Id = attribute.Description + " |  All Links",
                            Caption = attribute.Description + " |  All Links",
                            Property = property,
                            ObjectType = ObjectType.LINK,
                            SWMMObjects = links.Values.ToList<SWMMObject>(),
                            Description = attribute.Description + " for all Links",
                            CacheValues = false,
                            ValueDefinition = valueDefinition,
                            TimeSet = new TimeSet(timeExtent),
                            Component = this,
                            Model = model,
                        };
                    }
                    else
                    {
                        allInput = new SWMMInputExchangeItem()
                        {
                            Id = attribute.Description + " |  All Links",
                            Caption = attribute.Description + " |  All Links",
                            Property = property,
                            ObjectType = ObjectType.LINK,
                            SWMMObjects = links.Values.ToList<SWMMObject>(),
                            Description = attribute.Description + " for all Links",
                            CacheValues = false,
                            ValueDefinition = valueDefinition,
                            TimeSet = new TimeSet(timeExtent),
                            Component = this,
                            Model = model,
                        };
                    }

                    allInput.ElementSet.ElementType = ElementType.IdBased;
                    allInput.ElementSet.HasZ = false;
                    inputs.Add(allInput);

                    for (int j = 0; j < keys.Count; j++)
                    {

                        Link link = links[keys[j]];
                        List<SWMMObject> tempLinks = new List<SWMMObject>(new SWMMObject[] { link });

                        SWMMInputExchangeItem input = null;

                        if (attribute.IsMultiInput)
                        {
                            input = new SWMMMultiInputExchangeItem()
                            {
                                Id = attribute.Description + " | Link = " + link.ObjectId,
                                Caption = attribute.Description + " | Link = " + link.ObjectId,
                                Property = property,
                                ObjectType = ObjectType.LINK,
                                SWMMObjects = tempLinks,
                                Description = attribute.Description + " for Link " + link.ObjectId,
                                CacheValues = false,
                                ValueDefinition = valueDefinition,
                                TimeSet = new TimeSet(timeExtent),
                                Component = this,
                                Model = model,
                            };
                        }
                        else
                        {
                            input = new SWMMInputExchangeItem()
                            {
                                Id = attribute.Description + " | Link = " + link.ObjectId,
                                Caption = attribute.Description + " | Link = " + link.ObjectId,
                                Property = property,
                                ObjectType = ObjectType.LINK,
                                SWMMObjects = tempLinks,
                                Description = attribute.Description + " for Link " + link.ObjectId,
                                CacheValues = false,
                                ValueDefinition = valueDefinition,
                                TimeSet = new TimeSet(timeExtent),
                                Component = this,
                                Model = model,
                            };
                        }

                        input.ElementSet.ElementType = ElementType.IdBased;
                        input.ElementSet.HasZ = false;

                        inputs.Add(input);
                    }
                }
            }
        }

        void InitializeInputSubcatchments()
        {

        }

        void InitializeOutputExchangeItems()
        {
            outputs.Clear();
            InitializeOutputNodes();
            InitializeOutputLinks();
            InitializeOutputSubcatchments();
        }

        void InitializeOutputNodes()
        {
            Dictionary<string, Node> nodes = model.Nodes;
            List<string> keys = nodes.Keys.ToList<string>();
            List<PropertyInfo> properties = SWMMObject.GetAvailableProperties(typeof(Node));

            for (int j = 0; j < properties.Count; j++)
            {
                PropertyInfo property = properties[j];
                SWMMVariableDefinitionAttribute attribute = property.GetCustomAttribute<SWMMVariableDefinitionAttribute>();
                IValueDefinition valueDefinition = Node.GetValueDefinition(attribute.ValueDefinition);

                if (attribute.IsOutput)
                {
                    SWMMOutputExchangeItem all = new SWMMOutputExchangeItem()
                    {
                        Id = attribute.Description + " |  All Nodes",
                        Caption = attribute.Description + " |  All Nodes",
                        ObjectType = ObjectType.NODE,
                        Property = property,
                        SWMMObjects = nodes.Values.ToList<SWMMObject>(),
                        Description = attribute.Description + " for all nodes",
                        CacheValues = false,
                        ValueDefinition = valueDefinition,
                        TimeSet = new TimeSet(timeExtent),
                        Component = this,
                        Model = model,
                    };

                    all.ElementSet.ElementType = ElementType.IdBased;
                    all.ElementSet.HasZ = false;

                    outputs.Add(all);

                    for (int i = 0; i < keys.Count; i++)
                    {
                        Node node = nodes[keys[i]];
                        List<SWMMObject> tempNodes = new List<SWMMObject>(new SWMMObject[] { node });

                        SWMMOutputExchangeItem output = new SWMMOutputExchangeItem()
                        {
                            Id = attribute.Description + " | Node = " + node.ObjectId,
                            Caption = attribute.Description + " | Node = " + node.ObjectId,
                            ObjectType = ObjectType.NODE,
                            Property = property,
                            SWMMObjects = tempNodes,
                            Description = attribute.Description + " for node " + node.ObjectId,
                            CacheValues = false,
                            ValueDefinition = valueDefinition,
                            TimeSet = new TimeSet(timeExtent),
                            Component = this,
                            Model = model,
                        };
                        output.ElementSet.ElementType = ElementType.IdBased;
                        output.ElementSet.HasZ = false;
                        outputs.Add(output);
                    }
                }
            }
        }

        void InitializeOutputLinks()
        {
            Dictionary<string, Link> links = model.Links;
            List<string> keys = links.Keys.ToList<string>();
            List<PropertyInfo> properties = SWMMObject.GetAvailableProperties(typeof(Link));


            for (int j = 0; j < properties.Count; j++)
            {
                PropertyInfo property = properties[j];
                SWMMVariableDefinitionAttribute attribute = property.GetCustomAttribute<SWMMVariableDefinitionAttribute>();
                IValueDefinition valueDefinition = Link.GetValueDefinition(attribute.ValueDefinition);

                if (attribute.IsOutput)
                {
                    SWMMOutputExchangeItem all = new SWMMOutputExchangeItem()
                    {
                        Id = attribute.Description + " |  All Links",
                        Caption = attribute.Description + " |  All Links",
                        Property = property,
                        ObjectType = ObjectType.LINK,
                        SWMMObjects = links.Values.ToList<SWMMObject>(),
                        Description = attribute.Description + " for all Links",
                        CacheValues = false,
                        ValueDefinition = valueDefinition,
                        TimeSet = new TimeSet(timeExtent),
                        Component = this,
                        Model = model,
                    };

                    all.ElementSet.ElementType = ElementType.IdBased;
                    all.ElementSet.HasZ = false;
                    outputs.Add(all);

                    for (int i = 0; i < keys.Count; i++)
                    {
                        Link link = links[keys[i]];
                        List<SWMMObject> tempLink = new List<SWMMObject>(new SWMMObject[] { link });

                        SWMMOutputExchangeItem output = new SWMMOutputExchangeItem()
                        {
                            Id = attribute.Description + " | Link = " + link.ObjectId,
                            Caption = attribute.Description + " | Link = " + link.ObjectId,
                            Property = property,
                            ObjectType = ObjectType.LINK,
                            SWMMObjects = tempLink,
                            Description = attribute.Description + " for Link " + link.ObjectId,
                            CacheValues = false,
                            ValueDefinition = valueDefinition,
                            TimeSet = new TimeSet(timeExtent),
                            Component = this,
                            Model = model,
                        };
                        output.ElementSet.ElementType = ElementType.IdBased;
                        output.ElementSet.HasZ = false;
                        outputs.Add(output);
                    }
                }
            }
        }

        void InitializeOutputSubcatchments()
        {
            Dictionary<string, SubCatchment> catchments = model.SubCatchments;
            List<string> keys = catchments.Keys.ToList<string>();
            List<PropertyInfo> properties = SWMMObject.GetAvailableProperties(typeof(SubCatchment));

            for (int i = 0; i < properties.Count; i++)
            {
                PropertyInfo property = properties[i];
                SWMMVariableDefinitionAttribute attribute = property.GetCustomAttribute<SWMMVariableDefinitionAttribute>();
                IValueDefinition valueDefinition = Link.GetValueDefinition(attribute.ValueDefinition);

                if (attribute.IsOutput)
                {
                    SWMMOutputExchangeItem all = new SWMMOutputExchangeItem()
                    {
                        Id = attribute.Description + " |  All Sub-Catchments",
                        Caption = attribute.Description + " |  All Sub-Catchments",
                        Property = property,
                        ObjectType = ObjectType.SUBCATCH,
                        SWMMObjects = catchments.Values.ToList<SWMMObject>(),
                        Description = attribute.Description + " for all sub-catchments",
                        CacheValues = false,
                        ValueDefinition = valueDefinition,
                        TimeSet = new TimeSet(timeExtent),
                        Component = this,
                        Model = model
                    };

                    all.ElementSet.ElementType = ElementType.IdBased;
                    all.ElementSet.HasZ = false;
                    outputs.Add(all);

                    for (int j = 0; j < keys.Count; j++)
                    {

                        SubCatchment subCatchment = catchments[keys[j]];
                        List<SWMMObject> tempSub = new List<SWMMObject>(new SWMMObject[] { subCatchment });

                        SWMMOutputExchangeItem output = new SWMMOutputExchangeItem()
                        {
                            Id = attribute.Description + " | Sub-Catchment = " + subCatchment.ObjectId,
                            Caption = attribute.Description + " | Sub-Catchment = " + subCatchment.ObjectId,
                            Property = property,
                            ObjectType = ObjectType.SUBCATCH,
                            SWMMObjects = tempSub,
                            Description = attribute.Description + " for sub-catchments " + subCatchment.ObjectId,
                            CacheValues = false,
                            ValueDefinition = valueDefinition,
                            TimeSet = new TimeSet(timeExtent),
                            Component = this,
                            Model = model
                        };
                        output.ElementSet.ElementType = ElementType.IdBased;
                        output.ElementSet.HasZ = false;
                        outputs.Add(output);
                    }
                }
            }
        }

        void RetrieveAllConsumers()
        {
            consumers = new List<IBaseInput>();

            foreach (IBaseOutput output in outputs)
            {
                if (output.Consumers.Count > 0)
                {
                    consumers.AddRange(output.Consumers);
                }

                if (output.AdaptedOutputs.Count > 0)
                {
                    foreach (IBaseAdaptedOutput adaptedOutput in output.AdaptedOutputs)
                    {
                        GetAdaptedOutputsConsumers(adaptedOutput, consumers);
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
                              where n.Provider != null || (n is IBaseMultiInput && (n as IBaseMultiInput).Providers.Count > 0)
                              select n).ToArray();
        }

        void GetAdaptedOutputsConsumers(IBaseAdaptedOutput adaptedOutput, List<IBaseInput> consumers)
        {
            if (adaptedOutput.Consumers.Count > 0)
            {
                consumers.AddRange(adaptedOutput.Consumers);
            }

            foreach (IBaseAdaptedOutput adaptedOutputT in adaptedOutput.AdaptedOutputs)
            {
                GetAdaptedOutputsConsumers(adaptedOutputT, consumers);
            }
        }

        #endregion

        #region update functions

        void UpdateRequiredInputExchangeItems()
        {
            for (int i = 0; i < requiredInputs.Length; i++)
            {
                SWMMInputExchangeItem inputItem = requiredInputs[i] as SWMMInputExchangeItem;
                inputItem.Update();
            }
        }

        void UpdateRequiredOutputExchangeItems(IBaseOutput[] required = null)
        {
      
      
                for (int i = 0; i < requiredOutputs.Length; i++)
                {
                    SWMMOutputExchangeItem output = (SWMMOutputExchangeItem)requiredOutputs[i];
                    output.Update();
                }
      

        }

        bool OutputItemsStillRequireData()
        {
            for (int i = 0; i < consumers.Count; i++)
            {
                LinkableComponentStatus status = consumers[i].Component.Status;

                if (status != LinkableComponentStatus.Done &&
                    status != LinkableComponentStatus.Finished &&
                    status != LinkableComponentStatus.Finishing
                   )
                {
                    return true;
                }
            }

            return false;
        }

        DateTime GetEarliestTimeRequiredConsumers(IBaseOutput[] required = null)
        {
            DateTime dateTimeTest = model.EndDateTime;

            if (required == null || required.Length == 0)
            {
                for (int j = 0; j < consumers.Count; j++)
                {
                    ITimeSpaceInput input = consumers[j] as ITimeSpaceInput;
                    ITime time = input.TimeSet.Times[input.TimeSet.Times.Count - 1];
                    DateTime date = time.ToDateTime().AddDays(time.DurationInDays);

                    if (date < dateTimeTest)
                        dateTimeTest = date;
                }
            }
            else
            {
                for (int i = 0; i < required.Length; i++)
                {
                    ITimeSpaceOutput output = required[i] as ITimeSpaceOutput;

                    for (int j = 0; j < output.Consumers.Count; j++)
                    {
                        ITimeSpaceInput input = output.Consumers[j] as ITimeSpaceInput;
                        ITime time = input.TimeSet.Times[input.TimeSet.Times.Count - 1];
                        DateTime date = time.ToDateTime().AddDays(time.DurationInDays);

                        if (date < dateTimeTest)
                            dateTimeTest = date;
                    }

                    for (int j = 0; j < output.AdaptedOutputs.Count; j++)
                    {
                        ITimeSpaceAdaptedOutput adaptedOutput = output.AdaptedOutputs[j] as ITimeSpaceAdaptedOutput;

                        for (int k = 0; k < adaptedOutput.Consumers.Count; k++)
                        {
                            ITimeSpaceInput input = adaptedOutput.Consumers[k] as ITimeSpaceInput;
                            ITime time = input.TimeSet.Times[input.TimeSet.Times.Count - 1];
                            DateTime date = time.ToDateTime().AddDays(time.DurationInDays);

                            if (date < dateTimeTest)
                                dateTimeTest = date;
                        }
                    }
                }
            }

            return dateTimeTest;
        }

        #endregion

        #endregion

        public override string ToString()
        {
            return Caption;
        }

        # endregion

        #region eventhandlers

        public event EventHandler<LinkableComponentStatusChangeEventArgs> StatusChanged;

        #endregion
    }
}
