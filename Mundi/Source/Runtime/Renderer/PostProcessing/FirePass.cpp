#include "pch.h"
#include "FirePass.h"
#include "../SceneView.h"
#include "../../RHI/SwapGuard.h"
#include "../../RHI/ConstantBufferType.h"

void FFirePass::Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice)
{
    if (!IsApplicable(M)) return;

    // 1) 스왑 + SRV 언바인드 관리 (SceneColorSource 한 장만 읽음)
    FSwapGuard Swap(RHIDevice, /*FirstSlot*/0, /*NumSlotsToUnbind*/1);

    // 2) 타깃 RTV 설정
    RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

    // Depth State: Depth Test/Write 모두 OFF
    RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
    RHIDevice->OMSetBlendState(false); // 전화면 덮어쓰기

    // 3) 셰이더
    UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
    UShader* FirePS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/Fire_PS.hlsl");
    if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !FirePS || !FirePS->GetPixelShader())
    {
        UE_LOG("Fire용 셰이더 없음!\n");
        return;
    }

    RHIDevice->PrepareShader(FullScreenTriangleVS, FirePS);

    // 4) SRV / Sampler (현재 SceneColorSource)
    ID3D11ShaderResourceView* SceneSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
    ID3D11SamplerState* LinearClampSamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);

    if (!SceneSRV || !LinearClampSamplerState)
    {
        UE_LOG("Fire: Scene SRV / LinearClamp Sampler is null!\n");
        return;
    }

    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &SceneSRV);
    RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, &LinearClampSamplerState);

    // 5) 상수 버퍼 업데이트
    FFireBufferType FireConstant;
    FireConstant.FireColor = M.Payload.Color;
    FireConstant.Intensity = M.Payload.Params0.X;
    FireConstant.EdgeStart = M.Payload.Params0.Y;
    FireConstant.NoiseScale = M.Payload.Params0.Z;
    FireConstant.Time = M.Payload.Params0.W;
    FireConstant.Weight = M.Weight;

    RHIDevice->SetAndUpdateConstantBuffer(FireConstant);

    // 6) Draw
    RHIDevice->DrawFullScreenQuad();

    // 7) 확정
    Swap.Commit();
}
