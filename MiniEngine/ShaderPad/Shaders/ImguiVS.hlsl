//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//


#include "imgui.hlsli"

cbuffer VSConstants : register(b0)
{
	float4x4 modelToProjection;
	float4x4 modelToShadow;
	float3 ViewerPos;
};


struct VSInput
{
	float2 pos: POSITION;
	float2 uv: TEXCOORD;
	float4 color: COLOR;
};


[RootSignature(Imgui_RootSig)]
VSOutput main(VSInput vsInput)
{
	VSOutput vsOutput;

	vsOutput.pos = mul(modelToProjection, float4(vsInput.pos.xy,1, 475));
	//vsOutput.position = float4(vsInput.position, 1.0);
	vsOutput.uv = vsInput.uv;

	return vsOutput;
}
