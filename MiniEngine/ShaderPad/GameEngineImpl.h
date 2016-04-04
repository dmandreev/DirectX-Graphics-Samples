#pragma once


using namespace GameCore;
using namespace Math;
using namespace Graphics;


#include "Model.h"

#include "CompiledShaders/DepthViewerVS.h"
#include "CompiledShaders/DepthViewerPS.h"
#include "CompiledShaders/ModelViewerVS.h"
#include "CompiledShaders/ModelViewerPS.h"

#include "CompiledShaders/ImguiPS.h"
#include "CompiledShaders/ImguiVS.h"




class GameEngineImpl : public GameCore::IGameApp
{
public:

	GameEngineImpl()
		: m_pCameraController(nullptr)
	{
		

	}



	virtual ComPtr<IUnknown> GetMainWindow(void) override
	{
		return ComPtr<IUnknown>(reinterpret_cast<IUnknown *>(Windows::UI::Core::CoreWindow::GetForCurrentThread()));
	}

	virtual void RenderUI(class GraphicsContext& gfxContext)
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

			gfxContext.SetRootSignature(m_RootSig);

			ScopedTimer _prof(L"Imgui", gfxContext);
			gfxContext.SetPipelineState(m_ImguiPSO);
			gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);



			gfxContext.SetDynamicDescriptor(2, 0, imguiVertexBuffer.GetSRV());
			gfxContext.SetIndexBuffer(imguiIndexBuffer.IndexBufferView());
			//gfxContext.SetDepthStencilTarget(g_SceneDepthBuffer);
			gfxContext.SetRenderTarget(g_OverlayBuffer);// , g_SceneDepthBuffer, true);


			gfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);


			__declspec(align(64))  struct VSConstants
			{
				Matrix4 modelToProjection;
				Matrix4 modelToShadow;
				XMFLOAT3 viewerPos;
			} vsConstants;
			vsConstants.modelToProjection = Matrix4(kIdentity);
			vsConstants.modelToShadow = m_SunShadow.GetShadowMatrix();
			XMStoreFloat3(&vsConstants.viewerPos, m_Camera.GetPosition());

			gfxContext.SetDynamicDescriptors(4, 0, 2, m_ExtraTextures);
			gfxContext.SetDynamicConstantBufferView(0, sizeof(vsConstants), &vsConstants);
			gfxContext.SetDynamicConstantBufferView(1, sizeof(psConstants), &psConstants);


			gfxContext.SetConstants(5, 0);

			gfxContext.DrawIndexed(3, 0);

			/*
			Context.SetDynamicDescriptor(0, 0, g_OverlayBuffer.GetSRV());

			Context.SetPipelineState(s_BlendUIPSO);
			Context.Draw(3);
			*/

		}

	};


	virtual void Startup(void) override
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


		//-

		m_ImguiPSO.SetRootSignature(m_RootSig);
		m_ImguiPSO.SetRasterizerState(RasterizerTwoSided);
		m_ImguiPSO.SetBlendState(BlendPreMultiplied);
		m_ImguiPSO.SetDepthStencilState(DepthStateDisabled);
		m_ImguiPSO.SetSampleMask(0xFFFFFFFF);

		m_ImguiPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

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
			float3 position;               // Offset:    0
			float2 texcoord0;              // Offset:   12
			float3 normal;                 // Offset:   20
			float3 tangent;                // Offset:   32
			float3 bitangent;              // Offset:   44
		};

		VSInput *buf = (VSInput *)_aligned_malloc(sizeof(VSInput) * 3, 16);

		memset(buf, 0, sizeof(VSInput) * 3);

		buf[0].position.x = 1;
		buf[0].position.y = 1;
		buf[0].position.z = 0.98;

		buf[1].position.x = 1;
		buf[1].position.y = 100;
		buf[1].position.z = 0.98;

		buf[2].position.x = 100;
		buf[2].position.y = 1;
		buf[2].position.z = 0.98;

		imguiVertexBuffer.Create(L"imguiVertexBuffer", 3, sizeof(VSInput), buf);
		_aligned_free(buf);

		__declspec(align(16)) uint16 idxs[3];
		idxs[0] = 0;
		idxs[1] = 1;
		idxs[2] = 2;

		imguiIndexBuffer.Create(L"imguiIndexBuffer", 3, sizeof(uint16_t), &idxs);

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

	virtual void Cleanup(void) override
	{
		m_Model.Clear();

		delete m_pCameraController;
		m_pCameraController = nullptr;

	};


	virtual void Update(float deltaT) override;
	virtual void RenderScene(void) override;

private:

	void RenderObjects(GraphicsContext& Context, const Matrix4& ViewProjMat);
	void CreateParticleEffects()
	{
		ParticleEffectProperties Effect = ParticleEffectProperties();
		Effect.MinStartColor = Effect.MaxStartColor = Effect.MinEndColor = Effect.MaxEndColor = Color(1.0f, 1.0f, 1.0f, 0.0f);
		Effect.TexturePath = L"sparkTex.dds";

		Effect.TotalActiveLifetime = FLT_MAX;
		Effect.Size = Vector4(4.0f, 8.0f, 4.0f, 8.0f);
		Effect.Velocity = Vector4(20.0f, 200.0f, 50.0f, 180.0f);
		Effect.LifeMinMax = XMFLOAT2(1.0f, 3.0f);
		Effect.MassMinMax = XMFLOAT2(4.5f, 15.0f);
		Effect.EmitProperties.Gravity = XMFLOAT3(0.0f, -100.0f, 0.0f);
		Effect.EmitProperties.FloorHeight = -0.5f;
		Effect.EmitProperties.EmitPosW = Effect.EmitProperties.LastEmitPosW = XMFLOAT3(-1200.0f, 185.0f, -445.0f);
		Effect.EmitProperties.MaxParticles = 800;
		Effect.EmitRate = 64.0f;
		Effect.Spread.x = 20.0f;
		Effect.Spread.y = 50.0f;
		ParticleEffects::InstantiateEffect(&Effect);

		ParticleEffectProperties Smoke = ParticleEffectProperties();
		Smoke.TexturePath = L"smoke.dds";

		Smoke.TotalActiveLifetime = FLT_MAX;;
		Smoke.EmitProperties.MaxParticles = 25;
		Smoke.EmitProperties.EmitPosW = Smoke.EmitProperties.LastEmitPosW = XMFLOAT3(1120.0f, 185.0f, -445.0f);
		Smoke.EmitRate = 64.0f;
		Smoke.LifeMinMax = XMFLOAT2(2.5f, 4.0f);
		Smoke.Size = Vector4(60.0f, 108.0f, 30.0f, 208.0f);
		Smoke.Velocity = Vector4(30.0f, 30.0f, 10.0f, 40.0f);
		Smoke.MassMinMax = XMFLOAT2(1.0, 3.5);
		Smoke.Spread.x = 60.0f;
		Smoke.Spread.y = 70.0f;
		Smoke.Spread.z = 20.0f;
		ParticleEffects::InstantiateEffect(&Smoke);

		ParticleEffectProperties Fire = ParticleEffectProperties();
		Fire.MinStartColor = Fire.MaxStartColor = Fire.MinEndColor = Fire.MaxEndColor = Color(1.0f, 1.0f, 1.0f, 0.0f);
		Fire.TexturePath = L"fire.dds";

		Fire.TotalActiveLifetime = FLT_MAX;
		Fire.Size = Vector4(54.0f, 68.0f, 0.1f, 0.3f);
		Fire.Velocity = Vector4(10.0f, 30.0f, 50.0f, 50.0f);
		Fire.LifeMinMax = XMFLOAT2(1.0f, 3.0f);
		Fire.MassMinMax = XMFLOAT2(10.5f, 14.0f);
		Fire.EmitProperties.Gravity = XMFLOAT3(0.0f, 1.0f, 0.0f);
		Fire.EmitProperties.EmitPosW = Fire.EmitProperties.LastEmitPosW = XMFLOAT3(1120.0f, 125.0f, 405.0f);
		Fire.EmitProperties.MaxParticles = 25;
		Fire.EmitRate = 64.0f;
		Fire.Spread.x = 1.0f;
		Fire.Spread.y = 60.0f;
		ParticleEffects::InstantiateEffect(&Fire);

	};
	Camera m_Camera;
	CameraController* m_pCameraController;
	Matrix4 m_ViewProjMatrix;
	D3D12_VIEWPORT m_MainViewport;
	D3D12_RECT m_MainScissor;

	RootSignature m_RootSig;
	GraphicsPSO m_DepthPSO;
	GraphicsPSO m_ModelPSO;
	GraphicsPSO m_ShadowPSO;
	GraphicsPSO m_ImguiPSO;

	D3D12_CPU_DESCRIPTOR_HANDLE m_ExtraTextures[2];

	Model m_Model;

	Vector3 m_SunDirection;
	ShadowCamera m_SunShadow;



	StructuredBuffer imguiVertexBuffer;
	ByteAddressBuffer imguiIndexBuffer;


};