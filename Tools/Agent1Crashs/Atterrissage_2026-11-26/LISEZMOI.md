# Vol : ramené au point de départ en redescendant (26/11/2026, agent 1)

Cause : `ARoueModes::EmpecherChute` (ChellesRoue) prenait le joueur pour « passé sous la carte » dès qu'il
était 25 m plus bas que son dernier point au sol. Parti d'un toit ou d'une butte, on vole plus loin et on
redescend plus bas que ce point : il était renvoyé au décollage, à mi-descente.

Correctif : avant de le rattraper, un rayon vers le bas cherche le sol. S'il y a du sol dessous (à moins
de 150 m sous le décollage), on le laisse descendre. Seul un vrai passage sous la carte le ramène.

À lancer : `Tools\SuperheroVol\REPARER_ATTERRISSAGE.bat`.
