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

    /// <summary>
    /// Interface class for generic SWMM ITimeSpaceExchangeItem
    /// </summary>
    /// <remarks></remarks>
    public interface ISWMMExchangeItem : ITimeSpaceExchangeItem
    {
        List<SWMMObject> SWMMObjects{get;set;}

        /// <summary>
        /// Property indicating the name of the variable represented by the ISWMMExchangeItem for .NET SWMM (wrrapper around native SWMM struct) object
        /// </summary>
        PropertyInfo Property { get; set; }

        /// <summary>
        /// Property indicating the name of the variable represented by the ISWMMExchangeItem for marshalled native SWMM struct object
        /// </summary>
        FieldInfo NativeField { get; }

        /// <summary>
        /// SWMM Object type
        /// </summary>
        ObjectType ObjectType { get; set; }

        /// <summary>
        /// Parent SWMM model object
        /// </summary>
        SWMM Model {get; set; }

        void Update();
    }

}
