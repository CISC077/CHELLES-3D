// CHELLES 3D - Coffres d'armes du kit FPS_Game (25/11/2026, agent Armements) - UE 4.27
#include "CoffresFPS.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "UObject/UnrealType.h"

namespace
{
	/** Classes reconnues comme coffres d'armes du kit (le debut du nom de classe suffit). */
	bool EstCoffreDuKit(const AActor* A)
	{
		if (!A)
		{
			return false;
		}
		const FString Classe = A->GetClass()->GetName();
		return Classe.StartsWith(TEXT("BP_Chest")) || Classe.StartsWith(TEXT("Replicated_Chest")) || Classe.StartsWith(TEXT("BP_Loot_Chest"));
	}

	/** L'evenement d'ouverture du coffre (interface BPI_Interact, ou BPI_Interact_MP en multijoueur). */
	UFunction* EvenementOuverture(AActor* Coffre)
	{
		static const FName Noms[] = { FName(TEXT("BPI_Interaction")), FName(TEXT("Interact_MP_Server")), FName(TEXT("Interact_MP")) };
		for (const FName& Nom : Noms)
		{
			if (UFunction* F = Coffre->FindFunction(Nom))
			{
				return F;
			}
		}
		return nullptr;
	}
}

ACoffresFPS::ACoffresFPS()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicates(false);
}

void ACoffresFPS::BeginPlay()
{
	Super::BeginPlay();
	Recenser();
}

void ACoffresFPS::Recenser()
{
	Coffres.Reset();
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		if (EstCoffreDuKit(*It))
		{
			Coffres.Add(*It);
		}
	}
	if (!bJournalFait && W->GetTimeSeconds() > 3.f)
	{
		bJournalFait = true;
		int32 SansEvenement = 0;
		for (const TWeakObjectPtr<AActor>& C : Coffres)
		{
			if (C.IsValid() && !EvenementOuverture(C.Get()))
			{
				++SansEvenement;
				UE_LOG(LogTemp, Warning, TEXT("[CoffresFPS] %s (%s) : pas d'evenement BPI_Interaction, impossible de l'ouvrir."), *C->GetName(), *C->GetClass()->GetName());
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[CoffresFPS] Pret : %d coffre(s) d'armes du kit dans la carte (%d sans evenement). E a moins de 3 m = ouvrir."), Coffres.Num(), SansEvenement);
	}
}

APlayerController* ACoffresFPS::GetPC() const
{
	UWorld* W = GetWorld();
	return W ? W->GetFirstPlayerController() : nullptr;
}

AActor* ACoffresFPS::PlusProche(const APawn* Pion, float DistanceMax) const
{
	AActor* Meilleur = nullptr;
	float MeilleureDist = DistanceMax;
	for (const TWeakObjectPtr<AActor>& C : Coffres)
	{
		if (!C.IsValid() || Ouverts.Contains(C))
		{
			continue;
		}
		const float D = FVector::Dist(C->GetActorLocation(), Pion->GetActorLocation());
		if (D < MeilleureDist)
		{
			MeilleureDist = D;
			Meilleur = C.Get();
		}
	}
	return Meilleur;
}

bool ACoffresFPS::Ouvrir(AActor* Coffre, APawn* Pion, const TCHAR* Pourquoi)
{
	UFunction* F = Coffre ? EvenementOuverture(Coffre) : nullptr;
	if (!F)
	{
		return false;
	}
	// Parametres de l'evenement : le personnage (CharacterReference) s'il est du bon type, sinon vide.
	uint8* Parametres = (uint8*)FMemory_Alloca(FMath::Max<int32>(F->ParmsSize, 1));
	FMemory::Memzero(Parametres, FMath::Max<int32>(F->ParmsSize, 1));
	for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->InitializeValue_InContainer(Parametres);
		if (FObjectPropertyBase* Objet = CastField<FObjectPropertyBase>(*It))
		{
			if (Pion && Objet->PropertyClass && Pion->IsA(Objet->PropertyClass))
			{
				Objet->SetObjectPropertyValue_InContainer(Parametres, Pion);
			}
		}
	}
	Coffre->ProcessEvent(F, Parametres);
	for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(Parametres);
	}
	Ouverts.Add(Coffre);
	UE_LOG(LogTemp, Log, TEXT("[CoffresFPS] %s ouvert (%s, joueur %s)."), *Coffre->GetName(), Pourquoi, Pion ? *Pion->GetClass()->GetName() : TEXT("aucun"));
	return true;
}

void ACoffresFPS::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	const float Maintenant = W->GetTimeSeconds();
	if (Maintenant >= ProchainRecensement)
	{
		ProchainRecensement = Maintenant + 5.f; // les coffres peuvent apparaitre apres le debut (streaming)
		Recenser();
	}
	if (GetNetMode() == NM_Client || Coffres.Num() == 0)
	{
		return; // le coffre du kit n'est pas replique : seul le solo / l'hote peut l'ouvrir
	}
	APlayerController* PC = GetPC();
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (!Pion || PC->bShowMouseCursor) // menu ouvert (ARMEMENTS, pause...) : on ne touche a rien
	{
		return;
	}
	if (PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Left))
	{
		if (AActor* Coffre = PlusProche(Pion, 300.f))
		{
			Ouvrir(Coffre, Pion, TEXT("touche E"));
		}
	}
}

void ACoffresFPS::Commande(const TArray<FString>& Arguments)
{
	const FString Quoi = Arguments.Num() > 0 ? Arguments[0] : TEXT("Liste");
	Recenser();
	APlayerController* PC = GetPC();
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (Quoi.Equals(TEXT("Ouvrir"), ESearchCase::IgnoreCase))
	{
		AActor* Coffre = Pion ? PlusProche(Pion, 1.0e8f) : nullptr;
		if (!Coffre || !Ouvrir(Coffre, Pion, TEXT("console")))
		{
			UE_LOG(LogTemp, Warning, TEXT("[CoffresFPS] Aucun coffre a ouvrir."));
		}
		return;
	}
	for (const TWeakObjectPtr<AActor>& C : Coffres)
	{
		if (C.IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("[CoffresFPS] %s  %s  a %.0f m  %s  evenement %s"), *C->GetName(), *C->GetActorLocation().ToString(),
				Pion ? FVector::Dist(C->GetActorLocation(), Pion->GetActorLocation()) / 100.f : 0.f,
				Ouverts.Contains(C) ? TEXT("OUVERT") : TEXT("ferme"), EvenementOuverture(C.Get()) ? TEXT("oui") : TEXT("NON"));
		}
	}
}
