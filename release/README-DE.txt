HARRY POTTER II VR - v1.0.2

Installation
1. Installiere deine eigene PC-Version von Harry Potter und die Kammer des
   Schreckens. Prüfe zuerst, dass das normale Spiel auf deinem PC startet.
2. Entpacke dieses ZIP in den Spielordner. Start-HP2VR.cmd liegt danach NEBEN
   dem vorhandenen Ordner System. Es werden keine Originaldateien ersetzt.
3. Starte Start-HP2VR.cmd. Im ursprünglichen Hauptmenü wählst du Flat oder VR.
   Für VR zuerst SteamVR starten und das Headset verbinden.
   Eine vorhandene CD-/Desktop-Verknüpfung startet weiterhin das Originalspiel.
   Du kannst eine eigene Verknüpfung auf Start-HP2VR.cmd anlegen.

Flat startet die originale Game.exe ohne VR-Modifikation. VR verwendet die
Mod. Beide Modi benutzen dieselben sechs Speicherplätze des normalen Spiels.
Beim Wechsel wird das Hauptmenü neu geöffnet. Beende das Startfenster erst,
nachdem das Spiel geschlossen wurde und die Spielstände abgeglichen sind.
Nach einem Absturz holt der nächste Start fertig geschriebene VR-Spielstände
nach. Unvollständige Save.tmp-Dateien werden niemals als Spielstand übernommen.
Starte nicht gleichzeitig eine zweite Installation oder das Spiel direkt.

Voraussetzungen und Grenzen
- Windows 10/11 auf einem x64-PC, funktionierende Originalinstallation,
  SteamVR und ein darin eingerichtetes Headset mit zwei Controllern.
- Die Mod ist an die Engine-Version HPCos_021009_1205-1 gebunden. Vor VR-Start
  werden Game.exe, Engine-Komponenten und HGame.u geprüft. Bei einer anderen
  Ausgabe/Patch-Version bleibt Flat verfügbar; VR zeigt eine Fehlermeldung.
  Die Sprache allein sagt nichts über die unterstützte Binärversion aus.
- Grafikausgabe über Direct3D 11 auf der von SteamVR gemeldeten Grafikkarte;
  kein fest eingebauter Hersteller oder GPU-Name. Andere PCs/Headsets wurden
  nicht physisch geprüft. Kein Anspruch auf jede CD-Ausgabe oder jeden Treiber.
- Quest/Touch wurde praktisch getestet. Weitere Standardbelegungen und deren
  Grenzen stehen in CONTROLLERS.txt. SteamVR bietet eigene Controllerbelegung.
- Die komplette Handlung ist noch nicht lückenlos auf allen Systemen geprüft.
  Drahtlose Tonlücken/Kompressionsartefakte können weiterhin auftreten.

Sprache und Leistung
Die zusätzlichen Mod-Menüs erkennen Language aus Game.ini, ersatzweise aus
System/Default.ini. Deutsch wird deutsch angezeigt, Englisch englisch; andere
Sprachen verwenden für neue Mod-Texte Englisch. Texte, Stimmen und Ressourcen
des installierten Originalspiels bleiben in dessen Sprache.
In HP2VR/Options.ini kannst du Language=de/en/auto und Quality ändern:
performance (1280), balanced (1536, Vorgabe), sharp (2048). Bei Ruckeln zuerst
performance probieren. Keine SteamVR-, WLAN- oder Audio-Systemeinstellungen
werden automatisch geändert. Die Mod lädt beim Start nichts aus dem Internet.
Zusätzlich begrenzt AutoQuality die Auflösung anhand des lokalen Grafikspeicherbudgets:
unter 2 GB oder bei unbekanntem Speicher auf 1280, unter 4 GB auf 1536.
AutoQuality=0 in der Benutzerdatei config/hp2vr.ini hebt diese Begrenzung auf.
Die Speicherprüfung ersetzt keine Leistungsmessung und garantiert keine Bildrate.

Steuerung (Touch, rechte Hand)
Linker Stick: laufen/seitwärts; rechter Stick: drehen; A: springen/Flugaktion;
rechter Trigger: zaubern/werfen/auswählen; B: Menü/zurück; X: bestätigen;
Y: Trank; linker Trigger: Karte; linker Griff halten: Handanzeige;
rechter Griff: gehen/Besen-Boost; linker Stickklick: Blick zentrieren;
rechter Stickklick: zusätzlich speichern. Speicherbücher bleiben aktiv.
Im Spielmenü unter Controllerbelegung lassen sich Aktionen neu belegen.
Filmsequenzen lassen sich dort zwischen Leinwand und 3D/360 Grad umschalten.

Fehler melden
Diagnose-HP2VR.cmd neben dem Starter erstellt eine ZIP mit Start- und
Laufzeitprotokollen, Windows-Version, Grafikadaptern und Dateiprüfungen.
Der Berichtsordner öffnet sich anschließend. Sende die ZIP mit einer kurzen
Fehlerbeschreibung und deinem Headset-Modell an den Mod-Betreuer.
Persönliche Pfade sowie Benutzer-/PC-Namen werden aus dem Bericht entfernt.
Spielstände und persönliche Einstellungen werden nicht eingepackt oder
verändert. Es gibt keinen automatischen Upload. Ein erneuter Absturz ist
nicht nötig. Details und eigene Datenpfade: README-DIAGNOSE.txt.
Fehlermeldungen verweisen auf das Werkzeug; Startfehler werden protokolliert.
v1.0.2 ergänzt einen Ausweichweg für abgelehnte Menü-/HUD-Grafikflächen.
Der gemeldete Speicherfehler wurde in Tests nachgestellt; die Bestätigung auf
dem betroffenen Test-PC steht noch aus.

Künftige Updates
Neue Versionen erscheinen unter Releases auf der GitHub-Projektseite.
Beende das Spiel und befolge die Update-Anleitung der jeweiligen Version.

Daten, Sicherungen und Deinstallation
Benutzereinstellungen, private Laufzeitkopie, Protokolle und Sicherungen liegen
unter %LOCALAPPDATA%/HP2VR/<Installationskennung>. Der Starter schreibt keine
Mod-Dateien in System und benötigt normalerweise keine Administratorrechte.
Ein schreibgeschützter Spielordner ist möglich. Der normale Spielstand liegt
weiterhin im Windows-Dokumente-Ordner unter Harry Potter II/Save.
Bei unterschiedlichen Laufwerken liegen zusätzliche Austausch-Sicherungen in
Save/.hp2vr-transactions. Alte Spielstände werden nicht stillschweigend gelöscht.
Diese Sicherungen enthalten persönliche Spieldaten: zum Teilen NUR das
unveränderte Veröffentlichungs-ZIP verwenden, keine Laufzeit-/Save-Ordner.

Zum Entfernen: Spiel beenden, HP2VR-Ordner, Start-HP2VR.cmd, Diagnose-HP2VR.cmd
und README-HP2VR.txt aus dem
Spielverzeichnis entfernen. Deine Originaldateien und gemeinsamen Spielstände
bleiben erhalten. Benutzerdaten/Sicherungen erst entfernen, wenn du sie nicht
mehr brauchst. --check prüft das Paket ohne Spielstart; --data-root PFAD erlaubt
einen kürzeren, selbst gewählten Datenordner für sehr lange Windows-Benutzerpfade.

Inhalt
Dieses Paket enthält nur die Mod, kleine Laufzeitkomponenten und deren Lizenzen.
Keine Original-EXE, Spielgrafiken, Musik, Spielstände oder persönlichen Profile.
Eine eigene Originalinstallation ist erforderlich. Inoffizielle Fan-Mod;
keine Verbindung zu den Rechteinhabern des Spiels oder SteamVR.
