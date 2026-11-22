#include "CityFrameProbe.h"
#include "Campaign/CityCampaignMode.h"
#include "RHIStats.h"
#include "DynamicRHI.h"
#include "RHIGlobals.h"
#include "RenderTimer.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
void UCityFrameProbe::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (!GetWorld()->IsGameWorld()) return;
    if (!FParse::Value(FCommandLine::Get(),TEXT("CityBench="),File)) return;
    FParse::Value(FCommandLine::Get(),TEXT("CityBenchFrames="),Limit);
    const auto GPU=GRHIAdapterName;
    FString Env=FString::Printf(TEXT("gpu=%s\ncpu=%s\nrhi=%s\n"),*GPU,*FPlatformMisc::GetCPUBrand(),GDynamicRHI?GDynamicRHI->GetName():TEXT("unknown"));
    FFileHelper::SaveStringToFile(Env,*(File+TEXT(".hardware.txt")));
    Rows=TEXT("frame,elapsed_ms,game_ms,render_ms,gpu_ms,working_set_mib,pawn_x,pawn_y,world_s,texture_mib,phase\n");
}
void UCityFrameProbe::Flush()
{
    if(!Rows.IsEmpty() && !File.IsEmpty()) {
        FFileHelper::SaveStringToFile(Rows,*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),bWrote?FILEWRITE_Append:0);
        bWrote=true;Rows.Reset();
    }
}
void UCityFrameProbe::Tick(float)
{
    const double Now=FPlatformTime::Seconds();
    if (Last>0)
    {
        auto* PC=GetWorld()->GetFirstPlayerController();
        const FVector At=PC && PC->GetPawn() ? PC->GetPawn()->GetActorLocation():FVector::ZeroVector;
        ++Samples;
        if (Samples%60==1)
        {
            MemoryMiB=double(FPlatformMemory::GetStats().UsedPhysical)/1048576.;
            if (GDynamicRHI) {
                FTextureMemoryStats Memory;
                RHIGetTextureMemoryStats(Memory);
                TextureMiB=double(Memory.StreamingMemorySize+Memory.NonStreamingMemorySize)/1048576.;
            }
        }
        const auto* GM=GetWorld()->GetAuthGameMode<ACityCampaignMode>();
        Rows+=FString::Printf(TEXT("%d,%.4f,%.4f,%.4f,%.4f,%.2f,%.0f,%.0f,%.3f,%.2f,%d\n"),Samples,(Now-Last)*1000,FPlatformTime::ToMilliseconds(GGameThreadTime),FPlatformTime::ToMilliseconds(GRenderThreadTime),FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()),MemoryMiB,At.X,At.Y,GetWorld()->GetTimeSeconds(),TextureMiB,GM?int32(GM->GetPhase()):-1);
        if (Samples%240==0 || (Limit>0 && Samples>=Limit)) Flush();
        if(Limit>0 && Samples>=Limit) { File.Empty();FPlatformMisc::RequestExit(false); }
    }
    Last=Now;
}
void UCityFrameProbe::Deinitialize() { Flush(); Super::Deinitialize(); }