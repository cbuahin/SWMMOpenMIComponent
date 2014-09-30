using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

 [CLSCompliant(true)]
static class WinLibraryLoader
{
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr LoadLibrary(string library);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr GetProcAddress(IntPtr hModule, string functionName);

    [ DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool FreeLibrary(IntPtr hModule);

    public static  T LoadFunction<T>(ref IntPtr hModule, string functionName) where T : class
    {
        IntPtr address = GetProcAddress(hModule, functionName);
        System.Delegate functionPointer = Marshal.GetDelegateForFunctionPointer(address, typeof(T));
        return functionPointer as T;
    }

    public static void SetValueForValueType<T>(this FieldInfo field, ref T item, object value) where T : struct
    {
        field.SetValueDirect(__makeref(item), value);
    }
}

