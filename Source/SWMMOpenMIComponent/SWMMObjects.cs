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
        string ID;              // node ID
        int type;            // node type code
        int subIndex;        // index of node's sub-category
        char rptFlag;         // reporting flag
        double invertElev;      // invert elevation (ft)
        double initDepth;       // initial storage level (ft)
        double fullDepth;       // dist. from invert to surface (ft)
        double surDepth;        // added depth under surcharge (ft)
        double pondedArea;      // area filled by ponded water (ft2)
        IntPtr extInflow;       // pointer to external inflow data
        IntPtr dwfInflow;       // pointer to dry weather flow inflow data
        IntPtr rdiiInflow;      // pointer to RDII inflow data
        IntPtr treatment;       // array of treatment data
        int degree;          // number of outflow links
        char updated;         // true if state has been updated
        double crownElev;       // top of highest connecting conduit (ft)
        double inflow;          // total inflow (cfs)
        double outflow;         // total outflow (cfs)
        double oldVolume;       // previous volume (ft3)
        double newVolume;       // current volume (ft3)
        double fullVolume;      // max. storage available (ft3)
        double overflow;        // overflow rate (cfs)
        double oldDepth;        // previous water depth (ft)
        double newDepth;        // current water depth (ft)
        double oldLatFlow;      // previous lateral inflow (cfs)
        double newLatFlow;      // current lateral inflow (cfs)
        IntPtr oldQual;         // previous quality state
        IntPtr newQual;         // current quality state
        double oldFlowInflow;   // previous flow inflow
        double oldNetInflow;    // previous net inflow
    }



    //------------
    // LINK OBJECT
    //------------
    [StructLayout(LayoutKind.Sequential)]
    public struct TLink
    {
        string ID;              // link ID
        int type;            // link type code
        int subIndex;        // index of link's sub-category
        char rptFlag;         // reporting flag
        int node1;           // start node index
        int node2;           // end node index
        double offset1;         // ht. above start node invert (ft)
        double offset2;         // ht. above end node invert (ft)
        IntPtr xsect;           // cross section data
        double q0;              // initial flow (cfs)
        double qLimit;          // constraint on max. flow (cfs)
        double cLossInlet;      // inlet loss coeff.
        double cLossOutlet;     // outlet loss coeff.
        double cLossAvg;        // avg. loss coeff.
        double seepRate;        // seepage rate (ft/sec)
        int hasFlapGate;     // true if flap gate present
        double oldFlow;         // previous flow rate (cfs)
        double newFlow;         // current flow rate (cfs)
        double oldDepth;        // previous flow depth (ft)
        double newDepth;        // current flow depth (ft)
        double oldVolume;       // previous flow volume (ft3)
        double newVolume;       // current flow volume (ft3)
        double surfArea1;       // upstream surface area (ft2)
        double surfArea2;       // downstream surface area (ft2)
        double qFull;           // flow when full (cfs)
        double setting;         // current control setting
        double targetSetting;   // target control setting
        double froude;          // Froude number
        IntPtr oldQual;         // previous quality state
        IntPtr newQual;         // current quality state
        IntPtr totalLoad;       // total quality mass loading
        int flowClass;       // flow classification
        double dqdh;            // change in flow w.r.t. head (ft2/sec)
        char direction;       // flow direction flag
        char bypassed;        // bypass dynwave calc. flag
        char normalFlow;      // normal flow limited flag
        char inletControl;    // culvert inlet control flag
    }
}
