﻿using OpenMI.Standard2;
using OpenMI.Standard2.TimeSpace;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Oatc.OpenMI.Sdk.Backbone.Generic;
using Oatc.OpenMI.Sdk.Backbone;

namespace SWMMOpenMIComponent
{
    public class SWMMInputExchangeItem : Identifiable, ISWMMInputExchangeItem, ITimeSpaceInput
    {
        # region variables

        SWMMTimeSpaceValueSet<double> values;
        ITimeSet timeSet;
        ElementSet elementSet;
        List<SWMMObjectIdentifier> objects;

        # endregion

        # region constructors

        public SWMMInputExchangeItem()
        {
            values = new SWMMTimeSpaceValueSet<double>();
            elementSet = new ElementSet("SWMM Input Element");
            timeSet = new TimeSet();
        }

        #endregion

        # region properties

        public List<SWMMObjectIdentifier> SWMMObjects
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

        public IBaseOutput Provider
        {
            get;
            set;
        }

        IBaseValueSet IBaseInput.Values
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

        #endregion

        #region functions

        public void UpdateModel(ref SWMM model)
        {
            if (timeSet.Times.Count > 0)
            {
                int latestTimeIndex = timeSet.Times.Count - 1;
                IList<double> valuesForElements = (IList<double>)values.GetElementValuesForTime(latestTimeIndex);

                for (int i = 0; i < SWMMObjects.Count; i++)
                {
                    SWMMObjectIdentifier id = SWMMObjects[i];
                    model.UpdateValue(ObjectType, id.ObjectId, PropertyName, valuesForElements[i]);
                }
            }
        }

        public void InitializeValuesAndElementSet()
        {
            elementSet.Elements = new Element[objects.Count];

            for (int i = 0; i < objects.Count; i++)
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
