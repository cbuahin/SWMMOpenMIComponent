using OpenMI.Standard2;
using OpenMI.Standard2.TimeSpace;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Oatc.OpenMI.Sdk.Backbone.Generic;
using Oatc.OpenMI.Sdk.Backbone;
using System.Collections;
using System.ComponentModel;
using System.Reflection;

namespace SWMMOpenMIComponent
{
    public class SWMMInputExchangeItem : Identifiable, ISWMMExchangeItem, ITimeSpaceInput
    {
        # region variables

        protected SWMMTimeSpaceValueSet<double> values;
        protected ITimeSet timeSet;
        protected ElementSet elementSet;
        protected List<SWMMObject> objects;
        protected ITimeSpaceOutput provider;
        protected SWMM model;
        protected FieldInfo nativeField;
        protected PropertyInfo property;

        # endregion

        # region constructors

        public SWMMInputExchangeItem()
        {
            values = new SWMMTimeSpaceValueSet<double>();
            elementSet = new ElementSet("SWMM Input Element");
            timeSet = new TimeSet();
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

                if(property != null)
                {
                    string nativeName = property.GetCustomAttribute<SWMMVariableDefinitionAttribute>().NativeName;
                    
                    switch(ObjectType)
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
            get
            {
                return provider;
            }
            set
            {
                provider = value as ITimeSpaceOutput;
            }
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
                else if(value is ITimeSpaceValueSet)
                {
                    values = new SWMMTimeSpaceValueSet<double>(value as ITimeSpaceValueSet);
                }
                else
                {
                    throw new Exception("Value must be type :" + typeof(ITimeSpaceValueSet).Name);
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

        public SWMM Model
        {
            get { return model; }
            set { model = value; }
        }

        #endregion

        #region functions

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

        public virtual void Update()
        {
            int lastIndex = timeSet.Times.Count - 1;
            Time time =  timeSet.Times[lastIndex] as Time;
            time.StampAsModifiedJulianDay = model.CurrentDateTime.ToModifiedJulianDay(); 

            Values = provider.GetValues(this);

            IList<double> valuesForElements = (IList<double>)values.GetElementValuesForTime(lastIndex);

            for (int i = 0; i < objects.Count; i++)
            {
                SWMMObject swmmObject = objects[i];
                property.SetValue(swmmObject, valuesForElements[i]);
                model.UpdateSWMMObject(swmmObject, nativeField.Name);
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
