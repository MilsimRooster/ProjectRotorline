#include "RotorlineWeatherBuildCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraphPin.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "NiagaraConstants.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterFactoryNew.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemFactoryNew.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

namespace RotorlineWeatherBuild
{
    UMaterial* BuildRainMaterial()
    {
        const FString PackageName = TEXT("/Game/FX/Weather/M_RotorlineRainStreak");
        UPackage* Package = CreatePackage(*PackageName);
        UMaterial* Material = NewObject<UMaterial>(
            Package, TEXT("M_RotorlineRainStreak"),
            RF_Public | RF_Standalone | RF_Transactional);
        Material->MaterialDomain = MD_Surface;
        Material->BlendMode = BLEND_Additive;
        Material->SetShadingModel(MSM_Unlit);
        Material->TwoSided = true;

        UMaterialExpressionConstant3Vector* Color =
            NewObject<UMaterialExpressionConstant3Vector>(Material);
        Color->Constant = FLinearColor(0.52f, 0.67f, 0.76f);
        Material->GetExpressionCollection().AddExpression(Color);
        Material->GetExpressionInputForProperty(MP_EmissiveColor)->Expression = Color;

        UMaterialExpressionTextureCoordinate* UV =
            NewObject<UMaterialExpressionTextureCoordinate>(Material);
        UMaterialExpressionSubtract* Centered =
            NewObject<UMaterialExpressionSubtract>(Material);
        UMaterialExpressionAbs* DistanceFromCenter =
            NewObject<UMaterialExpressionAbs>(Material);
        UMaterialExpressionMultiply* NormalizedDistance =
            NewObject<UMaterialExpressionMultiply>(Material);
        UMaterialExpressionOneMinus* CenterMask =
            NewObject<UMaterialExpressionOneMinus>(Material);
        UMaterialExpressionPower* NarrowMask =
            NewObject<UMaterialExpressionPower>(Material);
        UMaterialExpressionMultiply* Opacity =
            NewObject<UMaterialExpressionMultiply>(Material);
        for (UMaterialExpression* Expression : {
            static_cast<UMaterialExpression*>(UV),
            static_cast<UMaterialExpression*>(Centered),
            static_cast<UMaterialExpression*>(DistanceFromCenter),
            static_cast<UMaterialExpression*>(NormalizedDistance),
            static_cast<UMaterialExpression*>(CenterMask),
            static_cast<UMaterialExpression*>(NarrowMask),
            static_cast<UMaterialExpression*>(Opacity) })
        {
            Material->GetExpressionCollection().AddExpression(Expression);
        }
        Centered->A.Expression = UV;
        Centered->ConstB = 0.5f;
        DistanceFromCenter->Input.Expression = Centered;
        NormalizedDistance->A.Expression = DistanceFromCenter;
        NormalizedDistance->ConstB = 2.0f;
        CenterMask->Input.Expression = NormalizedDistance;
        NarrowMask->Base.Expression = CenterMask;
        NarrowMask->ConstExponent = 12.0f;
        Opacity->A.Expression = NarrowMask;
        Opacity->ConstB = 0.12f;
        Material->GetExpressionInputForProperty(MP_Opacity)->Expression = Opacity;
        Material->PostEditChange();
        FAssetRegistryModule::AssetCreated(Material);
        return Material;
    }

    void BindInput(UNiagaraNodeFunctionCall& Node, const TCHAR* InputName,
        const FNiagaraTypeDefinition& Type, const TCHAR* UserName)
    {
        const FNiagaraParameterHandle Input = FNiagaraParameterHandle::CreateModuleParameterHandle(InputName);
        const FNiagaraParameterHandle Aliased =
            FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(Input, &Node);
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
}

URotorlineWeatherBuildCommandlet::URotorlineWeatherBuildCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
    ShowErrorCount = true;
}

int32 URotorlineWeatherBuildCommandlet::Main(const FString& Params)
{
    using namespace RotorlineWeatherBuild;
    UMaterial* RainMaterial = BuildRainMaterial();
    if (!RainMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("ROTORLINE_WEATHER_BUILD|FAIL|material_missing"));
        return 1;
    }

    const FString EmitterPackageName = TEXT("/Game/FX/Weather/NE_RotorlineRainStreak");
    UPackage* EmitterPackage = CreatePackage(*EmitterPackageName);
    UNiagaraEmitter* Emitter = NewObject<UNiagaraEmitter>(
        EmitterPackage, TEXT("NE_RotorlineRainStreak"),
        RF_Public | RF_Standalone | RF_Transactional);
    UNiagaraEmitterFactoryNew::InitializeEmitter(Emitter, true);
    Emitter->bIsInheritable = true;

    FVersionedNiagaraEmitterData* Data = Emitter->GetLatestEmitterData();
    if (!Data || !Data->GraphSource) return 1;
    Data->SimTarget = ENiagaraSimTarget::CPUSim;
    Data->bLocalSpace = false;
    Data->bDeterminism = false;
    Data->InterpolatedSpawnMode = ENiagaraInterpolatedSpawnMode::Interpolation;

    UNiagaraScriptSource* Source = CastChecked<UNiagaraScriptSource>(Data->GraphSource);
    TArray<UNiagaraNodeFunctionCall*> Functions;
    Source->NodeGraph->GetNodesOfClass(Functions);
    for (UNiagaraNodeFunctionCall* Function : Functions)
    {
        if (!Function) continue;
        const FString Name = Function->GetFunctionName();
        if (Name.Equals(TEXT("SpawnRate"), ESearchCase::IgnoreCase))
        {
            BindInput(*Function, TEXT("SpawnRate"), FNiagaraTypeDefinition::GetFloatDef(),
                TEXT("User.RainSpawnRate"));
        }
        else if (Name.Equals(TEXT("AddVelocity"), ESearchCase::IgnoreCase))
        {
            BindInput(*Function, TEXT("Velocity"), FNiagaraTypeDefinition::GetVec3Def(),
                TEXT("User.RainVelocity"));
        }
    }

    TArray<UNiagaraNodeAssignment*> Assignments;
    Source->NodeGraph->GetNodesOfClass(Assignments);
    for (UNiagaraNodeAssignment* Assignment : Assignments)
    {
        if (!Assignment) continue;
        for (int32 Index = 0; Index < Assignment->NumTargets(); ++Index)
        {
            const FNiagaraVariable& Target = Assignment->GetAssignmentTarget(Index);
            if (Target.GetName() == SYS_PARAM_PARTICLES_SPRITE_SIZE.GetName())
            {
                const FString DefaultValue(TEXT("(X=1.2,Y=82.0)"));
                Assignment->SetAssignmentTarget(Index, Target, &DefaultValue);
            }
            else if (Target.GetName() == SYS_PARAM_PARTICLES_LIFETIME.GetName())
            {
                const FString DefaultValue(TEXT("0.58"));
                Assignment->SetAssignmentTarget(Index, Target, &DefaultValue);
            }
        }
    }

    UNiagaraSpriteRendererProperties* Renderer = nullptr;
    for (UNiagaraRendererProperties* Properties : Data->GetRenderers())
    {
        Renderer = Cast<UNiagaraSpriteRendererProperties>(Properties);
        if (Renderer) break;
    }
    if (!Renderer) return 1;
    Renderer->Material = RainMaterial;
    Renderer->Alignment = ENiagaraSpriteAlignment::VelocityAligned;
    Renderer->FacingMode = ENiagaraSpriteFacingMode::FaceCamera;
    Renderer->bCastShadows = false;
    Renderer->bEnableCameraDistanceCulling = true;
    Renderer->MaxCameraDistance = 12000.0f;
    Data->SpawnScriptProps.Script->SetUsage(ENiagaraScriptUsage::ParticleSpawnScriptInterpolated);
    FAssetRegistryModule::AssetCreated(Emitter);

    const FString SystemPackageName = TEXT("/Game/FX/Weather/NS_RotorlineRain");
    UPackage* SystemPackage = CreatePackage(*SystemPackageName);
    UNiagaraSystem* System = NewObject<UNiagaraSystem>(
        SystemPackage, TEXT("NS_RotorlineRain"),
        RF_Public | RF_Standalone | RF_Transactional);
    UNiagaraSystemFactoryNew::InitializeSystem(System, true);
    FNiagaraEditorUtilities::AddEmitterToSystem(
        *System, *Emitter, Emitter->GetExposedVersion().VersionGuid);
    System->GetExposedParameters().AddParameter(FNiagaraVariable(
        FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.RainSpawnRate")));
    System->GetExposedParameters().AddParameter(FNiagaraVariable(
        FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.RainVelocity")));
    System->SetFixedBounds(FBox(
        FVector(-700.0f, -700.0f, -4000.0f),
        FVector(700.0f, 700.0f, 2200.0f)));
    System->RequestCompile(false);
    FAssetRegistryModule::AssetCreated(System);

    const bool bSaved =
        SaveAsset(RainMaterial, TEXT("/Game/FX/Weather/M_RotorlineRainStreak")) &&
        SaveAsset(Emitter, EmitterPackageName) &&
        SaveAsset(System, SystemPackageName);
    UE_LOG(LogTemp, Display,
        TEXT("ROTORLINE_WEATHER_BUILD|%s|niagara=1|world_space=1|volumetric_fog=runtime"),
        bSaved ? TEXT("PASS") : TEXT("FAIL"));
    return bSaved ? 0 : 1;
}
