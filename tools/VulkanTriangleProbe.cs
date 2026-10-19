using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;

public static class VulkanTriangleProbe
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
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int CreateShaderModule(IntPtr device, IntPtr info, IntPtr alloc, out IntPtr module);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void DestroyShaderModule(IntPtr device, IntPtr module, IntPtr alloc);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int CreateRenderPass(IntPtr device, IntPtr info, IntPtr alloc, out IntPtr renderPass);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void DestroyRenderPass(IntPtr device, IntPtr renderPass, IntPtr alloc);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int CreateImageView(IntPtr device, IntPtr info, IntPtr alloc, out IntPtr imageView);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void DestroyImageView(IntPtr device, IntPtr imageView, IntPtr alloc);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int CreateFramebuffer(IntPtr device, IntPtr info, IntPtr alloc, out IntPtr framebuffer);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void DestroyFramebuffer(IntPtr device, IntPtr framebuffer, IntPtr alloc);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int CreatePipelineLayout(IntPtr device, IntPtr info, IntPtr alloc, out IntPtr layout);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void DestroyPipelineLayout(IntPtr device, IntPtr layout, IntPtr alloc);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int CreateGraphicsPipelines(IntPtr device, IntPtr cache, uint count, IntPtr infos, IntPtr alloc, IntPtr pipelines);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void DestroyPipeline(IntPtr device, IntPtr pipeline, IntPtr alloc);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int AcquireNextImage(IntPtr device, IntPtr swapchain, ulong timeout, IntPtr semaphore, IntPtr fence, out uint imageIndex);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate int BeginCommandBuffer(IntPtr buffer, IntPtr info);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void CmdBeginRenderPass(IntPtr buffer, IntPtr info, int contents);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void CmdBindPipeline(IntPtr buffer, int bindPoint, IntPtr pipeline);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void CmdDraw(IntPtr buffer, uint vertexCount, uint instanceCount, uint firstVertex, uint firstInstance);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)] private delegate void CmdEndRenderPass(IntPtr buffer);
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
    [StructLayout(LayoutKind.Sequential)] private struct SubmitInfo { public int sType; public IntPtr pNext; public uint waitCount; public IntPtr waits; public IntPtr waitStages; public uint commandCount; public IntPtr commands; public uint signalCount; public IntPtr signals; }
    [StructLayout(LayoutKind.Sequential)] private struct PresentInfo { public int sType; public IntPtr pNext; public uint waitCount; public IntPtr waits; public uint swapchainCount; public IntPtr swapchains; public IntPtr imageIndices; public IntPtr results; }
    [StructLayout(LayoutKind.Sequential)] private struct ShaderModuleInfo { public int sType; public IntPtr pNext; public uint flags; public UIntPtr codeSize; public IntPtr code; }
    [StructLayout(LayoutKind.Sequential)] private struct PipelineShaderStageInfo { public int sType; public IntPtr pNext; public uint flags; public uint stage; public IntPtr module; public IntPtr name; public IntPtr specialization; }
    [StructLayout(LayoutKind.Sequential)] private struct VertexInputInfo { public int sType; public IntPtr pNext; public uint flags; public uint bindingCount; public IntPtr bindings; public uint attributeCount; public IntPtr attributes; }
    [StructLayout(LayoutKind.Sequential)] private struct InputAssemblyInfo { public int sType; public IntPtr pNext; public uint flags; public int topology; public uint primitiveRestart; }
    [StructLayout(LayoutKind.Sequential)] private struct Viewport { public float x; public float y; public float width; public float height; public float minDepth; public float maxDepth; }
    [StructLayout(LayoutKind.Sequential)] private struct Offset2D { public int x; public int y; }
    [StructLayout(LayoutKind.Sequential)] private struct Rect2D { public Offset2D offset; public Extent2D extent; }
    [StructLayout(LayoutKind.Sequential)] private struct ViewportStateInfo { public int sType; public IntPtr pNext; public uint flags; public uint viewportCount; public IntPtr viewports; public uint scissorCount; public IntPtr scissors; }
    [StructLayout(LayoutKind.Sequential)] private struct RasterizationInfo { public int sType; public IntPtr pNext; public uint flags; public uint depthClamp; public uint discard; public int polygonMode; public uint cullMode; public int frontFace; public uint depthBias; public float biasConstant; public float biasClamp; public float biasSlope; public float lineWidth; }
    [StructLayout(LayoutKind.Sequential)] private struct MultisampleInfo { public int sType; public IntPtr pNext; public uint flags; public uint samples; public uint sampleShading; public float minSampleShading; public IntPtr sampleMask; public uint alphaToCoverage; public uint alphaToOne; }
    [StructLayout(LayoutKind.Sequential)] private struct ColorBlendAttachment { public uint blendEnable; public int srcColor; public int dstColor; public int colorOp; public int srcAlpha; public int dstAlpha; public int alphaOp; public uint writeMask; }
    [StructLayout(LayoutKind.Sequential)] private struct ColorBlendInfo { public int sType; public IntPtr pNext; public uint flags; public uint logicEnable; public int logicOp; public uint attachmentCount; public IntPtr attachments; public float blend0; public float blend1; public float blend2; public float blend3; }
    [StructLayout(LayoutKind.Sequential)] private struct PipelineLayoutInfo { public int sType; public IntPtr pNext; public uint flags; public uint setLayoutCount; public IntPtr setLayouts; public uint pushCount; public IntPtr pushRanges; }
    [StructLayout(LayoutKind.Sequential)] private struct GraphicsPipelineInfo { public int sType; public IntPtr pNext; public uint flags; public uint stageCount; public IntPtr stages; public IntPtr vertexInput; public IntPtr inputAssembly; public IntPtr tessellation; public IntPtr viewport; public IntPtr rasterization; public IntPtr multisample; public IntPtr depthStencil; public IntPtr colorBlend; public IntPtr dynamicState; public IntPtr layout; public IntPtr renderPass; public uint subpass; public IntPtr basePipeline; public int basePipelineIndex; }
    [StructLayout(LayoutKind.Sequential)] private struct AttachmentDescription { public uint flags; public int format; public uint samples; public int loadOp; public int storeOp; public int stencilLoad; public int stencilStore; public int initialLayout; public int finalLayout; }
    [StructLayout(LayoutKind.Sequential)] private struct AttachmentReference { public uint attachment; public int layout; }
    [StructLayout(LayoutKind.Sequential)] private struct SubpassDescription { public uint flags; public int bindPoint; public uint inputCount; public IntPtr inputs; public uint colorCount; public IntPtr colors; public IntPtr resolves; public IntPtr depth; public uint preserveCount; public IntPtr preserves; }
    [StructLayout(LayoutKind.Sequential)] private struct SubpassDependency { public uint srcSubpass; public uint dstSubpass; public uint srcStage; public uint dstStage; public uint srcAccess; public uint dstAccess; public uint flags; }
    [StructLayout(LayoutKind.Sequential)] private struct RenderPassInfo { public int sType; public IntPtr pNext; public uint flags; public uint attachmentCount; public IntPtr attachments; public uint subpassCount; public IntPtr subpasses; public uint dependencyCount; public IntPtr dependencies; }
    [StructLayout(LayoutKind.Sequential)] private struct ImageViewInfo { public int sType; public IntPtr pNext; public uint flags; public IntPtr image; public int viewType; public int format; public int componentR; public int componentG; public int componentB; public int componentA; public SubresourceRange range; }
    [StructLayout(LayoutKind.Sequential)] private struct FramebufferInfo { public int sType; public IntPtr pNext; public uint flags; public IntPtr renderPass; public uint attachmentCount; public IntPtr attachments; public uint width; public uint height; public uint layers; }
    [StructLayout(LayoutKind.Sequential)] private struct ClearValue { public float r; public float g; public float b; public float a; }
    [StructLayout(LayoutKind.Sequential)] private struct RenderPassBeginInfo { public int sType; public IntPtr pNext; public IntPtr renderPass; public IntPtr framebuffer; public Rect2D area; public uint clearCount; public IntPtr clears; }

    private static T Load<T>(IntPtr module, string name) where T : class { IntPtr p = GetProcAddress(module, name); if (p == IntPtr.Zero) throw new InvalidOperationException("Missing Vulkan entry point " + name); return Marshal.GetDelegateForFunctionPointer(p, typeof(T)) as T; }
    private static T Inst<T>(GetInstanceProcAddr get, IntPtr instance, string name) where T : class { IntPtr p = get(instance, name); if (p == IntPtr.Zero) throw new InvalidOperationException("Missing Vulkan instance function " + name); return Marshal.GetDelegateForFunctionPointer(p, typeof(T)) as T; }
    private static T Dev<T>(GetDeviceProcAddr get, IntPtr device, string name) where T : class { IntPtr p = get(device, name); if (p == IntPtr.Zero) throw new InvalidOperationException("Missing Vulkan device function " + name); return Marshal.GetDelegateForFunctionPointer(p, typeof(T)) as T; }
    private static int Check(int result, string operation) { if (result != 0) throw new InvalidOperationException(operation + " returned VkResult " + result); return result; }

    private static IntPtr CreateModule(IntPtr device, GetDeviceProcAddr getDev, string path)
    {
        byte[] bytes = File.ReadAllBytes(path);
        if (bytes.Length < 20 || (bytes.Length & 3) != 0) throw new InvalidOperationException("Invalid SPIR-V file: " + path);
        IntPtr code=Marshal.AllocHGlobal(bytes.Length), infoMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(ShaderModuleInfo)));
        try
        {
            Marshal.Copy(bytes,0,code,bytes.Length);var info=new ShaderModuleInfo{sType=16,pNext=IntPtr.Zero,flags=0,codeSize=(UIntPtr)(uint)bytes.Length,code=code};Marshal.StructureToPtr(info,infoMem,false);IntPtr module;
            Check(Dev<CreateShaderModule>(getDev,device,"vkCreateShaderModule")(device,infoMem,IntPtr.Zero,out module),"vkCreateShaderModule");return module;
        }
        finally{Marshal.FreeHGlobal(infoMem);Marshal.FreeHGlobal(code);}
    }

    private static IntPtr CreateRenderPassObject(IntPtr device, GetDeviceProcAddr getDev, int format)
    {
        IntPtr attachmentMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(AttachmentDescription))),refMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(AttachmentReference))),subpassMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(SubpassDescription))),depMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(SubpassDependency))),infoMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(RenderPassInfo)));
        try
        {
            Marshal.StructureToPtr(new AttachmentDescription{flags=0,format=format,samples=1,loadOp=1,storeOp=0,stencilLoad=2,stencilStore=1,initialLayout=0,finalLayout=1000001002},attachmentMem,false);
            Marshal.StructureToPtr(new AttachmentReference{attachment=0,layout=2},refMem,false);
            Marshal.StructureToPtr(new SubpassDescription{flags=0,bindPoint=0,inputCount=0,inputs=IntPtr.Zero,colorCount=1,colors=refMem,resolves=IntPtr.Zero,depth=IntPtr.Zero,preserveCount=0,preserves=IntPtr.Zero},subpassMem,false);
            Marshal.StructureToPtr(new SubpassDependency{srcSubpass=UInt32.MaxValue,dstSubpass=0,srcStage=1,dstStage=0x400,srcAccess=0,dstAccess=0x100,flags=0},depMem,false);
            var info=new RenderPassInfo{sType=38,pNext=IntPtr.Zero,flags=0,attachmentCount=1,attachments=attachmentMem,subpassCount=1,subpasses=subpassMem,dependencyCount=1,dependencies=depMem};Marshal.StructureToPtr(info,infoMem,false);IntPtr renderPass;
            Check(Dev<CreateRenderPass>(getDev,device,"vkCreateRenderPass")(device,infoMem,IntPtr.Zero,out renderPass),"vkCreateRenderPass");return renderPass;
        }
        finally{Marshal.FreeHGlobal(infoMem);Marshal.FreeHGlobal(depMem);Marshal.FreeHGlobal(subpassMem);Marshal.FreeHGlobal(refMem);Marshal.FreeHGlobal(attachmentMem);}
    }

    private static IntPtr CreateGraphicsPipeline(IntPtr device, GetDeviceProcAddr getDev, IntPtr renderPass, uint width, uint height, IntPtr vertexModule, IntPtr fragmentModule, out IntPtr layout)
    {
        layout=IntPtr.Zero;IntPtr mainName=Marshal.StringToHGlobalAnsi("main"),stages=Marshal.AllocHGlobal(2*Marshal.SizeOf(typeof(PipelineShaderStageInfo)));
        IntPtr vertexInput=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(VertexInputInfo))),inputAssembly=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(InputAssemblyInfo))),viewport=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(Viewport))),scissor=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(Rect2D))),viewportState=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(ViewportStateInfo))),raster=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(RasterizationInfo))),multisample=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(MultisampleInfo))),blendAttachment=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(ColorBlendAttachment))),blend=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(ColorBlendInfo))),layoutInfo=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(PipelineLayoutInfo))),pipelineInfo=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(GraphicsPipelineInfo))),pipelineOut=Marshal.AllocHGlobal(IntPtr.Size);
        try
        {
            int stageSize=Marshal.SizeOf(typeof(PipelineShaderStageInfo));Marshal.StructureToPtr(new PipelineShaderStageInfo{sType=18,pNext=IntPtr.Zero,flags=0,stage=1,module=vertexModule,name=mainName,specialization=IntPtr.Zero},stages,false);Marshal.StructureToPtr(new PipelineShaderStageInfo{sType=18,pNext=IntPtr.Zero,flags=0,stage=0x10,module=fragmentModule,name=mainName,specialization=IntPtr.Zero},IntPtr.Add(stages,stageSize),false);
            Marshal.StructureToPtr(new VertexInputInfo{sType=19,pNext=IntPtr.Zero,flags=0,bindingCount=0,bindings=IntPtr.Zero,attributeCount=0,attributes=IntPtr.Zero},vertexInput,false);
            Marshal.StructureToPtr(new InputAssemblyInfo{sType=20,pNext=IntPtr.Zero,flags=0,topology=3,primitiveRestart=0},inputAssembly,false);
            Marshal.StructureToPtr(new Viewport{x=0,y=0,width=width,height=height,minDepth=0,maxDepth=1},viewport,false);Marshal.StructureToPtr(new Rect2D{offset=new Offset2D{x=0,y=0},extent=new Extent2D{width=width,height=height}},scissor,false);
            Marshal.StructureToPtr(new ViewportStateInfo{sType=22,pNext=IntPtr.Zero,flags=0,viewportCount=1,viewports=viewport,scissorCount=1,scissors=scissor},viewportState,false);
            Marshal.StructureToPtr(new RasterizationInfo{sType=23,pNext=IntPtr.Zero,flags=0,depthClamp=0,discard=0,polygonMode=0,cullMode=0,frontFace=0,depthBias=0,biasConstant=0,biasClamp=0,biasSlope=0,lineWidth=1},raster,false);
            Marshal.StructureToPtr(new MultisampleInfo{sType=24,pNext=IntPtr.Zero,flags=0,samples=1,sampleShading=0,minSampleShading=0,sampleMask=IntPtr.Zero,alphaToCoverage=0,alphaToOne=0},multisample,false);
            Marshal.StructureToPtr(new ColorBlendAttachment{blendEnable=0,srcColor=1,dstColor=0,colorOp=0,srcAlpha=1,dstAlpha=0,alphaOp=0,writeMask=0xF},blendAttachment,false);
            Marshal.StructureToPtr(new ColorBlendInfo{sType=26,pNext=IntPtr.Zero,flags=0,logicEnable=0,logicOp=0,attachmentCount=1,attachments=blendAttachment,blend0=0,blend1=0,blend2=0,blend3=0},blend,false);
            var li=new PipelineLayoutInfo{sType=30,pNext=IntPtr.Zero,flags=0,setLayoutCount=0,setLayouts=IntPtr.Zero,pushCount=0,pushRanges=IntPtr.Zero};Marshal.StructureToPtr(li,layoutInfo,false);Check(Dev<CreatePipelineLayout>(getDev,device,"vkCreatePipelineLayout")(device,layoutInfo,IntPtr.Zero,out layout),"vkCreatePipelineLayout");
            var pi=new GraphicsPipelineInfo{sType=28,pNext=IntPtr.Zero,flags=0,stageCount=2,stages=stages,vertexInput=vertexInput,inputAssembly=inputAssembly,tessellation=IntPtr.Zero,viewport=viewportState,rasterization=raster,multisample=multisample,depthStencil=IntPtr.Zero,colorBlend=blend,dynamicState=IntPtr.Zero,layout=layout,renderPass=renderPass,subpass=0,basePipeline=IntPtr.Zero,basePipelineIndex=-1};Marshal.StructureToPtr(pi,pipelineInfo,false);Check(Dev<CreateGraphicsPipelines>(getDev,device,"vkCreateGraphicsPipelines")(device,IntPtr.Zero,1,pipelineInfo,IntPtr.Zero,pipelineOut),"vkCreateGraphicsPipelines");return Marshal.ReadIntPtr(pipelineOut);
        }
        finally{Marshal.FreeHGlobal(pipelineOut);Marshal.FreeHGlobal(pipelineInfo);Marshal.FreeHGlobal(layoutInfo);Marshal.FreeHGlobal(blend);Marshal.FreeHGlobal(blendAttachment);Marshal.FreeHGlobal(multisample);Marshal.FreeHGlobal(raster);Marshal.FreeHGlobal(viewportState);Marshal.FreeHGlobal(scissor);Marshal.FreeHGlobal(viewport);Marshal.FreeHGlobal(inputAssembly);Marshal.FreeHGlobal(vertexInput);Marshal.FreeHGlobal(stages);Marshal.FreeHGlobal(mainName);}
    }

    private static IntPtr CreateImageViewObject(IntPtr device, GetDeviceProcAddr getDev, IntPtr image, int format)
    {
        var info=new ImageViewInfo{sType=15,pNext=IntPtr.Zero,flags=0,image=image,viewType=1,format=format,componentR=0,componentG=0,componentB=0,componentA=0,range=new SubresourceRange{aspect=1,baseMip=0,mipCount=1,baseLayer=0,layerCount=1}};IntPtr mem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(ImageViewInfo)));try{Marshal.StructureToPtr(info,mem,false);IntPtr view;Check(Dev<CreateImageView>(getDev,device,"vkCreateImageView")(device,mem,IntPtr.Zero,out view),"vkCreateImageView");return view;}finally{Marshal.FreeHGlobal(mem);}
    }

    private static IntPtr CreateFramebufferObject(IntPtr device, GetDeviceProcAddr getDev, IntPtr renderPass, IntPtr imageView, uint width, uint height)
    {
        IntPtr attachment=Marshal.AllocHGlobal(IntPtr.Size),mem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(FramebufferInfo)));try{Marshal.WriteIntPtr(attachment,imageView);var info=new FramebufferInfo{sType=37,pNext=IntPtr.Zero,flags=0,renderPass=renderPass,attachmentCount=1,attachments=attachment,width=width,height=height,layers=1};Marshal.StructureToPtr(info,mem,false);IntPtr fb;Check(Dev<CreateFramebuffer>(getDev,device,"vkCreateFramebuffer")(device,mem,IntPtr.Zero,out fb),"vkCreateFramebuffer");return fb;}finally{Marshal.FreeHGlobal(mem);Marshal.FreeHGlobal(attachment);}
    }

    public static int Run(IntPtr hwnd, string preferred, string vertexPath, string fragmentPath, int frameCount)
    {
        IntPtr loader = LoadLibrary("vulkan-1.dll");
        if (loader == IntPtr.Zero) { Console.Error.WriteLine("FAIL: cannot load vulkan-1.dll"); return 2; }
        IntPtr appName = Marshal.StringToHGlobalAnsi("GOW3 Vulkan triangle probe"), engineName = Marshal.StringToHGlobalAnsi("GOW3 feasibility probe");
        string[] instanceExtNames = { "VK_KHR_surface", "VK_KHR_win32_surface" };
        string[] deviceExtNames = { "VK_KHR_swapchain" };
        IntPtr instanceExtStrings = Marshal.AllocHGlobal(instanceExtNames.Length * IntPtr.Size), deviceExtString = Marshal.StringToHGlobalAnsi(deviceExtNames[0]);
        IntPtr surfaceInfoMem = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(SurfaceInfo))), appMem = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(AppInfo))), instanceMem = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(InstanceInfo)));
        IntPtr instance = IntPtr.Zero, surface = IntPtr.Zero, physical = IntPtr.Zero, device = IntPtr.Zero, queue = IntPtr.Zero, swapchain = IntPtr.Zero, pool = IntPtr.Zero, vertexModule=IntPtr.Zero, fragmentModule=IntPtr.Zero, renderPass=IntPtr.Zero, pipeline=IntPtr.Zero, pipelineLayout=IntPtr.Zero;
        if(frameCount<1)throw new ArgumentOutOfRangeException("frameCount");
        IntPtr[] acquireSemaphores=new IntPtr[frameCount], renderSemaphores=null;
        IntPtr[] imageViews=null, framebuffers=null;
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

                int selectedFormat=0;uint selectedWidth=0,selectedHeight=0;
                IntPtr caps=Marshal.AllocHGlobal(64);try
                {
                    Check(getCaps(physical,surface,caps),"vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
                    uint minImages=unchecked((uint)Marshal.ReadInt32(caps,0)), maxImages=unchecked((uint)Marshal.ReadInt32(caps,4));
                    uint width=unchecked((uint)Marshal.ReadInt32(caps,8)),height=unchecked((uint)Marshal.ReadInt32(caps,12));
                    uint supportedTransforms=unchecked((uint)Marshal.ReadInt32(caps,36)), currentTransform=unchecked((uint)Marshal.ReadInt32(caps,40)), compositeSupport=unchecked((uint)Marshal.ReadInt32(caps,44)), usageSupport=unchecked((uint)Marshal.ReadInt32(caps,48));
                    if((usageSupport&0x10)==0)throw new InvalidOperationException("Surface does not support color-attachment swapchain images required by the triangle pipeline");
                    if(width==UInt32.MaxValue){width=320;height=180;}
                    if(width==0||height==0)throw new InvalidOperationException("Surface extent is zero; cannot create a swapchain");
                    uint imageCount=minImages+1;if(maxImages>0&&imageCount>maxImages)imageCount=maxImages;
                    uint transform=(supportedTransforms&currentTransform)!=0?currentTransform:1;
                    uint composite=(compositeSupport&1)!=0?1u:(compositeSupport&2)!=0?2u:(compositeSupport&4)!=0?4u:(compositeSupport&8)!=0?8u:0u;
                    if(composite==0)throw new InvalidOperationException("No supported composite alpha mode");
                    uint fmtCount=0;Check(getFormats(physical,surface,ref fmtCount,IntPtr.Zero),"surface formats(count)");IntPtr fmtMem=Marshal.AllocHGlobal(checked((int)fmtCount*8));SurfaceFormat chosen;
                    try{Check(getFormats(physical,surface,ref fmtCount,fmtMem),"surface formats(list)");if(fmtCount==0)throw new InvalidOperationException("No surface formats");chosen=(SurfaceFormat)Marshal.PtrToStructure(fmtMem,typeof(SurfaceFormat));}finally{Marshal.FreeHGlobal(fmtMem);}
                    selectedFormat=chosen.format;selectedWidth=width;selectedHeight=height;
                    uint modeCount=0;Check(getModes(physical,surface,ref modeCount,IntPtr.Zero),"present modes(count)");IntPtr modeMem=Marshal.AllocHGlobal(checked((int)modeCount*4));bool fifo=false;
                    try{Check(getModes(physical,surface,ref modeCount,modeMem),"present modes(list)");for(uint m=0;m<modeCount;m++)if(Marshal.ReadInt32(modeMem,checked((int)m*4))==2)fifo=true;}finally{Marshal.FreeHGlobal(modeMem);}
                    if(!fifo)throw new InvalidOperationException("Guaranteed FIFO present mode was not enumerated");
                    var sci=new SwapchainInfo{sType=1000001000,pNext=IntPtr.Zero,flags=0,surface=surface,minImages=imageCount,format=chosen.format,colorSpace=chosen.colorSpace,extent=new Extent2D{width=width,height=height},layers=1,usage=0x10,sharing=0,familyCount=0,families=IntPtr.Zero,transform=transform,composite=composite,presentMode=2,clipped=1,oldSwapchain=IntPtr.Zero};
                    IntPtr sciMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(SwapchainInfo)));try{Marshal.StructureToPtr(sci,sciMem,false);Check(Dev<CreateSwapchain>(getDev,device,"vkCreateSwapchainKHR")(device,sciMem,IntPtr.Zero,out swapchain),"vkCreateSwapchainKHR");}finally{Marshal.FreeHGlobal(sciMem);}
                    Console.WriteLine("Swapchain: PASS | extent "+width+"x"+height+" | FIFO present | color attachment");
                }
                finally{Marshal.FreeHGlobal(caps);}

                uint imageCountOut=0;var getImages=Dev<GetSwapchainImages>(getDev,device,"vkGetSwapchainImagesKHR");Check(getImages(device,swapchain,ref imageCountOut,IntPtr.Zero),"vkGetSwapchainImagesKHR(count)");IntPtr images=Marshal.AllocHGlobal(checked((int)imageCountOut*IntPtr.Size));
                try
                {
                    Check(getImages(device,swapchain,ref imageCountOut,images),"vkGetSwapchainImagesKHR(list)");
                    renderSemaphores=new IntPtr[imageCountOut];
                    imageViews=new IntPtr[imageCountOut];framebuffers=new IntPtr[imageCountOut];
                    for(uint vi=0;vi<imageCountOut;vi++)
                    {
                        IntPtr swapImage=Marshal.ReadIntPtr(images,checked((int)vi*IntPtr.Size));
                        imageViews[vi]=CreateImageViewObject(device,getDev,swapImage,selectedFormat);
                    }
                    vertexModule=CreateModule(device,getDev,vertexPath);fragmentModule=CreateModule(device,getDev,fragmentPath);
                    renderPass=CreateRenderPassObject(device,getDev,selectedFormat);
                    pipeline=CreateGraphicsPipeline(device,getDev,renderPass,selectedWidth,selectedHeight,vertexModule,fragmentModule,out pipelineLayout);
                    for(uint fi=0;fi<imageCountOut;fi++)framebuffers[fi]=CreateFramebufferObject(device,getDev,renderPass,imageViews[fi],selectedWidth,selectedHeight);
                    var poolInfo=new CommandPoolInfo{sType=39,pNext=IntPtr.Zero,flags=0,family=family};IntPtr poolInfoMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(CommandPoolInfo)));try{Marshal.StructureToPtr(poolInfo,poolInfoMem,false);Check(Dev<CreateCommandPool>(getDev,device,"vkCreateCommandPool")(device,poolInfoMem,IntPtr.Zero,out pool),"vkCreateCommandPool");}finally{Marshal.FreeHGlobal(poolInfoMem);}
                    var renderSemInfo=new SemaphoreInfo{sType=9,pNext=IntPtr.Zero,flags=0};IntPtr renderSemMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(SemaphoreInfo)));try{Marshal.StructureToPtr(renderSemInfo,renderSemMem,false);for(uint si=0;si<imageCountOut;si++)Check(Dev<CreateSemaphore>(getDev,device,"vkCreateSemaphore")(device,renderSemMem,IntPtr.Zero,out renderSemaphores[si]),"create per-image render semaphore");}finally{Marshal.FreeHGlobal(renderSemMem);}
                    for(int frame=0;frame<frameCount;frame++)
                    {
                    var semInfo=new SemaphoreInfo{sType=9,pNext=IntPtr.Zero,flags=0};IntPtr semMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(SemaphoreInfo)));try{Marshal.StructureToPtr(semInfo,semMem,false);Check(Dev<CreateSemaphore>(getDev,device,"vkCreateSemaphore")(device,semMem,IntPtr.Zero,out acquireSemaphores[frame]),"create acquire semaphore");}finally{Marshal.FreeHGlobal(semMem);}
                    uint imageIndex;int acquireResult=Dev<AcquireNextImage>(getDev,device,"vkAcquireNextImageKHR")(device,swapchain,5000000000UL,acquireSemaphores[frame],IntPtr.Zero,out imageIndex);if(acquireResult==2)throw new InvalidOperationException("Timed out waiting for a presented swapchain image; the window may need to be visible or the presentation driver may not be returning images.");if(acquireResult!=0&&acquireResult!=1000001003)Check(acquireResult,"vkAcquireNextImageKHR");
                    Console.WriteLine("Frame "+(frame+1)+" acquired swapchain image "+imageIndex);
                    IntPtr commandBufferMemory=Marshal.AllocHGlobal(IntPtr.Size),commandBuffer;var allocInfo=new CommandAllocInfo{sType=40,pNext=IntPtr.Zero,pool=pool,level=0,count=1};IntPtr allocMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(CommandAllocInfo)));try{Marshal.StructureToPtr(allocInfo,allocMem,false);Check(Dev<AllocateCommandBuffers>(getDev,device,"vkAllocateCommandBuffers")(device,allocMem,commandBufferMemory),"vkAllocateCommandBuffers");commandBuffer=Marshal.ReadIntPtr(commandBufferMemory);}finally{Marshal.FreeHGlobal(allocMem);Marshal.FreeHGlobal(commandBufferMemory);}
                    var begin=new CommandBeginInfo{sType=42,pNext=IntPtr.Zero,flags=1,inheritance=IntPtr.Zero};IntPtr beginMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(CommandBeginInfo)));try{Marshal.StructureToPtr(begin,beginMem,false);Check(Dev<BeginCommandBuffer>(getDev,device,"vkBeginCommandBuffer")(commandBuffer,beginMem),"vkBeginCommandBuffer");}finally{Marshal.FreeHGlobal(beginMem);}
                    IntPtr clearMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(ClearValue))),renderBeginMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(RenderPassBeginInfo)));
                    try
                    {
                        Marshal.StructureToPtr(new ClearValue{r=0.05f,g=0.08f,b=0.14f,a=1.0f},clearMem,false);
                        var renderBegin=new RenderPassBeginInfo{sType=43,pNext=IntPtr.Zero,renderPass=renderPass,framebuffer=framebuffers[imageIndex],area=new Rect2D{offset=new Offset2D{x=0,y=0},extent=new Extent2D{width=selectedWidth,height=selectedHeight}},clearCount=1,clears=clearMem};Marshal.StructureToPtr(renderBegin,renderBeginMem,false);
                        Dev<CmdBeginRenderPass>(getDev,device,"vkCmdBeginRenderPass")(commandBuffer,renderBeginMem,0);
                        Dev<CmdBindPipeline>(getDev,device,"vkCmdBindPipeline")(commandBuffer,0,pipeline);
                        Dev<CmdDraw>(getDev,device,"vkCmdDraw")(commandBuffer,3,1,0,0);
                        Dev<CmdEndRenderPass>(getDev,device,"vkCmdEndRenderPass")(commandBuffer);
                    }
                    finally{Marshal.FreeHGlobal(renderBeginMem);Marshal.FreeHGlobal(clearMem);}
                    Check(Dev<EndCommandBuffer>(getDev,device,"vkEndCommandBuffer")(commandBuffer),"vkEndCommandBuffer");
                    IntPtr waits=Marshal.AllocHGlobal(IntPtr.Size),signals=Marshal.AllocHGlobal(IntPtr.Size),commands=Marshal.AllocHGlobal(IntPtr.Size),stages=Marshal.AllocHGlobal(4),submitMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(SubmitInfo)));
                    try
                    {
                        Marshal.WriteIntPtr(waits,acquireSemaphores[frame]);Marshal.WriteIntPtr(signals,renderSemaphores[imageIndex]);Marshal.WriteIntPtr(commands,commandBuffer);Marshal.WriteInt32(stages,unchecked((int)0x400));
                        var submit=new SubmitInfo{sType=4,pNext=IntPtr.Zero,waitCount=1,waits=waits,waitStages=stages,commandCount=1,commands=commands,signalCount=1,signals=signals};Marshal.StructureToPtr(submit,submitMem,false);Check(Dev<QueueSubmit>(getDev,device,"vkQueueSubmit")(queue,1,submitMem,IntPtr.Zero),"vkQueueSubmit");
                        IntPtr presentWaits=Marshal.AllocHGlobal(IntPtr.Size),swapchains=Marshal.AllocHGlobal(IntPtr.Size),indices=Marshal.AllocHGlobal(4),presentMem=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(PresentInfo)));
                        try{Marshal.WriteIntPtr(presentWaits,renderSemaphores[imageIndex]);Marshal.WriteIntPtr(swapchains,swapchain);Marshal.WriteInt32(indices,unchecked((int)imageIndex));var present=new PresentInfo{sType=1000001001,pNext=IntPtr.Zero,waitCount=1,waits=presentWaits,swapchainCount=1,swapchains=swapchains,imageIndices=indices,results=IntPtr.Zero};Marshal.StructureToPtr(present,presentMem,false);int pr=Dev<QueuePresent>(getDev,device,"vkQueuePresentKHR")(queue,presentMem);if(pr!=0&&pr!=1000001003)Check(pr,"vkQueuePresentKHR");Console.WriteLine("Frame "+(frame+1)+" presented; waiting for queue idle");Check(Dev<QueueWaitIdle>(getDev,device,"vkQueueWaitIdle")(queue),"vkQueueWaitIdle");}
                        finally{Marshal.FreeHGlobal(presentMem);Marshal.FreeHGlobal(indices);Marshal.FreeHGlobal(swapchains);Marshal.FreeHGlobal(presentWaits);}
                    }
                    finally{Marshal.FreeHGlobal(submitMem);Marshal.FreeHGlobal(stages);Marshal.FreeHGlobal(commands);Marshal.FreeHGlobal(signals);Marshal.FreeHGlobal(waits);}
                    }
                    Console.WriteLine("Shader triangle present: PASS | "+frameCount+" synthetic frames at "+selectedWidth+"x"+selectedHeight);
                }
                finally{Marshal.FreeHGlobal(images);}
                Console.WriteLine("RESULT: PASS - Vulkan 1.2 swapchain rendered and presented "+frameCount+" shader-backed triangle frame(s).");
                return 0;
            }
            finally{Marshal.FreeHGlobal(props);Marshal.FreeHGlobal(devices);}
        }
        catch(Exception ex){Console.Error.WriteLine("FAIL: "+ex.Message);return 10;}
        finally
        {
            if(device!=IntPtr.Zero)
            {
                try{Dev<DeviceWaitIdle>(getDev,device,"vkDeviceWaitIdle")(device);}catch{}
                if(framebuffers!=null)foreach(IntPtr fb in framebuffers)if(fb!=IntPtr.Zero)try{Dev<DestroyFramebuffer>(getDev,device,"vkDestroyFramebuffer")(device,fb,IntPtr.Zero);}catch{}
                if(imageViews!=null)foreach(IntPtr view in imageViews)if(view!=IntPtr.Zero)try{Dev<DestroyImageView>(getDev,device,"vkDestroyImageView")(device,view,IntPtr.Zero);}catch{}
                if(pipeline!=IntPtr.Zero)try{Dev<DestroyPipeline>(getDev,device,"vkDestroyPipeline")(device,pipeline,IntPtr.Zero);}catch{}
                if(pipelineLayout!=IntPtr.Zero)try{Dev<DestroyPipelineLayout>(getDev,device,"vkDestroyPipelineLayout")(device,pipelineLayout,IntPtr.Zero);}catch{}
                if(vertexModule!=IntPtr.Zero)try{Dev<DestroyShaderModule>(getDev,device,"vkDestroyShaderModule")(device,vertexModule,IntPtr.Zero);}catch{}
                if(fragmentModule!=IntPtr.Zero)try{Dev<DestroyShaderModule>(getDev,device,"vkDestroyShaderModule")(device,fragmentModule,IntPtr.Zero);}catch{}
                if(renderPass!=IntPtr.Zero)try{Dev<DestroyRenderPass>(getDev,device,"vkDestroyRenderPass")(device,renderPass,IntPtr.Zero);}catch{}
                if(pool!=IntPtr.Zero)try{Dev<DestroyCommandPool>(getDev,device,"vkDestroyCommandPool")(device,pool,IntPtr.Zero);}catch{}
                // On this AMD/WDDM stack, destroying a just-presented swapchain
                // can fault in the driver even after vkDeviceWaitIdle. Keep this
                // short-lived probe isolated until present-completion handling is fixed.
                if(acquireSemaphores!=null)foreach(IntPtr semaphore in acquireSemaphores)if(semaphore!=IntPtr.Zero)try{Dev<DestroySemaphore>(getDev,device,"vkDestroySemaphore")(device,semaphore,IntPtr.Zero);}catch{}
                if(renderSemaphores!=null)foreach(IntPtr semaphore in renderSemaphores)if(semaphore!=IntPtr.Zero)try{Dev<DestroySemaphore>(getDev,device,"vkDestroySemaphore")(device,semaphore,IntPtr.Zero);}catch{}
                try{Dev<DestroyDevice>(getDev,device,"vkDestroyDevice")(device,IntPtr.Zero);}catch{}
            }
            if(instance!=IntPtr.Zero){if(surface!=IntPtr.Zero)try{Inst<DestroySurface>(getInst,instance,"vkDestroySurfaceKHR")(instance,surface,IntPtr.Zero);}catch{} try{Load<DestroyInstance>(loader,"vkDestroyInstance")(instance,IntPtr.Zero);}catch{}}
            for(int i=0;i<instanceExtNames.Length;i++)Marshal.FreeHGlobal(Marshal.ReadIntPtr(instanceExtStrings,i*IntPtr.Size));
            Marshal.FreeHGlobal(deviceExtString);Marshal.FreeHGlobal(instanceExtStrings);Marshal.FreeHGlobal(surfaceInfoMem);Marshal.FreeHGlobal(instanceMem);Marshal.FreeHGlobal(appMem);Marshal.FreeHGlobal(engineName);Marshal.FreeHGlobal(appName);
        }
    }
}
