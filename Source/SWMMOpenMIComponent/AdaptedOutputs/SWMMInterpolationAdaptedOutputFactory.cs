using OpenMI.Standard2;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SWMMOpenMIComponent
{
    public class SWMMInterpolationAdaptedOutputFactory : IAdaptedOutputFactory
    {

        # region variables

        const string id = "SWWM Interpolation Factory";
        string caption = "SWMM Interpolation Factory";
        string description = "SWMM Interpolation Factory";
        Dictionary<string, TimeInterpolationAdaptedOutput> createdInterpolationAdaptedOutputs = new Dictionary<string,TimeInterpolationAdaptedOutput>();

        #endregion

        # region properties

        public string Id
        {
            get 
            {
                return  id; 
            }
        }

        public string Caption
        {
            get
            {
                return caption;
            }
            set
            {
                caption = value;
            }
        }

        public string Description
        {
            get
            {
                return description;
            }
            set
            {
                description = value;
            }
        }

        # endregion
        
        # region functions

        public IIdentifiable[] GetAvailableAdaptedOutputIds(IBaseOutput adaptee, IBaseInput target)
        {
            Identifiable id = new Identifiable()
            {
                Id = adaptee.Id + " => SWMM Interpolation",
                Caption = "SWMM Interpolation of " + adaptee.Id,
                Description = "SWMM Interpolation of " + adaptee.Id
            };

            return new IIdentifiable[] { id };
        }

        public IBaseAdaptedOutput CreateAdaptedOutput(IIdentifiable adaptedOutputId, IBaseOutput adaptee, IBaseInput target)
        {
            if (createdInterpolationAdaptedOutputs.ContainsKey(adaptedOutputId.Id))
            {
                TimeInterpolationAdaptedOutput adaptedOutput = createdInterpolationAdaptedOutputs[adaptedOutputId.Id];
                adaptedOutput.Adaptee = adaptee;
               
                if (!adaptee.AdaptedOutputs.Contains(adaptedOutput))
                {
                    adaptee.AddAdaptedOutput(adaptedOutput);
                }

                return adaptedOutput;
            }
            else
            {
                TimeInterpolationAdaptedOutput adaptedOutput = new TimeInterpolationAdaptedOutput(adaptedOutputId.Id)
                {
                    Adaptee = adaptee
                };

                if (!adaptee.AdaptedOutputs.Contains(adaptedOutput))
                {
                    adaptee.AddAdaptedOutput(adaptedOutput);
                }

                return adaptedOutput;
            }
        }

        #endregion
    }
}
