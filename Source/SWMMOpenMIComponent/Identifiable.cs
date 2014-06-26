using OpenMI.Standard2;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SWMMOpenMIComponent
{
    public class Identifiable : Describable , IIdentifiable
    {
        public string Id
        {
            get;
            set;
        }
    }
}
