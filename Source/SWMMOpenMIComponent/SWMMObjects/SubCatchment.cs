using OpenMI.Standard2;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace SWMMOpenMIComponent
{
    //---------------
    // SUBAREA OBJECT
    //---------------
    // An array of 3 subarea objects is associated with each subcatchment object.
    // They describe the runoff process on 3 types of surfaces:
    //   1 - impervious with no depression storage
    //   2 - impervious with depression storage
    //   3 - pervious
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct TSubarea
    {
        public int routeTo;         // code indicating where outflow is sent
        public double fOutlet;         // fraction of outflow to outlet
        public double N;               // Manning's n
        public double fArea;           // fraction of total area
        public double dStore;          // depression storage (ft)
        public double alpha;           // overland flow factor
        public double inflow;          // inflow rate (ft/sec)
        public double runoff;          // runoff rate (ft/sec)
        public double depth;           // depth of surface runoff (ft)
    }

    //--------------------
    // SUBCATCHMENT OBJECT
    //--------------------
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct TSubcatch
    {
        public IntPtr ID;              // subcatchment name
        public char rptFlag;         // reporting flag
        public int gage;            // raingage index
        public int outNode;         // outlet node index
        public int outSubcatch;     // outlet subcatchment index
        public int infil;           // infiltration object index
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public TSubarea[] subArea;      // sub-area data
        public double width;           // overland flow width (ft)
        public double area;            // area (ft2)
        public double fracImperv;      // fraction impervious
        public double slope;           // slope (ft/ft)
        public double curbLength;      // total curb length (ft)
        public IntPtr initBuildup;     // initial pollutant buildup (mass/ft2)
        public IntPtr landFactor;      // array of land use factors
        public IntPtr groundwater;     // associated groundwater data
        public IntPtr gwFlowExpr;      // user-supplied outflow expression
        public IntPtr snowpack;        // associated snow pack data
        public double lidArea;         // area devoted to LIDs (ft2)
        public double rainfall;        // current rainfall (ft/sec)
        public double evapLoss;        // current evap losses (ft/sec)
        public double infilLoss;       // current infil losses (ft/sec) 
        public double runon;           // runon from other subcatchments (cfs)
        public double oldRunoff;       // previous runoff (cfs)
        public double newRunoff;       // current runoff (cfs)
        public double oldSnowDepth;    // previous snow depth (ft)
        public double newSnowDepth;    // current snow depth (ft)
        public IntPtr oldQual;         // previous runoff quality (mass/L)
        public IntPtr newQual;         // current runoff quality (mass/L)
        public IntPtr pondedQual;      // ponded surface water quality (mass)
        public IntPtr totalLoad;       // total washoff load (lbs or kg)
    }

    public class SubCatchment : SWMMObject
    {
        public TSubcatch NativeSubCatchment;

        static Dictionary<string, IValueDefinition> valueDefinitions;

        static SubCatchment()
        {
            valueDefinitions = new Dictionary<string, IValueDefinition>();
            valueDefinitions.Add("Flow", ValueDefinitions.Flow);
        }

        public override string ObjectId
        {
            get 
            {
                return Marshal.PtrToStringAnsi(NativeSubCatchment.ID);
            }
        }

        public override ObjectType ObjectType
        {
            get
            {
                return ObjectType.SUBCATCH; 
            }
        }

        public static IValueDefinition GetValueDefinition(string valueDefinition)
        {
            return valueDefinitions[valueDefinition];
        }

        [SWMMVariableDefinitionAttribute(Name = "Runoff", IsInput = false, IsOutput = true, IsMultiInput = false, Description = "Runoff (cfs)", NativeName = "surDepth", ValueDefinition = "Flow", VariableTimeType = VariableTimeType.TimeVarying)]
        public double Runoff
        {
            get { return NativeSubCatchment.newRunoff; }
            set
            {
                NativeSubCatchment.newRunoff = value;
            }
        }

    }
}
