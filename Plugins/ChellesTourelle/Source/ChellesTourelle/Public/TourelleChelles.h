// CHELLES 3D - Tourelle automatique BP_Turet2026
//
//  - Posee sur la carte (glissee dans l'editeur ou posee par un joueur depuis l'inventaire I),
//    elle tourne vers l'ennemi ou le zombie le plus proche qu'elle voit et lui tire dessus.
//  - Ennemis vises : ennemis FPS_Game (dossier Bonus/AI, bandits), ennemis ChellesEnnemis,
//    zombies ChellesZombies. Jamais les joueurs, passants, policiers ni marchands.
//  - Multijoueur : le serveur choisit la cible et applique les degats, tout le monde voit la tourelle
//    tourner et tirer.
//  - Le Blueprint BP_Turet2026 (Content\ScifiGun) est un enfant de cette classe : il donne seulement
//    le modele 3D (voir Tools\Tourelle\creer_bp_turet2026.py). Reglages communs : Plugins\ChellesTourelle\Tourelle.txt
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "Sound/SoundWaveProcedural.h"
#include "TourelleChelles.generated.h"

class AController;
class UMaterialInstanceDynamic;
class UParticleSystem;
class UPointLightComponent;
class UPoseableMeshComponent;
class USkeletalMesh;
class USoundAttenuation;
class UStaticMesh;
class UStaticMeshComponent;

/** Reglages lus dans Plugins\ChellesTourelle\Tourelle.txt (les memes pour tout le monde). */
struct FReglagesTourelle
{
	bool bActif = true;

	// ---- Combat
	float PorteeMetres = 30.f;
	float TirsParSeconde = 4.f;
	/** Degats d'un tir (5 ou plus = une balle dans le corps pour les ennemis et zombies Chelles). */
	float Degats = 20.f;
	bool bViserTete = false;
	/** Vitesse a laquelle la tourelle tourne vers sa cible (plus grand = plus vif). */
	float VitesseRotation = 6.f;
	FLinearColor CouleurTraceur = FLinearColor(1.f, 0.55f, 0.05f);
	float VolumeTir = 0.7f;

	// ---- Cadeau apres X minutes de jeu
	float MinutesAvantCadeau = 50.f;
	/** Vrai = temps de jeu total (toutes les parties). Faux = il faut rester X minutes dans la meme partie. */
	bool bTempsTotal = true;
	/** Combien de tourelles un joueur peut poser en meme temps. */
	int32 TourellesParJoueur = 1;
	/** Tourelle posee par les joueurs (le Blueprint du dossier ScifiGun). */
	FString ClassePosee = TEXT("/Game/ScifiGun/BP_Turet2026.BP_Turet2026_C");
	/** Distance devant le joueur ou la tourelle est posee (cm). */
	float DistancePose = 250.f;
	/** Ferme le menu d'inventaire apres avoir pose la tourelle. */
	bool bFermerInventaire = true;
	/** Touche rapide pour poser / ramasser sans ouvrir l'inventaire (vide = aucune). */
	FKey ToucheRapide;
	/** Morceaux de nom des menus d'inventaire a cote desquels la case TOURELLE s'affiche. */
	TArray<FString> MenusInventaire = { TEXT("W_WeaponInventory_UI") };
	/** Place de la case dans l'ecran : Droite, Gauche, Haut, Bas. */
	FString PlaceCase = TEXT("Droite");

	/** Morceaux de nom de classe en plus a traiter en ennemis. */
	TArray<FString> ClassesEnnemies = { TEXT("Zombie"), TEXT("BP_AI_Char"), TEXT("Bandit") };
	/** Morceaux de nom de classe jamais vises. */
	TArray<FString> ClassesIgnorees = { TEXT("Passant"), TEXT("Policier"), TEXT("Police"), TEXT("Merchant") };
};

CHELLESTOURELLE_API const FReglagesTourelle& ReglagesTourelle();
CHELLESTOURELLE_API void RechargerReglagesTourelle();

/** Bruit de tir fabrique par le code (aucun fichier son a importer). */
UCLASS()
class CHELLESTOURELLE_API USonTourelleChelles : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	USonTourelleChelles(const FObjectInitializer& ObjectInitializer);
	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

	/** 0 = rafale de tir, 1 = bip de pose, 2 = bip de rangement, 3 = jingle du cadeau. */
	int32 Genre = 0;

private:
	float Phase = 0.f;
	int64 Echantillon = 0;
	uint32 Graine = 51515u;
	float Bruit = 0.f;
};

/**
 * La tourelle. A poser telle quelle (formes simples) ou, mieux, BP_Turet2026 avec le modele du dossier ScifiGun.
 * Le modele peut etre :
 *  - deux StaticMesh : ModelePied (ne bouge pas) + ModeleCanon (tourne vers la cible) ;
 *  - un seul StaticMesh dans ModeleCanon (toute la tourelle tourne) ;
 *  - un SkeletalMesh dans ModeleSquelette : seul l'os OsQuiTourne tourne vers la cible.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Tourelle Chelles"))
class CHELLESTOURELLE_API ATourelleChelles : public AActor
{
	GENERATED_BODY()

public:
	ATourelleChelles();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---------------- Modele 3D (regle dans BP_Turet2026)

	/** Partie fixe (pieds). Peut rester vide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tourelle|Modele")
	UStaticMesh* ModelePied = nullptr;

	/** Partie qui tourne vers la cible (canon). Si ModelePied est vide, c'est toute la tourelle qui tourne. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tourelle|Modele")
	UStaticMesh* ModeleCanon = nullptr;

	/** Modele avec squelette (a la place des deux StaticMesh). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tourelle|Modele")
	USkeletalMesh* ModeleSquelette = nullptr;

	/** Os du squelette qui tourne vers la cible (vide = tout le modele tourne). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tourelle|Modele")
	FName OsQuiTourne;

	/** Hauteur de l'axe de rotation du canon (cm, au-dessus du sol). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tourelle|Modele")
	FVector PivotCanon = FVector(0.f, 0.f, 120.f);

	/** Pour remettre le modele dans le bon sens si le canon ne regarde pas devant (90, -90, 180). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tourelle|Modele")
	float RotationModele = 0.f;

	/** Bout du canon, par rapport au pivot, canon tourne vers l'avant (cm). Les balles partent d'ici. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tourelle|Modele")
	FVector BoutDuCanonLocal = FVector(140.f, 0.f, 0.f);

	/** Le canon leve / baisse aussi vers la cible (seulement pour un canon StaticMesh separe). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tourelle|Modele")
	bool bViserEnHauteur = true;

	/** Taille du modele (1 = taille d'origine). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tourelle|Modele")
	float Echelle = 1.f;

	// ---------------- Combat (0 = valeur de Tourelle.txt)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tourelle|Combat")
	float PorteeMetres = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tourelle|Combat")
	float TirsParSeconde = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tourelle|Combat")
	float Degats = 0.f;

	/** Ce que la tourelle vise en ce moment (tout le monde la voit tourner vers lui). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Tourelle")
	AActor* Cible = nullptr;

	/** Le joueur qui l'a posee (les tirs comptent comme les siens). Vide = tourelle de la carte. */
	TWeakObjectPtr<AController> Tireur;

	/** Personnage ennemi des joueurs ? */
	static bool EstEnnemi(AActor* Acteur);
	/** Ennemi encore debout ? */
	static bool EstVivant(AActor* Acteur);

	FString Resume() const;

protected:
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastTir(FVector_NetQuantize Fin);

	UPROPERTY(VisibleAnywhere, Category = "Tourelle") USceneComponent* Racine = nullptr;
	UPROPERTY(VisibleAnywhere, Category = "Tourelle") UStaticMeshComponent* Pied = nullptr;
	UPROPERTY(VisibleAnywhere, Category = "Tourelle") USceneComponent* Pivot = nullptr;
	UPROPERTY(VisibleAnywhere, Category = "Tourelle") UStaticMeshComponent* Canon = nullptr;
	UPROPERTY(VisibleAnywhere, Category = "Tourelle") UPoseableMeshComponent* Squelette = nullptr;
	UPROPERTY(VisibleAnywhere, Category = "Tourelle") UPointLightComponent* Eclair = nullptr;

private:
	void AppliquerModele();
	void ConstruireFormesSimples();
	void TickServeur(float Maintenant);
	void Viser(float DeltaSeconds);
	AActor* ChoisirCible() const;
	bool PointDeVisee(AActor* Ennemi, FVector& Point, FName& Os) const;
	bool EstVisible(AActor* Ennemi, const FVector& Point) const;
	FVector BoutDuCanon() const;
	float Portee() const;
	void JouerSon(int32 Genre, float Volume);

	UPROPERTY(Transient) TArray<UStaticMeshComponent*> FormesSimples;
	UPROPERTY(Transient) UMaterialInstanceDynamic* MatTraceur = nullptr;
	UPROPERTY(Transient) UStaticMesh* FormeCylindre = nullptr;
	UPROPERTY(Transient) USoundAttenuation* Attenuation = nullptr;
	UPROPERTY(Transient) UParticleSystem* Etincelles = nullptr;
	UPROPERTY(Transient) UParticleSystem* FlammeCanon = nullptr;

	struct FTraceur
	{
		TWeakObjectPtr<UStaticMeshComponent> Forme;
		float Age = 0.f;
	};
	TArray<FTraceur> Traceurs;

	/** Angle actuel du canon (par rapport a la tourelle), lisse sur chaque PC. */
	float Cap = 0.f;
	float Hausse = 0.f;
	float Temps = 0.f;
	float AgeEclair = 10.f;
	/** Rotation de depart de l'os qui tourne (espace du modele). */
	FQuat RotationOsDepart = FQuat::Identity;
	bool bOsMesure = false;

	float ProchainChoix = 0.f;
	float ProchainTir = 0.f;
};
