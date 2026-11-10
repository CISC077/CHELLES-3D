// CHELLES 3D - Les passants parlent (en francais) quand le joueur passe pres d'eux.
// Acteur pose automatiquement au lancement du jeu : rien a faire dans le niveau.
//
// Les phrases sont dans Plugins/ChellesParoles/Repliques.txt (modifiable avec le Bloc-notes).
// Les voix (.wav) sont fabriquees par Generer_les_voix.bat dans Plugins/ChellesParoles/Voix/.
// Sans voix, les phrases s'affichent au-dessus de la tete du passant.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ParolesPassants.generated.h"

class ACharacter;
class UAudioComponent;
class USoundAttenuation;
class SBullesParoles;

/** Une replique qui a une voix enregistree (ligne de Voix/index.txt). */
struct FRepliqueVoix
{
	FString Categorie;
	bool bFeminin = false;
	int32 Voix = 0;        // numero de la voix Windows qui l'a prononcee
	FString Fichier;       // chemin complet du .wav
	FString Texte;
};

/** Un son .wav lu sur le disque, garde en memoire. */
struct FSonCharge
{
	TArray<uint8> PCM;     // echantillons 16 bits
	int32 Frequence = 22050;
	int32 Canaux = 1;
	float Duree = 0.f;
};

/** Ce que l'on retient de chaque passant. */
struct FEtatPassant
{
	bool bInitialise = false;
	bool bFeminin = false;
	int32 Voix = -1;
	float Hauteur = 1.f;           // hauteur de la voix (pitch)
	double DerniereParole = -1000.0;
	double DerniereBousculade = -1000.0;
	double FinParole = 0.0;
	float TempsAFixer = 0.f;
	bool bEtaitProche = false;
	TWeakObjectPtr<UAudioComponent> Audio;
};

UCLASS(Blueprintable, meta = (DisplayName = "Paroles des passants (Chelles)"))
class CHELLESPAROLES_API AParolesPassants : public AActor
{
	GENERATED_BODY()

public:
	AParolesPassants();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;

	/** Seuls les personnages dont la classe contient ce mot parlent (les passants du plugin ChellesPassants). */
	UPROPERTY(EditAnywhere, Category = "Paroles")
	FString MotCleClasse = TEXT("Passant");

	/** Distance a laquelle un passant peut dire bonjour quand le joueur arrive (cm). */
	UPROPERTY(EditAnywhere, Category = "Paroles")
	float DistanceSalut = 350.f;

	/** Chance (0 a 1) qu'un passant parle quand le joueur arrive pres de lui. */
	UPROPERTY(EditAnywhere, Category = "Paroles", meta = (ClampMin = "0", ClampMax = "1"))
	float ChanceSalut = 0.35f;

	/** Vitesse du joueur au-dessus de laquelle un contact compte comme une bousculade (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Paroles")
	float VitesseBousculade = 180.f;

	/** Pousse un peu le passant quand on le bouscule. */
	UPROPERTY(EditAnywhere, Category = "Paroles")
	bool bPousserQuandBouscule = true;

	/** Au-dessus de cette vitesse, le joueur est en vehicule (motoneige, auto...) et les passants crient (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Paroles")
	float VitesseVehicule = 900.f;

	/** Distance a laquelle un vehicule rapide fait reagir les passants (cm). */
	UPROPERTY(EditAnywhere, Category = "Paroles")
	float DistanceVehicule = 700.f;

	/** Temps (s) que le joueur doit rester immobile tout pres d'un passant avant qu'il demande ce qu'on lui veut. */
	UPROPERTY(EditAnywhere, Category = "Paroles")
	float TempsAvantRemarque = 4.f;

	/** Un meme passant ne reparle pas avant ce delai (s). */
	UPROPERTY(EditAnywhere, Category = "Paroles")
	float AttenteParPassant = 15.f;

	/** Deux passants differents ne parlent pas a moins de ce delai (s), sauf bousculade. */
	UPROPERTY(EditAnywhere, Category = "Paroles")
	float AttenteEntrePassants = 2.5f;

	/** Volume des voix. */
	UPROPERTY(EditAnywhere, Category = "Paroles")
	float Volume = 1.f;

	/** Distance a laquelle on n'entend plus du tout la voix (cm). */
	UPROPERTY(EditAnywhere, Category = "Paroles")
	float PorteeDeLaVoix = 2500.f;

	/** Affiche les phrases dans une bulle noire (texte blanc) au-dessus de la tete du passant. */
	UPROPERTY(EditAnywhere, Category = "Paroles|Bulles")
	bool bAfficherLesBulles = true;

	/** Au-dela de cette distance (cm), la bulle n'est plus affichee. */
	UPROPERTY(EditAnywhere, Category = "Paroles|Bulles")
	float PorteeDesBulles = 2500.f;

	/** Taille du texte dans les bulles. */
	UPROPERTY(EditAnywhere, Category = "Paroles|Bulles", meta = (ClampMin = "8", ClampMax = "40"))
	int32 TaillePoliceBulles = 15;

	/** Affiche la phrase au-dessus de la tete quand elle n'a pas de voix (fichier .wav absent). */
	UPROPERTY(EditAnywhere, Category = "Paroles")
	bool bTexteSiPasDeVoix = true;

	/** Affiche la bulle meme quand le passant parle a voix haute. */
	UPROPERTY(EditAnywhere, Category = "Paroles")
	bool bToujoursAfficherLeTexte = true;

private:
	void ChargerRepliques();
	bool EstUnPassant(const ACharacter* Personnage) const;
	void PreparerPassant(ACharacter* Passant, FEtatPassant& Etat);
	void Parler(ACharacter* Passant, FEtatPassant& Etat, const FString& Categorie, bool bPrioritaire);
	const FSonCharge* ChargerSon(const FString& Fichier);

	/** Phrases de Repliques.txt, par categorie (pour l'affichage sans voix). */
	TMap<FString, TArray<FString>> Textes;

	/** Phrases qui ont une voix (Voix/index.txt). */
	TArray<FRepliqueVoix> Voix;

	/** Numeros des voix Windows trouvees, selon qu'elles sont feminines ou masculines. */
	TArray<int32> VoixFeminines;
	TArray<int32> VoixMasculines;

	/** Derniere phrase dite par categorie (pour ne pas repeter deux fois de suite). */
	TMap<FString, FString> DernierePhrase;

	TMap<FString, FSonCharge> Sons;
	TMap<TWeakObjectPtr<ACharacter>, FEtatPassant> Etats;

	UPROPERTY()
	USoundAttenuation* Attenuation = nullptr;

	TSharedPtr<SBullesParoles> Bulles;

	double DerniereParoleGlobale = -1000.0;
	float TempsAvantNettoyage = 10.f;
};
