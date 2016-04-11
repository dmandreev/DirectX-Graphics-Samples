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

	void SetDpi(float dpi)
	{
		m_dpi = dpi;
	}


	void PointerPressed()
	{
		pointerPressed = true;
	}

	void PointerReleased()
	{
		pointerPressed = false;
	}

	virtual ComPtr<IUnknown> GetMainWindow(void) override
	{
		return ComPtr<IUnknown>(reinterpret_cast<IUnknown *>(Windows::UI::Core::CoreWindow::GetForCurrentThread()));
	}

	virtual void GameEngineImpl::RenderUI(class GraphicsContext& gfxContext) override;




	virtual void Cleanup(void) override
	{
		m_Model.Clear();

		delete m_pCameraController;
		m_pCameraController = nullptr;

	};


	virtual void Update(float deltaT) override;
	virtual void RenderScene(void) override;
	virtual void Startup(void) override;

private:

	float m_dpi = 96.0f;
	bool pointerPressed = false;

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

	RootSignature m_ImguiSig;

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


	Texture m_imguiFontTexture;

	char shader_text[1024*1024] = "";

	ComPtr<ID3DBlob> dynamic_shader_ps_blob;
	ComPtr<ID3DBlob> dynamic_shader_ps_blob_old;

	std::atomic<HRESULT> last_compile_result = S_FALSE;

	std::string last_comile_error;

	static const size_t fontSize = 18;

	float total_time = 0;

};