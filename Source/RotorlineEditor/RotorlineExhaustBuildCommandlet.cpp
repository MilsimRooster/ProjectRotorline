#include "RotorlineExhaustBuildCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EdGraph/EdGraphPin.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "NiagaraConstants.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterFactoryNew.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeFunctionCall.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemFactoryNew.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

namespace RotorlineExhaustBuild
{
    void BindInput(
        UNiagaraNodeFunctionCall& Node,
        const TCHAR* InputName,
        const FNiagaraTypeDefinition& Type,
        const TCHAR* UserName)
    {
        const FNiagaraParameterHandle Input = FNiagaraParameterHandle::CreateModuleParameterHandle(InputName);
        const FNiagaraParameterHandle Aliased = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(Input, &Node);
        UEdGraphPin& Pin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
            Node, Aliased, Type, FGuid(), FGuid());
        const FNiagaraVariableBase Linked(Type, FName(UserName));
        TSet<FNiagaraVariableBase> Known;
        Known.Add(Linked);
        FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(Pin, Linked, Known);
    }

    bool SaveAsset(UObject* Asset, const FString& PackageName)
    {
        if (!Asset) return false;
        UPackage* Package = Asset->GetOutermost();
        Package->MarkPackageDirty();
        const FString Filename = FPackageName::LongPackageNameToFilename(
            PackageName, FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, Asset, *Filename, Args);
    }

    UNiagaraEmitter* BuildEmitter(
        const FString& PackageName,
        const FName AssetName,
        UMaterialInterface* Material,
        const TCHAR* SpawnRateUser,
        const FVector2D& SpriteSize,
        float Lifetime,
        bool bVapor)
    {
        UPackage* Package = CreatePackage(*PackageName);
        UNiagaraEmitter* Emitter = NewObject<UNiagaraEmitter>(
            Package, AssetName, RF_Public | RF_Standalone | RF_Transactional);
        // Let NiagaraEditor construct the standard graph internally. Calling the
        // stack graph construction helpers directly from a project commandlet
        // is not supported because those helpers are intentionally not exported.
        UNiagaraEmitterFactoryNew::InitializeEmitter(Emitter, true);
        Emitter->bIsInheritable = true;

        FVersionedNiagaraEmitterData* Data = Emitter->GetLatestEmitterData();
        if (!Data || !Data->GraphSource) return nullptr;
        Data->SimTarget = ENiagaraSimTarget::CPUSim;
        Data->bLocalSpace = false;
        Data->bDeterminism = false;
        Data->InterpolatedSpawnMode = ENiagaraInterpolatedSpawnMode::Interpolation;

        UNiagaraScriptSource* Source = CastChecked<UNiagaraScriptSource>(Data->GraphSource);

        TArray<UNiagaraNodeFunctionCall*> FunctionNodes;
        Source->NodeGraph->GetNodesOfClass(FunctionNodes);
        for (UNiagaraNodeFunctionCall* FunctionNode : FunctionNodes)
        {
            if (!FunctionNode) continue;
            const FString FunctionName = FunctionNode->GetFunctionName();
            if (FunctionName.Equals(TEXT("SpawnRate"), ESearchCase::IgnoreCase))
            {
                BindInput(*FunctionNode, TEXT("SpawnRate"), FNiagaraTypeDefinition::GetFloatDef(), SpawnRateUser);
            }
            else if (FunctionName.Equals(TEXT("AddVelocity"), ESearchCase::IgnoreCase))
            {
                BindInput(*FunctionNode, TEXT("Velocity"), FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.ExhaustJetVelocity"));
            }
        }

        TArray<UNiagaraNodeAssignment*> AssignmentNodes;
        Source->NodeGraph->GetNodesOfClass(AssignmentNodes);
        for (UNiagaraNodeAssignment* Assignment : AssignmentNodes)
        {
            if (!Assignment) continue;
            for (int32 Index = 0; Index < Assignment->NumTargets(); ++Index)
            {
                const FNiagaraVariable& Target = Assignment->GetAssignmentTarget(Index);
                FString NewDefault;
                bool bReplace = false;
                if (Target.GetName() == SYS_PARAM_PARTICLES_SPRITE_SIZE.GetName())
                {
                    NewDefault = FString::Printf(TEXT("(X=%.2f,Y=%.2f)"), SpriteSize.X, SpriteSize.Y);
                    bReplace = true;
                }
                else if (Target.GetName() == SYS_PARAM_PARTICLES_LIFETIME.GetName())
                {
                    NewDefault = FString::Printf(TEXT("%.3f"), Lifetime);
                    bReplace = true;
                }
                if (bReplace)
                {
                    Assignment->SetAssignmentTarget(Index, Target, &NewDefault);
                }
            }
        }

        UNiagaraSpriteRendererProperties* Renderer = nullptr;
        for (UNiagaraRendererProperties* RendererProperties : Data->GetRenderers())
        {
            Renderer = Cast<UNiagaraSpriteRendererProperties>(RendererProperties);
            if (Renderer) break;
        }
        if (!Renderer) return nullptr;
        Renderer->Material = Material;
        Renderer->Alignment = ENiagaraSpriteAlignment::VelocityAligned;
        Renderer->FacingMode = ENiagaraSpriteFacingMode::FaceCamera;
        Renderer->bCastShadows = false;
        Renderer->bEnableCameraDistanceCulling = true;
        Renderer->MinCameraDistance = 0.0f;
        Renderer->MaxCameraDistance = bVapor ? 15000.0f : 22000.0f;
        Data->SpawnScriptProps.Script->SetUsage(ENiagaraScriptUsage::ParticleSpawnScriptInterpolated);
        FAssetRegistryModule::AssetCreated(Emitter);
        return Emitter;
    }

    void AddUserParameter(UNiagaraSystem& System, const FNiagaraTypeDefinition& Type, const TCHAR* Name)
    {
        System.GetExposedParameters().AddParameter(FNiagaraVariable(Type, FName(Name)));
    }
}

URotorlineExhaustBuildCommandlet::URotorlineExhaustBuildCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
    ShowErrorCount = true;
}

int32 URotorlineExhaustBuildCommandlet::Main(const FString& Params)
{
    using namespace RotorlineExhaustBuild;
    UMaterialInterface* HeatMaterial = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Game/FX/HelicopterExhaust/M_ExhaustHeatDistortion.M_ExhaustHeatDistortion"));
    UMaterialInterface* VaporMaterial = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Game/FX/HelicopterExhaust/M_ExhaustVapor.M_ExhaustVapor"));
    if (!HeatMaterial || !VaporMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_EXHAUST_BUILD|FAIL|materials_missing"));
        return 1;
    }

    const FString HeatPackage = TEXT("/Game/FX/HelicopterExhaust/NE_ExhaustHeatCore");
    const FString VaporPackage = TEXT("/Game/FX/HelicopterExhaust/NE_ExhaustVaporWisps");
    UNiagaraEmitter* Heat = BuildEmitter(
        HeatPackage, TEXT("NE_ExhaustHeatCore"), HeatMaterial, TEXT("User.HeatSpawnRate"), FVector2D(34.0f, 168.0f), 0.58f, false);
    UNiagaraEmitter* Vapor = BuildEmitter(
        VaporPackage, TEXT("NE_ExhaustVaporWisps"), VaporMaterial, TEXT("User.VaporSpawnRate"), FVector2D(22.0f, 210.0f), 1.05f, true);
    if (!Heat || !Vapor)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_EXHAUST_BUILD|FAIL|emitter_creation"));
        return 1;
    }

    const FString SystemPackageName = TEXT("/Game/FX/HelicopterExhaust/NS_RotorlineTurboshaftExhaust");
    UPackage* SystemPackage = CreatePackage(*SystemPackageName);
    UNiagaraSystem* System = NewObject<UNiagaraSystem>(
        SystemPackage, TEXT("NS_RotorlineTurboshaftExhaust"), RF_Public | RF_Standalone | RF_Transactional);
    UNiagaraSystemFactoryNew::InitializeSystem(System, true);
    FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Heat, Heat->GetExposedVersion().VersionGuid);
    FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Vapor, Vapor->GetExposedVersion().VersionGuid);

    AddUserParameter(*System, FNiagaraTypeDefinition::GetBoolDef(), TEXT("User.ExhaustEnabled"));
    AddUserParameter(*System, FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.EngineRPM"));
    AddUserParameter(*System, FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.EnginePower"));
    AddUserParameter(*System, FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.ExhaustIntensity"));
    AddUserParameter(*System, FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.ExhaustTemperature"));
    AddUserParameter(*System, FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.AircraftVelocity"));
    AddUserParameter(*System, FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.WindVelocity"));
    AddUserParameter(*System, FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.RotorWashStrength"));
    AddUserParameter(*System, FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.AmbientHumidity"));
    AddUserParameter(*System, FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.DamageLevel"));
    AddUserParameter(*System, FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.HeatSpawnRate"));
    AddUserParameter(*System, FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.VaporSpawnRate"));
    AddUserParameter(*System, FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.ExhaustJetVelocity"));
    AddUserParameter(*System, FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.OutletDiameter"));
    System->SetFixedBounds(FBox(FVector(-1400.0f, -500.0f, -500.0f), FVector(500.0f, 500.0f, 500.0f)));
    System->RequestCompile(false);
    FAssetRegistryModule::AssetCreated(System);

    const bool bSaved = SaveAsset(Heat, HeatPackage) && SaveAsset(Vapor, VaporPackage)
        && SaveAsset(System, SystemPackageName);
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_EXHAUST_BUILD|%s|emitters=2|world_space=1|fixed_bounds=1|cpu_low_count=1"),
        bSaved ? TEXT("PASS") : TEXT("FAIL"));
    return bSaved ? 0 : 1;
}
