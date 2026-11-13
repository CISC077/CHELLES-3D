// CHELLES 3D - Chat global : connexion au serveur du VPS (WebSocket)
#include "ConnexionChatGlobal.h"

#include "CoreGlobals.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "IWebSocket.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreMisc.h"
#include "Modules/ModuleManager.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "WebSocketsModule.h"

namespace
{
	/** Adresse par defaut ; peut etre changee dans Config\DefaultGame.ini, section [ChellesChat], cle Adresse. */
	const TCHAR* AdresseParDefaut = TEXT("ws://51.178.54.208:8080/chat");

	/** L'option est ecrite par le menu (plugin ChellesMenu) dans GameUserSettings.ini. */
	const TCHAR* SectionMenu = TEXT("ChellesMenu");
	const TCHAR* CleOption = TEXT("ChatGlobal");
	const TCHAR* SectionChat = TEXT("ChellesChat");
	const TCHAR* ClePseudo = TEXT("Pseudo");

	const int32 LignesMax = 120;
	/** Attente avant un nouvel essai apres une coupure : 2, 4, 8, 16 puis 30 s (protocole). */
	const float Delais[] = { 2.f, 4.f, 8.f, 16.f, 30.f };

	FConnexionChatGlobal* GConnexion = nullptr;

	double Maintenant() { return FPlatformTime::Seconds(); }

	FString Champ(const TSharedPtr<FJsonObject>& Objet, const TCHAR* Nom)
	{
		FString V;
		if (Objet.IsValid())
		{
			Objet->TryGetStringField(Nom, V);
		}
		return V;
	}

	bool Booleen(const TSharedPtr<FJsonObject>& Objet, const TCHAR* Nom)
	{
		bool V = false;
		if (Objet.IsValid())
		{
			Objet->TryGetBoolField(Nom, V);
		}
		return V;
	}

	FLigneChatGlobal LigneDepuisJson(const TSharedPtr<FJsonObject>& Objet)
	{
		FLigneChatGlobal L;
		L.Pseudo = Champ(Objet, TEXT("pseudo"));
		L.Texte = Champ(Objet, TEXT("texte"));
		L.Source = Champ(Objet, TEXT("source")) == TEXT("jeu") ? ESourceChatGlobal::Jeu : ESourceChatGlobal::Site;
		L.bAdmin = Booleen(Objet, TEXT("admin"));
		return L;
	}
}

// ---------------------------------------------------------------- Creation

FConnexionChatGlobal& FConnexionChatGlobal::Get()
{
	if (!GConnexion)
	{
		Creer();
	}
	return *GConnexion;
}

FConnexionChatGlobal* FConnexionChatGlobal::Trouver()
{
	return GConnexion;
}

void FConnexionChatGlobal::Creer()
{
	if (!GConnexion)
	{
		GConnexion = new FConnexionChatGlobal();
	}
}

void FConnexionChatGlobal::Detruire()
{
	delete GConnexion;
	GConnexion = nullptr;
}

FConnexionChatGlobal::FConnexionChatGlobal()
{
	Ticker = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FConnexionChatGlobal::Tick), 0.25f);
}

FConnexionChatGlobal::~FConnexionChatGlobal()
{
	FTicker::GetCoreTicker().RemoveTicker(Ticker);
	Fermer();
	AJeter.Reset();
}

// ---------------------------------------------------------------- Reglages

bool FConnexionChatGlobal::OptionActive() const
{
	bool V = true; // ON par defaut : rien ne part tant qu'aucun pseudo n'est choisi
	if (GConfig)
	{
		GConfig->GetBool(SectionMenu, CleOption, V, GGameUserSettingsIni);
	}
	return V;
}

FString FConnexionChatGlobal::PseudoMemorise() const
{
	FString V;
	if (GConfig)
	{
		GConfig->GetString(SectionChat, ClePseudo, V, GGameUserSettingsIni);
	}
	return NettoyerPseudo(V);
}

void FConnexionChatGlobal::MemoriserPseudo(const FString& Pseudo)
{
	if (GConfig)
	{
		GConfig->SetString(SectionChat, ClePseudo, *Pseudo, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

FString FConnexionChatGlobal::Adresse() const
{
	FString V;
	if (GConfig && GConfig->GetString(SectionChat, TEXT("Adresse"), V, GGameIni) && !V.TrimStartAndEnd().IsEmpty())
	{
		return V.TrimStartAndEnd();
	}
	return AdresseParDefaut;
}

FString FConnexionChatGlobal::NettoyerPseudo(const FString& Brut)
{
	// Memes regles que le serveur : 3 a 20 lettres, chiffres, espace, _ - .
	FString R;
	bool bEspace = false;
	for (TCHAR C : Brut)
	{
		if (FChar::IsWhitespace(C) || C < 32)
		{
			bEspace = !R.IsEmpty();
			continue;
		}
		if (!(FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('-') || C == TEXT('.')))
		{
			continue;
		}
		if (bEspace)
		{
			R.AppendChar(TEXT(' '));
			bEspace = false;
		}
		R.AppendChar(C);
	}
	R = R.Left(20).TrimStartAndEnd();
	return R.Len() >= 3 ? R : FString();
}

// ---------------------------------------------------------------- Parties

void FConnexionChatGlobal::PartieCommence()
{
	++Parties;
	FinDePartie = -1.0;
}

void FConnexionChatGlobal::PartieTerminee(bool bDefinitif)
{
	Parties = FMath::Max(0, Parties - 1);
	if (Parties == 0)
	{
		// Changement de carte (passage en multi...) : on attend un peu avant de couper.
		FinDePartie = bDefinitif ? 0.0 : Maintenant() + 10.0;
	}
}

// ---------------------------------------------------------------- Boucle

bool FConnexionChatGlobal::Tick(float Delta)
{
	AJeter.Reset();
	const double T = Maintenant();
	if (T >= ProchaineLecture)
	{
		ProchaineLecture = T + 0.5;
		bOptionLue = OptionActive();
	}

	const bool bPartie = Parties > 0 || (FinDePartie > 0.0 && T < FinDePartie);
	const bool bVoulu = bPartie && bOptionLue && !IsRunningDedicatedServer() && !IsRunningCommandlet();

	if (!bVoulu)
	{
		if (Socket.IsValid())
		{
			Fermer();
			if (!bOptionLue)
			{
				AjouterLocal(TEXT("Chat global coup\u00E9 (OPTIONS > JEU > CHAT GLOBAL : OFF)."));
			}
		}
		if (EtatActuel != EEtatChatGlobal::Bloque || !bOptionLue)
		{
			// Remettre OFF puis ON debloque aussi apres une expulsion
			EtatActuel = EEtatChatGlobal::Eteint;
		}
		Essais = 0;
		return true;
	}

	if (EtatActuel == EEtatChatGlobal::Bloque)
	{
		return true;
	}

	if (PseudoMemorise().IsEmpty())
	{
		if (!Socket.IsValid())
		{
			EtatActuel = EEtatChatGlobal::SansPseudo;
		}
		return true;
	}

	if (!Socket.IsValid())
	{
		if (EtatActuel == EEtatChatGlobal::Attente && T < ProchainEssai)
		{
			return true;
		}
		Connecter();
	}
	return true;
}

void FConnexionChatGlobal::Connecter()
{
	Fermer();
	FWebSocketsModule& Module = FModuleManager::LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets"));
	Socket = Module.CreateWebSocket(Adresse(), FString());
	if (!Socket.IsValid())
	{
		PrevoirNouvelEssai();
		return;
	}
	Socket->OnConnected().AddRaw(this, &FConnexionChatGlobal::SurConnecte);
	Socket->OnConnectionError().AddRaw(this, &FConnexionChatGlobal::SurErreur);
	Socket->OnClosed().AddRaw(this, &FConnexionChatGlobal::SurFermeture);
	Socket->OnMessage().AddRaw(this, &FConnexionChatGlobal::SurMessage);
	EtatActuel = EEtatChatGlobal::Connexion;
	bBienvenue = false;
	UE_LOG(LogTemp, Log, TEXT("[ChatGlobal] Connexion a %s ..."), *Adresse());
	Socket->Connect();
}

void FConnexionChatGlobal::Fermer()
{
	if (Socket.IsValid())
	{
		Socket->OnConnected().RemoveAll(this);
		Socket->OnConnectionError().RemoveAll(this);
		Socket->OnClosed().RemoveAll(this);
		Socket->OnMessage().RemoveAll(this);
		if (Socket->IsConnected())
		{
			Socket->Close(1000, TEXT("au revoir"));
		}
		// Pas de destruction ici : on peut etre en plein dans un evenement de cette WebSocket.
		// Elle est relachee au prochain passage de la boucle.
		AJeter.Add(Socket);
		Socket.Reset();
	}
	bBienvenue = false;
	NombreEnLigne = 0;
	NombreJeu = 0;
}

void FConnexionChatGlobal::PrevoirNouvelEssai()
{
	const float Delai = Delais[FMath::Min(Essais, (int32)UE_ARRAY_COUNT(Delais) - 1)];
	++Essais;
	ProchainEssai = Maintenant() + Delai;
	EtatActuel = EEtatChatGlobal::Attente;
}

// ---------------------------------------------------------------- Envoi

void FConnexionChatGlobal::EnvoyerJson(const TSharedRef<FJsonObject>& Objet)
{
	if (!Socket.IsValid() || !Socket->IsConnected())
	{
		return;
	}
	FString Texte;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Ecrivain = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Texte);
	FJsonSerializer::Serialize(Objet, Ecrivain);
	Socket->Send(Texte); // envoye en UTF-8
}

void FConnexionChatGlobal::DireBonjour()
{
	TSharedRef<FJsonObject> Objet = MakeShared<FJsonObject>();
	Objet->SetStringField(TEXT("type"), TEXT("bonjour"));
	Objet->SetStringField(TEXT("pseudo"), PseudoMemorise());
	Objet->SetStringField(TEXT("source"), TEXT("jeu"));
	EnvoyerJson(Objet);
}

bool FConnexionChatGlobal::ChoisirPseudo(const FString& Pseudo)
{
	const FString Propre = NettoyerPseudo(Pseudo);
	if (Propre.IsEmpty())
	{
		AjouterLocal(TEXT("Pseudo pas valable : 3 \u00E0 20 lettres ou chiffres (espace, _ - . autoris\u00E9s)."));
		return false;
	}
	MemoriserPseudo(Propre);
	if (EtatActuel == EEtatChatGlobal::Bloque)
	{
		return true; // expulse / banni : on ne se reconnecte pas tout seul
	}
	Essais = 0;
	if (Socket.IsValid() && Socket->IsConnected() && !bBienvenue)
	{
		DireBonjour(); // pseudo refuse juste avant : on retente sur la meme connexion
	}
	else
	{
		// Nouveau pseudo : nouvelle connexion (le serveur ne change pas un pseudo en cours de route)
		Fermer();
		EtatActuel = EEtatChatGlobal::Attente;
		ProchainEssai = 0.0;
	}
	return true;
}

void FConnexionChatGlobal::Envoyer(const FString& Brut)
{
	const FString Texte = Brut.Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" ")).TrimStartAndEnd();
	if (Texte.IsEmpty())
	{
		return;
	}
	if (Texte.StartsWith(TEXT("/pseudo"), ESearchCase::IgnoreCase))
	{
		ChoisirPseudo(Texte.Mid(7));
		return;
	}
	if (EtatActuel == EEtatChatGlobal::SansPseudo)
	{
		ChoisirPseudo(Texte);
		return;
	}
	if (!bBienvenue)
	{
		AjouterLocal(TEXT("Pas encore connect\u00E9 au chat global, r\u00E9essaie dans un instant."));
		return;
	}
	TSharedRef<FJsonObject> Objet = MakeShared<FJsonObject>();
	Objet->SetStringField(TEXT("type"), TEXT("message"));
	Objet->SetStringField(TEXT("texte"), Texte.Left(LimiteTexte));
	EnvoyerJson(Objet);
}

// ---------------------------------------------------------------- Lignes

void FConnexionChatGlobal::Ajouter(const FLigneChatGlobal& Ligne)
{
	Messages.Add(Ligne);
	if (Messages.Num() > LignesMax)
	{
		Messages.RemoveAt(0, Messages.Num() - LignesMax);
	}
	++NumeroVersion;
	HeureDernierMessage = Maintenant();
}

void FConnexionChatGlobal::AjouterLocal(const FString& Texte)
{
	FLigneChatGlobal L;
	L.Texte = Texte;
	L.Source = ESourceChatGlobal::Local;
	Ajouter(L);
}

// ---------------------------------------------------------------- Evenements

void FConnexionChatGlobal::SurConnecte()
{
	UE_LOG(LogTemp, Log, TEXT("[ChatGlobal] Connecte, presentation en cours."));
	DireBonjour();
}

void FConnexionChatGlobal::SurErreur(const FString& Erreur)
{
	UE_LOG(LogTemp, Warning, TEXT("[ChatGlobal] Connexion impossible : %s"), *Erreur);
	if (Essais == 0)
	{
		AjouterLocal(TEXT("Serveur du chat injoignable, nouvel essai automatique..."));
	}
	Fermer();
	PrevoirNouvelEssai();
}

void FConnexionChatGlobal::SurFermeture(int32 Code, const FString& Raison, bool bPropre)
{
	UE_LOG(LogTemp, Log, TEXT("[ChatGlobal] Connexion fermee (code %d %s)."), Code, *Raison);
	const bool bEtaitConnecte = bBienvenue;
	Fermer();
	if (Code == 4001 || Code == 4003)
	{
		EtatActuel = EEtatChatGlobal::Bloque;
		AjouterLocal(Code == 4003
			? FString(TEXT("Tu es banni du chat global."))
			: FString(TEXT("Tu as \u00E9t\u00E9 expuls\u00E9 du chat global. Pour revenir : OPTIONS > JEU > CHAT GLOBAL OFF puis ON.")));
		return;
	}
	if (bEtaitConnecte)
	{
		AjouterLocal(TEXT("Connexion au chat global perdue, reconnexion..."));
		Essais = 0;
	}
	PrevoirNouvelEssai();
}

void FConnexionChatGlobal::SurMessage(const FString& Texte)
{
	TSharedPtr<FJsonObject> Objet;
	TSharedRef<TJsonReader<TCHAR>> Lecteur = TJsonReaderFactory<TCHAR>::Create(Texte);
	if (!FJsonSerializer::Deserialize(Lecteur, Objet) || !Objet.IsValid())
	{
		return;
	}
	const FString Type = Champ(Objet, TEXT("type"));

	if (Type == TEXT("message"))
	{
		Ajouter(LigneDepuisJson(Objet));
	}
	else if (Type == TEXT("systeme"))
	{
		FLigneChatGlobal L;
		L.Texte = Champ(Objet, TEXT("texte"));
		L.Source = ESourceChatGlobal::Systeme;
		Ajouter(L);
	}
	else if (Type == TEXT("bienvenue"))
	{
		bBienvenue = true;
		Essais = 0;
		EtatActuel = EEtatChatGlobal::Connecte;
		PseudoConnecte = Champ(Objet, TEXT("pseudo"));
		const TSharedPtr<FJsonObject>* Limites = nullptr;
		double Max = 0.0;
		if (Objet->TryGetObjectField(TEXT("limites"), Limites) && Limites && (*Limites)->TryGetNumberField(TEXT("texteMax"), Max) && Max > 0.0)
		{
			LimiteTexte = FMath::Clamp((int32)Max, 1, 1000);
		}
		UE_LOG(LogTemp, Log, TEXT("[ChatGlobal] Bienvenue, pseudo %s."), *PseudoConnecte);
	}
	else if (Type == TEXT("historique"))
	{
		// On garde les messages locaux, on remplace l'ancien historique du serveur
		Messages.RemoveAll([](const FLigneChatGlobal& L) { return L.Source == ESourceChatGlobal::Jeu || L.Source == ESourceChatGlobal::Site; });
		const TArray<TSharedPtr<FJsonValue>>* Liste = nullptr;
		if (Objet->TryGetArrayField(TEXT("messages"), Liste) && Liste)
		{
			for (const TSharedPtr<FJsonValue>& V : *Liste)
			{
				if (V.IsValid() && V->Type == EJson::Object)
				{
					Messages.Add(LigneDepuisJson(V->AsObject()));
				}
			}
		}
		if (Messages.Num() > LignesMax)
		{
			Messages.RemoveAt(0, Messages.Num() - LignesMax);
		}
		AjouterLocal(FString::Printf(TEXT("Connect\u00E9 au chat global en tant que %s."), *PseudoConnecte));
	}
	else if (Type == TEXT("presence"))
	{
		double N = 0.0;
		Objet->TryGetNumberField(TEXT("en_ligne"), N);
		NombreEnLigne = (int32)N;
		NombreJeu = 0;
		const TArray<TSharedPtr<FJsonValue>>* Joueurs = nullptr;
		if (Objet->TryGetArrayField(TEXT("joueurs"), Joueurs) && Joueurs)
		{
			for (const TSharedPtr<FJsonValue>& V : *Joueurs)
			{
				if (V.IsValid() && V->Type == EJson::Object && Champ(V->AsObject(), TEXT("source")) == TEXT("jeu"))
				{
					++NombreJeu;
				}
			}
		}
	}
	else if (Type == TEXT("effacer"))
	{
		Messages.Reset();
		++NumeroVersion;
	}
	else if (Type == TEXT("refus"))
	{
		const FString Raison = Champ(Objet, TEXT("raison"));
		if (Raison == TEXT("banni"))
		{
			EtatActuel = EEtatChatGlobal::Bloque;
			AjouterLocal(TEXT("Tu es banni du chat global."));
		}
		else
		{
			// Pseudo refuse : on l'oublie et on en demande un autre (la connexion reste ouverte)
			MemoriserPseudo(FString());
			EtatActuel = EEtatChatGlobal::SansPseudo;
			const FString Pourquoi = Champ(Objet, TEXT("texte"));
			AjouterLocal((Pourquoi.IsEmpty() ? FString(TEXT("Pseudo refus\u00E9.")) : Pourquoi) + TEXT(" Choisis un autre pseudo."));
		}
	}
}
