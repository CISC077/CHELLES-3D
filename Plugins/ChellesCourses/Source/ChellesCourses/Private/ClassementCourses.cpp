// CHELLES 3D - Courses de rue : classement de tous les joueurs (serveur du VPS)
#include "ClassementCourses.h"

#include "CoreGlobals.h"
#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	FString PseudoPropre(const FString& Brut)
	{
		FString P;
		for (const TCHAR C : Brut)
		{
			if (C >= 32 && C != TEXT('|') && C != TEXT('"') && C != TEXT('\\'))
			{
				P.AppendChar(C);
			}
		}
		P = P.TrimStartAndEnd().Left(20);
		return P.IsEmpty() ? FString(TEXT("Joueur")) : P;
	}
}

FClassementCourses& FClassementCourses::Get()
{
	static FClassementCourses Instance;
	return Instance;
}

void FClassementCourses::Configurer(bool bEnLigne, const FString& NouvelleAdresse)
{
	bEnLigneActif = bEnLigne;
	if (NouvelleAdresse.StartsWith(TEXT("http")))
	{
		Adresse = NouvelleAdresse;
		Adresse.RemoveFromEnd(TEXT("/"));
	}
}

FString FClassementCourses::PseudoLocal()
{
	FString Pseudo;
	if (GConfig)
	{
		GConfig->GetString(TEXT("ChellesChat"), TEXT("Pseudo"), Pseudo, GGameUserSettingsIni);
	}
	Pseudo = Pseudo.TrimStartAndEnd();
	return PseudoPropre(Pseudo.Len() >= 3 ? Pseudo : FString(TEXT("Joueur")));
}

FString FClassementCourses::Temps(int32 Ms)
{
	Ms = FMath::Max(0, Ms);
	const int32 Minutes = Ms / 60000;
	const int32 Secondes = (Ms / 1000) % 60;
	const int32 Centiemes = (Ms / 10) % 100;
	return FString::Printf(TEXT("%d:%02d.%02d"), Minutes, Secondes, Centiemes);
}

FString FClassementCourses::Encoder(const FString& Texte)
{
	return FGenericPlatformHttp::UrlEncode(Texte);
}

void FClassementCourses::Rafraichir(const FString& Id, bool bForcer)
{
	FClassementUneCourse& C = Courses.FindOrAdd(Id);
	if (!bEnLigneActif)
	{
		C.Etat = EEtatClassementCourses::Coupe;
		return;
	}
	const double Maintenant = FPlatformTime::Seconds();
	if (!bForcer && (C.Etat == EEtatClassementCourses::Chargement || Maintenant - C.DerniereDemande < 20.0))
	{
		return;
	}
	C.DerniereDemande = Maintenant;
	if (C.Etat != EEtatClassementCourses::Pret)
	{
		C.Etat = EEtatClassementCourses::Chargement;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Requete = FHttpModule::Get().CreateRequest();
	Requete->SetURL(Url(TEXT("/api/courses?limite=10&course=") + Encoder(Id)));
	Requete->SetVerb(TEXT("GET"));
	Requete->SetHeader(TEXT("Accept"), TEXT("application/json"));
	const FString IdCopie = Id;
	Requete->OnProcessRequestComplete().BindLambda([IdCopie](FHttpRequestPtr, FHttpResponsePtr Reponse, bool bReussi)
	{
		FClassementUneCourse& Classement = FClassementCourses::Get().Courses.FindOrAdd(IdCopie);
		TSharedPtr<FJsonObject> Racine;
		if (bReussi && Reponse.IsValid() && Reponse->GetResponseCode() == 200)
		{
			const TSharedRef<TJsonReader<>> Lecteur = TJsonReaderFactory<>::Create(Reponse->GetContentAsString());
			FJsonSerializer::Deserialize(Lecteur, Racine);
		}
		const TArray<TSharedPtr<FJsonValue>>* Valeurs = nullptr;
		if (!Racine.IsValid() || !Racine->TryGetArrayField(TEXT("meilleurs"), Valeurs) || !Valeurs)
		{
			if (Classement.Etat != EEtatClassementCourses::Pret)
			{
				Classement.Etat = EEtatClassementCourses::HorsLigne;
			}
			UE_LOG(LogTemp, Log, TEXT("[Courses] Classement en ligne injoignable (code %d) : records de ce PC seulement."),
				Reponse.IsValid() ? Reponse->GetResponseCode() : 0);
			return;
		}
		Classement.Meilleurs.Reset();
		for (const TSharedPtr<FJsonValue>& Valeur : *Valeurs)
		{
			const TSharedPtr<FJsonObject> O = Valeur.IsValid() ? Valeur->AsObject() : nullptr;
			if (!O.IsValid())
			{
				continue;
			}
			FLigneCourseEnLigne L;
			FString Pseudo;
			O->TryGetStringField(TEXT("pseudo"), Pseudo);
			L.Pseudo = PseudoPropre(Pseudo);
			O->TryGetNumberField(TEXT("rang"), L.Rang);
			O->TryGetNumberField(TEXT("temps_ms"), L.TempsMs);
			O->TryGetStringField(TEXT("mode"), L.Mode);
			O->TryGetStringField(TEXT("quand"), L.Date);
			L.Date = L.Date.Left(10);
			Classement.Meilleurs.Add(L);
		}
		Racine->TryGetNumberField(TEXT("total"), Classement.Total);
		Classement.Etat = EEtatClassementCourses::Pret;
	});
	Requete->ProcessRequest();
}

void FClassementCourses::Envoyer(const FString& Id, const FString& Nom, int32 TempsMs, int32 Portes, float LongueurM, const FString& Mode)
{
	FClassementUneCourse& C = Courses.FindOrAdd(Id);
	C.RangEnvoye = 0;
	C.bReponseEnvoi = false;
	if (!bEnLigneActif)
	{
		return;
	}
	const TSharedRef<FJsonObject> Objet = MakeShared<FJsonObject>();
	Objet->SetStringField(TEXT("pseudo"), PseudoLocal());
	Objet->SetStringField(TEXT("course"), Id);
	Objet->SetStringField(TEXT("nom"), Nom);
	Objet->SetNumberField(TEXT("temps_ms"), TempsMs);
	Objet->SetNumberField(TEXT("portes"), Portes);
	Objet->SetNumberField(TEXT("longueur_m"), FMath::RoundToInt(LongueurM));
	Objet->SetStringField(TEXT("mode"), Mode);
	FString Corps;
	const TSharedRef<TJsonWriter<>> Ecrivain = TJsonWriterFactory<>::Create(&Corps);
	FJsonSerializer::Serialize(Objet, Ecrivain);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Requete = FHttpModule::Get().CreateRequest();
	Requete->SetURL(Url(TEXT("/api/courses")));
	Requete->SetVerb(TEXT("POST"));
	Requete->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));
	Requete->SetHeader(TEXT("X-Chelles-Jeu"), TEXT("courses-1"));
	Requete->SetContentAsString(Corps);
	const FString IdCopie = Id;
	Requete->OnProcessRequestComplete().BindLambda([IdCopie](FHttpRequestPtr, FHttpResponsePtr Reponse, bool bReussi)
	{
		FClassementCourses& Tout = FClassementCourses::Get();
		FClassementUneCourse& Classement = Tout.Courses.FindOrAdd(IdCopie);
		Classement.bReponseEnvoi = true;
		if (bReussi && Reponse.IsValid() && Reponse->GetResponseCode() == 200)
		{
			TSharedPtr<FJsonObject> Racine;
			const TSharedRef<TJsonReader<>> Lecteur = TJsonReaderFactory<>::Create(Reponse->GetContentAsString());
			if (FJsonSerializer::Deserialize(Lecteur, Racine) && Racine.IsValid())
			{
				Racine->TryGetNumberField(TEXT("rang"), Classement.RangEnvoye);
				Racine->TryGetNumberField(TEXT("total"), Classement.Total);
				UE_LOG(LogTemp, Log, TEXT("[Courses] Temps envoye au serveur : place %d sur %d."), Classement.RangEnvoye, Classement.Total);
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[Courses] Envoi du temps impossible (code %d) : il reste sur ce PC."),
				Reponse.IsValid() ? Reponse->GetResponseCode() : 0);
		}
		Tout.Rafraichir(IdCopie, true);
	});
	Requete->ProcessRequest();
}
