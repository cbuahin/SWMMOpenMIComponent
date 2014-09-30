using Oatc.OpenMI.Sdk.Backbone;
using OpenMI.Standard2;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace SWMMOpenMIComponent
{
    public static class Units
    {
        public static IUnit LengthUnit = new Unit("Length (ft)", 1.0, 0.0, "")
        {
            Dimension = new Dimension(PredefinedDimensions.Length),
        };

        public static IUnit AreaUnit = new Unit("Area (ft^2)", 1.0, 0.0, "")
        {
            Dimension = new Dimension(PredefinedDimensions.Area)
        };

        public static IUnit FlowUnit = new Unit("Flow (ft^3/s)", 1.0, 0.0, "")
        {
            Dimension = new Dimension(PredefinedDimensions.VolumePerTime)
        };

        public static IUnit CoeffUnit = new Unit("Coefficient", 1.0, 0.0, "")
        {
            Dimension = new Dimension()
        };

        public static IUnit InfiltrationUnit = new Unit("Hydraulic Conductivity Units (ft/s)", 1.0, 0.0, "")
        {
            Dimension = new Dimension(PredefinedDimensions.LengthPerTime)
        };
    }

    public static class ValueDefinitions
    {
        public static IValueDefinition Elevation = new Quantity("Elevation")
        {
            Unit = Units.LengthUnit,
            MissingDataValue = -9999999,
            Caption = "Elevation (ft)",
            Description = "",
            ValueType = typeof(double)
        };

        public static IValueDefinition Depth = new Quantity("Depth")
        {
            Unit = Units.LengthUnit,
            MissingDataValue = -9999999,
            Caption = "Depth (ft)",
            Description = "",
            ValueType = typeof(double)
        };

        public static IValueDefinition Height = new Quantity("Height")
        {
            Unit = Units.LengthUnit,
            MissingDataValue = -9999999,
            Caption = "Height (ft)",
            Description = "",
            ValueType = typeof(double)
        };

        public static IValueDefinition Area = new Quantity("Area")
        {
            Unit = Units.AreaUnit,
            MissingDataValue = -9999999,
            Caption = "Area (ft^2)",
            Description = "",
            ValueType = typeof(double)
        };

        public static IValueDefinition Flow = new Quantity("Flow")
        {
            Unit = Units.FlowUnit,
            MissingDataValue = -9999999,
            Caption = "Flow (ft^3/s)",
            Description = "",
            ValueType = typeof(double)
        };

        public static IValueDefinition Coefficient = new Quantity("Coefficient")
        {
            Unit = Units.CoeffUnit,
            MissingDataValue = -9999999,
            Caption = "Coefficient",
            Description = "",
            ValueType = typeof(double)
        };

        public static IValueDefinition HydraulicConductivity = new Quantity("Infiltration")
        {
            Unit = Units.InfiltrationUnit,
            MissingDataValue = -9999999,
            Caption = "Infiltration",
            Description = "",
            ValueType = typeof(double)
        };
    }
}
