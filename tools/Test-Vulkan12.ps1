param(
    [string] $PreferDevice = "Vega 8"
)

$ErrorActionPreference = "Stop"

# A dependency-free Windows probe. It asks the installed Vulkan loader to create
# a Vulkan 1.2 instance and enumerate actual physical devices; it does not use
# vulkaninfo text parsing and does not access any game files.
$source = @'
using System;
using System.Runtime.InteropServices;

public static class Vulkan12Probe
{
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
    private delegate void DestroyInstanceDelegate(IntPtr instance, IntPtr allocator);

    [StructLayout(LayoutKind.Sequential)]
    private struct ApplicationInfo
    {
        public int sType;
        public IntPtr pNext;
        public IntPtr pApplicationName;
        public uint applicationVersion;
        public IntPtr pEngineName;
        public uint engineVersion;
        public uint apiVersion;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct InstanceCreateInfo
    {
        public int sType;
        public IntPtr pNext;
        public uint flags;
        public IntPtr pApplicationInfo;
        public uint enabledLayerCount;
        public IntPtr ppEnabledLayerNames;
        public uint enabledExtensionCount;
        public IntPtr ppEnabledExtensionNames;
    }

    private static T Load<T>(IntPtr module, string name) where T : class
    {
        IntPtr address = GetProcAddress(module, name);
        if (address == IntPtr.Zero)
            throw new InvalidOperationException("Vulkan loader is missing entry point " + name);
        return Marshal.GetDelegateForFunctionPointer(address, typeof(T)) as T;
    }

    private static string ApiVersion(uint version)
    {
        uint major = version >> 22;
        uint minor = (version >> 12) & 0x3ff;
        uint patch = version & 0xfff;
        return major + "." + minor + "." + patch;
    }

    public static int Run(string preferredDevice)
    {
        IntPtr loader = LoadLibrary("vulkan-1.dll");
        if (loader == IntPtr.Zero)
        {
            Console.Error.WriteLine("FAIL: Windows could not load vulkan-1.dll (Win32 error " + Marshal.GetLastWin32Error() + ").");
            return 2;
        }

        IntPtr appName = Marshal.StringToHGlobalAnsi("GOW3 Vulkan 1.2 capability probe");
        IntPtr engineName = Marshal.StringToHGlobalAnsi("GOW3 feasibility probe");
        IntPtr appInfoMemory = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(ApplicationInfo)));
        IntPtr createInfoMemory = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(InstanceCreateInfo)));
        IntPtr propertiesMemory = Marshal.AllocHGlobal(2048);
        IntPtr instance = IntPtr.Zero;

        try
        {
            var appInfo = new ApplicationInfo {
                sType = 0,
                pNext = IntPtr.Zero,
                pApplicationName = appName,
                applicationVersion = 1,
                pEngineName = engineName,
                engineVersion = 1,
                apiVersion = (1u << 22) | (2u << 12)
            };
            Marshal.StructureToPtr(appInfo, appInfoMemory, false);

            var createInfo = new InstanceCreateInfo {
                sType = 1,
                pNext = IntPtr.Zero,
                flags = 0,
                pApplicationInfo = appInfoMemory,
                enabledLayerCount = 0,
                ppEnabledLayerNames = IntPtr.Zero,
                enabledExtensionCount = 0,
                ppEnabledExtensionNames = IntPtr.Zero
            };
            Marshal.StructureToPtr(createInfo, createInfoMemory, false);

            var createInstance = Load<CreateInstanceDelegate>(loader, "vkCreateInstance");
            int result = createInstance(createInfoMemory, IntPtr.Zero, out instance);
            if (result != 0)
            {
                Console.Error.WriteLine("FAIL: vkCreateInstance requested Vulkan 1.2 and returned VkResult " + result + ".");
                return 3;
            }

            var enumerate = Load<EnumeratePhysicalDevicesDelegate>(loader, "vkEnumeratePhysicalDevices");
            var getProperties = Load<GetPhysicalDevicePropertiesDelegate>(loader, "vkGetPhysicalDeviceProperties");
            var destroy = Load<DestroyInstanceDelegate>(loader, "vkDestroyInstance");

            uint count = 0;
            result = enumerate(instance, ref count, IntPtr.Zero);
            if (result != 0 || count == 0)
            {
                Console.Error.WriteLine("FAIL: no Vulkan physical devices were enumerated (VkResult " + result + ").");
                return 4;
            }

            IntPtr devicesMemory = Marshal.AllocHGlobal(checked((int)count * IntPtr.Size));
            try
            {
                result = enumerate(instance, ref count, devicesMemory);
                if (result != 0)
                {
                    Console.Error.WriteLine("FAIL: device enumeration returned VkResult " + result + ".");
                    return 5;
                }

                bool supported = false;
                bool preferredSupported = false;
                Console.WriteLine("Instance creation: PASS (requested Vulkan 1.2)");
                Console.WriteLine("Physical devices: " + count);
                for (uint i = 0; i < count; ++i)
                {
                    IntPtr device = Marshal.ReadIntPtr(devicesMemory, checked((int)i * IntPtr.Size));
                    for (int offset = 0; offset < 2048; offset += 4)
                        Marshal.WriteInt32(propertiesMemory, offset, 0);
                    getProperties(device, propertiesMemory);

                    uint api = unchecked((uint)Marshal.ReadInt32(propertiesMemory, 0));
                    int type = Marshal.ReadInt32(propertiesMemory, 16);
                    string name = Marshal.PtrToStringAnsi(IntPtr.Add(propertiesMemory, 20)).TrimEnd('\0');
                    bool has12 = (api >> 22) > 1 || ((api >> 22) == 1 && ((api >> 12) & 0x3ff) >= 2);
                    bool isPreferred = !String.IsNullOrWhiteSpace(preferredDevice) && name.IndexOf(preferredDevice, StringComparison.OrdinalIgnoreCase) >= 0;
                    supported |= has12;
                    preferredSupported |= has12 && isPreferred;
                    Console.WriteLine("- " + name + " | device API " + ApiVersion(api) + " | type " + type + " | Vulkan 1.2: " + (has12 ? "PASS" : "FAIL"));
                }

                if (preferredDevice.Length > 0)
                {
                    Console.WriteLine("Preferred device match (\"" + preferredDevice + "\"): " + (preferredSupported ? "PASS" : "NOT FOUND / DOES NOT REPORT 1.2"));
                    if (!preferredSupported)
                    {
                        Console.Error.WriteLine("FAIL: the requested target adapter was not found with Vulkan 1.2 or later.");
                        return 7;
                    }
                }

                if (!supported)
                {
                    Console.Error.WriteLine("FAIL: no enumerated physical device reports Vulkan 1.2 or later.");
                    return 6;
                }

                Console.WriteLine("RESULT: PASS - at least one physical device reports Vulkan 1.2 or later.");
                Console.WriteLine("Note: this checks instance creation and device API versions only; it does not test rendering, swapchain presentation, performance, or game compatibility.");
                destroy(instance, IntPtr.Zero);
                instance = IntPtr.Zero;
                return 0;
            }
            finally
            {
                Marshal.FreeHGlobal(devicesMemory);
            }
        }
        finally
        {
            if (instance != IntPtr.Zero)
                Load<DestroyInstanceDelegate>(loader, "vkDestroyInstance")(instance, IntPtr.Zero);
            Marshal.FreeHGlobal(propertiesMemory);
            Marshal.FreeHGlobal(createInfoMemory);
            Marshal.FreeHGlobal(appInfoMemory);
            Marshal.FreeHGlobal(engineName);
            Marshal.FreeHGlobal(appName);
        }
    }
}
'@

Add-Type -TypeDefinition $source -Language CSharp
exit [Vulkan12Probe]::Run($PreferDevice)
