// CHELLES 3D - Voitures du pack DV_Car_Horo garees au bord des rues (demande Rachid 25/11/2026).
//
// Outils sans etat utilises par AVolVoitureChelles :
//  - reconnaitre une voiture Horo (BP_Car_Hatchback_Horo ou un Blueprint enfant, ex. BP_Bugatti_Chelles) ;
//  - regler ses options du pack (saut americain, reparation, carburant, plaque au hasard...) par leur nom ;
//  - ecrire "CHELLES-3D" sur toutes ses plaques d'immatriculation ;
//  - lancer une fonction du pack (phares, saut, clignotants...) comme si sa touche avait ete pressee :
//    c'est ce qui permet de la jouer sur TOUTES les machines en multijoueur ;
//  - habiller une voiture en Bugatti (carrosserie + 4 roues de Rachid accrochees aux os des roues de la Horo).
// Le pack n'est pas modifie : tout passe par les noms de ses variables et de ses actions.
#pragma once

#include "CoreMinimal.h"

class AActor;
class APawn;
class UWorld;
class UStaticMesh;
class UMaterialInterface;

/** Actions du pack DV_Car_Horo rejouees chez tout le monde (numero envoye par le reseau). */
namespace EActionHoro
{
	enum Type : uint8
	{
		Phares = 0,
		Saut = 1,
		Reinitialiser = 2,
		ClignotantGauche = 3,
		ClignotantDroit = 4,
		Warnings = 5,
		Nombre = 6
	};
}

/** Apparence Bugatti (meshes importes par Tools\VoituresHoro\INSTALLER_VOITURES_HORO.bat). */
struct FHabillageBugatti
{
	UStaticMesh* Carrosserie = nullptr;
	/** 0 = avant gauche, 1 = avant droite, 2 = arriere gauche, 3 = arriere droite */
	UStaticMesh* Roues[4] = { nullptr, nullptr, nullptr, nullptr };
	/** Centre de chaque roue dans la Bugatti (cm, X vers l'avant, Y vers la droite, Z vers le haut, sol = 0) */
	FVector Centres[4] = {
		FVector(126.25f, -82.2f, 38.75f), FVector(126.25f, 82.1f, 38.75f),
		FVector(-141.25f, -80.9f, 34.25f), FVector(-141.25f, 81.25f, 34.25f) };
	/** Rayon des roues de la Bugatti (cm) : avant, arriere */
	float RayonAvant = 39.f;
	float RayonArriere = 34.5f;
	/** Correction en plus du calage automatique (1 = aucune) */
	float EchelleLargeur = 1.f;
	float EchelleHauteur = 1.f;
	FVector Decalage = FVector::ZeroVector;
	float Lacet = 0.f;
	FString TextePlaque = TEXT("CHELLES-3D");

	bool EstPret() const { return Carrosserie != nullptr; }
};

namespace HoroChelles
{
	/** La classe est-elle BP_Car_Hatchback_Horo (ou un Blueprint enfant) ? */
	bool EstClasseHoro(const UClass* Classe);
	bool EstHoro(const AActor* Acteur);

	/** Regle des variables booleennes du Blueprint par leur nom exact (ex. "bUse_Low_Rider_?"). Retourne le nombre reglees. */
	int32 ReglerOptions(AActor* Voiture, const TMap<FString, bool>& Options);

	/** Coupe les zones "monter avec E" du pack (elles cherchent le personnage du pack et ecrivent des Accessed None). */
	void CouperDeclencheursDuPack(AActor* Voiture);

	/** Ecrit le texte sur toutes les plaques (widgets UMG_Licence_Plate_* de la voiture et des pieces detachees). */
	int32 EcrirePlaques(AActor* Voiture, const FString& Texte);

	/** Lance la fonction du Blueprint liee a l'action d'entree (ex. "toggle_light") comme si la touche etait pressee. */
	bool LancerActionPack(AActor* Voiture, FName Action);

	/** Nom de l'action du pack pour une action Chelles. */
	FName NomAction(EActionHoro::Type Action);

	/** Retire des commandes de la voiture conduite les actions du pack qu'on ne veut pas (Use = sortie du pack, Horn = klaxon double). */
	int32 RetirerLiaisons(APawn* Voiture, const TArray<FName>& Actions);

	/** Retire de l'ecran les compteurs du pack (UMG du dossier DV_Car_Horo) quand on descend. */
	void RetirerAffichagesDuPack(UWorld* Monde);

	/** Remet la voiture sur ses roues, un peu plus haut, sans vitesse (serveur). */
	void RemettreSurRoues(APawn* Voiture);

	/** Habille la voiture en Bugatti (une seule fois par voiture, local a chaque machine). */
	bool HabillerBugatti(APawn* Voiture, const FHabillageBugatti& Habillage);
	bool EstHabilleeBugatti(const AActor* Voiture);
}
