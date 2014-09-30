using OpenMI.Standard2;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SWMMOpenMIComponent
{
    /// <summary>
    /// IDescribable implementation. 
    /// Provides descriptive information on an OpenMI entity.  An entity that is describable has a caption (title or heading) and a description. These are not to be used for identification (see <see cref="IIdentifiable"/>
    /// </summary>
    public class Describable : IDescribable
    {

        /// <summary>
        /// Caption string (not to be used as an id)
        /// </summary>
        public string Caption
        {
            get;
            set;
        }

        /// <summary>
        /// Additional descriptive information about the entity.
        /// </summary>
        public string Description
        {
            get;
            set;
        }
    }
}
