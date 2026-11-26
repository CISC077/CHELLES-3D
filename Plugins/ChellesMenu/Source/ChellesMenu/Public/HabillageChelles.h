// CHELLES 3D - HABILLAGE : sexe, tenue et look du joueur (packs "Stylized Male and Female" et "Female Mannequin")
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/StreamableManager.h"
#include "HabillageChelles.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UMaterialInterface;
class UDataTable;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UPointLightComponent;
class UStaticMeshComponent;
class APawn;
class APlayerState;

/**
 * Le catalogue des vetements est lu dans les tableaux de donnees des deux packs (rien n'est copie) :
 *  - /Game/Action_Male_and_Female/Data/Heads/DT_Heads : un modele = silhouette + peau, avec ses tableaux
 *    Haut, Veste, Bas, Chaussures, Gants, Cheveux, Chapeau, Lunettes.
 *  - /Game/Female_Mannequin/Blueprints/DT_Presets_* : les mannequins tout faits.
 * Les morceaux choisis sont poses sur le corps du soldat (qui reste anime par le jeu, avec ses armes),
 * le corps d'origine est seulement rendu invisible. Spider-Man = on ne touche a rien (comme avant).
 */
namespace ChellesHabillage
{
	enum ESexe : int32
	{
		SpiderMan = 0,
		Feminin = 1,
		Masculin = 2,
		/** PANTERA (2026-11-11) : comme Spider-Male on ne touche pas au soldat ; au lancement la roue TAB passe en PANTERA. */
		Pantera = 3,
		/** Personnages crees par MERIDJA (2026-11-24) : on ne touche pas au soldat, le plugin ChellesWraith
		 *  pose le personnage sur lui (GameUserSettings [ChellesWraith] Demande=LENA / SOFIA / LUCIA).
		 *  Pour en ajouter un : une valeur ici + une ligne dans PersosWraith (HabillageChelles.cpp). */
		Lena = 4,
		Sofia = 5,
		Lucia = 6,
		Brenda = 7,    // 25/11/2026
		/** Mr.SHEMS (25/11/2026) : personnage VIP, debloque par un code (EstPersoVIP / DebloquerPerso). */
		Shems = 8,
		Rebecca = 9    // 26/11/2026
	};

	/** Nom du personnage pose par ChellesWraith pour ce choix (LENA, SOFIA...), nullptr pour les autres choix. */
	CHELLESMENU_API const TCHAR* NomPersoWraith(int32 Sexe);
	/** Tous les choix "personnage" (dans l'ordre des boutons de HABILLAGE) : LENA, SOFIA, LUCIA, puis (2026-11-24)
	 *  TOUS les heros de la touche K du SOLDAT, lus dans Plugins\ChellesWraith\Wraith.txt (lignes Skin=) :
	 *  Wraith, Dekker, Sci-Fi, Aurora, Boris... Un Skin= ajoute dans Wraith.txt apparait tout seul dans le menu. */
	CHELLESMENU_API TArray<int32> PersosWraith();
	/** Vrai pour les personnages crees par MERIDJA (LENA, SOFIA, LUCIA) : ils dansent (touche T). */
	CHELLESMENU_API bool EstPersoCree(int32 Sexe);
	/** Couleur de la carte du heros (5e valeur de sa ligne Skin=), noir si aucune. */
	CHELLESMENU_API FLinearColor CouleurPersoWraith(int32 Sexe);
	/** Personnage VIP (liste dans Plugins\ChellesMenu\VIP.txt) : il faut taper le code VIP dans le lobby avant de le jouer. */
	CHELLESMENU_API bool EstPersoVIP(int32 Sexe);
	/** Vrai si ce n'est pas un VIP, ou si le code VIP a ete tape depuis le lancement du jeu (jamais enregistre). */
	CHELLESMENU_API bool PersoDebloque(int32 Sexe);
	/** Verifie le code (CODE= de VIP.txt) ; s'il est bon TOUS les VIP sont debloques jusqu'a la fermeture du jeu. */
	CHELLESMENU_API bool DebloquerPerso(int32 Sexe, const FString& Code);

	/** Le choix du joueur, enregistre dans GameUserSettings.ini (section ChellesHabillage, cle Tenue). */
	struct CHELLESMENU_API FTenue
	{
		int32 Sexe = SpiderMan;
		FString Modele;
		TMap<FString, FString> Choix; // cle d'emplacement (Upper, Lower...) -> ligne du tableau ("" = aucun)

		FString VersTexte() const;
		static FTenue DepuisTexte(const FString& Texte);
		bool EstSpiderMan() const { return Sexe != Feminin && Sexe != Masculin; }
		bool EstPantera() const { return Sexe == Pantera; }
		bool EstPersoWraith() const { return NomPersoWraith(Sexe) != nullptr; }
	};

	/** Un emplacement de vetement (HAUT, BAS...) et ses choix possibles. */
	struct CHELLESMENU_API FEmplacement
	{
		FString Cle;
		FString Titre;
		TWeakObjectPtr<UDataTable> Table;
		TArray<FName> Lignes; // NAME_None = aucun
		TArray<FString> Noms;
		int32 IndexDe(const FString& Ligne) const;
	};

	/** Un modele de depart : silhouette + peau (pack Stylized) ou mannequin tout fait (pack Female Mannequin). */
	struct CHELLESMENU_API FModele
	{
		FString Id;
		FString Nom;
		bool bFemme = false;
		bool bMannequin = false;
		TWeakObjectPtr<UDataTable> Table;
		FName Ligne;
		TArray<FEmplacement> Emplacements;
	};

	/** Un morceau a poser sur le corps. */
	struct CHELLESMENU_API FPiece
	{
		USkeletalMesh* Mesh = nullptr;
		TArray<TPair<int32, UMaterialInterface*>> Materiaux;
		/** Vrai : suit le squelette du corps (vetements). Faux : accroche a un os (cheveux, chapeau, lunettes). */
		bool bSuitSquelette = true;
		FName Os;
		FTransform Relatif = FTransform::Identity;
		/** Bas et chaussures : aussi poses sur les jambes vues en vue a la 1re personne. */
		bool bJambes = false;
	};

	CHELLESMENU_API const TArray<FModele>& Modeles();
	CHELLESMENU_API const FModele* TrouverModele(const FString& Id);
	CHELLESMENU_API TArray<const FModele*> ModelesDuSexe(int32 Sexe);
	/** Corrige un choix (modele du bon sexe, vetements qui existent) et met les valeurs par defaut. */
	CHELLESMENU_API void Completer(FTenue& Tenue);
	/** Tire une tenue au hasard pour le sexe de Tenue. */
	CHELLESMENU_API void Hasard(FTenue& Tenue);
	/** Liste les morceaux a poser. Squelette : mesh complet qui sert de corps dans l'apercu du menu. */
	CHELLESMENU_API bool Assembler(const FTenue& Tenue, TArray<FPiece>& Pieces, UClass*& ClasseAnim, USkeletalMesh*& Squelette);
	/** Cree les morceaux sur Proprio, animes par Chef (et Jambes pour la vue a la 1re personne). */
	CHELLESMENU_API void Construire(AActor* Proprio, USkeletalMeshComponent* Chef, USkeletalMeshComponent* Jambes,
		const TArray<FPiece>& Pieces, TArray<USkeletalMeshComponent*>& Crees, bool bPendantPause);

	CHELLESMENU_API FTenue TenueLocale();
	CHELLESMENU_API void EnregistrerTenue(const FTenue& Tenue);
	/** Les packs sont-ils dans le projet ? */
	CHELLESMENU_API bool PacksPresents();
	/** Lance le chargement du catalogue en arriere-plan (appele a l'ouverture du menu). */
	/** 25/11 : a appeler a chaque image jusqu'a ce qu'il rende vrai : charge en arriere-plan les tables ET tout ce
	 *  qu'elles designent (tetes, sous-tables), puis prepare le catalogue. Ouvrir HABILLAGE ne gele plus. */
	CHELLESMENU_API bool PrechargerCatalogue();
}

/**
 * En multijoueur, un relais par joueur (proprietaire = son PlayerController), cree par le serveur.
 * Le joueur envoie sa tenue au serveur, le relais la copie chez tout le monde : chacun voit le look des autres.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESMENU_API AHabillageRelais : public AActor
{
	GENERATED_BODY()

public:
	AHabillageRelais();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable)
	void ServeurTenue(const FString& Texte);

	UPROPERTY(Replicated)
	APlayerState* Joueur = nullptr;

	UPROPERTY(Replicated)
	FString Tenue;
};

/**
 * Pose automatiquement au lancement du jeu (une par machine, non repliquee).
 * Habille le soldat de chaque joueur avec sa tenue, en solo comme en multijoueur.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESMENU_API AHabillageChelles : public AActor
{
	GENERATED_BODY()

public:
	AHabillageChelles();

protected:
	virtual void Tick(float Delta) override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;

private:
	struct FHabille
	{
		FString Texte;
		TArray<TWeakObjectPtr<USkeletalMeshComponent>> Corps;
		TArray<TWeakObjectPtr<USkeletalMeshComponent>> Bas;
		TWeakObjectPtr<USkeletalMeshComponent> Chef;
		TWeakObjectPtr<USkeletalMeshComponent> Jambes;
		bool bChefOmbre = true;
		bool bJambesOmbre = false;
	};

	void GererRelais();
	void EnvoyerMaTenue();
	void Habiller(APawn* Pion, const FString& Texte, USkeletalMeshComponent* Chef);
	/** Cavalier d'une monture (composant Horse_Rider du cheval HorseStarterKit), sinon nullptr. */
	static USkeletalMeshComponent* Cavalier(APawn* Pion);
	void Deshabiller(APawn* Pion);
	void Suivre();
	bool EstSoldat(APawn* Pion) const;
	/** PANTERA choisi dans HABILLAGE : demande a la roue TAB (plugin ChellesRoue, par reflexion) de passer en PANTERA. */
	void DemanderPantera(float Delta);
	bool bPanteraFait = false;
	int32 EssaisPantera = 0;
	float ProchainPantera = 0.f;

	TMap<TWeakObjectPtr<APawn>, FHabille> Habilles;
	float ProchainControle = 0.f;
	bool bCatalogueLance = false;   // catalogue des tenues charge en arriere-plan (25/11)
	float ProchainEnvoi = 0.f;
	FString DernierEnvoi;
};

/** Apercu 3D du menu HABILLAGE : un personnage loin au-dessus de la carte, filme dans une texture. */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESMENU_API AApercuHabillage : public AActor
{
	GENERATED_BODY()

public:
	AApercuHabillage();

	void Montrer(const ChellesHabillage::FTenue& Tenue, USkeletalMesh* MeshSpiderMan, UClass* AnimSpiderMan);
	void Tourner(float Degres) { Angle += Degres; Pause = 3.f; }
	UTextureRenderTarget2D* Image() const { return Cible; }

	virtual void Tick(float Delta) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	USceneComponent* Pivot = nullptr;

	UPROPERTY(Transient)
	USkeletalMeshComponent* Corps = nullptr;

	UPROPERTY(Transient)
	USceneCaptureComponent2D* Camera = nullptr;

	UPROPERTY(Transient)
	UPointLightComponent* Lampe1 = nullptr;

	UPROPERTY(Transient)
	UPointLightComponent* Lampe2 = nullptr;

	UPROPERTY(Transient)
	UTextureRenderTarget2D* Cible = nullptr;

	/** Decor du lobby (2026-11-24) : mur et sol bleus derriere le personnage, au lieu d'un fond noir. */
	UPROPERTY(Transient)
	UStaticMeshComponent* Fond = nullptr;

	UPROPERTY(Transient)
	UStaticMeshComponent* Sol = nullptr;

	UPROPERTY(Transient)
	TArray<USkeletalMeshComponent*> Morceaux;

	/** 25/11/2026 : les derniers corps / animations montres restent en memoire pendant que le menu est ouvert :
	 *  revenir sur un perso deja vu ne recharge plus son corps (LUCIA, SOFIA = plus de 120 Mo) -> plus d'a-coup. */
	UPROPERTY(Transient)
	TArray<UObject*> DejaVus;
	void GarderDejaVu();

	/** 25/11/2026 : corps lourds (heros Paragon, LENA...) charges en arriere-plan ; l'apercu d'avant reste affiche
	 *  pendant le chargement, puis Tick montre le perso demande (plus de gel du menu HABILLAGE). */
	FStreamableManager Chargeur;
	TSharedPtr<FStreamableHandle> Attente;
	ChellesHabillage::FTenue TenueAttendue;
	TWeakObjectPtr<USkeletalMesh> SpiderAttendu;
	TWeakObjectPtr<UClass> AnimAttendue;
	bool bApresAttente = false;

	float Angle = 0.f;
	float Pause = 0.f;
};
