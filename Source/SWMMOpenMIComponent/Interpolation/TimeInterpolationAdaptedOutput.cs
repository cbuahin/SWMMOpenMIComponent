using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using OpenMI.Standard2;
using OpenMI.Standard2.TimeSpace;
using Oatc.OpenMI.Sdk.Backbone;
using System.ComponentModel;
using System.Diagnostics;

namespace SWMMOpenMIComponent
{
    public class TimeInterpolationAdaptedOutput : ITimeSpaceAdaptedOutput
    {


        # region variables

        string id, caption, description;
        Dictionary<string, IArgument> arguments;
        ITimeSpaceOutput adaptee;
        IList<IBaseInput> consumers;
        IList<IBaseAdaptedOutput> adaptedOutputs;
        SWMMTimeSpaceValueSet<double> values;
        ITimeSet timeSet;
        SortedDictionary<DateTime, SWMMTimeSpaceValueSet<double>> valuesCache;
        bool initialized = false;

        # endregion

        # region constructor

        public TimeInterpolationAdaptedOutput()
        {
            this.id = "Interpolation Output";

            valuesCache = new SortedDictionary<DateTime, SWMMTimeSpaceValueSet<double>>();

            arguments = new Dictionary<string, IArgument>();

            arguments.Add("InterpolationType", new ArgumentString("InterpolationType") { Caption = "Interpolation Type", Description = "Interpolation Type", DefaultValue = "Linear", IsOptional = false, IsReadOnly = false, Value = "Linear" });
            arguments.Add("RelaxationFactor", new ArgumentDouble("RelaxationFactor", 0, 0) { Caption = "Relaxation Factor", Description = "Relaxation Factor", IsOptional = false, IsReadOnly = false });

            consumers = new List<IBaseInput>();
            adaptedOutputs = new List<IBaseAdaptedOutput>();
        }

        public TimeInterpolationAdaptedOutput(string id)
        {
            this.id = id;
            caption = id;
            description = id;

            valuesCache = new SortedDictionary<DateTime, SWMMTimeSpaceValueSet<double>>();

            arguments = new Dictionary<string, IArgument>();

            arguments.Add("InterpolationType", new ArgumentString("InterpolationType") { Caption = "Interpolation Type", Description = "Interpolation Type", DefaultValue = "Linear", IsOptional = false, IsReadOnly = false, Value = "Linear" });
            arguments.Add("RelaxationFactor", new ArgumentDouble("RelaxationFactor", 0, 0) { Caption = "Relaxation Factor", Description = "Relaxation Factor", IsOptional = false, IsReadOnly = false });

            consumers = new List<IBaseInput>();
            adaptedOutputs = new List<IBaseAdaptedOutput>();
        }

        # endregion

        # region properties

        public string Id
        {
            get
            {
                return id;
            }
        }

        public string Caption
        {
            get
            {
                return caption;
            }
            set
            {
                caption = value;
            }
        }

        public string Description
        {
            get
            {
                return description;
            }
            set
            {
                description = value;
            }
        }

        public IList<IArgument> Arguments
        {
            get
            {
                return arguments.Values.ToList();
            }
        }

        public IBaseOutput Adaptee
        {
            get
            {
                return adaptee;
            }
            set
            {
                adaptee = value as ITimeSpaceOutput;

                values = new SWMMTimeSpaceValueSet<double>(adaptee.Values);
                timeSet = new TimeSet(adaptee.TimeSet as TimeSet);
            }
        }

        public IList<IBaseInput> Consumers
        {
            get
            {
                return consumers;
            }
        }

        public IList<IBaseAdaptedOutput> AdaptedOutputs
        {
            get
            {
                return adaptedOutputs;
            }
        }

        public IBaseValueSet Values
        {
            get
            {
                return values;
            }
        }

        ITimeSpaceValueSet ITimeSpaceOutput.Values
        {
            get
            {
                return values;
            }

        }

        public IValueDefinition ValueDefinition
        {
            get
            {
                if (adaptee != null)
                    return adaptee.ValueDefinition;
                return null;
            }
        }

        public IBaseLinkableComponent Component
        {
            get
            {
                if (adaptee != null)
                    return adaptee.Component;
                return null;
            }
        }

        public ITimeSet TimeSet
        {
            get
            {
                return timeSet;
            }
            set
            {
                timeSet = value;
            }
        }

        public ISpatialDefinition SpatialDefinition
        {
            get
            {
                if (adaptee != null)
                    return adaptee.SpatialDefinition;
                else
                    return null;
            }
        }

        public InterpolationType Interpolationtype
        {
            get;
            set;
        }

        public double RelaxationFactor { get; set; }

        # endregion

        # region functions

        public void Initialize()
        {
            if (!initialized)
            {
                if (arguments.ContainsKey("InterpolationType"))
                {
                    string value = (string)arguments["InterpolationType"].Value;

                    switch (value)
                    {
                        case "Quadratic":
                            this.Interpolationtype = InterpolationType.Quadratic;
                            break;
                        case "Cubic":
                            this.Interpolationtype = InterpolationType.Cubic;
                            break;
                        default:
                            this.Interpolationtype = InterpolationType.Linear;
                            break;
                    }
                }

                if (arguments.ContainsKey("RelaxationFactor"))
                {
                    IArgument value = arguments["RelaxationFactor"];
                    RelaxationFactor = (double)value.Value;
                }

                for(int i = 0 ; i < adaptedOutputs.Count ; i++)
                {
                    adaptedOutputs[i].Initialize();
                }

                initialized = true;
            }
        }

        public void Refresh()
        {
            if (Adaptee.Component.Status != LinkableComponentStatus.Preparing && Adaptee.Component.Status != LinkableComponentStatus.Updating)
            {
                throw new Exception("Update function can only be called from component when it is validating or updating");
            }

            UpdateBuffer();

            // Update dependent adaptedOutput
            foreach (ITimeSpaceAdaptedOutput adaptedOutput in AdaptedOutputs)
            {
                adaptedOutput.Refresh();
            }
        }

        public void AddConsumer(IBaseInput consumer)
        {

            for (int i = 0; i < consumers.Count; i++)
            {
                if (consumer.Id == consumers[i].Id)
                    return;
            }

            if (!consumers.Contains(consumer))
            {
                consumers.Add(consumer);

                if (consumer is IBaseMultiInput)
                {
                    IBaseMultiInput mconsumer = consumer as IBaseMultiInput;
                    if (!mconsumer.Providers.Contains(this))
                        mconsumer.AddProvider(this);
                }
                else
                {
                    consumer.Provider = this;
                }
            }
        }

        public void RemoveConsumer(IBaseInput consumer)
        {
            for (int i = 0; i < consumers.Count; i++)
            {
                if (consumer.Id == consumers[i].Id)
                {
                    consumers.RemoveAt(i);
                    return;
                }
            }

            consumers.Remove(consumer);
        }

        public void AddAdaptedOutput(IBaseAdaptedOutput adaptedOutput)
        {
            for (int i = 0; i < adaptedOutputs.Count; i++)
            {
                if (adaptedOutput.Id == adaptedOutputs[i].Id)
                    return;
            }

            if (!adaptedOutputs.Contains(adaptedOutput))
            {
                adaptedOutputs.Add(adaptedOutput);
            }

        }

        public void RemoveAdaptedOutput(IBaseAdaptedOutput adaptedOutput)
        {
            for (int i = 0; i < adaptedOutputs.Count; i++)
            {
                if (adaptedOutput.Id == adaptedOutputs[i].Id)
                {
                    adaptedOutputs.RemoveAt(i);
                    return;
                }
            }

            adaptedOutputs.Remove(adaptedOutput);
        }

        IBaseValueSet IBaseOutput.GetValues(IBaseExchangeItem querySpecifier)
        {
            return GetValues(querySpecifier);
        }

        public virtual ITimeSpaceValueSet GetValues(IBaseExchangeItem querySpecifier)
        {
            ITimeSpaceExchangeItem timeSpaceQuery = querySpecifier as ITimeSpaceExchangeItem;

            if (timeSpaceQuery == null)
                throw new ArgumentException("querySpecifier must be an ITimeSpaceExchangeItem - add an adaptor");

            if (timeSpaceQuery.TimeSet == null ||
                timeSpaceQuery.TimeSet.Times == null ||
                timeSpaceQuery.TimeSet.Times.Count == 0)
            {
                throw new Exception("Given the TimeSet of output item \"" + Id +
                                    "\", it can not produce one set of values for \"" + querySpecifier.Id + "\"");
            }

            if (TimeSet == null || TimeSet.Times == null)
            {
                throw new Exception("Invalid time specifier in output item \"" + Id +
                                    "\" for in updating according to a time specification" + querySpecifier.Id);

            }

            ITime queryTime = timeSpaceQuery.TimeSet.Times[0].End();
            double queryTimeMjd = queryTime.StampAsModifiedJulianDay;

            double availableTimeMjd = Double.NegativeInfinity;

            List<DateTime> dtimes = valuesCache.Keys.ToList();

            if (dtimes.Count > 0)
                availableTimeMjd = new Time(dtimes[dtimes.Count - 1]).StampAsModifiedJulianDay;

            IBaseLinkableComponent component = Component;

            while ((component.Status == LinkableComponentStatus.Valid || component.Status == LinkableComponentStatus.Updated) && availableTimeMjd < queryTimeMjd)
            {
                component.Update();
                dtimes = valuesCache.Keys.ToList();
                availableTimeMjd = new Time(dtimes[dtimes.Count - 1]).StampAsModifiedJulianDay;
            }

            Interpolate(queryTime);

            return values;
        }

        public event EventHandler<ExchangeItemChangeEventArgs> ItemChanged;

        private void UpdateBuffer()
        {
            IList<ITime> times = adaptee.TimeSet.Times;
            ITime time = times[times.Count - 1];
            DateTime current = time.End().ToDateTime();

            if (valuesCache.Count == 0)
            {
                valuesCache.Add(current, new SWMMTimeSpaceValueSet<double>(adaptee.Values));
            }
            else
            {
                List<DateTime> dtimes = valuesCache.Keys.ToList();

                if (current > dtimes[dtimes.Count - 1])
                {
                    valuesCache.Add(current, new SWMMTimeSpaceValueSet<double>(adaptee.Values));
                }

                switch (Interpolationtype)
                {
                    case InterpolationType.Linear:

                        while (valuesCache.Count > 2)
                        {
                            dtimes = valuesCache.Keys.ToList();
                            valuesCache.Remove(dtimes[0]);
                        }

                        break;
                    case InterpolationType.Quadratic:

                        while (valuesCache.Count > 3)
                        {
                            dtimes = valuesCache.Keys.ToList();
                            valuesCache.Remove(dtimes[0]);
                        }

                        break;
                    case InterpolationType.Cubic:

                        while (valuesCache.Count > 4)
                        {
                            dtimes = valuesCache.Keys.ToList();
                            valuesCache.Remove(dtimes[0]);
                        }

                        break;
                }
            }
        }

        private void Interpolate(ITime time)
        {

            timeSet.Times[0] = new Time(time);

            DateTime queryDT = time.ToDateTime();
            double qdt = queryDT.ToOADate();

            List<DateTime> dates = valuesCache.Keys.ToList();

            switch (valuesCache.Count)
            {
                case 1:
                    {
                        this.values = valuesCache[dates[0]];
                    }
                    break;
                case 2:
                    {
                        DateTime dt1 = dates[0];
                        DateTime dt2 = dates[1];


                        SWMMTimeSpaceValueSet<double> v1 = valuesCache[dt1];
                        SWMMTimeSpaceValueSet<double> v2 = valuesCache[dt2];

                        double d1 = dt1.ToOADate();
                        double d2 = dt2.ToOADate();

                        int eCount = v1.Values2D[0].Count;

                        for (int i = 0; i < eCount; i++)
                        {
                            double val1 = (double)v1.Values2D[0][i];
                            double val2 = (double)v2.Values2D[0][i];

                            double interp = ((val2 - val1) * (qdt - d1) * (1 - RelaxationFactor) / (d2 - d1)) + val1;

                            this.values.SetValue(new int[] { 0, i }, interp);

                            //if(adaptee.Id == "Flowrate (cfs) | Link = CDT-1253")
                            //{
                            //    string vat =  (dt2-dt1).TotalSeconds.ToString();  
                            //    Debug.Print(vat);
                                
                            //}
                        }

                    }
                    break;
                case 3:
                    {
                        DateTime dt1 = dates[0];
                        DateTime dt2 = dates[1];
                        DateTime dt3 = dates[2];

                        double d1 = dt1.ToOADate();
                        double d2 = dt2.ToOADate();
                        double d3 = dt3.ToOADate();
                        
                        SWMMTimeSpaceValueSet<double> v1 = valuesCache[dt1];
                        SWMMTimeSpaceValueSet<double> v2 = valuesCache[dt2];
                        SWMMTimeSpaceValueSet<double> v3 = valuesCache[dt3];

                        int eCount = v1.Values2D[0].Count;

                        for (int i = 0; i < eCount; i++)
                        {
                            double val1 = (double)v1.Values2D[0][i];
                            double val2 = (double)v2.Values2D[0][i];
                            double val3 = (double)v3.Values2D[0][i];



                            double interp = ((qdt - d2) * (qdt - d1) / ((d3 - d2) * (d3 - d1))) * (val3)
                                             + ((qdt - d3) * (qdt - d1) / ((d2 - d3) * (d2 - d1))) * (val2)
                                             + ((qdt - d3) * (qdt - d2) / ((d1 - d2) * (d1 - d3))) * (val1);

                            this.values.SetValue(new int[] { 0, i }, interp);
                        }

                    }
                    break;
                case 4:
                    {
                        DateTime dt1 = dates[0];
                        DateTime dt2 = dates[1];
                        DateTime dt3 = dates[2];
                        DateTime dt4 = dates[3];


                        double d1 = dt1.ToOADate();
                        double d2 = dt2.ToOADate();
                        double d3 = dt3.ToOADate();
                        double d4 = dt4.ToOADate();


                        SWMMTimeSpaceValueSet<double> v1 = valuesCache[dt1];
                        SWMMTimeSpaceValueSet<double> v2 = valuesCache[dt2];
                        SWMMTimeSpaceValueSet<double> v3 = valuesCache[dt3];
                        SWMMTimeSpaceValueSet<double> v4 = valuesCache[dt4];

                        int eCount = v1.Values2D[0].Count;

                        for (int i = 0; i < eCount; i++)
                        {
                            double val1 = (double)v1.Values2D[0][i];
                            double val2 = (double)v2.Values2D[0][i];
                            double val3 = (double)v3.Values2D[0][i];
                            double val4 = (double)v4.Values2D[0][i];

                            double A = (val1 - 2 * val2 + val3) / ((d1 - d2) * (d1 - d2));
                            double B = (val2 - 2 * val3 + val4) / ((d2 - d3) * (d2 - d3));
                            double C = (val2 - 2 * val3 + val4) / ((d1 - d2) * (d1 - d2));
                            double D = (val1 - 2 * val2 + val3) / ((d2 - d3) * (d2 - d3));

                            double p1 = (qdt - d3);
                            double pp1 = p1*p1*p1;
                            double p2 = (qdt - d4);
                            double pp2 = p2*p2*p2;

                            double interp = ((qdt - d3) * (((val3 - val4) / (d3 - d4)) - (A + B) * (d4 - d3))) + (C *(pp1)/(d4-d3)) + (D*(pp2)/(d4-d3)) ;

                            this.values.SetValue(new int[] { 0, i }, interp);

                        }
                    }
                    break;
            }


        }

        public override string ToString()
        {
            return id;
        }

        # endregion

        # region enums

        public enum InterpolationType
        {
            Linear,
            Quadratic,
            Cubic,
        }

        # endregion
    }
}
