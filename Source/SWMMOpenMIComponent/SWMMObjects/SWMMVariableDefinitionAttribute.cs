using OpenMI.Standard2;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SWMMOpenMIComponent
{
    [AttributeUsage(AttributeTargets.Property , AllowMultiple = false)]
    public class SWMMVariableDefinitionAttribute : Attribute
    {
        public SWMMVariableDefinitionAttribute()
        {
            IsOutput = IsInput = IsMultiInput = false;
        }

        public string Name
        {
            get;
            set;
        }

        public string NativeName
        {
            get;
            set;
        }

        public string Description
        {
            get;
            set;
        }

        public string ValueDefinition
        {
            get;
            set;
        }

        public VariableTimeType VariableTimeType
        {
            get;
            set;
        }

        public bool IsOutput
        {
            get;
            set;
        }

        public bool IsInput
        {
            get;
            set;
        }

        public bool IsMultiInput
        {
            get;
            set;
        }
    }

    public enum VariableTimeType
    {
        TimeVarying,
        Constant,
    }
}
