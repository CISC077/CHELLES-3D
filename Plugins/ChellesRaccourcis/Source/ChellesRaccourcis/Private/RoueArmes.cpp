// CHELLES 3D - Roue des ARMES sur TAB (voir RoueArmes.h)
#include "RoueArmes.h"

#include "Components/InputComponent.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RaccourcisTypes.h"
#include "SRoueArmes.h"
#include "UObject/UnrealType.h"

// Espace de noms nomme (pas anonyme) : le plugin peut etre compile en "unity build" avec RaccourcisF.cpp.
namespace RoueArmesOutils
{
	const FLinearColor GrisBleu(0.45f, 0.62f, 0.85f);

	/** Armes par defaut (tant que le tableau DT_Armes n'existe pas). Meme contenu que Tools\RaccourcisF\Armes.csv. */
	struct FDefaut
	{
		const TCHAR* Nom;
		const TCHAR* Classe;   // nom du Blueprint dans /Game/FPS_Game/Blueprints/Weapons (vide = arsenal)
		const TCHAR* Arsenal;
		const TCHAR* Categorie;
		FLinearColor Couleur;
	};
	const FDefaut Defauts[] = {
		{ TEXT("PISTOLET"),        TEXT("BPC_WeaponPistol"),     TEXT(""), TEXT("Pistolet"),  FLinearColor(0.f, 0.f, 0.f, 0.f) },
		{ TEXT("PISTOLET AUTO"),   TEXT("BPC_WeaponPistolAuto"), TEXT(""), TEXT("Pistolet"),  FLinearColor(0.f, 0.f, 0.f, 0.f) },
		{ TEXT("FUSIL D'ASSAUT"),  TEXT("BPC_WeaponRifle"),      TEXT(""), TEXT("Fusil"),     FLinearColor(0.f, 0.f, 0.f, 0.f) },
		{ TEXT("FUSIL A POMPE"),   TEXT("BPC_WeaponShotgun"),    TEXT(""), TEXT("Pompe"),     FLinearColor(0.f, 0.f, 0.f, 0.f) },
		{ TEXT("SNIPER"),          TEXT("BPC_WeaponSniper"),     TEXT(""), TEXT("Precision"), FLinearColor(0.f, 0.f, 0.f, 0.f) },
		{ TEXT("LANCE-ROQUETTES"), TEXT("BPC_WeaponLauncher"),   TEXT(""), TEXT("Lourde"),    FLinearColor(0.f, 0.f, 0.f, 0.f) },
		{ TEXT("EPEE"),            TEXT("BPC_WeaponSword"),      TEXT(""), TEXT("Melee"),     FLinearColor(0.f, 0.f, 0.f, 0.f) },
		{ TEXT("RAYON"),           TEXT("BPC_WeaponBeam"),       TEXT(""), TEXT("Energie"),   FLinearColor(0.f, 0.f, 0.f, 0.f) },
		{ TEXT("LASER"),           TEXT("BPC_WeaponLaser_New"),  TEXT(""), TEXT("Energie"),   FLinearColor(0.f, 0.f, 0.f, 0.f) },
		{ TEXT("FUSIL RECUL"),     TEXT("BPC_WeaponRecoil"),     TEXT(""), TEXT("Fusil"),     FLinearColor(0.f, 0.f, 0.f, 0.f) },
		{ TEXT("BLASTER"),         TEXT(""), TEXT("Blaster"),      TEXT("Energie"), FLinearColor(1.f, 0.12f, 0.08f) },
		{ TEXT("BAZOOKA"),         TEXT(""), TEXT("Bazooka"),      TEXT("Lourde"),  FLinearColor(1.f, 0.55f, 0.1f) },
		{ TEXT("SABRE LUMINEUX"),  TEXT(""), TEXT("Sabre"),        TEXT("Melee"),   FLinearColor(0.1f, 0.9f, 1.f) },
		{ TEXT("GRENADE"),         TEXT(""), TEXT("Grenade"),      TEXT("Lancer"),  FLinearColor(0.3f, 1.f, 0.3f) },
		{ TEXT("LANCE-FLAMMES"),   TEXT(""), TEXT("LanceFlammes"), TEXT("Lourde"),  FLinearColor(1.f, 0.35f, 0.f) },
		{ TEXT("MITRAILLEUSE"),    TEXT(""), TEXT("Mitrailleuse"), TEXT("Lourde"),  FLinearColor(1.f, 0.9f, 0.2f) },
	};

	bool DeLaClasse(const UObject* Objet, const TCHAR* NomClasse)
	{
		for (UClass* C = Objet ? Objet->GetClass() : nullptr; C; C = C->GetSuperClass())
		{
			if (C->GetName() == NomClasse)
			{
				return true;
			}
		}
		return false;
	}

	FArrayProperty* Inventaire(UClass* Classe)
	{
		for (const TCHAR* Nom : { TEXT("UT_Inventory"), TEXT("Weapons") })
		{
			if (FArrayProperty* P = FindFProperty<FArrayProperty>(Classe, FName(Nom)))
			{
				if (CastField<FObjectPropertyBase>(P->Inner))
				{
					return P;
				}
			}
		}
		return nullptr;
	}

	bool Soldat(APawn* Pion)
	{
		return Pion && FindFProperty<FIntProperty>(Pion->GetClass(), TEXT("SelectedWeaponIndex"))
			&& Pion->FindFunction(TEXT("EquipWeapon")) && Inventaire(Pion->GetClass());
	}

	/** Armes ramassees par le soldat (cases de UT_Inventory, nulles comprises). */
	TArray<UObject*> ArmesDuSoldat(APawn* Pion)
	{
		TArray<UObject*> Liste;
		FArrayProperty* Tableau = Pion ? Inventaire(Pion->GetClass()) : nullptr;
		if (!Tableau)
		{
			return Liste;
		}
		FObjectPropertyBase* Interieur = CastField<FObjectPropertyBase>(Tableau->Inner);
		FScriptArrayHelper Cases(Tableau, Tableau->ContainerPtrToValuePtr<void>(Pion));
		for (int32 i = 0; i < Cases.Num(); ++i)
		{
			Liste.Add(Interieur->GetObjectPropertyValue(Cases.GetRawPtr(i)));
		}
		return Liste;
	}

	int32 CaseEnMain(APawn* Pion)
	{
		FIntProperty* P = Pion ? FindFProperty<FIntProperty>(Pion->GetClass(), TEXT("SelectedWeaponIndex")) : nullptr;
		return P ? P->GetPropertyValue_InContainer(Pion) : INDEX_NONE;
	}

	/** "BPC_Weapon_Pistol_C" et "BPC_WeaponPistol" -> "bpcweaponpistol" (versions reseau et solo du kit). */
	FString NomSimple(const FString& Nom)
	{
		FString S = Nom;
		S.RemoveFromEnd(TEXT("_C"));
		S = S.Replace(TEXT("_"), TEXT("")).ToLower();
		return S;
	}

	bool AppelerSansParametre(UObject* Objet, const TCHAR* Nom)
	{
		UFunction* F = Objet ? Objet->FindFunction(FName(Nom)) : nullptr;
		if (!F)
		{
			return false;
		}
		uint8* Parametres = static_cast<uint8*>(FMemory_Alloca(FMath::Max<int32>(1, F->ParmsSize)));
		FMemory::Memzero(Parametres, FMath::Max<int32>(1, F->ParmsSize));
		for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Parametres);
		}
		Objet->ProcessEvent(F, F->ParmsSize > 0 ? Parametres : nullptr);
		for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Parametres);
		}
		return true;
	}

	bool LireNombre(const UObject* Objet, const TCHAR* Nom, int64& Valeur)
	{
		FNumericProperty* P = Objet ? FindFProperty<FNumericProperty>(Objet->GetClass(), FName(Nom)) : nullptr;
		if (!P)
		{
			return false;
		}
		const void* V = P->ContainerPtrToValuePtr<void>(Objet);
		Valeur = P->IsFloatingPoint() ? int64(FMath::RoundToInt(float(P->GetFloatingPointPropertyValue(V)))) : P->GetSignedIntPropertyValue(V);
		return true;
	}

	bool LireOuiNon(const UObject* Objet, const TCHAR* Nom)
	{
		FBoolProperty* P = Objet ? FindFProperty<FBoolProperty>(Objet->GetClass(), FName(Nom)) : nullptr;
		return P && P->GetPropertyValue_InContainer(Objet);
	}

	FText NomDeLArme(const UObject* Arme)
	{
		if (!Arme)
		{
			return FText::GetEmpty();
		}
		for (const TCHAR* Champ : { TEXT("WeaponName"), TEXT("Weapon Name"), TEXT("DisplayName"), TEXT("ItemName") })
		{
			if (FTextProperty* T = FindFProperty<FTextProperty>(Arme->GetClass(), FName(Champ)))
			{
				const FText Valeur = T->GetPropertyValue_InContainer(Arme);
				if (!Valeur.IsEmpty())
				{
					return Valeur;
				}
			}
			if (FStrProperty* S = FindFProperty<FStrProperty>(Arme->GetClass(), FName(Champ)))
			{
				const FString Valeur = S->GetPropertyValue_InContainer(Arme);
				if (!Valeur.IsEmpty())
				{
					return FText::FromString(Valeur);
				}
			}
		}
		FString Nom = Arme->GetClass()->GetName();
		Nom.RemoveFromEnd(TEXT("_C"));
		Nom.RemoveFromStart(TEXT("BPC_"));
		Nom.RemoveFromStart(TEXT("BP_"));
		Nom.RemoveFromStart(TEXT("Weapon_"));
		Nom.RemoveFromStart(TEXT("Weapon"));
		return FText::FromString(Nom.Replace(TEXT("_"), TEXT(" ")).ToUpper());
	}

	UTexture2D* ImageDeLArme(const UObject* Arme)
	{
		for (const TCHAR* Champ : { TEXT("WeaponImage"), TEXT("Weapon Image"), TEXT("Icon"), TEXT("Image") })
		{
			if (FObjectPropertyBase* P = Arme ? FindFProperty<FObjectPropertyBase>(Arme->GetClass(), FName(Champ)) : nullptr)
			{
				if (UTexture2D* T = Cast<UTexture2D>(P->GetObjectPropertyValue_InContainer(Arme)))
				{
					return T;
				}
			}
		}
		return nullptr;
	}

	FText TexteMunitions(const UObject* Arme, bool bModele = false)
	{
		if (!Arme)
		{
			return FText::FromString(TEXT("0 / 0"));
		}
		if (LireOuiNon(Arme, TEXT("InfiniteAmmo")))
		{
			return FText::FromString(TEXT("\u221E")); // infini
		}
		int64 Chargeur = 0;
		int64 Reserve = 0;
		const bool bChargeur = LireNombre(Arme, TEXT("AmmoCurrent"), Chargeur);
		const bool bReserve = LireNombre(Arme, TEXT("AmmoInReserve"), Reserve);
		if (bModele)
		{
			// Arme pas encore en main : munitions qu'elle aura une fois donnee (chargeur plein + reserve pleine)
			int64 Plein = 0;
			if (Chargeur <= 0 && LireNombre(Arme, TEXT("AmmoMax"), Plein))
			{
				Chargeur = Plein;
			}
			if (Reserve <= 0 && LireNombre(Arme, TEXT("AmmoInReserve_Max"), Plein))
			{
				Reserve = Plein;
			}
		}
		if (!bChargeur && !bReserve)
		{
			return FText::GetEmpty(); // arme sans munitions (epee...)
		}
		return FText::FromString(FString::Printf(TEXT("%lld / %lld"), Chargeur, Reserve));
	}

	TSharedPtr<FSlateBrush> Pinceau(UTexture2D* Image)
	{
		if (!Image)
		{
			return nullptr;
		}
		TSharedPtr<FSlateBrush> B = MakeShared<FSlateBrush>();
		B->SetResourceObject(Image);
		B->DrawAs = ESlateBrushDrawType::Image;
		const float X = float(Image->GetSizeX());
		const float Y = float(Image->GetSizeY());
		B->ImageSize = (X > 1.f && Y > 1.f) ? FVector2D(X, Y) : FVector2D(256.f, 256.f);
		return B;
	}
}

using namespace RoueArmesOutils;

// =====================================================================================
//  Construction, reglages, tableau de donnees
// =====================================================================================

ARoueArmes::ARoueArmes()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorHiddenInGame(true);
	bReplicates = false; // une par machine (ecran du joueur)
	InputPriority = 25;  // passe avant l'ancienne roue des modes (20), le personnage et les vehicules
	Touche = EKeys::Tab;
}

void ARoueArmes::BeginPlay()
{
	Super::BeginPlay();
	ChargerReglages();
	LierClavier();
}

void ARoueArmes::EndPlay(const EEndPlayReason::Type Raison)
{
	Fermer();
	Super::EndPlay(Raison);
}

void ARoueArmes::ChargerReglages()
{
	bActif = true;
	Touche = EKeys::Tab;
	Ralenti = 0.3f;
	Sensibilite = 8.f;
	bToutesLesArmes = true;
	bDonnerToutes = true; // 2026-11-21 : MERIDJA veut des armes utilisables tout de suite dans la roue
	bAnimation = true;
	CheminTableau = TEXT("/Game/Chelles/Donnees/DT_Armes.DT_Armes");

	const FString Fichier = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesRaccourcis"), TEXT("Raccourcis.txt"));
	TArray<FString> Lignes;
	FFileHelper::LoadFileToStringArray(Lignes, *Fichier);
	for (FString Ligne : Lignes)
	{
		Ligne.TrimStartAndEndInline();
		FString Cle, Valeur;
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")) || !Ligne.Split(TEXT("="), &Cle, &Valeur))
		{
			continue;
		}
		Cle.TrimStartAndEndInline();
		Valeur.TrimStartAndEndInline();
		const bool bOui = Valeur.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || Valeur == TEXT("1");
		const float Nombre = FCString::Atof(*Valeur.Replace(TEXT(","), TEXT(".")));
		if (Cle.Equals(TEXT("RoueArmes"), ESearchCase::IgnoreCase))
		{
			bActif = bOui;
		}
		else if (Cle.Equals(TEXT("ToucheRoueArmes"), ESearchCase::IgnoreCase))
		{
			const FKey K(*Valeur);
			if (K.IsValid())
			{
				Touche = K;
			}
		}
		else if (Cle.Equals(TEXT("TableauArmes"), ESearchCase::IgnoreCase))
		{
			CheminTableau = Valeur;
		}
		else if (Cle.Equals(TEXT("RoueArmesRalenti"), ESearchCase::IgnoreCase))
		{
			Ralenti = FMath::Clamp(Nombre, 0.05f, 1.f);
		}
		else if (Cle.Equals(TEXT("RoueArmesSensibilite"), ESearchCase::IgnoreCase))
		{
			Sensibilite = FMath::Clamp(Nombre, 0.5f, 50.f);
		}
		else if (Cle.Equals(TEXT("ToutesLesArmes"), ESearchCase::IgnoreCase))
		{
			bToutesLesArmes = bOui;
		}
		else if (Cle.Equals(TEXT("DonnerLesArmes"), ESearchCase::IgnoreCase))
		{
			bDonnerToutes = bOui;
		}
		else if (Cle.Equals(TEXT("Animation"), ESearchCase::IgnoreCase))
		{
			bAnimation = bOui;
		}
	}
	ChargerTableau();
	UE_LOG(LogTemp, Log, TEXT("[RoueArmes] Pret (%s, touche %s, %d armes lues dans %s)."),
		bActif ? TEXT("active") : TEXT("coupee"), *Touche.ToString(), Armes.Num(),
		bDepuisTableau ? *CheminTableau : TEXT("la liste par defaut (tableau DT_Armes absent)"));
}

void ARoueArmes::ListeParDefaut()
{
	Armes.Reset();
	for (const FDefaut& D : Defauts)
	{
		FArmeDeLaRoue A;
		A.Nom = FText::FromString(D.Nom);
		if (*D.Classe)
		{
			A.Classe = TSoftClassPtr<AActor>(FSoftObjectPath(FString::Printf(TEXT("/Game/FPS_Game/Blueprints/Weapons/%s.%s_C"), D.Classe, D.Classe)));
		}
		A.Arsenal = D.Arsenal;
		A.Categorie = D.Categorie;
		A.Couleur = D.Couleur;
		Armes.Add(A);
	}
}

void ARoueArmes::ChargerTableau()
{
	bDepuisTableau = false;
	ListeParDefaut();
	if (CheminTableau.IsEmpty())
	{
		return;
	}
	UDataTable* Tableau = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *CheminTableau, nullptr, LOAD_NoWarn | LOAD_Quiet));
	if (!Tableau)
	{
		return;
	}
	if (Tableau->GetRowStruct() != FChellesArmeLigne::StaticStruct())
	{
		UE_LOG(LogTemp, Warning, TEXT("[RoueArmes] %s n'utilise pas la structure ChellesArmeLigne : ignore."), *CheminTableau);
		return;
	}
	TArray<FArmeDeLaRoue> Lignes;
	for (const TPair<FName, uint8*>& Paire : Tableau->GetRowMap()) // dans l'ordre du tableau
	{
		const FChellesArmeLigne* L = reinterpret_cast<const FChellesArmeLigne*>(Paire.Value);
		if (!L)
		{
			continue;
		}
		if (L->ClasseArme.IsNull() && L->Arsenal.TrimStartAndEnd().IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[RoueArmes] Ligne %s : ni ClasseArme ni Arsenal, ignoree."), *Paire.Key.ToString());
			continue;
		}
		FArmeDeLaRoue A;
		A.Nom = L->Nom.IsEmpty() ? FText::FromName(Paire.Key) : L->Nom;
		A.Classe = L->ClasseArme;
		A.Arsenal = L->Arsenal.TrimStartAndEnd();
		A.Icone = L->Icone;
		A.Couleur = L->Couleur;
		A.Categorie = L->Categorie;
		A.bDonner = L->bDonner;
		if (L->Nom.IsEmpty() && !A.Classe.IsNull())
		{
			A.Nom = FText::GetEmpty(); // le nom viendra de l'arme (WeaponName)
		}
		Lignes.Add(A);
	}
	if (Lignes.Num() > 0)
	{
		Armes = MoveTemp(Lignes);
		bDepuisTableau = true;
	}
}

APlayerController* ARoueArmes::GetPC() const
{
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	return (PC && PC->IsLocalController()) ? PC : nullptr;
}

void ARoueArmes::Message(const FString& Texte) const
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(int32(0xC4E115), 3.f, FColor(160, 200, 255), Texte);
	}
}

// =====================================================================================
//  Clavier : TAB (repris a l'ancienne roue des modes)
// =====================================================================================

void ARoueArmes::LierClavier()
{
	if (bLieClavier)
	{
		return;
	}
	if (APlayerController* PC = GetPC())
	{
		if (APlayerController* Ancien = PCLie.Get())
		{
			DisableInput(Ancien);
		}
		if (InputComponent)
		{
			InputComponent->KeyBindings.Reset();
		}
		EnableInput(PC);
		if (InputComponent && Touche.IsValid())
		{
			InputComponent->BindKey(Touche, IE_Pressed, this, &ARoueArmes::Ouvrir);
			InputComponent->BindKey(Touche, IE_Released, this, &ARoueArmes::Relacher);
			bLieClavier = true;
			PCLie = PC;
			UE_LOG(LogTemp, Log, TEXT("[RoueArmes] Maintenir %s pour choisir une arme."), *Touche.ToString());
		}
	}
}

AActor* ARoueArmes::Roue() const
{
	if (AActor* Deja = RoueModes.Get())
	{
		return Deja;
	}
	UWorld* W = GetWorld();
	if (!W)
	{
		return {};
	}
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		if (DeLaClasse(*It, TEXT("RoueModes")))
		{
			const_cast<ARoueArmes*>(this)->RoueModes = *It;
			return *It;
		}
	}
	return nullptr;
}

void ARoueArmes::ReprendreTabALaRoueDesModes()
{
	// L'ancienne roue des modes (ChellesRoue) s'ouvrait aussi sur TAB. Les modes sont maintenant sur F1-F12 :
	// on retire seulement sa liaison TAB (le plugin ChellesRoue n'est pas modifie, tout le reste marche).
	AActor* R = Roue();
	UInputComponent* Entrees = R ? R->InputComponent : nullptr;
	if (!Entrees || !bActif)
	{
		return;
	}
	const int32 Avant = Entrees->KeyBindings.Num();
	Entrees->KeyBindings.RemoveAll([this](const FInputKeyBinding& Liaison)
	{
		return Liaison.Chord.Key == Touche;
	});
	if (Entrees->KeyBindings.Num() != Avant && !bTabRepris)
	{
		bTabRepris = true;
		UE_LOG(LogTemp, Log, TEXT("[RoueArmes] %s retire de l'ancienne roue des modes : %s ouvre maintenant la roue des armes (modes sur F1-F12)."),
			*Touche.ToString(), *Touche.ToString());
	}
}

// =====================================================================================
//  Ouvrir / fermer
// =====================================================================================

void ARoueArmes::Ouvrir()
{
	APlayerController* PC = GetPC();
	UWorld* Monde = GetWorld();
	if (!bActif || bOuverte || !PC || !Monde)
	{
		return;
	}
	if (PC->IsPaused() || PC->bShowMouseCursor)
	{
		UE_LOG(LogTemp, Log, TEXT("[RoueArmes] %s appuye mais roue non ouverte : %s."), *Touche.ToString(),
			PC->IsPaused() ? TEXT("jeu en pause") : TEXT("curseur de la souris affiche (un menu est ouvert)"));
		return;
	}
	UGameViewportClient* Vue = Monde->GetGameViewport();
	if (!Vue)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RoueArmes] Pas d'ecran de jeu : roue non ouverte."));
		return;
	}
	Widget = SNew(SRoueArmes);
	Remplir();
	if (Widget->Cases.Num() == 0)
	{
		Widget.Reset();
		Message(TEXT("Roue des armes vide : aucune arme dans le tableau DT_Armes"));
		return;
	}
	Vue->AddViewportWidgetContent(Widget.ToSharedRef(), 110);

	if (!EntreesRoue)
	{
		EntreesRoue = NewObject<UInputComponent>(this, TEXT("EntreesRoueArmes"));
		EntreesRoue->Priority = InputPriority + 1;
		EntreesRoue->RegisterComponent();
		EntreesRoue->BindAxisKey(EKeys::MouseX, this, &ARoueArmes::SourisX);
		EntreesRoue->BindAxisKey(EKeys::MouseY, this, &ARoueArmes::SourisY);
		EntreesRoue->BindAxisKey(EKeys::Gamepad_RightX, this, &ARoueArmes::ManetteX);
		EntreesRoue->BindAxisKey(EKeys::Gamepad_RightY, this, &ARoueArmes::ManetteY);
		EntreesRoue->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ARoueArmes::Valider);
		EntreesRoue->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ARoueArmes::Annuler);
	}
	PC->PushInputComponent(EntreesRoue);
	PC->SetIgnoreLookInput(true);
	bIgnoreVue = true;

	AncienRalenti = UGameplayStatics::GetGlobalTimeDilation(Monde);
	if (Ralenti < 0.999f && GetNetMode() == NM_Standalone) // en multijoueur on ne ralentit pas le temps des autres
	{
		UGameplayStatics::SetGlobalTimeDilation(Monde, Ralenti);
	}
	Curseur = FVector2D::ZeroVector;
	Manette = FVector2D::ZeroVector;
	Survol = INDEX_NONE;
	Apparition = 0.f;
	bOuverte = true;
	MettreAJourCentre();
	UE_LOG(LogTemp, Log, TEXT("[RoueArmes] Roue ouverte (%d cases)."), Widget->Cases.Num());
}

void ARoueArmes::Remplir()
{
	// Une case par ligne du tableau (ou par arme ramassee seulement si ToutesLesArmes=Non),
	// puis les armes ramassees qui ne sont dans aucune ligne.
	Cibles.Reset();
	Images.Reset();
	APlayerController* PC = GetPC();
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	const bool bSoldat = Soldat(Pion);
	const TArray<UObject*> Ramassees = bSoldat ? ArmesDuSoldat(Pion) : TArray<UObject*>();
	const int32 EnMain = bSoldat ? CaseEnMain(Pion) : INDEX_NONE;
	const bool bArsenalEnMain = ArsenalEnMain();
	{
		FString Liste;
		for (UObject* R : Ramassees)
		{
			Liste += R ? (TEXT(" ") + R->GetClass()->GetName()) : FString(TEXT(" (vide)"));
		}
		UE_LOG(LogTemp, Log, TEXT("[RoueArmes] Joueur %s : %s, %d case(s) d'armes :%s, en main %d, inventaire %s."),
			Pion ? *Pion->GetClass()->GetName() : TEXT("aucun"), bSoldat ? TEXT("soldat") : TEXT("PAS soldat (pas de UT_Inventory / EquipWeapon)"),
			Ramassees.Num(), *Liste, EnMain,
			(Pion && Inventaire(Pion->GetClass())) ? *Inventaire(Pion->GetClass())->GetName() : TEXT("introuvable"));
	}
	const bool bArsenalPresent = IConsoleManager::Get().FindConsoleObject(TEXT("Chelles.Arsenal")) != nullptr;
	AActor* Relais = RelaisArsenal();

	// Quelle case de l'inventaire va avec quelle ligne : d'abord le meme Blueprint (ou ses versions reseau),
	// puis un debut de nom commun (BPC_Weapon_Laser <-> BPC_WeaponLaser_New).
	TArray<UClass*> Classes;
	for (const FArmeDeLaRoue& A : Armes)
	{
		Classes.Add(A.Classe.IsNull() ? nullptr : A.Classe.LoadSynchronous());
	}
	TArray<int32> CaseDeLigne;
	CaseDeLigne.Init(INDEX_NONE, Armes.Num());
	TArray<bool> Prise;
	Prise.Init(false, Ramassees.Num());
	for (int32 Passe = 0; Passe < 2; ++Passe)
	{
		for (int32 l = 0; l < Armes.Num(); ++l)
		{
			if (CaseDeLigne[l] != INDEX_NONE || (!Classes[l] && Armes[l].Classe.IsNull()))
			{
				continue;
			}
			const FString NomLigne = NomSimple(Classes[l] ? Classes[l]->GetName() : Armes[l].Classe.GetAssetName());
			for (int32 c = 0; c < Ramassees.Num(); ++c)
			{
				if (Prise[c] || !Ramassees[c])
				{
					continue;
				}
				UClass* K = Ramassees[c]->GetClass();
				const FString NomCase = NomSimple(K->GetName());
				const bool bPareil = (Classes[l] && K->IsChildOf(Classes[l])) || NomCase == NomLigne;
				const bool bProche = Passe == 1 && NomLigne.Len() > 9 && NomCase.Len() > 9
					&& (NomCase.StartsWith(NomLigne) || NomLigne.StartsWith(NomCase));
				if (bPareil || bProche)
				{
					CaseDeLigne[l] = c;
					Prise[c] = true;
					break;
				}
			}
		}
	}

	auto AjouterCase = [&](const FText& Nom, UTexture2D* Image, const FLinearColor& Couleur, const FString& Categorie,
		bool bPossedee, bool bDansLaMain, const FCibleRoueArmes& Cible)
	{
		FCaseArme Case;
		Case.Nom = Nom;
		Case.Image = Pinceau(Image);
		if (Image)
		{
			Images.Add(Image);
		}
		Case.Couleur = Couleur.A > 0.01f ? Couleur : GrisBleu;
		Case.Categorie = Categorie;
		Case.bPossedee = bPossedee;
		Case.bEnMain = bDansLaMain;
		Widget->Cases.Add(Case);
		Cibles.Add(Cible);
	};

	for (int32 l = 0; l < Armes.Num(); ++l)
	{
		const FArmeDeLaRoue& A = Armes[l];
		FCibleRoueArmes Cible;
		Cible.Ligne = l;
		Cible.Classe = A.Classe;
		Cible.Arsenal = A.Arsenal;
		Cible.bDonner = A.bDonner;
		UTexture2D* Image = A.Icone.IsNull() ? nullptr : A.Icone.LoadSynchronous();
		if (!A.Arsenal.IsEmpty())
		{
			bool bDispo = bArsenalPresent && bSoldat;
			if (bDispo && Relais)
			{
				int64 Charge = 1;
				if (A.Arsenal.StartsWith(TEXT("Lance"), ESearchCase::IgnoreCase) || A.Arsenal.StartsWith(TEXT("Flam"), ESearchCase::IgnoreCase))
				{
					LireNombre(Relais, TEXT("ChargeFlammes"), Charge);
				}
				else if (A.Arsenal.StartsWith(TEXT("Mitr"), ESearchCase::IgnoreCase))
				{
					LireNombre(Relais, TEXT("ChargeMitrailleuse"), Charge);
				}
				bDispo = Charge > 0;
			}
			if (!bDispo && !bToutesLesArmes)
			{
				continue;
			}
			const bool bDansLaMain = bArsenalEnMain && A.Arsenal.Equals(DernierArsenal, ESearchCase::IgnoreCase);
			AjouterCase(A.Nom.IsEmpty() ? FText::FromString(A.Arsenal.ToUpper()) : A.Nom, Image, A.Couleur, A.Categorie, bDispo, bDansLaMain, Cible);
			continue;
		}
		const int32 c = CaseDeLigne[l];
		UObject* Instance = Ramassees.IsValidIndex(c) ? Ramassees[c] : nullptr;
		if (!Instance && !bToutesLesArmes)
		{
			continue;
		}
		const UObject* Modele = Instance ? Instance : (Classes[l] ? Classes[l]->GetDefaultObject() : nullptr);
		if (!Image)
		{
			Image = ImageDeLArme(Modele);
		}
		Cible.Inventaire = Instance ? c : INDEX_NONE;
		Cible.Instance = Instance;
		const FText Nom = !A.Nom.IsEmpty() ? A.Nom : (Modele ? NomDeLArme(Modele) : FText::FromString(A.Classe.GetAssetName()));
		// Pas encore ramassee : utilisable quand meme si elle peut etre donnee (DonnerLesArmes=Oui ou colonne bDonner)
		const bool bUtilisable = Instance != nullptr || (bSoldat && (A.bDonner || bDonnerToutes) && Classes[l] != nullptr);
		AjouterCase(Nom, Image, A.Couleur, A.Categorie, bUtilisable, Instance && c == EnMain && !bArsenalEnMain, Cible);
	}

	// Armes ramassees qui ne sont dans aucune ligne du tableau : on les montre quand meme
	for (int32 c = 0; c < Ramassees.Num(); ++c)
	{
		if (Prise[c] || !Ramassees[c])
		{
			continue;
		}
		FCibleRoueArmes Cible;
		Cible.Inventaire = c;
		Cible.Instance = Ramassees[c];
		AjouterCase(NomDeLArme(Ramassees[c]), ImageDeLArme(Ramassees[c]), FLinearColor(0.f, 0.f, 0.f, 0.f), TEXT("Fusil"),
			true, c == EnMain && !bArsenalEnMain, Cible);
	}

	FText NomMode;
	FLinearColor CouleurMode;
	LireMode(NomMode, CouleurMode);
	Widget->NomPerso = NomMode;
	Widget->CouleurPerso = CouleurMode;
}

void ARoueArmes::MettreAJourCentre()
{
	if (!Widget.IsValid())
	{
		return;
	}
	Widget->Survol = Survol;
	Widget->Curseur = Curseur;
	Widget->Apparition = Apparition;

	APlayerController* PC = GetPC();
	const bool bSoldat = Soldat(PC ? PC->GetPawn() : nullptr);

	// Case montree au centre : celle visee, sinon l'arme en main
	int32 Montree = Survol;
	if (!Widget->Cases.IsValidIndex(Montree))
	{
		Montree = Widget->Cases.IndexOfByPredicate([](const FCaseArme& C) { return C.bEnMain; });
	}
	if (!Widget->Cases.IsValidIndex(Montree) || !Cibles.IsValidIndex(Montree))
	{
		Widget->Titre = FText::FromString(TEXT("ARMES"));
		Widget->Munitions = FText::GetEmpty();
		Widget->Aide = FText::FromString(bSoldat ? TEXT("Bouge la souris vers une arme") : TEXT("Armes : passe en Soldat (F1)"));
		return;
	}
	const FCaseArme& Case = Widget->Cases[Montree];
	const FCibleRoueArmes& Cible = Cibles[Montree];
	Widget->Titre = Case.Nom;

	if (!Cible.Arsenal.IsEmpty())
	{
		int64 Charge = -1;
		if (AActor* Relais = RelaisArsenal())
		{
			if (Cible.Arsenal.StartsWith(TEXT("Lance"), ESearchCase::IgnoreCase) || Cible.Arsenal.StartsWith(TEXT("Flam"), ESearchCase::IgnoreCase))
			{
				LireNombre(Relais, TEXT("ChargeFlammes"), Charge);
			}
			else if (Cible.Arsenal.StartsWith(TEXT("Mitr"), ESearchCase::IgnoreCase))
			{
				LireNombre(Relais, TEXT("ChargeMitrailleuse"), Charge);
			}
		}
		Widget->Munitions = FText::FromString(Charge >= 0 ? FString::Printf(TEXT("%lld"), Charge) : FString(TEXT("\u221E")));
	}
	else
	{
		if (UObject* Instance = Cible.Instance.Get())
		{
			Widget->Munitions = TexteMunitions(Instance);
		}
		else
		{
			UClass* Classe = Cible.Classe.IsNull() ? nullptr : Cible.Classe.LoadSynchronous();
			Widget->Munitions = TexteMunitions(Classe ? Classe->GetDefaultObject() : nullptr, true);
		}
	}

	const TCHAR* Aide = TEXT("Bouge la souris vers une arme");
	if (Survol != INDEX_NONE)
	{
		if (!bSoldat)
		{
			Aide = TEXT("Armes : passe en Soldat (F1)");
		}
		else if (Case.bEnMain)
		{
			Aide = TEXT("D\u00E9j\u00E0 en main");
		}
		else if (Case.bPossedee)
		{
			Aide = TEXT("Rel\u00E2che TAB pour la prendre");
		}
		else if (!Cible.Arsenal.IsEmpty())
		{
			Aide = TEXT("Prends-la sur un r\u00E2telier en bois");
		}
		else if (Cible.bDonner || bDonnerToutes)
		{
			Aide = TEXT("Rel\u00E2che TAB : elle t'est donn\u00E9e");
		}
		else
		{
			Aide = TEXT("Pas encore ramass\u00E9e");
		}
	}
	else if (Case.bEnMain)
	{
		Aide = TEXT("En main");
	}
	Widget->Aide = FText::FromString(Aide);
}

void ARoueArmes::Relacher()
{
	if (bOuverte)
	{
		Valider();
	}
}

void ARoueArmes::Valider()
{
	if (!bOuverte)
	{
		return;
	}
	const int32 Choix = Survol;
	Fermer();
	if (Choix != INDEX_NONE)
	{
		Choisir(Choix);
	}
	Cibles.Reset();
}

void ARoueArmes::Annuler()
{
	Fermer();
	Cibles.Reset();
}

void ARoueArmes::Fermer()
{
	if (!bOuverte)
	{
		return;
	}
	bOuverte = false;
	APlayerController* PC = GetPC();
	if (PC && EntreesRoue)
	{
		PC->PopInputComponent(EntreesRoue);
	}
	if (PC && bIgnoreVue)
	{
		PC->SetIgnoreLookInput(false);
	}
	bIgnoreVue = false;
	if (UWorld* Monde = GetWorld())
	{
		if (Ralenti < 0.999f && GetNetMode() == NM_Standalone)
		{
			UGameplayStatics::SetGlobalTimeDilation(Monde, AncienRalenti > 0.f ? AncienRalenti : 1.f);
		}
		if (Widget.IsValid())
		{
			if (UGameViewportClient* Vue = Monde->GetGameViewport())
			{
				Vue->RemoveViewportWidgetContent(Widget.ToSharedRef());
			}
		}
	}
	Widget.Reset();
	Images.Reset();
}

void ARoueArmes::SourisX(float Valeur)
{
	if (bOuverte)
	{
		Curseur.X += Valeur * Sensibilite * 0.01f;
	}
}

void ARoueArmes::SourisY(float Valeur)
{
	if (bOuverte)
	{
		Curseur.Y -= Valeur * Sensibilite * 0.01f; // l'ecran descend vers le bas
	}
}

void ARoueArmes::ManetteX(float Valeur)
{
	Manette.X = Valeur;
}

void ARoueArmes::ManetteY(float Valeur)
{
	Manette.Y = -Valeur;
}

void ARoueArmes::MettreAJourSurvol()
{
	if (Manette.SizeSquared() > 0.25f * 0.25f)
	{
		Curseur = Manette.GetSafeNormal();
	}
	if (Curseur.SizeSquared() > 1.f)
	{
		Curseur = Curseur.GetSafeNormal();
	}
	const int32 N = Widget.IsValid() ? Widget->Cases.Num() : 0;
	if (N == 0 || Curseur.SizeSquared() < 0.3f * 0.3f)
	{
		return; // pas assez bouge : on garde le choix precedent
	}
	const float Part = 2.f * PI / N;
	float Angle = FMath::Atan2(Curseur.Y, Curseur.X) + PI / 2.f; // 0 = en haut, sens horaire
	if (Angle < 0.f)
	{
		Angle += 2.f * PI;
	}
	Survol = FMath::RoundToInt(Angle / Part) % N;
}

void ARoueArmes::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bLieClavier && PCLie.Get() != GetPC())
	{
		bLieClavier = false; // le joueur a change de PlayerController (menu -> partie) : on se relie au nouveau
	}
	if (!bLieClavier)
	{
		LierClavier();
	}
	// Lecture directe de la touche, comme les touches F1-F12 : marche meme si un autre element du jeu
	// prend TAB avant la roue (2026-11-21 : la liaison seule ne suffisait pas chez MERIDJA).
	if (APlayerController* PCTouche = GetPC())
	{
		if (Touche.IsValid() && bActif)
		{
			if (!bOuverte && PCTouche->WasInputKeyJustPressed(Touche))
			{
				Ouvrir();
			}
			else if (bOuverte && !PCTouche->IsInputKeyDown(Touche))
			{
				Relacher();
			}
		}
	}
	const float Maintenant = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
	if (Maintenant >= ProchaineVerification || Maintenant + 1.f < ProchaineVerification)
	{
		ProchaineVerification = Maintenant + 0.5f;
		ReprendreTabALaRoueDesModes();
	}
	if (bOuverte)
	{
		APlayerController* PC = GetPC();
		if (!PC || PC->IsPaused())
		{
			Annuler();
			return;
		}
		Apparition = FMath::Min(1.f, Apparition + float(FApp::GetDeltaTime()) / 0.12f);
		MettreAJourSurvol();
		MettreAJourCentre();
	}
}

// =====================================================================================
//  Prendre l'arme choisie
// =====================================================================================

void ARoueArmes::Choisir(int32 Index)
{
	if (!Cibles.IsValidIndex(Index))
	{
		return;
	}
	const FCibleRoueArmes Cible = Cibles[Index];
	APlayerController* PC = GetPC();
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (!Soldat(Pion))
	{
		Message(TEXT("Les armes se prennent en mode Soldat (touche F1)"));
		return;
	}
	if (!Cible.Arsenal.IsEmpty())
	{
		if (!LancerCommande(TEXT("Chelles.Arsenal"), { Cible.Arsenal }))
		{
			Message(TEXT("Arsenal introuvable (plugin ChellesArmes pas compile ?)"));
			return;
		}
		DernierArsenal = Cible.Arsenal; // l'arsenal affiche lui-meme son message (ou pourquoi elle n'est pas prete)
		return;
	}
	int32 Case = Cible.Inventaire;
	if (Case == INDEX_NONE || !Cible.Instance.IsValid())
	{
		if (!(Cible.bDonner || bDonnerToutes))
		{
			Message(TEXT("Pas encore ramass\u00E9e : trouve-la sur la carte"));
			return;
		}
		UClass* Classe = Cible.Classe.LoadSynchronous();
		Case = Classe ? Donner(Pion, Classe) : INDEX_NONE;
		if (Case == INDEX_NONE)
		{
			return;
		}
	}
	PrendreCase(Pion, Case);
}

bool ARoueArmes::PrendreCase(APawn* Pion, int32 Inventaire)
{
	UClass* C = Pion->GetClass();
	if (LireOuiNon(Pion, TEXT("isSwappingWeapons")))
	{
		Message(TEXT("Changement d'arme d\u00E9j\u00E0 en cours"));
		return false;
	}
	const TArray<UObject*> Ramassees = ArmesDuSoldat(Pion);
	if (!Ramassees.IsValidIndex(Inventaire) || !Ramassees[Inventaire])
	{
		return false;
	}
	FIntProperty* Selection = FindFProperty<FIntProperty>(C, TEXT("SelectedWeaponIndex"));
	const bool bArsenal = ArsenalEnMain();
	if (bArsenal)
	{
		LancerCommande(TEXT("Chelles.Arsenal"), { TEXT("Aucune") }); // rend l'arme normale
	}
	if (Selection->GetPropertyValue_InContainer(Pion) == Inventaire && !bArsenal)
	{
		return true; // deja en main
	}
	// Comme la roue d'armes du kit (W_CircleWeaponSelect) : stop tir auto, case choisie, baisser l'arme, equiper
	AppelerSansParametre(Pion, TEXT("AutoFireStop"));
	Selection->SetPropertyValue_InContainer(Pion, Inventaire);
	if (bAnimation)
	{
		AppelerSansParametre(Pion, TEXT("PlayLowerMontage"));
	}
	AppelerSansParametre(Pion, TEXT("EquipWeapon"));
	UE_LOG(LogTemp, Log, TEXT("[RoueArmes] Case %d -> %s"), Inventaire + 1, *Ramassees[Inventaire]->GetClass()->GetName());
	return true;
}

int32 ARoueArmes::Donner(APawn* Pion, UClass* Classe)
{
	// DonnerLesArmes=Oui (ou colonne bDonner) : on donne l'arme comme un ramassage du kit (SpawnAndGiveWeapon).
	if (!Pion->HasAuthority())
	{
		Message(TEXT("En multijoueur : ramasse l'arme sur la carte"));
		return INDEX_NONE;
	}
	UFunction* F = Pion->FindFunction(TEXT("SpawnAndGiveWeapon"));
	if (!F)
	{
		F = Pion->FindFunction(TEXT("AddWeapon"));
	}
	if (!F)
	{
		Message(TEXT("Le soldat ne sait pas recevoir d'arme (SpawnAndGiveWeapon absent)"));
		return INDEX_NONE;
	}
	uint8* Parametres = static_cast<uint8*>(FMemory_Alloca(FMath::Max<int32>(1, F->ParmsSize)));
	FMemory::Memzero(Parametres, FMath::Max<int32>(1, F->ParmsSize));
	for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->InitializeValue_InContainer(Parametres);
	}
	bool bRempli = false;
	AActor* Neuve = nullptr;
	for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm) && !bRempli; ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
		{
			continue;
		}
		if (FClassProperty* K = CastField<FClassProperty>(*It))
		{
			if (Classe->IsChildOf(K->MetaClass))
			{
				K->SetObjectPropertyValue_InContainer(Parametres, Classe);
				bRempli = true;
			}
		}
		else if (FObjectProperty* O = CastField<FObjectProperty>(*It))
		{
			if (O->PropertyClass && Classe->IsChildOf(O->PropertyClass) && Classe->IsChildOf(AActor::StaticClass()))
			{
				FActorSpawnParameters P;
				P.Owner = Pion;
				P.Instigator = Pion;
				P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Neuve = GetWorld()->SpawnActor<AActor>(Classe, Pion->GetActorTransform(), P);
				if (Neuve)
				{
					O->SetObjectPropertyValue_InContainer(Parametres, Neuve);
					bRempli = true;
				}
			}
		}
	}
	if (bRempli)
	{
		Pion->ProcessEvent(F, Parametres);
	}
	for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(Parametres);
	}
	if (!bRempli)
	{
		Message(FString::Printf(TEXT("Impossible de donner %s"), *Classe->GetName()));
		UE_LOG(LogTemp, Warning, TEXT("[RoueArmes] %s : aucun parametre de %s n'accepte %s."), *Pion->GetClass()->GetName(), *F->GetName(), *Classe->GetName());
		return INDEX_NONE;
	}
	const TArray<UObject*> Ramassees = ArmesDuSoldat(Pion);
	for (int32 c = Ramassees.Num() - 1; c >= 0; --c)
	{
		if (Ramassees[c] && (Ramassees[c] == Neuve || Ramassees[c]->GetClass()->IsChildOf(Classe)))
		{
			UE_LOG(LogTemp, Log, TEXT("[RoueArmes] %s donnee (case %d)."), *Classe->GetName(), c + 1);
			return c;
		}
	}
	Message(TEXT("Plus de place pour une arme de plus"));
	if (Neuve && !Neuve->IsPendingKill() && Neuve->GetOwner() == Pion && !Neuve->GetAttachParentActor())
	{
		Neuve->Destroy(); // refusee par le soldat : on ne la laisse pas trainer
	}
	return INDEX_NONE;
}

bool ARoueArmes::ArsenalEnMain() const
{
	// L'arme de l'arsenal est dessinee par un acteur ArsenalVue, visible seulement quand elle est en main.
	UWorld* W = GetWorld();
	if (!W)
	{
		return {};
	}
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		if (It->GetClass()->GetName() == TEXT("ArsenalVue") && !It->IsHidden())
		{
			return true;
		}
	}
	return false;
}

AActor* ARoueArmes::RelaisArsenal() const
{
	// Munitions des armes lourdes : AArsenalRelais (ChellesArmes) du joueur de cette machine.
	UWorld* W = GetWorld();
	APlayerController* PC = GetPC();
	AActor* Premier = nullptr;
	if (!W)
	{
		return nullptr;
	}
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		if (It->GetClass()->GetName() == TEXT("ArsenalRelais"))
		{
			if (PC && It->GetOwner() == PC)
			{
				return *It;
			}
			Premier = Premier ? Premier : *It;
		}
	}
	return GetNetMode() == NM_Standalone ? Premier : nullptr;
}

bool ARoueArmes::LancerCommande(const FString& Nom, const TArray<FString>& Arguments) const
{
	IConsoleObject* Objet = IConsoleManager::Get().FindConsoleObject(*Nom);
	IConsoleCommand* Commande = Objet ? Objet->AsCommand() : nullptr;
	return Commande ? Commande->Execute(Arguments, GetWorld(), *GLog) : false;
}

// =====================================================================================
//  Personnage joue (ancienne roue des modes, lue par reflexion)
// =====================================================================================

void ARoueArmes::LireMode(FText& Nom, FLinearColor& Couleur) const
{
	Nom = FText::FromString(TEXT("SOLDAT"));
	Couleur = FLinearColor(0.f, 0.94f, 1.f);
	AActor* R = Roue();
	UFunction* F = R ? R->FindFunction(TEXT("ModeActuel")) : nullptr;
	FArrayProperty* Tableau = R ? FindFProperty<FArrayProperty>(R->GetClass(), TEXT("Modes")) : nullptr;
	FStructProperty* Interieur = Tableau ? CastField<FStructProperty>(Tableau->Inner) : nullptr;
	if (!F || !Interieur || F->ParmsSize > 64)
	{
		return;
	}
	uint8 Parametres[64];
	FMemory::Memzero(Parametres, sizeof(Parametres));
	R->ProcessEvent(F, Parametres);
	int32 Index = INDEX_NONE;
	for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		if (FIntProperty* I = CastField<FIntProperty>(*It))
		{
			if (It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				Index = I->GetPropertyValue_InContainer(Parametres);
			}
		}
	}
	FScriptArrayHelper Modes(Tableau, Tableau->ContainerPtrToValuePtr<void>(R));
	if (Index < 0 || Index >= Modes.Num())
	{
		return;
	}
	const uint8* Element = Modes.GetRawPtr(Index);
	if (FTextProperty* T = FindFProperty<FTextProperty>(Interieur->Struct, TEXT("Nom")))
	{
		Nom = T->GetPropertyValue_InContainer(Element);
	}
	FStructProperty* C = FindFProperty<FStructProperty>(Interieur->Struct, TEXT("Couleur"));
	if (C && C->Struct == TBaseStructure<FLinearColor>::Get())
	{
		Couleur = *C->ContainerPtrToValuePtr<FLinearColor>(Element);
	}
}

// =====================================================================================
//  Console : Chelles.RoueArmes
// =====================================================================================

void ARoueArmes::Commande(const TArray<FString>& Arguments)
{
	const FString Ordre = Arguments.Num() > 0 ? Arguments[0] : FString(TEXT("Info"));
	if (Ordre.Equals(TEXT("Relire"), ESearchCase::IgnoreCase))
	{
		Fermer();
		ChargerReglages();
		Message(FString::Printf(TEXT("Roue des armes : %d armes (%s)"), Armes.Num(), bDepuisTableau ? TEXT("tableau DT_Armes") : TEXT("liste par defaut")));
		return;
	}
	if (Ordre == TEXT("0") || Ordre == TEXT("1"))
	{
		bActif = (Ordre == TEXT("1"));
		if (!bActif)
		{
			Fermer();
		}
		Message(bActif ? TEXT("Roue des armes : active") : TEXT("Roue des armes : coupee"));
		return;
	}
	FString Liste;
	for (const FArmeDeLaRoue& A : Armes)
	{
		Liste += FString::Printf(TEXT("\n  %s : %s"), *A.Nom.ToString(), A.Arsenal.IsEmpty() ? *A.Classe.GetAssetName() : *(TEXT("arsenal ") + A.Arsenal));
	}
	UE_LOG(LogTemp, Display, TEXT("[RoueArmes] %s, touche %s, %d armes (%s), TAB repris a la roue des modes : %s%s"),
		bActif ? TEXT("active") : TEXT("coupee"), *Touche.ToString(), Armes.Num(),
		bDepuisTableau ? *CheminTableau : TEXT("liste par defaut"), bTabRepris ? TEXT("oui") : TEXT("non"), *Liste);
	Message(FString::Printf(TEXT("Roue des armes : %d armes, %s (details dans le journal)"), Armes.Num(), bDepuisTableau ? TEXT("tableau DT_Armes") : TEXT("liste par defaut")));
}
