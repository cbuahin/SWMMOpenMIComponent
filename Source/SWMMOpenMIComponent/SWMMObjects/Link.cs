using OpenMI.Standard2;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace SWMMOpenMIComponent
{

    public enum LinkType
    {
        CONDUIT,
        PUMP,
        ORIFICE,
        WEIR,
        OUTLET
    }

    //-----------------------------
    // CROSS SECTION DATA STRUCTURE
    //-----------------------------
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct TXsect
    {
        public int type;            // type code of cross section shape
        public int culvertCode;     // type of culvert (if any)
        public int transect;        // index of transect/shape (if applicable)
        public double yFull;           // depth when full (ft)
        public double wMax;            // width at widest point (ft)
        public double ywMax;           // depth at widest point (ft)
        public double aFull;           // area when full (ft2)
        public double rFull;           // hyd. radius when full (ft)
        public double sFull;           // section factor when full (ft^4/3)
        public double sMax;            // section factor at max. flow (ft^4/3)
        public double yBot;            // depth of bottom section
        public double aBot;            // area of bottom section
        public double sBot;            // slope of bottom section
        public double rBot;            // radius of bottom section
    }

    //------------
    // LINK OBJECT
    //------------
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct TLink
    {
        public IntPtr ID;              // link ID
        public int type;            // link type code
        public int subIndex;        // index of link's sub-category
        public char rptFlag;         // reporting flag
        public int node1;           // start node index
        public int node2;           // end node index
        public double offset1;         // ht. above start node invert (ft)
        public double offset2;         // ht. above end node invert (ft)
        public TXsect xsect;           // cross section data
        public double q0;              // initial flow (cfs)
        public double qLimit;          // constraint on max. flow (cfs)
        public double cLossInlet;      // inlet loss coeff.
        public double cLossOutlet;     // outlet loss coeff.
        public double cLossAvg;        // avg. loss coeff.
        public double seepRate;        // seepage rate (ft/sec)
        public int hasFlapGate;     // true if flap gate present
        public double oldFlow;         // previous flow rate (cfs)
        public double newFlow;         // current flow rate (cfs)
        public double oldDepth;        // previous flow depth (ft)
        public double newDepth;        // current flow depth (ft)
        public double oldVolume;       // previous flow volume (ft3)
        public double newVolume;       // current flow volume (ft3)
        public double surfArea1;       // upstream surface area (ft2)
        public double surfArea2;       // downstream surface area (ft2)
        public double qFull;           // flow when full (cfs)
        public double setting;         // current control setting
        public double targetSetting;   // target control setting
        public double froude;          // Froude number
        public IntPtr oldQual;         // previous quality state
        public IntPtr newQual;         // current quality state
        public IntPtr totalLoad;       // total quality mass loading
        public int flowClass;       // flow classification
        public double dqdh;            // change in flow w.r.t. head (ft2/sec)
        public char direction;       // flow direction flag
        public char bypassed;        // bypass dynwave calc. flag
        public char normalFlow;      // normal flow limited flag
        public char inletControl;    // culvert inlet control flag
    }

    public class Link : SWMMObject
    {
        public TLink NativeLink;

        static Dictionary<string, IValueDefinition> valueDefinitions;

        static Link()
        {
            valueDefinitions = new Dictionary<string, IValueDefinition>();
            valueDefinitions.Add("Elevation", ValueDefinitions.Elevation);
            valueDefinitions.Add("Height", ValueDefinitions.Height);
            valueDefinitions.Add("Flow", ValueDefinitions.Flow);
            valueDefinitions.Add("Coefficient", ValueDefinitions.Coefficient);
            valueDefinitions.Add("Infiltration", ValueDefinitions.HydraulicConductivity);
        }

        public override string ObjectId
        {
            get 
            {
                return Marshal.PtrToStringAnsi(NativeLink.ID); 
            }
        }

        public override ObjectType ObjectType
        {
            get 
            {
                return ObjectType.LINK; 
            }
        }

        public static IValueDefinition GetValueDefinition(string valueDefinition)
        {
            return valueDefinitions[valueDefinition];
        }

        [SWMMVariableDefinitionAttribute(Name = "Upstream Invert Offset", IsInput = true, IsOutput = true, IsMultiInput = false, Description = "Height Above Start Node (ft)", NativeName = "offset1", ValueDefinition = "Height", VariableTimeType = VariableTimeType.Constant)]
        public double UpstreamInvertOffset
        {
            get { return NativeLink.offset1; }
            set
            {
                NativeLink.offset1 = value;
            }
        }

        [SWMMVariableDefinitionAttribute(Name = "Upstream Invert Elevation", IsInput = true, IsOutput = true, IsMultiInput = false, Description = "Upstream Invert Elevation (ft)", NativeName = "offset1", ValueDefinition = "Elevation", VariableTimeType = VariableTimeType.Constant)]
        public double UpstreamInvertElevation
        {
            get 
            {
                return UpstreamNode.InvertElevation + NativeLink.offset1; 
            }
            set
            {
                NativeLink.offset1 = value - UpstreamNode.InvertElevation;
            }
        }

        [SWMMVariableDefinitionAttribute(Name = "Downstream Invert Offset", IsInput = true, IsOutput = true, IsMultiInput = false, Description = "Height Above End Node (ft)", NativeName = "offset2", ValueDefinition = "Height", VariableTimeType = VariableTimeType.Constant)]
        public double DownstreamInvertOffset
        {
            get { return NativeLink.offset2; }
            set
            {
                NativeLink.offset2 = value;
            }
        }

        [SWMMVariableDefinitionAttribute(Name = "Downstream Invert Elevation", IsInput = true, IsOutput = true, IsMultiInput = false, Description = "Downstream Invert Elevation (ft)", NativeName = "offset2", ValueDefinition = "Elevation", VariableTimeType = VariableTimeType.Constant)]
        public double DownstreamInvertElevation
        {
            get { return DownstreamNode.InvertElevation + NativeLink.offset2; }
            set
            {
                NativeLink.offset2 = value - DownstreamNode.InvertElevation;
            }
        }

        [SWMMVariableDefinitionAttribute(Name = "Initial Flow", IsInput = true, IsOutput = false, IsMultiInput = true, Description = "Initial Flow (cfs)", NativeName = "q0", ValueDefinition = "Flow", VariableTimeType = VariableTimeType.Constant)]
        public double InitialFlow
        {
            get { return NativeLink.q0; }
            set 
            {
                NativeLink.q0 = value; 
            }
        }

        [SWMMVariableDefinitionAttribute(Name = "Inlet Loss Coefficient", IsInput = true, IsOutput = true, IsMultiInput = false, Description = "Inlet Loss Coefficient", NativeName = "cLossInlet", ValueDefinition = "Coefficient", VariableTimeType = VariableTimeType.Constant)]
        public double InletLossCoefficient
        {
            get { return NativeLink.cLossInlet; }
            set
            {
                NativeLink.cLossInlet = value;
            }
        }

        [SWMMVariableDefinitionAttribute(Name = "Outlet Loss Coefficient", IsInput = true, IsOutput = true, IsMultiInput = false, Description = "Outlet Loss Coefficient", NativeName = "cLossOutlet", ValueDefinition = "Coefficient", VariableTimeType = VariableTimeType.Constant)]
        public double OutletLossCoefficient
        {
            get { return NativeLink.cLossOutlet; }
            set
            {
                NativeLink.cLossOutlet = value;
            }
        }

        [SWMMVariableDefinitionAttribute(Name = "Seepage Rate", IsInput = true, IsOutput = true, IsMultiInput = false, Description = "Seepage Rate (ft/sec)", NativeName = "seepRate", ValueDefinition = "Infiltration", VariableTimeType = VariableTimeType.Constant)]
        public double SeepageRate
        {
            get { return NativeLink.seepRate; }
            set
            {
                NativeLink.seepRate = value;
            }
        }

        [SWMMVariableDefinitionAttribute(Name = "Flowrate", IsInput = false, IsOutput = true, IsMultiInput = false, Description = "Flowrate (cfs)", NativeName = "newFlow", ValueDefinition = "Flow", VariableTimeType = VariableTimeType.TimeVarying)]
        public double Flowrate
        {
            get { return NativeLink.newFlow; }
            set
            {
                NativeLink.newFlow = value;
            }
        }

        public Node UpstreamNode
        {
            get;
            set;
        }

        public Node DownstreamNode
        {
            get;
            set;
        }
    }
}
