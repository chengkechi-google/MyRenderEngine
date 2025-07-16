#include "Im3DImpl.h"
#include "Core/Engine.h"
#include "Utils/profiler.h"
#include "im3d/im3d.h"
#include "imgui/imgui.h"

Im3DImpl::Im3DImpl(Renderer* pRenderer) : m_pRenderer(pRenderer)
{

}

Im3DImpl::~Im3DImpl()
{

}

bool Im3DImpl::Init()
{
	RHIMeshShaderPipelineDesc msDesc;
	msDesc.m_rasterizerState.m_cullMode = RHICullMode::None;
	msDesc.m_depthStencilState.m_depthWrite = false;
	msDesc.m_depthStencilState.m_depthTest = true;
	msDesc.m_depthStencilState.m_depthFunc = RHICompareFunc::Greater;
	msDesc.m_blendState[0].m_blendEnable = true;
	msDesc.m_blendState[0].m_colorSrc = RHIBlendFactor::SrcAlpha;
	msDesc.m_blendState[0].m_colorDst = RHIBlendFactor::InvSrcAlpha;
	msDesc.m_blendState[0].m_alphaSrc = RHIBlendFactor::One;
	msDesc.m_blendState[0].m_alphaDst = RHIBlendFactor::InvSrcAlpha;
	msDesc.m_rtFormat[0] = m_pRenderer->GetSwapChain()->GetDesc().m_format;
	msDesc.m_depthStencilFromat = RHIFormat::D32F;

	msDesc.m_pMS = m_pRenderer->GetShader("Im3D.hlsl", "ms_main", RHIShaderType::MS, { "POINTS=1" });
	msDesc.m_pPS = m_pRenderer->GetShader("Im3D.hlsl", "ps_main", RHIShaderType::PS, { "POINTS=1" });
	m_pPointPSO = m_pRenderer->GetPipelineState(msDesc, "Im3D Point PSO");

	msDesc.m_pMS = m_pRenderer->GetShader("Im3D.hlsl", "ms_main", RHIShaderType::MS, { "LINES=1" });
	msDesc.m_pPS = m_pRenderer->GetShader("Im3D.hlsl", "ps_main", RHIShaderType::PS, { "LINES=1" });
	m_pLinePSO = m_pRenderer->GetPipelineState(msDesc, "Im3D Line PSO");

	msDesc.m_pMS = m_pRenderer->GetShader("Im3D.hlsl", "ms_main", RHIShaderType::MS, { "TRIANGLES=1" });
	msDesc.m_pPS = m_pRenderer->GetShader("Im3D.hlsl", "ps_main", RHIShaderType::PS, { "TRIANGLES=1" });
	m_pTrianglePSO = m_pRenderer->GetPipelineState(msDesc, "IM3D Triangle PSO");;


	return true;
}

void Im3DImpl::NewFrame()
{
	Im3d::AppData& appData = Im3d::GetAppData();
	Camera* pCamera = Engine::GetInstance()->GetWorld()->GetCamera();

	appData.m_deltaTime = Engine::GetInstance()->GetFrameDeltaTime();
	appData.m_viewportSize = Im3d::Vec2((float)m_pRenderer->GetDisplayWidth(), (float)m_pRenderer->GetDisplayHeight());
	appData.m_viewOrigin = pCamera->GetPosition();
	appData.m_viewDirection = pCamera->GetForward();
	appData.m_worldUp = Im3d::Vec3(0.0f, 1.0f, 0.0f);
	appData.m_projScaleY = tanf(degree_to_radian(pCamera->GetFOV()) * 0.5f) * 2.0f;

	Im3d::Vec2 cursorPos(ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
	cursorPos.x = (cursorPos.x / appData.m_viewportSize.x) * 2.0f - 1.0f;
	cursorPos.y = (cursorPos.y / appData.m_viewportSize.y) * 2.0f - 1.0f;
	cursorPos.y = -cursorPos.y;

	float3 rayOrigin, rayDirection;
	rayOrigin = appData.m_viewOrigin;
	rayDirection.x = cursorPos.x / pCamera->GetNonJitterProjectionMatrix()[0][0];
	rayDirection.y = cursorPos.y / pCamera->GetNonJitterProjectionMatrix()[1][1];
	rayDirection.z = -1.0f;
	rayDirection = mul(pCamera->GetWorldMatrix(), float4(normalize(rayDirection), 0.0f)).xyz();

	appData.m_cursorRayOrigin = rayOrigin;
	appData.m_cursorRayDirection = rayDirection;

	bool ctrlDown = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
	appData.m_keyDown[Im3d::Key_L] = ctrlDown && ImGui::IsKeyDown(ImGuiKey_L);
	appData.m_keyDown[Im3d::Key_T] = ctrlDown && ImGui::IsKeyDown(ImGuiKey_T);
	appData.m_keyDown[Im3d::Key_R] = ctrlDown && ImGui::IsKeyDown(ImGuiKey_R);
	appData.m_keyDown[Im3d::Key_S] = ctrlDown && ImGui::IsKeyDown(ImGuiKey_S);
	appData.m_keyDown[Im3d::Mouse_Left] = ctrlDown && ImGui::IsKeyDown(ImGuiKey_MouseLeft);

	appData.m_snapTranslation = ctrlDown ? 0.1f : 0.0f;
	appData.m_snapRotation = ctrlDown ? degree_to_radian(30.0f) : 0.0f;
	appData.m_snapScale = ctrlDown ? 0.5f : 0.0f;

	Im3d::NewFrame();
}

void Im3DImpl::Render(IRHICommandList* pCommandList)
{
	GPU_EVENT(pCommandList, "Im3d");

	Im3d::EndFrame();

	uint32_t frameIndex = m_pRenderer->GetFrameID() % RHI_MAX_INFLIGHT_FRAMES;
	uint vertexCount = GetVertexCount();

	if (m_pVertexBuffer[frameIndex] == nullptr || m_pVertexBuffer[frameIndex]->GetBuffer()->GetDesc().m_size < vertexCount * sizeof(Im3d::VertexData))
	{
		m_pVertexBuffer[frameIndex].reset(m_pRenderer->CreateRawBuffer(nullptr, (vertexCount + 1000) * sizeof(Im3d::VertexData), "Im3D Vertex Buffer"));	//< The 1000 is use to make sure we don't re-size vertex buffer frequently
	}

	uint32_t vertexBufferOffset = 0;
	for (uint32_t i = 0; i < Im3d::GetDrawListCount(); ++i)
	{
		const Im3d::DrawList& drawList = Im3d::GetDrawLists()[i];

		memcpy((char*)m_pVertexBuffer[frameIndex]->GetBuffer()->GetCPUAddress() + vertexBufferOffset, drawList.m_vertexData, sizeof(Im3d::VertexData) * drawList.m_vertexCount);
		
		uint32_t primitiveCount = 0;
		switch (drawList.m_primType)
		{
		case Im3d::DrawPrimitive_Points:
			primitiveCount = drawList.m_vertexCount;
			pCommandList->SetPipelineState(m_pPointPSO);
			break;

		case Im3d::DrawPrimitive_Lines:
			primitiveCount = drawList.m_vertexCount / 2;
			pCommandList->SetPipelineState(m_pLinePSO);
			break;

		case Im3d::DrawPrimitive_Triangles:
			primitiveCount = drawList.m_vertexCount / 3;
			pCommandList->SetPipelineState(m_pTrianglePSO);
			break;

		default:
			break;
		}

		uint32_t cb[] =
		{
			primitiveCount,
			m_pVertexBuffer[frameIndex]->GetSRV()->GetHeapIndex(),
			vertexBufferOffset
		};

		pCommandList->SetGraphicsConstants(0, cb, sizeof(cb));
		pCommandList->DispatchMesh(DivideRoundingUp(primitiveCount, 64), 1, 1);
		vertexBufferOffset += sizeof(Im3d::VertexData) * drawList.m_vertexCount;
	}
}

uint32_t Im3DImpl::GetVertexCount() const
{
	uint32_t vertexCount = 0;

	for (uint32_t i = 0; i < Im3d::GetDrawListCount(); ++ i)
	{
		const Im3d::DrawList& drawList = Im3d::GetDrawLists()[i];
		vertexCount += drawList.m_vertexCount;
	}

	return vertexCount;
}