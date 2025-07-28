// Simulation.fx

cbuffer ConstantBufferCamera : register(b0)
{
    matrix View;
    matrix Projection;
    float4 eyePos;
}

// --- Per-Object Constant Buffer (for non-instanced objects) ---
cbuffer ConstantBuffer : register(b1)
{
    matrix World;
    float4 lightColour;
    float4 darkColour;
    float2 checkerSize;
    float2 padding;
}

// --- Shader I/O Structures ---
struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float3 Normal : NORMAL;
    //float4 Color : COLOR0;
    float3 WorldPos : TEXCOORD0;
    float4 Light : COLOR1;
    float4 Dark : COLOR2;
};

// =================================================================
// SHADER 1: For Fixed, Non-Instanced Objects (The Old Way)
// =================================================================
VS_OUTPUT VS(float3 Pos : POSITION, float3 Normal : NORMAL) //, float4 Color : COLOR)
{
    VS_OUTPUT output = (VS_OUTPUT) 0;

    // World matrix comes from the constant buffer (b1)
    float4 worldPos = mul(float4(Pos, 1.0f), World);
    output.Pos = mul(worldPos, View);
    output.Pos = mul(output.Pos, Projection);

    output.WorldPos = Pos; // Using local position for checker pattern
    output.Normal = mul(Normal, (float3x3) World);
    //output.Color = Color;

    output.Light = lightColour;
    output.Dark = darkColour;

    return output;
}

// =================================================================
// SHADER 2: For Moving, Instanced Objects (The New Way)
// =================================================================
struct VS_Instanced_Input
{
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    //float4 Color : COLOR;

    // World matrix from second buffer
    row_major float4x4 InstanceWorld : INSTANCE_TRANSFORM;

    // Colours from second buffer
    float4 Light : INSTANCE_LIGHTCOLOUR;
    float4 Dark : INSTANCE_DARKCOLOUR;
};

VS_OUTPUT VS_Instanced(VS_Instanced_Input input)
{
    VS_OUTPUT output = (VS_OUTPUT) 0;

    float4 worldPos = mul(float4(input.Pos, 1.0f), input.InstanceWorld);
    output.Pos = mul(worldPos, View);
    output.Pos = mul(output.Pos, Projection);

    output.WorldPos = input.Pos; // still local for checker
    output.Normal = mul(input.Normal, (float3x3) input.InstanceWorld);
   // output.Color = input.Color;

    output.Light = input.Light;
    output.Dark = input.Dark;

    return output;
}

// --- Pixel Shader (now uses VS-provided colours) ---
float4 PS(VS_OUTPUT input) : SV_Target
{
    float size = checkerSize[0];
    float2 uv = input.WorldPos.xy / size + size * 5;
    float patternMask = fmod(floor(uv.x) + fmod(floor(uv.y), 2.0), 2.0);
    return lerp(input.Light, input.Dark, patternMask);
}
