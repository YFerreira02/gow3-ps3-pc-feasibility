param(
    [string] $PreferDevice = "Vega 8"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Windows.Forms

# Creates a Vulkan 1.2 instance, Win32 surface, logical device, swapchain,
# clears one acquired swapchain image, and presents it. The small hidden window
# keeps this probe nonintrusive. It contains no game code or assets.
$source = @'
using System;
using System.Runtime.InteropServices;

public static class VulkanSwapchainProbe
{
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] private static extern IntPtr GetModuleHandle(string moduleName);
    [DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true)] private static extern IntPtr LoadLibrary(string name);
    [DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true)] private static extern IntPtr GetProcAddress(IntPtr module, string name);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int CreateInstance(IntPtr info, IntPtr alloc, out IntPtr instance);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int EnumDevices(IntPtr instance, ref uint count, IntPtr devices);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void GetProperties(IntPtr physical, IntPtr properties);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void GetQueues(IntPtr physical, ref uint count, IntPtr properties);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int CreateSurface(IntPtr instance, IntPtr info, IntPtr alloc, out IntPtr surface);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int GetSurfaceSupport(IntPtr physical, uint family, IntPtr surface, out uint supported);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int GetSurfaceCaps(IntPtr physical, IntPtr surface, IntPtr caps);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int GetSurfaceFormats(IntPtr physical, IntPtr surface, ref uint count, IntPtr formats);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int GetPresentModes(IntPtr physical, IntPtr surface, ref uint count, IntPtr modes);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate IntPtr GetInstanceProcAddr(IntPtr instance, [MarshalAs(UnmanagedType.LPStr)] string name);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate IntPtr GetDeviceProcAddr(IntPtr device, [MarshalAs(UnmanagedType.LPStr)] string name);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int CreateDevice(IntPtr physical, IntPtr info, IntPtr alloc, out IntPtr device);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void GetDeviceQueue(IntPtr device, uint family, uint index, out IntPtr queue);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int CreateSwapchain(IntPtr device, IntPtr info, IntPtr alloc, out IntPtr swapchain);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int GetSwapchainImages(IntPtr device, IntPtr swapchain, ref uint count, IntPtr images);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int CreateCommandPool(IntPtr device, IntPtr info, IntPtr alloc, out IntPtr pool);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int AllocateCommandBuffers(IntPtr device, IntPtr info, IntPtr buffers);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int CreateSemaphore(IntPtr device, IntPtr info, IntPtr alloc, out IntPtr semaphore);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int AcquireNextImage(IntPtr device, IntPtr swapchain, ulong timeout, IntPtr semaphore, IntPtr fence, out uint imageIndex);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int BeginCommandBuffer(IntPtr buffer, IntPtr info);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void CmdPipelineBarrier(IntPtr buffer, uint srcStage, uint dstStage, uint dependencyFlags, uint memoryCount, IntPtr memoryBarriers, uint bufferCount, IntPtr bufferBarriers, uint imageCount, IntPtr imageBarriers);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void CmdClearColorImage(IntPtr buffer, IntPtr image, int layout, IntPtr color, uint rangeCount, IntPtr ranges);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int EndCommandBuffer(IntPtr buffer);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int QueueSubmit(IntPtr queue, uint count, IntPtr submits, IntPtr fence);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int QueuePresent(IntPtr queue, IntPtr presentInfo);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int QueueWaitIdle(IntPtr queue);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int DeviceWaitIdle(IntPtr device);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void DestroyDevice(IntPtr device, IntPtr alloc);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void DestroyInstance(IntPtr instance, IntPtr alloc);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void DestroySurface(IntPtr instance, IntPtr surface, IntPtr alloc);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void DestroySwapchain(IntPtr device, IntPtr swapchain, IntPtr alloc);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void DestroyCommandPool(IntPtr device, IntPtr pool, IntPtr alloc);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void DestroySemaphore(IntPtr device, IntPtr semaphore, IntPtr alloc);

    [StructLayout(LayoutKind.Sequential)] private struct AppInfo { public int sType; public IntPtr pNext; public IntPtr app; public uint appVer; public IntPtr engine; public uint engineVer; public uint api; }
    [StructLayout(LayoutKind.Sequential)] private struct InstanceInfo { public int sType; public IntPtr pNext; public uint flags; public IntPtr app; public uint layerCount; public IntPtr layers; public uint extCount; public IntPtr extensions; }
    [StructLayout(LayoutKind.Sequential)] private struct SurfaceInfo { public int sType; public IntPtr pNext; public uint flags; public IntPtr hinstance; public IntPtr hwnd; }
    [StructLayout(LayoutKind.Sequential)] private struct DeviceQueueInfo { public int sType; public IntPtr pNext; public uint flags; public uint family; public uint count; public IntPtr priorities; }
    [StructLayout(LayoutKind.Sequential)] private struct DeviceInfo { public int sType; public IntPtr pNext; public uint flags; public uint queueCount; public IntPtr queues; public uint layerCount; public IntPtr layers; public uint extCount; public IntPtr extensions; public IntPtr features; }
    [StructLayout(LayoutKind.Sequential)] private struct Extent2D { public uint width; public uint height; }
    [StructLayout(LayoutKind.Sequential)] private struct SurfaceFormat { public int format; public int colorSpace; }
    [StructLayout(LayoutKind.Sequential)] private struct SwapchainInfo { public int sType; public IntPtr pNext; public uint flags; public IntPtr surface; public uint minImages; public int format; public int colorSpace; public Extent2D extent; public uint layers; public uint usage; public int sharing; public uint familyCount; public IntPtr families; public uint transform; public uint composite; public int presentMode; public uint clipped; public IntPtr oldSwapchain; }
    [StructLayout(LayoutKind.Sequential)] private struct CommandPoolInfo { public int sType; public IntPtr pNext; public uint flags; public uint family; }
    [StructLayout(LayoutKind.Sequential)] private struct CommandAllocInfo { public int sType; public IntPtr pNext; public IntPtr pool; public int level; public uint count; }
    [StructLayout(LayoutKind.Sequential)] private struct SemaphoreInfo { public int sType; public IntPtr pNext; public uint flags; }
    [StructLayout(LayoutKind.Sequential)] private struct CommandBeginInfo { public int sType; public IntPtr pNext; public uint flags; public IntPtr inheritance; }
    [StructLayout(LayoutKind.Sequential)] private struct SubresourceRange { public uint aspect; public uint baseMip; public uint mipCount; public uint baseLayer; public uint layerCount; }
    [StructLayout(LayoutKind.Sequential)] private struct ImageBarrier { public int sType; public IntPtr pNext; public uint srcAccess; public uint dstAccess; public int oldLayout; public int newLayout; public uint srcQueue; public uint dstQueue; public IntPtr image; public SubresourceRange range; }
    [StructLayout(LayoutKind.Sequential)] private struct SubmitInfo { public int sType; public IntPtr pNext; public uint waitCount; public IntPtr waits; public IntPtr waitStages; public uint commandCount; public IntPtr commands; public uint signalCount; public IntPtr signals; }
    [StructLayout(LayoutKind.Sequential)] private struct PresentInfo { public int sType; public IntPtr pNext; public uint waitCount; public IntPtr waits; public uint swapchainCount; public IntPtr swapchains; public IntPtr imageIndices; public IntPtr results; }

    private static T Load<T>(IntPtr module, string name) where T : class { IntPtr p = GetProcAddress(module, name); if (p == IntPtr.Zero) throw new InvalidOperationException("Missing Vulkan entry point " + name); return Marshal.GetDelegateForFunctionPointer(p, typeof(T)) as T; }
    private static T Inst<T>(GetInstanceProcAddr get, IntPtr instance, string name) where T : class { IntPtr p = get(instance, name); if (p == IntPtr.Zero) throw new InvalidOperationException("Missing Vulkan instance function " + name); return Marshal.GetDelegateForFunctionPointer(p, typeof(T)) as T; }
    private static T Dev<T>(GetDeviceProcAddr get, IntPtr device, string name) where T : class { IntPtr p = get(device, name); if (p == IntPtr.Zero) throw new InvalidOperationException("Missing Vulkan device function " + name); return Marshal.GetDelegateForFunctionPointer(p, typeof(T)) as T; }
    private static int Check(int result, string operation) { if (result != 0) throw new InvalidOperationException(operation + " returned VkResult " + result); return result; }

    public static int Run(IntPtr hwnd, string preferred)
    {
        IntPtr loader = LoadLibrary("vulkan-1.dll");
        if (loader == IntPtr.Zero) { Console.Error.WriteLine("FAIL: cannot load vulkan-1.dll"); return 2; }
        IntPtr appName = Marshal.StringToHGlobalAnsi("GOW3 Vulkan swapchain probe"), engineName = Marshal.StringToHGlobalAnsi("GOW3 feasibility probe");
        string[] instanceExtNames = { "VK_KHR_surface", "VK_KHR_win32_surface" };
        string[] deviceExtNames = { "VK_KHR_swapchain" };
        IntPtr instanceExtStrings = Marshal.AllocHGlobal(instanceExtNames.Length * IntPtr.Size), deviceExtString = Marshal.StringToHGlobalAnsi(deviceExtNames[0]);
        IntPtr surfaceInfoMem = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(SurfaceInfo))), appMem = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(AppInfo))), instanceMem = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(InstanceInfo)));
        IntPtr instance = IntPtr.Zero, surface = IntPtr.Zero, physical = IntPtr.Zero, device = IntPtr.Zero, queue = IntPtr.Zero, swapchain = IntPtr.Zero, pool = IntPtr.Zero, acquireSemaphore = IntPtr.Zero, renderSemaphore = IntPtr.Zero;
        GetInstanceProcAddr getInst = null; GetDeviceProcAddr getDev = null;
        try
        {
            for (int i = 0; i < instanceExtNames.Length; ++i) Marshal.WriteIntPtr(instanceExtStrings, i * IntPtr.Size, Marshal.StringToHGlobalAnsi(instanceExtNames[i]));
            var app = new AppInfo { sType=0, pNext=IntPtr.Zero, app=appName, appVer=1, engine=engineName, engineVer=1, api=(1u<<22)|(2u<<12) };
            Marshal.StructureToPtr(app, appMem, false);
            var ci = new InstanceInfo { sType=1, pNext=IntPtr.Zero, flags=0, app=appMem, layerCount=0, layers=IntPtr.Zero, extCount=2, extensions=instanceExtStrings };
            Marshal.StructureToPtr(ci, instanceMem, false);
            Check(Load<CreateInstance>(loader, "vkCreateInstance")(instanceMem, IntPtr.Zero, out instance), "vkCreateInstance");
            getInst = Load<GetInstanceProcAddr>(loader, "vkGetInstanceProcAddr");
            var createSurface = Inst<CreateSurface>(getInst, instance, "vkCreateWin32SurfaceKHR");
            var surfaceCreate = new SurfaceInfo { sType=1000009000, pNext=IntPtr.Zero, flags=0, hinstance=GetModuleHandle(null), hwnd=hwnd };
            Marshal.StructureToPtr(surfaceCreate, surfaceInfoMem, false);
            Check(createSurface(instance, surfaceInfoMem, IntPtr.Zero, out surface), "vkCreateWin32SurfaceKHR");

            var enumDevices = Load<EnumDevices>(loader, "vkEnumeratePhysicalDevices"); uint deviceCount=0;
            Check(enumDevices(instance, ref deviceCount, IntPtr.Zero), "vkEnumeratePhysicalDevices(count)");
            IntPtr devices = Marshal.AllocHGlobal(checked((int)deviceCount*IntPtr.Size)), props = Marshal.AllocHGlobal(2048);
            try
            {
                Check(enumDevices(instance, ref deviceCount, devices), "vkEnumeratePhysicalDevices(list)");
                var getProperties=Load<GetProperties>(loader,"vkGetPhysicalDeviceProperties");
                var getQueues=Load<GetQueues>(loader,"vkGetPhysicalDeviceQueueFamilyProperties");
                var getSupport=Inst<GetSurfaceSupport>(getInst,instance,"vkGetPhysicalDeviceSurfaceSupportKHR");
                var getCaps=Inst<GetSurfaceCaps>(getInst,instance,"vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
                var getFormats=Inst<GetSurfaceFormats>(getInst,instance,"vkGetPhysicalDeviceSurfaceFormatsKHR");
                var getModes=Inst<GetPresentModes>(getInst,instance,"vkGetPhysicalDeviceSurfacePresentModesKHR");
                uint family=UInt32.MaxValue; string deviceName=null;
                const int qSize=24;
                for(uint i=0;i<deviceCount;i++)
                {
                    IntPtr candidate=Marshal.ReadIntPtr(devices,checked((int)i*IntPtr.Size)); getProperties(candidate,props);
                    string name=Marshal.PtrToStringAnsi(IntPtr.Add(props,20)).TrimEnd('\0');
                    if(String.IsNullOrWhiteSpace(preferred)||name.IndexOf(preferred,StringComparison.OrdinalIgnoreCase)>=0)
                    {
                        uint qn=0;getQueues(candidate,ref qn,IntPtr.Zero);IntPtr qmem=Marshal.AllocHGlobal(checked((int)qn*qSize));
                        try { getQueues(candidate,ref qn,qmem); for(uint q=0;q<qn;q++){IntPtr qe=IntPtr.Add(qmem,checked((int)q*qSize));uint flags=unchecked((uint)Marshal.ReadInt32(qe,0)), queues=unchecked((uint)Marshal.ReadInt32(qe,4));uint canPresent; if((flags&1)!=0&&queues>0&&getSupport(candidate,q,surface,out canPresent)==0&&canPresent!=0){physical=candidate;family=q;deviceName=name;break;}} }
                        finally{Marshal.FreeHGlobal(qmem);} if(physical!=IntPtr.Zero)break;
                    }
                }
                if(physical==IntPtr.Zero)throw new InvalidOperationException("Preferred adapter/graphics-present queue not found: "+preferred);
                Console.WriteLine("Adapter: "+deviceName+" | queue family "+family);

                IntPtr priority=Marshal.AllocHGlobal(sizeof(float)), qInfoMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(DeviceQueueInfo))), devInfoMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(DeviceInfo)));
                IntPtr devQueueName=deviceExtString; IntPtr devExtArray=Marshal.AllocHGlobal(IntPtr.Size); Marshal.WriteIntPtr(devExtArray,devQueueName);
                try
                {
                    Marshal.WriteInt32(priority,BitConverter.ToInt32(BitConverter.GetBytes(1.0f),0));
                    var qi=new DeviceQueueInfo{sType=2,pNext=IntPtr.Zero,flags=0,family=family,count=1,priorities=priority};Marshal.StructureToPtr(qi,qInfoMem,false);
                    var di=new DeviceInfo{sType=3,pNext=IntPtr.Zero,flags=0,queueCount=1,queues=qInfoMem,layerCount=0,layers=IntPtr.Zero,extCount=1,extensions=devExtArray,features=IntPtr.Zero};Marshal.StructureToPtr(di,devInfoMem,false);
                    Check(Load<CreateDevice>(loader,"vkCreateDevice")(physical,devInfoMem,IntPtr.Zero,out device),"vkCreateDevice(VK_KHR_swapchain)");
                }
                finally{Marshal.FreeHGlobal(devExtArray);Marshal.FreeHGlobal(devInfoMem);Marshal.FreeHGlobal(qInfoMem);Marshal.FreeHGlobal(priority);}

                getDev=Load<GetDeviceProcAddr>(loader,"vkGetDeviceProcAddr");Dev<GetDeviceQueue>(getDev,device,"vkGetDeviceQueue")(device,family,0,out queue);
                if(queue==IntPtr.Zero)throw new InvalidOperationException("vkGetDeviceQueue returned null");

                IntPtr caps=Marshal.AllocHGlobal(64);try
                {
                    Check(getCaps(physical,surface,caps),"vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
                    uint minImages=unchecked((uint)Marshal.ReadInt32(caps,0)), maxImages=unchecked((uint)Marshal.ReadInt32(caps,4));
                    uint width=unchecked((uint)Marshal.ReadInt32(caps,8)),height=unchecked((uint)Marshal.ReadInt32(caps,12));
                    uint supportedTransforms=unchecked((uint)Marshal.ReadInt32(caps,36)), currentTransform=unchecked((uint)Marshal.ReadInt32(caps,40)), compositeSupport=unchecked((uint)Marshal.ReadInt32(caps,44)), usageSupport=unchecked((uint)Marshal.ReadInt32(caps,48));
                    if((usageSupport&2)==0)throw new InvalidOperationException("Surface does not support transfer-destination swapchain images required by this no-shader clear probe");
                    if(width==UInt32.MaxValue){width=320;height=180;}
                    if(width==0||height==0)throw new InvalidOperationException("Surface extent is zero; cannot create a swapchain");
                    uint imageCount=minImages+1;if(maxImages>0&&imageCount>maxImages)imageCount=maxImages;
                    uint transform=(supportedTransforms&currentTransform)!=0?currentTransform:1;
                    uint composite=(compositeSupport&1)!=0?1u:(compositeSupport&2)!=0?2u:(compositeSupport&4)!=0?4u:(compositeSupport&8)!=0?8u:0u;
                    if(composite==0)throw new InvalidOperationException("No supported composite alpha mode");
                    uint fmtCount=0;Check(getFormats(physical,surface,ref fmtCount,IntPtr.Zero),"surface formats(count)");IntPtr fmtMem=Marshal.AllocHGlobal(checked((int)fmtCount*8));SurfaceFormat chosen;
                    try{Check(getFormats(physical,surface,ref fmtCount,fmtMem),"surface formats(list)");if(fmtCount==0)throw new InvalidOperationException("No surface formats");chosen=(SurfaceFormat)Marshal.PtrToStructure(fmtMem,typeof(SurfaceFormat));}finally{Marshal.FreeHGlobal(fmtMem);}
                    uint modeCount=0;Check(getModes(physical,surface,ref modeCount,IntPtr.Zero),"present modes(count)");IntPtr modeMem=Marshal.AllocHGlobal(checked((int)modeCount*4));bool fifo=false;
                    try{Check(getModes(physical,surface,ref modeCount,modeMem),"present modes(list)");for(uint m=0;m<modeCount;m++)if(Marshal.ReadInt32(modeMem,checked((int)m*4))==2)fifo=true;}finally{Marshal.FreeHGlobal(modeMem);}
                    if(!fifo)throw new InvalidOperationException("Guaranteed FIFO present mode was not enumerated");
                    var sci=new SwapchainInfo{sType=1000001000,pNext=IntPtr.Zero,flags=0,surface=surface,minImages=imageCount,format=chosen.format,colorSpace=chosen.colorSpace,extent=new Extent2D{width=width,height=height},layers=1,usage=2,sharing=0,familyCount=0,families=IntPtr.Zero,transform=transform,composite=composite,presentMode=2,clipped=1,oldSwapchain=IntPtr.Zero};
                    IntPtr sciMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(SwapchainInfo)));try{Marshal.StructureToPtr(sci,sciMem,false);Check(Dev<CreateSwapchain>(getDev,device,"vkCreateSwapchainKHR")(device,sciMem,IntPtr.Zero,out swapchain),"vkCreateSwapchainKHR");}finally{Marshal.FreeHGlobal(sciMem);}
                    Console.WriteLine("Swapchain: PASS | extent "+width+"x"+height+" | FIFO present");
                }
                finally{Marshal.FreeHGlobal(caps);}

                uint imageCountOut=0;var getImages=Dev<GetSwapchainImages>(getDev,device,"vkGetSwapchainImagesKHR");Check(getImages(device,swapchain,ref imageCountOut,IntPtr.Zero),"vkGetSwapchainImagesKHR(count)");IntPtr images=Marshal.AllocHGlobal(checked((int)imageCountOut*IntPtr.Size));
                try
                {
                    Check(getImages(device,swapchain,ref imageCountOut,images),"vkGetSwapchainImagesKHR(list)");
                    var poolInfo=new CommandPoolInfo{sType=39,pNext=IntPtr.Zero,flags=0,family=family};IntPtr poolInfoMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(CommandPoolInfo)));try{Marshal.StructureToPtr(poolInfo,poolInfoMem,false);Check(Dev<CreateCommandPool>(getDev,device,"vkCreateCommandPool")(device,poolInfoMem,IntPtr.Zero,out pool),"vkCreateCommandPool");}finally{Marshal.FreeHGlobal(poolInfoMem);}
                    IntPtr commandBufferMemory=Marshal.AllocHGlobal(IntPtr.Size),commandBuffer;var allocInfo=new CommandAllocInfo{sType=40,pNext=IntPtr.Zero,pool=pool,level=0,count=1};IntPtr allocMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(CommandAllocInfo)));try{Marshal.StructureToPtr(allocInfo,allocMem,false);Check(Dev<AllocateCommandBuffers>(getDev,device,"vkAllocateCommandBuffers")(device,allocMem,commandBufferMemory),"vkAllocateCommandBuffers");commandBuffer=Marshal.ReadIntPtr(commandBufferMemory);}finally{Marshal.FreeHGlobal(allocMem);Marshal.FreeHGlobal(commandBufferMemory);}
                    var semInfo=new SemaphoreInfo{sType=9,pNext=IntPtr.Zero,flags=0};IntPtr semMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(SemaphoreInfo)));try{Marshal.StructureToPtr(semInfo,semMem,false);Check(Dev<CreateSemaphore>(getDev,device,"vkCreateSemaphore")(device,semMem,IntPtr.Zero,out acquireSemaphore),"create acquire semaphore");Check(Dev<CreateSemaphore>(getDev,device,"vkCreateSemaphore")(device,semMem,IntPtr.Zero,out renderSemaphore),"create render semaphore");}finally{Marshal.FreeHGlobal(semMem);}
                    uint imageIndex;int acquireResult=Dev<AcquireNextImage>(getDev,device,"vkAcquireNextImageKHR")(device,swapchain,UInt64.MaxValue,acquireSemaphore,IntPtr.Zero,out imageIndex);if(acquireResult!=0&&acquireResult!=1000001003)Check(acquireResult,"vkAcquireNextImageKHR");
                    var begin=new CommandBeginInfo{sType=42,pNext=IntPtr.Zero,flags=1,inheritance=IntPtr.Zero};IntPtr beginMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(CommandBeginInfo)));try{Marshal.StructureToPtr(begin,beginMem,false);Check(Dev<BeginCommandBuffer>(getDev,device,"vkBeginCommandBuffer")(commandBuffer,beginMem),"vkBeginCommandBuffer");}finally{Marshal.FreeHGlobal(beginMem);}
                    IntPtr image=Marshal.ReadIntPtr(images,checked((int)imageIndex*IntPtr.Size));var range=new SubresourceRange{aspect=1,baseMip=0,mipCount=1,baseLayer=0,layerCount=1};IntPtr rangeMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(SubresourceRange)));Marshal.StructureToPtr(range,rangeMem,false);
                    IntPtr barrierMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(ImageBarrier)));
                    try
                    {
                        var barrier=new ImageBarrier{sType=45,pNext=IntPtr.Zero,srcAccess=0,dstAccess=0x1000,oldLayout=0,newLayout=7,srcQueue=UInt32.MaxValue,dstQueue=UInt32.MaxValue,image=image,range=range};Marshal.StructureToPtr(barrier,barrierMem,false);
                        Dev<CmdPipelineBarrier>(getDev,device,"vkCmdPipelineBarrier")(commandBuffer,1,0x1000,0,0,IntPtr.Zero,0,IntPtr.Zero,1,barrierMem);
                        IntPtr color=Marshal.AllocHGlobal(16);try{Marshal.WriteInt32(color,BitConverter.ToInt32(BitConverter.GetBytes(0.08f),0));Marshal.WriteInt32(color,4,BitConverter.ToInt32(BitConverter.GetBytes(0.16f),0));Marshal.WriteInt32(color,8,BitConverter.ToInt32(BitConverter.GetBytes(0.32f),0));Marshal.WriteInt32(color,12,BitConverter.ToInt32(BitConverter.GetBytes(1.0f),0));Dev<CmdClearColorImage>(getDev,device,"vkCmdClearColorImage")(commandBuffer,image,7,color,1,rangeMem);}finally{Marshal.FreeHGlobal(color);}
                        barrier.srcAccess=0x1000;barrier.dstAccess=0;barrier.oldLayout=7;barrier.newLayout=1000001002;Marshal.StructureToPtr(barrier,barrierMem,false);Dev<CmdPipelineBarrier>(getDev,device,"vkCmdPipelineBarrier")(commandBuffer,0x1000,0x2000,0,0,IntPtr.Zero,0,IntPtr.Zero,1,barrierMem);
                    }
                    finally{Marshal.FreeHGlobal(barrierMem);Marshal.FreeHGlobal(rangeMem);}
                    Check(Dev<EndCommandBuffer>(getDev,device,"vkEndCommandBuffer")(commandBuffer),"vkEndCommandBuffer");
                    IntPtr waits=Marshal.AllocHGlobal(IntPtr.Size),signals=Marshal.AllocHGlobal(IntPtr.Size),commands=Marshal.AllocHGlobal(IntPtr.Size),stages=Marshal.AllocHGlobal(4),submitMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(SubmitInfo)));
                    try
                    {
                        Marshal.WriteIntPtr(waits,acquireSemaphore);Marshal.WriteIntPtr(signals,renderSemaphore);Marshal.WriteIntPtr(commands,commandBuffer);Marshal.WriteInt32(stages,unchecked((int)0x1000));
                        var submit=new SubmitInfo{sType=4,pNext=IntPtr.Zero,waitCount=1,waits=waits,waitStages=stages,commandCount=1,commands=commands,signalCount=1,signals=signals};Marshal.StructureToPtr(submit,submitMem,false);Check(Dev<QueueSubmit>(getDev,device,"vkQueueSubmit")(queue,1,submitMem,IntPtr.Zero),"vkQueueSubmit");
                        IntPtr presentWaits=Marshal.AllocHGlobal(IntPtr.Size),swapchains=Marshal.AllocHGlobal(IntPtr.Size),indices=Marshal.AllocHGlobal(4),presentMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(PresentInfo)));
                        try{Marshal.WriteIntPtr(presentWaits,renderSemaphore);Marshal.WriteIntPtr(swapchains,swapchain);Marshal.WriteInt32(indices,unchecked((int)imageIndex));var present=new PresentInfo{sType=1000001001,pNext=IntPtr.Zero,waitCount=1,waits=presentWaits,swapchainCount=1,swapchains=swapchains,imageIndices=indices,results=IntPtr.Zero};Marshal.StructureToPtr(present,presentMem,false);int pr=Dev<QueuePresent>(getDev,device,"vkQueuePresentKHR")(queue,presentMem);if(pr!=0&&pr!=1000001003)Check(pr,"vkQueuePresentKHR");Check(Dev<QueueWaitIdle>(getDev,device,"vkQueueWaitIdle")(queue),"vkQueueWaitIdle");Console.WriteLine("Synthetic clear frame present: PASS | one image submitted and presented");}
                        finally{Marshal.FreeHGlobal(presentMem);Marshal.FreeHGlobal(indices);Marshal.FreeHGlobal(swapchains);Marshal.FreeHGlobal(presentWaits);}
                    }
                    finally{Marshal.FreeHGlobal(submitMem);Marshal.FreeHGlobal(stages);Marshal.FreeHGlobal(commands);Marshal.FreeHGlobal(signals);Marshal.FreeHGlobal(waits);}
                }
                finally{Marshal.FreeHGlobal(images);}
                Console.WriteLine("RESULT: PASS - Vulkan swapchain acquired, cleared, and presented one frame.");
                return 0;
            }
            finally{Marshal.FreeHGlobal(props);Marshal.FreeHGlobal(devices);}
        }
        catch(Exception ex){Console.Error.WriteLine("FAIL: "+ex.Message);return 10;}
        finally
        {
            if(device!=IntPtr.Zero){try{Dev<DeviceWaitIdle>(getDev,device,"vkDeviceWaitIdle")(device);}catch{} if(acquireSemaphore!=IntPtr.Zero)try{Dev<DestroySemaphore>(getDev,device,"vkDestroySemaphore")(device,acquireSemaphore,IntPtr.Zero);}catch{} if(renderSemaphore!=IntPtr.Zero)try{Dev<DestroySemaphore>(getDev,device,"vkDestroySemaphore")(device,renderSemaphore,IntPtr.Zero);}catch{} if(pool!=IntPtr.Zero)try{Dev<DestroyCommandPool>(getDev,device,"vkDestroyCommandPool")(device,pool,IntPtr.Zero);}catch{} if(swapchain!=IntPtr.Zero)try{Dev<DestroySwapchain>(getDev,device,"vkDestroySwapchainKHR")(device,swapchain,IntPtr.Zero);}catch{} try{Dev<DestroyDevice>(getDev,device,"vkDestroyDevice")(device,IntPtr.Zero);}catch{}}
            if(instance!=IntPtr.Zero){if(surface!=IntPtr.Zero)try{Inst<DestroySurface>(getInst,instance,"vkDestroySurfaceKHR")(instance,surface,IntPtr.Zero);}catch{} try{Load<DestroyInstance>(loader,"vkDestroyInstance")(instance,IntPtr.Zero);}catch{}}
            for(int i=0;i<instanceExtNames.Length;i++)Marshal.FreeHGlobal(Marshal.ReadIntPtr(instanceExtStrings,i*IntPtr.Size));
            Marshal.FreeHGlobal(deviceExtString);Marshal.FreeHGlobal(instanceExtStrings);Marshal.FreeHGlobal(surfaceInfoMem);Marshal.FreeHGlobal(instanceMem);Marshal.FreeHGlobal(appMem);Marshal.FreeHGlobal(engineName);Marshal.FreeHGlobal(appName);
        }
    }
}
'@

Add-Type -TypeDefinition $source -Language CSharp
$window = New-Object System.Windows.Forms.Form
$window.Text = "GOW3 Vulkan swapchain probe"
$window.Width = 320
$window.Height = 180
$window.ShowInTaskbar = $false
$window.Opacity = 0
$window.CreateControl()
try {
    $result = [VulkanSwapchainProbe]::Run($window.Handle, $PreferDevice)
}
finally {
    $window.Dispose()
}
exit $result
