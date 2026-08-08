#include "RotorlineGroundingLibrary.h"

#include "Components/PrimitiveComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "LandscapeProxy.h"

namespace RotorlineGrounding
{
    const FName ApprovedGroundTag(TEXT("RotorlineApprovedGround"));
    const FName MissionPadTag(TEXT("RotorlineMissionPad"));
    const FName RejectedGroundTag(TEXT("RotorlineRejectedGround"));
    const FName IgnoreBoundsTag(TEXT("RotorlineGroundingIgnore"));

    struct FSampleHit
    {
        FVector Point = FVector::ZeroVector;
        FVector Normal = FVector::UpVector;
        TWeakObjectPtr<AActor> Actor;
        bool bHadRejectedObstruction = false;
    };

    bool IsWaterLike(const AActor* Actor)
    {
        if (!Actor) return false;
        const FString Description = Actor->GetName() + TEXT(" ") + Actor->GetClass()->GetName();
        return Description.Contains(TEXT("Water"), ESearchCase::IgnoreCase) ||
            Description.Contains(TEXT("Ocean"), ESearchCase::IgnoreCase);
    }

    bool IsApproved(const FHitResult& Hit, const FRotorlineGroundingProfile& Profile)
    {
        const AActor* HitActor = Hit.GetActor();
        const UPrimitiveComponent* HitComponent = Hit.GetComponent();
        if (!HitActor || IsWaterLike(HitActor) || HitActor->ActorHasTag(RejectedGroundTag) ||
            (HitComponent && HitComponent->ComponentHasTag(RejectedGroundTag)))
        {
            return false;
        }
        if (Profile.bAllowLandscape && HitActor->IsA<ALandscapeProxy>()) return true;
        if (Profile.bAllowPreparedGround &&
            (HitActor->ActorHasTag(ApprovedGroundTag) || HitActor->ActorHasTag(MissionPadTag) ||
                (HitComponent && (HitComponent->ComponentHasTag(ApprovedGroundTag) ||
                    HitComponent->ComponentHasTag(MissionPadTag)))))
        {
            return true;
        }
        return false;
    }

    void AddIgnoredAssembly(AActor* IgnoredActor, FCollisionQueryParams& Params)
    {
        if (!IgnoredActor) return;
        Params.AddIgnoredActor(IgnoredActor);
        TArray<AActor*> AttachedActors;
        IgnoredActor->GetAttachedActors(AttachedActors, true, true);
        Params.AddIgnoredActors(AttachedActors);
    }

    bool TraceApprovedSurface(
        UWorld* World,
        const FVector2D& XY,
        float ReferenceZ,
        AActor* IgnoredActor,
        const FRotorlineGroundingProfile& Profile,
        FSampleHit& OutHit)
    {
        if (!World) return false;
        const FVector Start(XY.X, XY.Y, ReferenceZ + Profile.TraceHeightCm);
        const FVector End(XY.X, XY.Y, ReferenceZ - Profile.TraceDepthCm);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(RotorlineSharedGrounding), true);
        AddIgnoredAssembly(IgnoredActor, Params);
        TArray<FHitResult> Hits;
        if (!World->LineTraceMultiByChannel(Hits, Start, End, ECC_Visibility, Params))
        {
            if (Profile.bDrawDebug)
            {
                DrawDebugLine(World, Start, End, FColor::Red, false, 12.0f, 0, 4.0f);
            }
            return false;
        }

        const FHitResult* ApprovedHit = nullptr;
        for (const FHitResult& Hit : Hits)
        {
            if (IsApproved(Hit, Profile))
            {
                ApprovedHit = &Hit;
                break;
            }
        }
        if (!ApprovedHit)
        {
            if (Profile.bDrawDebug)
            {
                DrawDebugLine(World, Start, End, FColor::Red, false, 12.0f, 0, 4.0f);
            }
            return false;
        }

        for (const FHitResult& Hit : Hits)
        {
            if (&Hit == ApprovedHit) break;
            const AActor* HitActor = Hit.GetActor();
            if (!HitActor || HitActor == IgnoredActor || IsWaterLike(HitActor)) continue;
            if (Hit.ImpactPoint.Z > ApprovedHit->ImpactPoint.Z + Profile.ObstructionClearanceCm)
            {
                OutHit.bHadRejectedObstruction = true;
                break;
            }
        }

        OutHit.Point = ApprovedHit->ImpactPoint;
        OutHit.Normal = ApprovedHit->ImpactNormal.GetSafeNormal();
        OutHit.Actor = ApprovedHit->GetActor();
        if (Profile.bDrawDebug)
        {
            const FColor Color = OutHit.bHadRejectedObstruction ? FColor::Orange : FColor::Green;
            DrawDebugLine(World, Start, OutHit.Point, Color, false, 12.0f, 0, 3.0f);
            DrawDebugPoint(World, OutHit.Point, 18.0f, Color, false, 12.0f);
            DrawDebugDirectionalArrow(World, OutHit.Point, OutHit.Point + OutHit.Normal * 250.0f,
                55.0f, FColor::Cyan, false, 12.0f, 0, 5.0f);
        }
        return true;
    }

    TArray<FVector2D> BuildSamplePoints(const FVector& Center, const FVector2D& HalfExtent, const FRotorlineGroundingProfile& Profile)
    {
        TArray<FVector2D> Points;
        Points.Add(FVector2D(Center.X, Center.Y));
        if (Profile.Mode == ERotorlineGroundingMode::Upright ||
            Profile.Mode == ERotorlineGroundingMode::LinearPoint ||
            HalfExtent.IsNearlyZero())
        {
            return Points;
        }

        const float Inset = FMath::Clamp(Profile.FootprintInsetRatio, 0.0f, 0.45f);
        const float X = HalfExtent.X * (1.0f - Inset);
        const float Y = HalfExtent.Y * (1.0f - Inset);
        for (const float SX : {-1.0f, 1.0f})
        {
            for (const float SY : {-1.0f, 1.0f})
            {
                Points.Add(FVector2D(Center.X + SX * X, Center.Y + SY * Y));
            }
        }
        for (int32 Index = 0; Index < FMath::Max(1, Profile.EdgeSamplesPerSide); ++Index)
        {
            const float Alpha = (Index + 1.0f) / (FMath::Max(1, Profile.EdgeSamplesPerSide) + 1.0f);
            const float AlongX = FMath::Lerp(-X, X, Alpha);
            const float AlongY = FMath::Lerp(-Y, Y, Alpha);
            Points.Add(FVector2D(Center.X + AlongX, Center.Y - Y));
            Points.Add(FVector2D(Center.X + AlongX, Center.Y + Y));
            Points.Add(FVector2D(Center.X - X, Center.Y + AlongY));
            Points.Add(FVector2D(Center.X + X, Center.Y + AlongY));
        }
        return Points;
    }

    bool SolveContactInternal(
        UWorld* World,
        const FVector& CurrentContactPoint,
        const FVector2D& FootprintHalfExtent,
        AActor* IgnoredActor,
        const FRotorlineGroundingProfile& Profile,
        FRotorlineGroundingResult& OutResult)
    {
        OutResult = FRotorlineGroundingResult();
        OutResult.ProfileName = Profile.ProfileName;
        if (!World)
        {
            OutResult.Failure = ERotorlineGroundingFailure::InvalidWorld;
            OutResult.Detail = TEXT("World is unavailable");
            return false;
        }

        const TArray<FVector2D> Samples = BuildSamplePoints(CurrentContactPoint, FootprintHalfExtent, Profile);
        OutResult.RequestedSamples = Samples.Num();
        TArray<FSampleHit> Hits;
        for (const FVector2D& Sample : Samples)
        {
            FSampleHit Hit;
            if (TraceApprovedSurface(World, Sample, CurrentContactPoint.Z, IgnoredActor, Profile, Hit))
            {
                Hits.Add(Hit);
            }
        }
        OutResult.ValidSamples = Hits.Num();
        if (Hits.IsEmpty() || (Profile.bRequireAllSamples && Hits.Num() != Samples.Num()))
        {
            OutResult.Failure = ERotorlineGroundingFailure::MissingGroundHit;
            OutResult.Detail = FString::Printf(TEXT("Valid samples %d/%d"), Hits.Num(), Samples.Num());
            return false;
        }
        if (Profile.bRejectObstructionsAboveGround && Hits.ContainsByPredicate([](const FSampleHit& Hit)
            { return Hit.bHadRejectedObstruction; }))
        {
            OutResult.Failure = ERotorlineGroundingFailure::Obstructed;
            OutResult.Detail = TEXT("Rejected collision exists above the approved ground surface");
            return false;
        }

        float MinZ = Hits[0].Point.Z;
        float MaxZ = Hits[0].Point.Z;
        FVector AverageNormal = FVector::ZeroVector;
        for (const FSampleHit& Hit : Hits)
        {
            MinZ = FMath::Min(MinZ, Hit.Point.Z);
            MaxZ = FMath::Max(MaxZ, Hit.Point.Z);
            AverageNormal += Hit.Normal;
        }
        AverageNormal = AverageNormal.GetSafeNormal();
        if (AverageNormal.IsNearlyZero()) AverageNormal = FVector::UpVector;
        OutResult.MinimumGroundZ = MinZ;
        OutResult.MaximumGroundZ = MaxZ;
        OutResult.FootprintRoughnessCm = MaxZ - MinZ;
        OutResult.SurfaceNormal = AverageNormal;
        OutResult.SurfaceSlopeDegrees = FMath::RadiansToDegrees(FMath::Acos(
            FMath::Clamp(FVector::DotProduct(AverageNormal, FVector::UpVector), -1.0f, 1.0f)));
        OutResult.GroundActor = Hits[0].Actor.Get();

        if (OutResult.SurfaceSlopeDegrees > Profile.MaximumSlopeDegrees)
        {
            OutResult.Failure = ERotorlineGroundingFailure::ExcessiveSlope;
            OutResult.Detail = FString::Printf(TEXT("Slope %.1f exceeds %.1f degrees"),
                OutResult.SurfaceSlopeDegrees, Profile.MaximumSlopeDegrees);
            return false;
        }
        if (Profile.Mode == ERotorlineGroundingMode::MultiPointFootprint &&
            OutResult.FootprintRoughnessCm > Profile.MaximumFootprintRoughnessCm)
        {
            OutResult.Failure = ERotorlineGroundingFailure::UnevenFootprint;
            OutResult.Detail = FString::Printf(TEXT("Footprint roughness %.1f exceeds %.1f cm"),
                OutResult.FootprintRoughnessCm, Profile.MaximumFootprintRoughnessCm);
            return false;
        }

        const float SupportZ = Profile.Mode == ERotorlineGroundingMode::MultiPointFootprint
            ? MaxZ
            : Hits[0].Point.Z;
        const float DesiredContactZ = SupportZ + Profile.PreservedOffsetCm - Profile.ContactSinkCm;
        OutResult.ContactPoint = FVector(CurrentContactPoint.X, CurrentContactPoint.Y, DesiredContactZ);
        OutResult.TranslationDelta = FVector(0.0f, 0.0f, DesiredContactZ - CurrentContactPoint.Z);
        OutResult.bSuccess = true;
        OutResult.Detail = FString::Printf(TEXT("Grounded with %d/%d samples"), Hits.Num(), Samples.Num());
        return true;
    }

    bool IsContactComponent(const UPrimitiveComponent* Primitive, const FRotorlineGroundingProfile& Profile)
    {
        if (!Primitive || Primitive->ComponentHasTag(IgnoreBoundsTag)) return false;
        if (Profile.ContactComponentNames.IsEmpty()) return Primitive->IsVisible();
        return Profile.ContactComponentNames.Contains(Primitive->GetFName());
    }

    bool CalculateActorBoundsAtTransform(
        AActor* Actor,
        const FRotorlineGroundingProfile& Profile,
        const FTransform& CandidateActorTransform,
        FBox& OutBounds)
    {
        OutBounds = FBox(EForceInit::ForceInit);
        if (!Actor) return false;
        const FTransform CurrentActorTransform = Actor->GetActorTransform();
        TInlineComponentArray<UPrimitiveComponent*> Primitives(Actor);
        for (UPrimitiveComponent* Primitive : Primitives)
        {
            if (!IsContactComponent(Primitive, Profile)) continue;
            const FTransform RelativeToActor = Primitive->GetComponentTransform().GetRelativeTransform(CurrentActorTransform);
            const FTransform CandidateComponentTransform = RelativeToActor * CandidateActorTransform;
            OutBounds += Primitive->CalcBounds(CandidateComponentTransform).GetBox();
        }
        return OutBounds.IsValid != 0;
    }

    void ApplyGroundingRotation(FTransform& Transform, const FRotorlineGroundingProfile& Profile, const FVector& SurfaceNormal)
    {
        if (Profile.Mode == ERotorlineGroundingMode::SurfaceAligned || Profile.Mode == ERotorlineGroundingMode::Vehicle)
        {
            const FVector Forward = Transform.GetRotation().GetForwardVector();
            const FVector SurfaceForward = FVector::VectorPlaneProject(Forward, SurfaceNormal).GetSafeNormal();
            if (!SurfaceForward.IsNearlyZero())
            {
                Transform.SetRotation(FRotationMatrix::MakeFromXZ(SurfaceForward, SurfaceNormal).ToQuat());
            }
        }
        else
        {
            const FRotator CurrentRotation = Transform.Rotator();
            Transform.SetRotation(FRotator(0.0f, CurrentRotation.Yaw, 0.0f).Quaternion());
        }
    }
}

FRotorlineGroundingProfile URotorlineGroundingLibrary::MakeProfile(ERotorlineGroundingMode Mode, FName ProfileName)
{
    FRotorlineGroundingProfile Profile;
    Profile.Mode = Mode;
    Profile.ProfileName = ProfileName;
    switch (Mode)
    {
    case ERotorlineGroundingMode::SurfaceAligned:
        Profile.MaximumSlopeDegrees = 28.0f;
        Profile.MaximumFootprintRoughnessCm = 90.0f;
        Profile.bCheckCollisionPenetration = false;
        break;
    case ERotorlineGroundingMode::MultiPointFootprint:
        Profile.MaximumSlopeDegrees = 8.0f;
        Profile.MaximumFootprintRoughnessCm = 125.0f;
        Profile.EdgeSamplesPerSide = 2;
        break;
    case ERotorlineGroundingMode::Vehicle:
        Profile.MaximumSlopeDegrees = 18.0f;
        Profile.MaximumFootprintRoughnessCm = 95.0f;
        Profile.EdgeSamplesPerSide = 1;
        break;
    case ERotorlineGroundingMode::LinearPoint:
        Profile.MaximumSlopeDegrees = 35.0f;
        Profile.bCheckCollisionPenetration = false;
        break;
    default:
        break;
    }
    return Profile;
}

bool URotorlineGroundingLibrary::CalculateContactBounds(
    AActor* Actor,
    const FRotorlineGroundingProfile& Profile,
    FBox& OutBounds)
{
    OutBounds = FBox(EForceInit::ForceInit);
    if (!Actor) return false;
    TInlineComponentArray<UPrimitiveComponent*> Primitives(Actor);
    for (UPrimitiveComponent* Primitive : Primitives)
    {
        if (!RotorlineGrounding::IsContactComponent(Primitive, Profile)) continue;
        Primitive->UpdateBounds();
        OutBounds += Primitive->Bounds.GetBox();
    }
    return OutBounds.IsValid != 0;
}

bool URotorlineGroundingLibrary::SolveGroundContact(
    UObject* WorldContextObject,
    const FVector& CurrentContactPoint,
    const FVector2D& FootprintHalfExtent,
    AActor* IgnoredActor,
    const FRotorlineGroundingProfile& Profile,
    FRotorlineGroundingResult& OutResult)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    return RotorlineGrounding::SolveContactInternal(
        World, CurrentContactPoint, FootprintHalfExtent, IgnoredActor, Profile, OutResult);
}

bool URotorlineGroundingLibrary::SolveActorGrounding(
    AActor* Actor,
    const FRotorlineGroundingProfile& Profile,
    FRotorlineGroundingResult& OutResult)
{
    if (!Actor || !Actor->GetWorld())
    {
        OutResult = FRotorlineGroundingResult();
        OutResult.Failure = ERotorlineGroundingFailure::InvalidWorld;
        return false;
    }

    FBox ContactBounds;
    if (!CalculateContactBounds(Actor, Profile, ContactBounds))
    {
        OutResult = FRotorlineGroundingResult();
        OutResult.ProfileName = Profile.ProfileName;
        OutResult.Failure = ERotorlineGroundingFailure::MissingContactBounds;
        OutResult.Detail = TEXT("No visible contact primitive matched the placement profile");
        return false;
    }

    const FVector ContactCenter(ContactBounds.GetCenter().X, ContactBounds.GetCenter().Y, ContactBounds.Min.Z);
    const FVector BoundsExtent = ContactBounds.GetExtent();
    const FVector2D Footprint(BoundsExtent.X, BoundsExtent.Y);
    if (!RotorlineGrounding::SolveContactInternal(
        Actor->GetWorld(), ContactCenter, Footprint, Actor, Profile, OutResult))
    {
        const ERotorlineGroundingFailure InitialFailure = OutResult.Failure;
        FRotorlineGroundingResult BestResult;
        FVector2D BestOffset = FVector2D::ZeroVector;
        bool bFoundRelocation = false;
        const int32 Directions = FMath::Clamp(Profile.RelocationDirections, 4, 32);
        for (float Radius = FMath::Max(100.0f, Profile.RelocationStepCm);
             Radius <= Profile.MaximumRelocationRadiusCm + KINDA_SMALL_NUMBER && !bFoundRelocation;
             Radius += FMath::Max(100.0f, Profile.RelocationStepCm))
        {
            for (int32 DirectionIndex = 0; DirectionIndex < Directions; ++DirectionIndex)
            {
                const float Angle = 2.0f * PI * DirectionIndex / Directions;
                const FVector2D Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius);
                const FVector CandidateContact = ContactCenter + FVector(Offset.X, Offset.Y, 0.0f);
                FRotorlineGroundingResult CandidateResult;
                if (RotorlineGrounding::SolveContactInternal(
                    Actor->GetWorld(), CandidateContact, Footprint, Actor, Profile, CandidateResult))
                {
                    BestResult = CandidateResult;
                    BestOffset = Offset;
                    bFoundRelocation = true;
                    break;
                }
            }
        }
        if (!bFoundRelocation)
        {
            OutResult.Failure = InitialFailure;
            return false;
        }
        OutResult = BestResult;
        OutResult.TranslationDelta.X = BestOffset.X;
        OutResult.TranslationDelta.Y = BestOffset.Y;
        OutResult.Detail += FString::Printf(TEXT("; relocated %.0f cm"), BestOffset.Size());
    }

    FTransform Desired = Actor->GetActorTransform();
    Desired.AddToTranslation(OutResult.TranslationDelta);
    RotorlineGrounding::ApplyGroundingRotation(Desired, Profile, OutResult.SurfaceNormal);

    // Rotation changes an asymmetric mesh's world-space bottom. Re-evaluate
    // physical contact at the candidate transform until position and rotation
    // converge, rather than requiring repeated editor passes after reload.
    const FVector OriginalLocation = Actor->GetActorLocation();
    for (int32 Iteration = 0; Iteration < 4; ++Iteration)
    {
        FBox CandidateBounds;
        if (!RotorlineGrounding::CalculateActorBoundsAtTransform(Actor, Profile, Desired, CandidateBounds)) break;
        const FVector CandidateContact(
            CandidateBounds.GetCenter().X, CandidateBounds.GetCenter().Y, CandidateBounds.Min.Z);
        const FVector CandidateExtent = CandidateBounds.GetExtent();
        FRotorlineGroundingResult IterationResult;
        if (!RotorlineGrounding::SolveContactInternal(
            Actor->GetWorld(), CandidateContact, FVector2D(CandidateExtent.X, CandidateExtent.Y),
            Actor, Profile, IterationResult))
        {
            OutResult = IterationResult;
            return false;
        }
        Desired.AddToTranslation(IterationResult.TranslationDelta);
        RotorlineGrounding::ApplyGroundingRotation(Desired, Profile, IterationResult.SurfaceNormal);
        OutResult = IterationResult;
        if (FMath::Abs(IterationResult.TranslationDelta.Z) <= 0.5f) break;
    }
    OutResult.TranslationDelta = Desired.GetLocation() - OriginalLocation;
    OutResult.DesiredTransform = Desired;
    return true;
}

bool URotorlineGroundingLibrary::ApplyActorGrounding(
    AActor* Actor,
    const FRotorlineGroundingProfile& Profile,
    FRotorlineGroundingResult& OutResult,
    bool bSweep)
{
    if (!SolveActorGrounding(Actor, Profile, OutResult)) return false;
    FHitResult SweepHit;
    const bool bMoved = Actor->SetActorTransform(
        OutResult.DesiredTransform, bSweep, &SweepHit, ETeleportType::None);
    if (!bMoved && !Actor->GetActorTransform().Equals(OutResult.DesiredTransform, 0.2f))
    {
        OutResult.bSuccess = false;
        OutResult.Failure = ERotorlineGroundingFailure::CollisionPenetration;
        OutResult.Detail = SweepHit.GetActor()
            ? FString::Printf(TEXT("Sweep blocked by %s"), *SweepHit.GetActor()->GetName())
            : TEXT("Grounding sweep was blocked");
        return false;
    }
    return true;
}

FRotorlineGroundingResult URotorlineGroundingLibrary::EvaluateActorGrounding(
    AActor* Actor,
    const FRotorlineGroundingProfile& Profile)
{
    FRotorlineGroundingResult Result;
    SolveActorGrounding(Actor, Profile, Result);
    return Result;
}

bool URotorlineGroundingLibrary::SolveInstancedMeshGrounding(
    UInstancedStaticMeshComponent* Component,
    int32 InstanceIndex,
    const FRotorlineGroundingProfile& Profile,
    FRotorlineGroundingResult& OutResult)
{
    OutResult = FRotorlineGroundingResult();
    OutResult.ProfileName = Profile.ProfileName;
    if (!Component || !Component->GetWorld() || !Component->GetStaticMesh() ||
        InstanceIndex < 0 || InstanceIndex >= Component->GetInstanceCount())
    {
        OutResult.Failure = ERotorlineGroundingFailure::MissingContactBounds;
        OutResult.Detail = TEXT("Invalid instanced mesh component, mesh, or instance index");
        return false;
    }

    FTransform InstanceTransform;
    if (!Component->GetInstanceTransform(InstanceIndex, InstanceTransform, true))
    {
        OutResult.Failure = ERotorlineGroundingFailure::MissingContactBounds;
        OutResult.Detail = TEXT("Could not read world-space instance transform");
        return false;
    }

    const FBox LocalBounds = Component->GetStaticMesh()->GetBoundingBox();
    const auto BoundsForTransform = [&LocalBounds](const FTransform& Transform)
    {
        FBox Bounds(EForceInit::ForceInit);
        for (const float X : {LocalBounds.Min.X, LocalBounds.Max.X})
        {
            for (const float Y : {LocalBounds.Min.Y, LocalBounds.Max.Y})
            {
                for (const float Z : {LocalBounds.Min.Z, LocalBounds.Max.Z})
                {
                    Bounds += Transform.TransformPosition(FVector(X, Y, Z));
                }
            }
        }
        return Bounds;
    };
    FBox WorldBounds = BoundsForTransform(InstanceTransform);
    if (!WorldBounds.IsValid)
    {
        OutResult.Failure = ERotorlineGroundingFailure::MissingContactBounds;
        OutResult.Detail = TEXT("Instance world bounds are invalid");
        return false;
    }

    const FVector ContactCenter(WorldBounds.GetCenter().X, WorldBounds.GetCenter().Y, WorldBounds.Min.Z);
    const FVector BoundsExtent = WorldBounds.GetExtent();
    if (!RotorlineGrounding::SolveContactInternal(
        Component->GetWorld(), ContactCenter, FVector2D(BoundsExtent.X, BoundsExtent.Y),
        Component->GetOwner(), Profile, OutResult))
    {
        return false;
    }

    FTransform Desired = InstanceTransform;
    Desired.AddToTranslation(OutResult.TranslationDelta);
    RotorlineGrounding::ApplyGroundingRotation(Desired, Profile, OutResult.SurfaceNormal);
    const FVector OriginalLocation = InstanceTransform.GetLocation();
    for (int32 Iteration = 0; Iteration < 4; ++Iteration)
    {
        const FBox CandidateBounds = BoundsForTransform(Desired);
        const FVector CandidateContact(
            CandidateBounds.GetCenter().X, CandidateBounds.GetCenter().Y, CandidateBounds.Min.Z);
        const FVector CandidateExtent = CandidateBounds.GetExtent();
        FRotorlineGroundingResult IterationResult;
        if (!RotorlineGrounding::SolveContactInternal(
            Component->GetWorld(), CandidateContact, FVector2D(CandidateExtent.X, CandidateExtent.Y),
            Component->GetOwner(), Profile, IterationResult))
        {
            OutResult = IterationResult;
            return false;
        }
        Desired.AddToTranslation(IterationResult.TranslationDelta);
        RotorlineGrounding::ApplyGroundingRotation(Desired, Profile, IterationResult.SurfaceNormal);
        OutResult = IterationResult;
        if (FMath::Abs(IterationResult.TranslationDelta.Z) <= 0.5f) break;
    }
    OutResult.TranslationDelta = Desired.GetLocation() - OriginalLocation;
    OutResult.DesiredTransform = Desired;
    return true;
}

bool URotorlineGroundingLibrary::ApplyInstancedMeshGrounding(
    UInstancedStaticMeshComponent* Component,
    int32 InstanceIndex,
    const FRotorlineGroundingProfile& Profile,
    FRotorlineGroundingResult& OutResult)
{
    if (!SolveInstancedMeshGrounding(Component, InstanceIndex, Profile, OutResult)) return false;
    if (!Component->UpdateInstanceTransform(
        InstanceIndex, OutResult.DesiredTransform, true, true, true))
    {
        OutResult.bSuccess = false;
        OutResult.Failure = ERotorlineGroundingFailure::CollisionPenetration;
        OutResult.Detail = TEXT("Could not update world-space instance transform");
        return false;
    }
    return true;
}

FRotorlineGroundingResult URotorlineGroundingLibrary::EvaluateInstancedMeshGrounding(
    UInstancedStaticMeshComponent* Component,
    int32 InstanceIndex,
    const FRotorlineGroundingProfile& Profile)
{
    FRotorlineGroundingResult Result;
    SolveInstancedMeshGrounding(Component, InstanceIndex, Profile, Result);
    return Result;
}
