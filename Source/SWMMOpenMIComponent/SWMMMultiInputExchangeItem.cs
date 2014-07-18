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

        public void AddProvider(IBaseOutput provider)
        {
            if(!providers.Contains(provider))
            {
                providers.Add(provider);
            }
        }

        public void RemoveProvider(IBaseOutput provider)
        {
            providers.Remove(provider);
        }

        public override void Update(ITime time)
        {
            List<ITimeSpaceValueSet> tempValues = new List<ITimeSpaceValueSet>();
            int lastIndex = timeSet.Times.Count - 1;
            timeSet.Times[lastIndex] = new Time(time);

            //values.SetElementValuesForTime(lastIndex , Enumerable.Repeat<double>(0,SWMMObjects.Count).ToList());
  
            for(int i = 0 ; i < providers.Count ; i++)
            {
                tempValues.Add((ITimeSpaceValueSet)providers[i].GetValues(this));
            }

            switch(Operator)
            {
                case Operator.Addition:

                    for (int o = 0; o < SWMMObjects.Count; o++)
                    {
                        double tval = 0;

                        for (int i = 0; i < providers.Count; i++)
                        {
                            tval += (double)tempValues[i].GetValue(0, o);
                        }
                        if(tval > 0)
                        {
                            tval.ToString();
                        }
                        values.SetValue(0, o, tval);
                    }

                    break;
                case Operator.Multiply:

                    for (int o = 0; o < SWMMObjects.Count; o++)
                    {
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
                    }
                    break;
                case Operator.Average:

                    for (int o = 0; o < SWMMObjects.Count; o++)
                    {
                        double tval = 0;

                        for (int i = 0; i < providers.Count; i++)
                        {
                            tval += (double)tempValues[i].GetValue(0, o);
                        }

                        tval = tval / (1.0 * providers.Count);

                        values.SetValue(0, o, tval);
                    }
                    
                    break;
            }
        }

        #endregion

        #region functions

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
