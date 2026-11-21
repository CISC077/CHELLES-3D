// CHELLES 3D - Touches F1 a F12 : modes de la roue + menu deroulant en haut (voir RaccourcisF.h)
#include "RaccourcisF.h"

#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RaccourcisTypes.h"
#include "Styling/CoreStyle.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** Touches par defaut (si Raccourcis.txt manque) : les 11 modes de la roue, F12 reservee. */
	const TCHAR* const ActionsParDefaut[12] = {
		TEXT("Soldat"), TEXT("Spider-Male"), TEXT("Spider-Femelle"), TEXT("PANTERA"),
		TEXT("Motoneige"), TEXT("POLICE"), TEXT("FLASH ECLAIR"), TEXT("FLAMME FIRE"),
		TEXT("SuperPowers"), TEXT("DYNAMIC COMBAT"), TEXT("HORSE"), TEXT("Reserve")
	};

	const FLinearColor CouleurNeon(0.f, 0.94f, 1.f);
	const FLinearColor CouleurFond(0.02f, 0.f, 0.08f, 0.72f);

	bool EstReserve(const FString& Action)
	{
		return Action.IsEmpty() || Action.Equals(TEXT("Reserve"), ESearchCase::IgnoreCase) || Action.Equals(TEXT("Rien"), ESearchCase::IgnoreCase);
	}

	bool EstNomArsenal(const FString& Action)
	{
		static const TCHAR* const Noms[] = {
			TEXT("Blaster"), TEXT("Bazooka"), TEXT("Sabre"), TEXT("Grenade"),
			TEXT("LanceFlammes"), TEXT("Mitrailleuse"), TEXT("Aucune")
		};
		for (const TCHAR* Nom : Noms)
		{
			if (Action.Equals(Nom, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	/** "Case3" -> 2 ; autre chose -> INDEX_NONE */
	int32 IndexCase(const FString& Action)
	{
		if (Action.StartsWith(TEXT("Case"), ESearchCase::IgnoreCase))
		{
			const FString Chiffre = Action.Mid(4).TrimStartAndEnd();
			if (Chiffre.IsNumeric())
			{
				const int32 N = FCString::Atoi(*Chiffre);
				if (N >= 1 && N <= 32)
				{
					return N - 1;
				}
			}
		}
		return INDEX_NONE;
	}

	const TCHAR* const NomsTableauArmes[] = { TEXT("UT_Inventory"), TEXT("Weapons") };

	FArrayProperty* TableauArmes(UClass* Classe)
	{
		for (const TCHAR* Nom : NomsTableauArmes)
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

	/**
	 * Appelle une fonction (Blueprint ou C++) par son nom. Le premier parametre texte recoit Texte,
	 * le premier parametre entier recoit Entier. Rend la valeur de retour (bool ou entier) dans Retour.
	 */
	bool Appeler(UObject* Objet, const TCHAR* Nom, const FString* Texte = nullptr, int32* Retour = nullptr)
	{
		UFunction* Fonction = Objet ? Objet->FindFunction(FName(Nom)) : nullptr;
		if (!Fonction)
		{
			return false;
		}
		if (Fonction->ParmsSize == 0)
		{
			Objet->ProcessEvent(Fonction, nullptr);
			return true;
		}
		uint8* Parametres = static_cast<uint8*>(FMemory_Alloca(Fonction->ParmsSize));
		FMemory::Memzero(Parametres, Fonction->ParmsSize);
		for (TFieldIterator<FProperty> It(Fonction); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Parametres);
		}
		bool bTexteMis = false;
		for (TFieldIterator<FProperty> It(Fonction); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
			{
				continue;
			}
			if (FStrProperty* S = CastField<FStrProperty>(*It))
			{
				if (Texte && !bTexteMis)
				{
					S->SetPropertyValue_InContainer(Parametres, *Texte);
					bTexteMis = true;
				}
			}
		}
		Objet->ProcessEvent(Fonction, Parametres);
		if (Retour)
		{
			for (TFieldIterator<FProperty> It(Fonction); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				if (!It->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					continue;
				}
				if (FBoolProperty* B = CastField<FBoolProperty>(*It))
				{
					*Retour = B->GetPropertyValue_InContainer(Parametres) ? 1 : 0;
				}
				else if (FIntProperty* I = CastField<FIntProperty>(*It))
				{
					*Retour = I->GetPropertyValue_InContainer(Parametres);
				}
			}
		}
		for (TFieldIterator<FProperty> It(Fonction); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Parametres);
		}
		return true;
	}

	bool EstDeClasse(const UObject* Objet, const TCHAR* NomClasse)
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
}

// =====================================================================================
//  Construction, reglages
// =====================================================================================

ARaccourcisF::ARaccourcisF()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true; // le menu deroulant remonte meme en pause
	SetActorHiddenInGame(true);
	bReplicates = false; // un par machine (ecran du joueur)
}

void ARaccourcisF::BeginPlay()
{
	Super::BeginPlay();
	ChargerReglages();
	CreerEcran();
}

void ARaccourcisF::EndPlay(const EEndPlayReason::Type Raison)
{
	RetirerEcran();
	Super::EndPlay(Raison);
}

float ARaccourcisF::Maintenant() const
{
	return GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
}

void ARaccourcisF::ChargerReglages()
{
	bActif = true;
	bAnimation = true;
	bMessages = true;
	AffichageMenu = 1;
	bVertical = true;
	DureeMenu = 3.f;
	CheminTableau = TEXT("/Game/Chelles/Donnees/DT_Raccourcis.DT_Raccourcis");
	bDepuisTableau = false;
	Raccourcis.Reset();
	for (int32 i = 0; i < 12; ++i)
	{
		FRaccourciF R;
		R.Touche = FKey(*FString::Printf(TEXT("F%d"), i + 1));
		R.Action = ActionsParDefaut[i];
		Raccourcis.Add(R);
	}

	const FString Fichier = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesRaccourcis"), TEXT("Raccourcis.txt"));
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Fichier))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Raccourcis] %s introuvable : touches par defaut (F1-F11 = modes)."), *Fichier);
	}
	for (FString Ligne : Lignes)
	{
		Ligne.TrimStartAndEndInline();
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")))
		{
			continue;
		}
		FString Cle, Valeur;
		if (!Ligne.Split(TEXT("="), &Cle, &Valeur))
		{
			continue;
		}
		Cle.TrimStartAndEndInline();
		Valeur.TrimStartAndEndInline();
		const bool bOui = Valeur.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || Valeur == TEXT("1");
		if (Cle.Equals(TEXT("Actif"), ESearchCase::IgnoreCase))
		{
			bActif = bOui;
		}
		else if (Cle.Equals(TEXT("Animation"), ESearchCase::IgnoreCase))
		{
			bAnimation = bOui;
		}
		else if (Cle.Equals(TEXT("Messages"), ESearchCase::IgnoreCase))
		{
			bMessages = bOui;
		}
		else if (Cle.Equals(TEXT("Menu"), ESearchCase::IgnoreCase))
		{
			AffichageMenu = Valeur.Equals(TEXT("Toujours"), ESearchCase::IgnoreCase) ? 2
				: (Valeur.Equals(TEXT("Jamais"), ESearchCase::IgnoreCase) ? 0 : 1);
		}
		else if (Cle.Equals(TEXT("Disposition"), ESearchCase::IgnoreCase))
		{
			bVertical = !Valeur.Equals(TEXT("Horizontale"), ESearchCase::IgnoreCase);
		}
		else if (Cle.Equals(TEXT("Tableau"), ESearchCase::IgnoreCase))
		{
			CheminTableau = Valeur;
		}
		else if (Cle.Equals(TEXT("DureeMenu"), ESearchCase::IgnoreCase))
		{
			DureeMenu = FMath::Clamp(FCString::Atof(*Valeur.Replace(TEXT(","), TEXT("."))), 0.5f, 60.f);
		}
		else if (Cle.Len() >= 2 && (Cle[0] == TCHAR('F') || Cle[0] == TCHAR('f')) && Cle.Mid(1).IsNumeric())
		{
			const int32 N = FCString::Atoi(*Cle.Mid(1));
			if (N >= 1 && N <= 12)
			{
				Raccourcis[N - 1].Action = Valeur;
			}
		}
	}
	ChargerTableau();
	FString Liste;
	for (const FRaccourciF& R : Raccourcis)
	{
		Liste += FString::Printf(TEXT(" %s=%s"), *R.Touche.ToString(), *R.Action);
	}
	UE_LOG(LogTemp, Log, TEXT("[Raccourcis] Pret (%s, touches lues dans %s, menu %s %s).%s"),
		bActif ? TEXT("actif") : TEXT("coupe"),
		bDepuisTableau ? *CheminTableau : TEXT("Raccourcis.txt (tableau DT_Raccourcis absent)"),
		bVertical ? TEXT("vertical") : TEXT("horizontal"),
		AffichageMenu == 2 ? TEXT("toujours") : (AffichageMenu == 0 ? TEXT("jamais") : TEXT("deroulant")),
		*Liste);
	RemplirMenu();
}

void ARaccourcisF::ChargerTableau()
{
	if (CheminTableau.IsEmpty())
	{
		return;
	}
	UDataTable* Tableau = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *CheminTableau, nullptr, LOAD_NoWarn | LOAD_Quiet));
	if (!Tableau)
	{
		return;
	}
	if (Tableau->GetRowStruct() != FChellesRaccourciLigne::StaticStruct())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Raccourcis] %s n'utilise pas la structure ChellesRaccourciLigne : ignore."), *CheminTableau);
		return;
	}
	TArray<FRaccourciF> Lignes;
	for (const TPair<FName, uint8*>& Paire : Tableau->GetRowMap()) // dans l'ordre du tableau
	{
		const FChellesRaccourciLigne* L = reinterpret_cast<const FChellesRaccourciLigne*>(Paire.Value);
		if (!L || !L->Touche.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Raccourcis] Ligne %s : touche invalide, ignoree."), *Paire.Key.ToString());
			continue;
		}
		FRaccourciF R;
		R.Touche = L->Touche;
		R.Action = L->Action.TrimStartAndEnd();
		R.NomAffiche = L->NomAffiche.ToString();
		R.Couleur = L->Couleur;
		Lignes.Add(R);
	}
	if (Lignes.Num() > 0)
	{
		Raccourcis = MoveTemp(Lignes);
		bDepuisTableau = true;
	}
}

APlayerController* ARaccourcisF::GetPC() const
{
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	return (PC && PC->IsLocalController()) ? PC : nullptr;
}

// =====================================================================================
//  Touches
// =====================================================================================

void ARaccourcisF::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Liste des modes et mode joue (pour le menu)
	if (Maintenant() >= ProchaineLecture)
	{
		ProchaineLecture = Maintenant() + 0.25f;
		if (NomsModes.Num() == 0 && Roue())
		{
			LireModes();
			RemplirMenu();
		}
		const int32 Index = LireModeActuel();
		ModeJoue = NomsModes.IsValidIndex(Index) ? NomsModes[Index] : FString();
	}

	if (!bActif)
	{
		return;
	}
	APlayerController* PC = GetPC();
	if (!PC || PC->bShowMouseCursor || PC->IsPaused())
	{
		return; // un menu est ouvert (pause, inventaire, roue TAB...) : on ne touche a rien
	}
	if (PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::RightControl)
		|| PC->IsInputKeyDown(EKeys::LeftAlt) || PC->IsInputKeyDown(EKeys::RightAlt))
	{
		// Ctrl+F1 / Ctrl+F2 = aides du pack vehicules (ShowCG / ShowCT), Alt+F4 = quitter...
		// (Maj n'est pas bloquee : c'est la course)
		return;
	}
	for (int32 i = 0; i < Raccourcis.Num(); ++i)
	{
		const FKey& Touche = Raccourcis[i].Touche;
		if (Touche.IsValid() && PC->WasInputKeyJustPressed(Touche) && !ToucheDejaPrise(PC, Touche))
		{
			Appuyer(i);
		}
	}
}

bool ARaccourcisF::ToucheDejaPrise(APlayerController* PC, const FKey& Touche) const
{
	// Un mode peut se servir d'une touche F (DYNAMIC COMBAT : F2 roulade, F3 saut, F4 potion, F5 ramasser, F6 cible).
	// Tant que ce mode est joue, la touche lui reste.
	if (!PC || !PC->PlayerInput)
	{
		return false;
	}
	for (const FInputActionKeyMapping& M : PC->PlayerInput->ActionMappings)
	{
		if (M.Key == Touche && !M.bCtrl && !M.bAlt && !M.bCmd)
		{
			return true;
		}
	}
	return false;
}

void ARaccourcisF::Appuyer(int32 Numero)
{
	const FRaccourciF& R = Raccourcis[Numero];
	MontrerMenu();
	if (EstReserve(R.Action))
	{
		return; // touche reservee pour plus tard
	}

	APlayerController* PC = GetPC();
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	const int32 Case = IndexCase(R.Action);
	if (Case != INDEX_NONE)
	{
		if (EstSoldat(Pion))
		{
			ChoisirCase(Pion, Case);
		}
		return;
	}
	if (EstNomArsenal(R.Action))
	{
		if (EstSoldat(Pion))
		{
			ChoisirArsenal(R.Action);
		}
		return;
	}
	ChoisirMode(R.Action);
}

// =====================================================================================
//  Modes de la roue (plugin ChellesRoue)
// =====================================================================================

AActor* ARaccourcisF::Roue() const
{
	if (AActor* Deja = RoueTrouvee.Get())
	{
		return Deja;
	}
	UWorld* W = GetWorld();
	if (!W)
	{
		return nullptr;
	}
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		if (EstDeClasse(*It, TEXT("RoueModes")))
		{
			const_cast<ARaccourcisF*>(this)->RoueTrouvee = *It;
			return *It;
		}
	}
	return nullptr;
}

void ARaccourcisF::LireModes()
{
	NomsModes.Reset();
	CouleursModes.Reset();
	AActor* R = Roue();
	FArrayProperty* Tableau = R ? FindFProperty<FArrayProperty>(R->GetClass(), TEXT("Modes")) : nullptr;
	FStructProperty* Interieur = Tableau ? CastField<FStructProperty>(Tableau->Inner) : nullptr;
	if (!Interieur)
	{
		return;
	}
	FTextProperty* Nom = FindFProperty<FTextProperty>(Interieur->Struct, TEXT("Nom"));
	FStructProperty* Couleur = FindFProperty<FStructProperty>(Interieur->Struct, TEXT("Couleur"));
	FScriptArrayHelper Modes(Tableau, Tableau->ContainerPtrToValuePtr<void>(R));
	for (int32 i = 0; i < Modes.Num(); ++i)
	{
		const uint8* Element = Modes.GetRawPtr(i);
		NomsModes.Add(Nom ? Nom->GetPropertyValue_InContainer(Element).ToString() : FString::Printf(TEXT("Mode %d"), i + 1));
		FLinearColor C = CouleurNeon;
		if (Couleur && Couleur->Struct == TBaseStructure<FLinearColor>::Get())
		{
			C = *Couleur->ContainerPtrToValuePtr<FLinearColor>(Element);
		}
		CouleursModes.Add(C);
	}
	UE_LOG(LogTemp, Log, TEXT("[Raccourcis] Roue trouvee : %d modes (%s)."), NomsModes.Num(), *FString::Join(NomsModes, TEXT(", ")));
}

int32 ARaccourcisF::LireModeActuel() const
{
	int32 Index = INDEX_NONE;
	if (AActor* R = Roue())
	{
		Appeler(R, TEXT("ModeActuel"), nullptr, &Index);
	}
	return Index;
}

bool ARaccourcisF::ChoisirMode(const FString& Nom)
{
	AActor* R = Roue();
	if (!R)
	{
		Message(TEXT("Roue des modes introuvable (plugin ChellesRoue pas compile ?)"));
		return false;
	}
	int32 bFait = 0;
	if (!Appeler(R, TEXT("ChoisirModeParNom"), &Nom, &bFait))
	{
		Message(TEXT("Roue des modes trop ancienne : recompilez ChellesRoue"));
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("[Raccourcis] Mode -> %s (%s)"), *Nom, bFait ? TEXT("ok") : TEXT("pas encore / indisponible"));
	if (!bFait && GetNetMode() == NM_Standalone)
	{
		bool bConnu = false;
		for (const FString& M : NomsModes)
		{
			bConnu |= M.Equals(Nom, ESearchCase::IgnoreCase);
		}
		Message(bConnu ? FString::Printf(TEXT("%s indisponible pour l'instant"), *Nom)
			: FString::Printf(TEXT("\"%s\" : aucun mode de ce nom (Raccourcis.txt)"), *Nom));
	}
	ProchaineLecture = 0.f; // relire le mode joue tout de suite
	return bFait != 0;
}

// =====================================================================================
//  Armes du soldat FPS_Game (si une touche F est reglee sur Case1..Case4)
// =====================================================================================

bool ARaccourcisF::EstSoldat(APawn* Pion)
{
	if (!Pion)
	{
		return false;
	}
	UClass* C = Pion->GetClass();
	return FindFProperty<FIntProperty>(C, TEXT("SelectedWeaponIndex")) != nullptr
		&& Pion->FindFunction(TEXT("EquipWeapon")) != nullptr
		&& TableauArmes(C) != nullptr;
}

FString ARaccourcisF::NomArme(UObject* Arme)
{
	if (!Arme)
	{
		return FString();
	}
	static const TCHAR* const Champs[] = { TEXT("WeaponName"), TEXT("Weapon Name"), TEXT("DisplayName"), TEXT("ItemName"), TEXT("Item Name") };
	for (const TCHAR* Champ : Champs)
	{
		if (FTextProperty* T = FindFProperty<FTextProperty>(Arme->GetClass(), FName(Champ)))
		{
			const FText Valeur = T->GetPropertyValue_InContainer(Arme);
			if (!Valeur.IsEmpty())
			{
				return Valeur.ToString();
			}
		}
		if (FStrProperty* S = FindFProperty<FStrProperty>(Arme->GetClass(), FName(Champ)))
		{
			const FString Valeur = S->GetPropertyValue_InContainer(Arme);
			if (!Valeur.IsEmpty())
			{
				return Valeur;
			}
		}
	}
	FString Nom = Arme->GetClass()->GetName();
	Nom.RemoveFromEnd(TEXT("_C"));
	Nom.RemoveFromStart(TEXT("BP_"));
	Nom.RemoveFromStart(TEXT("Weapon_"));
	return Nom.Replace(TEXT("_"), TEXT(" "));
}

bool ARaccourcisF::ChoisirCase(APawn* Pion, int32 Index)
{
	UClass* C = Pion->GetClass();
	if (FBoolProperty* Change = FindFProperty<FBoolProperty>(C, TEXT("isSwappingWeapons")))
	{
		if (Change->GetPropertyValue_InContainer(Pion))
		{
			return false; // changement d'arme deja en cours (comme la roue du pack)
		}
	}
	FArrayProperty* Tableau = TableauArmes(C);
	FObjectPropertyBase* Interieur = CastField<FObjectPropertyBase>(Tableau->Inner);
	FScriptArrayHelper Armes(Tableau, Tableau->ContainerPtrToValuePtr<void>(Pion));
	UObject* Arme = (Index < Armes.Num()) ? Interieur->GetObjectPropertyValue(Armes.GetRawPtr(Index)) : nullptr;
	if (!Arme)
	{
		Message(FString::Printf(TEXT("Case %d vide : ramassez une arme"), Index + 1));
		return false;
	}
	FIntProperty* Selection = FindFProperty<FIntProperty>(C, TEXT("SelectedWeaponIndex"));
	const bool bArsenal = ArsenalEnMain();
	if (Selection->GetPropertyValue_InContainer(Pion) == Index && !bArsenal)
	{
		Message(FString::Printf(TEXT("%d  %s"), Index + 1, *NomArme(Arme)));
		return true;
	}
	if (bArsenal)
	{
		LancerCommande(TEXT("Chelles.Arsenal"), { TEXT("Aucune") });
	}
	// Comme la roue d'armes du pack (W_CircleWeaponSelect) : stop tir auto, case choisie, baisser l'arme, equiper
	Appeler(Pion, TEXT("AutoFireStop"));
	Selection->SetPropertyValue_InContainer(Pion, Index);
	if (bAnimation)
	{
		Appeler(Pion, TEXT("PlayLowerMontage"));
	}
	Appeler(Pion, TEXT("EquipWeapon"));
	UE_LOG(LogTemp, Log, TEXT("[Raccourcis] Case %d -> %s"), Index + 1, *Arme->GetClass()->GetName());
	Message(FString::Printf(TEXT("%d  %s"), Index + 1, *NomArme(Arme)));
	return true;
}

bool ARaccourcisF::ArsenalEnMain() const
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return false;
	}
	// L'arme de l'arsenal est dessinee par un acteur ArsenalVue, visible seulement quand elle est en main.
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		if (It->GetClass()->GetName() == TEXT("ArsenalVue") && !It->IsHidden())
		{
			return true;
		}
	}
	return false;
}

bool ARaccourcisF::LancerCommande(const FString& Nom, const TArray<FString>& Arguments) const
{
	IConsoleObject* Objet = IConsoleManager::Get().FindConsoleObject(*Nom);
	IConsoleCommand* Commande = Objet ? Objet->AsCommand() : nullptr;
	return Commande ? Commande->Execute(Arguments, GetWorld(), *GLog) : false;
}

bool ARaccourcisF::ChoisirArsenal(const FString& Nom)
{
	if (!LancerCommande(TEXT("Chelles.Arsenal"), { Nom }))
	{
		Message(TEXT("Arsenal introuvable (plugin ChellesArmes)"));
		return false;
	}
	return true; // l'arsenal affiche lui-meme son message
}

// =====================================================================================
//  Console : Chelles.Raccourcis
// =====================================================================================

void ARaccourcisF::Commande(const TArray<FString>& Arguments)
{
	const FString Ordre = Arguments.Num() > 0 ? Arguments[0] : FString(TEXT("Info"));
	if (Ordre.Equals(TEXT("Relire"), ESearchCase::IgnoreCase))
	{
		ChargerReglages();
		MontrerMenu();
		Message(TEXT("Raccourcis.txt relu"));
	}
	else if (Ordre == TEXT("0") || Ordre == TEXT("1"))
	{
		bActif = (Ordre == TEXT("1"));
		Message(bActif ? TEXT("Touches F1-F12 actives") : TEXT("Touches F1-F12 coupees"));
	}
	else if (Ordre.Equals(TEXT("Menu"), ESearchCase::IgnoreCase))
	{
		MontrerMenu();
	}
	else if (Ordre.Len() >= 2 && (Ordre[0] == TCHAR('F') || Ordre[0] == TCHAR('f')) && Ordre.Mid(1).IsNumeric())
	{
		const int32 N = FCString::Atoi(*Ordre.Mid(1));
		if (N >= 1 && N <= Raccourcis.Num())
		{
			Appuyer(N - 1); // test sans clavier
		}
	}
	else
	{
		FString Texte = FString(bActif ? TEXT("Touches actives") : TEXT("Touches COUPEES")) + (bDepuisTableau ? TEXT(" (DT_Raccourcis) :") : TEXT(" (Raccourcis.txt) :"));
		for (const FRaccourciF& R : Raccourcis)
		{
			Texte += FString::Printf(TEXT(" %s=%s"), *R.Touche.ToString(), *R.Action);
		}
		UE_LOG(LogTemp, Log, TEXT("[Raccourcis] %s | roue %s, %d modes, mode joue : %s"),
			*Texte, Roue() ? TEXT("trouvee") : TEXT("ABSENTE"), NomsModes.Num(), ModeJoue.IsEmpty() ? TEXT("?") : *ModeJoue);
		MontrerMenu();
	}
}

// =====================================================================================
//  Ecran : menu deroulant en haut + petit message
// =====================================================================================

void ARaccourcisF::Message(const FString& Texte)
{
	UE_LOG(LogTemp, Log, TEXT("[Raccourcis] %s"), *Texte);
	if (!bMessages)
	{
		return;
	}
	TexteMessage = Texte;
	FinMessage = Maintenant() + 2.5f;
	CreerEcran();
}

void ARaccourcisF::MontrerMenu()
{
	DebutMenu = Maintenant();
	CreerEcran();
}

void ARaccourcisF::CreerEcran()
{
	UWorld* W = GetWorld();
	if (Ecran.IsValid() || !W || GetNetMode() == NM_DedicatedServer || !FSlateApplication::IsInitialized())
	{
		return;
	}
	UGameViewportClient* VueJeu = W->GetGameViewport();
	if (!VueJeu)
	{
		return;
	}
	TWeakObjectPtr<ARaccourcisF> Moi(this);

	// Le menu descend en 0,2 s, reste DureeMenu secondes, puis remonte en 0,25 s.
	auto Descente = [Moi]() -> float
	{
		const ARaccourcisF* A = Moi.Get();
		if (!A || A->AffichageMenu == 0)
		{
			return 1.f;
		}
		if (A->AffichageMenu == 2)
		{
			return 0.f;
		}
		const float T = A->Maintenant() - A->DebutMenu;
		if (T < 0.2f)
		{
			return 1.f - T / 0.2f;
		}
		if (T < A->DureeMenu)
		{
			return 0.f;
		}
		return FMath::Min((T - A->DureeMenu) / 0.25f, 1.f);
	};

	ListeVerticale = SNew(SVerticalBox);
	ListeHorizontale = SNew(SHorizontalBox);

	Ecran = SNew(SOverlay)
		.Visibility(EVisibility::HitTestInvisible)
		// Menu des touches F, vertical (en haut a gauche)
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(FMargin(24.f, 18.f))
		[
			SNew(SBox)
			.Visibility_Lambda([Moi, Descente]()
			{
				const ARaccourcisF* A = Moi.Get();
				return (A && A->bVertical && Descente() < 1.f) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			.RenderTransform_Lambda([Descente]() { return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2D(0.f, -520.f * Descente()))); })
			[ ListeVerticale.ToSharedRef() ]
		]
		// Menu des touches F, horizontal (en haut au centre, comme le schema)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(FMargin(24.f, 18.f))
		[
			SNew(SBox)
			.Visibility_Lambda([Moi, Descente]()
			{
				const ARaccourcisF* A = Moi.Get();
				return (A && !A->bVertical && Descente() < 1.f) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			.RenderTransform_Lambda([Descente]() { return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2D(0.f, -120.f * Descente()))); })
			[ ListeHorizontale.ToSharedRef() ]
		]
		// Petit message en bas
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 190.f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(CouleurFond)
			.Padding(FMargin(18.f, 8.f))
			.Visibility_Lambda([Moi]()
			{
				const ARaccourcisF* A = Moi.Get();
				return (A && A->Maintenant() < A->FinMessage) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
				.ColorAndOpacity(CouleurNeon)
				.ShadowOffset(FVector2D(1.f, 1.f))
				.ShadowColorAndOpacity(FLinearColor(1.f, 0.f, 0.78f, 0.8f))
				.Text_Lambda([Moi]() { const ARaccourcisF* A = Moi.Get(); return FText::FromString(A ? A->TexteMessage : FString()); })
			]
		];
	VueJeu->AddViewportWidgetContent(Ecran.ToSharedRef(), 40);
	RemplirMenu();
}

void ARaccourcisF::RemplirMenu()
{
	if (!ListeVerticale.IsValid() || !ListeHorizontale.IsValid())
	{
		return;
	}
	ListeVerticale->ClearChildren();
	ListeHorizontale->ClearChildren();
	TWeakObjectPtr<ARaccourcisF> Moi(this);

	for (const FRaccourciF& R : Raccourcis)
	{
		// Nom affiche et couleur de la case
		FString Nom = R.NomAffiche.IsEmpty() ? R.Action : R.NomAffiche;
		FLinearColor Couleur = CouleurNeon;
		const bool bReserve = EstReserve(R.Action);
		if (bReserve)
		{
			Nom = TEXT("(libre)");
			Couleur = FLinearColor(0.5f, 0.5f, 0.55f);
		}
		else
		{
			for (int32 i = 0; i < NomsModes.Num(); ++i)
			{
				if (NomsModes[i].Equals(R.Action, ESearchCase::IgnoreCase))
				{
					if (R.NomAffiche.IsEmpty())
					{
						Nom = NomsModes[i];
					}
					Couleur = CouleursModes.IsValidIndex(i) ? CouleursModes[i] : CouleurNeon;
				}
			}
			if (R.Couleur.A > 0.f)
			{
				Couleur = FLinearColor(R.Couleur.R, R.Couleur.G, R.Couleur.B, 1.f);
			}
		}
		const FString Action = R.Action;
		auto EstJoue = [Moi, Action]()
		{
			const ARaccourcisF* A = Moi.Get();
			return A && !A->ModeJoue.IsEmpty() && A->ModeJoue.Equals(Action, ESearchCase::IgnoreCase);
		};

		TSharedRef<SWidget> Case =
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor_Lambda([EstJoue, Couleur]()
			{
				return EstJoue() ? FSlateColor(FLinearColor(Couleur.R, Couleur.G, Couleur.B, 0.85f)) : FSlateColor(CouleurFond);
			})
			.Padding(FMargin(8.f, 3.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.MinDesiredWidth(34.f)
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
						.Text(FText::FromString(R.Touche.ToString()))
						.ColorAndOpacity_Lambda([EstJoue]() { return EstJoue() ? FSlateColor(FLinearColor::Black) : FSlateColor(CouleurNeon); })
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(6.f, 0.f, 0.f, 0.f))
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle(bReserve ? "Regular" : "Bold", 13))
					.Text(FText::FromString(Nom))
					.ColorAndOpacity_Lambda([EstJoue, Couleur]() { return EstJoue() ? FSlateColor(FLinearColor::Black) : FSlateColor(Couleur); })
					.ShadowOffset(FVector2D(1.f, 1.f))
					.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.7f))
				]
			];

		ListeVerticale->AddSlot().AutoHeight().Padding(FMargin(0.f, 1.f))[ Case ];
		// En horizontal on refait une case (un widget Slate n'a qu'un seul parent)
		TSharedRef<SWidget> CaseH =
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor_Lambda([EstJoue, Couleur]()
			{
				return EstJoue() ? FSlateColor(FLinearColor(Couleur.R, Couleur.G, Couleur.B, 0.85f)) : FSlateColor(CouleurFond);
			})
			.Padding(FMargin(6.f, 3.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
					.Text(FText::FromString(R.Touche.ToString()))
					.ColorAndOpacity_Lambda([EstJoue]() { return EstJoue() ? FSlateColor(FLinearColor::Black) : FSlateColor(CouleurNeon); })
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.Text(FText::FromString(Nom))
					.ColorAndOpacity_Lambda([EstJoue, Couleur]() { return EstJoue() ? FSlateColor(FLinearColor::Black) : FSlateColor(Couleur); })
				]
			];
		ListeHorizontale->AddSlot().AutoWidth().Padding(FMargin(1.f, 0.f))[ CaseH ];
	}
}

void ARaccourcisF::RetirerEcran()
{
	if (!Ecran.IsValid())
	{
		return;
	}
	UWorld* W = GetWorld();
	if (UGameViewportClient* VueJeu = W ? W->GetGameViewport() : nullptr)
	{
		VueJeu->RemoveViewportWidgetContent(Ecran.ToSharedRef());
	}
	Ecran.Reset();
	ListeVerticale.Reset();
	ListeHorizontale.Reset();
}
