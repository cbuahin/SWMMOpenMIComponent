using OpenMI.Standard2.TimeSpace;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace SWMMOpenMIComponent
{

    public interface ISWMMExchangeItem : ITimeSpaceExchangeItem
    {
        List<SWMMObject> SWMMObjects{get;set;}

        PropertyInfo Property { get; set; }

        FieldInfo NativeField { get; }

        ObjectType ObjectType { get; set; }

        SWMM Model {get; set; }

        void Update();
    }

}
