using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace SWMMOpenMIComponent
{
    //-------------------------------------
    // Names of major object types
    //-------------------------------------
    public enum ObjectType
    {
        GAGE,                            // rain gage
        SUBCATCH,                        // subcatchment
        NODE,                            // conveyance system node
        LINK,                            // conveyance system link
        POLLUTANT,                          // pollutant
        LANDUSE,                         // land use category
        TIMEPATTERN,                     // dry weather flow time pattern
        CURVE,                           // generic table of values
        TSERIES,                         // generic time series of values
        CONTROL,                         // conveyance system control rules
        TRANSECT,                        // irregular channel cross-section
        AQUIFER,                         // groundwater aquifer
        UNITHYD,                         // RDII unit hydrograph
        SNOWMELT,                        // snowmelt parameter set
        SHAPE,                           // custom conduit shape
        LID,                             // LID treatment units
        MAX_OBJ_TYPES
    }
    
    public abstract class SWMMObject
    {

        public SWMMObject()
        {
        }

        public abstract string ObjectId { get; }

        public int ObjectIndex { get; set; }

        public abstract ObjectType ObjectType { get; }

        public static List<PropertyInfo> GetAvailableProperties(Type type)
        {
            PropertyInfo[] properties = type.GetProperties();

            return (from n in properties where n.GetCustomAttribute<SWMMVariableDefinitionAttribute>() != null
                       select n).ToList<PropertyInfo>();
        }

        public override string ToString()
        {
            return "Node: " + ObjectId;
        }

    }
}
