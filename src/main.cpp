#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/CAMetalLayer.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <AppKit/AppKit.hpp>
#include <random>
#include <iostream>
#include <fstream>
#include <sstream>
#include <simd/simd.h>
#include <math.h>

#include "backend/glfw_adapter.h"

struct vertex
{
    simd::float2 pos;
    simd::float3 color;
};

struct mesh
{
    MTL::Buffer *vertex_buffer;
    MTL::Buffer *index_buffer;
};

struct point_pos
{
    float x, y;
};

simd::float4x4 identity_mat()
{
    simd_float4 col0 = {1.0f, 0.0f, 0.0f, 0.0f};
    simd_float4 col1 = {0.0f, 1.0f, 0.0f, 0.0f};
    simd_float4 col2 = {0.0f, 0.0f, 1.0f, 0.0f};
    simd_float4 col3 = {0.0f, 0.0f, 0.0f, 1.0f};
    
    return simd_matrix(col0, col1, col2, col3);
}

simd::float4x4 translation(simd::float3 dPos)
{
    simd_float4 col0 = {1.0f, 0.0f, 0.0f, 0.0f};
    simd_float4 col1 = {0.0f, 1.0f, 0.0f, 0.0f};
    simd_float4 col2 = {0.0f, 0.0f, 1.0f, 0.0f};
    simd_float4 col3 = {dPos[0], dPos[1], dPos[2], 1.0f};

    return simd_matrix(col0, col1, col2, col3);
}

simd::float4x4 z_rotation(float theta)
{
    theta = theta * M_PI / 180.0f;
    float c = cosf(theta);
    float s = sinf(theta);

    simd_float4 col0 = {c, s, 0.0f, 0.0f};
    simd_float4 col1 = {-s, c, 0.0f, 0.0f};
    simd_float4 col2 = {0.0f, 0.0f, 1.0f, 0.0f};
    simd_float4 col3 = {0.0f, 0.0f, 0.0f, 1.0f};

    return simd_matrix(col0, col1, col2, col3);
}

simd::float4x4 scale(float factor)
{
    simd_float4 col0 = {factor, 0.0f, 0.0f, 0.0f};
    simd_float4 col1 = {0.0f, factor, 0.0f, 0.0f};
    simd_float4 col2 = {0.0f, 0.0f, factor, 0.0f};
    simd_float4 col3 = {0.0f, 0.0f, 0.0f, 1.0f};
    
    return simd_matrix(col0, col1, col2, col3);
}

MTL::RenderPipelineState *build_shader(const char *filename, const char *vert_name, const char *frag_name, MTL::Device *device)
{
    std::ifstream file;
    file.open(filename);
    std::stringstream reader;
    reader << file.rdbuf();
    std::string str = reader.str();
    std::cout << str << std::endl;
    NS::String *shader_code = NS::String::string(str.c_str(), NS::StringEncoding::UTF8StringEncoding);
    NS::Error *error = nullptr;
    MTL::CompileOptions *options = nullptr;
    MTL::Library *library = device->newLibrary(shader_code, options, &error);
    if (!library)
    {
        std::cout << error->localizedDescription()->utf8String() << std::endl;
        return nullptr;
    }
    NS::String *vertexName = NS::String::string(vert_name, NS::StringEncoding::UTF8StringEncoding);
    MTL::Function *vertex_func = library->newFunction(vertexName);
    NS::String *fragName = NS::String::string(frag_name, NS::StringEncoding::UTF8StringEncoding);
    MTL::Function *frag_func = library->newFunction(fragName);

    MTL::RenderPipelineDescriptor *descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setVertexFunction(vertex_func);
    descriptor->setFragmentFunction(frag_func);
    descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm);
    
    MTL::VertexDescriptor *vertex_descriptor = MTL::VertexDescriptor::alloc()->init();
    auto attributes = vertex_descriptor->attributes();
    auto pos_descriptor = attributes->object(0);
    pos_descriptor->setFormat(MTL::VertexFormatFloat2);
    pos_descriptor->setOffset(0);
    pos_descriptor->setBufferIndex(0);

    auto color_descriptor = attributes->object(1);
    color_descriptor->setFormat(MTL::VertexFormatFloat3);
    color_descriptor->setOffset(4 * sizeof(float));
    color_descriptor->setBufferIndex(0);

    auto layout_descriptor = vertex_descriptor->layouts()->object(0);
    layout_descriptor->setStride(8 * sizeof(float));

    descriptor->setVertexDescriptor(vertex_descriptor);
    MTL::RenderPipelineState *render_pipeline = device->newRenderPipelineState(descriptor, &error);
    if (!render_pipeline)
    {
        std::cout << error->localizedDescription()->utf8String() << std::endl;
        return nullptr;
    }
    descriptor->release();
    vertex_func->release();
    frag_func->release();
    library->release();
    return render_pipeline;
}

MTL::Buffer *build_triangle(MTL::Device* device)
{
    vertex vertices[3] = 
    {
        {{-0.75, -0.75}, {1.0, 0.0, 0.0}},
        {{0.75, -0.75}, {0.0, 1.0, 0.0}},
        {{0.0, 0.75}, {0.0, 0.0, 1.0}}
    };


    MTL::Buffer *buffer = device->newBuffer(3 * sizeof(vertex), MTL::ResourceStorageModeShared);
    memcpy(buffer->contents(), vertices, 3 * sizeof(vertex));
    return buffer;
}

mesh build_quadrilater(MTL::Device *device)
{
    mesh Mesh;
    vertex vertices[4] = 
    {
        {{-0.75, -0.75}, {1.0, 0.0, 0.0}},
        {{0.75, -0.75}, {0.0, 1.0, 0.0}},
        {{0.75, 0.75}, {0.0, 0.0, 1.0}},
        {{-0.75, 0.75}, {0.0, 1.0, 0.0}}
    };

    ushort indices[6] = {0, 1, 2, 2, 3, 0};

    Mesh.vertex_buffer = device->newBuffer(4 * sizeof(vertex), MTL::ResourceStorageModeShared);
    memcpy(Mesh.vertex_buffer->contents(), vertices, 4 * sizeof(vertex));

    Mesh.index_buffer = device->newBuffer(6 * sizeof(ushort), MTL::ResourceStorageModeShared);
    memcpy(Mesh.index_buffer->contents(), indices, 6 * sizeof(ushort));
    
    return Mesh;
}

mesh build_quadrilater(MTL::Device *device, point_pos *positions)
{
    mesh Mesh;
    vertex vertices[4] = 
    {
        {{positions[0].x, positions[0].y}, {1.0, 1.0, 1.0}},
        {{positions[1].x, positions[1].y}, {1.0, 1.0, 1.0}},
        {{positions[2].x, positions[2].y}, {1.0, 1.0, 1.0}},
        {{positions[3].x, positions[3].y}, {1.0, 1.0, 1.0}}
    };

    ushort indices[6] = {0, 1, 2, 2, 3, 0};

    Mesh.vertex_buffer = device->newBuffer(4 * sizeof(vertex), MTL::ResourceStorageModeShared);
    memcpy(Mesh.vertex_buffer->contents(), vertices, 4 * sizeof(vertex));

    Mesh.index_buffer = device->newBuffer(6 * sizeof(ushort), MTL::ResourceStorageModeShared);
    memcpy(Mesh.index_buffer->contents(), indices, 6 * sizeof(ushort));
    
    return Mesh;
}

void draw(simd::float4x4 transform, MTL::RenderCommandEncoder* renderCommandEncoder, MTL::Buffer *buffer)
{
    renderCommandEncoder->setVertexBytes(&transform, sizeof(simd::float4x4), 1);
    renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
}

void draw_indexed(simd::float4x4 transform, MTL::RenderCommandEncoder* renderCommandEncoder, mesh &mesh)
{
    renderCommandEncoder->setVertexBytes(&transform, sizeof(simd::float4x4), 1);
    renderCommandEncoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(6), MTL::IndexType::IndexTypeUInt16,
                                                mesh.index_buffer, NS::UInteger(0), NS::UInteger(1));
}

int main() 
{
    //glfw stuff
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* glfwWindow = glfwCreateWindow(800, 600, "Test", NULL, NULL);
    std::default_random_engine generator;
    std::uniform_real_distribution<float> distribution(0.0, 1.0);
    //Metal Device
    MTL::Device* device = MTL::CreateSystemDefaultDevice();

    //Metal Layer
    CA::MetalLayer* metalLayer = CA::MetalLayer::layer()->retain();
    metalLayer->setDevice(device);
    metalLayer->setPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm);

    //Adapt glfw window to NS Window, and give it a layer to draw to
    NS::Window* window = get_ns_window(glfwWindow, metalLayer)->retain();

    //Drawable Area
    CA::MetalDrawable* metalDrawable;

    //Command Queue
    MTL::CommandQueue* commandQueue = device->newCommandQueue()->retain();

    MTL::RenderPipelineState *render_pipeline = build_shader("/Users/luna/metal-cpp_test/src/shaders/triangle.metal", "vertexMain", "fragmentMain", device);
    if (!render_pipeline)
    {
        return -1;
    }
    MTL::RenderPipelineState *general_pipeline =build_shader("/Users/luna/metal-cpp_test/src/shaders/general.metal", "vertexMainGeneral", "fragmentMainGeneral", device);
    if (!general_pipeline)
    {
        return -1;
    }
    MTL::Buffer *triangleMesh = build_triangle(device);
    mesh quadrilaterMesh = build_quadrilater(device);
    float t = 0.0f;
    float positions[1200];
    for (auto &position: positions)
        position = distribution(generator);
    float steps[1200];
    for (auto &step: steps)
        step = 0.01f;
    while (!glfwWindowShouldClose(glfwWindow)) 
    {
        t += 1.0f;
        if (t > 360.0f)
            t -= 360.0f;
        glfwPollEvents();
        for (int i = 0; i < 1200; i++)
        {
            positions[i] += steps[i];
            if (positions[i] > 0.75f)
                steps[i] = -0.01;
            else if (positions[i] < -0.75)
                steps[i] = 0.01;
        }
        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
        metalDrawable = metalLayer->nextDrawable();
        MTL::CommandBuffer* commandBuffer = commandQueue->commandBuffer();
        MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
        MTL::RenderPassColorAttachmentDescriptor* colorAttachment = renderPassDescriptor->colorAttachments()->object(0);
        colorAttachment->setTexture(metalDrawable->texture());
        colorAttachment->setLoadAction(MTL::LoadActionClear);
        colorAttachment->setClearColor(MTL::ClearColor(0.0f, 0.0f, 0.0f, 1.0f));
        colorAttachment->setStoreAction(MTL::StoreActionStore);

        MTL::RenderCommandEncoder* renderCommandEncoder = commandBuffer->renderCommandEncoder(renderPassDescriptor);
        renderCommandEncoder->setRenderPipelineState(general_pipeline);
        simd::float4x4 transform = identity_mat();
        renderCommandEncoder->setVertexBuffer(quadrilaterMesh.vertex_buffer, 0, 0);
        draw_indexed(transform, renderCommandEncoder, quadrilaterMesh);
        renderCommandEncoder->setVertexBuffer(triangleMesh, 0, 0);
        for (int i = 0; i < 1200; i++)
        {
            transform = translation(simd_float3{positions[i], 0.2f, 0.0f}) * z_rotation(t) * scale(0.3f);
            draw(transform, renderCommandEncoder, triangleMesh);
        }
        renderCommandEncoder->endEncoding();
        commandBuffer->presentDrawable(metalDrawable);
        commandBuffer->commit();
        commandBuffer->waitUntilCompleted();
        pool->release();
    }


    glfwTerminate();
    device->release();
    triangleMesh->release();
    commandQueue->release();
    window->release();
    metalLayer->release();

    return 0;
}
