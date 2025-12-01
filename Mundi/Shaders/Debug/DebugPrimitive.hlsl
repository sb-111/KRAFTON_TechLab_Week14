//================================================================================================
// Filename:      DebugPrimitive.hlsl
// Description:   Shader for rendering debug primitives (Sphere, Box, Capsule)
//                - Translucent rendering with alpha blending
//                - Simple lighting for depth perception
//                - Used for Physics Body visualization in Physics Asset Editor
//================================================================================================

// --- Constant Buffers ---

// b0: ModelBuffer (VS) - World transform
cbuffer ModelBuffer : register(b0)
{
    row_major float4x4 WorldMatrix;
    row_major float4x4 WorldInverseTranspose; // Used for correct normal transformation
}

// b1: ViewProjBuffer (VS) - Camera matrices
cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
}

// b3: ColorBuffer (PS) - Color and UUID for object picking
cbuffer ColorBuffer : register(b3)
{
    float4 DebugColor;  // RGBA color (alpha for transparency)
    uint UUID;          // Object ID for picking
}

// --- Input/Output Structures ---

struct VS_INPUT
{
    float3 position : POSITION;     // Vertex position
    float3 normal : NORMAL;         // Vertex normal
    float2 uv : TEXCOORD;           // UV coordinates (not used)
    float4 tangent : TANGENT;       // Tangent (not used)
    float4 color : COLOR;           // Vertex color (not used)
};

struct PS_INPUT
{
    float4 position : SV_POSITION;  // Clip-space position
    float3 worldNormal : TEXCOORD0; // World-space normal for Lighting
    float3 worldPos : TEXCOORD1;    // World-space position for Lighting
};

struct PS_OUTPUT
{
    float4 Color : SV_Target0;      // Final color output
    uint UUID : SV_Target1;         // Object ID for picking
};

//================================================================================================
// Vertex Shader
//================================================================================================
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    // Calculate world position
    float4 worldPos = mul(float4(input.position, 1.0f), WorldMatrix);
    output.worldPos = worldPos.xyz;

    // Transform vertex position: Model -> World -> View -> Clip space
    float4x4 MVP = mul(mul(WorldMatrix, ViewMatrix), ProjectionMatrix);
    output.position = mul(float4(input.position, 1.0f), MVP);

    // Transform normal from model space to world space for lighting
    output.worldNormal = normalize(mul(input.normal, (float3x3) WorldInverseTranspose));

    return output;
}

//================================================================================================
// Pixel Shader
//================================================================================================
PS_OUTPUT mainPS(PS_INPUT input)
{
    PS_OUTPUT Output;

    // Face normal calculation for flat shading
    float3 ddx_pos = ddx(input.worldPos);
    float3 ddy_pos = ddy(input.worldPos);
    float3 faceNormal = normalize(cross(ddy_pos, ddx_pos));
    float3 N = faceNormal;

    // View-based lighting calculation (simple diffuse)
    float3 cameraPos = InverseViewMatrix[3].xyz;
    float3 viewDir = normalize(cameraPos - input.worldPos);
    float NdotL = abs(dot(N, -viewDir));  // lightDir = -viewDir

    // Define ambient and diffuse lighting
    float ambient = 0.3f;           // Minimum brightness
    float diffuseIntensity = 0.7f;  // Additional brightness from lighting
    float diffuse = ambient + diffuseIntensity * NdotL;

    // Final color is debug color modulated by lighting, preserving alpha
    Output.Color = float4(DebugColor.rgb * diffuse, DebugColor.a);
    Output.UUID = UUID;

    return Output;
}
