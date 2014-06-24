using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;


static class WinLibraryLoader
{
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr LoadLibrary(string library);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr GetProcAddress(IntPtr hModule, string functionName);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool FreeLibrary(IntPtr hModule);

    public static  T LoadFunction<T>(ref IntPtr hModule, string functionName) where T : class
    {
        IntPtr address = GetProcAddress(hModule, functionName);
        System.Delegate functionPointer = Marshal.GetDelegateForFunctionPointer(address, typeof(T));
        return functionPointer as T;
    }
}

