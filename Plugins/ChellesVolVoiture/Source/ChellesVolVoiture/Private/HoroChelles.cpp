// CHELLES 3D - Voitures DV_Car_Horo (voir HoroChelles.h).
#include "HoroChelles.h"

#include "AnimationRuntime.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/DecalComponent.h"
#include "Components/EditableText.h"
#include "Components/InputComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InputActionDelegateBinding.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#include "VehicleWheel.h"
#include "WheeledVehicleMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogHoroChelles, Log, All);

namespace HoroOutils
{
	const FName TAG_BUGATTI(TEXT("ChellesBugatti"));
	const TCHAR* CLASSE_HORO = TEXT("BP_Car_Hatchback_Horo_C");
	const TCHAR* DOSSIER_PACK = TEXT("/Game/DV_Car_Horo/");

	void PourChaqueActeurAttache(AActor* Racine, TFunctionRef<void(AActor*)> Faire, int32 Profondeur = 0)
	{
		if (!Racine || Profondeur > 3)
		{
			return;
		}
		TArray<AActor*> Attaches;
		Racine->GetAttachedActors(Attaches);
		for (AActor* A : Attaches)
		{
			if (A && !A->IsA<APawn>()) // jamais un personnage
			{
				Faire(A);
				PourChaqueActeurAttache(A, Faire, Profondeur + 1);
			}
		}
	}
}
using namespace HoroOutils;

bool HoroChelles::EstClasseHoro(const UClass* Classe)
{
	for (const UClass* C = Classe; C; C = C->GetSuperClass())
	{
		if (C->GetFName() == FName(CLASSE_HORO))
		{
			return true;
		}
	}
	return false;
}

bool HoroChelles::EstHoro(const AActor* Acteur)
{
	return Acteur && EstClasseHoro(Acteur->GetClass());
}

int32 HoroChelles::ReglerOptions(AActor* Voiture, const TMap<FString, bool>& Options)
{
	if (!Voiture)
	{
		return 0;
	}
	int32 N = 0;
	for (const TPair<FString, bool>& Option : Options)
	{
		if (FBoolProperty* P = FindFProperty<FBoolProperty>(Voiture->GetClass(), FName(*Option.Key)))
		{
			P->SetPropertyValue_InContainer(Voiture, Option.Value);
			++N;
		}
	}
	return N;
}

void HoroChelles::CouperDeclencheursDuPack(AActor* Voiture)
{
	if (!Voiture)
	{
		return;
	}
	TArray<UPrimitiveComponent*> Composants;
	Voiture->GetComponents<UPrimitiveComponent>(Composants);
	for (UPrimitiveComponent* C : Composants)
	{
		if (!C)
		{
			continue;
		}
		const FString Nom = C->GetName();
		if (Nom.StartsWith(TEXT("TR_Enter_Vehicle")) || Nom.StartsWith(TEXT("TR_Door_")))
		{
			C->SetGenerateOverlapEvents(false);
		}
	}
}

int32 HoroChelles::EcrirePlaques(AActor* Voiture, const FString& Texte)
{
	if (!Voiture || Texte.IsEmpty())
	{
		return 0;
	}
	// Texte coupe en deux (format du pack : GAUCHE - DROITE) : "CHELLES-3D" -> "CHELLES" "-" "3D"
	FString Gauche = Texte, Droite;
	FString Separateur = TEXT(" ");
	if (Texte.Split(TEXT("-"), &Gauche, &Droite))
	{
		Separateur = TEXT("-");
	}
	int32 Changees = 0;
	auto Ecrire = [&](UWidget* W, const FString& Voulu)
	{
		if (UEditableText* E = Cast<UEditableText>(W))
		{
			if (E->GetText().ToString() != Voulu)
			{
				E->SetText(FText::FromString(Voulu));
				++Changees;
			}
		}
		else if (UTextBlock* T = Cast<UTextBlock>(W))
		{
			if (T->GetText().ToString() != Voulu)
			{
				T->SetText(FText::FromString(Voulu));
				++Changees;
			}
		}
	};
	auto TraiterActeur = [&](AActor* A)
	{
		TArray<UWidgetComponent*> Widgets;
		A->GetComponents<UWidgetComponent>(Widgets);
		for (UWidgetComponent* WC : Widgets)
		{
			UUserWidget* UW = WC ? WC->GetUserWidgetObject() : nullptr;
			if (!UW || !UW->WidgetTree || !UW->GetClass()->GetName().Contains(TEXT("Licence")))
			{
				continue;
			}
			UW->WidgetTree->ForEachWidget([&](UWidget* W)
			{
				if (!W)
				{
					return;
				}
				const FString Nom = W->GetName();
				if (Nom.EndsWith(TEXT("_L")))
				{
					Ecrire(W, Gauche);
				}
				else if (Nom.EndsWith(TEXT("_R")))
				{
					Ecrire(W, Droite);
				}
				else if (Nom.Contains(TEXT("Seperator")) || Nom.Contains(TEXT("Separator")))
				{
					Ecrire(W, Separateur);
				}
				else if (W->IsA<UEditableText>() || W->IsA<UTextBlock>())
				{
					Ecrire(W, Texte); // EditableText_Custom et tout autre texte de la plaque
				}
			});
		}
	};
	TraiterActeur(Voiture);
	PourChaqueActeurAttache(Voiture, TraiterActeur);
	return Changees;
}

FName HoroChelles::NomAction(EActionHoro::Type Action)
{
	switch (Action)
	{
	case EActionHoro::Phares:           return FName(TEXT("toggle_light"));
	case EActionHoro::Saut:             return FName(TEXT("Low_Rider"));
	case EActionHoro::Reinitialiser:    return FName(TEXT("Repair_Vehicle"));
	case EActionHoro::ClignotantGauche: return FName(TEXT("Indicators Left"));
	case EActionHoro::ClignotantDroit:  return FName(TEXT("Indicators Right"));
	case EActionHoro::Warnings:         return FName(TEXT("Hazard"));
	default:                            return NAME_None;
	}
}

bool HoroChelles::LancerActionPack(AActor* Voiture, FName Action)
{
	if (!Voiture || Action.IsNone())
	{
		return false;
	}
	// Les evenements "InputAction" d'un Blueprint sont des fonctions : la liste (action -> fonction) est rangee dans la classe.
	for (UClass* C = Voiture->GetClass(); C; C = C->GetSuperClass())
	{
		UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(C);
		if (!BPGC)
		{
			continue;
		}
		UInputActionDelegateBinding* Liste = Cast<UInputActionDelegateBinding>(
			UBlueprintGeneratedClass::GetDynamicBindingObject(BPGC, UInputActionDelegateBinding::StaticClass()));
		if (!Liste)
		{
			continue;
		}
		const FBlueprintInputActionDelegateBinding* Choix = nullptr;
		for (const FBlueprintInputActionDelegateBinding& L : Liste->InputActionDelegateBindings)
		{
			if (L.InputActionName != Action)
			{
				continue;
			}
			if (L.InputKeyEvent == IE_Pressed)
			{
				Choix = &L;
				break;
			}
			if (!Choix)
			{
				Choix = &L;
			}
		}
		if (!Choix)
		{
			continue;
		}
		UFunction* F = Voiture->FindFunction(Choix->FunctionNameToBind);
		if (!F)
		{
			continue;
		}
		const int32 Taille = FMath::Max<int32>(F->ParmsSize, 1);
		uint8* Parametres = (uint8*)FMemory_Alloca(Taille);
		FMemory::Memzero(Parametres, Taille);
		for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Parametres);
		}
		Voiture->ProcessEvent(F, Parametres);
		for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Parametres);
		}
		return true;
	}
	UE_LOG(LogHoroChelles, Verbose, TEXT("[Vol] Action %s absente de %s"), *Action.ToString(), *Voiture->GetClass()->GetName());
	return false;
}

int32 HoroChelles::RetirerLiaisons(APawn* Voiture, const TArray<FName>& Actions)
{
	UInputComponent* Entrees = Voiture ? Voiture->InputComponent : nullptr;
	if (!Entrees)
	{
		return 0;
	}
	int32 N = 0;
	for (int32 i = Entrees->GetNumActionBindings() - 1; i >= 0; --i)
	{
		if (Actions.Contains(Entrees->GetActionBinding(i).GetActionName()))
		{
			Entrees->RemoveActionBinding(i);
			++N;
		}
	}
	return N;
}

void HoroChelles::RetirerAffichagesDuPack(UWorld* Monde)
{
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* W = *It;
		if (W && W->GetWorld() == Monde && W->IsInViewport() && W->GetClass()->GetPathName().StartsWith(DOSSIER_PACK))
		{
			W->RemoveFromParent();
		}
	}
}

void HoroChelles::RemettreSurRoues(APawn* Voiture)
{
	if (!Voiture)
	{
		return;
	}
	const FRotator Droit(0.f, Voiture->GetActorRotation().Yaw, 0.f);
	Voiture->SetActorLocationAndRotation(Voiture->GetActorLocation() + FVector(0.f, 0.f, 80.f), Droit, false, nullptr,
		ETeleportType::TeleportPhysics);
	if (UPrimitiveComponent* Corps = Cast<UPrimitiveComponent>(Voiture->GetRootComponent()))
	{
		Corps->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Corps->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	}
}

// =====================================================================================================
// Bugatti
// =====================================================================================================

bool HoroChelles::EstHabilleeBugatti(const AActor* Voiture)
{
	if (!Voiture)
	{
		return false;
	}
	TArray<UActorComponent*> Composants;
	Voiture->GetComponents(Composants);
	for (const UActorComponent* C : Composants)
	{
		if (C && C->ComponentHasTag(TAG_BUGATTI))
		{
			return true;
		}
	}
	return false;
}

bool HoroChelles::HabillerBugatti(APawn* Voiture, const FHabillageBugatti& H)
{
	if (!Voiture || !H.EstPret() || EstHabilleeBugatti(Voiture))
	{
		return false;
	}
	USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(Voiture->GetRootComponent());
	UWheeledVehicleMovementComponent* Mouvement = Voiture->FindComponentByClass<UWheeledVehicleMovementComponent>();
	if (!Mesh || !Mesh->SkeletalMesh || !Mouvement || Mouvement->WheelSetups.Num() < 4)
	{
		return false;
	}
	const FReferenceSkeleton& Squelette = Mesh->SkeletalMesh->RefSkeleton;

	// 1) Ou sont les 4 roues de la Horo (pose de repos, repere du mesh) et quelle taille font-elles ?
	struct FRoueHoro { FName Os; FTransform Repos; float Rayon = 0.f; };
	TArray<FRoueHoro> RouesHoro;
	for (int32 i = 0; i < Mouvement->WheelSetups.Num(); ++i)
	{
		const FName Os = Mouvement->WheelSetups[i].BoneName;
		const int32 Index = Squelette.FindBoneIndex(Os);
		if (Index == INDEX_NONE)
		{
			continue;
		}
		FRoueHoro R;
		R.Os = Os;
		R.Repos = FAnimationRuntime::GetComponentSpaceTransformRefPose(Squelette, Index);
		if (Mouvement->Wheels.IsValidIndex(i) && Mouvement->Wheels[i])
		{
			R.Rayon = Mouvement->Wheels[i]->ShapeRadius;
		}
		else if (UClass* CR = Mouvement->WheelSetups[i].WheelClass)
		{
			R.Rayon = CR->GetDefaultObject<UVehicleWheel>()->ShapeRadius;
		}
		RouesHoro.Add(R);
	}
	if (RouesHoro.Num() < 4)
	{
		return false;
	}
	// Ranger : avant = X le plus grand, gauche = Y negatif
	float XMilieu = 0.f;
	for (const FRoueHoro& R : RouesHoro)
	{
		XMilieu += R.Repos.GetLocation().X / RouesHoro.Num();
	}
	const FRoueHoro* Place[4] = { nullptr, nullptr, nullptr, nullptr };
	for (const FRoueHoro& R : RouesHoro)
	{
		const FVector L = R.Repos.GetLocation();
		const int32 k = (L.X >= XMilieu ? 0 : 2) + (L.Y < 0.f ? 0 : 1);
		if (!Place[k])
		{
			Place[k] = &R;
		}
	}
	for (int32 k = 0; k < 4; ++k)
	{
		if (!Place[k])
		{
			UE_LOG(LogHoroChelles, Warning, TEXT("[Vol] Bugatti : roues de %s mal reconnues."), *Voiture->GetName());
			return false;
		}
	}
	auto PosHoro = [&](int32 k) { return Place[k]->Repos.GetLocation(); };

	// 2) Calage : les roues de la Bugatti tombent sur celles de la Horo (longueur et largeur), hauteur comme la longueur
	const float EmpHoro = (PosHoro(0).X + PosHoro(1).X - PosHoro(2).X - PosHoro(3).X) * 0.5f;
	const float EmpBug = (H.Centres[0].X + H.Centres[1].X - H.Centres[2].X - H.Centres[3].X) * 0.5f;
	const float VoieHoro = (PosHoro(1).Y + PosHoro(3).Y - PosHoro(0).Y - PosHoro(2).Y) * 0.5f;
	const float VoieBug = (H.Centres[1].Y + H.Centres[3].Y - H.Centres[0].Y - H.Centres[2].Y) * 0.5f;
	if (FMath::Abs(EmpBug) < 1.f || FMath::Abs(VoieBug) < 1.f)
	{
		return false;
	}
	const float SX = FMath::Clamp(EmpHoro / EmpBug, 0.3f, 3.f);
	const float SY = FMath::Clamp(VoieHoro / VoieBug, 0.3f, 3.f) * H.EchelleLargeur;
	const float SZ = SX * H.EchelleHauteur;
	const FVector Echelle(SX, SY, SZ);
	FVector MilieuHoro = FVector::ZeroVector, MilieuBug = FVector::ZeroVector;
	for (int32 k = 0; k < 4; ++k)
	{
		MilieuHoro += PosHoro(k) * 0.25f;
		MilieuBug += H.Centres[k] * 0.25f;
	}
	const FVector Origine = MilieuHoro - MilieuBug * Echelle + H.Decalage;

	// 3) Cacher la Horo (la physique et les os continuent : les roues tournent toujours)
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	TArray<USceneComponent*> Composants;
	Voiture->GetComponents<USceneComponent>(Composants);
	for (USceneComponent* C : Composants)
	{
		if (C && (C->IsA<UMeshComponent>() || C->IsA<UDecalComponent>() || C->IsA<UTextRenderComponent>()))
		{
			C->SetHiddenInGame(true, false);
		}
	}
	PourChaqueActeurAttache(Voiture, [](AActor* A) { A->SetActorHiddenInGame(true); });

	auto NouveauMesh = [&](UStaticMesh* Modele, const TCHAR* Nom) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Voiture, MakeUniqueObjectName(Voiture, UStaticMeshComponent::StaticClass(), Nom));
		C->SetStaticMesh(Modele);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->SetMobility(EComponentMobility::Movable);
		C->ComponentTags.Add(TAG_BUGATTI);
		return C;
	};

	// 4) Carrosserie
	UStaticMeshComponent* Corps = NouveauMesh(H.Carrosserie, TEXT("BugattiCarrosserie"));
	Corps->SetupAttachment(Mesh);
	Corps->SetRelativeTransform(FTransform(FRotator(0.f, H.Lacet, 0.f), Origine, Echelle));
	Corps->RegisterComponent();

	// 5) Roues accrochees aux os des roues (elles tournent et braquent avec la Horo)
	for (int32 k = 0; k < 4; ++k)
	{
		UStaticMesh* Modele = H.Roues[k];
		if (!Modele)
		{
			continue;
		}
		const float RayonBug = k < 2 ? H.RayonAvant : H.RayonArriere;
		const float SR = (Place[k]->Rayon > 1.f && RayonBug > 1.f) ? Place[k]->Rayon / RayonBug : SX;
		const FTransform Voulu(FQuat::Identity, PosHoro(k), FVector(SR, SY, SR));
		UStaticMeshComponent* Roue = NouveauMesh(Modele, TEXT("BugattiRoue"));
		Roue->SetupAttachment(Mesh, Place[k]->Os);
		Roue->SetRelativeTransform(Voulu.GetRelativeTransform(Place[k]->Repos));
		Roue->RegisterComponent();
	}

	// 6) Plaques blanches CHELLES 3D devant (sur le logo de la calandre) et derriere
	FBox Boite = H.Carrosserie->GetBoundingBox();
	const FVector Min = Origine + Boite.Min * Echelle;
	const FVector Max = Origine + Boite.Max * Echelle;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	for (int32 Face = 0; Face < 2; ++Face)
	{
		const bool bAvant = Face == 0;
		const FVector& Voulu = bAvant ? H.PlaqueAvant : H.PlaqueArriere;
		FVector Centre;
		if (!Voulu.IsNearlyZero())
		{
			// Place donnee dans la Bugatti (devant le logo) : meme calage que la carrosserie
			Centre = Origine + Voulu * Echelle;
		}
		else
		{
			const float X = bAvant ? Max.X - 6.f : Min.X + 10.f;
			const float Z = Min.Z + (Max.Z - Min.Z) * (bAvant ? 0.22f : 0.30f);
			Centre = FVector(X, (Min.Y + Max.Y) * 0.5f, Z);
		}
		if (Cube)
		{
			UStaticMeshComponent* Fond = NouveauMesh(Cube, TEXT("BugattiPlaque"));
			Fond->SetupAttachment(Mesh);
			Fond->SetRelativeTransform(FTransform(FRotator::ZeroRotator, Centre, FVector(0.01f, 0.52f, 0.12f)));
			Fond->SetCastShadow(false);
			Fond->RegisterComponent();
			if (UMaterialInstanceDynamic* Mat = Fond->CreateDynamicMaterialInstance(0))
			{
				Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.95f, 0.95f));
			}
		}
		UTextRenderComponent* Texte = NewObject<UTextRenderComponent>(Voiture, MakeUniqueObjectName(Voiture, UTextRenderComponent::StaticClass(), TEXT("BugattiTexte")));
		Texte->SetupAttachment(Mesh);
		Texte->SetText(FText::FromString(H.TextePlaque));
		Texte->SetTextRenderColor(FColor(15, 15, 25));
		Texte->SetHorizontalAlignment(EHTA_Center);
		Texte->SetVerticalAlignment(EVRTA_TextCenter);
		Texte->SetWorldSize(8.5f);
		Texte->SetCastShadow(false);
		Texte->ComponentTags.Add(TAG_BUGATTI);
		Texte->SetRelativeLocationAndRotation(Centre + FVector(bAvant ? 0.8f : -0.8f, 0.f, 0.f), FRotator(0.f, bAvant ? 0.f : 180.f, 0.f));
		Texte->RegisterComponent();
	}
	UE_LOG(LogHoroChelles, Log, TEXT("[Vol] %s habillee en Bugatti (echelle %.2f x %.2f x %.2f)."), *Voiture->GetName(), SX, SY, SZ);
	return true;
}
