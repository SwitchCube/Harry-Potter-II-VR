# Building Harry Potter II VR

## English

This repository contains the mod source for the v1.0.2 implementation. The
native gameplay source matches the native inputs used for the published v1.0.2
release. Publication adds portable setup helpers and removes machine-specific
development data. Download the ready-to-use mod from [Releases](https://github.com/SwitchCube/Harry-Potter-II-VR/releases/latest).

### Requirements

- Windows 10/11 x64 with PowerShell and **64-bit Python 3.11 or newer** available
  as `python`. The produced mod binaries target 32-bit Windows, matching the game.
- Your own supported Harry Potter and the Chamber of Secrets (2002 PC)
  installation, engine build `HPCos_021009_1205-1`. The exact reference files
  are listed by relative path and SHA-256 in `config/target-fingerprint.json`.
- Internet access for the pinned Zig 0.15.2, OpenVR, MinHook, Capstone and Python
  runtime dependencies. Downloads are verified against checked-in hashes and
  stored inside this checkout. SteamVR is needed to play in VR.

Use a separate checkout in a writable directory. The commands below run from
the repository root. In the first command, replace the example directory with
your own game installation:

```powershell
python -B scripts/configure_local.py --game-directory 'D:\Games\Harry Potter II'
if ($LASTEXITCODE -ne 0) { throw 'Original game verification failed' }
./scripts/Setup-ProbeToolchain.ps1
python -B scripts/setup_native_dependencies.py
if ($LASTEXITCODE -ne 0) { throw 'Native dependency setup failed' }
python -B scripts/setup_analysis.py
if ($LASTEXITCODE -ne 0) { throw 'Analysis setup failed' }
python -B scripts/setup_release_dependencies.py
if ($LASTEXITCODE -ne 0) { throw 'Release dependency setup failed' }
```

`configure_local.py` verifies and copies only the fingerprinted files into the
ignored `system/` directory. It leaves the source installation unchanged and
creates an empty development profile and ignored `config/project.json`.
It does not import saved games. A different game build is rejected; do not
bypass the checks or replace the recorded hashes to make it pass.

### Compile, test and package

```powershell
python -B -m unittest discover -s tests -p 'test_*.py'
if ($LASTEXITCODE -ne 0) { throw 'Python tests failed' }
$build = ./scripts/Build-NativeVR.ps1 | Select-Object -Last 1
python -B scripts/package_release.py --build $build --output dist/HP2VR-1.0.2-local.zip
if ($LASTEXITCODE -ne 0) { throw 'Packaging failed' }
```

The native build runs the C++ math, pixel, controller, capability, HUD fallback
and frame-lifecycle checks. These checks use the local graphics stack but do
not launch the game or a headset session. Python tests include checks against
the verified original files, so those files are required for the full suite.

Build output is under `build/native-vr-<timestamp>/`. Packaging produces a ZIP
and SHA-256 checksum in `dist/`. A locally generated Large Address Aware game
image stays in the ignored build directory; the game executable and assets
are never included in the mod ZIP. Install the resulting ZIP into a separate
working game installation following the release instructions. A full playthrough
and physical headset checks remain separate from these automated tests.

### Source layout

| Directory | Contents |
| --- | --- |
| `src/` | C++ VR integration, graphics, controls, menus, launchers and ABI descriptors |
| `scripts/` | Build, dependency setup, packaging, profile management and diagnostics |
| `release/` | Launcher Python source, command scripts, default options and player instructions |
| `config/vr/` | SteamVR action manifests and controller bindings |
| `config/*.lock.json` | Pinned development and runtime dependencies |
| `tests/` | Python tests, C++ checks and optional game integration test tools |

Legacy development launchers and game integration smoke tests require a complete
local game copy and additional local session settings. They are not part of the
build commands above. Use the packaged launcher for normal play.

Third-party components retain their own licenses, referenced in
[`release/THIRD-PARTY.txt`](../release/THIRD-PARTY.txt) and included in built ZIPs.
Downloaded dependencies, compiled binaries, original game files, profiles,
savegames, logs, local paths and development history must stay out of source
commits. `.gitignore` excludes these files. Before publishing, inspect the staged
diff and use a pseudonymous Git author with a GitHub noreply email address.
Finished mod ZIPs belong in GitHub Releases.

The historical `v1.0.2` tag identifies the original distribution commit. Browse
`main` for the published source. GitHub's automatically generated archives of
older tags reflect their historical contents.

## Deutsch

Dieses Repository enthält den Mod-Quellcode der Implementierung v1.0.2.
Die nativen Gameplay-Quelldateien entsprechen den Eingaben des veröffentlichten
Builds. Fertige Downloads stehen unter [Releases](https://github.com/SwitchCube/Harry-Potter-II-VR/releases/latest).

Vorausgesetzt werden Windows 10/11 x64, PowerShell, **64-Bit-Python ab 3.11**
und eine eigene unterstützte HP2-PC-Installation (`HPCos_021009_1205-1`).
Führe die oben aufgeführten Befehle im Stammverzeichnis eines separaten
Checkouts aus. Ersetze den Beispiel-Spielpfad durch deinen Installationsordner.
Das Setup prüft die Originaldateien anhand ihrer SHA-256-Werte und kopiert nur
die für den Build benötigten Dateien. Die Originalinstallation bleibt unverändert;
Spielstände werden nicht übernommen. Abhängigkeiten werden mit festen Versionen
und Prüfsummen lokal heruntergeladen.

Die zweite Befehlsgruppe führt die Python-Tests aus, kompiliert die 32-Bit-Mod
einschließlich nativer Tests und erzeugt `dist/HP2VR-1.0.2-local.zip` samt
Prüfsumme. Originalspiel, Spielstände und persönliche Einstellungen werden nicht
in das Mod-Paket aufgenommen. Ein kompletter Spieldurchgang und ein echter
Headset-Test werden damit nicht ersetzt.

`src/` enthält C++, `scripts/` die Entwicklungswerkzeuge, `release/` den
Python-Starter und die Installationsdateien, `config/vr/` die Controllerbelegungen
und `tests/` die Prüfungen. Fertige Mod-Pakete gehören ausschließlich unter
GitHub Releases. Lokale Konfigurationen, Protokolle, Spielstände, Binärdateien
und Originalspieldateien sind durch `.gitignore` ausgeschlossen.
Vor einem Upload den vorgemerkten Diff sowie Git-Autor und noreply-Adresse prüfen.

Der historische Tag `v1.0.2` bleibt dem ursprünglichen Download zugeordnet.
Der veröffentlichte Quellcode liegt auf `main`; automatische GitHub-Archive alter
Tags behalten den damaligen Inhalt.
