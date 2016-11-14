#include <vulkan/vulkan.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>
#include <fstream>

#ifdef ENABLE_LAYERS
// print out a debug repot to stderr
VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(
    VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
    uint64_t object, size_t location, int32_t messageCode,
    const char *pLayerPrefix, const char *pMessage, void *pUserData) {
  fprintf(stderr, "%s\n", pMessage);
  return VK_FALSE;
}
#endif

// load a SPIR-V binary from disc
std::vector<char> loadShaderCode(const char *filename) {
  std::vector<char> shaderCode;
  if (FILE *fp = fopen(filename, "rb"))
  {
    char buf[1024];
    while(size_t len = fread(buf, 1, sizeof(buf), fp)) {
      shaderCode.insert(shaderCode.end(), buf, buf + len);
    }
    fclose(fp);
  }
  return shaderCode;
}

int main() {
  // tell the driver about your app
  VkApplicationInfo applicationInfo = {};
  applicationInfo.pApplicationName = "Vulkan compute example";
  // using version 1.0.0 is required so your app will work with any loader
  applicationInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);

  //
  VkInstanceCreateInfo instanceCreateInfo = {};
  instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.pApplicationInfo = &applicationInfo;

  uint32_t count;
#ifdef ENABLE_LAYERS
  // enabling validation layers is vital when developing an application, the
  // standard validation layer ensures your app conforms to the specification
  std::vector<const char *> enabledLayerNames{
      "VK_LAYER_LUNARG_standard_validation"};
  instanceCreateInfo.enabledLayerCount = enabledLayerNames.size();
  instanceCreateInfo.ppEnabledLayerNames = enabledLayerNames.data();

  // enabling validation layers is not helpful without the debug report
  // extension to tell you when things go wrong
  std::vector<const char *> enabledExtensionNames{"VK_EXT_debug_report"};
  instanceCreateInfo.enabledExtensionCount = enabledExtensionNames.size();
  instanceCreateInfo.ppEnabledExtensionNames = enabledExtensionNames.data();
#endif

  // the instance holds driver state, a process can own multiple instances
  VkInstance instance = VK_NULL_HANDLE;
  VkResult error = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
  if (error) {
    return error;
  }

#ifdef ENABLE_LAYERS
  // load the debug report extension function pointers
  auto vkCreateDebugReportCallbackEXT =
      reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(
          vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));
  auto vkDestroyDebugReportCallbackEXT =
      reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
          vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT"));

  // register the debug report callback
  VkDebugReportCallbackCreateInfoEXT callbackCreateInfo;
  callbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
  callbackCreateInfo.pNext = nullptr;
  // define the level of reports we want to receive
  callbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
                             VK_DEBUG_REPORT_WARNING_BIT_EXT |
                             VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
  callbackCreateInfo.pfnCallback = &debugReportCallback;
  callbackCreateInfo.pUserData = nullptr;
  VkDebugReportCallbackEXT callback = VK_NULL_HANDLE;
  error = vkCreateDebugReportCallbackEXT(instance, &callbackCreateInfo, nullptr,
                                         &callback);
#endif

  // get a list of all the available physical devices
  error = vkEnumeratePhysicalDevices(instance, &count, nullptr);
  if (error) {
    return error;
  }
  std::vector<VkPhysicalDevice> physicalDevices(count);
  error = vkEnumeratePhysicalDevices(instance, &count, physicalDevices.data());
  if (error) {
    return error;
  }

  // find a physical device which supports a compute queue
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  // we need the index into the array of queue families to create a device later
  uint32_t queueFamilyIndex = 0;
  for (auto device : physicalDevices) {
    // query the physical device for its queue properties
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count,
                                             queueFamilyProperties.data());
    // choose the first device which has a compute queue
    uint32_t index = 0;
    for (auto &properties : queueFamilyProperties) {
      if (properties.queueFlags & VK_QUEUE_COMPUTE_BIT) {
        // we've found a suitable physical device, store and break the loop
        physicalDevice = device;
        queueFamilyIndex = index;
        break;
      }
      index++;
    }
    if (physicalDevice) {
      break;
    }
  }

  // queue's are created at the same time as logical devices
  VkDeviceQueueCreateInfo queueCreateInfo = {};
  queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueCreateInfo.queueCount = 1;
  queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
  float queuePriority = 1.0f;  // specifies if this queue gets preference
  queueCreateInfo.pQueuePriorities = &queuePriority;

  // tell the driver what the logical device should enable
  VkDeviceCreateInfo deviceCreateInfo = {};
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.queueCreateInfoCount = 1;
  deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
#ifdef ENABLE_LAYERS
  // the list of enabled device layers must match the instance layers
  deviceCreateInfo.enabledLayerCount = enabledLayerNames.size();
  deviceCreateInfo.ppEnabledLayerNames = enabledLayerNames.data();
#endif
  VkDevice device = VK_NULL_HANDLE;
  error = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
  if (error) {
    return error;
  }

  // query the device for our compute queue
  VkQueue queue;
  vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

  // vector_add.comp uses 3 bindings
  VkDescriptorSetLayoutBinding layoutBindings[3] = {};

  // describe the first SSBO input used in the vector_add shader
  // layout (std430, set=0, binding=0) buffer inA { float A[]; };
  layoutBindings[0].binding = 0;
  layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  layoutBindings[0].descriptorCount = 1;
  layoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  // describe the second SSBO input in the vector_add shader
  // layout (std430, set=0, binding=1) buffer inB { float B[]; };
  layoutBindings[1].binding = 1;
  layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  layoutBindings[1].descriptorCount = 1;
  layoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  // describe the third SSBO output in the vector_add shader
  // layout (std430, set=0, binding=2) buffer outR { float R[]; };
  layoutBindings[2].binding = 2;
  layoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  layoutBindings[2].descriptorCount = 1;
  layoutBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  // use the descriptor bindings to define a layout to tell the driver where
  // descriptors are expected to live this is descriptor set 0 and refers to
  // set=0 in the shader
  VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = {};
  setLayoutCreateInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  setLayoutCreateInfo.bindingCount = 2;
  setLayoutCreateInfo.pBindings = layoutBindings;
  VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
  error = vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr,
                                      &setLayout);
  if (error) {
    return error;
  }

  // pipeline layouts can consist of multiple descritor set layouts
  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
  pipelineLayoutCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutCreateInfo.setLayoutCount = 1;  // but we only need one
  pipelineLayoutCreateInfo.pSetLayouts = &setLayout;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  error = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr,
                                 &pipelineLayout);

  // load vector_add.spv from file so we can create a pipeline
  VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
  shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  auto shaderCode = loadShaderCode("vector_add.spv");
  shaderModuleCreateInfo.pCode =
      reinterpret_cast<uint32_t *>(shaderCode.data());
  shaderModuleCreateInfo.codeSize = shaderCode.size();
  VkShaderModule shaderModule = VK_NULL_HANDLE;
  error = vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr,
                               &shaderModule);

  // create our compute pipeline from the shader module and the pipeline layout
  VkComputePipelineCreateInfo pipelineCreateInfo = {};
  pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineCreateInfo.stage.sType =
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  pipelineCreateInfo.stage.module = shaderModule;
  // name of the shader stage entry point for GLSL shaders is always "main"
  pipelineCreateInfo.stage.pName = "main";
  pipelineCreateInfo.layout = pipelineLayout;
  VkPipeline pipeline = VK_NULL_HANDLE;
  error = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1,
                                   &pipelineCreateInfo, nullptr, &pipeline);
  if (error)  {
    return error;
  }

  // a shader module can be destroyed after being consumed by a pipeline
  vkDestroyShaderModule(device, shaderModule, nullptr);

  // TODO: destriptor pool, set
  // TODO: command buffer
  // TODO: queue submit
  // TODO: verify results

  // destroy all the resources we created in reverse order
  vkDestroyPipeline(device, pipeline, nullptr);
  vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
  vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
  vkDestroyDevice(device, nullptr);
#ifdef ENABLE_LAYERS
  vkDestroyDebugReportCallbackEXT(instance, callback, nullptr);
#endif
  vkDestroyInstance(instance, nullptr);

  return 0;
}
