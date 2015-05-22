using Oatc.OpenMI.Sdk.Backbone;
using Oatc.OpenMI.Sdk.Backbone.Generic;
using OpenMI.Standard2;
using OpenMI.Standard2.TimeSpace;
using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace SWMMOpenMIComponent
{
    public class SWMMOutputExchangeItem : Identifiable, ISWMMExchangeItem, ITimeSpaceOutput
    {
        # region variables

        SWMMTimeSpaceValueSet<double> values;
        ITimeSet timeSet;
        ElementSet elementSet;
        List<SWMMObject> objects;
        FieldInfo nativeField;
        PropertyInfo property;
        SWMM model;
        IList<IBaseAdaptedOutput> adaptedOutputs;
        IList<IBaseInput> consumers ;
        # endregion

        # region constructors

        public SWMMOutputExchangeItem()
        {
            values = new SWMMTimeSpaceValueSet<double>();
            elementSet = new ElementSet("SWMM Output Element");
            timeSet = new TimeSet();
            adaptedOutputs = new List<IBaseAdaptedOutput>();
            consumers = new List<IBaseInput>();
            objects = new List<SWMMObject>();
        }

        #endregion

        # region properties

        public List<SWMMObject> SWMMObjects
        {
            get
            {
                return objects;
            }
            set
            {
                objects = value;

                InitializeValuesAndElementSet();
            }
        }

        public ObjectType ObjectType
        {
            get;
            set;
        }

        public PropertyInfo Property
        {
            get { return property; }
            set
            {
                property = value;

                if (property != null)
                {
                    string nativeName = property.GetCustomAttribute<SWMMVariableDefinitionAttribute>().NativeName;

                    switch (ObjectType)
                    {
                        case ObjectType.NODE:
                            nativeField = typeof(TNode).GetField(nativeName);
                            break;
                        case ObjectType.LINK:
                            nativeField = typeof(TLink).GetField(nativeName);
                            break;
                        case ObjectType.SUBCATCH:
                            nativeField = typeof(TSubcatch).GetField(nativeName);
                            break;
                    }
                }
            }
        }

        public FieldInfo NativeField
        {
            get
            {
                return nativeField;
            }
        }

        public bool CacheValues
        {
            get;
            set;
        }

        public ITimeSpaceValueSet Values
        {
            get
            {
                return values;
            }
            set
            {
                if (value is SWMMTimeSpaceValueSet<double>)
                {
                    values = (SWMMTimeSpaceValueSet<double>)value;
                }
                else
                {
                    values = new SWMMTimeSpaceValueSet<double>(value);
                }
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
                return elementSet;
            }

            set
            {
                if (value is ElementSet)
                {
                    elementSet = (ElementSet)value;
                }
                else
                {
                    throw new ArgumentException("Argument must be :" + typeof(ElementSet).Name);
                }
            }
        }

        public ElementSet ElementSet
        {
            get
            {
                return elementSet;
            }
            set
            {
                elementSet = (ElementSet)value;
            }
        }

        public IValueDefinition ValueDefinition
        {
            get;
            set;
        }

        public IBaseLinkableComponent Component
        {
            get;
            set;
        }

        public IList<IBaseAdaptedOutput> AdaptedOutputs
        {
            get 
            {
                return adaptedOutputs;
            }
            set
            {
                adaptedOutputs = value;
            }
        }

        public IList<IBaseInput> Consumers
        {
            get
            {
             return consumers   ;
            }
            set
            {
                consumers = value;
            }
        }

        IBaseValueSet IBaseOutput.Values
        {
            get
            {
                return values;
            }

        }

        public SWMM Model
        {
            get { return model; }
            set { model = value; }
        }

        #endregion

        #region functions

        public void AddConsumer(IBaseInput consumer)
        {
            for (int i = 0; i < consumers.Count; i++)
            {
                IBaseInput consumerInternal = consumers[i];

                if (consumer.Id == consumerInternal.Id && consumerInternal.Component.Id == consumer.Component.Id)
                {
                    return;
                }
            }

            if (!consumers.Contains(consumer))
                consumers.Add(consumer);
        }

        public void RemoveConsumer(IBaseInput consumer)
        {
            for (int i = 0; i < consumers.Count; i++)
            {
                IBaseInput consumerInternal = consumers[i];

                if (consumer.Id == consumerInternal.Id && consumerInternal.Component.Id == consumer.Component.Id)
                {
                    consumers.Remove(consumerInternal);
                    return;
                }
            }

           consumers.Remove(consumer);
        }

        public void AddAdaptedOutput(IBaseAdaptedOutput adaptedOutput)
        {
            for (int i = 0; i < adaptedOutputs.Count; i++)
            {
                IBaseAdaptedOutput adaptedOutputInternal = adaptedOutputs[i];

                if (adaptedOutput.Id == adaptedOutputInternal.Id && adaptedOutputInternal.Component.Id == adaptedOutput.Component.Id)
                {
                    return;
                }
            }

            if (!adaptedOutputs.Contains(adaptedOutput))
                adaptedOutputs.Add(adaptedOutput);
        }

        public void RemoveAdaptedOutput(IBaseAdaptedOutput adaptedOutput)
        {
            for (int i = 0; i < adaptedOutputs.Count; i++)
            {
                IBaseAdaptedOutput adaptedOutputInternal = adaptedOutputs[i];

                if (adaptedOutput.Id == adaptedOutputInternal.Id && adaptedOutputInternal.Component.Id == adaptedOutput.Component.Id)
                {
                    adaptedOutputs.Remove(adaptedOutputInternal);
                }
            }

            adaptedOutputs.Remove(adaptedOutput);
        }

        IBaseValueSet IBaseOutput.GetValues(IBaseExchangeItem querySpecifier)
        {
            return GetValues(querySpecifier);
        }

        public ITimeSpaceValueSet GetValues(IBaseExchangeItem querySpecifier)
        {
            ITimeSpaceInput timeSpaceQuery = querySpecifier as ITimeSpaceInput;

            ITimeSet qTimeSet = timeSpaceQuery.TimeSet;

            double queryTimeMjd = qTimeSet.Times[0].End().StampAsModifiedJulianDay;

            double availableTimeMjd = Double.NegativeInfinity;

            int tindex = timeSet.Times.Count - 1;
            if (timeSet.Times.Count > 0)
                availableTimeMjd = timeSet.Times[tindex].End().StampAsModifiedJulianDay;

            IBaseLinkableComponent component = Component;

            while ((component.Status == LinkableComponentStatus.Valid || component.Status == LinkableComponentStatus.Updated) && availableTimeMjd <= queryTimeMjd)
            {

                component.Update( new IBaseOutput[]{ timeSpaceQuery.Provider });
                availableTimeMjd = timeSet.Times[tindex].End().StampAsModifiedJulianDay;
            }

            return values;
        }

        public bool OutputAndInputElementSetsFit(ref ITimeSpaceExchangeItem consumer)
        {
            if (consumer == null)
            {
                throw new ArgumentNullException("consumer");
            }

            bool elementSetFits = false;

            IElementSet sourceElementSet = this.ElementSet();
            IElementSet targetElementSet = consumer.ElementSet();

            if (sourceElementSet == null && targetElementSet != null)
            {
                // NOTE: Source has no elementset specification, source has.
                // Source fits target if target requires only one element.
                elementSetFits = targetElementSet.ElementCount == 1;
            }
            else if (targetElementSet == null && sourceElementSet != null)
            {
                // NOTE: Target has no elementset specification, source has.
                // Source fits target if source has values for only one element available.
                elementSetFits = sourceElementSet.ElementCount == 1;

                // Both source and target have an element set specification
                // If the source is a regular exchange item, the #elements will fit
                // (has been checked configuration time)

                // If it is a spatial extension, we need to check if valeus on the newly required
                // element set can be delivered
            }
            else if (targetElementSet != null && sourceElementSet != null)
            {
                return true;
            }

            return elementSetFits;
        }

        public bool OutputAndInputTimeSetsFit(ref ITimeSpaceExchangeItem consumer)
        {
            if (consumer == null)
            {
                throw new ArgumentNullException("consumer");
            }

            bool timeFits = true;
            ITimeSet sourceTimeSet = TimeSet;
            ITimeSet targetTimeSet = consumer.TimeSet;

            if (sourceTimeSet == null)
            {
                if (targetTimeSet != null)
                {
                    // NOTE: Source has no timeset specification, source has.
                    // Source fits target if target requires only one time step.
                    timeFits = targetTimeSet.Times.Count == 1;
                }
            }
            else
            {
                if (targetTimeSet == null)
                {
                    // NOTE: Target has no timeset specification, source has.
                    // Source fits target if source has values for only one time step available.
                    timeFits = sourceTimeSet.Times.Count == 1;
                }
                else
                {
                    /*
                     * SH/AM: TODO I Think this code is wrong, IOutput and IAdaptedOutput should be treated the same
                     * (SH: reactivated (if (provider is IAdaptedOutput) code
                     * to make things work for time extrapolators again.
                     */
                    // Both source and target have time set specification

                    timeFits = false;
                    // regular (output) exchange item, check if all times fit
                    IList<ITime> sourceTimes = sourceTimeSet.Times;
                    IList<ITime> requiredTimes = targetTimeSet.Times;

                    if (sourceTimes.Count == requiredTimes.Count)
                    {
                        timeFits = true;

                        for (int timeIndex = 0; timeIndex < requiredTimes.Count; timeIndex++)
                        {
                            if ((requiredTimes[timeIndex].DurationInDays > 0 && !(sourceTimes[timeIndex].DurationInDays > 0)) ||
                               (sourceTimes[timeIndex].DurationInDays > 0 && !(requiredTimes[timeIndex].DurationInDays > 0)))
                            {
                                throw new Exception("Incompatible times (stamp versus span) between outputItem \"" + Id +
                                          " and inputItem \"" + consumer.Id + "\"");
                            }
                            if (requiredTimes[timeIndex].Equals(sourceTimes[timeIndex])) continue;
                            timeFits = false;
                            break;
                        }
                    }
                }
            }
            return timeFits;
        }

        public void InitializeValuesAndElementSet()
        {
            Element[] els = new Element[objects.Count];

            for (int i = 0; i < objects.Count; i++)
            {
                SWMMObject id = objects[i];

                els[i] = new Element(id.ObjectId)
                    {
                        Caption = id.ObjectId,
                        Description = id.ObjectId
                    };
            }

            elementSet.Elements = els;

            IList<IList> valueSet = new ListIList<double>();

            double[] v = new double[objects.Count];

            valueSet.Add(v.ToList());

            values.Values2D = valueSet;
        }

        public void Update()
        {
            int lastIndex = timeSet.Times.Count - 1;
            Time time = timeSet.Times[lastIndex] as Time;
            time.StampAsModifiedJulianDay = model.CurrentDateTime.ToModifiedJulianDay();

            for (int k = 0; k < objects.Count; k++)
            {
                SWMMObject sObject = objects[k];
                model.GetSWMMObject(sObject);
                double value = (double)property.GetValue(sObject);
                values.SetValue(new int[] { 0, k }, value);
            }

            for (int j = 0; j < adaptedOutputs.Count; j++)
            {
                ITimeSpaceAdaptedOutput adaptedOutput = (ITimeSpaceAdaptedOutput)adaptedOutputs[j];
                adaptedOutput.Refresh();
            }
        }

        public override string ToString()
        {
            return Id;
        }

        #endregion

        #region eventhandlers

        public event EventHandler<ExchangeItemChangeEventArgs> ItemChanged;

        #endregion
    }
}
