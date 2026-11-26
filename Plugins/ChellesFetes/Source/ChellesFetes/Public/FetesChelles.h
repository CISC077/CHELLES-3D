// CHELLES 3D - Evenements du calendrier (UE 4.27)
// Selon la date du PC (ou le reglage Mode de Fetes.txt) :
//  - HALLOWEEN (25/10 au 02/11)      : citrouilles lumineuses, fantomes qui flottent, passants a tete de citrouille,
//                                      ambiance sonore inquietante la nuit.
//  - 14 JUILLET (13/07 au 15/07)     : feux d'artifice bleu blanc rouge (et autres couleurs) au-dessus du quartier.
//  - FETE DE LA MUSIQUE (20/06 au 22/06) : une scene avec projecteurs, de la musique, et des PNJ qui dansent.
// Rien n'est modifie dans la map : tout est cree au lancement de la partie (Play) et disparait a la fin.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FetesChelles.generated.h"

class UAnimationAsset;
class UAudioComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class USoundBase;
class USpotLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

UENUM()
enum class EFeteChelles : uint8
{
	Aucune,
	Halloween,
	QuatorzeJuillet,
	FeteMusique
};

/** Reglages lus dans Plugins/ChellesFetes/Fetes.txt */
struct FReglagesFetes
{
	bool bActif = true;
	FString Mode = TEXT("Auto");   // Auto / Halloween / 14Juillet / Musique / Non
	FString Carte = TEXT("Rachid"); // la fete n'a lieu que sur cette carte (Toutes = partout)

	// Dates (MMJJ, bornes comprises)
	int32 HalloweenDebut = 1025, HalloweenFin = 1102;
	int32 JuilletDebut = 713, JuilletFin = 715;
	int32 MusiqueDebut = 620, MusiqueFin = 622;

	// Halloween
	int32 Citrouilles = 40;
	float CitrouillesRayon = 150.f;     // metres autour du depart
	float TailleCitrouille = 60.f;      // cm
	int32 BougiesMax = 24;
	int32 Fantomes = 8;
	int32 TetesCitrouille = 35;         // % des passants
	FString CitrouilleMaillage;         // vide = citrouille fabriquee
	FString AmbianceHalloween = TEXT("/Game/Audio/Menu/zapsplat_horror_landscape_drone_dark_night_43217");
	float VolumeAmbiance = 0.35f;

	// 14 juillet
	FString LieuFeux = TEXT("Joueur");  // Joueur / Depart / X,Y,Z
	float DistanceFeux = 160.f;         // metres
	float HauteurFeux = 90.f;           // metres
	float PauseSpectacle = 120.f;       // secondes entre deux spectacles
	float DureeSpectacle = 70.f;        // secondes
	int32 EtincellesParBouquet = 90;
	float TailleEtincelle = 1.f;
	float ForceLumiere = 60.f;
	float VolumeFeux = 0.7f;
	FString SonExplosion = TEXT("/Game/Audio/Explosion_Cue");
	FString SonDepart = TEXT("/Game/Audio/SoundFX/Whoosh2_Cue");
	TArray<FLinearColor> CouleursFeux;

	// Fete de la musique
	int32 Danseurs = 12;
	TArray<FString> MaillagesDanseurs;
	TArray<FString> Danses;
	TArray<FString> Musiques;
	float VolumeMusique = 0.8f;
	float PorteeMusique = 70.f;         // metres
	float RotationDanseurs = -90.f;     // sens du maillage (personnages Unreal : -90)
};

/** Une fusee de feu d'artifice qui monte, puis eclate. */
struct FFuseeFete
{
	FVector Position = FVector::ZeroVector;
	FVector Vitesse = FVector::ZeroVector;
	float Duree = 2.f;      // temps de montee
	float Age = 0.f;
	int32 Couleur = 0;
	int32 Forme = 0;        // 0 boule, 1 anneau, 2 palmier, 3 double
};

/** Une etincelle apres l'explosion. */
struct FEtincelleFete
{
	FVector Position = FVector::ZeroVector;
	FVector Vitesse = FVector::ZeroVector;
	float Age = 0.f;
	float Vie = 2.f;
	int32 Couleur = 0;
	float Taille = 1.f;
};

/** Un fantome qui flotte en rond. */
struct FFantomeFete
{
	USceneComponent* Racine = nullptr;
	FVector Centre = FVector::ZeroVector;
	float Rayon = 500.f;
	float Vitesse = 0.3f;
	float Phase = 0.f;
	float Hauteur = 400.f;
};

UCLASS(NotPlaceable, Transient)
class CHELLESFETES_API AFetesChelles : public AActor
{
	GENERATED_BODY()

public:
	AFetesChelles();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Console : Chelles.Fetes [Info|Auto|Halloween|Juillet|Musique|Non|Feux|PoserScene|RetirerScene|Relire] */
	void Commande(const TArray<FString>& Arguments);

	/** La fete en cours (lue par d'autres plugins si besoin). */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Fetes")
	EFeteChelles FeteEnCours = EFeteChelles::Aucune;

private:
	void LireReglages();
	EFeteChelles FeteDuJour() const;
	EFeteChelles FeteVoulue() const;
	void Lancer(EFeteChelles Fete);
	void Arreter();
	static FString NomFete(EFeteChelles Fete);

	// Outils
	FVector Depart() const;
	bool PointDeVue(FVector& Lieu, FRotator& Direction) const;
	bool TrouverSol(const FVector& XY, FVector& Sol, bool bHerbeSeulement) const;
	bool PlaceLibre(const FVector& Sol, float Rayon, float Hauteur) const;
	bool EstNuit() const;
	UStaticMeshComponent* Piece(USceneComponent* Parent, UStaticMesh* Maillage, UMaterialInterface* Materiau,
		const FVector& Position, const FRotator& Rotation, const FVector& Echelle, AActor* Proprietaire = nullptr);
	UMaterialInterface* Couleur(const FLinearColor& Teinte, float Force = 0.f, bool bLumineux = false);
	UMaterialInterface* Fantome(float Opacite);
	USceneComponent* NouvelleRacine(const FTransform& Place);

	// Halloween
	void LancerHalloween();
	USceneComponent* FabriquerCitrouille(USceneComponent* Parent, const FVector& Position, float Lacet, float Taille,
		bool bBougie, AActor* Proprietaire = nullptr);
	void FabriquerFantome(const FVector& Centre, FRandomStream& Hasard);
	void CoifferPassants();
	void AnimerHalloween(float DeltaSeconds);

	// 14 juillet
	void LancerJuillet();
	void PreparerFeux();
	void DemarrerSpectacle(float Duree);
	void TirerFusee();
	void Eclater(const FFuseeFete& Fusee);
	void AnimerFeux(float DeltaSeconds);

	// Fete de la musique
	void LancerMusique();
	bool PlaceScene(FVector& Sol, float& Lacet) const;
	void FabriquerScene(const FVector& Sol, float Lacet);
	void PoserSceneDevantJoueur();
	void RetirerSceneEnregistree();
	void AnimerMusique(float DeltaSeconds);
	void JouerMorceau();

	UFUNCTION()
	void MorceauFini();

	FReglagesFetes Reglages;
	int32 ModeForce = -1;   // -1 = selon Fetes.txt, sinon EFeteChelles

	/** Tout ce que la fete a cree (detruit a l'arret). */
	UPROPERTY(Transient)
	TArray<UActorComponent*> Crees;

	UPROPERTY(Transient)
	TMap<FString, UMaterialInterface*> Materiaux;

	UPROPERTY(Transient)
	UMaterialInterface* MateriauLumiere = nullptr;

	UPROPERTY(Transient)
	UMaterialInterface* MateriauFantome = nullptr;

	UPROPERTY(Transient)
	UMaterialInterface* MateriauBase = nullptr;

	UPROPERTY(Transient)
	UStaticMesh* Sphere = nullptr;

	UPROPERTY(Transient)
	UStaticMesh* Cube = nullptr;

	UPROPERTY(Transient)
	UStaticMesh* Cylindre = nullptr;

	UPROPERTY(Transient)
	UStaticMesh* Cone = nullptr;

	float Temps = 0.f;

	// Halloween
	UPROPERTY(Transient)
	TArray<UPointLightComponent*> Bougies;

	TArray<FFantomeFete> Fantomes;
	TArray<TWeakObjectPtr<USceneComponent>> Coiffes;
	TSet<FName> PassantsVus;
	float ProchainCoiffage = 0.f;

	UPROPERTY(Transient)
	UAudioComponent* Ambiance = nullptr;

	// 14 juillet
	UPROPERTY(Transient)
	TArray<UInstancedStaticMeshComponent*> Etincelles;   // une par couleur

	UPROPERTY(Transient)
	TArray<UPointLightComponent*> Eclairs;

	TArray<float> ForceEclairs;
	TArray<FFuseeFete> Fusees;
	TArray<FEtincelleFete> Points;
	float FinSpectacle = 0.f;
	float ProchainSpectacle = 0.f;
	float ProchaineFusee = 0.f;
	int32 ProchainEclair = 0;

	UPROPERTY(Transient)
	USoundBase* SonBoum = nullptr;

	UPROPERTY(Transient)
	USoundBase* SonFusee = nullptr;

	UPROPERTY(Transient)
	class USoundAttenuation* PorteeFeux = nullptr;

	/** Le bruit arrive apres l'eclair (vitesse du son). */
	TArray<TPair<float, FVector>> BoumsEnAttente;
	FVector CentreFeux = FVector::ZeroVector;

	// Fete de la musique
	UPROPERTY(Transient)
	USceneComponent* Scene = nullptr;

	UPROPERTY(Transient)
	TArray<USpotLightComponent*> Projecteurs;

	UPROPERTY(Transient)
	TArray<UMaterialInstanceDynamic*> Lumieres;

	UPROPERTY(Transient)
	UAudioComponent* Musique = nullptr;

	int32 Morceau = 0;
};
