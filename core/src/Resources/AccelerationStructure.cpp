// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Resources/AccelerationStructure.hpp"
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "luvk/Core/Bindless.hpp"
#include "luvk/Core/Context.hpp"
#include "luvk/Core/Error.hpp"
#include "luvk/Core/FeatureChain.hpp"
#include "luvk/Core/Vulkan.hpp"

namespace
{
    VkDeviceSize AlignUp(const VkDeviceSize Value, const VkDeviceSize Alignment)
    {
        return Alignment > 1U ? (Value + Alignment - 1U) / Alignment * Alignment : Value;
    }

    luvk::Buffer MakeInput(luvk::Context& Ctx, const std::span<const std::byte> Data, const char* const Name)
    {
        luvk::Buffer Input(Ctx,
                           {.Size   = std::size(Data),
                            .Memory = luvk::MemoryUsage::Upload,
                            .Name   = Name});
        Input.Write(Data);
        return Input;
    }
}

luvk::RayTracingModule::RayTracingModule(const bool Required)
    : m_Required(Required)
{
}

void luvk::RayTracingModule::ConfigureDevice(DeviceRequirements& Requirements)
{
    auto& Extensions = m_Required ? Requirements.Extensions : Requirements.OptionalExtensions;
    auto& Features   = m_Required ? Requirements.Features : Requirements.OptionalFeatures;

    Extensions.emplace_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    Extensions.emplace_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
    Extensions.emplace_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

    auto& Structures = Features.Get<VkPhysicalDeviceAccelerationStructureFeaturesKHR>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
                                                                                    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    Structures.accelerationStructure                                 = VK_TRUE;
    Structures.descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE;

    Features.Get<VkPhysicalDeviceRayQueryFeaturesKHR>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR, VK_KHR_RAY_QUERY_EXTENSION_NAME).rayQuery = VK_TRUE;
}

bool luvk::RayTracingModule::IsAvailable(const Context& Ctx)
{
    const auto* const Structures = Ctx.EnabledFeatures().Find<VkPhysicalDeviceAccelerationStructureFeaturesKHR>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR);
    const auto* const RayQuery   = Ctx.EnabledFeatures().Find<VkPhysicalDeviceRayQueryFeaturesKHR>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR);

    return Structures != nullptr && Structures->accelerationStructure != VK_FALSE &&
           Structures->descriptorBindingAccelerationStructureUpdateAfterBind != VK_FALSE &&
           RayQuery != nullptr && RayQuery->rayQuery != VK_FALSE &&
           Ctx.IsDeviceExtensionEnabled(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) &&
           Ctx.IsDeviceExtensionEnabled(VK_KHR_RAY_QUERY_EXTENSION_NAME);
}

luvk::AccelerationStructure::~AccelerationStructure()
{
    Reset();
}

luvk::AccelerationStructure::AccelerationStructure(AccelerationStructure&& Other) noexcept
    : m_Context(std::exchange(Other.m_Context, nullptr)),
      m_Storage(std::move(Other.m_Storage)),
      m_Handle(std::exchange(Other.m_Handle, VK_NULL_HANDLE)),
      m_Address(std::exchange(Other.m_Address, 0U)),
      m_BindlessIndex(std::exchange(Other.m_BindlessIndex, InvalidBindlessIndex)) {}

luvk::AccelerationStructure& luvk::AccelerationStructure::operator=(AccelerationStructure&& Other) noexcept
{
    if (this != &Other)
    {
        Reset();
        m_Context       = std::exchange(Other.m_Context, nullptr);
        m_Storage       = std::move(Other.m_Storage);
        m_Handle        = std::exchange(Other.m_Handle, VK_NULL_HANDLE);
        m_Address       = std::exchange(Other.m_Address, 0U);
        m_BindlessIndex = std::exchange(Other.m_BindlessIndex, InvalidBindlessIndex);
    }
    return *this;
}

void luvk::AccelerationStructure::Reset() noexcept
{
    if (m_Context == nullptr)
    {
        return;
    }

    if (m_BindlessIndex != InvalidBindlessIndex)
    {
        m_Context->Retire({.Kind = RetiredKind::BindlessSlot, .Slot = static_cast<std::uint32_t>(BindlessSlot::AccelerationStructure), .Index = m_BindlessIndex});
        m_BindlessIndex = InvalidBindlessIndex;
    }

    if (m_Handle != VK_NULL_HANDLE)
    {

        m_Context->Retire({.Kind = RetiredKind::AccelerationStructure, .Handle = HandleToU64(m_Handle)});
        m_Context->TrackResource(-1);
        m_Handle = VK_NULL_HANDLE;
    }

    m_Storage.Reset();
    m_Address = 0U;
}

luvk::AccelerationStructure luvk::AccelerationStructure::BuildBottomLevel(Context& Ctx, const BottomLevelGeometry& Geometry, const std::string_view Name)
{
    if (std::size(Geometry.Positions) < 9U || std::size(Geometry.Positions) % 3U != 0U)
    {
        throw std::invalid_argument("[luvk] Bottom-level geometry needs at least one triangle (xyz floats).");
    }

    const auto VertexCount = static_cast<std::uint32_t>(std::size(Geometry.Positions) / 3U);
    const bool Indexed     = !std::empty(Geometry.Indices);

    if ((Indexed ? std::size(Geometry.Indices) : VertexCount) % 3U != 0U)
    {
        throw std::invalid_argument("[luvk] Bottom-level geometry must be a triangle list.");
    }

    const Buffer Vertices = MakeInput(Ctx, std::as_bytes(Geometry.Positions), "luvk blas vertices");
    const Buffer Indices  = Indexed ? MakeInput(Ctx, std::as_bytes(Geometry.Indices), "luvk blas indices") : Buffer{};

    const VkAccelerationStructureGeometryKHR Description{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry     = {.triangles = {.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                                       .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                                       .vertexData   = {.deviceAddress = Vertices.Address()},
                                       .vertexStride = 3U * sizeof(float),
                                       .maxVertex    = VertexCount - 1U,
                                       .indexType    = Indexed ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_NONE_KHR,
                                       .indexData    = {.deviceAddress = Indexed ? Indices.Address() : 0U}}},
        .flags = Geometry.Opaque ? static_cast<VkGeometryFlagsKHR>(VK_GEOMETRY_OPAQUE_BIT_KHR) : VkGeometryFlagsKHR{0U}};

    const std::uint32_t Triangles = (Indexed ? static_cast<std::uint32_t>(std::size(Geometry.Indices)) : VertexCount) / 3U;
    return Build(Ctx, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, Description, Triangles, Name);
}

luvk::AccelerationStructure luvk::AccelerationStructure::BuildTopLevel(Context& Ctx, const std::span<const TopLevelInstance> Instances, const std::string_view Name)
{
    if (std::empty(Instances))
    {
        throw std::invalid_argument("[luvk] A top-level acceleration structure needs at least one instance.");
    }

    std::vector<VkAccelerationStructureInstanceKHR> Records;
    Records.reserve(std::size(Instances));

    for (const TopLevelInstance& Instance : Instances)
    {
        if (Instance.BottomLevel == nullptr || !*Instance.BottomLevel)
        {
            throw std::invalid_argument("[luvk] Top-level instance without a bottom-level structure.");
        }

        VkAccelerationStructureInstanceKHR Record = {};
        std::memcpy(&Record.transform, std::data(Instance.Transform), sizeof(Record.transform));
        Record.instanceCustomIndex                    = Instance.CustomIndex & 0xFFFFFFU;
        Record.mask                                   = Instance.Mask;
        Record.instanceShaderBindingTableRecordOffset = 0U;
        Record.flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        Record.accelerationStructureReference         = Instance.BottomLevel->Address();
        Records.push_back(Record);
    }

    // VkAccelerationStructureInstanceKHR data must be 16-byte aligned, while a VMA suballocation is not
    // guaranteed to be. Reserve 16 extra bytes and write the records at the first aligned address.
    constexpr VkDeviceSize InstanceAlignment = 16U;
    const std::span<const std::byte> RecordsBytes = std::as_bytes(std::span(Records));
    Buffer InstanceData(Ctx,
                        {.Size   = static_cast<VkDeviceSize>(std::size(RecordsBytes)) + InstanceAlignment,
                         .Memory = MemoryUsage::Upload,
                         .Name   = "luvk tlas instances"});
    const VkDeviceAddress InstanceAddress = AlignUp(InstanceData.Address(), InstanceAlignment);
    InstanceData.Write(RecordsBytes, InstanceAddress - InstanceData.Address());

    const VkAccelerationStructureGeometryKHR Description{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry     = {.instances = {.sType           = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                                       .arrayOfPointers = VK_FALSE,
                                       .data            = {.deviceAddress = InstanceAddress}}}};

    AccelerationStructure Result = Build(Ctx, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, Description, static_cast<std::uint32_t>(std::size(Records)), Name);

    Result.m_BindlessIndex = Ctx.Bindless().Allocate(BindlessSlot::AccelerationStructure);
    Ctx.Bindless().WriteAccelerationStructure(Result.m_BindlessIndex, Result.m_Handle);
    return Result;
}

luvk::AccelerationStructure luvk::AccelerationStructure::Build(Context&                                  Ctx,
                                                               const std::int32_t                        Type,
                                                               const VkAccelerationStructureGeometryKHR& Geometry,
                                                               const std::uint32_t                       PrimitiveCount,
                                                               const std::string_view                    Name)
{
    if (!RayTracingModule::IsAvailable(Ctx))
    {
        throw std::logic_error("[luvk] Acceleration structures require luvk::RayTracingModule on a device that supports ray queries.");
    }

    const VkDevice Device = Ctx.Device();

    VkAccelerationStructureBuildGeometryInfoKHR BuildInfo{.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                                                          .type          = static_cast<VkAccelerationStructureTypeKHR>(Type),
                                                          .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                                                          .mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
                                                          .geometryCount = 1U,
                                                          .pGeometries   = &Geometry};

    VkAccelerationStructureBuildSizesInfoKHR Sizes{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(Device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &BuildInfo, &PrimitiveCount, &Sizes);

    AccelerationStructure Result;
    Result.m_Context = &Ctx;
    Result.m_Storage = Buffer(Ctx,
                              {.Size  = Sizes.accelerationStructureSize,
                               .Name  = Name});

    const VkAccelerationStructureCreateInfoKHR CreateInfo{.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                                                          .buffer = Result.m_Storage.Handle(),
                                                          .size   = Sizes.accelerationStructureSize,
                                                          .type   = static_cast<VkAccelerationStructureTypeKHR>(Type)};
    Check(vkCreateAccelerationStructureKHR(Device, &CreateInfo, nullptr, &Result.m_Handle), "vkCreateAccelerationStructureKHR");
    Ctx.TrackResource(+1);
    Ctx.SetDebugName(Result.m_Handle, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, Name);

    VkPhysicalDeviceAccelerationStructurePropertiesKHR Properties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
    VkPhysicalDeviceProperties2                        Wrapper{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &Properties};
    vkGetPhysicalDeviceProperties2(Ctx.PhysicalDevice(), &Wrapper);

    const VkDeviceSize Alignment = Properties.minAccelerationStructureScratchOffsetAlignment;
    Buffer             Scratch(Ctx, {.Size = Sizes.buildScratchSize + Alignment, .Name = "luvk as scratch"});

    BuildInfo.dstAccelerationStructure  = Result.m_Handle;
    BuildInfo.scratchData.deviceAddress = AlignUp(Scratch.Address(), Alignment);

    const VkAccelerationStructureBuildRangeInfoKHR        Range{.primitiveCount = PrimitiveCount};
    const VkAccelerationStructureBuildRangeInfoKHR* const Ranges = &Range;

    const VkCommandPoolCreateInfo PoolInfo{.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                           .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                                           .queueFamilyIndex = Ctx.GraphicsQueue().Family()};
    VkCommandPool Pool = VK_NULL_HANDLE;
    Check(vkCreateCommandPool(Device, &PoolInfo, nullptr, &Pool), "vkCreateCommandPool (as build)");

    try
    {
        const VkCommandBufferAllocateInfo AllocateInfo{.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                       .commandPool        = Pool,
                                                       .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                       .commandBufferCount = 1U};
        VkCommandBuffer Commands = VK_NULL_HANDLE;
        Check(vkAllocateCommandBuffers(Device, &AllocateInfo, &Commands), "vkAllocateCommandBuffers (as build)");

        constexpr VkCommandBufferBeginInfo BeginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
        Check(vkBeginCommandBuffer(Commands, &BeginInfo), "vkBeginCommandBuffer (as build)");

        constexpr VkMemoryBarrier2 Before{.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                          .srcStageMask  = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                          .srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                                          .dstStageMask  = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                          .dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR};
        const VkDependencyInfo BeforeDependency{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1U, .pMemoryBarriers = &Before};
        vkCmdPipelineBarrier2(Commands, &BeforeDependency);

        vkCmdBuildAccelerationStructuresKHR(Commands, 1U, &BuildInfo, &Ranges);

        constexpr VkMemoryBarrier2 After{.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                         .srcStageMask  = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                         .srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                                         .dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                         .dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR};
        const VkDependencyInfo AfterDependency{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1U, .pMemoryBarriers = &After};
        vkCmdPipelineBarrier2(Commands, &AfterDependency);

        Check(vkEndCommandBuffer(Commands), "vkEndCommandBuffer (as build)");

        const VkCommandBufferSubmitInfo CommandInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = Commands};
        const std::uint64_t             Value = Ctx.GraphicsQueue().Submit({&CommandInfo, 1U});
        Ctx.GraphicsQueue().Wait(Value);

        Scratch.Release(TimelinePoint{.Graphics = Value, .Transfer = 0U});
        vkDestroyCommandPool(Device, Pool, nullptr);
    }
    catch (...)
    {
        vkDestroyCommandPool(Device, Pool, nullptr);
        throw;
    }

    const VkAccelerationStructureDeviceAddressInfoKHR AddressInfo{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                                                                  .accelerationStructure = Result.m_Handle};
    Result.m_Address = vkGetAccelerationStructureDeviceAddressKHR(Device, &AddressInfo);
    return Result;
}
