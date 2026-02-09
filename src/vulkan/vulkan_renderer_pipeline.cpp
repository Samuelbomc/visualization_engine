// =============================================================================
// vulkan_renderer_pipeline.cpp
// Creación y recreación del pipeline gráfico de Vulkan.
// El pipeline es un objeto compilado e inmutable que define cómo se procesan
// los vértices y fragmentos: shaders, vertex input, rasterización, MSAA,
// depth test, color blending y estados dinámicos.
// =============================================================================

#include "vulkan_renderer.hpp"
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// readFile: lee un archivo binario completo en un vector de bytes.
// Se usa para cargar los shaders SPIR-V compilados desde disco.
// Abre el archivo posicionándose al final (ate) para obtener el tamaño total,
// luego rebobina y lee todo el contenido de una vez.
// -----------------------------------------------------------------------------
std::vector<char> VulkanRenderer::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

// -----------------------------------------------------------------------------
// createShaderModule: envuelve bytecode SPIR-V en un VkShaderModule que puede
// vincularse a una etapa del pipeline. El bytecode debe estar alineado a 4 bytes.
// -----------------------------------------------------------------------------
VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }
    return shaderModule;
}

// -----------------------------------------------------------------------------
// loadShaderModules: lee los archivos SPIR-V del vertex y fragment shader
// y crea módulos de shader que se cachean para toda la vida del renderer.
// Esto evita releer los archivos desde disco cada vez que se recrea el pipeline.
// -----------------------------------------------------------------------------
void VulkanRenderer::loadShaderModules() {
    const std::string shaderDir = SHADER_DIR;
    auto vertShaderCode = readFile(shaderDir + "/shader.vert.spv");
    auto fragShaderCode = readFile(shaderDir + "/shader.frag.spv");

    cachedVertShaderModule = createShaderModule(vertShaderCode);
    cachedFragShaderModule = createShaderModule(fragShaderCode);
}

// -----------------------------------------------------------------------------
// destroyShaderModules: destruye los módulos de shader cacheados.
// Se llama una sola vez en el destructor del renderer.
// -----------------------------------------------------------------------------
void VulkanRenderer::destroyShaderModules() {
    if (cachedVertShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, cachedVertShaderModule, nullptr);
        cachedVertShaderModule = VK_NULL_HANDLE;
    }
    if (cachedFragShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, cachedFragShaderModule, nullptr);
        cachedFragShaderModule = VK_NULL_HANDLE;
    }
}

// -----------------------------------------------------------------------------
// recreateGraphicsPipeline: destruye el pipeline y layout existentes y los
// recrea desde cero. Se invoca cuando cambia el layout de vértices, la
// topología de la malla, o el render pass (tras recrear el swapchain).
// -----------------------------------------------------------------------------
void VulkanRenderer::recreateGraphicsPipeline() {
    if (graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        graphicsPipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
    createGraphicsPipeline();
}

// -----------------------------------------------------------------------------
// createGraphicsPipeline: configura y crea el pipeline gráfico completo.
//
// Etapas del pipeline configuradas:
//   1. Shader stages: vertex y fragment shader (módulos cacheados).
//   2. Vertex input: describe el layout de los vértices de la malla actual
//      (binding description y attribute descriptions dinámicos).
//   3. Input assembly: topología de la geometría (ej: TRIANGLE_LIST).
//   4. Viewport/scissor: dinámicos, configurados por frame en recordCommandBuffer.
//   5. Rasterización: relleno de polígonos, culling de caras traseras (BACK_BIT),
//      front face counter-clockwise.
//   6. Multisampling: nivel de MSAA determinado al inicio, con sample shading
//      habilitado para suavizar el interior de los polígonos (no solo bordes).
//   7. Depth/stencil: depth test activado con COMPARE_OP_LESS.
//   8. Color blending: desactivado (escritura directa de colores).
//   9. Pipeline layout: referencia al descriptor set layout (UBO).
//
// Se usa el pipeline cache para acelerar la compilación si el driver puede
// reutilizar resultados previos.
// Los shader modules no se destruyen aquí porque están cacheados.
// -----------------------------------------------------------------------------
void VulkanRenderer::createGraphicsPipeline() {
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = cachedVertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = cachedFragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    const auto& geometry = mesh.getData();
    const auto& bindingDescription = geometry.bindingDescription;
    const auto& attributeDescriptions = geometry.attributeDescriptions;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = geometry.topology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    const std::array<VkDynamicState, 2> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = msaaSamples;
    multisampling.sampleShadingEnable = (msaaSamples != VK_SAMPLE_COUNT_1_BIT) ? VK_TRUE : VK_FALSE;
    multisampling.minSampleShading = 0.2f;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }
}