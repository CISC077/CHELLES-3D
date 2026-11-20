// CHELLES 3D - Drone compagnon (ajoute tout seul au lancement du jeu)
//
//  - Chaque joueur a son drone : il vole a cote de lui (au-dessus de l'epaule droite) et le suit partout,
//    a pied, en vol ou en vehicule.
//  - Le drone repere l'ennemi ou le zombie le plus proche (a portee et visible) et lui tire dessus au laser.
//    Les tirs comptent comme ceux du joueur : les regles des plugins Ennemis et Zombies sont respectees
//    (le drone vise le torse : il faut plusieurs tirs, il ne tue pas d'un coup a la tete).
//  - F10 (reglable) = ranger le drone / le rappeler.
//  - Multijoueur : le serveur decide des cibles et des degats, tout le monde voit les drones et les tirs.
//  Reglages : Plugins\ChellesDrone\Drone.txt (Bloc-notes). Rien n'est ecrit dans les fichiers du projet.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "Sound/SoundWaveProcedural.h"
#include "DroneChelles.generated.h"

class APawn;
class APlayerController;
class UAudioComponent;
class UMaterialInstanceDynamic;
class UMeshComponent;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Reglages lus dans Plugins\ChellesDrone\Drone.txt (les memes pour tout le monde). */
struct FReglagesDrone
{
	bool bActif = true;
	/** Oui = le drone est deja sorti quand le joueur arrive dans la partie. */
	bool bSortiAuDepart = true;
	FKey Touche = EKeys::F10;

	// Combat
	float PorteeMetres = 25.f;
	float TirsParSeconde = 2.f;
	/** Degats d'un tir (5 ou plus = compte comme une balle dans le corps pour les ennemis et zombies Chelles). */
	float Degats = 20.f;
	/** Non = le drone vise le torse (plusieurs tirs). Oui = la tete (un zombie meurt d'un tir : trop facile). */
	bool bViserTete = false;
	/** Le drone ne tire pas quand son joueur est dans un vehicule. */
	bool bTirerEnVehicule = true;

	// Place a cote du joueur (cm)
	float Hauteur = 150.f;
	float Cote = 90.f;
	float Arriere = 70.f;
	float Souplesse = 4.f;

	// Apparence
	/** Modele 3D du drone (SkeletalMesh ou StaticMesh). Vide ou introuvable = drone en formes simples. */
	FString Modele = TEXT("/Game/PoliceDr\u00F4ne/Police+Drone+Model");
	/** Animation jouee en boucle sur le modele (helices...). Vide = aucune. */
	FString Animation = TEXT("/Game/PoliceDr\u00F4ne/Police+Drone+Model_Anim");
	/** Envergure du modele en cm (le modele est agrandi ou reduit pour faire cette taille). */
	float TailleModeleCm = 90.f;
	/** Pour remettre le modele dans le bon sens s'il vole de cote (90, -90, 180). */
	float RotationModele = 0.f;
	float Taille = 1.f;
	FLinearColor CouleurCorps = FLinearColor(0.04f, 0.04f, 0.05f);
	FLinearColor CouleurBras = FLinearColor(0.85f, 0.45f, 0.02f);
	FLinearColor CouleurLaser = FLinearColor(0.1f, 0.85f, 1.f);

	// Sons
	float VolumeTir = 0.6f;
	float VolumeMoteur = 0.25f;

	/** Morceaux de nom de classe en plus a traiter en ennemis (ex. BP_ZOMBIE). */
	TArray<FString> ClassesEnnemies = { TEXT("Zombie"), TEXT("BP_AI_Char"), TEXT("Bandit") };
	/** Morceaux de nom de classe que le drone ne vise jamais (passants, policiers...). */
	TArray<FString> ClassesIgnorees = { TEXT("Passant"), TEXT("Policier"), TEXT("Police"), TEXT("Merchant") };
};

/** Lit Drone.txt (une seule fois par partie). */
CHELLESDRONE_API const FReglagesDrone& ReglagesDrone();
CHELLESDRONE_API void RechargerReglagesDrone();

/** Sons du drone fabriques par le code (aucun fichier son a importer). */
UCLASS()
class CHELLESDRONE_API USonDroneChelles : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	USonDroneChelles(const FObjectInitializer& ObjectInitializer);

	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

	/** 0 = bourdonnement des helices (en boucle), 1 = "piou" du laser, 2 = bip (rappel), 3 = bip descendant (rangement). */
	int32 Genre = 0;

private:
	// Utilises uniquement par le fil audio
	float Phase = 0.f;
	float Phase2 = 0.f;
	int64 Echantillon = 0;
	uint32 Graine = 22222u;
	float Bruit = 0.f;
};

/** Le drone d'un joueur (pose par le serveur, vu par tout le monde). */
UCLASS(NotPlaceable, Transient)
class CHELLESDRONE_API ADroneChelles : public AActor
{
	GENERATED_BODY()

public:
	ADroneChelles();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Le personnage que le drone accompagne (change avec la roue Tab, les vehicules...). */
	UPROPERTY(Replicated)
	APawn* Maitre = nullptr;

	/** Faux = drone range (invisible, ne tire pas). */
	UPROPERTY(ReplicatedUsing = OnRep_Sorti)
	bool bSorti = true;

	/** Ce que le drone vise en ce moment (tout le monde le voit tourner son canon vers lui). */
	UPROPERTY(Replicated)
	AActor* Cible = nullptr;

	/** Le joueur proprietaire demande de ranger / rappeler son drone. */
	UFUNCTION(Server, Reliable)
	void ServeurBasculer();

	/** Serveur : range ou rappelle le drone. */
	void Basculer();

	bool EstServeur() const;
	FString Resume() const;

	/** Personnage ennemi des joueurs ? (ennemis FPS_Game, ennemis et zombies Chelles). */
	static bool EstEnnemi(AActor* Acteur);
	/** Ennemi encore debout ? */
	static bool EstVivant(AActor* Acteur);

protected:
	UFUNCTION()
	void OnRep_Sorti();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastTir(FVector_NetQuantize Depart, FVector_NetQuantize Fin);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSon(uint8 Genre);

private:
	void Construire();
	/** Pose le modele 3D de Drone.txt. Rend faux s'il est introuvable (on garde alors les formes simples). */
	bool ConstruireModele(FVector& PointCanon);
	UStaticMeshComponent* AjouterForme(USceneComponent* Parent, UStaticMesh* Forme, const FVector& Place, const FRotator& Rotation,
		const FVector& Echelle, const FLinearColor& Couleur, bool bOmbre = false);
	void Suivre(float DeltaSeconds, bool bImmediat);
	void Animer(float DeltaSeconds);
	void TickServeur(float Maintenant);
	void TickTouches();
	void AppliquerVisibilite();
	void JouerSon(int32 Genre, float Volume);
	AActor* ChoisirCible() const;
	bool PointDeVisee(AActor* Ennemi, FVector& Point, FName& Os) const;
	bool EstVisible(AActor* Ennemi, const FVector& Point) const;
	bool MaitreDisponible() const;
	FVector BoutDuCanon() const;

	// Ce qu'on voit
	UPROPERTY(Transient) USceneComponent* Racine = nullptr;
	UPROPERTY(Transient) USceneComponent* Corps = nullptr;
	UPROPERTY(Transient) USceneComponent* Tourelle = nullptr;
	/** Le modele 3D choisi dans Drone.txt (PoliceDrone) : remplace les formes simples. */
	UPROPERTY(Transient) UMeshComponent* ModeleDrone = nullptr;
	UPROPERTY(Transient) TArray<UStaticMeshComponent*> Helices;
	UPROPERTY(Transient) UPointLightComponent* Oeil = nullptr;
	UPROPERTY(Transient) UPointLightComponent* Eclair = nullptr;
	UPROPERTY(Transient) UAudioComponent* Moteur = nullptr;
	UPROPERTY(Transient) USonDroneChelles* SonMoteur = nullptr;
	UPROPERTY(Transient) UMaterialInstanceDynamic* MatLaser = nullptr;
	UPROPERTY(Transient) UStaticMesh* FormeCylindre = nullptr;
	UPROPERTY(Transient) class USoundAttenuation* Attenuation = nullptr;
	UPROPERTY(Transient) class UParticleSystem* Etincelles = nullptr;

	struct FRayon
	{
		TWeakObjectPtr<UStaticMeshComponent> Forme;
		float Age = 0.f;
	};
	TArray<FRayon> Rayons;

	bool bConstruit = false;
	bool bPlace = false;
	FVector Vitesse = FVector::ZeroVector;
	float Temps = 0.f;
	float AgeEclair = 10.f;
	float RotationHelices = 0.f;

	// Serveur
	float ProchainChoix = 0.f;
	float ProchainTir = 0.f;
};

/** Pose un drone pour chaque joueur et le tient a jour (seulement sur le serveur ou en solo). */
UCLASS(NotPlaceable, Transient)
class CHELLESDRONE_API ADronesChelles : public AActor
{
	GENERATED_BODY()

public:
	ADronesChelles();

	virtual void Tick(float DeltaSeconds) override;

	ADroneChelles* DroneDe(APlayerController* Joueur) const;
	void Resume() const;

private:
	TMap<TWeakObjectPtr<APlayerController>, TWeakObjectPtr<ADroneChelles>> Drones;
};
