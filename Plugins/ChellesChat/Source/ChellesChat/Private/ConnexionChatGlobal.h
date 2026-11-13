// CHELLES 3D - Chat global : connexion au serveur du VPS (WebSocket), partagee entre le jeu et le site chelles3d.fr
#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

class IWebSocket;

/** D'ou vient une ligne du chat. */
enum class ESourceChatGlobal : uint8
{
	Jeu,      // un joueur du jeu (affiche avec la manette)
	Site,     // un visiteur du site (affiche avec le globe)
	Systeme,  // message du serveur (arrivees, departs, annonces)
	Local     // message de ce PC (connexion, erreurs)
};

struct FLigneChatGlobal
{
	FString Pseudo;
	FString Texte;
	ESourceChatGlobal Source = ESourceChatGlobal::Systeme;
	bool bAdmin = false;
};

enum class EEtatChatGlobal : uint8
{
	Eteint,        // option CHAT GLOBAL sur OFF, ou pas de partie en cours
	SansPseudo,    // il faut choisir un pseudo (premiere fois, ou pseudo refuse)
	Connexion,     // connexion en cours
	Connecte,      // pret : on peut ecrire
	Attente,       // coupure : nouvel essai dans quelques secondes
	Bloque         // expulse ou banni : pas de reconnexion automatique
};

/**
 * Une seule connexion pour tout le jeu (elle survit aux changements de carte et au passage en multi).
 * Elle ne s'ouvre que si l'option CHAT GLOBAL est sur ON (OPTIONS > JEU) ET qu'un pseudo a ete choisi.
 * Protocole : D:\SITE CHELLES3D\VPS\PROTOCOLE_CHAT.txt
 */
class FConnexionChatGlobal
{
public:
	static FConnexionChatGlobal& Get();
	/** nullptr si la connexion n'existe pas (ou plus : jeu en train de se fermer). */
	static FConnexionChatGlobal* Trouver();
	static void Creer();
	static void Detruire();

	~FConnexionChatGlobal();
	FConnexionChatGlobal(const FConnexionChatGlobal&) = delete;
	FConnexionChatGlobal& operator=(const FConnexionChatGlobal&) = delete;

	/** Une partie commence / se termine sur ce PC (appele par l'acteur AChatGlobalChelles). */
	void PartieCommence();
	void PartieTerminee(bool bDefinitif);

	/** Envoie un message a tout le monde (jeu + site). Commandes : /pseudo NouveauNom */
	void Envoyer(const FString& Texte);
	/** Choisit (et memorise) le pseudo, puis se connecte. Renvoie false si le pseudo n'est pas valable. */
	bool ChoisirPseudo(const FString& Pseudo);

	EEtatChatGlobal Etat() const { return EtatActuel; }
	bool OptionActive() const;
	FString PseudoMemorise() const;
	const FString& PseudoServeur() const { return PseudoConnecte; }
	int32 EnLigne() const { return NombreEnLigne; }
	int32 JoueursDuJeu() const { return NombreJeu; }
	const TArray<FLigneChatGlobal>& Lignes() const { return Messages; }
	/** Change a chaque nouveau message : la fenetre se redessine quand il change. */
	int32 Version() const { return NumeroVersion; }
	/** Heure (FPlatformTime::Seconds) du dernier message recu. */
	double DernierMessage() const { return HeureDernierMessage; }
	int32 TexteMax() const { return LimiteTexte; }

	/** Nettoie un pseudo (3 a 20 lettres, chiffres, espace, _ - .). Vide si pas valable. */
	static FString NettoyerPseudo(const FString& Brut);

private:
	FConnexionChatGlobal();
	bool Tick(float Delta);

	void Connecter();
	void Fermer();
	void DireBonjour();
	void PrevoirNouvelEssai();
	void EnvoyerJson(const TSharedRef<class FJsonObject>& Objet);
	void Ajouter(const FLigneChatGlobal& Ligne);
	void AjouterLocal(const FString& Texte);
	void MemoriserPseudo(const FString& Pseudo);
	FString Adresse() const;

	// Evenements de la WebSocket (toujours sur le fil du jeu)
	void SurConnecte();
	void SurErreur(const FString& Erreur);
	void SurFermeture(int32 Code, const FString& Raison, bool bPropre);
	void SurMessage(const FString& Texte);

	TSharedPtr<IWebSocket> Socket;
	TArray<TSharedPtr<IWebSocket>> AJeter;
	FDelegateHandle Ticker;

	EEtatChatGlobal EtatActuel = EEtatChatGlobal::Eteint;
	bool bBienvenue = false;
	int32 Parties = 0;
	double FinDePartie = -1.0;
	double ProchainEssai = 0.0;
	int32 Essais = 0;
	double ProchaineLecture = 0.0;
	bool bOptionLue = true;

	FString PseudoConnecte;
	int32 NombreEnLigne = 0;
	int32 NombreJeu = 0;
	int32 LimiteTexte = 200;

	TArray<FLigneChatGlobal> Messages;
	int32 NumeroVersion = 0;
	double HeureDernierMessage = -100.0;
};
