using Oatc.OpenMI.Sdk.Backbone;
using OpenMI.Standard2;
using OpenMI.Standard2.TimeSpace;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SWMMOpenMIComponent
{
    class SWMMOutputExchangeItem : Identifiable, ISWMMOutputExchangeItem, ITimeSpaceOutput
    {
        # region variables

        SWMMTimeSpaceValueSet<double> values;
        ITimeSet timeSet;
        ElementSet elementSet;
        List<SWMMObjectIdentifier> objects;

        # endregion

        # region constructors

        public SWMMOutputExchangeItem()
        {
            values = new SWMMTimeSpaceValueSet<double>();
            elementSet = new ElementSet("SWMM Output Element");
            timeSet = new TimeSet();
            AdaptedOutputs = new List<IBaseAdaptedOutput>();
            Consumers = new List<IBaseInput>();
            objects = new List<SWMMObjectIdentifier>();
        }

        #endregion

        # region properties

        public List<SWMMObjectIdentifier> SWMMObjects
        {
            get
            {
               return objects ;
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

        public string PropertyName
        {
            get;
            set;
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
                    throw new Exception("Value must be type :" + typeof(SWMMTimeSpaceValueSet<double>).Name);
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
            get;
            set;
        }

        public IList<IBaseInput> Consumers
        {
            get;
            set;
        }

        IBaseValueSet IBaseOutput.Values
        {
            get
            {
                return values;
            }

        }

        #endregion

        #region functions

        public void RetrieveFromModel(ref SWMM model)
        {
            for (int i = 0; i < SWMMObjects.Count; i++)
            {
                 SWMMObjectIdentifier id = SWMMObjects[i];
                 values.SetValue(new int[]{0,i},  model.GetValue(ObjectType, id.ObjectId, PropertyName));
            }
        }

        public void AddConsumer(IBaseInput consumer)
        {
            Consumers.Add(consumer);
        }

        public void RemoveConsumer(IBaseInput consumer)
        {
            Consumers.Remove(consumer);
        }

        public void AddAdaptedOutput(IBaseAdaptedOutput adaptedOutput)
        {
            AdaptedOutputs.Add(adaptedOutput);
        }

        public void RemoveAdaptedOutput(IBaseAdaptedOutput adaptedOutput)
        {
            AdaptedOutputs.Remove(adaptedOutput);
        }

        IBaseValueSet IBaseOutput.GetValues(IBaseExchangeItem querySpecifier)
        {
            return GetValues(querySpecifier);
        }

        public ITimeSpaceValueSet GetValues(IBaseExchangeItem querySpecifier)
        {
            ITimeSpaceExchangeItem timeSpaceQuery = querySpecifier as ITimeSpaceExchangeItem;

            if (timeSpaceQuery == null)
                throw new ArgumentException("querySpecifier must be an ITimeSpaceExchangeItem - add an adaptor");

            if (!OutputAndInputTimeSetsFit(ref timeSpaceQuery))
            {
                // Time set of query must be defined and have at least 1 time
                if (timeSpaceQuery.TimeSet == null ||
                    timeSpaceQuery.TimeSet.Times == null ||
                    timeSpaceQuery.TimeSet.Times.Count == 0)
                {
                    throw new Exception("Given the TimeSet of output item \"" + Id +
                                        "\", it can not produce one set of values for \"" + querySpecifier.Id + "\"");
                }

                // Output time set must be defined
                if (TimeSet == null || TimeSet.Times == null)
                {
                    throw new Exception("Invalid time specifier in output item \"" + Id +
                                        "\" for in updating according to a time specification" + querySpecifier.Id);

                }

                // Compute until this time is available
                double queryTimeMjd = timeSpaceQuery.TimeSet.Times[0].End().StampAsModifiedJulianDay;

                // The current available time from the output item
                double availableTimeMjd = Double.NegativeInfinity;

                if (TimeSet.Times.Count > 0)
                    availableTimeMjd = TimeSet.Times[TimeSet.Times.Count - 1].End().StampAsModifiedJulianDay;

                // Update component until querytime is available
                // If component is "busy" (LinkableComponentStatus.Updating), the
                // component will not be updated.
                IBaseLinkableComponent component = Component;
                while ((component.Status == LinkableComponentStatus.Valid ||
                        component.Status == LinkableComponentStatus.Updated) &&
                       availableTimeMjd + Time.EpsilonForTimeCompare < queryTimeMjd)
                {
                    component.Update();
                    availableTimeMjd = TimeSet.Times[TimeSet.Times.Count - 1].End().StampAsModifiedJulianDay;
                }

                // Return true if component was updated up until queryTimeMjd
                //return (availableTimeMjd + Time.EpsilonForTimeCompare >= queryTimeMjd);

            }

            if (!OutputAndInputTimeSetsFit(ref timeSpaceQuery))
            {
                throw new Exception("Could not update engine \"" + Component.Id + "\" to required time for output item \"" + Id +
                                    "\" (requiring input item \"" + querySpecifier.Id + "\"). Use a Time Extrapolator.");
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
            elementSet.Elements = new Element[objects.Count];

            for(int i = 0 ; i < objects.Count ; i++)
            {
                elementSet.Elements[i] = new Element(objects[i].ObjectId);
            }
        }

        #endregion

        #region eventhandlers

        public event EventHandler<ExchangeItemChangeEventArgs> ItemChanged;

        #endregion
    }
}
