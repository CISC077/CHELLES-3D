// CHELLES 3D - Wraith (pack gratuit Paragon: Wraith) a la place du SOLDAT (UE 4.27)
//
// Rien n'est modifie dans FPS_Game : pendant la partie, sur chaque soldat joueur,
//  - le corps du soldat est cache (il continue de marcher, tirer, etc. : rien ne change dans le jeu)
//  - Wraith est pose a sa place, anime par son propre Animation Blueprint, son fusil dans les mains
//  - en 1re personne, le fusil de Wraith remplace l'arme du soldat a l'ecran
//  - voix de Wraith (saut, douleur, mort...) et eclair du fusil de Wraith a chaque tir
//  - touche K : skin suivant (Wraith.txt). En multijoueur, le skin passe par AWraithRelais.
//  - heros "pose soldat" (Sci-Fi Soldier, squelette mannequin) : copie les mouvements du soldat, garde ses armes
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

/** Une animation simple (heros sans Animation Blueprint, ex. LENA de Mixamo). Fige >= 0 : image figee a ce temps (en secondes). */
struct FAnimSimpleWraith
{
	UAnimSequenceBase* Anim = nullptr;
	float Fige = -1.f;
};

/** Etats des animations simples : repos, marche, course, vol, accroupi, chute (ordre de la ligne Heros=Nom|Anims:...). */
enum class EEtatAnimWraith : uint8 { Repos, Marche, Course, Vol, Accroupi, Chute, Nombre };

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
	bool bPoseSoldat = false;       // AnimBP "Soldat" : le corps copie les mouvements du soldat (squelette mannequin)
	bool bAnimsSimples = false;     // "Anims:repos;marche;course;vol;accroupi;chute" : animations jouees selon la vitesse (LENA)
	FAnimSimpleWraith Anims[(int32)EEtatAnimWraith::Nombre];
	float Taille = 1.f;             // taille du corps de ce heros (1 = celle du modele)
	float Lacet = 0.f;              // rotation du corps (degres) si le modele regarde de cote
	TArray<UAnimSequenceBase*> Danses; // ligne Danses=Nom|anim;anim... : jouees a la suite (touche des danses)
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
	FString NomHeros;              // Wraith, Dekker, SciFi...
	UMaterialInterface* Materiau = nullptr; // tenue : materiau pose sur tout le corps (facultatif)
	int32 Heros = 0;
};

/** Une ligne Tenue= de Wraith.txt : couleur d'un heros de la roue (Spider-Male, Spider-Femelle, PANTERA). */
struct FTenueHeros
{
	FString Mot;                   // morceau du nom du corps, ex. SK_SpiderMan
	FString Nom;                   // nom affiche
	float Teinte = 0.f;            // decalage de couleur (0 a 1 = tour du cercle des couleurs)
	FLinearColor Unie = FLinearColor::Black;
	float Melange = 0.f;           // 0 = couleurs du costume, 1 = couleur unie
	TArray<FString> Materiaux;     // materiaux qui prennent la couleur unie (vide = tous)
};

/** Un corps de heros qu'on a colore (pour le remettre comme avant). */
struct FTenueComposant
{
	TWeakObjectPtr<USkeletalMeshComponent> Composant;
	TWeakObjectPtr<USkeletalMesh> Maillage;
	int32 Tenue = -2;
	TArray<TWeakObjectPtr<UMaterialInterface>> Avant;   // surcharges d'origine (par emplacement)
	TArray<TWeakObjectPtr<UMaterialInterface>> Poses;   // nos materiaux (par emplacement, vide = pas touche)
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

	// Animations simples (LENA)
	TWeakObjectPtr<UAnimSequenceBase> AnimJouee;
	int32 EtatAnim = -1;
	bool bDanse = false;            // en train de danser
	int32 DanseIndex = 0;
	float DanseDebut = 0.f;
	float DanseFin = 0.f;           // fin de la danse en cours (temps du jeu)

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
	/** Choix venu du menu HABILLAGE (plugin ChellesMenu) : GameUserSettings [ChellesWraith] Demande=NOM DU SKIN. */
	void LireDemandeMenu();
	float ProchaineDemande = 0.f;
	int32 SkinParNom(const FString& Nom) const;
	int32 SkinDe(ACharacter* Soldat) const;
	AWraithRelais* MonRelais() const;
	float ProchainEnvoi = 0.f;
	float ProchainRelais = 0.f;
	void GererRelais();

	// ---- Habillage ----
	bool EstSoldat(ACharacter* C) const;
	/** Corps d'origine du soldat : SK_Mannequin, ou SK_SpiderMan_UE4 (le soldat du jeu, BP_Character_SpiderMan). */
	static bool EstCorpsDeSoldat(const USkeletalMesh* Corps);
	void AnimerSimple(FSoldatWraith& S);
	bool AnimerDanse(FSoldatWraith& S, const FHerosWraith& H, float Vitesse, bool bEnLair);

	// ---- Danses (LENA, SOFIA, LUCIA...) ----
	FKey ToucheDanse;
	bool bDanseAlt = false;
	bool bDanseCtrl = false;
	bool bDanseShift = false;
	bool bDanseArretSeulement = false;
	bool bDanseLocale = false;
	TMap<FString, TArray<FString>> DansesLues;    // Danses=Nom|... (relie aux heros a la fin de LireReglages)
	bool DanseDe(ACharacter* C) const;
	/** 25/11/2026 : chaque appui sur la touche des danses (J) = danse suivante. Numero voulu (moi : local, autres : relais). */
	int32 DanseVoulueLocale = 0;
	int32 NumeroDanseDe(ACharacter* C) const;
	void Danser(bool bDanser, bool bAnnoncer);
	bool ToucheDanseAppuyee(APlayerController* PC) const;
	FString NomToucheDanse() const;
	FTransform PlacementCorps(const FHerosWraith& H) const;
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

	// ---- Tenues des heros de la roue (touche K) ----
	TArray<FTenueHeros> Tenues;
	TMap<FString, int32> TenueLocale;              // mot du heros -> tenue choisie (-1 = origine)
	TArray<FTenueComposant> Colores;
	float ProchainesTenues = 0.f;
	bool bAvisTenues = false;
	FString MotTenue(const USkeletalMeshComponent* C) const;
	int32 TenueDe(ACharacter* C, const FString& Mot) const;
	void TenueSuivante(ACharacter* Moi);
	void SuivreTenues();
	bool PoserTenue(FTenueComposant& T, int32 Index);
	static void RemettreTenue(FTenueComposant& T);
	void RetirerTenues();

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
