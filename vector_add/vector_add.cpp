#include <vulkan/vulkan.h>

#include <cstdio>
#include <vector>

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
int32_t findMemoryTypeFromProperties(
    uint32_t memoryTypeBits, VkPhysicalDeviceMemoryProperties properties,
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
  // tell the driver about your app
  VkApplicationInfo applicationInfo = {};
  applicationInfo.pApplicationName = "Vulkan compute example";
  // using version 1.0.0 is required so your app will work with any loader
  applicationInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);

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
  std::vector<VkDescriptorSetLayoutBinding> layoutBindings;

  // describe the first SSBO input used in the vector_add shader
  // layout (std430, set=0, binding=0) buffer inA { int A[]; };
  VkDescriptorSetLayoutBinding layoutBinding = {};
  layoutBinding.binding = 0;
  layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  layoutBinding.descriptorCount = 1;
  layoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  layoutBindings.push_back(layoutBinding);

  // describe the second SSBO input in the vector_add shader
  // layout (std430, set=0, binding=1) buffer inB { int B[]; };
  layoutBinding.binding = 1;
  layoutBindings.push_back(layoutBinding);

  // describe the third SSBO output in the vector_add shader
  // layout (std430, set=0, binding=2) buffer outR { int R[]; };
  layoutBinding.binding = 2;
  layoutBindings.push_back(layoutBinding);

  // use the descriptor bindings to define a layout to tell the driver where
  // descriptors are expected to live this is descriptor set 0 and refers to
  // set=0 in the shader
  VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = {};
  setLayoutCreateInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  setLayoutCreateInfo.bindingCount = layoutBindings.size();
  setLayoutCreateInfo.pBindings = layoutBindings.data();
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
  auto shaderCode = loadShaderCode(SHADER_PATH "vector_add.spv");
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
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipelineCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  pipelineCreateInfo.stage.module = shaderModule;
  // name of the shader stage entry point for GLSL shaders is always "main"
  pipelineCreateInfo.stage.pName = "main";
  pipelineCreateInfo.layout = pipelineLayout;
  VkPipeline pipeline = VK_NULL_HANDLE;
  error = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1,
                                   &pipelineCreateInfo, nullptr, &pipeline);
  if (error) {
    return error;
  }

  // a shader module can be destroyed after being consumed by the pipeline
  vkDestroyShaderModule(device, shaderModule, nullptr);

  // create the buffers which will hold the data to be consumed by out shader
  const uint32_t elements = 1024;
  VkBufferCreateInfo bufferCreateInfo = {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = sizeof(uint32_t) * elements;  // size in bytes
  // we will use SSBO or storage buffer so we can read and write
  bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  bufferCreateInfo.queueFamilyIndexCount = 1;
  bufferCreateInfo.pQueueFamilyIndices = &queueFamilyIndex;
  VkBuffer bufferA;
  error = vkCreateBuffer(device, &bufferCreateInfo, nullptr, &bufferA);
  if (error) {
    return error;
  }
  // all our buffers will be exactly like each other
  VkBuffer bufferB;
  error = vkCreateBuffer(device, &bufferCreateInfo, nullptr, &bufferB);
  if (error) {
    return error;
  }
  // so we can reuse the create info structure
  VkBuffer bufferResult;
  error = vkCreateBuffer(device, &bufferCreateInfo, nullptr, &bufferResult);
  if (error) {
    return error;
  }

  // a buffer does not own any memory but only describes how the buffer will be
  // used but a buffer without physical memory backing is not very useful so we
  // need to query each buffer for its memory requirements and figure out the
  // size of memory required
  VkDeviceSize requiredMemorySize = 0;
  VkMemoryRequirements bufferAMemoryRequirements;
  vkGetBufferMemoryRequirements(device, bufferA, &bufferAMemoryRequirements);
  requiredMemorySize += bufferAMemoryRequirements.size;
  VkMemoryRequirements bufferBMemoryRequirements;
  vkGetBufferMemoryRequirements(device, bufferA, &bufferBMemoryRequirements);
  requiredMemorySize += bufferBMemoryRequirements.size;
  VkMemoryRequirements bufferResultMemoryRequirements;
  vkGetBufferMemoryRequirements(device, bufferA,
                                &bufferResultMemoryRequirements);
  requiredMemorySize += bufferResultMemoryRequirements.size;

  // we need to find out about the physical devices memory properties
  VkPhysicalDeviceMemoryProperties memoryProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
  // find a compatible memory type which provides host access to the memory,
  // also ensure the memory is coherent so we don't have to manually flush the
  // cache to access data
  auto memoryTypeIndex = findMemoryTypeFromProperties(
      bufferAMemoryRequirements.memoryTypeBits, memoryProperties,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (0 > memoryTypeIndex) {
    return ~42;  // returns -43
  }

  // now we know how much memory we need we can allocate it all at once, it is
  // not efficient to allocate small blocks of device memory instead we must
  // manually sub-allocate out large memory block
  VkMemoryAllocateInfo allocateInfo = {};
  allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocateInfo.allocationSize = requiredMemorySize;
  allocateInfo.memoryTypeIndex = memoryTypeIndex;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  error = vkAllocateMemory(device, &allocateInfo, nullptr, &memory);
  if (error) {
    return error;
  }

  // to sub-allocate our memory block our buffers we bind the memory to the
  // buffer starting at offset 0
  VkDeviceSize memoryOffset = 0;
  error = vkBindBufferMemory(device, bufferA, memory, memoryOffset);
  if (error) {
    return error;
  }
  // each bind we increase the offset so it points to the end of the previous
  // buffer range
  memoryOffset += bufferAMemoryRequirements.size;
  error = vkBindBufferMemory(device, bufferB, memory, memoryOffset);
  if (error) {
    return error;
  }
  // since all of these buffers are of they same type their alignment
  // requirements match, however this will not always be the case so ensure
  // that the offset meets the buffer memory alignment requirements
  memoryOffset += bufferBMemoryRequirements.size;
  error = vkBindBufferMemory(device, bufferResult, memory, memoryOffset);
  if (error) {
    return error;
  }

  // now that we have our buffers we need a way to describe them to the driver
  // to do this we need a descriptor set however it is very common to have a
  // large number of small descriptor sets being allocated so to avoid large
  // numbers of small allocations a descriptor pool is used, this is analogous
  // to a memory heap but specialized for creating descriptor sets
  VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
  descriptorPoolCreateInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  // we only need one set in this example
  descriptorPoolCreateInfo.maxSets = 1;
  // and we only need one type of descriptor, when an application uses more
  // descriptor types a new pool is required for each descriptor type
  descriptorPoolCreateInfo.poolSizeCount = 1;
  VkDescriptorPoolSize poolSize = {};
  // we must provide the type of descriptor the pool will allocate
  poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  // and the number of descriptors
  poolSize.descriptorCount = layoutBindings.size();
  descriptorPoolCreateInfo.pPoolSizes = &poolSize;
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr,
                         &descriptorPool);

  // now we have our pool we can allocate a descriptor set
  VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
  descriptorSetAllocateInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  descriptorSetAllocateInfo.descriptorPool = descriptorPool;
  descriptorSetAllocateInfo.descriptorSetCount = 1;
  // this is the same layout we used to describe to the pipeline which
  // descriptors will be used
  descriptorSetAllocateInfo.pSetLayouts = &setLayout;
  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
  error = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo,
                                   &descriptorSet);
  if (error) {
    return error;
  }

  // now we need to update the descriptor set telling it about our buffers
  std::vector<VkWriteDescriptorSet> descriptorSetWrites;
  // we can reuse this structure as it will be copied each time we push to the
  // vector of descriptor set writes
  VkWriteDescriptorSet writeDescriptorSet = {};
  writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDescriptorSet.dstSet = descriptorSet;
  writeDescriptorSet.dstBinding = 0;
  writeDescriptorSet.dstArrayElement = 0;
  writeDescriptorSet.descriptorCount = 1;
  writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

  // each buffer needs its own buffer info as its passed as a pointer
  VkDescriptorBufferInfo bufferAInfo = {};
  bufferAInfo.buffer = bufferA;
  bufferAInfo.offset = 0;
  bufferAInfo.range = VK_WHOLE_SIZE;
  writeDescriptorSet.pBufferInfo = &bufferAInfo;
  descriptorSetWrites.push_back(writeDescriptorSet);

  VkDescriptorBufferInfo bufferBInfo = {};
  bufferBInfo.buffer = bufferB;
  bufferBInfo.offset = 0;
  bufferBInfo.range = VK_WHOLE_SIZE;
  // but we can reuse the write descriptor set structure
  writeDescriptorSet.dstBinding = 1;
  writeDescriptorSet.pBufferInfo = &bufferBInfo;
  descriptorSetWrites.push_back(writeDescriptorSet);

  VkDescriptorBufferInfo bufferResultInfo = {};
  bufferResultInfo.buffer = bufferResult;
  bufferResultInfo.offset = 0;
  bufferResultInfo.range = VK_WHOLE_SIZE;
  // just changing the binding and buffer info pointer for each buffer
  writeDescriptorSet.dstBinding = 2;
  writeDescriptorSet.pBufferInfo = &bufferResultInfo;
  descriptorSetWrites.push_back(writeDescriptorSet);

  vkUpdateDescriptorSets(device, descriptorSetWrites.size(),
                         descriptorSetWrites.data(), 0, nullptr);

  // as with descriptor sets command buffers are allocated from a pool
  VkCommandPoolCreateInfo commandPoolCreateInfo = {};
  commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  // our command buffer will only be used once so we set the transient bit
  commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
  VkCommandPool commandPool = VK_NULL_HANDLE;
  error = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr,
                              &commandPool);
  if (error) {
    return error;
  }

  // now we can create our command buffer
  VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
  commandBufferAllocateInfo.sType =
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandPool = commandPool;
  // we will use a primary command buffer in our example, secondary command
  // buffers are a powerful feature but we don't need that power here
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  commandBufferAllocateInfo.commandBufferCount = 1;
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
  error = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo,
                                   &commandBuffer);
  if (error) {
    return error;
  }

  // now we can record our commands
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  // first we find the compute pipeline containing our shader code
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
  // then we bind the descriptor set with out data
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
  // finally we record the dispatch command which will do the actual work
  vkCmdDispatch(commandBuffer, elements, 1, 1);

  // that was it!
  error = vkEndCommandBuffer(commandBuffer);
  if (error) {
    return error;
  }

  // before we submit the command buffer we need to map our memory and write
  // input data so that our shader doesn't produce garbage
  int32_t *data = nullptr;
  error = vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0,
                      reinterpret_cast<void **>(&data));
  if (error) {
    return error;
  }
  // the device memory can now be written from the host
  size_t dataOffset = 0;
  int32_t *aData = data;
  // as before we need to manually specify where our buffers data lives
  dataOffset += elements;
  int32_t *bData = data + elements;
  dataOffset += elements;
  int32_t *resultData = data + elements;
  // now we can write our data into the memory for each input buffer
  for (int32_t index = 0; index < elements; index++) {
    aData[index] = index;
    bData[index] = -index;
    // to ensure we are actually calculating a result we will set the result
    // data to 42, the actual result should be 0
    resultData[index] = 42;
  }
  // now we unmap the memory ready for the work to be submitted to the device
  vkUnmapMemory(device, memory);

  // submitting work to the queue is simply pointing it to the command buffer,
  // more complex applications will use semaphores and fences to perform
  // synchronisation
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;
  vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
  // but we can simply wait for all the work to be done
  vkQueueWaitIdle(queue);

  // now we map the memory again, read result and verify it
  data = nullptr;
  error = vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0,
                      reinterpret_cast<void **>(&data));
  dataOffset = 0;
  aData = data;
  dataOffset += elements;
  bData = data + dataOffset;
  dataOffset += elements;
  resultData = data + dataOffset;
  for (uint32_t index = 0; index < elements; index++) {
    if (resultData[index] != aData[index] + bData[index]) {
      fprintf(stderr, "result[%u] is '%d' not '%d'!\n", index,
              resultData[index], aData[index] + bData[index]);
    }
  }
  // and unmap before freeing the memory
  vkUnmapMemory(device, memory);

  // destroy all the resources we created in reverse order
  vkDestroyCommandPool(device, commandPool, nullptr);
  vkFreeMemory(device, memory, nullptr);
  vkDestroyBuffer(device, bufferResult, nullptr);
  vkDestroyBuffer(device, bufferB, nullptr);
  vkDestroyBuffer(device, bufferA, nullptr);
  vkDestroyDescriptorPool(device, descriptorPool, nullptr);
  vkDestroyPipeline(device, pipeline, nullptr);
  vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
  vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
  vkDestroyDevice(device, nullptr);
#ifdef ENABLE_LAYERS
  vkDestroyDebugReportCallbackEXT(instance, callback, nullptr);
#endif
  vkDestroyInstance(instance, nullptr);

  printf("success\n");

  return 0;
}
