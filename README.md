# About
New Vulkan Renderer being made from scratch with the objective of being modern while aiming modularity, performance and a better structurization.

The idea started from problems that started to occur [in this project](https://github.com/lucoiso/vulkan-renderer) where I used to start learning about how to use Vulkan with C++ while trying some extensions, but without defining how the implementations would be inserted in the project, resulting in hard maintainability and debugging.

This new library will focus on designing the renderer to be reusable on other projects that I will start on the future, having less dependencies than the other one. I'll try to not rush and implement the modules with patience.

## Notes
- I'll avoid using C++20 modules: Some IDEs still doesn't support well and it is terrible to read with so many false errors.
- I'll avoid third party libraries: Will rely on third party only when strictly necessary.
- Descriptor Buffers will be optional: For debugging purpose. Many tools still does not support this extension.
- Mesh and Compute Shaders are supported.

# Dependencies

1. Compiler w/ support for C++23
2. CMake +3.28
3. Vulkan SDK +1.4 (used while developing)
