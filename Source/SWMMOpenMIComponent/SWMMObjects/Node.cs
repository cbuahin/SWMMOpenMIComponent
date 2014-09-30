using OpenMI.Standard2;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace SWMMOpenMIComponent
{

    public enum NodeType
    {
        JUNCTION,
        OUTFALL,
        STORAGE,
        DIVIDER
    }

    //------------
    // NODE OBJECT
    //------------
    [StructLayout(LayoutKind.Sequential , CharSet = CharSet.Ansi)]
    public struct TNode
    {
        public IntPtr ID;              // node ID
        public int type;            // node type code
        public int subIndex;        // index of node's sub-category
        public char rptFlag;         // reporting flag
        public double invertElev;      // invert elevation (ft)
        public double initDepth;       // initial storage level (ft)
        public double fullDepth;       // dist. from invert to surface (ft)
        public double surDepth;        // added depth under surcharge (ft)
        public double pondedArea;      // area filled by ponded water (ft2)
        public IntPtr extInflow;       // pointer to external inflow data
        public IntPtr dwfInflow;       // pointer to dry weather flow inflow data
        public IntPtr rdiiInflow;      // pointer to RDII inflow data
        public IntPtr treatment;       // array of treatment data
        public int degree;          // number of outflow links
        public char updated;         // true if state has been updated
        public double crownElev;       // top of highest connecting conduit (ft)
        public double inflow;          // total inflow (cfs)
        public double outflow;         // total outflow (cfs)
        public double oldVolume;       // previous volume (ft3)
        public double newVolume;       // current volume (ft3)
        public double fullVolume;      // max. storage available (ft3)
        public double overflow;        // overflow rate (cfs)
        public double oldDepth;        // previous water depth (ft)
        public double newDepth;        // current water depth (ft)
        public double oldLatFlow;      // previous lateral inflow (cfs)
        public double newLatFlow;      // current lateral inflow (cfs)
        public IntPtr oldQual;         // previous quality state
        public IntPtr newQual;         // current quality state
        public double oldFlowInflow;   // previous flow inflow
        public double oldNetInflow;    // previous net inflow
    }

    public class Node : SWMMObject
    {
        public TNode NativeNode;
        
        static Dictionary<string, IValueDefinition> valueDefinitions;
  
        static Node()
        {
            valueDefinitions = new Dictionary<string, IValueDefinition>();
            valueDefinitions.Add("Elevation", ValueDefinitions.Elevation);
            valueDefinitions.Add("Depth", ValueDefinitions.Depth);
            valueDefinitions.Add("Flow", ValueDefinitions.Flow);
        }

        public override string ObjectId
        {
            get
            {
                return Marshal.PtrToStringAnsi(NativeNode.ID);
            }
        }

        public override ObjectType ObjectType
        {
            get 
            {
                return ObjectType.NODE; 
            }
        }

        public static IValueDefinition GetValueDefinition(string valueDefinition)
        {
            return valueDefinitions[valueDefinition];
        }

        [SWMMVariableDefinitionAttribute (Name = "Invert Elevation", IsInput = true, IsOutput = true, IsMultiInput = false, Description = "Invert Elevation (ft)", NativeName = "invertElev", ValueDefinition = "Elevation", VariableTimeType = VariableTimeType.Constant)]
        public double InvertElevation
        {
            get { return NativeNode.invertElev; }
            set
            {
                NativeNode.invertElev = value;
            }
        }

        [SWMMVariableDefinitionAttribute(Name = "Crown Elevation", IsInput = true, IsOutput = true, IsMultiInput = false, Description = "Top of Highest Connecting Conduit (ft)", NativeName = "crownElev", ValueDefinition = "Elevation", VariableTimeType = VariableTimeType.Constant)]
        public double CrownElevation
        {
            get { return NativeNode.crownElev; }
            set
            {
                NativeNode.crownElev = value;
            }
        }

        [SWMMVariableDefinitionAttribute(Name = "Initial Water Depth", IsInput = true, IsOutput = true, IsMultiInput = false, Description = "Initial Water Depth (ft)", NativeName = "initDepth", ValueDefinition = "Depth", VariableTimeType = VariableTimeType.Constant)]
        public double InitialDepth
        {
            get { return NativeNode.initDepth; }
            set
            {
                NativeNode.initDepth = value;
            }
        }

        [SWMMVariableDefinitionAttribute(Name = "Initial Water Elevation", IsInput = true, IsOutput = true, IsMultiInput = false, Description = "Initial Water Elevation (ft)", NativeName = "initDepth", ValueDefinition = "Elevation", VariableTimeType = VariableTimeType.Constant)]
        public double InitialElevation
        {
            get { return NativeNode.initDepth + NativeNode.invertElev; }
            set
            {
                if (value - NativeNode.invertElev >= 0)
                    NativeNode.initDepth = value - NativeNode.invertElev;
                else
                    NativeNode.initDepth = 0;
            }
        }

        [SWMMVariableDefinitionAttribute(Name = "Surcharge Depth", IsInput = true, IsOutput = true, IsMultiInput = false, Description = "Added Depth Under Surcharge (ft)", NativeName = "surDepth", ValueDefinition = "Depth", VariableTimeType = VariableTimeType.TimeVarying)]
        public double SurchargeDepth
        {
            get { return NativeNode.surDepth; }
            set
            {
                NativeNode.surDepth = value;
            }
        }

        [SWMMVariableDefinitionAttribute(Name = "Water Depth", IsInput = true, IsOutput = true, IsMultiInput = false, Description = "Water Depth (ft)", NativeName = "newDepth", ValueDefinition = "Depth", VariableTimeType = VariableTimeType.TimeVarying)]
        public double WaterDepth
        {
            get 
            { 
                return NativeNode.newDepth;
            }
            set
            {
                NativeNode.newDepth = value;
            }
        }

        [SWMMVariableDefinitionAttribute(Name = "Water Surface Elevation", IsInput = true, IsOutput = true, IsMultiInput = false, Description = "Water Surface Elevation (ft)", NativeName = "newDepth", ValueDefinition = "Elevation", VariableTimeType = VariableTimeType.TimeVarying)]
        public double WaterElevation
        {
            get 
            { 
                return NativeNode.newDepth + NativeNode.invertElev;
            }
            set
            {
                if (value - NativeNode.invertElev >= 0)
                    NativeNode.newDepth = value - NativeNode.invertElev;
                else
                    NativeNode.newDepth = 0;
            }
        }

        [SWMMVariableDefinitionAttribute(Name = "Total Outflow", IsInput = false, IsOutput = true, IsMultiInput = false, Description = "Total Outflow (cfs)", NativeName = "outflow", ValueDefinition = "Flow", VariableTimeType = VariableTimeType.TimeVarying)]
        public double TotalOutflow
        {
            get { return NativeNode.outflow; }
            set
            {
                NativeNode.outflow = value;
            }
        }

        [SWMMVariableDefinitionAttribute(Name = "Total Inflow", IsInput = false, IsOutput = true, IsMultiInput = false, Description = "Total Inflow (cfs)", NativeName = "inflow", ValueDefinition = "Flow", VariableTimeType = VariableTimeType.TimeVarying)]
        public double TotalInflow
        {
            get { return NativeNode.inflow; }
            set
            {
                NativeNode.inflow = value;
            }
        }

        [SWMMVariableDefinitionAttribute(Name = "Lateral Inflow", IsInput = true, IsOutput = true, IsMultiInput = true, Description = "Lateral Inflow (cfs)", NativeName = "newLatFlow", ValueDefinition = "Flow", VariableTimeType = VariableTimeType.TimeVarying)]
        public double LateralInflow
        {
            get { return NativeNode.newLatFlow; }
            set
            {
                NativeNode.newLatFlow = value;
            }
        }
    }
}
