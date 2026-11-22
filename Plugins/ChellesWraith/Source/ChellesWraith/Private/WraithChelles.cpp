// CHELLES 3D - Wraith a la place du SOLDAT (voir WraithChelles.h).
//
// Comment ca marche, sans toucher a FPS_Game :
//  - le corps du soldat (SK_Mannequin) n'est plus dessine (RenderInMainPass = faux) mais il est
//    toujours la : collisions, tirs, degats, armes, touches... tout marche comme avant.
//  - un composant "ChellesWraithCorps" (le modele du pack Paragon: Wraith) est accroche au corps
//    du soldat et joue l'Animation Blueprint de Wraith (marche, course, saut, visee).
//  - l'arme 3e personne du soldat est cachee : Wraith tient son propre fusil.
//  - en 1re personne, le fusil de Wraith (seulement la partie "Gun" du modele) est pose sur
//    l'arme du soldat, qui est cachee.
//  - Touche K : skin suivant (lignes Skin= de Wraith.txt). Choix garde dans GameUserSettings.ini.

#include "WraithChelles.h"
#include "WraithRelais.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FName TagWraith(TEXT("ChellesWraith"));
	const FName TagSansHabillage(TEXT("ChellesSansHabillage")); // pose par la roue TAB (Spider-Male, PANTERA...)
	const TCHAR* const Section = TEXT("ChellesWraith");

	FString CheminReglages()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesWraith"), TEXT("Wraith.txt"));
	}

	bool EstOui(const FString& Texte)
	{
		const FString T = Texte.TrimStartAndEnd();
		return T.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || T.Equals(TEXT("1")) || T.Equals(TEXT("True"), ESearchCase::IgnoreCase) || T.Equals(TEXT("Yes"), ESearchCase::IgnoreCase);
	}

	/** "0.1, 0.2, 0.3" -> 3 nombres */
	bool LireTrois(const FString& Texte, float& A, float& B, float& C)
	{
		TArray<FString> Morceaux;
		Texte.ParseIntoArray(Morceaux, TEXT(","), true);
		if (Morceaux.Num() < 3)
		{
			return false;
		}
		A = FCString::Atof(*Morceaux[0].TrimStartAndEnd());
		B = FCString::Atof(*Morceaux[1].TrimStartAndEnd());
		C = FCString::Atof(*Morceaux[2].TrimStartAndEnd());
		return true;
	}

	// ------------------------------------------------------------------
	// Lecture des variables des Blueprints FPS_Game par leur nom (aucun lien de compilation)
	// ------------------------------------------------------------------
	UObject* LireObjet(UObject* Objet, FName Nom)
	{
		if (!Objet)
		{
			return nullptr;
		}
		FObjectPropertyBase* P = FindFProperty<FObjectPropertyBase>(Objet->GetClass(), Nom);
		return P ? P->GetObjectPropertyValue_InContainer(Objet) : nullptr;
	}

	bool LireNombre(UObject* Objet, FName Nom, double& Valeur)
	{
		if (!Objet)
		{
			return false;
		}
		FNumericProperty* P = FindFProperty<FNumericProperty>(Objet->GetClass(), Nom);
		if (!P)
		{
			return false;
		}
		const void* Adresse = P->ContainerPtrToValuePtr<void>(Objet);
		Valeur = P->IsFloatingPoint() ? P->GetFloatingPointPropertyValue(Adresse) : double(P->GetSignedIntPropertyValue(Adresse));
		return true;
	}

	bool LireBool(UObject* Objet, FName Nom, bool& Valeur)
	{
		if (!Objet)
		{
			return false;
		}
		FBoolProperty* P = FindFProperty<FBoolProperty>(Objet->GetClass(), Nom);
		if (!P)
		{
			return false;
		}
		Valeur = P->GetPropertyValue_InContainer(Objet);
		return true;
	}

	/** Sante du soldat : variable du personnage, ou d'un de ses composants. */
	bool LireSante(ACharacter* C, float& Sante, float& Max)
	{
		double V = 0.0;
		double M = 100.0;
		UObject* Porteur = nullptr;
		if (LireNombre(C, TEXT("HealthCurrent"), V))
		{
			Porteur = C;
		}
		else
		{
			for (UActorComponent* Comp : C->GetComponents())
			{
				if (Comp && LireNombre(Comp, TEXT("HealthCurrent"), V))
				{
					Porteur = Comp;
					break;
				}
			}
		}
		if (!Porteur)
		{
			return false;
		}
		LireNombre(Porteur, TEXT("HealthMax"), M);
		Sante = float(V);
		Max = FMath::Max(1.f, float(M));
		return true;
	}

	UActorComponent* TrouverComposant(AActor* Acteur, const TCHAR* Nom)
	{
		if (!Acteur)
		{
			return nullptr;
		}
		for (UActorComponent* C : Acteur->GetComponents())
		{
			if (C && C->GetName().Equals(Nom))
			{
				return C;
			}
		}
		return nullptr;
	}

	/** Couteau, epee, grenade : on garde l'arme du soldat. */
	bool EstCorpsACorps(const AActor* Arme)
	{
		const FString N = Arme ? Arme->GetClass()->GetName() : FString();
		return N.Contains(TEXT("Sword")) || N.Contains(TEXT("Knife")) || N.Contains(TEXT("Melee")) || N.Contains(TEXT("Grenade")) || N.Contains(TEXT("Couteau"));
	}

	/** Position d'un os du modele en pose de reference, dans l'espace du composant. */
	bool OsReference(const USkeletalMesh* Corps, FName Os, FTransform& Sortie)
	{
		if (!Corps)
		{
			return false;
		}
		const FReferenceSkeleton& Ref = Corps->GetRefSkeleton();
		int32 Index = Ref.FindBoneIndex(Os);
		if (Index == INDEX_NONE)
		{
			return false;
		}
		const TArray<FTransform>& Pose = Ref.GetRefBonePose();
		FTransform T = FTransform::Identity;
		while (Index != INDEX_NONE)
		{
			T = T * Pose[Index];
			Index = Ref.GetParentIndex(Index);
		}
		Sortie = T;
		return true;
	}

	/** Le soldat n'est plus dessine, mais garde ses reglages (remis a la fin). */
	void Sync(USkeletalMeshComponent* Copie, const UPrimitiveComponent* Modele)
	{
		if (Copie->bOwnerNoSee != Modele->bOwnerNoSee)
		{
			Copie->SetOwnerNoSee(Modele->bOwnerNoSee);
		}
		if (Copie->bOnlyOwnerSee != Modele->bOnlyOwnerSee)
		{
			Copie->SetOnlyOwnerSee(Modele->bOnlyOwnerSee);
		}
		if (Copie->GetVisibleFlag() != Modele->GetVisibleFlag())
		{
			Copie->SetVisibility(Modele->GetVisibleFlag());
		}
		if (Copie->bHiddenInGame != Modele->bHiddenInGame)
		{
			Copie->SetHiddenInGame(Modele->bHiddenInGame);
		}
	}
}

// ======================================================================
//  Demarrage
// ======================================================================

AWraithChelles::AWraithChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SetActorHiddenInGame(true);
	Touche = EKeys::K;
}

void AWraithChelles::BeginPlay()
{
	Super::BeginPlay();
	LireReglages();

	int32 Garde = 0;
	if (GConfig && GConfig->GetInt(Section, TEXT("Skin"), Garde, GGameUserSettingsIni))
	{
		SkinLocal = Skins.Num() > 0 ? FMath::Clamp(Garde, 0, Skins.Num() - 1) : 0;
	}
	UE_LOG(LogTemp, Log, TEXT("[Wraith] %s, %d skins, skin choisi : %s, touche %s"),
		bActif ? TEXT("actif") : TEXT("coupe"), Skins.Num(), Skins.IsValidIndex(SkinLocal) ? *Skins[SkinLocal].Nom : TEXT("?"), *Touche.ToString());
}

void AWraithChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	for (FSoldatWraith& S : Soldats)
	{
		Retirer(S);
	}
	Soldats.Reset();
	RetirerAnnonce();
	Super::EndPlay(Raison);
}

void AWraithChelles::LireReglages()
{
	// Heros et skins par defaut (si Wraith.txt est absent ou n'a pas de ligne Heros= / Skin=)
	const FString W = TEXT("/Game/ParagonWraith/Characters/Heroes/Wraith");
	const FString D = TEXT("/Game/ParagonDekker/Characters/Heroes/Dekker");
	TArray<FString> HerosDefaut;
	HerosDefaut.Add(TEXT("Heros=Wraith|") + W + TEXT("/Wraith_AnimBlueprint.Wraith_AnimBlueprint_C|") + W + TEXT("/Animations/Fire_A_Slow_Montage.Fire_A_Slow_Montage|")
		+ W + TEXT("/Animations/Death_Backward.Death_Backward|/Game/ParagonWraith/Audio/Cues/|Wraith|Gun|weapon_r;gun_stock_bott;gun_barrel;gun_scope|")
		+ TEXT("/Game/ParagonWraith/FX/Particles/Abilities/ScopedShot/FX/P_Wraith_Sniper_MuzzleFlash.P_Wraith_Sniper_MuzzleFlash"));
	HerosDefaut.Add(TEXT("Heros=Dekker|") + D + TEXT("/Dekker_AnimBlueprint.Dekker_AnimBlueprint_C|") + D + TEXT("/Animations/Fire_Med_V2_Montage.Fire_Med_V2_Montage|")
		+ D + TEXT("/Animations/Death_Bwd.Death_Bwd|") + D + TEXT("/Sounds/SoundCues/|Dekker|Weapon;Wapen|weapon;staff_bottom;staff_top;|")
		+ TEXT("/Game/ParagonDekker/FX/Particles/Abilities/Primary/FX/P_Dekker_PrimaryFire.P_Dekker_PrimaryFire"));
	TArray<FString> SkinsDefaut;
	SkinsDefaut.Add(TEXT("Skin=WRAITH VERT MILITAIRE|") + W + TEXT("/Skins/ODGreen/Meshes/Wraith_ODGreen.Wraith_ODGreen||1|0.10,0.13,0.07|Wraith"));
	SkinsDefaut.Add(TEXT("Skin=WRAITH ROUGE ET NOIR|") + W + TEXT("/Skins/LunarOps/Meshes/Wraith_LunarOps.Wraith_LunarOps|0.85,0.03,0.02|0.12|0.30,0.02,0.02|Wraith"));
	SkinsDefaut.Add(TEXT("Skin=WRAITH BLANC ET CYAN|") + W + TEXT("/Skins/LunarOps/Meshes/Wraith_LunarOps.Wraith_LunarOps||1|0.75,0.78,0.80|Wraith"));
	SkinsDefaut.Add(TEXT("Skin=WRAITH CLASSIQUE|") + W + TEXT("/Meshes/Wraith.Wraith||1|0.45,0.45,0.45|Wraith"));
	SkinsDefaut.Add(TEXT("Skin=DEKKER|") + D + TEXT("/Meshes/Dekker.Dekker||1|0.55,0.50,0.45|Dekker"));
	SkinsDefaut.Add(TEXT("Skin=DEKKER ENERGIE|") + D + TEXT("/Skins/Tier_1/Dekker_Energized/Meshes/DekkerEnergized.DekkerEnergized||1|0.12,0.05,0.18|Dekker"));
	SkinsDefaut.Add(TEXT("Skin=DEKKER GLACE|") + D + TEXT("/Skins/Tier_1/Dekker_Frost/Meshes/DekkerArctic.DekkerArctic||1|0.70,0.80,0.90|Dekker"));
	SkinsDefaut.Add(TEXT("Skin=DEKKER MAGMA|") + D + TEXT("/Skins/Tier_1/Dekker_Magma/Meshes/DekkerMagma.DekkerMagma||1|0.15,0.05,0.03|Dekker"));
	SkinsDefaut.Add(TEXT("Skin=DEKKER SAINT-VALENTIN|") + D + TEXT("/Skins/Tier_1/Dekker_Valentine/Meshes/DekkerValentine.DekkerValentine||1|0.60,0.20,0.30|Dekker"));
	const FString SF = TEXT("/Game/SciFiSoldier");
	HerosDefaut.Add(TEXT("Heros=SciFi|Soldat|||||||"));
	SkinsDefaut.Add(TEXT("Skin=SCI-FI NOIR|") + SF + TEXT("/Meshes/SK_SciFiSoldier.SK_SciFiSoldier||1|0.08,0.08,0.08|SciFi|") + SF + TEXT("/Materials/MI_SciFiSoldier.MI_SciFiSoldier"));
	SkinsDefaut.Add(TEXT("Skin=SCI-FI ROUGE|") + SF + TEXT("/Meshes/SK_SciFiSoldier.SK_SciFiSoldier||1|0.33,0.01,0.00|SciFi|") + SF + TEXT("/Materials/MI_SciFiSoldier_2.MI_SciFiSoldier_2"));
	SkinsDefaut.Add(TEXT("Skin=SCI-FI DORE|") + SF + TEXT("/Meshes/SK_SciFiSoldier.SK_SciFiSoldier||1|0.33,0.14,0.00|SciFi|") + SF + TEXT("/Materials/MI_SciFiSoldier_3.MI_SciFiSoldier_3"));
	SkinsDefaut.Add(TEXT("Skin=SCI-FI BLEU|") + SF + TEXT("/Meshes/SK_SciFiSoldier.SK_SciFiSoldier||1|0.00,0.09,0.40|SciFi|") + SF + TEXT("/Materials/MI_SciFiSoldier_4.MI_SciFiSoldier_4"));
	SkinsDefaut.Add(TEXT("Skin=SCI-FI VERT|") + SF + TEXT("/Meshes/SK_SciFiSoldier.SK_SciFiSoldier||1|0.07,0.08,0.04|SciFi|") + SF + TEXT("/Materials/MI_SciFiSoldier_5.MI_SciFiSoldier_5"));
	SkinsDefaut.Add(TEXT("Skin=SOLDAT D'ORIGINE|Soldat||1||"));

	bActif = true;
	Touche = EKeys::K;
	Skins.Reset();
	Heros.Reset();
	Gardes.Reset();

	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *CheminReglages()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Wraith] %s introuvable : reglages par defaut."), *CheminReglages());
	}
	const bool bHerosFichier = Lignes.ContainsByPredicate([](const FString& L) { return L.TrimStart().StartsWith(TEXT("Heros="), ESearchCase::IgnoreCase); });
	const bool bSkinsFichier = Lignes.ContainsByPredicate([](const FString& L) { return L.TrimStart().StartsWith(TEXT("Skin="), ESearchCase::IgnoreCase); });
	if (!bHerosFichier)
	{
		Lignes.Append(HerosDefaut);
	}
	if (!bSkinsFichier)
	{
		Lignes.Append(SkinsDefaut);
	}

	for (FString Ligne : Lignes)
	{
		Ligne = Ligne.TrimStartAndEnd();
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT(";")) || Ligne.StartsWith(TEXT("#")))
		{
			continue;
		}
		FString Cle, Valeur;
		if (!Ligne.Split(TEXT("="), &Cle, &Valeur))
		{
			continue;
		}
		Cle = Cle.TrimStartAndEnd();
		Valeur = Valeur.TrimStartAndEnd();
		float A = 0.f, B = 0.f, C = 0.f;

		if (Cle.Equals(TEXT("Actif"), ESearchCase::IgnoreCase))
		{
			bActif = EstOui(Valeur);
		}
		else if (Cle.Equals(TEXT("Touche"), ESearchCase::IgnoreCase))
		{
			const FKey K(*Valeur);
			Touche = K.IsValid() ? K : EKeys::K;
		}
		else if (Cle.Equals(TEXT("Heros"), ESearchCase::IgnoreCase))
		{
			// Nom|AnimBP|MontageTir|AnimMort|DossierVoix|Prefixe|MotsArme|Os poignee;arriere;avant;haut|EffetBouche
			TArray<FString> M;
			Valeur.ParseIntoArray(M, TEXT("|"), false);
			if (M.Num() < 2)
			{
				continue;
			}
			while (M.Num() < 9)
			{
				M.Add(FString());
			}
			for (FString& Morceau : M)
			{
				Morceau = Morceau.TrimStartAndEnd();
			}
			FHerosWraith H;
			H.Nom = M[0];
			H.bPoseSoldat = M[1].Equals(TEXT("Soldat"), ESearchCase::IgnoreCase);
			H.ClasseAnim = H.bPoseSoldat ? nullptr : LoadObject<UClass>(nullptr, *M[1]);
			H.MontageTir = M[2].IsEmpty() ? nullptr : LoadObject<UAnimMontage>(nullptr, *M[2]);
			H.AnimMort = M[3].IsEmpty() ? nullptr : LoadObject<UAnimSequenceBase>(nullptr, *M[3]);
			H.DossierVoix = M[4];
			if (!H.DossierVoix.IsEmpty() && !H.DossierVoix.EndsWith(TEXT("/")))
			{
				H.DossierVoix += TEXT("/");
			}
			H.Prefixe = M[5].IsEmpty() ? H.Nom : M[5];
			M[6].ParseIntoArray(H.MotsArme, TEXT(";"), true);
			TArray<FString> Os;
			M[7].ParseIntoArray(Os, TEXT(";"), false);
			if (Os.IsValidIndex(0) && !Os[0].IsEmpty()) H.OsPoignee = FName(*Os[0].TrimStartAndEnd());
			if (Os.IsValidIndex(1) && !Os[1].IsEmpty()) H.OsArriere = FName(*Os[1].TrimStartAndEnd());
			if (Os.IsValidIndex(2) && !Os[2].IsEmpty()) H.OsAvant = FName(*Os[2].TrimStartAndEnd());
			if (Os.IsValidIndex(3) && !Os[3].IsEmpty()) H.OsHaut = FName(*Os[3].TrimStartAndEnd());
			H.EffetBouche = M[8].IsEmpty() ? nullptr : LoadObject<UParticleSystem>(nullptr, *M[8]);
			UObject* AGarder[] = { H.ClasseAnim, H.MontageTir, H.AnimMort, H.EffetBouche };
			for (UObject* O : AGarder)
			{
				if (O)
				{
					Gardes.Add(O);
				}
			}
			if (!H.ClasseAnim && !H.bPoseSoldat)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Wraith] %s : Animation Blueprint introuvable (%s)"), *H.Nom, *M[1]);
			}
			Heros.Add(H);
		}
		else if (Cle.Equals(TEXT("Skin"), ESearchCase::IgnoreCase))
		{
			TArray<FString> M;
			Valeur.ParseIntoArray(M, TEXT("|"), false);
			if (M.Num() < 2)
			{
				continue;
			}
			FSkinWraith Skin;
			Skin.Nom = M[0].TrimStartAndEnd();
			Skin.Corps = M[1].TrimStartAndEnd();
			Skin.bSoldat = Skin.Corps.Equals(TEXT("Soldat"), ESearchCase::IgnoreCase) || Skin.Corps.IsEmpty();
			if (M.IsValidIndex(2) && LireTrois(M[2], A, B, C))
			{
				Skin.bRecolorer = true;
				Skin.Teinte = FLinearColor(A, B, C);
			}
			if (M.IsValidIndex(3) && !M[3].TrimStartAndEnd().IsEmpty())
			{
				Skin.Assombrir = FMath::Clamp(FCString::Atof(*M[3].TrimStartAndEnd()), 0.f, 4.f);
			}
			if (M.IsValidIndex(4) && LireTrois(M[4], A, B, C))
			{
				Skin.bBras = true;
				Skin.Bras = FLinearColor(A, B, C);
			}
			Skin.NomHeros = M.IsValidIndex(5) ? M[5].TrimStartAndEnd() : FString();
			if (M.IsValidIndex(6) && !M[6].TrimStartAndEnd().IsEmpty())
			{
				Skin.Materiau = LoadObject<UMaterialInterface>(nullptr, *M[6].TrimStartAndEnd());
				if (Skin.Materiau)
				{
					Gardes.Add(Skin.Materiau);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[Wraith] %s : materiau introuvable (%s)"), *Skin.Nom, *M[6]);
				}
			}
			Skins.Add(Skin);
		}
		else if (Cle.Equals(TEXT("Echelle"), ESearchCase::IgnoreCase))
		{
			Echelle = FMath::Clamp(FCString::Atof(*Valeur), 0.2f, 5.f);
		}
		else if (Cle.Equals(TEXT("ArmeWraith"), ESearchCase::IgnoreCase) || Cle.Equals(TEXT("ArmeHeros"), ESearchCase::IgnoreCase))
		{
			bArmeWraith = EstOui(Valeur);
		}
		else if (Cle.Equals(TEXT("ArmeFPP.Decalage"), ESearchCase::IgnoreCase) && LireTrois(Valeur, A, B, C))
		{
			ArmeDecalage = FVector(A, B, C);
		}
		else if (Cle.Equals(TEXT("ArmeFPP.Rotation"), ESearchCase::IgnoreCase) && LireTrois(Valeur, A, B, C))
		{
			ArmeRotation = FRotator(A, B, C);
		}
		else if (Cle.Equals(TEXT("ArmeFPP.Echelle"), ESearchCase::IgnoreCase))
		{
			ArmeEchelle = FMath::Clamp(FCString::Atof(*Valeur), 0.05f, 5.f);
		}
		else if (Cle.Equals(TEXT("EffetsTir"), ESearchCase::IgnoreCase))
		{
			bEffetsTir = EstOui(Valeur);
		}
		else if (Cle.Equals(TEXT("BrasCouleur"), ESearchCase::IgnoreCase))
		{
			bBrasCouleur = EstOui(Valeur);
		}
		else if (Cle.Equals(TEXT("Voix"), ESearchCase::IgnoreCase))
		{
			Voix = FMath::Clamp(FCString::Atof(*Valeur), 0.f, 2.f);
		}
	}

	// Chaque skin retrouve son heros (Wraith par defaut)
	for (FSkinWraith& Skin : Skins)
	{
		Skin.Heros = 0;
		for (int32 i = 0; i < Heros.Num(); ++i)
		{
			if (Heros[i].Nom.Equals(Skin.NomHeros, ESearchCase::IgnoreCase))
			{
				Skin.Heros = i;
				break;
			}
		}
		if (!Skin.bSoldat && !Heros.IsValidIndex(Skin.Heros))
		{
			Skin.bSoldat = true; // aucun heros declare
		}
	}
	if (SkinLocal >= Skins.Num())
	{
		SkinLocal = 0;
	}
}

const FHerosWraith* AWraithChelles::HerosDe(const FSoldatWraith& S) const
{
	return Heros.IsValidIndex(S.Heros) ? &Heros[S.Heros] : nullptr;
}

USkeletalMesh* AWraithChelles::Charger(const FString& Chemin) const
{
	USkeletalMesh* M = FindObject<USkeletalMesh>(nullptr, *Chemin);
	if (!M)
	{
		M = LoadObject<USkeletalMesh>(nullptr, *Chemin);
		if (M)
		{
			const_cast<AWraithChelles*>(this)->CorpsCharges.AddUnique(M);
		}
	}
	return M;
}

APlayerController* AWraithChelles::GetPC() const
{
	UWorld* W = GetWorld();
	return W ? W->GetFirstPlayerController() : nullptr;
}

// ======================================================================
//  Chaque image
// ======================================================================

void AWraithChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	const float Maintenant = W->GetRealTimeSeconds();

	if (EcranAnnonce.IsValid() && Maintenant > FinAnnonce)
	{
		RetirerAnnonce();
	}

	// Serveur multijoueur : un relais par joueur
	if (W->GetNetMode() == NM_ListenServer || W->GetNetMode() == NM_DedicatedServer)
	{
		if (Maintenant > ProchainRelais)
		{
			ProchainRelais = Maintenant + 1.f;
			GererRelais();
		}
	}

	// Touche K : skin suivant (seulement quand on joue le soldat, hors menus)
	APlayerController* PC = GetPC();
	if (bActif && PC && !PC->bShowMouseCursor && !W->IsPaused() && PC->WasInputKeyJustPressed(Touche))
	{
		ACharacter* Moi = Cast<ACharacter>(PC->GetPawn());
		if (EstSoldat(Moi) && !Moi->Tags.Contains(TagSansHabillage))
		{
			ChangerSkin(SkinLocal + 1, true);
		}
	}

	// Multijoueur : on donne notre skin au serveur (les autres le voient)
	if (W->GetNetMode() != NM_Standalone && Maintenant > ProchainEnvoi)
	{
		if (AWraithRelais* R = MonRelais())
		{
			if (R->Skin != SkinLocal)
			{
				ProchainEnvoi = Maintenant + 1.f;
				R->ServeurSkin(SkinLocal);
			}
		}
	}

	if (Maintenant > ProchaineRecherche)
	{
		ProchaineRecherche = Maintenant + 0.5f;
		Chercher();
	}

	for (int32 i = Soldats.Num() - 1; i >= 0; --i)
	{
		if (!Soldats[i].Soldat.IsValid())
		{
			Retirer(Soldats[i]);
			Soldats.RemoveAt(i);
			continue;
		}
		Suivre(Soldats[i], DeltaSeconds);
	}
}

bool AWraithChelles::EstSoldat(ACharacter* C) const
{
	if (!IsValid(C) || !C->GetMesh() || !C->GetMesh()->SkeletalMesh || !C->GetPlayerState())
	{
		return false;
	}
	bool bSoldat = false;
	for (UClass* K = C->GetClass(); K; K = K->GetSuperClass())
	{
		if (K->GetName().StartsWith(TEXT("BP_CharacterBase")))
		{
			bSoldat = true;
			break;
		}
	}
	// Le corps du soldat d'origine (pas Spider-Man / PANTERA de la roue TAB)
	return bSoldat && C->GetMesh()->SkeletalMesh->GetName().Contains(TEXT("Mannequin"));
}

void AWraithChelles::Chercher()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	for (TActorIterator<ACharacter> It(W); It; ++It)
	{
		ACharacter* C = *It;
		if (!EstSoldat(C))
		{
			continue;
		}
		const bool bDeja = Soldats.ContainsByPredicate([C](const FSoldatWraith& S) { return S.Soldat.Get() == C; });
		if (!bDeja)
		{
			FSoldatWraith S;
			S.Soldat = C;
			Soldats.Add(S);
		}
	}
}

int32 AWraithChelles::SkinDe(ACharacter* Soldat) const
{
	if (!Soldat)
	{
		return SkinLocal;
	}
	UWorld* W = GetWorld();
	if (Soldat->IsLocallyControlled() || !W || W->GetNetMode() == NM_Standalone)
	{
		return SkinLocal;
	}
	APlayerState* PS = Soldat->GetPlayerState();
	for (TActorIterator<AWraithRelais> It(W); It; ++It)
	{
		if (PS && It->Joueur == PS && It->Skin >= 0)
		{
			return It->Skin;
		}
	}
	return 0;
}

void AWraithChelles::Suivre(FSoldatWraith& S, float Dt)
{
	ACharacter* C = S.Soldat.Get();
	const int32 Index = SkinDe(C);
	const bool bVeut = bActif && C && Skins.IsValidIndex(Index) && !Skins[Index].bSoldat
		&& !C->Tags.Contains(TagSansHabillage)
		&& C->GetMesh() && C->GetMesh()->SkeletalMesh && C->GetMesh()->SkeletalMesh->GetName().Contains(TEXT("Mannequin"));
	if (!bVeut)
	{
		if (S.Corps.IsValid() || S.CachesCorps.Num() > 0 || S.Teintes.Num() > 0)
		{
			Retirer(S);
		}
		return;
	}

	Appliquer(S, Skins[Index], Index);
	if (!S.Corps.IsValid())
	{
		return;
	}
	SuivreVoix(S, Dt);
	if (C->IsLocallyControlled())
	{
		SuivreTirs(S);
		if (!S.bIntroJouee)
		{
			S.bIntroJouee = true;
			Dire(S, TEXT("Intro_Statement,Catchphrase"), 1.f, true);
		}
	}
}

// ======================================================================
//  Habiller / deshabiller
// ======================================================================

void AWraithChelles::Appliquer(FSoldatWraith& S, const FSkinWraith& Skin, int32 Index)
{
	ACharacter* C = S.Soldat.Get();
	USkeletalMeshComponent* Mesh = C->GetMesh();
	USkeletalMesh* Corps = Charger(Skin.Corps);
	if (!Corps)
	{
		if (!bAvisPack)
		{
			bAvisPack = true;
			const FString Texte = FString::Printf(TEXT("%s introuvable (%s). Ajoutez le pack gratuit Paragon au projet."), *Skin.Nom, *Skin.Corps);
			UE_LOG(LogTemp, Warning, TEXT("[Wraith] %s"), *Texte);
			if (C->IsLocallyControlled())
			{
				Annonce(Texte);
			}
		}
		return;
	}
	const FHerosWraith& H = Heros[Skin.Heros];

	// Changement de heros (Wraith <-> Dekker) : autres animations, on repart de zero
	if (S.Heros != Skin.Heros && S.Corps.IsValid())
	{
		S.Corps->DestroyComponent();
		S.Corps.Reset();
		RetirerArme(S);
		S.bIntroJouee = false;
	}
	S.Heros = Skin.Heros;

	USkeletalMeshComponent* W = S.Corps.Get();
	if (!W)
	{
		W = NewObject<USkeletalMeshComponent>(C, MakeUniqueObjectName(C, USkeletalMeshComponent::StaticClass(), TEXT("ChellesWraithCorps")));
		W->ComponentTags.Add(TagWraith);
		W->SetupAttachment(Mesh);
		W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		W->SetGenerateOverlapEvents(false);
		W->bReceivesDecals = false;
		W->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones; // ombre juste en 1re personne
		W->SetCastShadow(Mesh->CastShadow);
		W->bCastHiddenShadow = Mesh->bCastHiddenShadow;
		W->SetRelativeScale3D(FVector(Echelle));
		W->SetSkeletalMesh(Corps);
		W->RegisterComponent();
		if (H.bPoseSoldat)
		{
			W->SetMasterPoseComponent(Mesh); // memes os que le soldat : il fait exactement ses gestes
		}
		else if (H.ClasseAnim)
		{
			W->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			W->SetAnimInstanceClass(H.ClasseAnim);
		}
		S.Corps = W;
		S.Skin = -1;
		UE_LOG(LogTemp, Log, TEXT("[Wraith] %s habille en %s."), *C->GetName(), *H.Nom);
	}

	if (S.Skin != Index)
	{
		if (W->SkeletalMesh != Corps)
		{
			W->SetSkeletalMesh(Corps, false);
		}
		W->EmptyOverrideMaterials();
		if (Skin.Materiau)
		{
			for (int32 i = 0; i < W->GetNumMaterials(); ++i)
			{
				W->SetMaterial(i, Skin.Materiau);
			}
		}
		if (Skin.bRecolorer || Skin.Assombrir < 0.999f)
		{
			Recolorer(W, Skin, false);
		}
		S.Skin = Index;
		if (USkeletalMeshComponent* G = S.ArmeFPP.Get())
		{
			G->DestroyComponent(); // recree avec le nouveau skin
			S.ArmeFPP.Reset();
		}
	}

	// Meme visibilite que le corps du soldat (1re / 3e personne, vu par les autres)
	Sync(W, Mesh);
	if (W->bCastHiddenShadow != Mesh->bCastHiddenShadow)
	{
		W->bCastHiddenShadow = Mesh->bCastHiddenShadow;
		W->MarkRenderStateDirty();
	}

	CacherCorpsSoldat(S);

	if (C->IsLocallyControlled())
	{
		if (bArmeWraith && H.MotsArme.Num() > 0)
		{
			SuivreArme(S, Corps);
		}
		else if (S.ArmeFPP.IsValid() || S.CachesArme.Num() > 0)
		{
			RetirerArme(S); // heros sans arme a lui : il garde celles du soldat
		}
		if (bBrasCouleur && Skin.bBras && S.SkinBras != Index)
		{
			RemettreBras(S);
			TeindreBras(S, Skin, Index);
		}
		else if ((!bBrasCouleur || !Skin.bBras) && S.Teintes.Num() > 0)
		{
			RemettreBras(S);
		}
	}
}

void AWraithChelles::Cacher(TArray<FCacheWraith>& Liste, UPrimitiveComponent* C)
{
	if (!C || C->ComponentHasTag(TagWraith))
	{
		return;
	}
	FCacheWraith* Info = Liste.FindByPredicate([C](const FCacheWraith& I) { return I.Composant.Get() == C; });
	if (!Info)
	{
		FCacheWraith Nouveau;
		Nouveau.Composant = C;
		Nouveau.bRenduPrincipal = C->bRenderInMainPass;
		Nouveau.bOmbre = C->CastShadow;
		Liste.Add(Nouveau);
	}
	if (C->bRenderInMainPass)
	{
		C->SetRenderInMainPass(false);
	}
	if (C->CastShadow)
	{
		C->SetCastShadow(false);
	}
}

void AWraithChelles::Remettre(TArray<FCacheWraith>& Liste)
{
	for (const FCacheWraith& I : Liste)
	{
		if (UPrimitiveComponent* C = I.Composant.Get())
		{
			C->SetRenderInMainPass(I.bRenduPrincipal);
			C->SetCastShadow(I.bOmbre);
		}
	}
	Liste.Reset();
}

void AWraithChelles::CacherCorpsSoldat(FSoldatWraith& S)
{
	ACharacter* C = S.Soldat.Get();
	USkeletalMeshComponent* Mesh = C->GetMesh();
	Cacher(S.CachesCorps, Mesh);

	// Morceaux du menu HABILLAGE (ils suivent le corps du soldat)
	TArray<USceneComponent*> Enfants;
	Mesh->GetChildrenComponents(true, Enfants);
	for (USceneComponent* E : Enfants)
	{
		USkinnedMeshComponent* Morceau = Cast<USkinnedMeshComponent>(E);
		if (Morceau && Morceau->MasterPoseComponent.Get() == Mesh)
		{
			Cacher(S.CachesCorps, Morceau);
		}
	}

	// Arme 3e personne du soldat : Wraith tient son propre fusil
	const FHerosWraith* H = HerosDe(S);
	if (!bArmeWraith || !H || H->MotsArme.Num() == 0)
	{
		return;
	}
	AActor* Arme = Cast<AActor>(LireObjet(C, TEXT("EquippedWeapon")));
	if (!Arme || EstCorpsACorps(Arme))
	{
		return;
	}
	if (UPrimitiveComponent* TPP = Cast<UPrimitiveComponent>(TrouverComposant(Arme, TEXT("Weapon_Mesh_TPP"))))
	{
		Cacher(S.CachesCorps, TPP);
		TArray<USceneComponent*> Morceaux;
		TPP->GetChildrenComponents(true, Morceaux);
		for (USceneComponent* M : Morceaux)
		{
			if (UMeshComponent* Maillage = Cast<UMeshComponent>(M))
			{
				Cacher(S.CachesCorps, Maillage);
			}
		}
	}
}

void AWraithChelles::MontrerFusilSeul(USkeletalMeshComponent* G, const TArray<FString>& MotsArme)
{
	USkeletalMesh* M = G ? G->SkeletalMesh : nullptr;
	FSkeletalMeshRenderData* Rendu = M ? M->GetResourceForRendering() : nullptr;
	if (!Rendu)
	{
		return;
	}
	const TArray<FSkeletalMaterial>& Materiaux = M->GetMaterials();
	for (int32 L = 0; L < Rendu->LODRenderData.Num(); ++L)
	{
		const FSkeletalMeshLODInfo* Info = M->GetLODInfo(L);
		const TArray<FSkelMeshRenderSection>& Sections = Rendu->LODRenderData[L].RenderSections;
		for (int32 Sct = 0; Sct < Sections.Num(); ++Sct)
		{
			int32 Mat = Sections[Sct].MaterialIndex;
			if (Info && Info->LODMaterialMap.IsValidIndex(Sct) && Info->LODMaterialMap[Sct] != INDEX_NONE)
			{
				Mat = Info->LODMaterialMap[Sct];
			}
			bool bFusil = false;
			if (Materiaux.IsValidIndex(Mat))
			{
				const FString Nom = Materiaux[Mat].MaterialSlotName.ToString() + TEXT(" ") + GetNameSafe(Materiaux[Mat].MaterialInterface);
				for (const FString& Mot : MotsArme)
				{
					bFusil = bFusil || Nom.Contains(Mot);
				}
			}
			G->ShowMaterialSection(Mat, Sct, bFusil, L);
		}
	}
}

FTransform AWraithChelles::PlacementArme(const FHerosWraith& H, USkeletalMesh* Corps, UMeshComponent* MaillageFPP, AActor* Arme) const
{
	// Repere du fusil de Wraith (pose de reference) : poignee = os weapon_r, avant = crosse -> canon
	FTransform Poignee, Canon, Crosse, Lunette;
	if (!OsReference(Corps, H.OsPoignee, Poignee))
	{
		return FTransform::Identity;
	}
	FVector Avant = Poignee.GetUnitAxis(EAxis::X);
	const bool bCrosse = !H.OsArriere.IsNone() && OsReference(Corps, H.OsArriere, Crosse);
	if (bCrosse && !H.OsAvant.IsNone() && OsReference(Corps, H.OsAvant, Canon))
	{
		const FVector D = Canon.GetLocation() - Crosse.GetLocation();
		if (D.Size() > 1.f)
		{
			Avant = D.GetSafeNormal();
		}
	}
	FVector Haut = FVector::UpVector;
	if (bCrosse && !H.OsHaut.IsNone() && OsReference(Corps, H.OsHaut, Lunette))
	{
		const FVector V = Lunette.GetLocation() - Crosse.GetLocation();
		const FVector H = V - Avant * FVector::DotProduct(V, Avant);
		if (H.Size() > 0.5f)
		{
			Haut = H.GetSafeNormal();
		}
	}
	if (FMath::Abs(FVector::DotProduct(Haut, Avant)) > 0.95f)
	{
		Haut = FMath::Abs(Avant.Z) < 0.9f ? FVector::UpVector : FVector::RightVector;
	}
	const FTransform Fusil(FRotationMatrix::MakeFromXZ(Avant, Haut).ToQuat(), Poignee.GetLocation());

	// Repere de l'arme du soldat : avant = vers la bouche du canon (composant ProjectileSpawnFromMuzzleFPP)
	FVector AvantCible = FVector::ForwardVector;
	if (USceneComponent* Bouche = Cast<USceneComponent>(TrouverComposant(Arme, TEXT("ProjectileSpawnFromMuzzleFPP"))))
	{
		const FVector L = MaillageFPP->GetComponentTransform().InverseTransformPosition(Bouche->GetComponentLocation());
		if (L.Size() > 5.f)
		{
			AvantCible = L.GetSafeNormal();
		}
	}
	FVector HautCible = FVector::UpVector;
	if (FMath::Abs(FVector::DotProduct(HautCible, AvantCible)) > 0.95f)
	{
		HautCible = FVector::RightVector;
	}
	const FTransform Cible(FRotationMatrix::MakeFromXZ(AvantCible, HautCible).ToQuat());

	const float EchelleParent = FMath::Max(KINDA_SMALL_NUMBER, MaillageFPP->GetComponentScale().X);
	const FTransform Reglage(ArmeRotation.Quaternion(), ArmeDecalage, FVector(ArmeEchelle / EchelleParent));
	return Fusil.Inverse() * Reglage * Cible;
}

void AWraithChelles::SuivreArme(FSoldatWraith& S, USkeletalMesh* Corps)
{
	ACharacter* C = S.Soldat.Get();
	AActor* Arme = Cast<AActor>(LireObjet(C, TEXT("EquippedWeapon")));
	UMeshComponent* FPP = Arme ? Cast<UMeshComponent>(TrouverComposant(Arme, TEXT("Weapon_Mesh_FPP"))) : nullptr;
	if (!Arme || !FPP || EstCorpsACorps(Arme))
	{
		RetirerArme(S);
		return;
	}
	if (Arme != S.ArmeSuivie.Get() || FPP != S.MaillageFPPSuivi.Get())
	{
		RetirerArme(S);
		S.ArmeSuivie = Arme;
		S.MaillageFPPSuivi = FPP;
		S.MunitionsVues = -1;
	}

	USkeletalMeshComponent* G = S.ArmeFPP.Get();
	if (!G)
	{
		G = NewObject<USkeletalMeshComponent>(C, MakeUniqueObjectName(C, USkeletalMeshComponent::StaticClass(), TEXT("ChellesWraithFusil")));
		G->ComponentTags.Add(TagWraith);
		G->SetupAttachment(FPP);
		G->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		G->SetGenerateOverlapEvents(false);
		G->SetCastShadow(false);
		G->bReceivesDecals = false;
		G->SetSkeletalMesh(Corps);
		G->RegisterComponent();
		MontrerFusilSeul(G, Heros[S.Heros].MotsArme);
		if (USkeletalMeshComponent* W = S.Corps.Get())
		{
			for (int32 i = 0; i < W->OverrideMaterials.Num(); ++i)
			{
				if (W->OverrideMaterials[i])
				{
					G->SetMaterial(i, W->OverrideMaterials[i]);
				}
			}
		}
		S.ArmeFPP = G;
	}
	G->SetRelativeTransform(PlacementArme(Heros[S.Heros], Corps, FPP, Arme));
	Sync(G, FPP);

	// L'arme du soldat (et le modele pose par ChellesArmes dessus) n'est plus dessinee
	Cacher(S.CachesArme, FPP);
	TArray<USceneComponent*> Morceaux;
	FPP->GetChildrenComponents(true, Morceaux);
	for (USceneComponent* M : Morceaux)
	{
		UMeshComponent* Maillage = Cast<UMeshComponent>(M);
		if (Maillage && Maillage != G)
		{
			Cacher(S.CachesArme, Maillage);
		}
	}
}

void AWraithChelles::RetirerArme(FSoldatWraith& S)
{
	if (USkeletalMeshComponent* G = S.ArmeFPP.Get())
	{
		G->DestroyComponent();
	}
	S.ArmeFPP.Reset();
	Remettre(S.CachesArme);
	S.ArmeSuivie.Reset();
	S.MaillageFPPSuivi.Reset();
}

void AWraithChelles::TeindreBras(FSoldatWraith& S, const FSkinWraith& Skin, int32 Index)
{
	ACharacter* C = S.Soldat.Get();
	S.SkinBras = Index;
	static const TCHAR* const NomsBras[] = { TEXT("Mesh_FPP"), TEXT("Mesh_FPP_Legs") };
	for (const TCHAR* Nom : NomsBras)
	{
		UMeshComponent* M = Cast<UMeshComponent>(TrouverComposant(C, Nom));
		if (!M)
		{
			continue;
		}
		FMateriauxWraith Avant;
		Avant.Composant = M;
		for (int32 i = 0; i < M->GetNumMaterials(); ++i)
		{
			Avant.Avant.Add(M->OverrideMaterials.IsValidIndex(i) ? M->OverrideMaterials[i] : nullptr);
		}
		bool bChange = false;
		for (int32 i = 0; i < M->GetNumMaterials(); ++i)
		{
			UMaterialInterface* Mat = M->GetMaterial(i);
			FLinearColor Valeur;
			if (!Mat || !Mat->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("BodyColor")), Valeur))
			{
				continue;
			}
			UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Mat, M);
			D->SetVectorParameterValue(TEXT("BodyColor"), Skin.Bras);
			M->SetMaterial(i, D);
			bChange = true;
		}
		if (bChange)
		{
			S.Teintes.Add(Avant);
		}
	}
}

void AWraithChelles::RemettreBras(FSoldatWraith& S)
{
	for (const FMateriauxWraith& T : S.Teintes)
	{
		UMeshComponent* M = T.Composant.Get();
		if (!M)
		{
			continue;
		}
		for (int32 i = 0; i < T.Avant.Num(); ++i)
		{
			M->SetMaterial(i, T.Avant[i].Get());
		}
	}
	S.Teintes.Reset();
	S.SkinBras = -1;
}

void AWraithChelles::Recolorer(USkeletalMeshComponent* C, const FSkinWraith& Skin, bool bJournal)
{
	const FLinearColor TeinteHSV = Skin.Teinte.LinearRGBToHSV();
	for (int32 i = 0; i < C->GetNumMaterials(); ++i)
	{
		UMaterialInterface* M = C->GetMaterial(i);
		if (!M)
		{
			continue;
		}
		if (UMaterialInstanceDynamic* Deja = Cast<UMaterialInstanceDynamic>(M))
		{
			M = Deja->Parent ? Deja->Parent : M;
		}
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Ids;
		M->GetAllVectorParameterInfo(Infos, Ids);
		UMaterialInstanceDynamic* D = nullptr;
		for (const FMaterialParameterInfo& Info : Infos)
		{
			FLinearColor Valeur;
			if (!M->GetVectorParameterValue(FHashedMaterialParameterInfo(Info), Valeur))
			{
				continue;
			}
			const FLinearColor HSV = Valeur.LinearRGBToHSV(); // R = teinte, G = saturation, B = luminosite
			const FString N = Info.Name.ToString();
			const bool bCouleur = N.Contains(TEXT("Color")) || N.Contains(TEXT("Colour")) || N.Contains(TEXT("Tint")) || N.Contains(TEXT("Albedo")) || N.Contains(TEXT("Paint"));
			FLinearColor Nouvelle = Valeur;
			bool bChange = false;
			if (Skin.bRecolorer && HSV.G > 0.2f && HSV.B > 0.02f)
			{
				FLinearColor H = HSV;
				H.R = TeinteHSV.R;
				H.G = FMath::Max(HSV.G, TeinteHSV.G);
				Nouvelle = H.HSVToLinearRGB();
				Nouvelle.A = Valeur.A;
				bChange = true;
			}
			else if (Skin.Assombrir < 0.999f && bCouleur && HSV.G <= 0.2f)
			{
				Nouvelle = Valeur * Skin.Assombrir;
				Nouvelle.A = Valeur.A;
				bChange = true;
			}
			if (bJournal)
			{
				UE_LOG(LogTemp, Log, TEXT("[Wraith] %s / %s = (%.2f, %.2f, %.2f)%s"), *M->GetName(), *N, Valeur.R, Valeur.G, Valeur.B, bChange ? TEXT("  -> change") : TEXT(""));
			}
			if (bChange)
			{
				if (!D)
				{
					D = UMaterialInstanceDynamic::Create(M, C);
				}
				D->SetVectorParameterValueByInfo(Info, Nouvelle);
			}
		}
		if (D)
		{
			C->SetMaterial(i, D);
		}
	}
}

void AWraithChelles::Retirer(FSoldatWraith& S)
{
	if (USkeletalMeshComponent* W = S.Corps.Get())
	{
		W->DestroyComponent();
	}
	S.Corps.Reset();
	RetirerArme(S);
	Remettre(S.CachesCorps);
	RemettreBras(S);
	S.Skin = -1;
	S.bMort = false;
	S.SanteVue = -1.f;
}

// ======================================================================
//  Touche K et multijoueur
// ======================================================================

void AWraithChelles::ChangerSkin(int32 Nouveau, bool bAnnoncer)
{
	const int32 N = Skins.Num();
	if (N == 0)
	{
		return;
	}
	SkinLocal = ((Nouveau % N) + N) % N;
	if (GConfig)
	{
		GConfig->SetInt(Section, TEXT("Skin"), SkinLocal, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	if (AWraithRelais* R = MonRelais())
	{
		R->ServeurSkin(SkinLocal);
	}
	if (bAnnoncer)
	{
		Annonce(FString::Printf(TEXT("SOLDAT : %s      (%s = skin suivant)"), *Skins[SkinLocal].Nom, *Touche.GetDisplayName().ToString()));
		APlayerController* PC = GetPC();
		for (FSoldatWraith& S : Soldats)
		{
			if (PC && S.Soldat.Get() == PC->GetPawn() && !Skins[SkinLocal].bSoldat)
			{
				Suivre(S, 0.f); // habille tout de suite, puis la voix
				Dire(S, TEXT("DraftSelect,Emote_Taunt"), 1.f, true);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Wraith] Skin : %s"), *Skins[SkinLocal].Nom);
}

AWraithRelais* AWraithChelles::MonRelais() const
{
	APlayerController* PC = GetPC();
	UWorld* W = GetWorld();
	if (!PC || !W)
	{
		return nullptr;
	}
	for (TActorIterator<AWraithRelais> It(W); It; ++It)
	{
		if (It->GetOwner() == PC)
		{
			return *It;
		}
	}
	return nullptr;
}

void AWraithChelles::GererRelais()
{
	UWorld* W = GetWorld();
	TArray<AWraithRelais*> Relais;
	for (TActorIterator<AWraithRelais> It(W); It; ++It)
	{
		if (!IsValid(It->GetOwner()))
		{
			It->Destroy(); // joueur parti
			continue;
		}
		Relais.Add(*It);
	}
	for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		AWraithRelais* Le = nullptr;
		for (AWraithRelais* R : Relais)
		{
			if (R->GetOwner() == PC)
			{
				Le = R;
				break;
			}
		}
		if (!Le)
		{
			FActorSpawnParameters Params;
			Params.Owner = PC;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Le = W->SpawnActor<AWraithRelais>(AWraithRelais::StaticClass(), FTransform::Identity, Params);
		}
		if (Le && Le->Joueur != PC->PlayerState)
		{
			Le->Joueur = PC->PlayerState;
			Le->ForceNetUpdate();
		}
	}
}

// ======================================================================
//  Voix de Wraith et effets de tir
// ======================================================================

void AWraithChelles::Dire(FSoldatWraith& S, const TCHAR* Repliques, float Chance, bool bForcer)
{
	if (Voix <= 0.f || FMath::FRand() > Chance)
	{
		return;
	}
	USceneComponent* Ou = S.Corps.Get();
	if (!Ou)
	{
		return;
	}
	if (!bForcer && S.VoixEnCours.IsValid() && S.VoixEnCours->IsPlaying())
	{
		return; // il parle deja
	}
	const FHerosWraith* H = HerosDe(S);
	if (!H || H->DossierVoix.IsEmpty())
	{
		return;
	}
	// Repliques = fins de nom ("Effort_Jump") : on cherche <Prefixe>_<fin> dans le dossier des voix du heros
	TArray<FString> Fins;
	FString(Repliques).ParseIntoArray(Fins, TEXT(","), true);
	TArray<USoundBase*> Possibles;
	for (const FString& Fin : Fins)
	{
		const FString Nom = H->Prefixe + TEXT("_") + Fin.TrimStartAndEnd();
		const FString Chemin = FString::Printf(TEXT("%s%s.%s"), *H->DossierVoix, *Nom, *Nom);
		USoundBase* Trouve = nullptr;
		if (USoundBase** Deja = Sons.Find(Chemin))
		{
			Trouve = *Deja;
		}
		else
		{
			Trouve = LoadObject<USoundBase>(nullptr, *Chemin, nullptr, LOAD_Quiet | LOAD_NoWarn);
			Sons.Add(Chemin, Trouve);
		}
		if (Trouve)
		{
			Possibles.Add(Trouve);
		}
	}
	if (Possibles.Num() == 0)
	{
		return;
	}
	USoundBase* Son = Possibles[FMath::RandRange(0, Possibles.Num() - 1)];
	if (S.VoixEnCours.IsValid() && bForcer)
	{
		S.VoixEnCours->Stop();
	}
	S.VoixEnCours = UGameplayStatics::SpawnSoundAttached(Son, Ou, NAME_None, FVector(0.f, 0.f, 150.f), EAttachLocation::KeepRelativeOffset, true, Voix);
}

void AWraithChelles::SuivreVoix(FSoldatWraith& S, float Dt)
{
	ACharacter* C = S.Soldat.Get();
	UWorld* W = GetWorld();
	const float Maintenant = W->GetTimeSeconds();
	UCharacterMovementComponent* Mouvement = C->GetCharacterMovement();
	const bool bAir = Mouvement && Mouvement->IsFalling();
	const FVector V = C->GetVelocity();

	// Saut / atterrissage
	if (bAir && !S.bEnLair)
	{
		S.bEnLair = true;
		S.VitesseZMin = 0.f;
		if (V.Z > 200.f)
		{
			Dire(S, TEXT("Effort_Jump"), 0.6f);
		}
	}
	if (bAir)
	{
		S.VitesseZMin = FMath::Min(S.VitesseZMin, V.Z);
	}
	else if (S.bEnLair)
	{
		S.bEnLair = false;
		if (S.VitesseZMin < -1100.f)
		{
			Dire(S, TEXT("Effort_LandHeavy"), 1.f, true);
		}
		else if (S.VitesseZMin < -600.f)
		{
			Dire(S, TEXT("Effort_Land"), 0.5f);
		}
	}

	// Sante : douleur, sante basse, soin
	float Sante = 0.f, Max = 100.f;
	const bool bSante = LireSante(C, Sante, Max);
	if (bSante)
	{
		if (S.SanteVue >= 0.f)
		{
			const float Perte = S.SanteVue - Sante;
			if (Perte > 0.5f && Sante > 0.f && Maintenant - S.DerniereDouleur > 1.2f)
			{
				S.DerniereDouleur = Maintenant;
				Dire(S, Perte >= Max * 0.25f ? TEXT("Effort_PainHeavy") : TEXT("Effort_Pain"), 1.f, true);
			}
			if (Sante > 0.f && Sante < Max * 0.25f && Maintenant - S.DerniereSanteBasse > 25.f)
			{
				S.DerniereSanteBasse = Maintenant;
				Dire(S, TEXT("Health_Low,Health_Critical"));
			}
			if (-Perte > Max * 0.2f && Maintenant - S.DernierSoin > 10.f)
			{
				S.DernierSoin = Maintenant;
				Dire(S, TEXT("Health_Healed"), 0.7f);
			}
		}
		S.SanteVue = Sante;
	}

	// Mort : Wraith tombe a la place du soldat (le soldat d'origine part en poupee de chiffon, cache)
	bool bMort = false;
	if (!LireBool(C, TEXT("IsDead"), bMort))
	{
		bMort = bSante && Sante <= 0.f;
	}
	USkeletalMeshComponent* Corps = S.Corps.Get();
	if (bMort && !S.bMort)
	{
		S.bMort = true;
		Dire(S, TEXT("Effort_Death,Death"), 1.f, true);
		const FHerosWraith* HM = HerosDe(S);
		if (Corps && !(HM && HM->bPoseSoldat)) // "pose soldat" : il tombe avec le soldat
		{
			Corps->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			const FHerosWraith* H = HM;
			if (H && H->AnimMort)
			{
				Corps->PlayAnimation(H->AnimMort, false);
			}
		}
	}
	else if (!bMort && S.bMort)
	{
		S.bMort = false;
		const FHerosWraith* HV = HerosDe(S);
		if (Corps && C->GetMesh() && !(HV && HV->bPoseSoldat))
		{
			Corps->AttachToComponent(C->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			Corps->SetRelativeTransform(FTransform(FQuat::Identity, FVector::ZeroVector, FVector(Echelle)));
			const FHerosWraith* H = HerosDe(S);
			if (H && H->ClasseAnim)
			{
				Corps->SetAnimationMode(EAnimationMode::AnimationBlueprint);
				Corps->SetAnimInstanceClass(H->ClasseAnim);
			}
		}
	}

	// Reste immobile une minute : petite phrase
	if (!bAir && V.Size2D() < 10.f && !S.bMort)
	{
		S.ImmobileDepuis += Dt;
		if (S.ImmobileDepuis > 60.f && !S.bVoixImmobile)
		{
			S.bVoixImmobile = true;
			Dire(S, TEXT("Status_Idle,Status_Idle_010,Status_Idle_020,ProfileIdle_010,ProfileIdle_020"));
		}
	}
	else
	{
		S.ImmobileDepuis = 0.f;
		S.bVoixImmobile = false;
	}
}

void AWraithChelles::SuivreTirs(FSoldatWraith& S)
{
	ACharacter* C = S.Soldat.Get();
	APlayerController* PC = Cast<APlayerController>(C->GetController());
	AActor* Arme = Cast<AActor>(LireObjet(C, TEXT("EquippedWeapon")));
	UWorld* W = GetWorld();
	if (!PC || !Arme || EstCorpsACorps(Arme) || S.bMort || W->IsPaused())
	{
		S.MunitionsVues = -1;
		return;
	}
	const float Maintenant = W->GetTimeSeconds();

	// L'arme a-t-elle tire ? Ses munitions baissent. (Munitions infinies : clic gauche.)
	bool bTir = false;
	bool bInfini = false;
	LireBool(Arme, TEXT("InfiniteAmmo"), bInfini);
	double Munitions = 0.0;
	if (!bInfini && LireNombre(Arme, TEXT("AmmoCurrent"), Munitions))
	{
		const int64 M = int64(Munitions);
		bTir = S.MunitionsVues >= 0 && M < S.MunitionsVues;
		S.MunitionsVues = M;
	}
	else
	{
		bTir = !PC->bShowMouseCursor && PC->IsInputKeyDown(EKeys::LeftMouseButton) && Maintenant - S.DernierTir > 0.25f;
	}
	if (bTir)
	{
		Tir(S);
	}
}

void AWraithChelles::Tir(FSoldatWraith& S)
{
	ACharacter* C = S.Soldat.Get();
	UWorld* W = GetWorld();
	const float Maintenant = W->GetTimeSeconds();
	S.DernierTir = Maintenant;

	USkeletalMeshComponent* Corps = S.Corps.Get();
	USkeletalMeshComponent* FusilFPP = S.ArmeFPP.Get();
	const bool bVue1re = C->GetMesh() && C->GetMesh()->bOwnerNoSee; // en 1re personne le corps est cache au joueur

	const FHerosWraith* H = HerosDe(S);
	UParticleSystem* EffetBouche = H ? H->EffetBouche : nullptr;
	UAnimMontage* MontageTir = H ? H->MontageTir : nullptr;
	if (bEffetsTir && EffetBouche)
	{
		USkeletalMeshComponent* Source = (bVue1re && FusilFPP && FusilFPP->IsVisible()) ? FusilFPP : Corps;
		if (Source && Source->DoesSocketExist(TEXT("Muzzle_01")))
		{
			UParticleSystemComponent* P = UGameplayStatics::SpawnEmitterAttached(EffetBouche, Source, TEXT("Muzzle_01"),
				FVector::ZeroVector, FRotator::ZeroRotator, FVector(Source == FusilFPP ? 0.5f : 1.f), EAttachLocation::SnapToTarget, true);
			if (P && Source == FusilFPP)
			{
				P->SetOnlyOwnerSee(true);
			}
		}
	}
	if (Corps && MontageTir)
	{
		if (UAnimInstance* Anim = Corps->GetAnimInstance())
		{
			Anim->Montage_Play(MontageTir, 1.f);
		}
	}
	if (Maintenant - S.DerniereVoixTir > 6.f)
	{
		S.DerniereVoixTir = Maintenant;
		Dire(S, TEXT("Effort_Ability_Primary,Effort_Ability_Primary_Fire,Effort_Attack"), 0.35f);
	}
}

// ======================================================================
//  Message a l'ecran (nom du skin)
// ======================================================================

void AWraithChelles::Annonce(const FString& Texte)
{
	if (!GEngine || !GEngine->GameViewport || !GetWorld())
	{
		return;
	}
	RetirerAnnonce();
	EcranAnnonce = SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.f, 140.f, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(Texte))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 26))
			.ColorAndOpacity(FLinearColor(1.f, 0.78f, 0.2f))
			.ShadowOffset(FVector2D(2.f, 2.f))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f))
		];
	GEngine->GameViewport->AddViewportWidgetContent(EcranAnnonce.ToSharedRef(), 40);
	FinAnnonce = GetWorld()->GetRealTimeSeconds() + 2.5f;
}

void AWraithChelles::RetirerAnnonce()
{
	if (EcranAnnonce.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(EcranAnnonce.ToSharedRef());
	}
	EcranAnnonce.Reset();
}

// ======================================================================
//  Console : Chelles.Wraith ...
// ======================================================================

void AWraithChelles::Commande(const TArray<FString>& Arguments)
{
	const FString Quoi = Arguments.Num() > 0 ? Arguments[0] : TEXT("Info");
	auto Nombre = [&Arguments](int32 i, float Defaut) { return Arguments.IsValidIndex(i) ? FCString::Atof(*Arguments[i]) : Defaut; };
	FString Reponse;

	if (Quoi.Equals(TEXT("0")))
	{
		bActif = false;
		Reponse = TEXT("Wraith coupe : soldat d'origine.");
	}
	else if (Quoi.Equals(TEXT("1")))
	{
		bActif = true;
		Reponse = TEXT("Wraith remis.");
	}
	else if (Quoi.Equals(TEXT("Skin"), ESearchCase::IgnoreCase))
	{
		ChangerSkin(int32(Nombre(1, 0.f)) - 1, true);
		return;
	}
	else if (Quoi.Equals(TEXT("Relire"), ESearchCase::IgnoreCase))
	{
		for (FSoldatWraith& S : Soldats)
		{
			Retirer(S);
		}
		LireReglages();
		Reponse = FString::Printf(TEXT("Wraith.txt relu : %d skins."), Skins.Num());
	}
	else if (Quoi.Equals(TEXT("Arme"), ESearchCase::IgnoreCase))
	{
		ArmeDecalage = FVector(Nombre(1, 0.f), Nombre(2, 0.f), Nombre(3, 0.f));
	}
	else if (Quoi.Equals(TEXT("ArmeRot"), ESearchCase::IgnoreCase))
	{
		ArmeRotation = FRotator(Nombre(1, 0.f), Nombre(2, 0.f), Nombre(3, 0.f));
	}
	else if (Quoi.Equals(TEXT("ArmeEchelle"), ESearchCase::IgnoreCase))
	{
		ArmeEchelle = FMath::Clamp(Nombre(1, 0.8f), 0.05f, 5.f);
	}
	else if (Quoi.Equals(TEXT("Params"), ESearchCase::IgnoreCase))
	{
		for (FSoldatWraith& S : Soldats)
		{
			if (S.Corps.IsValid() && Skins.IsValidIndex(S.Skin))
			{
				Recolorer(S.Corps.Get(), Skins[S.Skin], true);
			}
		}
		Reponse = TEXT("Couleurs des materiaux ecrites dans Saved\\Logs\\FPS_Chelles.log (lignes [Wraith]).");
	}

	if (Quoi.StartsWith(TEXT("Arme"), ESearchCase::IgnoreCase))
	{
		Reponse = FString::Printf(TEXT("A recopier dans Wraith.txt :  ArmeFPP.Decalage=%g,%g,%g   ArmeFPP.Rotation=%g,%g,%g   ArmeFPP.Echelle=%g"),
			ArmeDecalage.X, ArmeDecalage.Y, ArmeDecalage.Z, ArmeRotation.Pitch, ArmeRotation.Yaw, ArmeRotation.Roll, ArmeEchelle);
	}
	if (Reponse.IsEmpty())
	{
		Reponse = FString::Printf(TEXT("Wraith %s - skin %d/%d : %s - %d soldat(s) habille(s) - touche %s. Commandes : 0, 1, Skin N, Relire, Arme X Y Z, ArmeRot P Y R, ArmeEchelle E, Params"),
			bActif ? TEXT("actif") : TEXT("coupe"), SkinLocal + 1, Skins.Num(), Skins.IsValidIndex(SkinLocal) ? *Skins[SkinLocal].Nom : TEXT("?"),
			Soldats.FilterByPredicate([](const FSoldatWraith& S) { return S.Corps.IsValid(); }).Num(), *Touche.ToString());
	}
	UE_LOG(LogTemp, Log, TEXT("[Wraith] %s"), *Reponse);
	Annonce(Reponse);
}
