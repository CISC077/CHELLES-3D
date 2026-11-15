// CHELLES 3D - Cible SERVEUR DEDIE (Linux, sans image ni son).
// Ne se compile qu'avec Unreal 4.27 compile depuis les sources (pas avec le Launcher).
// Copiee dans CHELLES 3D\Source par Tools\ServeurVPS\2_CREER_LE_SERVEUR.bat.

using UnrealBuildTool;
using System.Collections.Generic;

public class VehicleGameServerTarget : TargetRules
{
	public VehicleGameServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		ExtraModuleNames.Add("VehicleGame");
	}
}
