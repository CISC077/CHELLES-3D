// CHELLES 3D - HUD du joueur : lit la vie, l'XP, le pouvoir et l'invasion, puis met a jour le dessin
#include "HudChelles.h"
#include "SHudChelles.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"
#include "UObject/UnrealType.h"

bool GHudChellesAffiche = true;

namespace
{
	/** Cherche une variable (C++ ou Blueprint) par son nom, sans tenir compte des majuscules. */
	FProperty* VariableHud(const UObject* Objet, std::initializer_list<const TCHAR*> Noms)
	{
		if (!Objet)
		{
			return nullptr;
		}
		for (const TCHAR* Nom : Noms)
		{
			for (TFieldIterator<FProperty> It(Objet->GetClass()); It; ++It)
			{
				if (It->GetName().Equals(Nom, ESearchCase::IgnoreCase))
				{
					return *It;
				}
			}
		}
		return nullptr;
	}

	bool LireNombreHud(const UObject* Objet, std::initializer_list<const TCHAR*> Noms, float& Valeur)
	{
		FNumericProperty* P = CastField<FNumericProperty>(VariableHud(Objet, Noms));
		if (!P)
		{
			return false;
		}
		const void* Adresse = P->ContainerPtrToValuePtr<void>(Objet);
		Valeur = P->IsFloatingPoint() ? (float)P->GetFloatingPointPropertyValue(Adresse) : (float)P->GetSignedIntPropertyValue(Adresse);
		return true;
	}

	bool LireBooleenHud(const UObject* Objet, std::initializer_list<const TCHAR*> Noms, bool& Valeur)
	{
		FBoolProperty* P = CastField<FBoolProperty>(VariableHud(Objet, Noms));
		if (!P)
		{
			return false;
		}
		Valeur = P->GetPropertyValue_InContainer(Objet);
		return true;
	}

	/** Lit un tableau de nombres (ex. FinEffet du relais des pouvoirs). */
	bool LireTableauHud(const UObject* Objet, const TCHAR* Nom, TArray<float>& Valeurs)
	{
		FArrayProperty* P = CastField<FArrayProperty>(VariableHud(Objet, { Nom }));
		FNumericProperty* Dedans = P ? CastField<FNumericProperty>(P->Inner) : nullptr;
		if (!Dedans || !Dedans->IsFloatingPoint())
		{
			return false;
		}
		FScriptArrayHelper Tableau(P, P->ContainerPtrToValuePtr<void>(Objet));
		Valeurs.Reset();
		for (int32 i = 0; i < Tableau.Num(); ++i)
		{
			Valeurs.Add((float)Dedans->GetFloatingPointPropertyValue(Tableau.GetRawPtr(i)));
		}
		return true;
	}

	/** Classe C++ d'un autre plugin, trouvee par son nom (nullptr si le plugin n'est pas charge). */
	UClass* ClasseHud(const TCHAR* Nom)
	{
		return FindObject<UClass>(ANY_PACKAGE, Nom);
	}
}

AHudChelles::AHudChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bReplicates = false;
	SetActorHiddenInGame(true);
}

void AHudChelles::BeginPlay()
{
	Super::BeginPlay();
	CreerHud();
	UE_LOG(LogTemp, Log, TEXT("[HUD] HUD Chelles pret (vie, XP, pouvoir, zombies). Console : Chelles.Hud 0 / 1."));
}

void AHudChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	RetirerHud();
	Super::EndPlay(Raison);
}

APlayerController* AHudChelles::GetPC() const
{
	UWorld* W = GetWorld();
	return W ? W->GetFirstPlayerController() : nullptr;
}

void AHudChelles::CreerHud()
{
	if (Hud.IsValid() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UWorld* W = GetWorld();
	UGameViewportClient* Vue = W ? W->GetGameViewport() : nullptr;
	if (!Vue)
	{
		return;
	}
	Hud = SNew(SHudChelles);
	// Couche 3 : sous le panneau des pouvoirs (4) et sous les menus
	Vue->AddViewportWidgetContent(Hud.ToSharedRef(), 3);
}

void AHudChelles::RetirerHud()
{
	if (!Hud.IsValid())
	{
		return;
	}
	UWorld* W = GetWorld();
	if (UGameViewportClient* Vue = W ? W->GetGameViewport() : nullptr)
	{
		Vue->RemoveViewportWidgetContent(Hud.ToSharedRef());
	}
	Hud.Reset();
}

void AHudChelles::ChercherActeurs()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	if (!Pouvoirs.IsValid())
	{
		if (UClass* C = ClasseHud(TEXT("PouvoirsChelles")))
		{
			for (TActorIterator<AActor> It(W, C); It; ++It)
			{
				Pouvoirs = *It;
				break;
			}
		}
	}
	APlayerController* PC = GetPC();
	if (PC && (!Relais.IsValid() || Relais->GetOwner() != PC))
	{
		Relais = nullptr;
		if (UClass* C = ClasseHud(TEXT("PouvoirsRelais")))
		{
			for (TActorIterator<AActor> It(W, C); It; ++It)
			{
				if (It->GetOwner() == PC)
				{
					Relais = *It;
					break;
				}
			}
		}
	}
	if (!Invasion.IsValid())
	{
		if (UClass* C = ClasseHud(TEXT("InvasionZombies")))
		{
			for (TActorIterator<AActor> It(W, C); It; ++It)
			{
				Invasion = *It;
				break;
			}
		}
	}
}

void AHudChelles::LireVie()
{
	FEtatHudChelles& E = Hud->Etat;
	APlayerController* PC = GetPC();
	APawn* Pion = PC ? PC->GetPawn() : nullptr;

	float Vie = 0.f;
	float Max = 0.f;
	E.bVie = Pion && LireNombreHud(Pion, { TEXT("HealthCurrent"), TEXT("Health"), TEXT("CurrentHealth"), TEXT("Vie") }, Vie);
	if (!E.bVie)
	{
		VieAvant = -1.f;
		PionAvant = Pion;
		return;
	}
	if (!LireNombreHud(Pion, { TEXT("HealthMax"), TEXT("MaxHealth"), TEXT("VieMax") }, Max) || Max <= 0.f)
	{
		Max = FMath::Max(100.f, Vie);
	}
	bool bMort = false;
	LireBooleenHud(Pion, { TEXT("IsDead"), TEXT("bIsDead"), TEXT("Dead") }, bMort);

	// Coup recu : la vie baisse sur le meme personnage
	if (PionAvant.Get() == Pion && VieAvant >= 0.f && Vie < VieAvant - 0.01f)
	{
		E.AgeCoup = 0.f;
	}
	VieAvant = Vie;
	PionAvant = Pion;

	E.Vie = Vie;
	E.VieMax = Max;
	E.bMort = bMort || Vie <= 0.f;
}

void AHudChelles::LirePouvoirs()
{
	FEtatHudChelles& E = Hud->Etat;
	AActor* Gestion = Pouvoirs.Get();
	AActor* Mien = Relais.Get();
	E.bPouvoirs = Gestion != nullptr || Mien != nullptr;
	if (!E.bPouvoirs)
	{
		return;
	}

	// XP : celle de ce PC (a jour tout de suite) si on peut la lire, sinon celle connue du serveur
	float Nombre = 0.f;
	if (Gestion && LireNombreHud(Gestion, { TEXT("XPLocale") }, Nombre))
	{
		E.XP = FMath::RoundToInt(Nombre);
	}
	else if (Mien && LireNombreHud(Mien, { TEXT("XP") }, Nombre))
	{
		E.XP = FMath::RoundToInt(Nombre);
	}
	E.Selection = (Gestion && LireNombreHud(Gestion, { TEXT("Selection") }, Nombre)) ? FMath::RoundToInt(Nombre) : -1;

	// Effet et recharge : heures de fin (horloge du serveur) copiees dans le relais
	for (int32 i = 0; i < ChellesHudDonnees::NombrePouvoirs; ++i)
	{
		E.Effet[i] = 0.f;
		E.Recharge[i] = 0.f;
	}
	if (!Mien)
	{
		return;
	}
	UWorld* W = GetWorld();
	const AGameStateBase* EtatJeu = W ? W->GetGameState() : nullptr;
	const float Maintenant = EtatJeu ? EtatJeu->GetServerWorldTimeSeconds() : (W ? W->GetTimeSeconds() : 0.f);
	TArray<float> Fins;
	if (LireTableauHud(Mien, TEXT("FinEffet"), Fins))
	{
		for (int32 i = 0; i < FMath::Min(Fins.Num(), ChellesHudDonnees::NombrePouvoirs); ++i)
		{
			// CAMELEON n'a pas de duree : pas de compte a rebours
			E.Effet[i] = (i == 3) ? 0.f : FMath::Max(0.f, Fins[i] - Maintenant);
		}
	}
	if (LireTableauHud(Mien, TEXT("FinRecharge"), Fins))
	{
		for (int32 i = 0; i < FMath::Min(Fins.Num(), ChellesHudDonnees::NombrePouvoirs); ++i)
		{
			E.Recharge[i] = FMath::Max(0.f, Fins[i] - Maintenant);
		}
	}
}

void AHudChelles::LireZombies()
{
	FEtatHudChelles& E = Hud->Etat;
	AActor* Chef = Invasion.Get();
	E.bZombies = Chef != nullptr;
	if (!Chef)
	{
		E.bInvasion = false;
		return;
	}
	bool bEnCours = false;
	float Nombre = 0.f;
	LireBooleenHud(Chef, { TEXT("bInvasionEnCours") }, bEnCours);
	E.bInvasion = bEnCours;
	E.ZombiesRestants = LireNombreHud(Chef, { TEXT("ZombiesRestants") }, Nombre) ? FMath::RoundToInt(Nombre) : 0;
	E.SecondesInvasion = LireNombreHud(Chef, { TEXT("SecondesRestantes") }, Nombre) ? Nombre : 0.f;
}

void AHudChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Hud.IsValid())
	{
		CreerHud();
		if (!Hud.IsValid())
		{
			return;
		}
	}
	UWorld* W = GetWorld();
	const float Reel = float(FApp::GetDeltaTime());
	FEtatHudChelles& E = Hud->Etat;
	E.Horloge += Reel;
	E.AgeCoup += Reel;

	// Pas de HUD pendant le menu, la pause, le chat ou les options (souris visible), ni sans personnage
	APlayerController* PC = GetPC();
	E.bVisible = bAfficher && GHudChellesAffiche && PC && PC->GetPawn() && !PC->IsPaused() && !PC->bShowMouseCursor;
	if (!E.bVisible)
	{
		return;
	}

	const float Horloge = W ? W->GetRealTimeSeconds() : 0.f;
	if (Horloge >= ProchaineRecherche)
	{
		ProchaineRecherche = Horloge + 1.f;
		ChercherActeurs();
	}
	LireVie();
	LirePouvoirs();
	LireZombies();
}
