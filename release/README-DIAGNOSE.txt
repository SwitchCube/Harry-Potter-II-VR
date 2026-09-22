HP2VR v1.0.2 - Diagnose / Diagnostics

DEUTSCH
Das Diagnosewerkzeug ist fester Bestandteil der Mod. Es sammelt vorhandene
Start- und Laufzeitprotokolle, Windows-Version und Grafikadapter. Ausserdem
prueft es Mod-Dateien und die Original-Spielversion anhand von Pruefsummen.
Das Spiel und SteamVR werden dabei nicht gestartet. Spielstaende und
persoenliche Einstellungen werden nicht gelesen/geaendert.

1. Im Spielordner Diagnose-HP2VR.cmd neben Start-HP2VR.cmd suchen.
2. Diagnose-HP2VR.cmd doppelklicken. Ein erneuter Spielabsturz ist nicht noetig.
3. Der Ordner mit HP2VR-diagnostics-....zip wird geoeffnet. Diese ZIP zur
   Fehleranalyse an den Mod-Betreuer senden. Bitte dazu beschreiben, was kurz
   vor dem Fehler passiert ist und welches Headset verwendet wurde.

Der Bericht wird im vorhandenen Mod-Datenordner unter diagnostics erzeugt.
Benutzername, PC-Name und absolute Pfade werden aus den Texten entfernt.
Wer beim Spiel --data-root verwendet, muss denselben Parameter hier angeben.
Ohne Laufzeitprotokolle meldet das Werkzeug die Anzahl 0; auch der Bericht
enthaelt diese Angabe. Es werden keine Dateien geloescht oder hochgeladen.
Das Werkzeug funktioniert auch vor dem ersten Spielstart oder bei beschaedigten
Mod-Dateien, sofern die mitgelieferte Python-Laufzeit noch funktioniert.
Alternativ: Start-HP2VR.cmd --diagnostics. --no-open unterdrueckt das Oeffnen
des Berichtsordners. Der Bericht enthaelt nur Text/Pruefsummen, keine Spieldateien.
Eine Diagnose repariert keinen Grafikfehler automatisch.

ENGLISH
Diagnostics are included in the mod. They collect existing launcher and native
runtime logs, Windows version and graphics adapter names, and compare mod/game
checksums. They do not start the game or SteamVR, or read/modify saves or user
settings. Reports contain text and checksums, not game files.

Run Diagnose-HP2VR.cmd beside Start-HP2VR.cmd in the game folder. Send the
generated HP2VR-diagnostics-....zip from the folder that opens to the mod
maintainer. Describe what happened before the error and which headset you use.
There is no need to reproduce the crash. The ZIP is written to diagnostics
inside the existing mod data directory. User name, PC name and absolute paths
are removed from report text. Use the same --data-root as the game launcher if
you configured one. No files are deleted or uploaded.
This also works before first launch or with damaged mod files, provided the
bundled Python runtime still works. Alternatively: Start-HP2VR.cmd --diagnostics.
Use --no-open to suppress opening the report folder. This does not repair the
reported crash automatically.
