// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <string_view>
#include <glm/glm.hpp>
#include <luvk/App/Feature.hpp>
#include <luvk/Resources/Buffer.hpp>
#include <luvk/Resources/Pipeline.hpp>

namespace Example
{
    /** GPU particle fountain: a compute pass simulates the particles in place through a buffer device address, then
     *  camera-facing quads are pulled as instances and blended over the scene, depth-tested but not depth-writing. */
    class Particles final : public luvk::app::RenderFeature
    {
    public:
        /** One particle (matches the shader layout: two 16-byte rows). */
        struct Particle
        {
            /** World position. */
            glm::vec3 Position;

            /** Remaining lifetime in seconds (respawned at the emitter when it runs out). */
            float Life;

            /** Velocity in world units per second. */
            glm::vec3 Velocity;

            /** Per-particle random seed. */
            float Seed;
        };

        /** Constructor: places the fountain at Position. */
        explicit Particles(const glm::vec3& Position) : Emitter(Position) {}

        /** World position of the fountain. */
        glm::vec3 Emitter{0.F};

    private:
        /** CPU-side mirror of the shader push constant block. */
        struct PushData
        {
            /** Camera view-projection. */
            glm::mat4 ViewProjection;

            /** Device address of the particle storage buffer. */
            VkDeviceAddress Particles;

            /** Number of particles. */
            std::uint32_t Count;

            /** Simulation step of this frame (seconds). */
            float DeltaTime;

            /** World position of the fountain. */
            glm::vec3 Emitter;

            /** Elapsed time (respawn randomisation). */
            float Time;

            /** Camera right vector (billboarding). */
            glm::vec3 CameraRight;

            /** Half size of a particle quad in world units. */
            float Size;

            /** Camera up vector (billboarding). */
            glm::vec3 CameraUp;
        };

        /** Storage buffer holding the particles, owned by the GPU after the first upload. */
        luvk::Buffer m_Buffer;

        /** Compute pipeline simulating the particles. */
        luvk::Pipeline m_Compute;

        /** Graphics pipeline (recreated when the target formats or the wireframe setting change). */
        luvk::Pipeline m_Graphics;

        /** Compute pipeline writing velocity lines into the GPU debug line buffer (rebuilt when the stride changes). */
        luvk::Pipeline m_Debug;

        /** Stride the debug pipeline was specialized with (0 = not built yet). */
        std::uint32_t m_DebugBuiltStride = 0U;

        /** Color format the graphics pipeline was built for. */
        VkFormat m_Format = VK_FORMAT_UNDEFINED;

        /** Wireframe setting the graphics pipeline was built for. */
        bool m_Wireframe = false;

        /** Reverse depth setting the pipeline was built for. */
        bool m_ReverseDepth = false;

        /** Simulation step of the current frame (seconds). */
        float m_DeltaTime = 0.F;

        /** Elapsed time (seconds). */
        float m_Time = 0.F;

        /** Half size of a particle quad (tuned from the UI). */
        float m_Size = 0.035F;

        /** Simulation speed multiplier (tuned from the UI). */
        float m_Speed = 1.F;

        /** Draws a velocity line for every m_DebugStride-th particle, written by a compute pass (GPU debug lines). */
        bool m_ShowVelocities = false;

        /** Particle stride of the velocity lines (a specialization constant of the debug pipeline). */
        int m_DebugStride = 16;

        /** Length of a velocity line in seconds of motion. */
        float m_VelocityScale = 0.1F;

        /** Push constants of the velocity line pass (DebugPush in the shader). */
        struct DebugPush
        {
            /** Address of the particle buffer. */
            VkDeviceAddress Particles;

            /** Address of the GPU debug line header. */
            VkDeviceAddress Lines;

            /** Address of the first GPU debug line vertex. */
            VkDeviceAddress Vertices;

            /** Number of particles. */
            std::uint32_t Count;

            /** Line length in seconds of motion. */
            float Scale;
        };

        /** Adds the compute pass appending velocity lines to the frame's GPU debug line buffer. */
        void AddVelocityLines(luvk::app::FeatureContext& Frame, luvk::BufferHandle Data);

    public:
        /** Name shown in the UI. */
        [[nodiscard]] std::string_view Name() const override { return "Particle fountain (compute)"; }

        /** Blended over the opaque scene. */
        [[nodiscard]] luvk::app::FeatureStage Stage() const override { return luvk::app::FeatureStage::Transparent; }

        /** Creates the particle buffer (staggered lifetimes, so the fountain starts gradually) and the compute pipeline. */
        void OnAttach(luvk::Context& Ctx) override;

        /** Advances the simulation clock. */
        void Update(float DeltaTime) override;

        /** Adds the simulate and draw passes. */
        void AddPasses(luvk::app::FeatureContext& Frame) override;

        /** Particle size and simulation speed. */
        void OnGui() override;
    };
}
