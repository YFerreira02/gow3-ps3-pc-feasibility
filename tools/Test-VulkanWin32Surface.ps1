param(
    [string] $PreferDevice = "Vega 8"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Windows.Forms

# Creates a temporary, hidden Win32 window and asks the installed Vulkan
# implementation whether the preferred adapter's graphics queue can present
# to it. No game files are opened and no Vulkan SDK is needed.
$source = @'
using System;
using System.Runtime.InteropServices;

public static class VulkanWin32SurfaceProbe
{
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr GetModuleHandle(string moduleName);
    [DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true)]
    private static extern IntPtr LoadLibrary(string name);
    [DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true)]
    private static extern IntPtr GetProcAddress(IntPtr module, string name);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate int CreateInstanceDelegate(IntPtr createInfo, IntPtr allocator, out IntPtr instance);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate int EnumeratePhysicalDevicesDelegate(IntPtr instance, ref uint count, IntPtr devices);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate void GetPhysicalDevicePropertiesDelegate(IntPtr device, IntPtr properties);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate void GetQueueFamilyPropertiesDelegate(IntPtr device, ref uint count, IntPtr properties);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate int GetSurfaceSupportDelegate(IntPtr device, uint family, IntPtr surface, out uint supported);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate IntPtr GetInstanceProcAddrDelegate(IntPtr instance, [MarshalAs(UnmanagedType.LPStr)] string name);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate int CreateWin32SurfaceDelegate(IntPtr instance, IntPtr createInfo, IntPtr allocator, out IntPtr surface);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate void DestroySurfaceDelegate(IntPtr instance, IntPtr surface, IntPtr allocator);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate void DestroyInstanceDelegate(IntPtr instance, IntPtr allocator);

    [StructLayout(LayoutKind.Sequential)]
    private struct ApplicationInfo
    {
        public int sType; public IntPtr pNext; public IntPtr pApplicationName;
        public uint applicationVersion; public IntPtr pEngineName; public uint engineVersion; public uint apiVersion;
    }
    [StructLayout(LayoutKind.Sequential)]
    private struct InstanceCreateInfo
    {
        public int sType; public IntPtr pNext; public uint flags; public IntPtr pApplicationInfo;
        public uint enabledLayerCount; public IntPtr ppEnabledLayerNames;
        public uint enabledExtensionCount; public IntPtr ppEnabledExtensionNames;
    }
    [StructLayout(LayoutKind.Sequential)]
    private struct Win32SurfaceCreateInfo
    {
        public int sType; public IntPtr pNext; public uint flags; public IntPtr hinstance; public IntPtr hwnd;
    }

    private static T Load<T>(IntPtr module, string name) where T : class
    {
        IntPtr address = GetProcAddress(module, name);
        if (address == IntPtr.Zero) throw new InvalidOperationException("Missing Vulkan function: " + name);
        return Marshal.GetDelegateForFunctionPointer(address, typeof(T)) as T;
    }
    private static T InstanceFunction<T>(GetInstanceProcAddrDelegate getProc, IntPtr instance, string name) where T : class
    {
        IntPtr address = getProc(instance, name);
        if (address == IntPtr.Zero) throw new InvalidOperationException("Missing Vulkan instance function: " + name);
        return Marshal.GetDelegateForFunctionPointer(address, typeof(T)) as T;
    }

    public static int Run(IntPtr hwnd, string preferredDevice)
    {
        IntPtr loader = LoadLibrary("vulkan-1.dll");
        if (loader == IntPtr.Zero) { Console.Error.WriteLine("FAIL: could not load vulkan-1.dll."); return 2; }

        IntPtr appName = Marshal.StringToHGlobalAnsi("GOW3 Vulkan Win32 surface probe");
        IntPtr engineName = Marshal.StringToHGlobalAnsi("GOW3 feasibility probe");
        IntPtr extSurface = Marshal.StringToHGlobalAnsi("VK_KHR_surface");
        IntPtr extWin32 = Marshal.StringToHGlobalAnsi("VK_KHR_win32_surface");
        IntPtr extensionArray = Marshal.AllocHGlobal(2 * IntPtr.Size);
        IntPtr appMemory = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(ApplicationInfo)));
        IntPtr instanceInfoMemory = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(InstanceCreateInfo)));
        IntPtr surfaceInfoMemory = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(Win32SurfaceCreateInfo)));
        IntPtr properties = Marshal.AllocHGlobal(2048);
        IntPtr instance = IntPtr.Zero;
        IntPtr surface = IntPtr.Zero;
        try
        {
            Marshal.WriteIntPtr(extensionArray, 0, extSurface);
            Marshal.WriteIntPtr(extensionArray, IntPtr.Size, extWin32);
            var app = new ApplicationInfo { sType = 0, pNext = IntPtr.Zero, pApplicationName = appName,
                applicationVersion = 1, pEngineName = engineName, engineVersion = 1,
                apiVersion = (1u << 22) | (2u << 12) };
            Marshal.StructureToPtr(app, appMemory, false);
            var create = new InstanceCreateInfo { sType = 1, pNext = IntPtr.Zero, flags = 0,
                pApplicationInfo = appMemory, enabledLayerCount = 0, ppEnabledLayerNames = IntPtr.Zero,
                enabledExtensionCount = 2, ppEnabledExtensionNames = extensionArray };
            Marshal.StructureToPtr(create, instanceInfoMemory, false);

            IntPtr getProcAddress = GetProcAddress(loader, "vkGetInstanceProcAddr");
            if (getProcAddress == IntPtr.Zero) throw new InvalidOperationException("Missing vkGetInstanceProcAddr.");
            var getProc = Marshal.GetDelegateForFunctionPointer(getProcAddress, typeof(GetInstanceProcAddrDelegate)) as GetInstanceProcAddrDelegate;
            int result = Load<CreateInstanceDelegate>(loader, "vkCreateInstance")(instanceInfoMemory, IntPtr.Zero, out instance);
            if (result != 0) { Console.Error.WriteLine("FAIL: vkCreateInstance returned VkResult " + result + "."); return 3; }

            var makeSurface = InstanceFunction<CreateWin32SurfaceDelegate>(getProc, instance, "vkCreateWin32SurfaceKHR");
            var destroySurface = InstanceFunction<DestroySurfaceDelegate>(getProc, instance, "vkDestroySurfaceKHR");
            var enumerate = Load<EnumeratePhysicalDevicesDelegate>(loader, "vkEnumeratePhysicalDevices");
            var getProperties = Load<GetPhysicalDevicePropertiesDelegate>(loader, "vkGetPhysicalDeviceProperties");
            var getQueues = Load<GetQueueFamilyPropertiesDelegate>(loader, "vkGetPhysicalDeviceQueueFamilyProperties");
            var getSurfaceSupport = InstanceFunction<GetSurfaceSupportDelegate>(getProc, instance, "vkGetPhysicalDeviceSurfaceSupportKHR");

            var surfaceInfo = new Win32SurfaceCreateInfo { sType = 1000009000, pNext = IntPtr.Zero, flags = 0,
                hinstance = GetModuleHandle(null), hwnd = hwnd };
            Marshal.StructureToPtr(surfaceInfo, surfaceInfoMemory, false);
            result = makeSurface(instance, surfaceInfoMemory, IntPtr.Zero, out surface);
            if (result != 0) { Console.Error.WriteLine("FAIL: vkCreateWin32SurfaceKHR returned VkResult " + result + "."); return 4; }

            uint deviceCount = 0;
            result = enumerate(instance, ref deviceCount, IntPtr.Zero);
            if (result != 0 || deviceCount == 0) { Console.Error.WriteLine("FAIL: no physical devices found."); return 5; }
            IntPtr devices = Marshal.AllocHGlobal(checked((int)deviceCount * IntPtr.Size));
            try
            {
                result = enumerate(instance, ref deviceCount, devices);
                if (result != 0) { Console.Error.WriteLine("FAIL: physical device enumeration returned VkResult " + result + "."); return 6; }
                bool matched = false;
                for (uint d = 0; d < deviceCount; ++d)
                {
                    IntPtr physical = Marshal.ReadIntPtr(devices, checked((int)d * IntPtr.Size));
                    getProperties(physical, properties);
                    string name = Marshal.PtrToStringAnsi(IntPtr.Add(properties, 20)).TrimEnd('\0');
                    if (String.IsNullOrWhiteSpace(preferredDevice) || name.IndexOf(preferredDevice, StringComparison.OrdinalIgnoreCase) >= 0)
                    {
                        matched = true;
                        uint familyCount = 0;
                        getQueues(physical, ref familyCount, IntPtr.Zero);
                        const int queuePropertiesSize = 24;
                        IntPtr queueProperties = Marshal.AllocHGlobal(checked((int)familyCount * queuePropertiesSize));
                        try
                        {
                            getQueues(physical, ref familyCount, queueProperties);
                            bool supported = false;
                            for (uint q = 0; q < familyCount; ++q)
                            {
                                IntPtr entry = IntPtr.Add(queueProperties, checked((int)q * queuePropertiesSize));
                                uint flags = unchecked((uint)Marshal.ReadInt32(entry, 0));
                                uint queueCount = unchecked((uint)Marshal.ReadInt32(entry, 4));
                                if ((flags & 1) != 0 && queueCount > 0)
                                {
                                    uint canPresent;
                                    int supportResult = getSurfaceSupport(physical, q, surface, out canPresent);
                                    if (supportResult == 0 && canPresent != 0)
                                    {
                                        supported = true;
                                        Console.WriteLine("Adapter: " + name);
                                        Console.WriteLine("Win32 surface creation: PASS");
                                        Console.WriteLine("Graphics queue family " + q + " presentation support: PASS");
                                        break;
                                    }
                                }
                            }
                            if (!supported)
                            {
                                Console.Error.WriteLine("FAIL: adapter \"" + name + "\" has no graphics queue that can present to this Win32 surface.");
                                return 7;
                            }
                        }
                        finally { Marshal.FreeHGlobal(queueProperties); }
                        break;
                    }
                }
                if (!matched) { Console.Error.WriteLine("FAIL: preferred adapter not found: " + preferredDevice); return 8; }
                Console.WriteLine("RESULT: PASS - Win32 surface and presentation support confirmed.");
                Console.WriteLine("Note: this does not create a swapchain, render pixels, or measure performance.");
                destroySurface(instance, surface, IntPtr.Zero);
                surface = IntPtr.Zero;
                return 0;
            }
            finally { Marshal.FreeHGlobal(devices); }
        }
        finally
        {
            if (surface != IntPtr.Zero && instance != IntPtr.Zero)
            {
                try { InstanceFunction<DestroySurfaceDelegate>(
                    Marshal.GetDelegateForFunctionPointer(GetProcAddress(loader, "vkGetInstanceProcAddr"), typeof(GetInstanceProcAddrDelegate)) as GetInstanceProcAddrDelegate,
                    instance, "vkDestroySurfaceKHR")(instance, surface, IntPtr.Zero); } catch { }
            }
            if (instance != IntPtr.Zero)
            {
                try { Load<DestroyInstanceDelegate>(loader, "vkDestroyInstance")(instance, IntPtr.Zero); } catch { }
            }
            Marshal.FreeHGlobal(properties);
            Marshal.FreeHGlobal(surfaceInfoMemory);
            Marshal.FreeHGlobal(instanceInfoMemory);
            Marshal.FreeHGlobal(appMemory);
            Marshal.FreeHGlobal(extensionArray);
            Marshal.FreeHGlobal(extWin32);
            Marshal.FreeHGlobal(extSurface);
            Marshal.FreeHGlobal(engineName);
            Marshal.FreeHGlobal(appName);
        }
    }
}
'@

Add-Type -TypeDefinition $source -Language CSharp
$window = New-Object System.Windows.Forms.Form
$window.Text = "GOW3 Vulkan surface probe"
$window.Width = 320
$window.Height = 180
$window.ShowInTaskbar = $false
$window.Opacity = 0
$window.CreateControl()
try {
    $result = [VulkanWin32SurfaceProbe]::Run($window.Handle, $PreferDevice)
}
finally {
    $window.Dispose()
}
exit $result
