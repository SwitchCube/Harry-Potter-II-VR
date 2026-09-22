"""Run with the ZIP's embedded interpreter, in an isolated test data root."""
import json,os,subprocess,sys
from pathlib import Path
import portable_launcher as p
from project_paths import inside,write_text
from shared_saves import SharedSaves
package=Path(sys.argv[1]);game=package.parent
original=inside('cache/original-profile');original.mkdir(parents=True,exist_ok=True)
manifest=p.verify_package(package);options=p.read_options(package/'Options.ini')
p.LANGUAGE=os.environ.get('HP2VR_LANGUAGE','de')
p.setup_data(package,game,original,inside('.'),manifest,options)
manager=SharedSaves();manager.initialize();manager.recover()
build=inside(p.build_relative(manifest))
p.prepare_image(game/'system/Game.exe',build/'Game.exe',manifest['game_files']['Game.exe'],manifest['runtime_image_sha256'])
profile=p.prepare_profile(game,original)
result=subprocess.run([sys.executable,'-B',str(package/'scripts/stage_vr_bindings.py'),'--profile',str(profile)],capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
assert result.returncode==0,(result.stdout,result.stderr)
manager.begin(profile);manager.finish(profile)
write_text('config/launcher.ini','[Launch]\nMode=VR\n',exclusive=False)
assert p.known_folder('FDD39AD0-238F-46AF-ADB4-6C85480369C7').is_dir()
assert not p.any_game_running()
print(json.dumps(dict(profile=str(profile),build=str(build),game=str(game/'system/Game.exe'),language=p.LANGUAGE,embedded_interpreter=True,original_profile_exists=original.is_dir())))
