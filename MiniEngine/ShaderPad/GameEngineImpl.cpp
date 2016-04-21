#include "pch.h"
#include "GameEngineImpl.h"


#include <codecvt>
#include <locale>
#include "imconfig.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <ppltasks.h>
#include <d3dcompiler.h>


using namespace Concurrency;
using namespace std;

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


struct float2
{
	float x;
	float y;
};
struct float3
{
	float x;
	float y;
	float z;
};


struct VSInput
{
	float3 position;
	float2 texcoord0;
	float3 normal;
	float3 tangent;
	float3 bitangent;
};





class 
	CIncludeHandler : public ID3DInclude
{

public:
	CIncludeHandler()
	{

	}

	IFACEMETHODIMP Open(THIS_ D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes) override
	{

		std::string fname(pFileName);

		static char shader_text[1024 * 1024];

		std::string fullname = "Shaders/" + fname;


		FILE *file = nullptr;
		if (0 != fopen_s(&file, fullname.c_str(), "rb"))
		{
			//error
		}
		else
		{
			fseek(file, 0L, SEEK_END);
			auto sz = ftell(file);


			memset(shader_text, 0, ARRAYSIZE(shader_text));
			assert(ARRAYSIZE(shader_text) > sz);

			fseek(file, 0L, SEEK_SET);
			auto readed = fread(shader_text, sz, 1, file);

			auto err = ferror(file);


			assert(sz == readed*sz);

			include_text = std::string(shader_text);

			*ppData = include_text.c_str();
			*pBytes = include_text.length();


			fclose(file);
			return S_OK;
		}

		return E_NOTIMPL;
	}

	IFACEMETHODIMP Close(THIS_ LPCVOID pData) override
	{
		return S_OK;
	}

	std::string include_text;
};

void FindReplace(string& line, string& oldString, string& newString) {
	const size_t oldSize = oldString.length();

	// do nothing if line is shorter than the string to find
	if (oldSize > line.length()) return;

	const size_t newSize = newString.length();
	for (size_t pos = 0; ; pos += newSize) {
		// Locate the substring to replace
		pos = line.find(oldString, pos);
		if (pos == string::npos) return;
		if (oldSize == newSize) {
			// if they're same size, use std::string::replace
			line.replace(pos, oldSize, newString);
		}
		else {
			// if not same size, replace by erasing and inserting
			line.erase(pos, oldSize);
			line.insert(pos, newString);
		}
	}
}

void GameEngineImpl::Update(float deltaT)
{
	ScopedTimer _prof(L"Update State");
	total_time = total_time + deltaT;
	if (dynamic_shader_ps_blob != dynamic_shader_ps_blob_old)
	{

		if (dynamic_shader_ps_blob != nullptr)
		{
			m_ModelPSO.SetPixelShader(dynamic_shader_ps_blob->GetBufferPointer(), dynamic_shader_ps_blob->GetBufferSize());
			m_ModelPSO.Finalize();
		}
		dynamic_shader_ps_blob_old = dynamic_shader_ps_blob;

	}

	auto window = Windows::UI::Core::CoreWindow::GetForCurrentThread();



	auto mousePosX = floorf((window->PointerPosition.X - window->Bounds.X) * m_dpi / 96.0f);
	auto mousePosY = floorf((window->PointerPosition.Y - window->Bounds.Y)* m_dpi / 96.0f);


	ImGuiIO& io = ImGui::GetIO();


	io.DisplaySize = ImVec2(g_OverlayBuffer.GetWidth(), g_OverlayBuffer.GetHeight());
	io.DeltaTime = deltaT;
	io.MousePos = ImVec2(mousePosX, mousePosY);




	io.MouseDown[0] = pointerPressed;



	static bool init = true;
	static DWORD kbd_arr[256];

	if (init)
	{
		for (int i = 0; i < 255; i++)
		{
			DWORD new_keystate = (DWORD)window->GetKeyState(Windows::System::VirtualKey(i));
			kbd_arr[i] = new_keystate;
		}

		init = false;
	}



	//TODO:has focus test???
	for (size_t i = 0; i< 256; i++)
	{
		DWORD new_keystate = (DWORD)window->GetKeyState(Windows::System::VirtualKey(i));


		if (new_keystate != kbd_arr[i])
		{
			if (new_keystate & 1U)
			{
				io.KeysDown[i] = true;
			}
			else
			{
				io.KeysDown[i] = false;
			}
		}


		kbd_arr[i] = new_keystate;
	};


	io.KeyCtrl = io.KeysDown[VK_CONTROL];
	io.KeyShift = io.KeysDown[VK_SHIFT];
	io.KeyAlt = io.KeysDown[VK_MENU];








	ImGui::NewFrame();

	static bool showUI = true;

	static bool escapehitlock = true;
	static bool editorHasFocus = false;

	//messy, but works
	if (io.KeysDown[VK_ESCAPE])
	{
		if (escapehitlock)
		{
			showUI = !showUI;
			if (showUI)
				editorHasFocus = false;
			escapehitlock = false;
		}
	}


	//GImGui->Style.WindowPadding.x;
	//GImGui->Style.WindowPadding.y;

	
	//ImGuiStyle::ImGuiStyle::WindowPadding = ImVec2(0, 0);

	if (!escapehitlock)
	{
		escapehitlock = io.KeysDown[VK_ESCAPE] == false;
	}

	if (showUI)
	{
		//ImGui::ShowTestWindow();
	}
	



	//static bool show_test_window = true;

	static bool showEditor = true;


	if (showUI)
	{
		auto style = ImGui::GetStyle();

		auto sz = (fontSize * 24) + style.WindowPadding.y * 2;

		ImGui::SetNextWindowSize(ImVec2(fontSize * 60, sz), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiSetCond_FirstUseEver);



		//ImGuiWindowFlags_HorizontalScrollbar = 1 << 11,  // Allow horizontal scrollbar to appear (off by default). 
			//You need to use SetNextWindowContentSize(ImVec2(width,0.0f)); prior to calling Begin() to specify width. 
			//Read code in imgui_demo in the "Horizontal Scrolling" section.



		ImGui::Begin("Editor Window", &showEditor, ImGuiWindowFlags_NoTitleBar
			);





		std::string shadertexttemp(shader_text);



		if (!editorHasFocus)
		{
			ImGui::SetKeyboardFocusHere();
			editorHasFocus = true;
		}

		ImGui::InputTextMultiline("##source", shader_text, ARRAYSIZE(shader_text), ImVec2(-1, -1), ImGuiInputTextFlags_AllowTabInput);

		auto cmp = memcmp(shadertexttemp.c_str(), shader_text, shadertexttemp.length() + 1);
		static float duration = 0;

		static std::atomic_bool running = false;

		

		
		if (running == false)
		{
			if (cmp != 0)
			{
				running = true;
				std::string txt(shader_text);
				duration = 0;
				auto t = create_task([this, txt]()-> void
				{

					ComPtr<ID3DBlob> errors;

					D3D_SHADER_MACRO Shader_Macros[2] = { { "SOMEMACRO", "1" },
					{ nullptr,nullptr }
					};

					CIncludeHandler include_handler;

					ComPtr<ID3DBlob> blob;

					HRESULT hr = S_FALSE;

						hr = D3DCompile(txt.data(), txt.length(), "ps.hlsl", Shader_Macros, &include_handler, "main", "ps_5_1", //"ps_5_0" "ps_3_0", 
				
#ifdef _DEBUG
							D3DCOMPILE_DEBUG
							//D3DCOMPILE_OPTIMIZATION_LEVEL2 | D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_SKIP_OPTIMIZATION
#else
																																	 //D3DCOMPILE_OPTIMIZATION_LEVEL3
							D3DCOMPILE_OPTIMIZATION_LEVEL2 | D3DCOMPILE_SKIP_VALIDATION //| D3DCOMPILE_SKIP_OPTIMIZATION
#endif
							, 0, &blob, &errors);




					if (hr == S_OK)
					{
						dynamic_shader_ps_blob = blob;
						last_compile_result = S_OK;
						last_comile_error = "";
					}
					else
					{
						char *ptr = nullptr;
						if (errors != nullptr)
						{
							size_t arr_err_size = errors->GetBufferSize();




							ptr = new char[arr_err_size + 1];

							char *msg = (char *)errors->GetBufferPointer();
							memcpy(ptr, msg, arr_err_size);
							//TODO:redundant?
							ptr[arr_err_size] = 0;

						}


						auto err_str = std::string(ptr?ptr:"fatal error");

						int idx_of_last_slash = err_str.find("\\ps.hlsl");

						auto sub = err_str.substr(0, idx_of_last_slash);

						auto final_msg = err_str;

						FindReplace(final_msg, sub, string(""));


						last_comile_error = std::string(final_msg);

						delete ptr;

						last_compile_result = S_FALSE;
					}

					running = false;
					});
				}
			}
		else
		{
			duration = duration + io.DeltaTime;

		}


		ImGui::SetNextWindowPos(ImVec2(100, fontSize * 24+100+24),ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(fontSize *60, 200), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Compilation results");
		//ImGui::Text("duration %f", duration);
		ImGui::Text("%s", last_comile_error.c_str());
		
		ImGui::End();


		ImGui::End();
	}


	



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
/*
	struct VSInput
	{
		float3 position;
		float2 texcoord0;
		float3 normal;
		float3 tangent;
		float3 bitangent;
	};
*/

	Vector3 a(0,0,0);
	Vector3 b(100,0,0);
	Vector3 c(0,100,0);

	Vector3 normal(0, 0, 1);
	Vector3 tangent(1, 0, 0);
	Vector3 bitangent(0, 1, 0);

	auto rotation = OrthogonalTransform::MakeYRotation(IM_PI/4*3);

	auto rot1 = OrthogonalTransform::MakeXRotation(IM_PI / 4 * total_time);
	rotation = rotation*rot1;

	a = rotation*a;
	b = rotation*b;
	c = rotation*c;
	normal = rotation*normal;
	tangent = rotation*tangent;
	bitangent = rotation*bitangent;

	auto translation=OrthogonalTransform::MakeTranslation(Vector3(350, 200, 150));
	
	a = translation*a;
	b = translation*b;
	c = translation*c;

	



	auto c1 = cos(total_time);
	auto c2 = cos(total_time+1);
	auto c3 = cos(total_time+2);
	
	__declspec(align(16))
	  VSInput input[3] = {
		{
			{ a.GetX(),a.GetY(),a.GetZ() }, // position
			{0,1},   //texcoord0
			{ normal.GetX(),normal.GetY(),normal.GetZ()}, // normal
			{ tangent.GetX(),tangent.GetY(),tangent.GetZ()}, // tangent
			{ bitangent.GetX(),bitangent.GetY(),bitangent.GetZ() }, // bitangent
		},
		{
			{ b.GetX() ,b.GetY(),b.GetZ() }, // position
			{ 1,1 }, //texcoord0
			{ normal.GetX(),normal.GetY(),normal.GetZ() }, // normal
			{ tangent.GetX(),tangent.GetY(),tangent.GetZ() }, // tangent
			{ bitangent.GetX(),bitangent.GetY(),bitangent.GetZ() }, // bitangent
		}
		,
		{
			{ c.GetX(),c.GetY(),c.GetZ() }, // position
			{ 0,0 }, //texcoord0
			{ normal.GetX(),normal.GetY(),normal.GetZ() }, // normal
			{ tangent.GetX(),tangent.GetY(),tangent.GetZ() }, // tangent
			{ bitangent.GetX(),bitangent.GetY(),bitangent.GetZ() }, // bitangent
		}
	};

	__declspec(align(16)) unsigned short idxbuf[3] = { 0,1,2 };

	gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gfxContext.WriteBuffer(manualVertexBuffer, 0, &input, _ARRAYSIZE(input)*sizeof(VSInput));
	gfxContext.WriteBuffer(manualIndexBuffer, 0, &idxbuf, _ARRAYSIZE(idxbuf)*sizeof(unsigned short));



	gfxContext.SetDynamicDescriptor(2, 0, manualVertexBuffer.GetSRV());
	gfxContext.SetIndexBuffer(manualIndexBuffer.IndexBufferView());



	
	m_dummyTextures[0] = grid_texture->GetSRV();
	m_dummyTextures[1] = default_specular_texture->GetSRV();
	m_dummyTextures[2]= grid_texture->GetSRV();
	m_dummyTextures[3] = default_normal_texture->GetSRV();
	m_dummyTextures[4]= grid_texture->GetSRV();
	m_dummyTextures[5]= grid_texture->GetSRV();

	gfxContext.SetDynamicDescriptors(3, 0, 6, m_dummyTextures);


	gfxContext.SetConstants(5, 0);
	gfxContext.DrawIndexed(3, 0);

	gfxContext.SetIndexBuffer(m_Model.m_IndexBuffer.IndexBufferView());
	gfxContext.SetDynamicDescriptor(2, 0, m_Model.m_VertexBuffer.GetSRV());

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
		float time;
	} psConstants;

	psConstants.sunDirection = m_SunDirection;
	psConstants.sunLight = Vector3(1.0f, 1.0f, 1.0f) * m_SunLightIntensity;
	psConstants.ambientLight = Vector3(0.2f, 0.2f, 0.2f);
	psConstants.ShadowTexelSize = 1.0f / g_ShadowBuffer.GetWidth();
	psConstants.time = total_time;
	
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




void ShowMetricsWindow(bool* opened)
{

	if (ImGui::Begin("ImGui Metrics", opened))
	{
		ImGui::Text("ImGui %s", ImGui::GetVersion());
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::Text("%d vertices, %d indices (%d triangles)", ImGui::GetIO().MetricsRenderVertices, ImGui::GetIO().MetricsRenderIndices, ImGui::GetIO().MetricsRenderIndices / 3);
		ImGui::Text("%d allocations", ImGui::GetIO().MetricsAllocs);
		static bool show_clip_rects = true;
		ImGui::Checkbox("Show clipping rectangles when hovering a ImDrawCmd", &show_clip_rects);
		ImGui::Separator();

		struct Funcs
		{
			static void NodeDrawList(ImDrawList* draw_list, const char* label)
			{
				bool node_opened = ImGui::TreeNode(draw_list, "%s: '%s' %d vtx, %d indices, %d cmds", label, draw_list->_OwnerName ? draw_list->_OwnerName : "", draw_list->VtxBuffer.Size, draw_list->IdxBuffer.Size, draw_list->CmdBuffer.Size);
				if (draw_list == ImGui::GetWindowDrawList())
				{
					ImGui::SameLine();
					ImGui::TextColored(ImColor(255, 100, 100), "CURRENTLY APPENDING"); // Can't display stats for active draw list! (we don't have the data double-buffered)
				}
				if (!node_opened)
					return;

				int elem_offset = 0;
				for (const ImDrawCmd* pcmd = draw_list->CmdBuffer.begin(); pcmd < draw_list->CmdBuffer.end(); elem_offset += pcmd->ElemCount, pcmd++)
				{
					if (pcmd->UserCallback)
					{
						ImGui::BulletText("Callback %p, user_data %p", pcmd->UserCallback, pcmd->UserCallbackData);
						continue;
					}
					ImGui::BulletText("Draw %-4d %s vtx, tex = %p, clip_rect = (%.0f,%.0f)..(%.0f,%.0f)", pcmd->ElemCount, draw_list->IdxBuffer.Size > 0 ? "indexed" : "non-indexed", pcmd->TextureId, pcmd->ClipRect.x, pcmd->ClipRect.y, pcmd->ClipRect.z, pcmd->ClipRect.w);
					if (show_clip_rects && ImGui::IsItemHovered())
					{
						ImRect clip_rect = pcmd->ClipRect;
						ImRect vtxs_rect;
						ImDrawIdx* idx_buffer = (draw_list->IdxBuffer.Size > 0) ? draw_list->IdxBuffer.Data : NULL;
						for (int i = elem_offset; i < elem_offset + (int)pcmd->ElemCount; i++)
							vtxs_rect.Add(draw_list->VtxBuffer[idx_buffer ? idx_buffer[i] : i].pos);
						GImGui->OverlayDrawList.PushClipRectFullScreen();
						clip_rect.Round(); GImGui->OverlayDrawList.AddRect(clip_rect.Min, clip_rect.Max, ImColor(255, 255, 0));
						vtxs_rect.Round(); GImGui->OverlayDrawList.AddRect(vtxs_rect.Min, vtxs_rect.Max, ImColor(255, 0, 255));
						GImGui->OverlayDrawList.PopClipRect();
					}
				}
				ImGui::TreePop();
			}

			static void NodeWindows(ImVector<ImGuiWindow*>& windows, const char* label)
			{
				if (!ImGui::TreeNode(label, "%s (%d)", label, windows.Size))
					return;
				for (int i = 0; i < windows.Size; i++)
					Funcs::NodeWindow(windows[i], "Window");
				ImGui::TreePop();
			}

			static void NodeWindow(ImGuiWindow* window, const char* label)
			{
				if (!ImGui::TreeNode(window, "%s '%s', %d @ 0x%p", label, window->Name, window->Active || window->WasActive, window))
					return;
				NodeDrawList(window->DrawList, "DrawList");
				if (window->RootWindow != window) NodeWindow(window->RootWindow, "RootWindow");
				if (window->DC.ChildWindows.Size > 0) NodeWindows(window->DC.ChildWindows, "ChildWindows");
				ImGui::BulletText("Storage: %d bytes", window->StateStorage.Data.Size * (int)sizeof(ImGuiStorage::Pair));
				ImGui::TreePop();
			}
		};

		ImGuiState& g = *GImGui;                // Access private state
		Funcs::NodeWindows(g.Windows, "Windows");
		if (ImGui::TreeNode("DrawList", "Active DrawLists (%d)", g.RenderDrawLists[0].Size))
		{
			for (int i = 0; i < g.RenderDrawLists[0].Size; i++)
				Funcs::NodeDrawList(g.RenderDrawLists[0][i], "DrawList");
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Popups", "Opened Popups Stack (%d)", g.OpenedPopupStack.Size))
		{
			for (int i = 0; i < g.OpenedPopupStack.Size; i++)
			{
				ImGuiWindow* window = g.OpenedPopupStack[i].Window;
				ImGui::BulletText("PopupID: %08x, Window: '%s'%s%s", g.OpenedPopupStack[i].PopupID, window ? window->Name : "NULL", window && (window->Flags & ImGuiWindowFlags_ChildWindow) ? " ChildWindow" : "", window && (window->Flags & ImGuiWindowFlags_ChildMenu) ? " ChildMenu" : "");
			}
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Basic state"))
		{
			ImGui::Text("FocusedWindow: '%s'", g.FocusedWindow ? g.FocusedWindow->Name : "NULL");
			ImGui::Text("HoveredWindow: '%s'", g.HoveredWindow ? g.HoveredWindow->Name : "NULL");
			ImGui::Text("HoveredRootWindow: '%s'", g.HoveredRootWindow ? g.HoveredRootWindow->Name : "NULL");
			ImGui::Text("HoveredID: 0x%08X/0x%08X", g.HoveredId, g.HoveredIdPreviousFrame); // Data is "in-flight" so depending on when the Metrics window is called we may see current frame information or not
			ImGui::Text("ActiveID: 0x%08X/0x%08X", g.ActiveId, g.ActiveIdPreviousFrame);
			ImGui::TreePop();
		}
	}
	ImGui::End(); 
}


void GameEngineImpl::RenderUI(class GraphicsContext& gfxContext)
{


	//static bool showMetricsWindow;
	//ShowMetricsWindow(&showMetricsWindow);

	//implementation
	__declspec(align(16)) struct
	{
		Vector3 sunDirection;
		Vector3 sunLight;
		Vector3 ambientLight;
		float ShadowTexelSize;
		float time;
	} psConstants;

	psConstants.sunDirection = m_SunDirection;
	psConstants.sunLight = Vector3(1.0f, 1.0f, 1.0f);
	psConstants.ambientLight = Vector3(0.2f, 0.2f, 0.2f);
	psConstants.ShadowTexelSize = 1.0f / g_ShadowBuffer.GetWidth();
	psConstants.time = total_time;
	{

		gfxContext.SetRootSignature(m_ImguiSig);

		ScopedTimer _prof(L"Imgui", gfxContext);
		gfxContext.SetPipelineState(m_ImguiPSO);
		gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


		//gfxContext.SetVertexBuffer(0, imguiVertexBuffer.VertexBufferView());
		//gfxContext.SetIndexBuffer(imguiIndexBuffer.IndexBufferView());
		//gfxContext.SetDepthStencilTarget(g_SceneDepthBuffer);
		gfxContext.SetRenderTarget(g_OverlayBuffer);// , g_SceneDepthBuffer, true);



		//gfxContext.SetScissor(m_MainScissor);



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


		//style.WindowFillAlphaDefault = .3;
		//ImGui::ShowStyleEditor(&style);

	
		

		ImGui::Render();

		auto drawData = ImGui::GetDrawData();



		if (drawData->CmdListsCount > 0)
		{
			for (int n = 0; n < drawData->CmdListsCount; n++)
			{

			}

			for (int n = 0; n < drawData->CmdListsCount; n++)
			{
				int vtx_offset = 0;
				int idx_offset = 0;

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
						const D3D12_RECT r = { (LONG)pcmd->ClipRect.x, 
							(LONG)pcmd->ClipRect.y, 
							(LONG)pcmd->ClipRect.z, 
							(LONG)pcmd->ClipRect.w };
						gfxContext.SetScissor(r);

						gfxContext.DrawIndexedInstanced(pcmd->ElemCount,1,idx_offset,vtx_offset,0);
					}
					idx_offset += pcmd->ElemCount;
				}
				vtx_offset += verticesCount;
			}
		}
	}



	ImGuiIO& io = ImGui::GetIO();



	auto window = Windows::UI::Core::CoreWindow::GetForCurrentThread();

	
	/*
	for (size_t i = 0; i< 256; i++)
	{
		if (io.KeysDown[i])
		{
			auto keystate=window->GetKeyState(Windows::System::VirtualKey(i));

			if (keystate != Windows::UI::Core::CoreVirtualKeyStates::Down)
			{
				io.KeysDown[i] = false;
			}
		}
	};
	*/



	io.KeyCtrl = io.KeysDown[VK_CONTROL];
	io.KeyShift = io.KeysDown[VK_SHIFT];
	io.KeyAlt = io.KeysDown[VK_MENU];



	gfxContext.SetViewportAndScissor(0, 0, g_OverlayBuffer.GetWidth(), g_OverlayBuffer.GetHeight());

};




Platform::String ^ WaitForAsync(Windows::Foundation::IAsyncOperation<Platform::String ^> ^A)
{
	while (A->Status == Windows::Foundation::AsyncStatus::Started)
	{
		Windows::UI::Core::CoreWindow::GetForCurrentThread()->Dispatcher->
			ProcessEvents(Windows::UI::Core::CoreProcessEventsOption::ProcessAllIfPresent);
	}

	Windows::Foundation::AsyncStatus S = A->Status;

	assert(A->Status == Windows::Foundation::AsyncStatus::Completed);

	return A->GetResults();
	
}


static const char* GetClipboardTextFn_DefaultImpl()
{
	auto content=Windows::ApplicationModel::DataTransfer::Clipboard::GetContent();

	auto t = content->GetTextAsync();


	//TODO:dangerous, need timer
	auto retstr=WaitForAsync(t);

	std::wstring fooW(retstr->Begin());


	std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;
	
	auto b=utf8_conv.to_bytes(fooW);



	size_t length = b.length();

	char *retval=(char *)ImGui::MemAlloc(length+1);

	

	memcpy(retval, b.data(), length+1);

	return  retval;


}

static void SetClipboardTextFn_DefaultImpl(const char* text)
{

	//TODO: tab 2/4 length
	//TODO: bug bug - global keyboard focus and backgroind enter/delete???

	wchar_t *buf = (wchar_t *)alloca((strlen(text) + 1)*2);

	auto convresult=ImTextStrFromUtf8(
		(ImWchar *)buf
		,(strlen(text) + 1) * 2,
		text, 
		nullptr);


	std::wstring wid_str = std::wstring(buf);
	const wchar_t* w_char = wid_str.c_str();
	Platform::String^ p_string = ref new Platform::String(w_char);


	auto dataPackage = ref new Windows::ApplicationModel::DataTransfer::DataPackage();

	dataPackage->SetText(p_string);

	Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(dataPackage);
}



void* MemAllocFn(size_t sz)
{
	return _aligned_malloc(sz,16);
}
void MemFreeFn(void* ptr)
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

	D3D12_INPUT_ELEMENT_DESC imguiVertElem[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	m_ImguiPSO.SetRootSignature(m_ImguiSig);

	D3D12_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FrontCounterClockwise = true;
	rasterizerDesc.DepthBias = 0;
	rasterizerDesc.DepthBiasClamp = 0.f;
	rasterizerDesc.SlopeScaledDepthBias = 0.f;
	rasterizerDesc.DepthClipEnable = true;
	rasterizerDesc.MultisampleEnable = false;
	rasterizerDesc.AntialiasedLineEnable = true;
	rasterizerDesc.ForcedSampleCount = 1;
	rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	m_ImguiPSO.SetRasterizerState(RasterizerTwoSided);



	m_ImguiPSO.SetBlendState(BlendTraditional);


	m_ImguiPSO.SetDepthStencilState(DepthStateDisabled);
	m_ImguiPSO.SetSampleMask(0xFFFFFFFF);

	m_ImguiPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_ImguiPSO.SetInputLayout(_countof(imguiVertElem), imguiVertElem);


	m_ImguiPSO.SetRenderTargetFormat(g_OverlayBuffer.GetFormat(), DXGI_FORMAT_UNKNOWN);

	m_ImguiPSO.SetVertexShader(g_pImguiVS, sizeof(g_pImguiVS));
	m_ImguiPSO.SetPixelShader(g_pImguiPS, sizeof(g_pImguiPS));
	m_ImguiPSO.Finalize();


	imguiVertexBuffer.Create(L"imguiVertexBuffer", UINT16_MAX, sizeof(ImDrawVert), nullptr);
	imguiIndexBuffer.Create(L"imguiIndexBuffer", UINT16_MAX, sizeof(ImDrawIdx), nullptr);




	auto size_of_vsinput = sizeof(VSInput);

	manualVertexBuffer.Create(L"manualVertexBuffer", UINT16_MAX, size_of_vsinput, nullptr);
	manualIndexBuffer.Create(L"manualIndexBuffer", UINT16_MAX, sizeof(unsigned short), nullptr);


	ImGuiIO& io = ImGui::GetIO();


	io.KeyMap[ImGuiKey_Tab] = VK_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
    io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
    io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
    io.KeyMap[ImGuiKey_Home] = VK_HOME;
    io.KeyMap[ImGuiKey_End] = VK_END;
    io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
    io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
    io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
	io.KeyMap[ImGuiKey_Insert] = VK_INSERT;
	io.KeyMap[ImGuiKey_A] = 'A';
	io.KeyMap[ImGuiKey_C] = 'C';
	io.KeyMap[ImGuiKey_V] = 'V';
	io.KeyMap[ImGuiKey_X] = 'X';
	io.KeyMap[ImGuiKey_Y] = 'Y';
	io.KeyMap[ImGuiKey_Z] = 'Z';



	void * addr = &io.KeysDown;

	void * addr1 = &io.KeysDown[VK_ESCAPE];


	ImGuiStyle& style = ImGui::GetStyle();
	style.FrameRounding = 0;





	//auto addr = (DWORD)&io.MemAllocFn;

	io.MemAllocFn = MemAllocFn;
	io.MemFreeFn = MemFreeFn;
	io.GetClipboardTextFn = GetClipboardTextFn_DefaultImpl;
	io.SetClipboardTextFn = SetClipboardTextFn_DefaultImpl;

	unsigned char* pixels = 0;
	int width, height;

	ImFontConfig config;
	ZeroMemory(&config, sizeof(config));

	config.OversampleH = 1;
	config.OversampleV = 1;
	config.PixelSnapH = true;
	config.GlyphExtraSpacing.x = 1.0f;

	





	io.Fonts->AddFontFromFileTTF("assets/consola.ttf", fontSize, &config, io.Fonts->GetGlyphRangesDefault());
	config.MergeMode = true;
	io.Fonts->AddFontFromFileTTF("assets/consola.ttf", fontSize, &config, io.Fonts->GetGlyphRangesCyrillic());

	/*
	io.Fonts->AddFontFromFileTTF("assets/consolab.ttf", 26, &config, io.Fonts->GetGlyphRangesDefault());
	io.Fonts->AddFontFromFileTTF("assets/consolab.ttf", 26, &config, io.Fonts->GetGlyphRangesCyrillic());
	io.Fonts->AddFontFromFileTTF("assets/consolai.ttf", 26, &config, io.Fonts->GetGlyphRangesDefault());
	io.Fonts->AddFontFromFileTTF("assets/consolai.ttf", 26, &config, io.Fonts->GetGlyphRangesCyrillic());
	io.Fonts->AddFontFromFileTTF("assets/consolaz.ttf", 26, &config, io.Fonts->GetGlyphRangesDefault());
	io.Fonts->AddFontFromFileTTF("assets/consolaz.ttf", 26, &config, io.Fonts->GetGlyphRangesCyrillic());
	*/


	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	// use ImGui::PushFont()/ImGui::PopFont() to change the font at runtime


	GImGui->Style.FrameRounding = 0;
	GImGui->Style.WindowRounding = 0;
	GImGui->Style.ScrollbarRounding = 0;
	GImGui->Style.ScrollbarSize = 24;



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


	grid_texture = TextureManager::LoadDDSFromFile("grid.dds", true);
	grid_texture->WaitForLoad();
	default_normal_texture = TextureManager::LoadDDSFromFile("default_normal.dds", true);
	default_normal_texture->WaitForLoad();

	default_specular_texture = TextureManager::LoadDDSFromFile("default_specular.dds", true);
	default_specular_texture->WaitForLoad();



	//load shader text
	FILE *file = nullptr;
	if (0 != fopen_s(&file, "Shaders/ps.hlsl", "rb"))
	{
		assert(false);
		//error
	}
	else
	{
		fseek(file, 0L, SEEK_END);
		auto sz = ftell(file);


		memset(shader_text, 0, ARRAYSIZE(shader_text));
		assert(ARRAYSIZE(shader_text) > sz);

		fseek(file, 0L, SEEK_SET);
		auto readed=fread(shader_text, sz, 1, file);

		auto err=ferror(file);


		assert(sz == readed*sz);

		shader_text[sz] = 0;

		fclose(file);
	}

	string unix_style = string(shader_text);
	FindReplace(unix_style, string("\r"), string(""));

	memcpy(shader_text, unix_style.data(), unix_style.length() + 1);


	//--end load shader text

	//bool ok = false;

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
