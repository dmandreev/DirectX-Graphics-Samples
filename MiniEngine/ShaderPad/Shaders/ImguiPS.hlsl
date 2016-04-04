//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//

#include "imgui.hlsli"


cbuffer PSConstants : register(b0)
{
	float3 SunDirection;
	float3 SunColor;
	float3 AmbientColor;
	uint _pad;
	float ShadowTexelSize;
}


Texture2D<float4> texDiffuse : register(t0);
SamplerState sampler0 : register(s0);

[RootSignature(Imgui_RootSig)]
float4 main(VSOutput vsOutput) : SV_Target0
{
	float4 tex = texDiffuse.Sample(sampler0, vsOutput.uv);
	return vsOutput.color; //* tex;
}

