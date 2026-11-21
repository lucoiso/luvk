// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

// Every header below compiles without the Vulkan headers; raw Vulkan interop lives in luvk/Core/Vulkan.hpp.
#include "luvk/Core/Bindless.hpp"
#include "luvk/Core/Common.hpp"
#include "luvk/Core/Context.hpp"
#include "luvk/Core/DeletionQueue.hpp"
#include "luvk/Core/DiagnosticsTypes.hpp"
#include "luvk/Core/Error.hpp"
#include "luvk/Core/Handles.hpp"
#include "luvk/Core/Module.hpp"
#include "luvk/Core/Queue.hpp"
#include "luvk/Core/Types.hpp"
#include "luvk/Core/Upload.hpp"
#include "luvk/Render/CommandList.hpp"
#include "luvk/Render/FrameAllocator.hpp"
#include "luvk/Render/GpuProfiler.hpp"
#include "luvk/Render/PresentTarget.hpp"
#include "luvk/Render/ReadbackRing.hpp"
#include "luvk/Render/RenderGraph.hpp"
#include "luvk/Render/Renderer.hpp"
#include "luvk/Render/Swapchain.hpp"
#include "luvk/Resources/AccelerationStructure.hpp"
#include "luvk/Resources/Buffer.hpp"
#include "luvk/Resources/Image.hpp"
#include "luvk/Resources/Pipeline.hpp"
#include "luvk/Resources/Sampler.hpp"
#include "luvk/Resources/ShaderReflection.hpp"
