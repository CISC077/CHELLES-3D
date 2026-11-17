// CHELLES 3D - Ennemis des dossiers FPS_Game/Blueprints/Bonus/AI (ajoute tout seul au lancement du jeu)
//
//  1. A chaque lancement, le serveur (ou le jeu solo) pose 10 ennemis de chaque dossier
//     (Charger, Other, Patrol, Soldier, Zombie) au hasard dans la carte, et remplace ceux qui meurent.
//  2. Tout personnage (Blueprint) depose dans le dossier Other devient un ennemi des joueurs :
//     le plugin le dirige (repere, poursuit et frappe le joueur le plus proche), lui donne de la vie,
//     et le fait tomber quand on le tue. Solo et multijoueur.
//  3. Les ennemis qui ont encore le mannequin gris (SK_Mannequin) sont habilles : bandits de couleur
//     (Soldier, Charger, Patrol) ou vrai zombie (Zombie). Rien n'est ecrit dans les fichiers du projet.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnnemisChelles.generated.h"

class ACharacter;
class AController;
class APawn;
class UAnimMontage;
class UDamageType;
class UMaterialInterface;
class UPrimitiveComponent;
class USkeletalMesh;

/** Reglages lus dans Plugins\ChellesEnnemis\Ennemis.txt (modifiable avec le Bloc-notes). */
struct FReglagesEnnemis
{
	bool bActif = true;

	// Ennemis eparpilles
	int32 NombreParDossier = 10;
	TMap<FName, int32> NombreDuDossier;
	float ReapparitionSecondes = 90.f;
	float EspacementMetres = 25.f;
	float LoinDesJoueursMetres = 40.f;
	float EcartNiveauRueMetres = 12.f;
	FString DossierIA = TEXT("/Game/FPS_Game/Blueprints/Bonus/AI");
	FString DossierAuto = TEXT("/Game/FPS_Game/Blueprints/Bonus/AI/Other");
	TArray<FString> Exclus = { TEXT("BP_SimplePrototypingZombie"), TEXT("BP_EnemySpawner") };

	// Ennemis du dossier Other (diriges par le plugin)
	float DistanceDetectionMetres = 40.f;
	float DistanceAbandonMetres = 80.f;
	float DegatsPourcent = 15.f;
	float TempsEntreCoups = 1.5f;
	float DegatsCorpsPourcent = 25.f;
	bool bTeteTue = true;
	float VitesseCourse = 450.f;
	bool bPromenade = true;

	// Zombies du dossier Zombie (poses par ce plugin) : le plugin decide aussi de leur mort
	// (tete = mort, corps = DegatsCorpsPourcent), meme si leur Blueprint ne les fait pas mourir.
	bool bZombiesTuables = true;

	// Tenues
	bool bHabiller = true;
	FString MeshBandit = TEXT("/Game/Bandit_Sport/Mesh/SK_Bandit_FPS");
	FString PeauZombie = TEXT("/Game/DecayedZombieB/Mesh/SK_DecayedZombie_B_wPants");
	TMap<FString, FString> Tenues;
	FString TenueParDefaut = TEXT("/Game/Bandit_Sport/Material/M_Bandit_clothes_Blue");

	int32 NombrePour(FName Dossier) const
	{
		const int32* N = NombreDuDossier.Find(Dossier);
		return N ? *N : NombreParDossier;
	}
};

/** Ce que le serveur retient pour un ennemi du dossier Other (dirige par le plugin). */
struct FEnnemiAuto
{
	float Vie = 100.f;
	bool bMort = false;
	float VitesseMarche = 300.f;

	TWeakObjectPtr<APawn> Cible;
	float ProchainChoixCible = 0.f;
	float ProchaineAttaque = 0.f;
	float CoupPrevuA = -1.f;
	TWeakObjectPtr<APawn> CibleDuCoup;

	FVector DernierePosition = FVector::ZeroVector;
	float ProchainControle = 0.f;
	int32 Bloque = 0;
	float ContournerJusqua = 0.f;
	float CoteContournement = 1.f;

	FVector PointPromenade = FVector::ZeroVector;
	float ProchainePromenade = 0.f;

	// Dernier coup "precis" (balle) : sert a savoir si c'etait la tete
	uint64 ImageCoupPrecis = 0;
	bool bCoupTete = false;
	FVector DirectionCoup = FVector::ZeroVector;
};

/** Un ennemi pose par le plugin (pour le remplacer quand il meurt). */
struct FEnnemiPose
{
	TWeakObjectPtr<ACharacter> Pion;
	FName Dossier;
	float MortDepuis = -1.f;
};

UCLASS(NotPlaceable, Transient)
class CHELLESENNEMIS_API AEnnemisChelles : public AActor
{
	GENERATED_BODY()

public:
	AEnnemisChelles();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Le serveur (ou le jeu solo) decide de tout. */
	bool EstServeur() const;

	/** Console : Chelles.EnnemisPlus = encore NombreParDossier ennemis de chaque dossier. */
	void AjouterUneVague();
	/** Console : Chelles.Ennemis = ce que le plugin a trouve et pose. */
	void Resume() const;

	/** Tag pose sur chaque ennemi du dossier Other (dirige par ce plugin). */
	static const FName MarqueEnnemiAuto;

	/**
	 * Patrouilleurs (BP_AI_CharPatrol et ses enfants) : si la variable TargetPointArray de l'acteur
	 * est vide, pose 3 TargetPoint autour de lui et les met dans le tableau (avant son BeginPlay).
	 * Rend true si des points ont ete ajoutes. Rien n'est ecrit dans les fichiers du projet.
	 */
	static bool RemplirPatrouille(AActor* Acteur);

protected:
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastAttaque(ACharacter* Ennemi);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastMort(ACharacter* Ennemi, FVector Direction);

	UFUNCTION()
	void ToucheQuelconque(AActor* Touche, float Degats, const UDamageType* TypeDegats, AController* Auteur, AActor* Causeur);

	UFUNCTION()
	void TouchePrecis(AActor* Touche, float Degats, AController* Auteur, FVector PointImpact, UPrimitiveComponent* Composant,
		FName Os, FVector Direction, const UDamageType* TypeDegats, AActor* Causeur);

private:
	void ChargerReglages();
	void TrouverLesClasses();
	FName DossierDe(const UClass* Classe) const;

	// Eparpiller dans la carte (serveur)
	void PreparerPlaces();
	bool ChoisirPlace(const ACharacter* Modele, bool bLoinDesJoueurs, FVector& Resultat);
	ACharacter* PoserEnnemi(FName Dossier, bool bLoinDesJoueurs);
	void PoserTout();
	void SurveillerLesPoses(float Maintenant);

	// Ennemis du dossier Other (serveur)
	void PrendreEnMain(ACharacter* Ennemi);
	void Piloter(ACharacter* Ennemi, FEnnemiAuto& Etat, float Maintenant, float DeltaSeconds);
	void Tuer(ACharacter* Ennemi, const FVector& Direction);

	// Zombies du dossier Zombie : leur Blueprint garde son cerveau, le plugin compte seulement les balles (serveur)
	void SuivreZombie(ACharacter* Ennemi);
	FEnnemiAuto* EtatDe(ACharacter* Ennemi);
	UAnimMontage* MontageAttaquePour(ACharacter* Ennemi);

	// Tenues (sur chaque PC qui affiche le jeu)
	void ScannerLesPersonnages(float Maintenant);
	void Habiller(ACharacter* Ennemi, FName Dossier);

	void JoueursVivants(TArray<APawn*>& Resultat) const;
	static bool EstVivant(APawn* Joueur);
	static float VieMax(APawn* Joueur);
	bool EstMort(ACharacter* Ennemi) const;

	FReglagesEnnemis Reglages;

	/** Classes d'ennemis trouvees, par dossier (Charger, Other, Patrol, Soldier, Zombie). */
	TMap<FName, TArray<UClass*>> ClassesParDossier;

	/** Garde les classes chargees en memoire. */
	UPROPERTY(Transient)
	TArray<UClass*> ClassesGardees;

	/** Places possibles au niveau de la rue, calculees une fois (serveur). */
	TArray<FVector> Places;
	bool bPlacesPretes = false;

	TArray<FEnnemiPose> Poses;
	TMap<TWeakObjectPtr<ACharacter>, FEnnemiAuto> Autos;
	/** Zombies du dossier Zombie dont le plugin compte les balles (ils ne sont pas diriges par lui). */
	TMap<TWeakObjectPtr<ACharacter>, FEnnemiAuto> ZombiesSuivis;

	/** Patrouilleurs poses par le plugin qui ont recu des points de patrouille (pour le bilan). */
	int32 PatrouillesRemplies = 0;

	bool bPoseFaite = false;
	float PoseA = 3.f;
	float ProchaineSurveillance = 0.f;
	float ProchainScan = 0.f;

	UPROPERTY(Transient)
	USkeletalMesh* MeshBandit = nullptr;

	UPROPERTY(Transient)
	USkeletalMesh* PeauZombie = nullptr;

	UPROPERTY(Transient)
	UAnimMontage* MontageZombie = nullptr;

	UPROPERTY(Transient)
	TMap<FString, UMaterialInterface*> MateriauxTenues;

	TMap<TWeakObjectPtr<UClass>, TWeakObjectPtr<UAnimMontage>> MontagesParClasse;

	/** Reglages de collision d'un ennemi FPS_Game (pour que les balles touchent aussi les ennemis Other). */
	bool bCollisionsModeleLues = false;
	FCollisionResponseContainer ReponsesCorpsModele;
	FCollisionResponseContainer ReponsesCapsuleModele;
};
