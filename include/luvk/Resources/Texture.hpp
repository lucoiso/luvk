/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace luvk
{
    class Image;
    class Sampler;

    /**
     * Represents a texture combining an Image and a Sampler.
     * Supports Bindless rendering via Index.
     */
    class LUVK_API Texture
    {
    protected:
        /** The image data and view component. */
        std::shared_ptr<Image> m_Image{};

        /** The sampler object defining filtering and addressing. */
        std::shared_ptr<Sampler> m_Sampler{};

        /** Optional name for identification/debugging. */
        std::string m_Name{};

    public:
        /** Default constructor. */
        constexpr Texture() = default;

        /** Virtual destructor. */
        virtual ~Texture() = default;

        /**
         * Constructor.
         * @param Image Shared pointer to the Image object.
         * @param Sampler Shared pointer to the Sampler object.
         * @param Name Optional name of the texture.
         */
        explicit Texture(const std::shared_ptr<Image>& Image, const std::shared_ptr<Sampler>& Sampler, std::string_view Name = "");

        /** Get the shared pointer to the Image component. */
        [[nodiscard]] std::shared_ptr<Image> GetImage() const noexcept
        {
            return m_Image;
        }

        /** Get the shared pointer to the Sampler component. */
        [[nodiscard]] std::shared_ptr<Sampler> GetSampler() const noexcept
        {
            return m_Sampler;
        }

        /** Get the name of the texture. */
        [[nodiscard]] std::string_view GetName() const noexcept
        {
            return m_Name;
        }
    };
}
