using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace SWMMOpenMIComponent
{
    //------------
    // NODE OBJECT
    //------------
    [StructLayout(LayoutKind.Sequential)]
    public struct TNode
    {
        public string ID;              // node ID
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

    //------------
    // LINK OBJECT
    //------------
    [StructLayout(LayoutKind.Sequential)]
    public struct TLink
    {
        public string ID;              // link ID
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

    //-----------------------------
    // CROSS SECTION DATA STRUCTURE
    //-----------------------------
    [StructLayout(LayoutKind.Sequential)]
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


    public struct SWMMObjectIdentifier
    {
        public string ObjectId { get; set; }

        public int ObjectIndex { get; set; }

        public ObjectType ObjectType { get; set; }

    }
}
