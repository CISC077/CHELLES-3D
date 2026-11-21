// CHELLES 3D - Raccourcis armes F1 a F12 (voir RaccourcisArmes.h)
#include "RaccourcisArmes.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
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

	/** Appelle une fonction (Blueprint ou C++) par son nom ; les parametres eventuels restent a zero. */
	bool Appeler(UObject* Objet, const TCHAR* Nom)
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
		Objet->ProcessEvent(Fonction, Parametres);
		for (TFieldIterator<FProperty> It(Fonction); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Parametres);
		}
		return true;
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

	/** Touches par defaut (si Raccourcis.txt manque). */
	const TCHAR* const ActionsParDefaut[12] = {
		TEXT("Case1"), TEXT("Case2"), TEXT("Case3"), TEXT("Case4"),
		TEXT("Blaster"), TEXT("Bazooka"), TEXT("Sabre"), TEXT("Grenade"),
		TEXT("LanceFlammes"), TEXT("Mitrailleuse"),
		TEXT("Reserve"), TEXT("Reserve")
	};
}

// =====================================================================================
//  Construction, reglages
// =====================================================================================

ARaccourcisArmes::ARaccourcisArmes()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;
	SetActorHiddenInGame(true);
	bReplicates = false; // un par machine (ecran du joueur)
}

void ARaccourcisArmes::BeginPlay()
{
	Super::BeginPlay();
	ChargerReglages();
	CreerEcran();
}

void ARaccourcisArmes::EndPlay(const EEndPlayReason::Type Raison)
{
	RetirerEcran();
	Super::EndPlay(Raison);
}

void ARaccourcisArmes::ChargerReglages()
{
	bActif = true;
	bAnimation = true;
	bMessages = true;
	Raccourcis.Reset();
	for (int32 i = 0; i < 12; ++i)
	{
		FRaccourciArme R;
		R.Touche = FKey(*FString::Printf(TEXT("F%d"), i + 1));
		R.Action = ActionsParDefaut[i];
		Raccourcis.Add(R);
	}

	const FString Fichier = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesRaccourcis"), TEXT("Raccourcis.txt"));
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Fichier))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Raccourcis] %s introuvable : touches par defaut (F1-F4 cases, F5-F10 arsenal)."), *Fichier);
		return;
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
		else if (Cle.Len() >= 2 && (Cle[0] == TCHAR('F') || Cle[0] == TCHAR('f')) && Cle.Mid(1).IsNumeric())
		{
			const int32 N = FCString::Atoi(*Cle.Mid(1));
			if (N >= 1 && N <= 12)
			{
				Raccourcis[N - 1].Action = Valeur;
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Raccourcis] Pret (%s). F1=%s F2=%s F3=%s F4=%s F5=%s F6=%s F7=%s F8=%s F9=%s F10=%s F11=%s F12=%s"),
		bActif ? TEXT("actif") : TEXT("coupe"),
		*Raccourcis[0].Action, *Raccourcis[1].Action, *Raccourcis[2].Action, *Raccourcis[3].Action,
		*Raccourcis[4].Action, *Raccourcis[5].Action, *Raccourcis[6].Action, *Raccourcis[7].Action,
		*Raccourcis[8].Action, *Raccourcis[9].Action, *Raccourcis[10].Action, *Raccourcis[11].Action);
}

APlayerController* ARaccourcisArmes::GetPC() const
{
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	return (PC && PC->IsLocalController()) ? PC : nullptr;
}

// =====================================================================================
//  Touches
// =====================================================================================

void ARaccourcisArmes::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActif)
	{
		return;
	}
	APlayerController* PC = GetPC();
	if (!PC || PC->bShowMouseCursor)
	{
		return; // un menu est ouvert (pause, inventaire, roue TAB...) : on ne touche a rien
	}
	if (PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::RightControl)
		|| PC->IsInputKeyDown(EKeys::LeftAlt) || PC->IsInputKeyDown(EKeys::RightAlt))
	{
		// Ctrl+F1 / Ctrl+F2 = aides du pack vehicules (ShowCG / ShowCT), Alt+F4 = quitter...
		// (Maj n'est pas bloquee : c'est la course, on peut changer d'arme en courant)
		return;
	}
	for (int32 i = 0; i < Raccourcis.Num(); ++i)
	{
		if (Raccourcis[i].Touche.IsValid() && PC->WasInputKeyJustPressed(Raccourcis[i].Touche))
		{
			Appuyer(i);
		}
	}
}

void ARaccourcisArmes::Appuyer(int32 Numero)
{
	const FRaccourciArme& R = Raccourcis[Numero];
	APlayerController* PC = GetPC();
	APawn* Pion = PC ? PC->GetPawn() : nullptr;

	if (R.Action.IsEmpty() || R.Action.Equals(TEXT("Reserve"), ESearchCase::IgnoreCase) || R.Action.Equals(TEXT("Rien"), ESearchCase::IgnoreCase))
	{
		return; // touche reservee pour plus tard
	}
	if (!EstSoldat(Pion))
	{
		// Vehicule, cheval, DYNAMIC COMBAT (qui se sert de F2-F6)... : la touche est laissee aux autres.
		UE_LOG(LogTemp, Verbose, TEXT("[Raccourcis] %s ignoree : pas le soldat (%s)."), *R.Touche.ToString(), Pion ? *Pion->GetClass()->GetName() : TEXT("aucun pion"));
		return;
	}
	if (PionDecrit.Get() != Pion)
	{
		DecrireSoldat(Pion);
	}

	const int32 Case = IndexCase(R.Action);
	if (Case != INDEX_NONE)
	{
		ChoisirCase(Pion, Case);
	}
	else if (EstNomArsenal(R.Action))
	{
		ChoisirArsenal(Pion, R.Action);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Raccourcis] %s=%s : action inconnue (voir Raccourcis.txt)."), *R.Touche.ToString(), *R.Action);
		Message(FString::Printf(TEXT("%s : action inconnue \"%s\" (Raccourcis.txt)"), *R.Touche.ToString(), *R.Action));
	}
}

// =====================================================================================
//  Armes du soldat FPS_Game
// =====================================================================================

bool ARaccourcisArmes::EstSoldat(APawn* Pion)
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

void ARaccourcisArmes::DecrireSoldat(APawn* Pion)
{
	PionDecrit = Pion;
	UClass* C = Pion->GetClass();
	FArrayProperty* Tableau = TableauArmes(C);
	UFunction* Equiper = Pion->FindFunction(TEXT("EquipWeapon"));
	UE_LOG(LogTemp, Log, TEXT("[Raccourcis] Soldat %s : tableau %s, EquipWeapon (%d octets de parametres), PlayLowerMontage %s, AutoFireStop %s, isSwappingWeapons %s, EquippedSlotLimit %s."),
		*C->GetName(),
		Tableau ? *Tableau->GetName() : TEXT("?"),
		Equiper ? (int32)Equiper->ParmsSize : -1,
		Pion->FindFunction(TEXT("PlayLowerMontage")) ? TEXT("oui") : TEXT("non"),
		Pion->FindFunction(TEXT("AutoFireStop")) ? TEXT("oui") : TEXT("non"),
		FindFProperty<FBoolProperty>(C, TEXT("isSwappingWeapons")) ? TEXT("oui") : TEXT("non"),
		FindFProperty<FIntProperty>(C, TEXT("EquippedSlotLimit")) ? TEXT("oui") : TEXT("non"));
}

FString ARaccourcisArmes::NomArme(UObject* Arme)
{
	if (!Arme)
	{
		return FString();
	}
	static const TCHAR* const Champs[] = { TEXT("WeaponName"), TEXT("Weapon Name"), TEXT("Name"), TEXT("DisplayName"), TEXT("ItemName"), TEXT("Item Name") };
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

bool ARaccourcisArmes::ChoisirCase(APawn* Pion, int32 Index)
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
		return true; // deja en main
	}
	if (bArsenal)
	{
		LancerCommande(TEXT("Chelles.Arsenal"), { TEXT("Aucune") }); // range le blaster / bazooka...
	}

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

// =====================================================================================
//  Armes de l'arsenal (plugin ChellesArmes, commande console Chelles.Arsenal)
// =====================================================================================

bool ARaccourcisArmes::ArsenalEnMain() const
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

bool ARaccourcisArmes::LancerCommande(const FString& Nom, const TArray<FString>& Arguments) const
{
	IConsoleObject* Objet = IConsoleManager::Get().FindConsoleObject(*Nom);
	IConsoleCommand* Commande = Objet ? Objet->AsCommand() : nullptr;
	if (!Commande)
	{
		return false;
	}
	return Commande->Execute(Arguments, GetWorld(), *GLog);
}

bool ARaccourcisArmes::ChoisirArsenal(APawn* Pion, const FString& Nom)
{
	if (!LancerCommande(TEXT("Chelles.Arsenal"), { Nom }))
	{
		Message(TEXT("Arsenal introuvable (plugin ChellesArmes absent ou pas compile)"));
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("[Raccourcis] Arsenal -> %s"), *Nom);
	return true; // l'arsenal affiche lui-meme son message (nom de l'arme ou raison du refus)
}

// =====================================================================================
//  Console : Chelles.Raccourcis
// =====================================================================================

void ARaccourcisArmes::Commande(const TArray<FString>& Arguments)
{
	const FString Ordre = Arguments.Num() > 0 ? Arguments[0] : FString(TEXT("Info"));
	if (Ordre.Equals(TEXT("Relire"), ESearchCase::IgnoreCase))
	{
		ChargerReglages();
		Message(TEXT("Raccourcis.txt relu"));
	}
	else if (Ordre == TEXT("0") || Ordre == TEXT("1"))
	{
		bActif = (Ordre == TEXT("1"));
		Message(bActif ? TEXT("Raccourcis F1-F12 actifs") : TEXT("Raccourcis F1-F12 coupes"));
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
		APlayerController* PC = GetPC();
		APawn* Pion = PC ? PC->GetPawn() : nullptr;
		if (Pion && EstSoldat(Pion))
		{
			DecrireSoldat(Pion);
		}
		FString Texte = bActif ? TEXT("Raccourcis actifs :") : TEXT("Raccourcis COUPES :");
		for (const FRaccourciArme& R : Raccourcis)
		{
			Texte += FString::Printf(TEXT(" %s=%s"), *R.Touche.ToString(), *R.Action);
		}
		UE_LOG(LogTemp, Log, TEXT("[Raccourcis] %s"), *Texte);
		Message(Texte);
	}
}

// =====================================================================================
//  Petit message en bas de l'ecran (2 secondes)
// =====================================================================================

void ARaccourcisArmes::Message(const FString& Texte)
{
	if (!bMessages || !GetWorld())
	{
		return;
	}
	TexteMessage = Texte;
	FinMessage = GetWorld()->GetRealTimeSeconds() + 2.f;
	CreerEcran();
}

void ARaccourcisArmes::CreerEcran()
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
	TWeakObjectPtr<ARaccourcisArmes> Moi(this);
	Ecran = SNew(SOverlay)
		.Visibility(EVisibility::HitTestInvisible)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 190.f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.02f, 0.f, 0.08f, 0.75f))
			.Padding(FMargin(18.f, 8.f))
			.Visibility_Lambda([Moi]()
			{
				const ARaccourcisArmes* A = Moi.Get();
				return (A && A->GetWorld() && A->GetWorld()->GetRealTimeSeconds() < A->FinMessage)
					? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
				.ColorAndOpacity(FLinearColor(0.f, 0.94f, 1.f))
				.ShadowOffset(FVector2D(1.f, 1.f))
				.ShadowColorAndOpacity(FLinearColor(1.f, 0.f, 0.78f, 0.8f))
				.Text_Lambda([Moi]()
				{
					const ARaccourcisArmes* A = Moi.Get();
					return FText::FromString(A ? A->TexteMessage : FString());
				})
			]
		];
	VueJeu->AddViewportWidgetContent(Ecran.ToSharedRef(), 40);
}

void ARaccourcisArmes::RetirerEcran()
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
}
