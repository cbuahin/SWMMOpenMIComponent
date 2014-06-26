using OpenMI.Standard2.TimeSpace;
using System;
using System.Collections.Generic;
using System.Linq;
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

    public enum NodeType
    {
        JUNCTION,
        OUTFALL,
        STORAGE,
        DIVIDER
    }

    public enum LinkType
    {
        CONDUIT,
        PUMP,
        ORIFICE,
        WEIR,
        OUTLET
    }
   
    interface ISWMMExchangeItem : ITimeSpaceExchangeItem
    {
        List<SWMMObjectIdentifier> SWMMObjects{get;set;}

        string PropertyName { get; set; }

        ObjectType ObjectType { get; set; }

        bool CacheValues
        {
            get;
            set;
        }
    }

    interface ISWMMInputExchangeItem : ISWMMExchangeItem
    {
        void UpdateModel(ref SWMM model);
    }

    interface ISWMMOutputExchangeItem : ISWMMExchangeItem
    {
        void RetrieveFromModel(ref SWMM model);
    }
}
