#include "pch.h"
#include "GameEngineImpl.h"
#include "imgui.h"

namespace Graphics
{
	extern EnumVar DebugZoom;
}


ExpVar m_SunLightIntensity("Application/Sun Intensity", 4.0f, 0.0f, 16.0f, 0.25f);
NumVar m_SunOrientation("Application/Sun Orientation", -0.5f, -100.0f, 100.0f, 0.1f);
NumVar m_SunInclination("Application/Sun Inclination", 0.75f, 0.0f, 1.0f, 0.01f);
NumVar ShadowDimX("Application/Shadow Dim X", 5000, 1000, 10000, 100);
NumVar ShadowDimY("Application/Shadow Dim Y", 3000, 1000, 10000, 100);
NumVar ShadowDimZ("Application/Shadow Dim Z", 3000, 1000, 10000, 100);




void GameEngineImpl::Update(float deltaT) 
{
	ScopedTimer _prof(L"Update State");


	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(
		g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight());
	io.DeltaTime = deltaT;
	ImGui::NewFrame();

	



	if (GameInput::IsFirstPressed(GameInput::kLShoulder))
		DebugZoom.Decrement();
	else if (GameInput::IsFirstPressed(GameInput::kRShoulder))
		DebugZoom.Increment();

	

	m_pCameraController->Update(deltaT);

	

	m_ViewProjMatrix = m_Camera.GetViewProjMatrix();

	

	float costheta = cosf(m_SunOrientation);
	float sintheta = sinf(m_SunOrientation);
	float cosphi = cosf(m_SunInclination * 3.14159f * 0.5f);
	float sinphi = sinf(m_SunInclination * 3.14159f * 0.5f);
	m_SunDirection = Normalize(Vector3(costheta * cosphi, sinphi, sintheta * cosphi));

	// We use viewport offsets to jitter our color samples from frame to frame (with TAA.)
	// D3D has a design quirk with fractional offsets such that the implicit scissor
	// region of a viewport is floor(TopLeftXY) and floor(TopLeftXY + WidthHeight), so
	// having a negative fractional top left, e.g. (-0.25, -0.25) would also shift the
	// BottomRight corner up by a whole integer.  One solution is to pad your viewport
	// dimensions with an extra pixel.  My solution is to only use positive fractional offsets,
	// but that means that the average sample position is +0.5, which I use when I disable
	// temporal AA.
	if (TemporalAA::Enable)
	{
		uint64_t FrameIndex = Graphics::GetFrameCount();
#if 1
		// 2x super sampling with no feedback
		float SampleOffsets[2][2] =
		{
			{ 0.25f, 0.25f },
			{ 0.75f, 0.75f },
		};
		const float* Offset = SampleOffsets[FrameIndex & 1];
#else
		// 4x super sampling via controlled feedback
		float SampleOffsets[4][2] =
		{
			{ 0.125f, 0.625f },
			{ 0.375f, 0.125f },
			{ 0.875f, 0.375f },
			{ 0.625f, 0.875f }
		};
		const float* Offset = SampleOffsets[FrameIndex & 3];
#endif
		m_MainViewport.TopLeftX = Offset[0];
		m_MainViewport.TopLeftY = Offset[1];
	}
	else
	{
		m_MainViewport.TopLeftX = 0.5f;
		m_MainViewport.TopLeftY = 0.5f;
	}

	m_MainViewport.Width = (float)g_SceneColorBuffer.GetWidth();
	m_MainViewport.Height = (float)g_SceneColorBuffer.GetHeight();
	m_MainViewport.MinDepth = 0.0f;
	m_MainViewport.MaxDepth = 1.0f;

	m_MainScissor.left = 0;
	m_MainScissor.top = 0;
	m_MainScissor.right = (LONG)g_SceneColorBuffer.GetWidth();
	m_MainScissor.bottom = (LONG)g_SceneColorBuffer.GetHeight();

}


void GameEngineImpl::RenderObjects(GraphicsContext& gfxContext, const Matrix4& ViewProjMat)
{
	__declspec(align(64))  struct VSConstants
	{
		Matrix4 modelToProjection;
		Matrix4 modelToShadow;
		XMFLOAT3 viewerPos;
	} vsConstants;
	vsConstants.modelToProjection = ViewProjMat;
	vsConstants.modelToShadow = m_SunShadow.GetShadowMatrix();
	XMStoreFloat3(&vsConstants.viewerPos, m_Camera.GetPosition());

	gfxContext.SetDynamicConstantBufferView(0, sizeof(vsConstants), &vsConstants);

	uint32_t materialIdx = 0xFFFFFFFFul;

	uint32_t VertexStride = m_Model.m_VertexStride;

	for (unsigned int meshIndex = 0; meshIndex < m_Model.m_Header.meshCount; meshIndex++)
	{
		const Model::Mesh& mesh = m_Model.m_pMesh[meshIndex];

		uint32_t indexCount = mesh.indexCount;
		uint32_t startIndex = mesh.indexDataByteOffset / sizeof(uint16_t);
		uint32_t baseVertex = mesh.vertexDataByteOffset / VertexStride;

		if (mesh.materialIndex != materialIdx)
		{
			materialIdx = mesh.materialIndex;
			gfxContext.SetDynamicDescriptors(3, 0, 6, m_Model.GetSRVs(materialIdx));
		}

#if USE_VERTEX_BUFFER
		gfxContext.DrawIndexed(indexCount, startIndex, baseVertex);
#else
		gfxContext.SetConstants(5, baseVertex);
		gfxContext.DrawIndexed(indexCount, startIndex);
#endif
	}
}



void GameEngineImpl::RenderScene(void)
{
	GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");

	ParticleEffects::Update(gfxContext.GetComputeContext(), Graphics::GetFrameTime());

	__declspec(align(16)) struct
	{
		Vector3 sunDirection;
		Vector3 sunLight;
		Vector3 ambientLight;
		float ShadowTexelSize;
	} psConstants;

	psConstants.sunDirection = m_SunDirection;
	psConstants.sunLight = Vector3(1.0f, 1.0f, 1.0f) * m_SunLightIntensity;
	psConstants.ambientLight = Vector3(0.2f, 0.2f, 0.2f);
	psConstants.ShadowTexelSize = 1.0f / g_ShadowBuffer.GetWidth();

	
	{
		ScopedTimer _prof(L"Z PrePass", gfxContext);

		gfxContext.ClearDepth(g_SceneDepthBuffer);

		gfxContext.SetRootSignature(m_RootSig);
		gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		gfxContext.SetIndexBuffer(m_Model.m_IndexBuffer.IndexBufferView());
#if USE_VERTEX_BUFFER
		gfxContext.SetVertexBuffer(0, m_Model.m_VertexBuffer.VertexBufferView());
#elif USE_ROOT_BUFFER_SRV
		gfxContext.SetBufferSRV(2, m_Model.m_VertexBuffer);
#else
		gfxContext.SetDynamicDescriptor(2, 0, m_Model.m_VertexBuffer.GetSRV());
#endif
		gfxContext.SetDynamicConstantBufferView(1, sizeof(psConstants), &psConstants);

		gfxContext.SetPipelineState(m_DepthPSO);
		gfxContext.SetDepthStencilTarget(g_SceneDepthBuffer);
		gfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);
		RenderObjects(gfxContext, m_ViewProjMatrix);
	}

	SSAO::Render(gfxContext, m_Camera);
	
	if (!SSAO::DebugDraw)
	{
		ScopedTimer _prof(L"Main Render", gfxContext);

		gfxContext.ClearColor(g_SceneColorBuffer);

		// Set the default state for command lists
		auto& pfnSetupGraphicsState = [&](void)
		{
			gfxContext.SetRootSignature(m_RootSig);
			gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			gfxContext.SetIndexBuffer(m_Model.m_IndexBuffer.IndexBufferView());
#if USE_VERTEX_BUFFER
			gfxContext.SetVertexBuffer(0, m_Model.m_VertexBuffer.VertexBufferView());
#elif USE_ROOT_BUFFER_SRV
			gfxContext.SetBufferSRV(2, m_Model.m_VertexBuffer);
#else
			gfxContext.SetDynamicDescriptor(2, 0, m_Model.m_VertexBuffer.GetSRV());
#endif
			gfxContext.SetDynamicDescriptors(4, 0, 2, m_ExtraTextures);
			gfxContext.SetDynamicConstantBufferView(1, sizeof(psConstants), &psConstants);
		};

		pfnSetupGraphicsState();

		{
			ScopedTimer _prof(L"Render Shadow Map", gfxContext);

			m_SunShadow.UpdateMatrix(-m_SunDirection, Vector3(0, -500.0f, 0), Vector3(ShadowDimX, ShadowDimY, ShadowDimZ),
				(uint32_t)g_ShadowBuffer.GetWidth(), (uint32_t)g_ShadowBuffer.GetHeight(), 16);

			gfxContext.SetPipelineState(m_ShadowPSO);
			g_ShadowBuffer.BeginRendering(gfxContext);
			RenderObjects(gfxContext, m_SunShadow.GetViewProjMatrix());
			g_ShadowBuffer.EndRendering(gfxContext);
		}


		if (SSAO::AsyncCompute)
		{
			gfxContext.Flush();
			pfnSetupGraphicsState();

			// Make the 3D queue wait for the Compute queue to finish SSAO
			g_CommandManager.GetGraphicsQueue().StallForProducer(g_CommandManager.GetComputeQueue());
		}

		{
			ScopedTimer _prof(L"Render Color", gfxContext);
			gfxContext.SetPipelineState(m_ModelPSO);
			gfxContext.TransitionResource(g_SSAOFullScreen, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			gfxContext.SetRenderTarget(g_SceneColorBuffer, g_SceneDepthBuffer, true);
			gfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);
			RenderObjects(gfxContext, m_ViewProjMatrix);
		}
	}

	ParticleEffects::Render(gfxContext, m_Camera, g_SceneColorBuffer, g_SceneDepthBuffer, g_LinearDepth);

	MotionBlur::RenderCameraBlur(gfxContext, m_Camera);
	

	gfxContext.Finish();
}


void GameEngineImpl::RenderUI(class GraphicsContext& gfxContext)
{
	__declspec(align(16)) struct
	{
		Vector3 sunDirection;
		Vector3 sunLight;
		Vector3 ambientLight;
		float ShadowTexelSize;
	} psConstants;

	psConstants.sunDirection = m_SunDirection;
	psConstants.sunLight = Vector3(1.0f, 1.0f, 1.0f);
	psConstants.ambientLight = Vector3(0.2f, 0.2f, 0.2f);
	psConstants.ShadowTexelSize = 1.0f / g_ShadowBuffer.GetWidth();

	{

		gfxContext.SetRootSignature(m_ImguiSig);

		ScopedTimer _prof(L"Imgui", gfxContext);
		gfxContext.SetPipelineState(m_ImguiPSO);
		gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


		//gfxContext.SetVertexBuffer(0, imguiVertexBuffer.VertexBufferView());
		//gfxContext.SetIndexBuffer(imguiIndexBuffer.IndexBufferView());
		//gfxContext.SetDepthStencilTarget(g_SceneDepthBuffer);
		gfxContext.SetRenderTarget(g_OverlayBuffer);// , g_SceneDepthBuffer, true);


													//gfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);



			float translate = -0.5f * 2.f;
			const float L = 0.f;
			const float R = ImGui::GetIO().DisplaySize.x;
			const float B = ImGui::GetIO().DisplaySize.y;
			const float T = 0.f;
			__declspec(align(64))   const float mvp[4][4] =
			{
				{ 2.0f / (R - L),   0.0f,           0.0f,       0.0f },
				{ 0.0f,         2.0f / (T - B),     0.0f,       0.0f, },
				{ 0.0f,         0.0f,           0.5f,       0.0f },
				{ (R + L) / (L - R),  (T + B) / (B - T),    0.5f,       1.0f },
			};


		gfxContext.SetDynamicDescriptors(3, 0, 1, &m_imguiFontTexture.GetSRV());
		
		gfxContext.SetDynamicConstantBufferView(0, sizeof(mvp), &mvp);



		gfxContext.SetDynamicConstantBufferView(1, sizeof(psConstants), &psConstants);


		gfxContext.SetConstants(5, 0);

		//gfxContext.DrawIndexed(3, 0);

		bool show_test_window = true;


		ImGui::SetNextWindowSize(ImVec2(200, 100), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Another Window", &show_test_window);
		ImGui::Text("Hello");
		ImGui::End();


		ImGui::Render();

		auto drawData = ImGui::GetDrawData();



		if (drawData->CmdListsCount > 0)
		{
			//DynAlloc TempSpace = m_CpuLinearAllocator.Allocate(NumBytes, 512);
			//SIMDMemCopy(TempSpace.DataPtr, BufferData, Math::DivideByMultiple(NumBytes, 16));
			//CopyBufferRegion(Dest, DestOffset, TempSpace.Buffer, TempSpace.Offset, NumBytes);


			for (int n = 0; n < drawData->CmdListsCount; n++)
			{

			}

			for (int n = 0; n < drawData->CmdListsCount; n++)
			{
				const ImDrawList* cmd_list = drawData->CmdLists[n];

				size_t verticesCount = cmd_list->VtxBuffer.size();
				size_t indicesCount = cmd_list->IdxBuffer.size();
				size_t verticesSize = verticesCount * sizeof(ImDrawVert);
				size_t indicesSize = indicesCount * sizeof(ImDrawIdx);

				
				gfxContext.WriteBuffer(imguiVertexBuffer, 0, &cmd_list->VtxBuffer[0], verticesSize);
				gfxContext.WriteBuffer(imguiIndexBuffer, 0, &cmd_list->IdxBuffer[0], indicesSize);

				gfxContext.SetVertexBuffer(0, imguiVertexBuffer.VertexBufferView());
				gfxContext.SetIndexBuffer(imguiIndexBuffer.IndexBufferView());


				for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.size(); cmd_i++)
				{
					const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
					if (pcmd->UserCallback)
					{
						pcmd->UserCallback(cmd_list, pcmd);
					}
					else
					{
						const D3D12_RECT r = { (LONG)pcmd->ClipRect.x, (LONG)pcmd->ClipRect.y, (LONG)pcmd->ClipRect.z, (LONG)pcmd->ClipRect.w };
						gfxContext.SetScissor(r);

						gfxContext.DrawIndexedInstanced(pcmd->ElemCount, 1, 0, 0, 0);

					}
				}
			}
		}
	}

};


inline void* MemAllocFn(size_t sz)
{
	return _aligned_malloc(sz,16);
}
inline void MemFreeFn(void* ptr)
{
	_aligned_free(ptr);
}


void GameEngineImpl::Startup(void) 
{
	m_RootSig.Reset(6, 2);
	m_RootSig.InitStaticSampler(0, SamplerAnisoWrapDesc, D3D12_SHADER_VISIBILITY_PIXEL);
	m_RootSig.InitStaticSampler(1, SamplerShadowDesc, D3D12_SHADER_VISIBILITY_PIXEL);
	m_RootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
	m_RootSig[1].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_PIXEL);
#if USE_ROOT_BUFFER_SRV || USE_VERTEX_BUFFER
	m_RootSig[2].InitAsBufferSRV(0, D3D12_SHADER_VISIBILITY_VERTEX);
#else
	m_RootSig[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_VERTEX);
#endif
	m_RootSig[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 6, D3D12_SHADER_VISIBILITY_PIXEL);
	m_RootSig[4].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 64, 3, D3D12_SHADER_VISIBILITY_PIXEL);
	m_RootSig[5].InitAsConstants(1, 1, D3D12_SHADER_VISIBILITY_VERTEX);
#if USE_VERTEX_BUFFER
	m_RootSig.Finalize(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
#else
	m_RootSig.Finalize();
#endif


	m_ImguiSig.Reset(6, 2);
	m_ImguiSig.InitStaticSampler(0, SamplerAnisoWrapDesc, D3D12_SHADER_VISIBILITY_PIXEL);
	m_ImguiSig.InitStaticSampler(1, SamplerShadowDesc, D3D12_SHADER_VISIBILITY_PIXEL);
	m_ImguiSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
	m_ImguiSig[1].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_PIXEL);
	m_ImguiSig[2].InitAsBufferSRV(0, D3D12_SHADER_VISIBILITY_VERTEX);
	m_ImguiSig[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 6, D3D12_SHADER_VISIBILITY_PIXEL);
	m_ImguiSig[4].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 64, 3, D3D12_SHADER_VISIBILITY_PIXEL);
	m_ImguiSig[5].InitAsConstants(1, 1, D3D12_SHADER_VISIBILITY_VERTEX);
	m_ImguiSig.Finalize(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);


	DXGI_FORMAT ColorFormat = g_SceneColorBuffer.GetFormat();
	DXGI_FORMAT DepthFormat = g_SceneDepthBuffer.GetFormat();
	DXGI_FORMAT ShadowFormat = g_ShadowBuffer.GetFormat();

#if USE_VERTEX_BUFFER
	D3D12_INPUT_ELEMENT_DESC vertElem[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
#endif


	//-imgui

	D3D12_INPUT_ELEMENT_DESC vertElem[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	m_ImguiPSO.SetRootSignature(m_ImguiSig);
	m_ImguiPSO.SetRasterizerState(RasterizerTwoSided);
	m_ImguiPSO.SetBlendState(BlendPreMultiplied);
	m_ImguiPSO.SetDepthStencilState(DepthStateDisabled);
	m_ImguiPSO.SetSampleMask(0xFFFFFFFF);

	m_ImguiPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_ImguiPSO.SetInputLayout(_countof(vertElem), vertElem);


	m_ImguiPSO.SetRenderTargetFormat(g_OverlayBuffer.GetFormat(), DXGI_FORMAT_UNKNOWN);

	m_ImguiPSO.SetVertexShader(g_pImguiVS, sizeof(g_pImguiVS));
	m_ImguiPSO.SetPixelShader(g_pImguiPS, sizeof(g_pImguiPS));
	m_ImguiPSO.Finalize();

	struct float3
	{
		float x;
		float y;
		float z;
	};

	struct float2
	{
		float x;
		float y;
	};

	struct VSInput
	{
		float2 pos;               
		float2 uv;              
		unsigned int color;
	};

	VSInput *buf = (VSInput *)_aligned_malloc(sizeof(VSInput) * 3, 16);

	memset(buf, 0, sizeof(VSInput) * 3);

	buf[0].pos.x = g_OverlayBuffer.GetWidth()/2.0f;
	buf[0].pos.y = g_OverlayBuffer.GetHeight() / 2.0f;

	buf[0].uv.x = 2;
	buf[0].uv.y = 3;

	buf[0].color = 0x018003FF;

	buf[1].pos.x = g_OverlayBuffer.GetWidth() / 2.0f;
	buf[1].pos.y = 0;
	buf[1].color = 0x011003FF;

	buf[2].pos.x = 0;
	buf[2].pos.y = g_OverlayBuffer.GetHeight() / 2.0f;
	buf[2].color = 0xFF1003FF;


	imguiVertexBuffer.Create(L"imguiVertexBuffer", 30000, sizeof(VSInput), nullptr);




	_aligned_free(buf);

	__declspec(align(16)) uint16 idxs[3];
	idxs[0] = 0;
	idxs[1] = 1;
	idxs[2] = 2;

	imguiIndexBuffer.Create(L"imguiIndexBuffer", 30000, sizeof(uint16_t), nullptr);


	ImGuiIO& io = ImGui::GetIO();

	//io.RenderDrawListsFn = ImGui_ImplDX12_RenderDrawLists;

	io.MemAllocFn = MemAllocFn;
	io.MemFreeFn = MemFreeFn;

	unsigned char* pixels = 0;
	int width, height;

	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);



	m_imguiFontTexture.Create(width, height, DXGI_FORMAT_R8G8B8A8_UNORM, pixels);
	m_imguiFontTexture->SetName(L"Imgui font texture");




	//-



	m_DepthPSO.SetRootSignature(m_RootSig);
	m_DepthPSO.SetRasterizerState(RasterizerDefault);
	m_DepthPSO.SetBlendState(BlendNoColorWrite);
	m_DepthPSO.SetDepthStencilState(DepthStateReadWrite);
#if USE_VERTEX_BUFFER
	m_DepthPSO.SetInputLayout(_countof(vertElem), vertElem);
#endif
	m_DepthPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_DepthPSO.SetRenderTargetFormats(0, nullptr, DepthFormat);
	m_DepthPSO.SetVertexShader(g_pDepthViewerVS, sizeof(g_pDepthViewerVS));
	m_DepthPSO.SetPixelShader(g_pDepthViewerPS, sizeof(g_pDepthViewerPS));
	m_DepthPSO.Finalize();

	m_ShadowPSO = m_DepthPSO;
	m_ShadowPSO.SetRasterizerState(RasterizerShadow);
	m_ShadowPSO.SetRenderTargetFormats(0, nullptr, g_ShadowBuffer.GetFormat());
	m_ShadowPSO.Finalize();

	m_ModelPSO = m_DepthPSO;
	m_ModelPSO.SetBlendState(BlendDisable);
	m_ModelPSO.SetDepthStencilState(DepthStateTestEqual);
	m_ModelPSO.SetRenderTargetFormats(1, &ColorFormat, DepthFormat);
	m_ModelPSO.SetVertexShader(g_pModelViewerVS, sizeof(g_pModelViewerVS));
	m_ModelPSO.SetPixelShader(g_pModelViewerPS, sizeof(g_pModelViewerPS));
	m_ModelPSO.Finalize();

	m_ExtraTextures[0] = g_SSAOFullScreen.GetSRV();
	m_ExtraTextures[1] = g_ShadowBuffer.GetSRV();

	TextureManager::Initialize(L"Textures/");



	ASSERT(m_Model.Load("Models/sponza.h3d"), "Failed to load model");
	ASSERT(m_Model.m_Header.meshCount > 0, "Model contains no meshes");

	CreateParticleEffects();


	//movaps      xmmword ptr [rax-08h],xmm11
	//ASSERT(Math::IsAligned(a, 16));


	float modelRadius = Length(m_Model.m_Header.boundingBox.max - m_Model.m_Header.boundingBox.min) * .5f;
	const Vector3 eye = (m_Model.m_Header.boundingBox.min + m_Model.m_Header.boundingBox.max)
		* .4f + Vector3(modelRadius * .3f, 0.0f, 0.0f);

	m_Camera.SetEyeAtUp(eye, Vector3(kZero), Vector3(kYUnitVector));
	m_Camera.SetZRange(1.0f, 10000.0f);
	m_pCameraController = new CameraController(m_Camera, Vector3(kYUnitVector));

	MotionBlur::Enable = true;
	TemporalAA::Enable = true;
	FXAA::Enable = true;
	PostEffects::EnableHDR = true;
	PostEffects::EnableAdaptation = true;
	PostEffects::AdaptationRate = 0.05f;
	PostEffects::TargetLuminance = 0.4f;
	PostEffects::MinExposure = 1.0f;
	PostEffects::MaxExposure = 8.0f;
	PostEffects::BloomThreshold = 1.0f;
	PostEffects::BloomStrength = 0.10f;
};
