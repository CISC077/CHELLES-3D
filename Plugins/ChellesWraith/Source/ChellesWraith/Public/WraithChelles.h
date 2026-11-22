// CHELLES 3D - Wraith (pack gratuit Paragon: Wraith) a la place du SOLDAT (UE 4.27)
//
// Rien n'est modifie dans FPS_Game : pendant la partie, sur chaque soldat joueur,
//  - le corps du soldat est cache (il continue de marcher, tirer, etc. : rien ne change dans le jeu)
//  - Wraith est pose a sa place, anime par son propre Animation Blueprint, son fusil dans les mains
//  - en 1re personne, le fusil de Wraith remplace l'arme du soldat a l'ecran
//  - voix de Wraith (saut, douleur, mort...) et eclair du fusil de Wraith a chaque tir
//  - touche K : skin suivant (Wraith.txt). En multijoueur, le skin passe par AWraithRelais.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "WraithChelles.generated.h"

class ACharacter;
class APlayerController;
class AWraithRelais;
class SWidget;
class UAnimMontage;
class UAnimSequenceBase;
class UMaterialInterface;
class UMeshComponent;
class UParticleSystem;
class UPrimitiveComponent;
class USceneComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UAudioComponent;
class USoundBase;

/** Un heros Paragon (ligne Heros= de Wraith.txt) : ses animations, sa voix, son arme. */
struct FHerosWraith
{
	FString Nom;
	FString DossierVoix;            // dossier des SoundCue, ex. /Game/ParagonWraith/Audio/Cues/
	FString Prefixe;                // debut du nom des repliques, ex. Wraith
	TArray<FString> MotsArme;       // materiaux de l'arme (pour ne montrer qu'elle en 1re personne)
	FName OsPoignee = TEXT("weapon_r");
	FName OsArriere;
	FName OsAvant;
	FName OsHaut;
	UClass* ClasseAnim = nullptr;
	UAnimMontage* MontageTir = nullptr;
	UAnimSequenceBase* AnimMort = nullptr;
	UParticleSystem* EffetBouche = nullptr;
};

/** Une ligne Skin= de Wraith.txt */
struct FSkinWraith
{
	FString Nom;
	FString Corps;                 // chemin du SkeletalMesh, ou "Soldat"
	bool bSoldat = false;          // garder le soldat d'origine
	bool bRecolorer = false;
	FLinearColor Teinte = FLinearColor::Red;
	float Assombrir = 1.f;
	bool bBras = false;
	FLinearColor Bras = FLinearColor(0.3f, 0.3f, 0.3f);
	FString NomHeros;              // Wraith, Dekker...
	int32 Heros = 0;
};

/** Un composant du soldat qu'on a cache (remis comme avant a la fin). */
struct FCacheWraith
{
	TWeakObjectPtr<UPrimitiveComponent> Composant;
	bool bRenduPrincipal = true;
	bool bOmbre = true;
};

/** Materiaux d'origine d'un composant (bras / jambes 1re personne). */
struct FMateriauxWraith
{
	TWeakObjectPtr<UMeshComponent> Composant;
	TArray<TWeakObjectPtr<UMaterialInterface>> Avant;
};

/** Un soldat habille en Wraith. */
struct FSoldatWraith
{
	TWeakObjectPtr<ACharacter> Soldat;
	TWeakObjectPtr<USkeletalMeshComponent> Corps;    // Wraith (3e personne, vu par les autres)
	TWeakObjectPtr<USkeletalMeshComponent> ArmeFPP;  // fusil de Wraith vu en 1re personne
	TArray<FCacheWraith> CachesCorps;                // corps du soldat, habillage, arme 3e personne
	TArray<FCacheWraith> CachesArme;                 // arme 1re personne
	TArray<FMateriauxWraith> Teintes;                // bras / jambes 1re personne teintes
	TWeakObjectPtr<AActor> ArmeSuivie;
	TWeakObjectPtr<UMeshComponent> MaillageFPPSuivi;
	int32 Skin = -1;
	int32 Heros = -1;
	int32 SkinBras = -1;
	bool bIntroJouee = false;

	// Voix / effets
	bool bEnLair = false;
	float VitesseZMin = 0.f;
	float SanteVue = -1.f;
	bool bMort = false;
	float DerniereDouleur = -100.f;
	float DerniereSanteBasse = -100.f;
	float DernierSoin = -100.f;
	float DerniereVoixTir = -100.f;
	float ImmobileDepuis = 0.f;
	bool bVoixImmobile = false;
	int64 MunitionsVues = -1;
	float DernierTir = -100.f;
	TWeakObjectPtr<UAudioComponent> VoixEnCours;
};

UCLASS(NotPlaceable, Transient)
class CHELLESWRAITH_API AWraithChelles : public AActor
{
	GENERATED_BODY()

public:
	AWraithChelles();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Console (touche ²) : Chelles.Wraith [Info|0|1|Skin N|Arme X Y Z|ArmeRot P Y R|ArmeEchelle E|Relire|Params] */
	void Commande(const TArray<FString>& Arguments);

private:
	// ---- Reglages ----
	void LireReglages();
	bool bActif = true;
	FKey Touche;
	TArray<FSkinWraith> Skins;
	TArray<FHerosWraith> Heros;
	const FHerosWraith* HerosDe(const FSoldatWraith& S) const;
	float Echelle = 1.f;
	bool bArmeWraith = true;
	FVector ArmeDecalage = FVector::ZeroVector;
	FRotator ArmeRotation = FRotator::ZeroRotator;
	float ArmeEchelle = 0.8f;
	bool bEffetsTir = true;
	bool bBrasCouleur = true;
	float Voix = 0.8f;

	// ---- Skin choisi par ce joueur ----
	int32 SkinLocal = 0;
	void ChangerSkin(int32 Nouveau, bool bAnnoncer);
	int32 SkinDe(ACharacter* Soldat) const;
	AWraithRelais* MonRelais() const;
	float ProchainEnvoi = 0.f;
	float ProchainRelais = 0.f;
	void GererRelais();

	// ---- Habillage ----
	bool EstSoldat(ACharacter* C) const;
	void Chercher();
	void Suivre(FSoldatWraith& S, float Dt);
	void Appliquer(FSoldatWraith& S, const FSkinWraith& Skin, int32 Index);
	void Retirer(FSoldatWraith& S);
	void CacherCorpsSoldat(FSoldatWraith& S);
	void SuivreArme(FSoldatWraith& S, USkeletalMesh* Corps);
	void RetirerArme(FSoldatWraith& S);
	void TeindreBras(FSoldatWraith& S, const FSkinWraith& Skin, int32 Index);
	void RemettreBras(FSoldatWraith& S);
	void Recolorer(USkeletalMeshComponent* C, const FSkinWraith& Skin, bool bJournal);
	static void MontrerFusilSeul(USkeletalMeshComponent* C, const TArray<FString>& MotsArme);
	FTransform PlacementArme(const FHerosWraith& H, USkeletalMesh* Corps, UMeshComponent* MaillageFPP, AActor* Arme) const;

	static void Cacher(TArray<FCacheWraith>& Liste, UPrimitiveComponent* C);
	static void Remettre(TArray<FCacheWraith>& Liste);

	// ---- Voix / effets ----
	void SuivreVoix(FSoldatWraith& S, float Dt);
	void SuivreTirs(FSoldatWraith& S);
	void Tir(FSoldatWraith& S);
	void Dire(FSoldatWraith& S, const TCHAR* Repliques, float Chance = 1.f, bool bForcer = false);

	// ---- Message a l'ecran ----
	void Annonce(const FString& Texte);
	void RetirerAnnonce();
	TSharedPtr<SWidget> EcranAnnonce;
	float FinAnnonce = 0.f;

	APlayerController* GetPC() const;
	USkeletalMesh* Charger(const FString& Chemin) const;

	TArray<FSoldatWraith> Soldats;
	float ProchaineRecherche = 0.f;
	bool bAvisPack = false;

	/** Animations, montages, effets des heros : gardes en memoire. */
	UPROPERTY()
	TArray<UObject*> Gardes;

	UPROPERTY()
	TArray<USkeletalMesh*> CorpsCharges;

	UPROPERTY()
	TMap<FString, USoundBase*> Sons;
};
