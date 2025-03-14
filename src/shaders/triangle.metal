#include <metal_stdlib>
using namespace metal;

constant float4 positions[] =
{
    float4(-0.75, -0.75, 0.0, 1.0),
    float4(0.75, -0.75, 0.0, 1.0),
    float4(0.0, 0.75, 0.0, 1.0)
};

constant half3 colors[] =
{
    half3(1.0, 0.0, 0.0),
    half3(0.0, 1.0, 0.0),
    half3(0.0, 0.0, 1.0)
};

struct VertexPayload
{
    float4 position [[position]];
    half3 color;
};

VertexPayload vertex vertexMain(uint vertexID[[vertex_id]])
{
    VertexPayload payload;
    payload.position = positions[vertexID];
    payload.color = colors[vertexID];
    return payload;
}

half4 fragment fragmentMain(VertexPayload frag [[stage_in]])
{
    return half4(frag.color, 1.0);
}