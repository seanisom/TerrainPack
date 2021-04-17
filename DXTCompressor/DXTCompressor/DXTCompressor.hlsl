//--------------------------------------------------------------------------------------
// Copyright 2011 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
//
//--------------------------------------------------------------------------------------

Texture2D gTexture : register(t0);
Texture2D gCompressedTexture : register(t1);

SamplerState gSampler : register( s0 );

struct VS_INPUT
{
	float4 vPosition : POSITION;
	float2 vTexcoord : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 vPosition : SV_POSITION;
	float2 vTexcoord : TEXCOORD0;
};

// Pass-through vertex shader.
VS_OUTPUT PassThroughVS(VS_INPUT Input)
{
	VS_OUTPUT Output;

	Output.vPosition = Input.vPosition;
	Output.vTexcoord = Input.vTexcoord;

	return Output;
}

// Display pixel shader.
float4 RenderFramePS(VS_OUTPUT Input) : SV_TARGET
{
	float4 texel = gTexture.Sample(gSampler, Input.vTexcoord);

	return texel;
}

// Render to texture pixel shader.
float4 RenderTexturePS(VS_OUTPUT Input) : SV_TARGET
{
	float3 uncompressed = gTexture.Sample(gSampler, Input.vTexcoord);
	float3 compressed = gCompressedTexture.Sample(gSampler, Input.vTexcoord);
	float3 error = abs(compressed - uncompressed);

	return float4(error, 1.0f);
}
