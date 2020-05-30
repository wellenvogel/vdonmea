NMEA 0183 für VDO Windmesser
============================

Basierend auf einem Beitrag im [Segeln Forum](https://www.segeln-forum.de/board194-boot-technik/board35-elektrik-und-elektronik/board195-open-boat-projects-org/75527-reparaturhilfe-f%C3%BCr-vdo-windmessgeber/)
habe ich mit einem Arduino Nano und 8 Widerständen einen einfachen NMEA Adapter gebaut, der die Windrichtungsdaten und die Wind-Geschwindigkeit in einen MWV Datensatz übersetzt.
Der Arduino code enhtält einen einfachen Programmier-Mode um einen Abgleich auf die vorhandene Hardware zu machen.
Es müssen die Anschlüsse:
Farbe    |    Bedeutung
---------|--------------
grau     | Referenz-Spannung
weiss    | Geschwindigkeits-Puls
gelb     | "sinus"
grün     | "cosinus"
blau     | Masse

verbunden werden. 

Die Spannungswerte der 3 analogen Signale (grau,gelb,grün) werden mit Spannungsteilern auf auf einen Bereich von 0...ca. 3V umgesetzt, um sicher im Arbeitsbereich der A/D Wandler zu bleiben. Grau wird dabei als Referenz genutzt, damit ist die Versorgungsspannung des Arduino unkritisch.

Die Werte der Widerstände können ggf. im Sketch angepasst werden.

Wichtig ist es, vor der Nutzung eine Kalibrierung vorzunehmen - dazu nach den upload des sketch mit der seriellen Konsole verbinden (Standard: 19200 baud) und durch Eingabe von __xxprog__ in den Programmiermodus schalten.
Danach __minmax__ zum Start des Abgleichs der Minimal- und Maximalwerte angeben. Jetzt die Windfahne mehrfach langsam drehen bis sich die fortlaufend angezeigten Min- und Max-Werte nicht mehr ändern.
Jetzt die Fahne auf 0° stellen und mit dem Befehl __zero__ den 0-Offset einstellen.

Danach __save__ zum Speichern der Werte im Eeprom und __cancel__ zum Verlassen des Programmiermodus.

Es können noch weitere Werte eingestellt werden:

Kommando      |   Wert
--------------|---------
__interval__  | Aussende-Intervall in ms
__knotsperhz__| Knoten pro Hertz zum Anpassen der Geschwindigkeitsanzeige
__minpulse__  | minimale Zahle der Windimpulse, wenn in einem Intervall weniger Impulse vorhanden sind, wird über mehrere Intervalle gemittelt
__averagefactor__| Factor für eine Mittelwertbildung des Winkels (<1)
__talker__ | die genutzte talker id für den NMEA output

Im Programmier-Modus kann über __help__ eine Hilfe ausgegeben werden.

Falls eine "richtige" NMEA0183 Verbindung aufgebaut werden soll, müssen an den Arduino noch Pegelwandler angeschlossen werden (für die serielle Schnittstelle). Ausserdem benötigt er eine Stromversorgung.
Wenn aber ohnehin ein Anschluss per USB erfolgen soll (z.B. an einen Raspberry Pi) kann das direkt über die USB Schnittstelle des Arduino erfolgen.
