using OpenMI.Standard2;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SWMMOpenMIComponent
{
    public class Describable : IDescribable
    {

        public string Caption
        {
            get;
            set;
        }

        public string Description
        {
            get;
            set;
        }
    }
}
