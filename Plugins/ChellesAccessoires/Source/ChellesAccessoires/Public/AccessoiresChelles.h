// CHELLES 3D - ACCESSOIRES du joueur (25/11/2026, demande Rachid)
//
//  4 accessoires, pour TOUS les personnages, en solo comme en multijoueur :
//   - LAMPE TORCHE (touche 7) : allume / eteint une lampe devant le joueur. Les autres joueurs la voient.
//   - JUMELLES     (touche 8) : zoom x4, puis x8, puis on range (molette = zoom aussi).
//   - GRAPPIN      (touche 9) : vise un mur / un toit et le joueur est tire jusque-la (cable visible par tous).
//   - KIT DE SOIN  (touche 0) : rend de la vie (le serveur soigne). On en porte 5 au maximum.
//  Sur un clavier AZERTY les touches 7 8 9 0 marchent sans Maj (e accent grave, _, c cedille, a accent grave).
//
//  Inventaire : quand le menu ARMEMENTS (touche I) est ouvert, une colonne ACCESSOIRES s'affiche a cote
//  (cartes avec les icones du pack Craft Resources Icons). Clic sur une carte = l'utiliser.
//  A GAGNER : les accessoires marques "A gagner" dans Accessoires.txt et les kits de soin se trouvent
//  dans les coffres de butin (plugin ChellesButin, lu sans en dependre).
//  Ce que le joueur possede est garde sur son PC (Saved\SaveGames\ChellesAccessoires.sav).
//
//  Reglages : Plugins\ChellesAccessoires\Accessoires.txt (Bloc-notes, sans recompiler).
//  Console (touche ²) : Chelles.Accessoires Info | Lampe | Jumelles | Grappin | Soin | Donner <Lampe|Jumelles|Grappin|Kit> [n] | Tout | Relire
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "InputCoreTypes.h"
#include "Styling/SlateBrush.h"
#include "AccessoiresChelles.generated.h"

class APawn;
class APlayerController;
class FFiltreAccessoires;
class SWidget;
class UCameraComponent;
class UGameViewportClient;
class USpotLightComponent;
class UStaticMeshComponent;
class UTexture2D;

/** Les 4 accessoires (l'ordre = l'ordre des cartes). */
namespace ChellesAccessoires
{
	enum EType : uint8
	{
		Lampe = 0,
		Jumelles = 1,
		Grappin = 2,
		Kit = 3,
		Nombre = 4
	};
}

/** Accessoires.txt */
struct CHELLESACCESSOIRES_API FReglagesAccessoires
{
	bool bActif = true;

	/** Touches de chaque accessoire (plusieurs touches possibles : QWERTY + AZERTY). */
	TArray<FKey> Touches[ChellesAccessoires::Nombre];
	/** Vrai = possede des le depart, faux = a gagner dans les coffres. */
	bool bDepart[ChellesAccessoires::Nombre] = { true, true, false, true };
	/** Icone de chaque carte (texture du pack Craft Resources Icons ou autre). */
	FString Icones[ChellesAccessoires::Nombre];

	// Lampe
	float LampeIntensite = 15000.f;
	float LampeAngle = 24.f;
	float LampePortee = 4000.f;
	FLinearColor LampeCouleur = FLinearColor(1.f, 0.95f, 0.82f);
	bool bLampeOmbres = false;

	// Jumelles
	float ZoomJumelles1 = 4.f;
	float ZoomJumelles2 = 8.f;
	bool bCacheJumelles = true;

	// Grappin
	float GrappinPortee = 60.f;   // metres
	float GrappinVitesse = 2600.f; // cm/s
	float GrappinAttente = 1.5f;   // secondes entre deux tirs

	// Kit de soin
	float SoinVie = 50.f;
	float SoinAttente = 3.f;
	int32 KitsDepart = 2;
	int32 KitsMax = 5;

	// Butin (coffres du plugin ChellesButin) : chances en %
	bool bButin = true;
	int32 ChanceKit[3] = { 60, 100, 100 };
	int32 KitsParCoffre[3] = { 1, 1, 2 };
	int32 ChanceAccessoire[3] = { 10, 35, 100 };
	float DistanceCoffre = 6.f; // metres

	// Ecran
	FString PlaceCase = TEXT("BasGauche");
	TArray<FString> MenusInventaire;
	FString SonGain;
	FString SonSoin;
};

CHELLESACCESSOIRES_API const FReglagesAccessoires& ReglagesAccessoires();
CHELLESACCESSOIRES_API void RechargerReglagesAccessoires();

/** Ce que le joueur possede (sur son PC). */
UCLASS()
class CHELLESACCESSOIRES_API UAccessoiresSauvegarde : public USaveGame
{
	GENERATED_BODY()

public:
	/** Bit 0 = lampe, 1 = jumelles, 2 = grappin. */
	UPROPERTY()
	int32 Possedes = 0;

	UPROPERTY()
	int32 Kits = 0;

	/** Vrai = les cadeaux de depart ont deja ete donnes. */
	UPROPERTY()
	bool bDepartDonne = false;
};

/** Un par joueur : ses accessoires, sa lampe, son grappin, sa colonne dans l'inventaire. Visible par tous. */
UCLASS(NotPlaceable, Transient)
class CHELLESACCESSOIRES_API AAccessoiresJoueur : public AActor
{
	GENERATED_BODY()

public:
	AAccessoiresJoueur();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- Repliques a tout le monde (lampe et cable visibles par les autres joueurs)
	UPROPERTY(Replicated)
	APawn* Pion = nullptr;

	UPROPERTY(Replicated)
	bool bLampeAllumee = false;

	UPROPERTY(Replicated)
	bool bGrappinActif = false;

	UPROPERTY(Replicated)
	FVector_NetQuantize PointGrappin;

	// ---- Demandes du joueur au serveur
	UFUNCTION(Server, Reliable)
	void ServeurLampe(bool bAllumer);

	UFUNCTION(Server, Reliable)
	void ServeurGrappin(FVector Point);

	UFUNCTION(Server, Reliable)
	void ServeurFinGrappin();

	UFUNCTION(Server, Reliable)
	void ServeurSoigner();

	/** Le joueur dit au serveur ce qu'il possede (pour le butin). */
	UFUNCTION(Server, Reliable)
	void ServeurMesAccessoires(int32 Masque);

	/** Le serveur donne un objet gagne (coffre) au joueur. */
	UFUNCTION(Client, Reliable)
	void ClientGagner(uint8 Type, int32 Quantite);

	/** Le serveur confirme le soin (le joueur retire un kit). */
	UFUNCTION(Client, Reliable)
	void ClientSoigne(float VieRendue);

	// ---- Machine du joueur
	bool EstLocal() const;
	bool Possede(uint8 Type) const;
	int32 NombreKits() const { return Kits; }
	void Utiliser(uint8 Type);
	void Donner(uint8 Type, int32 Quantite, bool bMessage);
	void ToutDonner();
	FString Resume() const;

	/** Serveur : ce joueur possede-t-il cet accessoire ? (dit par sa machine) */
	bool ServeurPossede(uint8 Type) const { return (MasqueServeur & (1 << Type)) != 0; }

	// Lus par l'ecran
	bool bColonneVisible = false;
	bool bMessageVisible = false;
	FText TexteMessage;
	FLinearColor CouleurMessage = FLinearColor::White;
	float ZoomActuel() const { return Zoom; }
	bool JumellesActives() const { return Zoom > 1.f; }
	bool LampeAllumee() const { return bLampeAllumee; }
	FText TexteCarte(uint8 Type) const;
	UTexture2D* Icone(uint8 Type) const;
	bool Survol(uint8 Type) const;
	/** Appele par le filtre : clic sur une carte ? (vrai = clic garde) */
	bool Clic(const FVector2D& PositionEcran);
	/** Appele par le filtre : molette pendant les jumelles (vrai = gardee). */
	bool Molette(float Sens);

	TSharedPtr<SWidget> Cartes[ChellesAccessoires::Nombre];

	/** Serveur : soigne le pion de ce joueur. Rend la vie rendue (0 = rien). */
	float SoignerPion(float Quantite);

private:
	void InitialiserLocal(APlayerController* PC);
	void TickLocal(float DeltaSeconds, APlayerController* PC);
	void TickVisuel(float DeltaSeconds);
	void TickGrappinMouvement(float DeltaSeconds);
	void Sauvegarder();
	void EnvoyerMasque();
	bool MenuInventaireOuvert() const;
	void FermerMenus(APlayerController* PC);
	void AfficherMessage(const FText& Texte, float Duree, const FLinearColor& Couleur = FLinearColor::White);
	void JouerSon(const FString& Chemin);

	void BasculerLampe();
	void BasculerJumelles();
	void ReglerZoom(float NouveauZoom);
	void TirerGrappin();
	void Soigner();

	// Lampe et cable (sur chaque machine)
	void MettreAJourLampe();
	void MettreAJourCable();
	UCameraComponent* CameraDuPion() const;

	// ---- Etat local
	bool bLocalPret = false;
	int32 Possedes = 0;
	int32 Kits = 0;
	FString NomSauvegarde;
	float ProchainControleMenu = 0.f;
	float FinMessage = 0.f;
	float ProchainGrappin = 0.f;
	float ProchainSoin = 0.f;
	int32 ActionDemandee = -1;
	float Zoom = 1.f;
	float FOVAvant = 0.f;
	TWeakObjectPtr<UCameraComponent> CameraZoom;
	float FOVCameraAvant = 0.f;

	// Grappin (machine du joueur ET serveur : le mouvement est fait des deux cotes)
	bool bTireLocal = false;
	float TempsGrappin = 0.f;
	FVector PointLocal = FVector::ZeroVector;
	float DistancePrecedente = 0.f;
	float TempsBloque = 0.f;

	// Serveur
	int32 MasqueServeur = 0;
	float ProchainSoinServeur = 0.f;

	// Visuels
	UPROPERTY(Transient)
	USpotLightComponent* Lumiere = nullptr;

	UPROPERTY(Transient)
	UStaticMeshComponent* Cable = nullptr;

	UPROPERTY(Transient)
	TArray<UTexture2D*> Images;

	void CreerEcran();
	void CreerMasqueJumelles();
	const FSlateBrush* BrosseIcone(uint8 Type) const;
	const FSlateBrush* BrosseCache() const { return MasqueJumelles ? &BrosseMasque : nullptr; }

	UPROPERTY(Transient)
	UTexture2D* MasqueJumelles = nullptr;

	/** Images Slate des cartes et du cache des jumelles (les textures sont gardees par Images / MasqueJumelles). */
	FSlateBrush Brosses[ChellesAccessoires::Nombre];
	FSlateBrush BrosseMasque;

	TSharedPtr<SWidget> Ecran;
	TWeakObjectPtr<UGameViewportClient> Vue;
	TSharedPtr<FFiltreAccessoires> Filtre;

};

/** Pose un AAccessoiresJoueur par joueur et distribue le butin des coffres (serveur ou solo). */
UCLASS(NotPlaceable, Transient)
class CHELLESACCESSOIRES_API AAccessoiresChelles : public AActor
{
	GENERATED_BODY()

public:
	AAccessoiresChelles();
	virtual void Tick(float DeltaSeconds) override;

private:
	void SurveillerCoffres();

	TMap<TWeakObjectPtr<APlayerController>, TWeakObjectPtr<AAccessoiresJoueur>> Joueurs;
	TSet<TWeakObjectPtr<AActor>> CoffresVus;
	TWeakObjectPtr<UClass> ClasseCoffre;
	float ProchaineRechercheClasse = 0.f;
};
