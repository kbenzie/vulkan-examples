#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdio>
#include <vector>

#define PRINT_ERROR_CASE(ERROR)                                 \
  case ERROR:                                                   \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #ERROR); \
    break;

#define VK_FAIL_IF(RESULT)                                  \
  {                                                         \
    if (RESULT) {                                           \
      switch (RESULT) {                                     \
        PRINT_ERROR_CASE(VK_NOT_READY)                      \
        PRINT_ERROR_CASE(VK_TIMEOUT)                        \
        PRINT_ERROR_CASE(VK_EVENT_SET)                      \
        PRINT_ERROR_CASE(VK_EVENT_RESET)                    \
        PRINT_ERROR_CASE(VK_INCOMPLETE)                     \
        PRINT_ERROR_CASE(VK_ERROR_OUT_OF_HOST_MEMORY)       \
        PRINT_ERROR_CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY)     \
        PRINT_ERROR_CASE(VK_ERROR_INITIALIZATION_FAILED)    \
        PRINT_ERROR_CASE(VK_ERROR_DEVICE_LOST)              \
        PRINT_ERROR_CASE(VK_ERROR_MEMORY_MAP_FAILED)        \
        PRINT_ERROR_CASE(VK_ERROR_LAYER_NOT_PRESENT)        \
        PRINT_ERROR_CASE(VK_ERROR_EXTENSION_NOT_PRESENT)    \
        PRINT_ERROR_CASE(VK_ERROR_FEATURE_NOT_PRESENT)      \
        PRINT_ERROR_CASE(VK_ERROR_INCOMPATIBLE_DRIVER)      \
        PRINT_ERROR_CASE(VK_ERROR_TOO_MANY_OBJECTS)         \
        PRINT_ERROR_CASE(VK_ERROR_FORMAT_NOT_SUPPORTED)     \
        PRINT_ERROR_CASE(VK_ERROR_SURFACE_LOST_KHR)         \
        PRINT_ERROR_CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR) \
        PRINT_ERROR_CASE(VK_SUBOPTIMAL_KHR)                 \
        PRINT_ERROR_CASE(VK_ERROR_OUT_OF_DATE_KHR)          \
        PRINT_ERROR_CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR) \
        PRINT_ERROR_CASE(VK_ERROR_VALIDATION_FAILED_EXT)    \
        PRINT_ERROR_CASE(VK_ERROR_INVALID_SHADER_NV)        \
        default:                                            \
          fprintf(stderr, "Unknown VkResult %d\n", RESULT); \
          break;                                            \
      }                                                     \
      return RESULT;                                        \
    }                                                       \
  }

#define ASSERT(CONDITION, MESSAGE)                               \
  if (!(CONDITION)) {                                            \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, MESSAGE); \
    abort();                                                     \
  }

#define VK_GET_INSTANCE_PROC_ADDR(INSTANCE, FUNCTION) \
  auto FUNCTION = reinterpret_cast<PFN_##FUNCTION>(   \
      vkGetInstanceProcAddr(instance, #FUNCTION))

#ifdef ENABLE_LAYERS
// print out a debug report to stderr
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
  if (FILE *fp = fopen(filename, "rb")) {
    char buf[1024];
    while (size_t len = fread(buf, 1, sizeof(buf), fp)) {
      shaderCode.insert(shaderCode.end(), buf, buf + len);
    }
    fclose(fp);
  }
  return shaderCode;
}

// search for compatible memory properties and return the memory type index
int32_t findMemoryTypeIndexFromProperties(
    uint32_t memoryTypeBits, const VkPhysicalDeviceMemoryProperties &properties,
    VkMemoryPropertyFlags requiredProperties) {
  for (int32_t index = 0; index < properties.memoryTypeCount; ++index) {
    if (memoryTypeBits & (1 << index) &&
        (properties.memoryTypes[index].propertyFlags == requiredProperties)) {
      return index;
    }
  }
  return -1;
}

int main() {
  if (!glfwInit()) {
    fprintf(stderr, "GLFW failed to initialize\n");
    return 1;
  }

  if (!glfwVulkanSupported()) {
    fprintf(stderr, "GLFW failed to find Vulkan loader\n");
    return 1;
  }

  uint32_t width = 640;
  uint32_t height = 640;

  // tell the driver about your app
  VkApplicationInfo applicationInfo = {};
  applicationInfo.pApplicationName = "Vulkan graphics example";
  // using version 1.0.0 is required so your app will work with any loader
  applicationInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow *window = glfwCreateWindow(
      width, height, applicationInfo.pApplicationName, nullptr, nullptr);
  if (!window) {
    fprintf(stderr, "GLFW window creation failed");
    return ~42;
  }

#if 0
  glfwSetWindowUserPointer(window, nullptr);
  glfwSetWindowRefreshCallback(window, refreshCallback);
  glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
  glfwSetKeyCallback(window, keyCallback);
#endif

  // create the instance we will be using
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
  VK_FAIL_IF(error);

  VkSurfaceKHR surface = VK_NULL_HANDLE;
  glfwCreateWindowSurface(instance, window, NULL, &surface);

  // VK_GET_INSTANCE_PROC_ADDR(instance, vkCreateSwapchainKHR);
  // VK_GET_INSTANCE_PROC_ADDR(instance, vkDestroySwapchainKHR);
  // VK_GET_INSTANCE_PROC_ADDR(instance, vkGetSwapchainImagesKHR);
  VK_GET_INSTANCE_PROC_ADDR(instance,
                            vkGetPhysicalDeviceSurfaceCapabilitiesKHR);

#ifdef ENABLE_LAYERS
  // load the debug report extension function pointers
  VK_GET_INSTANCE_PROC_ADDR(instance, vkCreateDebugReportCallbackEXT);
  VK_GET_INSTANCE_PROC_ADDR(instance, vkDestroyDebugReportCallbackEXT);

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
  VK_FAIL_IF(error);
  std::vector<VkPhysicalDevice> physicalDevices(count);
  error = vkEnumeratePhysicalDevices(instance, &count, physicalDevices.data());
  VK_FAIL_IF(error);

  // find a physical device which supports a graphics queue
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  // we need the index into the array of queue families to create a device later
  uint32_t queueFamilyIndex = 0;
  for (auto device : physicalDevices) {
    // query the physical device for its queue properties
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count,
                                             queueFamilyProperties.data());
    // choose the first device which has a graphics queue
    uint32_t index = 0;
    for (auto &properties : queueFamilyProperties) {
      if (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
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
  VK_FAIL_IF(error);

  // query the device for our graphics queue
  VkQueue queue;
  vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

  VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
  shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  auto vertexShaderCode = loadShaderCode(SHADER_PATH "triangle.vert.spv");
  shaderModuleCreateInfo.pCode =
      reinterpret_cast<uint32_t *>(vertexShaderCode.data());
  shaderModuleCreateInfo.codeSize = vertexShaderCode.size();
  VkShaderModule vertexShaderModule = VK_NULL_HANDLE;
  error = vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr,
                               &vertexShaderModule);
  VK_FAIL_IF(error);

  auto fragmentShaderCode = loadShaderCode(SHADER_PATH "triangle.frag.spv");
  shaderModuleCreateInfo.pCode =
      reinterpret_cast<uint32_t *>(fragmentShaderCode.data());
  shaderModuleCreateInfo.codeSize = fragmentShaderCode.size();
  VkShaderModule fragmentShaderModule = VK_NULL_HANDLE;
  error = vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr,
                               &fragmentShaderModule);
  VK_FAIL_IF(error);

  std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfos;
  VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
  shaderStageCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStageCreateInfo.pName = "main";

  // vertex shader stage
  shaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStageCreateInfo.module = vertexShaderModule;
  shaderStageCreateInfos.push_back(shaderStageCreateInfo);

  // fragment shader stage
  shaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStageCreateInfo.module = fragmentShaderModule;
  shaderStageCreateInfos.push_back(shaderStageCreateInfo);

  std::vector<VkVertexInputBindingDescription> vertexBindings;
  std::vector<VkVertexInputAttributeDescription> vertexAttributes;

  VkVertexInputBindingDescription vertexBinding = {};
  VkVertexInputAttributeDescription vertexAttribute = {};

  // layout (location=0) in vec4 vertex_position;
  vertexBinding.binding = 0;
  vertexBinding.stride = sizeof(float) * 4;
  vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  vertexBindings.push_back(vertexBinding);
  vertexAttribute.location = 0;
  vertexAttribute.binding = 0;
  vertexAttribute.format = VK_FORMAT_R32G32B32A32_SFLOAT;
  vertexAttribute.offset = 0;
  vertexAttributes.push_back(vertexAttribute);

  // layout (location=1) in vec4 vertex_color;
  vertexBinding.binding = 1;
  vertexBindings.push_back(vertexBinding);
  vertexAttribute.location = 1;
  vertexAttribute.binding = 1;
  vertexAttribute.format = VK_FORMAT_R32G32B32A32_SFLOAT;
  vertexAttribute.offset = 0;
  vertexAttributes.push_back(vertexAttribute);

  VkPipelineVertexInputStateCreateInfo vertexInputState = {};
  vertexInputState.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputState.vertexBindingDescriptionCount = vertexBindings.size();
  vertexInputState.pVertexBindingDescriptions = vertexBindings.data();
  vertexInputState.vertexAttributeDescriptionCount = vertexAttributes.size();
  vertexInputState.pVertexAttributeDescriptions = vertexAttributes.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
  inputAssemblyState.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkViewport viewport = {};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(width);
  viewport.height = static_cast<float>(height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor = {};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent.width = width;
  scissor.extent.height = height;

  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizationState = {};
  rasterizationState.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizationState.cullMode =
      VK_CULL_MODE_NONE;  // TODO: Use back face culling
  rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

  std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
  // layout (location=0) in vec4 vertex_position;
  VkDescriptorSetLayoutBinding layoutBinding = {};
  layoutBinding.binding = 0;
  layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  layoutBinding.descriptorCount = 1;
  layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  layoutBindings.push_back(layoutBinding);
  // layout (location=1) in vec4 vertex_color;
  layoutBinding.binding = 1;
  layoutBindings.push_back(layoutBinding);

  VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = {};
  setLayoutCreateInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  setLayoutCreateInfo.bindingCount = layoutBindings.size();
  setLayoutCreateInfo.pBindings = layoutBindings.data();
  VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
  error = vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr,
                                      &setLayout);
  VK_FAIL_IF(error);

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
  pipelineLayoutCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutCreateInfo.setLayoutCount = 1;
  pipelineLayoutCreateInfo.pSetLayouts = &setLayout;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  error = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr,
                                 &pipelineLayout);
  VK_FAIL_IF(error);

  VkAttachmentDescription colorAttachmentDescription = {};
  colorAttachmentDescription.format = VK_FORMAT_R32G32B32A32_SFLOAT;
  colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachmentDescription.initialLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  colorAttachmentDescription.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference colorAttachmentReference = {};
  // layout (location=0) out vec4 frag_color;
  colorAttachmentReference.attachment = 0;
  colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription depthStencilAttachmentDescription = {};
  depthStencilAttachmentDescription.format = VK_FORMAT_D16_UNORM;
  depthStencilAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
  depthStencilAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthStencilAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depthStencilAttachmentDescription.stencilLoadOp =
      VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthStencilAttachmentDescription.stencilStoreOp =
      VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthStencilAttachmentDescription.initialLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  depthStencilAttachmentDescription.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthStencilAttachmentReference = {};
  depthStencilAttachmentReference.attachment = 1;
  depthStencilAttachmentReference.layout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  // subpass.inputAttachmentCount = ;
  // subpass.pInputAttachments = ;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentReference;
  // subpass.pResolveAttachments;
  subpass.pDepthStencilAttachment = &depthStencilAttachmentReference;
  // subpass.preserveAttachmentCount;
  // subpass.pPreserveAttachments;

  std::array<VkAttachmentDescription, 2> attachmentDescriptions;
  attachmentDescriptions[0] = colorAttachmentDescription;
  attachmentDescriptions[1] = depthStencilAttachmentDescription;

  VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
  depthStencilState.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencilState.depthTestEnable = VK_TRUE;
  depthStencilState.depthWriteEnable = VK_TRUE;
  depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  depthStencilState.depthBoundsTestEnable = VK_FALSE;
  depthStencilState.stencilTestEnable = VK_FALSE;
  depthStencilState.front.failOp = VK_STENCIL_OP_KEEP;
  depthStencilState.front.passOp = VK_STENCIL_OP_KEEP;
  depthStencilState.front.compareOp = VK_COMPARE_OP_ALWAYS;
  depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
  depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
  depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
  // depthStencilState.minDepthBounds;
  // depthStencilState.maxDepthBounds;

  VkRenderPassCreateInfo renderPassCreateInfo = {};
  renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  VkRenderPass renderPass = VK_NULL_HANDLE;
  renderPassCreateInfo.attachmentCount = attachmentDescriptions.size();
  renderPassCreateInfo.pAttachments = attachmentDescriptions.data();
  renderPassCreateInfo.subpassCount = 1;
  renderPassCreateInfo.pSubpasses = &subpass;
  error =
      vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);
  VK_FAIL_IF(error);

  VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
  pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineCreateInfo.stageCount = shaderStageCreateInfos.size();
  pipelineCreateInfo.pStages = shaderStageCreateInfos.data();
  pipelineCreateInfo.pVertexInputState = &vertexInputState;
  pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
  pipelineCreateInfo.pViewportState = &viewportState;
  pipelineCreateInfo.pRasterizationState = &rasterizationState;
  pipelineCreateInfo.pDepthStencilState  = &depthStencilState;
  pipelineCreateInfo.layout = pipelineLayout;
  pipelineCreateInfo.renderPass = renderPass;
  VkPipeline pipeline = VK_NULL_HANDLE;
  error = vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineCreateInfo,
                                    nullptr, &pipeline);
  VK_FAIL_IF(error);

  VkCommandPoolCreateInfo commandPoolCreateInfo = {};
  commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
  VkCommandPool commandPool = VK_NULL_HANDLE;
  error = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr,
                              &commandPool);
  VK_FAIL_IF(error);

  VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
  commandBufferAllocateInfo.sType =
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandPool = commandPool;
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  commandBufferAllocateInfo.commandBufferCount = 1;
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
  error = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo,
                                   &commandBuffer);
  VK_FAIL_IF(error);

  VkDescriptorPoolSize poolSize = {};
  poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSize.descriptorCount = 2;

  VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
  descriptorPoolCreateInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolCreateInfo.maxSets = 1;
  descriptorPoolCreateInfo.poolSizeCount = 1;
  descriptorPoolCreateInfo.pPoolSizes = &poolSize;
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  error = vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr,
                                 &descriptorPool);
  VK_FAIL_IF(error);

  VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
  descriptorSetAllocateInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  descriptorSetAllocateInfo.descriptorPool = descriptorPool;
  descriptorSetAllocateInfo.descriptorSetCount = 1;
  descriptorSetAllocateInfo.pSetLayouts = &setLayout;
  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
  error = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo,
                                   &descriptorSet);
  VK_FAIL_IF(error);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  error = vkBeginCommandBuffer(commandBuffer, &beginInfo);
  VK_FAIL_IF(error);

  struct vec4 {
    float x, y, z, w;
  };

  VkBufferCreateInfo bufferCreateInfo = {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = sizeof(vec4) * 3;
  bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  bufferCreateInfo.queueFamilyIndexCount = 1;
  bufferCreateInfo.pQueueFamilyIndices = &queueFamilyIndex;
  VkBuffer vertexPositions = VK_NULL_HANDLE;
  error = vkCreateBuffer(device, &bufferCreateInfo, nullptr, &vertexPositions);
  VK_FAIL_IF(error);

  VkBuffer vertexColors = VK_NULL_HANDLE;
  error = vkCreateBuffer(device, &bufferCreateInfo, nullptr, &vertexColors);
  VK_FAIL_IF(error);

  VkMemoryRequirements memoryRequirements;
  vkGetBufferMemoryRequirements(device, vertexPositions, &memoryRequirements);

  VkPhysicalDeviceMemoryProperties memoryProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
  auto memoryTypeIndex = findMemoryTypeIndexFromProperties(
      memoryRequirements.memoryTypeBits, memoryProperties,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  ASSERT(0 <= memoryTypeIndex, "memoryTypeIndex not found");

  VkMemoryAllocateInfo allocateInfo = {};
  allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocateInfo.allocationSize = memoryRequirements.size * 2;
  allocateInfo.memoryTypeIndex = memoryTypeIndex;
  VkDeviceMemory bufferMemory = VK_NULL_HANDLE;
  error = vkAllocateMemory(device, &allocateInfo, nullptr, &bufferMemory);
  VK_FAIL_IF(error);

  error = vkBindBufferMemory(device, vertexPositions, bufferMemory, 0);
  VK_FAIL_IF(error);
  error = vkBindBufferMemory(device, vertexColors, bufferMemory,
                             sizeof(vec4) * 3);
  VK_FAIL_IF(error);

  vec4 *data = nullptr;
  error = vkMapMemory(device, bufferMemory, 0, VK_WHOLE_SIZE, 0,
                      reinterpret_cast<void **>(&data));

  auto vertexPositionData = data;
  vertexPositionData[0] = {0.25f, 0.75f, 0.0f, 1.0f};  // bottom left
  vertexPositionData[1] = {0.5f, 0.25f, 0.0f, 1.0f};   // top middle
  vertexPositionData[2] = {0.75f, 0.75f, 0.0f, 1.0f};  // bottom right
  auto vertexColorData = data + 3;
  vertexColorData[0] = {1.0f, 0.0f, 0.0f, 1.0f};  // red
  vertexColorData[1] = {0.0f, 1.0f, 0.0f, 1.0f};  // blue
  vertexColorData[2] = {0.0f, 0.0f, 1.0f, 1.0f};  // green

  vkUnmapMemory(device, bufferMemory);

  std::vector<VkImageView> attachments;

  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count,
                                       nullptr);
  std::vector<VkSurfaceFormatKHR> surfaceFormats(count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count,
                                       surfaceFormats.data());

  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                            &surfaceCapabilities);

  VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
  swapchainCreateInfo.surface = surface;
  swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount + 1;
  swapchainCreateInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
  swapchainCreateInfo.imageColorSpace = surfaceFormats[0].colorSpace;
  swapchainCreateInfo.imageExtent.width = width;
  swapchainCreateInfo.imageExtent.height = height;
  swapchainCreateInfo.imageArrayLayers = 1;
  swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchainCreateInfo.queueFamilyIndexCount = 1;
  swapchainCreateInfo.pQueueFamilyIndices = &queueFamilyIndex;
  swapchainCreateInfo.preTransform =
      surfaceCapabilities
          .currentTransform;  // TODO: This might need to be different?
  swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  swapchainCreateInfo.clipped = VK_TRUE;
  swapchainCreateInfo.oldSwapchain = nullptr;  // TODO: Required for resize
  swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  error =
      vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain);

  VkImageViewCreateInfo swapchainViewCreateInfo = {};
  swapchainViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  swapchainViewCreateInfo.image = swapchainImage;
  swapchainViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  swapchainViewCreateInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
  swapchainViewCreateInfo.components;
  swapchainViewCreateInfo.subresourceRange;
  VkImageView swapchainView = VK_NULL_HANDLE;
  vkCreateImageView(device, &swapchainViewCreateInfo, )

  attachments.push_back(swapchainView);

  VkImageCreateInfo depthStencilCreateInfo = {};
  depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  depthStencilCreateInfo.imageType = VK_IMAGE_TYPE_2D;
  depthStencilCreateInfo.format = VK_FORMAT_D16_UNORM;
  depthStencilCreateInfo.extent.width = width;
  depthStencilCreateInfo.extent.height = height;
  depthStencilCreateInfo.extent.depth = 1;
  depthStencilCreateInfo.mipLevels = 1;
  depthStencilCreateInfo.arrayLayers = 1;
  depthStencilCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  depthStencilCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  depthStencilCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  // depthStencilCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  // depthStencilCreateInfo.queueFamilyIndexCount = 1;
  // depthStencilCreateInfo.pQueueFamilyIndices = &queueFamilyIndex;
  // depthStencilCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImage depthStencil = VK_NULL_HANDLE;
  error = vkCreateImage(device, &depthStencilCreateInfo, nullptr, &depthStencil);
  VK_FAIL_IF(error);

  VkImageViewCreateInfo depthStencilViewCreateInfo = {};
  depthStencilViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  depthStencilViewCreateInfo.image = depthStencil;
  depthStencilViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  depthStencilViewCreateInfo.format = depthStencilCreateInfo.format;
  depthStencilViewCreateInfo.subresourceRange.aspectMask =
      VK_IMAGE_ASPECT_DEPTH_BIT;
  depthStencilViewCreateInfo.subresourceRange.baseMipLevel = 0;
  depthStencilViewCreateInfo.subresourceRange.levelCount = 1;
  depthStencilViewCreateInfo.subresourceRange.baseArrayLayer = 0;
  depthStencilViewCreateInfo.subresourceRange.layerCount = 1;
  VkImageView depthStencilView = VK_NULL_HANDLE;
  error = vkCreateImageView(device, &depthStencilViewCreateInfo, nullptr,
                            &depthStencilView);
  VK_FAIL_IF(error);
  attachments.push_back(depthStencilView);

  memoryRequirements = {};
  vkGetImageMemoryRequirements(device, depthStencil, &memoryRequirements);
  memoryTypeIndex = findMemoryTypeIndexFromProperties(
      memoryRequirements.memoryTypeBits, memoryProperties,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  ASSERT(0 <= memoryTypeIndex, "memoryTypeIndex not found");

  allocateInfo.allocationSize = memoryRequirements.size;
  allocateInfo.memoryTypeIndex = memoryTypeIndex;
  VkDeviceMemory imageMemory;
  vkAllocateMemory(device, &allocateInfo, nullptr, &imageMemory);

  VkFramebufferCreateInfo framebufferCreateInfo = {};
  framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferCreateInfo.renderPass = renderPass;
  framebufferCreateInfo.attachmentCount = attachments.size();
  framebufferCreateInfo.pAttachments = attachments.data();
  framebufferCreateInfo.width = width;
  framebufferCreateInfo.height = height;
  framebufferCreateInfo.layers = 1;
  VkFramebuffer framebuffer;
  error = vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr,
                              &framebuffer);
  VK_FAIL_IF(error);

  VkRenderPassBeginInfo renderPassBeginInfo = {};
  renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassBeginInfo.renderPass = renderPass;
  renderPassBeginInfo.framebuffer = framebuffer;
  renderPassBeginInfo.renderArea.offset.x = 0;
  renderPassBeginInfo.renderArea.offset.y = 0;
  renderPassBeginInfo.renderArea.extent.width = width;
  renderPassBeginInfo.renderArea.extent.height = height;
  std::array<VkClearValue, 2> clearValues;
  clearValues[0].color.float32[0] = 0.2f;
  clearValues[0].color.float32[1] = 0.2f;
  clearValues[0].color.float32[2] = 0.2f;
  clearValues[0].color.float32[3] = 1.0f;
  clearValues[1].depthStencil.depth = 0.5f;  // TODO: Does this make sense?
  clearValues[1].depthStencil.stencil = 0;
  renderPassBeginInfo.clearValueCount = clearValues.size();
  renderPassBeginInfo.pClearValues = clearValues.data();
  vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  std::vector<VkBuffer> vertexBuffers = {vertexPositions, vertexColors};
  std::vector<VkDeviceSize> vertexBufferOffsets = {0, 0};
  vkCmdBindVertexBuffers(commandBuffer, 0, vertexBuffers.size(),
                         vertexBuffers.data(), vertexBufferOffsets.data());
  vkCmdDraw(commandBuffer, 3, 1, 0, 0);

  vkCmdEndRenderPass(commandBuffer);

  error = vkEndCommandBuffer(commandBuffer);
  VK_FAIL_IF(error);

  // destroy all the resources we created in reverse order
  vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
  vkDestroyCommandPool(device, commandPool, nullptr);

  vkDestroyDescriptorPool(device, descriptorPool, nullptr);
  vkDestroyRenderPass(device, renderPass, nullptr);
  vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
  vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
  vkDestroyPipeline(device, pipeline, nullptr);
  vkDestroyShaderModule(device, fragmentShaderModule, nullptr);
  vkDestroyShaderModule(device, vertexShaderModule, nullptr);
  vkDestroyDevice(device, nullptr);
#ifdef ENABLE_LAYERS
  vkDestroyDebugReportCallbackEXT(instance, callback, nullptr);
#endif
  vkDestroyInstance(instance, nullptr);
  glfwTerminate();

  return 0;
}
