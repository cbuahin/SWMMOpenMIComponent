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
    /// IIdentifiable Implementation
    /// </summary>
    public class Identifiable : Describable, IIdentifiable
    {
        /// <summary>
        /// IIdentifiable implementation
        /// Defines a method to get the Id of an OpenMI entity. The <see cref="IIdentifiable"/> extends the <see cref="IDescribable"/>, and therefore has, next to the id, a caption and a description.
        /// </summary>
        public string Id
        {
            get;
            set;
        }
    }
}
