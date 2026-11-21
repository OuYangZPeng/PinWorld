#include "MigrationScene.h"
#include "MigrationState.h"
#include "MigrationEnvironment.h"
#include "ProceduralMeshComponent.h"
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogRoomMigration, Log, All);

namespace
{
struct FParsedObj
{
    TArray<FVector> Vertices;
    TArray<int32> Indices;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TMap<int32, TArray<int32>> Sections;
};

static int32 ObjIndex(const FString& Token, int32 Count)
{
    int32 Index = FCString::Atoi(*Token);
    return Index < 0 ? Count + Index : Index - 1;
}

static bool ParseObj(const FString& Path, FParsedObj& Out)
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Path))
    {
        return false;
    }

    int32 Slot = 0;
    TArray<FString> Lines;
    Text.ParseIntoArrayLines(Lines, false);
    for (const FString& Line : Lines)
    {
        FString Trimmed = Line.TrimStartAndEnd();
        if (Trimmed.StartsWith(TEXT("v ")))
        {
            TArray<FString> P;
            Trimmed.ParseIntoArray(P, TEXT(" "), true);
            if (P.Num() >= 4)
            {
                Out.Vertices.Add(FVector(FCString::Atod(*P[1]), FCString::Atod(*P[2]), FCString::Atod(*P[3])));
            }
        }
        else if (Trimmed.StartsWith(TEXT("vt ")))
        {
            TArray<FString> P;
            Trimmed.ParseIntoArray(P, TEXT(" "), true);
            if (P.Num() >= 3)
            {
                Out.UVs.Add(FVector2D(FCString::Atod(*P[1]), FCString::Atod(*P[2])));
            }
        }
        else if (Trimmed.StartsWith(TEXT("vn ")))
        {
            TArray<FString> P;
            Trimmed.ParseIntoArray(P, TEXT(" "), true);
            if (P.Num() >= 4)
            {
                Out.Normals.Add(FVector(FCString::Atod(*P[1]), FCString::Atod(*P[2]), FCString::Atod(*P[3])));
            }
        }
        else if (Trimmed.StartsWith(TEXT("g submesh_")))
        {
            Slot = FCString::Atoi(*Trimmed.Mid(10));
            if (Slot < 0) return false;
        }
        else if (Trimmed.StartsWith(TEXT("usemtl slot_")))
        {
            if (FCString::Atoi(*Trimmed.Mid(12)) != Slot) return false;
        }
        else if (Trimmed.StartsWith(TEXT("f ")))
        {
            TArray<FString> P;
            Trimmed.ParseIntoArray(P, TEXT(" "), true);
            if (P.Num() < 4)
            {
                continue;
            }
            TArray<int32> Face;
            for (int32 I = 1; I < P.Num(); ++I)
            {
                TArray<FString> Parts;
                P[I].ParseIntoArray(Parts, TEXT("/"), false);
                if (Parts.Num() > 0)
                {
                    const int32 V = ObjIndex(Parts[0], Out.Vertices.Num());
                    // Exporter uses shared position/normal/UV indices. Reject other layouts
                    // rather than silently shading with the wrong corner attributes.
                    if (Parts.Num() > 1 && !Parts[1].IsEmpty() && ObjIndex(Parts[1], Out.UVs.Num()) != V) return false;
                    if (Parts.Num() > 2 && !Parts[2].IsEmpty() && ObjIndex(Parts[2], Out.Normals.Num()) != V) return false;
                    if (!Out.Vertices.IsValidIndex(V)) return false;
                    Face.Add(V);
                }
            }
            for (int32 I = 1; I + 1 < Face.Num(); ++I)
            {
                Out.Indices.Add(Face[0]);
                Out.Indices.Add(Face[I]);
                Out.Indices.Add(Face[I + 1]);
                TArray<int32>& Section = Out.Sections.FindOrAdd(Slot);
                Section.Add(Face[0]); Section.Add(Face[I]); Section.Add(Face[I + 1]);
            }
        }
    }
    return Out.Vertices.Num() > 0 && Out.Indices.Num() >= 3;
}

static FString StringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
{
    FString Value;
    Object->TryGetStringField(Name, Value);
    return Value;
}

static FVector UnityToUE(const FVector& V)
{
    return FVector(V.Z, V.X, V.Y);
}

// Unity serializes columns. Bake the complete world affine map exactly once;
// actor/component remain identity (no parent/TRS decomposition, preserving shear).
static bool WorldBasis(const TSharedPtr<FJsonObject>& Object, FVector& X, FVector& Y, FVector& Z, FVector& T)
{
    const TArray<TSharedPtr<FJsonValue>>* M = nullptr;
    if (!Object->TryGetArrayField(TEXT("worldMatrix"), M) || M->Num() != 16) return false;
    double V[16];
    for (int32 I = 0; I < 16; ++I)
        if (!(*M)[I]->TryGetNumber(V[I]) || !FMath::IsFinite(V[I])) return false;
    if (!FMath::IsNearlyZero(V[3]) || !FMath::IsNearlyZero(V[7]) || !FMath::IsNearlyZero(V[11]) || !FMath::IsNearlyEqual(V[15], 1.0)) return false;
    X = UnityToUE(FVector(V[0], V[1], V[2]));
    Y = UnityToUE(FVector(V[4], V[5], V[6]));
    Z = UnityToUE(FVector(V[8], V[9], V[10]));
    T = UnityToUE(FVector(V[12], V[13], V[14]));
    return FMath::Abs(FVector::DotProduct(X, FVector::CrossProduct(Y, Z))) > 1.e-12;
}

static bool SafeDataPath(const FString& Root, const FString& Relative, FString& Out)
{
    if (Relative.IsEmpty() || !FPaths::IsRelative(Relative) || Relative.Contains(TEXT(":"))) return false;
    Out = FPaths::ConvertRelativePathToFull(Root / Relative);
    FPaths::NormalizeFilename(Out);
    return FPaths::CollapseRelativeDirectories(Out) && Out.StartsWith(Root + TEXT("/"));
}

static FString MigrationRoot(bool Foxy)
{
    FString Root;
    const bool Override = FParse::Value(FCommandLine::Get(), TEXT("PinWorldMigrationRoot="), Root) && !Root.IsEmpty();
    FString Source = Override ? TEXT("command-line") : TEXT("deployed");
    if (!Override)
    {
        Root = FPaths::ProjectContentDir() / TEXT("UnityMigrationData");
#if WITH_EDITOR
        if (!FPaths::FileExists(Root / (Foxy ? TEXT("Foxy/manifest.json") : TEXT("manifest.json"))))
        {
            Root = FPaths::ProjectSavedDir() / TEXT("UnityMigration");
            Source = TEXT("editor-only-fallback");
        }
#endif
    }
    Root = FPaths::ConvertRelativePathToFull(Root);
    FPaths::NormalizeDirectoryName(Root);
    FPaths::CollapseRelativeDirectories(Root);
    if (Foxy) Root /= TEXT("Foxy");
    FPaths::NormalizeDirectoryName(Root);
    UE_LOG(LogRoomMigration, Display, TEXT("MIGRATION_DATA source=%s root=%s"), *Source, *Root);
    return Root;
}

}

bool UMigrationScene::LoadLevel(bool Foxy)
{
    State->Scroll = 0;
    SavedSlots.Reset();
    for (const TWeakObjectPtr<AActor>& Actor : Spawned)
    {
        if (Actor.IsValid())
        {
            Actor->Destroy();
        }
    }
    Spawned.Reset();
    State->Entries.Reset();
    State->Doors.Reset();
    Renderers.Reset();
    OriginalMaterials.Reset();
    Resources.Reset();
    OriginalTransforms.Reset();
    State->EntryByMember.Reset();
    State->EntryById.Reset();
    RoomBounds = FBox(ForceInit);
    State->IsFoxy = Foxy;

    const FString Root = State->IsTiananmen ? MigrationRoot(false) / TEXT("Tiananmen") : State->IsTaiheHall ? MigrationRoot(false) / TEXT("TaiheHall") : State->IsHallOfPrayer ? MigrationRoot(false) / TEXT("HallOfPrayer") : MigrationRoot(Foxy);
    const FString ManifestPath = Root / TEXT("manifest.json");
    FString JsonText;
    TSharedPtr<FJsonObject> Manifest;
    if (!FFileHelper::LoadFileToString(JsonText, *ManifestPath))
    {
        State->Status = FString::Printf(TEXT("Manifest read failed: %s"), *ManifestPath);
        UE_LOG(LogRoomMigration, Error, TEXT("ROOM_RESULT success=0 error=%s"), *State->Status);
        return false;
    }
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, Manifest) || !Manifest.IsValid())
    {
        State->Status = FString::Printf(TEXT("Manifest unavailable: %s"), *ManifestPath);
        UE_LOG(LogRoomMigration, Error, TEXT("ROOM_RESULT success=0 error=%s"), *State->Status);
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* EntryValues = nullptr;
    Manifest->TryGetArrayField(TEXT("panelEntries"), EntryValues);
    if (EntryValues)
    {
        for (const TSharedPtr<FJsonValue>& Value : *EntryValues)
        {
            const TSharedPtr<FJsonObject> EntryObject = Value->AsObject();
            if (!EntryObject.IsValid()) continue;
            FMigrationEntry Entry;
            Entry.Id = StringField(EntryObject, TEXT("id"));
            Entry.Name = StringField(EntryObject, TEXT("name"));
            Entry.EquivalenceGroup = StringField(EntryObject, TEXT("equivalenceGroup"));
            Entry.Placed = false;
            State->EntryById.Add(Entry.Id, State->Entries.Num());
            const TArray<TSharedPtr<FJsonValue>>* EntryRenderers = nullptr;
            if (EntryObject->TryGetArrayField(TEXT("rendererObjectIds"), EntryRenderers))
                for (const TSharedPtr<FJsonValue>& Renderer : *EntryRenderers) Entry.Renderers.Add(Renderer->AsString());
            const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
            if (EntryObject->TryGetArrayField(TEXT("memberPlaceableIds"), Members))
                for (const TSharedPtr<FJsonValue>& Member : *Members) { Entry.Members.Add(Member->AsString()); State->EntryByMember.Add(Member->AsString(), State->Entries.Num()); }
            State->Entries.Add(MoveTemp(Entry));
        }
    }

    // Cookable graph generated by the editor-only Python asset pass.
    UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/UnityMigration/M_Migration.M_Migration"));
    TArray<FMaterialParameterInfo> TextureParameters, ColorParameters;
    TArray<FGuid> ParameterIds;
    if (BaseMaterial)
    {
        BaseMaterial->GetAllTextureParameterInfo(TextureParameters, ParameterIds);
        ParameterIds.Reset();
        BaseMaterial->GetAllVectorParameterInfo(ColorParameters, ParameterIds);
    }
    TMap<FString, UTexture2D*> TextureCache;
    int32 TextureLoaded = 0, TextureFailed = 0, TextureBound = 0, ColorBound = 0;
    int32 MaterialSections = 0, ExtraMaterialSlots = 0;
    const TArray<TSharedPtr<FJsonValue>>* MaterialValues = nullptr;
    if (Manifest->TryGetArrayField(TEXT("materials"), MaterialValues) && MaterialValues)
    {
        for (const TSharedPtr<FJsonValue>& Value : *MaterialValues)
        {
            const TSharedPtr<FJsonObject> MaterialObject = Value->AsObject();
            if (!MaterialObject.IsValid()) continue;
            const FString Id = StringField(MaterialObject, TEXT("id"));
            if (Id.IsEmpty()) continue;
            UTexture2D* Texture = nullptr;
            const FString RelativePath = StringField(MaterialObject, TEXT("mainTexturePath"));
            if (!RelativePath.IsEmpty())
            {
                FString TexturePath = FPaths::ConvertRelativePathToFull(Root / RelativePath);
                FPaths::NormalizeFilename(TexturePath);
                if (UTexture2D** Cached = TextureCache.Find(TexturePath)) Texture = *Cached;
                else
                {
                    // Reject absolute paths and traversal outside this manifest root.
                    if (SafeDataPath(Root, RelativePath, TexturePath))
                    {
                        const FString AssetName = TEXT("T_") + FPaths::GetBaseFilename(RelativePath);
                        const FString AssetPath = FString::Printf(TEXT("/Game/UnityMigration/%s/%s.%s"), Foxy ? TEXT("Foxy") : TEXT("Room"), *AssetName, *AssetName);
                        Texture = LoadObject<UTexture2D>(nullptr, *AssetPath);
                        // External migration roots remain supported without editor modules.
                        if (!Texture) Texture = FImageUtils::ImportFileAsTexture2D(TexturePath);
                    }
                    TextureCache.Add(TexturePath, Texture);
                    if (Texture) { Resources.Add(Texture); ++TextureLoaded; }
                    else { ++TextureFailed; UE_LOG(LogRoomMigration, Warning, TEXT("ROOM_TEXTURE failed=%s"), *RelativePath); }
                }
            }
            if (!BaseMaterial) continue;
            UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
            if (!Material) continue;
            FLinearColor Color = FLinearColor::White;
            const TSharedPtr<FJsonObject>* ColorObject = nullptr;
            if (MaterialObject->TryGetObjectField(TEXT("color"), ColorObject) && ColorObject && ColorObject->IsValid())
            {
                double R = 1, G = 1, B = 1, A = 1;
                (*ColorObject)->TryGetNumberField(TEXT("r"), R);
                (*ColorObject)->TryGetNumberField(TEXT("g"), G);
                (*ColorObject)->TryGetNumberField(TEXT("b"), B);
                (*ColorObject)->TryGetNumberField(TEXT("a"), A);
                Color = FLinearColor(R, G, B, A);
            }
            for (const FMaterialParameterInfo& Parameter : ColorParameters)
            {
                if (Parameter.Name == TEXT("Color") || Parameter.Name == TEXT("BaseColor"))
                { Material->SetVectorParameterValue(Parameter.Name, Color); ++ColorBound; break; }
            }
            if (Texture)
            {
                for (const FMaterialParameterInfo& Parameter : TextureParameters)
                {
                    if (Parameter.Name == TEXT("Texture") || Parameter.Name == TEXT("BaseColorTexture") || Parameter.Name == TEXT("MainTexture"))
                    { Material->SetTextureParameterValue(Parameter.Name, Texture); ++TextureBound; break; }
                }
            }
            const auto ReadUV = [&MaterialObject](const TCHAR* Field, double Default)
            {
                double X = Default, Y = Default;
                const TSharedPtr<FJsonObject>* UV = nullptr;
                if (MaterialObject->TryGetObjectField(Field, UV) && UV && UV->IsValid())
                {
                    (*UV)->TryGetNumberField(TEXT("x"), X);
                    (*UV)->TryGetNumberField(TEXT("y"), Y);
                }
                return FLinearColor(X, Y, 0, 0);
            };
            Material->SetVectorParameterValue(TEXT("UVScale"), ReadUV(TEXT("textureScale"), 1));
            Material->SetVectorParameterValue(TEXT("UVOffset"), ReadUV(TEXT("textureOffset"), 0));
            OriginalMaterials.Add(Id, Material);
        }
    }

    const FString MeshRoot = Root / TEXT("meshes");
    const TArray<TSharedPtr<FJsonValue>>* Objects = nullptr;
    Manifest->TryGetArrayField(TEXT("objects"), Objects);
    TMap<FString, FParsedObj> Cache;
    int32 ObjectCount = Objects ? Objects->Num() : 0;
    int32 RendererCount = 0;
    int32 TriangleCount = 0;
    int32 Errors = 0, SectionCount = 0, MissingSlots = 0, Mirrored = 0;
    if (Objects)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Objects)
        {
            const TSharedPtr<FJsonObject> Object = Value->AsObject();
            if (!Object.IsValid()) continue;
            if (!Object->GetBoolField(TEXT("active")) || !Object->GetBoolField(TEXT("rendererEnabled"))) continue;
            // Unity camera UI uses transparent/custom shaders, not opaque world geometry.
            if (StringField(Object, TEXT("hierarchyPath")).StartsWith(TEXT("LevelCommon/"))) continue;
            const FString MeshId = StringField(Object, TEXT("meshId"));
            if (MeshId.IsEmpty()) continue;
            FParsedObj* Parsed = Cache.Find(MeshId);
            if (!Parsed)
            {
                FParsedObj NewMesh;
                FString ObjPath;
                if (!SafeDataPath(MeshRoot, MeshId + TEXT(".obj"), ObjPath) || !ParseObj(ObjPath, NewMesh))
                {
                    ++Errors;
                    continue;
                }
                Parsed = &Cache.Add(MeshId, MoveTemp(NewMesh));
            }
            if (!Parsed || Parsed->Indices.Num() < 3) continue;

            FVector X, Y, Z, T;
            if (!WorldBasis(Object, X, Y, Z, T)) { ++Errors; continue; }
            const double Determinant = FVector::DotProduct(X, FVector::CrossProduct(Y, Z));
            TArray<FVector> Vertices, Normals;
            FBox Bounds(ForceInit);
            for (const FVector& V : Parsed->Vertices)
            {
                const FVector World = (X * V.X + Y * V.Y + Z * V.Z + T) * 100.0;
                Vertices.Add(World);
            }
            for (int32 Index : Parsed->Indices) Bounds += Vertices[Index];
            // Inverse transpose, including determinant sign for mirrored transforms.
            for (const FVector& N : Parsed->Normals)
                Normals.Add(((FVector::CrossProduct(Y, Z) * N.X + FVector::CrossProduct(Z, X) * N.Y + FVector::CrossProduct(X, Y) * N.Z) / Determinant).GetSafeNormal());
            AActor* Actor = GetWorld()->SpawnActor<AActor>();
            if (!Actor) { ++Errors; continue; }
#if WITH_EDITOR
            Actor->SetActorLabel(StringField(Object, TEXT("name")));
#endif
            UProceduralMeshComponent* Component = NewObject<UProceduralMeshComponent>(Actor);
            Actor->SetRootComponent(Component);
            Component->RegisterComponent();
            const TArray<TSharedPtr<FJsonValue>>* MaterialIds = nullptr;
            Object->TryGetArrayField(TEXT("materialIds"), MaterialIds);
            for (const auto& Section : Parsed->Sections)
            {
                TArray<int32> Indices = Section.Value;
                // ZXY preserves handedness; UE front-face convention is opposite
                // Unity's exported triangles. Mirroring cancels this reversal.
                if (Determinant > 0)
                    for (int32 I = 0; I < Indices.Num(); I += 3) Swap(Indices[I + 1], Indices[I + 2]);
                TArray<FVector> SectionVertices, SectionNormals;
                TArray<FVector2D> SectionUVs;
                TMap<int32, int32> Remap;
                for (int32& Index : Indices)
                {
                    const int32 Source = Index;
                    if (const int32* Existing = Remap.Find(Source)) Index = *Existing;
                    else
                    {
                        Index = SectionVertices.Add(Vertices[Source]);
                        Remap.Add(Source, Index);
                        if (Normals.IsValidIndex(Source)) SectionNormals.Add(Normals[Source]);
                        if (Parsed->UVs.IsValidIndex(Source)) SectionUVs.Add(Parsed->UVs[Source]);
                    }
                }
                Component->CreateMeshSection(Section.Key, SectionVertices, Indices, SectionNormals, SectionUVs, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
                ++SectionCount;
                if (MaterialIds && MaterialIds->IsValidIndex(Section.Key))
                {
                    if (const TObjectPtr<UMaterialInstanceDynamic>* Material = OriginalMaterials.Find((*MaterialIds)[Section.Key]->AsString()))
                    {
                        Component->SetMaterial(Section.Key, Material->Get()); ++MaterialSections;
                        if (Section.Key > 0) ++ExtraMaterialSlots;
                    }
                    else ++MissingSlots;
                }
                else ++MissingSlots;
            }
            if (Determinant < 0) ++Mirrored;
            if (RendererCount < 2)
                UE_LOG(LogRoomMigration, Display, TEXT("ROOM_MATRIX id=%s det=%.9g local=%s worldCm=%s bounds=%s slots=%d"), *StringField(Object, TEXT("id")), Determinant, *Parsed->Vertices[0].ToString(), *Vertices[0].ToString(), *Bounds.ToString(), Parsed->Sections.Num());
            const FString ObjectId = StringField(Object, TEXT("id"));
            for (FMigrationEntry& Entry : State->Entries)
                if (Entry.Renderers.Contains(ObjectId)) Entry.Bounds += Bounds;
            Renderers.Add(StringField(Object, TEXT("id")), Component);
            OriginalTransforms.Add(StringField(Object, TEXT("id")), Actor->GetTransform());
            Spawned.Add(Actor);
            ++RendererCount;
            TriangleCount += Parsed->Indices.Num() / 3;
            RoomBounds += Bounds;
        }
    }
    const TArray<TSharedPtr<FJsonValue>>* DoorValues = nullptr;
    if (Manifest->TryGetArrayField(TEXT("doors"), DoorValues) && DoorValues)
    {
        for (const TSharedPtr<FJsonValue>& Value : *DoorValues)
        {
            const TSharedPtr<FJsonObject> DoorObject = Value->AsObject();
            if (!DoorObject.IsValid()) continue;
            FMigrationDoor Door;
            Door.Owner = StringField(DoorObject, TEXT("ownerEntryId"));
            Door.CanClose = !DoorObject->HasField(TEXT("canClose")) || DoorObject->GetBoolField(TEXT("canClose"));
            Door.Open = DoorObject->HasField(TEXT("isOpenedByDefault")) && DoorObject->GetBoolField(TEXT("isOpenedByDefault"));
            const TArray<TSharedPtr<FJsonValue>>* Inside = nullptr;
            if (DoorObject->TryGetArrayField(TEXT("insidePlaceableIds"), Inside)) for (const TSharedPtr<FJsonValue>& Id : *Inside) Door.Inside.Add(Id->AsString());
            if (DoorObject->TryGetArrayField(TEXT("rendererObjectIds"), Inside)) for (const TSharedPtr<FJsonValue>& Id : *Inside) Door.Renderers.Add(Id->AsString());
            const TArray<TSharedPtr<FJsonValue>>* Motions = nullptr;
            if (DoorObject->TryGetArrayField(TEXT("motions"), Motions))
                for (const auto& MotionValue : *Motions)
                {
                    auto MotionObject = MotionValue->AsObject();
                    FMigrationDoorMotion Motion;
                    for (const auto& FrameValue : MotionObject->GetArrayField(TEXT("frames")))
                    {
                        auto FrameObject = FrameValue->AsObject();
                        FMigrationDoorFrame Frame;
                        Frame.Time = FrameObject->GetNumberField(TEXT("time"));
                        for (const auto& RendererValue : FrameObject->GetArrayField(TEXT("renderers")))
                        {
                            auto Renderer = RendererValue->AsObject();
                            const auto& Values = Renderer->GetArrayField(TEXT("worldDelta"));
                            if (Values.Num() != 16) { ++Errors; continue; }
                            // Unity column vectors/metres -> UE row vectors/centimetres, ZXY axes.
                            const int Axes[3] = {2, 0, 1};
                            FMatrix Matrix = FMatrix::Identity;
                            for (int Row = 0; Row < 3; ++Row)
                                for (int Col = 0; Col < 3; ++Col)
                                    Matrix.M[Row][Col] = Values[Axes[Row] * 4 + Axes[Col]]->AsNumber();
                            for (int Col = 0; Col < 3; ++Col) Matrix.M[3][Col] = Values[12 + Axes[Col]]->AsNumber() * 100.0;
                            Frame.Renderers.Add(StringField(Renderer, TEXT("objectId")), FTransform(Matrix));
                        }
                        Motion.Frames.Add(MoveTemp(Frame));
                    }
                    Door.Motions.Add(StringField(MotionObject, TEXT("command")), MoveTemp(Motion));
                }
            if (Door.Open) { Door.Motion = TEXT("firstOpen"); Door.Time = 1000; Door.EverOpened = true; }
            State->Doors.Add(MoveTemp(Door));
        }
    }
    FMigrationEnvironment::Create(GetWorld(), Spawned);
    WhiteMaterial = BaseMaterial ? UMaterialInstanceDynamic::Create(BaseMaterial, this) : nullptr;
    if (WhiteMaterial)
    {
        WhiteMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.78f, 0.78f, 0.78f));
        WhiteMaterial->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.78f, 0.78f, 0.78f));
        UTexture2D* White = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
        if (White) for (const TCHAR* Name : {TEXT("Texture"), TEXT("BaseColorTexture"), TEXT("MainTexture")}) WhiteMaterial->SetTextureParameterValue(Name, White);
    }
    for (const auto& Pair : Renderers)
    {
        TArray<UMaterialInterface*>& Slots = SavedSlots.Add(Pair.Key);
        for (int32 I = 0; I < Pair.Value->GetNumMaterials(); ++I) Slots.Add(Pair.Value->GetMaterial(I));
    }
    UpdateAppearance();
    State->Menu = false;
    State->Status = FString::Printf(TEXT("Loaded %s"), Foxy ? TEXT("Foxy") : TEXT("Room"));
    UE_LOG(LogRoomMigration, Display, TEXT("ROOM_RESULT success=1 objects=%d cachedMeshes=%d renderers=%d sections=%d triangles=%d skipped=%d errors=%d bounds=%s"), ObjectCount, Cache.Num(), RendererCount, SectionCount, TriangleCount, FMath::Max(0, ObjectCount - RendererCount), Errors, *RoomBounds.ToString());
    UE_LOG(LogRoomMigration, Display, TEXT("ROOM_TEXTURE_STATS loaded=%d failed=%d textureBindings=%d colorBindings=%d materials=%d sections=%d extraSlotsBound=%d parent=%s textureParameters=%d vectorParameters=%d"), TextureLoaded, TextureFailed, TextureBound, ColorBound, OriginalMaterials.Num(), MaterialSections, ExtraMaterialSlots, *GetNameSafe(BaseMaterial), TextureParameters.Num(), ColorParameters.Num());
    if (TextureLoaded > 0 && TextureBound == 0)
        UE_LOG(LogRoomMigration, Warning, TEXT("ROOM_TEXTURE_BLOCKED: runtime parent has no supported texture parameter; color-only fallback. runtime textured parent still required for visual parity."));
    UE_LOG(LogRoomMigration, Display, TEXT("ROOM_GEOMETRY sections=%d bound=%d missingSlots=%d mirrored=%d matrixErrors=%d"), SectionCount, MaterialSections, MissingSlots, Mirrored, Errors);
    return RendererCount > 0 && Errors == 0 && MissingSlots == 0
        && BaseMaterial && TextureFailed == 0 && (TextureLoaded == 0 || TextureBound > 0);
}

void UMigrationScene::UpdateDoors(float DeltaSeconds)
{
    for (auto& Door : State->Doors)
    {
        const auto* Motion = Door.Motions.Find(Door.Motion);
        if (!Motion || Motion->Frames.IsEmpty()) continue;
        Door.Time = FMath::Min(Door.Time + DeltaSeconds, Motion->Frames.Last().Time);
        int32 B = 0;
        while (B + 1 < Motion->Frames.Num() && Motion->Frames[B].Time < Door.Time) ++B;
        int32 A = FMath::Max(0, B - 1);
        const float Span = Motion->Frames[B].Time - Motion->Frames[A].Time;
        const float Alpha = Span > 0 ? (Door.Time - Motion->Frames[A].Time) / Span : 0;
        for (const auto& Pair : Motion->Frames[B].Renderers)
        {
            auto* R = Renderers.FindRef(Pair.Key);
            if (!R) continue;
            FTransform Pose = Pair.Value;
            if (const auto* Start = Motion->Frames[A].Renderers.Find(Pair.Key)) Pose.Blend(*Start, Pair.Value, Alpha);
            R->SetWorldTransform(Pose);
        }
    }
}

void UMigrationScene::UpdateAppearance()
{
    // Resolve ownership once per renderer, not last-entry-wins. Shared geometry
    // stays white until every owner is placed; previews use separate components.
    for (const auto& Pair : Renderers)
    {
        const bool AllPlaced = State->RendererPlaced(Pair.Key);
        const auto* Slots = SavedSlots.Find(Pair.Key);
        if (!Slots) continue;
        const bool Color = AllPlaced;
        for (int32 S = 0; S < Slots->Num(); ++S)
            Pair.Value->SetMaterial(S, Color || !WhiteMaterial ? (*Slots)[S] : WhiteMaterial.Get());
    }
    State->Scroll = FMath::Clamp(State->Scroll, 0, FMath::Max(0, State->VisibleEntries().Num() - 3));
    State->Status = FString::Printf(TEXT("Placed %d/%d"), State->PlacedCount(), State->Entries.Num());
}

FBox UMigrationScene::DoorBounds(int32 Index) const
{
    FBox Bounds(ForceInit);
    for (const FString& Id : State->Doors[Index].Renderers)
        if (UProceduralMeshComponent* R = Renderers.FindRef(Id)) Bounds += R->Bounds.GetBox();
    return Bounds;
}

UWorld* UMigrationScene::GetWorld() const
{
    return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}
