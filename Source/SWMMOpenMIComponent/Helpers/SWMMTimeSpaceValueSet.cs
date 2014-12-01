using Oatc.OpenMI.Sdk.Backbone.Generic;
using OpenMI.Standard2.TimeSpace;
using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SWMMOpenMIComponent
{
    /// <summary>
    /// ITimeSpaceValueSet implementation. 
    /// </summary>
    public class SWMMTimeSpaceValueSet<T> : ITimeSpaceValueSet
    {
        ListIList<T> values;

        public SWMMTimeSpaceValueSet()
        {
            values = new ListIList<T>();
        }

        public SWMMTimeSpaceValueSet(ITimeSpaceValueSet timeSpaceValueSet)
        {
            values = new ListIList<T>();

            for (int i = 0; i < timeSpaceValueSet.Values2D.Count; i++)
            {
                IList<T> temp = new List<T>();
                IList value = timeSpaceValueSet.Values2D[i];

                for (int j = 0; j < value.Count; j++)
                {
                    temp.Add((T)value[j]);
                }

                values.Add(temp);
            }
        }

        public IList<IList> Values2D
        {
            get
            {
                return values;
            }
            set
            {
                if (value is ListIList<T>)
                {
                    values = (ListIList<T>)value;
                }
                else
                {
                    throw new ArgumentException("Argument must be type: " + typeof(ListIList<T>).Name);
                }
            }
        }

        public object GetValue(int timeIndex, int elementIndex)
        {
            IList<T> tvalue = values[timeIndex];
            return tvalue[elementIndex];
        }

        public void SetValue(int timeIndex, int elementIndex, object value)
        {
            IList<T> tvalue = values[timeIndex];
            tvalue[elementIndex] = (T)value;
        }

        public IList GetTimeSeriesValuesForElement(int elementIndex)
        {
            IList temp = new List<T>();

            for (int i = 0; i < values.Count; i++)
            {
                IList<T> tvalue = values[i];
                temp.Add(tvalue[elementIndex]);
            }

            return temp;
        }

        public void SetTimeSeriesValuesForElement(int elementIndex, IList values)
        {
            if (this.values.Count == values.Count)
            {
                for (int i = 0; i < values.Count; i++)
                {
                    IList<T> tvalue = this.values[i];
                    tvalue[elementIndex] = (T)values[i];
                }
            }
            else
            {
                throw new Exception("Time mismatch between values");
            }
        }

        public IList GetElementValuesForTime(int timeIndex)
        {
            return (IList)values[timeIndex];
        }

        public void SetElementValuesForTime(int timeIndex, IList values)
        {
            values[timeIndex] = values;
        }

        public Type ValueType
        {
            get
            {
                return typeof(T);
            }
        }

        public int NumberOfIndices
        {
            get
            {
                return 2; 
            }
        }

        public int GetIndexCount(int[] indices)
        {
            if(indices == null || indices.Length == 0)
            {
                return values.Count;
            }
            else if(indices.Length == 1)
            {
                return values[indices[0]].Count;
            }
            else
            {
                throw new ArgumentException("Indices does not have the correct length, length must be smaller than 2", "indices");
            }
        }

        public object GetValue(int[] indices)
        {
            if(indices.Length == 2)
            {
                return values[indices[0]][indices[1]];
            }
            else
            {
                throw new ArgumentException("Indices does not have the correct length, length must be smaller than 2", "indices");
            }
        }

        public void SetValue(int[] indices, object value)
        {
            if (indices.Length == 2)
            {
                 values[indices[0]][indices[1]] = (T)value;
            }
            else
            {
                throw new ArgumentException("Indices does not have the correct length, length must be smaller than 2", "indices");
            }
        }
    }
}
