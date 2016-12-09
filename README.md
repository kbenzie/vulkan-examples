# Vulkan Examples

Examples of using the Khronos Group's Vulkan standard.

## Examples

*   `vector_add` - a vector addition compute example
*   `triangle` - draw a triangle example

## Building

To build the examples a Vulkan driver is required, the CMake cross-platform
build system is used to generate platform specific build files. After cloning
the repository the following commands can be used to build the project.

```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

### Options

Validation layers are disabled by default, to enable validation you can specify
the following during CMake configuration.

```
cmake -DENABLE_LAYERS=ON ..
```

## License (Unlicense)

See [license](LICENSE.md) file.
