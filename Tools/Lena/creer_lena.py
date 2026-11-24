# -*- coding: utf-8 -*-
# Remplace par Tools\Personnages\creer_personnages.py (LENA, SOFIA... en une fois). Ce fichier lance simplement celui-la.
import os
import unreal
_script = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", "Personnages", "creer_personnages.py")
exec(compile(open(_script, encoding="utf-8").read(), _script, "exec"))
