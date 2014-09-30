using Oatc.OpenMI.Sdk.Backbone;
using OpenMI.Standard2;
using OpenMI.Standard2.TimeSpace;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SWMMOpenMIComponent
{
    public class SWMMMultiInputExchangeItem : SWMMInputExchangeItem, ITimeSpaceMultiInput
    {

        # region variables

        IList<IBaseOutput> providers;

        #endregion

        #region constructors

        public SWMMMultiInputExchangeItem()
            :base()
        {
            providers = new List<IBaseOutput>();
        }

        #endregion

        # region properties

        public Operator Operator
        {
            get;
            set;
        }

        public new IBaseValueSet Values
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

        public IList<IBaseOutput> Providers
        {
            get 
            {
               return providers;
            }
        }

        #endregion

        #region functions

        public void AddProvider(IBaseOutput provider)
        {
            for (int i = 0; i < providers.Count; i++)
            {
                IBaseOutput providerInternal = providers[i];

                if (provider.Id == providerInternal.Id && provider.Component.Id == providerInternal.Component.Id)
                {
                    return;
                }
            }

            if (!providers.Contains(provider))
            {
                providers.Add(provider);
            }
        }

        public void RemoveProvider(IBaseOutput provider)
        {
            for (int i = 0; i < providers.Count; i++)
            {
                IBaseOutput providerInternal = providers[i];

                if (provider.Id == providerInternal.Id && provider.Component.Id == providerInternal.Component.Id)
                {
                    providers.Remove(providerInternal);
                    return;
                }
            }

            providers.Remove(provider);
        }

        public override void Update()
        {
            int lastIndex = timeSet.Times.Count - 1;
            Time time = timeSet.Times[lastIndex] as Time;
            time.StampAsModifiedJulianDay = model.CurrentDateTime.ToModifiedJulianDay();

            List<ITimeSpaceValueSet> tempValues = new List<ITimeSpaceValueSet>();

            for (int i = 0; i < providers.Count; i++)
            {
                tempValues.Add((ITimeSpaceValueSet)providers[i].GetValues(this));
            }

            switch (Operator)
            {
                case Operator.Addition:

                    for (int o = 0; o < objects.Count; o++)
                    {
                        SWMMObject swmmObject = objects[o];

                        double tval = 0;

                        for (int i = 0; i < providers.Count; i++)
                        {
                            tval += (double)tempValues[i].GetValue(0, o);
                        }
                        if (tval > 0)
                        {
                            tval.ToString();
                        }

                        values.SetValue(0, o, tval);
                        property.SetValue(swmmObject, tval);
                        model.UpdateSWMMObject(swmmObject, nativeField.Name);
                    }

                    break;
                case Operator.Multiply:

                    for (int o = 0; o < objects.Count; o++)
                    {
                        SWMMObject swmmObject = objects[o];
                        double tval = 0;

                        for (int i = 0; i < providers.Count; i++)
                        {
                            if (i == 0)
                            {
                                tval += (double)tempValues[i].GetValue(0, o);
                            }
                            else
                            {
                                tval *= (double)tempValues[i].GetValue(0, o);
                            }
                        }

                        values.SetValue(0, o, tval);
                        property.SetValue(swmmObject, tval);
                        model.UpdateSWMMObject(swmmObject, nativeField.Name);
                    }
                    break;
                case Operator.Average:

                    for (int o = 0; o < SWMMObjects.Count; o++)
                    {
                        SWMMObject swmmObject = SWMMObjects[o];

                        double tval = 0;

                        for (int i = 0; i < providers.Count; i++)
                        {
                            tval += (double)tempValues[i].GetValue(0, o);
                        }

                        tval = tval / (1.0 * providers.Count);

                        values.SetValue(0, o, tval);
                        property.SetValue(swmmObject, tval);
                        model.UpdateSWMMObject(swmmObject, nativeField.Name);
                    }

                    break;
            }
        }

        #endregion

        #region eventhandlers

        #endregion

    }

    public enum Operator
    {
        Addition,
        Multiply,
        Average,
    }
}
