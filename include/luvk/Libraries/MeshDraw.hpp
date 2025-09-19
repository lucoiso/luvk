// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <volk/volk.h>
#include "luvk/Types/MeshEntry.hpp"

namespace luvk
{
    LUVKMODULE_API void RecordMeshCommands(const VkCommandBuffer& Command, const MeshEntry& Entry);
} // namespace luvk
