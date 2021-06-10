/**
 * @file MPI_Channel.h
 * @author Toni Hollfelder (Toni.Hollfelder@uni-bayreuth.de)
 * @brief Header of MPI Channel
 * @version 0.1
 * @date 2021-01-04
 * 
 * @copyright Copyright (c) 2021
 * 
 * @note Peeking from a channel might need several calls because a first call of MPI_Iprobe does not guarantee to return true 
 * 
 */

#ifndef MPI_CHANNEL_H
#define MPI_CHANNEL_H

//#include <stdlib.h>
#include <stdbool.h>
#include "mpi.h"

// Needed for debugging, copies struct definition of MPI_Channel to header
#include "MPI_Channel_Struct.h"

// Only needed when the user is not supposed to see internal implementation
// Problem is that MPI_Channel can only be used as pointer
//typedef struct MPI_Channel MPI_Channel;
//typedef enum MPI_Chan_internal_type MPI_Chan_internal_type;

// ****************************
// CHANNELS API 
// ****************************

/* @brief     Supposed to check for the best channel implementations
*/
int channel_init();

/**
 * @brief Allocates and returns a channel with MPI_Chan_traits chan_traits for elements of size bytes and capacity n.
 *
 * @param size_t size                 Determines element size in bytes
 * @param unsigned int n              Capacity of channel, maximum number of buffered elements
 * @param MPI_Chan_internal_type it   Represents the internal channel type
 * @param MPI_Comm comm               States the group of processes the channel will be linking
 * @param   int                         States the rank of the target processor (where buffer is saved)
 * 
 * @return  MPI_Channel*        Pointer to a newly allocated MPI_Channel (or NULL if failed) 
 * 
 * @note The success of channel construction depends on the type of channel to be created
 * 
 * @note In general:
 *  - the size of the element must not be 0 or negative
 *  - the communicator must be valid (e.g. no null communicator)
 *  - the target rank must not be negative or greater than the size of the communicator
 * 
 * @note For SPSC channels:
 *  - the capacity of the channels should be 0
 *  - the size of the communicator shhould be 2 

 * 
 * 
*/
MPI_Channel* channel_alloc(size_t size, int n, MPI_Chan_type type, MPI_Comm comm, int is_receiver);

/** 
 * @brief     Sends sz bytes at address data to channel ch
 *
 * @param   MPI_Channel *ch     Pointer to channel *ch
 * @param   void *data          Pointer to start address
 * @param   size_t sz           sz bytes to send 
 * 
 * @result  bool                Returns false if channel is full, true otherwise  
*/
int channel_send(MPI_Channel *ch, void *data /*, size_t sz*/);

/**
 * @brief     Receives an element of sz bytes from channel ch. Element is stored at address data
 *
 * @param   MPI_Channel *ch     Pointer to channel *ch
 * @param   void *data          Pointer to address where it will be stored
 * @param   size_t sz           sz bytes to receive
 * 
 * @result  bool                Returns false if channel is empty, otherwise true
*/
int channel_receive(MPI_Channel *ch, void *data /*, size_t sz*/);

//int channel_send_multiple(MPI_Channel *ch, void *data, int count);

//int channel_receive_multiple(MPI_Channel *ch, void *data, int count);

/**
 * @brief Peeks at the channel and returns the number of messages which can be received (if receiver calls) or sent (if sender calls)
 *
 * @param ch Pointer to a MPI_Channel
 * 
 * @return Number of messages which can be received
 * 
 * @note The returned value of the function depends on the channel type and the caller:
 *  - Sender calls on a buffered channel: returns number of messages which can be sent
 *  - Receiver calls on a buffered channel: returns number of buffered messages which can be received
 *  - Sender calls on a unbuffered, synchronous channel: returns always 1
 *  - Receiver calls on a unbuffered, synchronous channel: returns number of messages which can be received
 * 
 * Different semantics depending on channel type:
 * If channel is buffered:
 *  - Sender: Returns current channel capacity
 *  - Receiver: Returns current number of buffered items (at least 1 if one message is buffered)
 * If channel is unbuffered:
 *  - No use
 * 
 * 
*/
int channel_peek(MPI_Channel *ch);

/** 
 * @brief Deletes the passed MPI_Channel
 * 
 * @param[in,out] ch Pointer to a MPI_Channel 
 * 
 * @return Returns 0 if deletion was succesfull otherwise -1
*/
int channel_free(MPI_Channel *ch);



// TODO: channel_get_rank()
// TODO: Document which functions are collective and/or threadsafe
// TODO: Check if this implementation is independent of endianness 
// TODO: Send_multiple to use advantages of rma

// TODO: Channel_peek dokumentieren, dass Fortschritt nicht garantiet wird...
#endif




/*
 * Optimierungen:
 * - Referenzen der Channelfunktionen in struct speichern und abrufen
 * 
 * 
 */





/*
 *
 * - Alle Channels implementiert
 * 
 * - RMA MPSC BUF:
 *  - 3 Varianten: 
 *    - Circular Buffer bei Receiver 0 mit exklusiven Lock
 *      - Es kann zu jedem Zeitpunkt genau ein Prozess (Producer oder der Consumer) auf Queue zugreifen
 *      - Ist Queue leer oder voll wird iterativ Lock genommen und freigegeben, bis Queue nicht mehr leer/voll ist
 * 
 *    - Circular Buffer bei Receiver 0 mit geteilten bzw. verteilten Lock
 *      - Es können zu jedem Zeitpunkt genau zwei Prozesse (1 Producer und der Consumer) auf Queue zugreifen
 *      - Lock muss bei leerer bzw. voller Queue nicht iterativ genommen und freigegeben werden, aber Producer aktualisiert
 *      iterativ Write und Read Indices bis Queue nicht mehr voll ist
 * 
 *    - Nonblocking Queue ohne Lock (außer MPI_Win_lock(SHARED,...))
 *      - Arbeitsweise ähnlich wie M&S Queue
 *      - Manche Mechanismen wurden allerdings verändert, da M&S Queue weder fair noch starvation-free ist
 *        => Fairness/Starvation-freeness als Channelvoraussetzung
 *      - Zu jedem Zeitpunkt können alle Prozesse (Producer und der Consumer) gleichzeitig auf Liste zugreifen und Fortschritt erzielen
 * 
 *      - Producer können Knoten lokal erstellen und fügen Referenz zu Knoten atomar in Liste ein. 
 *        => Knoten wird erstellt; Adresse mit Tail atomar ausgetauscht; Next von vorherigen Tailknoten aktualisiert
 *        => Implementierung ist damit waitfree für Sender: Jeder channel_send() Aufruf wird bei passender Pufferkapazität in einer 
 *        festen Anzahl von Schritten abgeschlossen
 * 
 *      - Consumer wartet, bis Head einen Knoten referenziert, holt sich die Daten und tauscht Head mit der Adresse
 *        des nächsten Knotens aus
 *        => Implenetierung ist ebenfalls waitfree für Receiver 
 * 
 * 
 *  - RMA MPMC BUF:
 *  - Ähnlich wie RMA MPSC BUF Implementierung
 *  - Größter Unterschied: Da nun mehrere Receiver an der Queue beteiligt sind, wird Zugriff von Consumern auf Queue mit verteilten Lock
 *    geregelt; nötig da sonst Channel_receive() nicht fair/starvationfree arbeitet; in M&S Queue:
 *    - while (node not received):
 *      - Receiver lädt Head und Head.next
 *      - Receiver versucht mittels Compare and Swap Head mit Head.next auszutauschen, falls Head immernoch ursprünglichen
 *        Headknoten referenziert
 *      - if (CAS successful)
 *          return 1;
 * 
 *  - Vorläufige Messergebnisse (auf lokaler Maschine): 
 *    - Variante 1 (exklusiver Lock) ist bei geringer Anzahl an Producern (< 4) am effizientesten
 *      => Kein Overhead mit verteilten Lock; Prozesse müssen keinen Nachfolgerprozess aufwecken
 *    - Je größer die Anzahl der Prozesse wird, desto schlechter wird diese Variante
 *      => Bruteforce Verfahren mit exklusiven Lock skaliert schlecht
 *      => Außerdem: Variante ist nicht fair/starvationfree; hängt von interner MPI_Win_lock() Implementierung ab
 * 
 *    - Variante 2 (verteilter Lock) ist erst bei größerer Produceranzahl effizienter (> 4)
 *    - Implementierung zeigt faire Lockverteilung
 * 
 *    - Variante 3 (Nonblocking Queue) ist ist bei kleiner Anzahl an Producern zwischen Variante 1 und 2 einzuordnen
 *    - Laufzeit verbessert sich aber deutlich schneller mit Anzahl der Producern im Vergleich zu den anderen Varianten
 * 
 * 
 * 
 * 
 * - Durchsatz-Benchmark fertig geschrieben; ist kompatibel mit allen Channels
 * 
 * 
 * 
 * Todo:
 * 
 *  - Messergebnisse der 3 Varianten auf Unicluster verifizieren
 *  - Weitere Benchmarks fertig programmieren 
 *    - Durchsatz mit zufälligen Wartezeiten und Vergleich von Channels mit selber Eigenschaft (SYNC oder BUF) 
 *    - Halotest
 * 
 *  - Channelimplementierung "verschönern" (Union verwenden, nicht verwendete Variablen löschen, konstante Variablen
 *    auslagern, magic numbers als Makro, etc.)
 * 
 *  - Bachelorarbeit weiter ausformulieren
 * 
 */












/*
 *
 * Adresse eines Knoten: Prozessrang + Readindex
 * 
 * LAYOUT SENDER:
 * | R | W | K_1 | ... | K_n |
 * 
 * 1. Überprüfe ob genügenden Platz in Puffer
 * 2. Erzeuge bei Position Write neuen Knoten mit passenden Element
 * 3. Aktualisiere Write
 * 
 * 4. Fetch and Replace letzten Knoten mit neusten Knoten
 * 5. Füge bei neusten Knoten als Next Adresse hinzu
 * 
 * ODER
 * 
 * 4. Hole Adresse des letzten Knoten
 * 5. Aktualisere Next mit geholter Adresse
 * 6. CAS mit geholter Adresse und letzter Adresse und Swappe mit eigenen Knoten
 * 
 * LAYOUT RECEIVER 0:
 * | HEAD | TAIL |
 * 
 * 1. Überprüfe Ne
 * 
 * 1. Hole Headknoten und speichere lokal Next und Value
 * 2. Aktualisere Read bei Sender
 * 3. Aktualisere Head mit Next
 * 
 * 
 * 
 * 
 * Fälle:
 * |  HEAD  |  TAIL  |
 * |  Rang=0, Write=0  |  NULL  |
 * 
 * 
 * Sender:
 * 1. Solange kein Platz, wiederhole
 * 2. Erstelle an Position Writeindex neuen Knoten bestehend aus Next=NULL und Value=Wert
 * 3. Aktualisiere Writeindex
 * 4. Fetch and Replace Adresse des Knoten (Rang, Readindex) mit Tail
 * 5. Falls Tail == NULL
 *    - Nur Dummyknoten hängt an Liste
 *    - Aktualisiere Next von Dummy Knoten
 * 6. Else 
 *    - Aktualisiere Next von altem Tail
 * 
 * Receiver:
 * 1. Solange Next von Dummyknoten NULL, Wiederhole
 * 2. 
 * 
 * 
 * 
 *    HEAD
 *    NULL; 1;2
 * 
 * 
 * 
 * 
 * Gründe für Darstellung von NEXT als Inttupel: 
 * - Es soll nicht ständig neu Speicher allokiert werden, da Speicherallokation in MPI RMA teuer ist (kollektiv)
 * - Producer muss also wissen, welcher Speicher frei ist und Consumer muss wissen, welcher Speicher gelesen werden kann
 * 
 * LAYOUT OF NODE
 * | NEXT | VALUE |
 * where NEXT = (rank, readindex)
 * 
 * 
 * void queue.enqueue(value v):
 *    node* w := new node(v, null); fence(Wk)                     // allocate node for new value
 *    ptr t, n
 *    loop
 *        t := tail.load()                                        // t points to tail node
 *        n := t.p->next.load()                                   // n points to the next node of the current tail
 *        if t = tail.load()                                      // is t still pointing to the current tail?
 *            if n.p = null                                       // was tail pointing to the last node?
 *                if CAS(&t.p->next, n, <w, n.c+1i>)              // try to add w at end of list
 *                    break                                       // success; exit loop
 *            else                                                // tail was not pointing to the last node
 *                (void) CAS(&tail, t, <n.p, t.c+1i>)             // if tail and t are the same change tail to the next node
 *    (void) CAS(&tail, t, <w, t.c+1i>)                            // try to swing tail to inserted node
 * 
 * 
 * 
 * 
 * void dequeue(value v):
 *    // Sperre mit shared lock windows aller Prozesse
 *    MPI_Lock_all(...)         
 * 
 *    // Warte bis mindestens ein Element in Liste eingefügt wurde
 *    while (Dummyknoten.next == NULL)
 *      MPI_Win_sync()
 *  
 *    // Lade Knoten lokal
 *    MPI_Get(Dummyknoten.next, ...)
 * 
 *    // Aktualisiere Readindex
 *    MPI_Replace(Readindex)
 * 
 *    // Teste ob Next null ist
 *    // Falls ja muss überprüft werden ob Knoten einziger ist
 *    if (Knoten.next == null)
 *      // Falls Tail und zweiter Knoten gleich, setzte Tail auf NULL
 *      if CAS(Tail, Knoten, NULL)
 *      else
 *        // Iteriere über Next bis nicht mehr null     
 * 
 *    // Next ist nicht null, es gibt einen dritten Knoten
 *    else
 *      MPI_Replace(Dummy.next, Knoten)
 * 
 * 
 * void enqueue(value v):
 *    // Sperre mit shared lock windows aller Prozesse
 *    MPI_Lock_all(...)   
 * 
 *    // Solange Speicher belegt, kann kein neuer Knoten hinzugefügt werden
 *    while (knotenpuffer voll)
 *      MPI_Win_sync()
 * 
 *    // Erzeuge neuen Knoten an Position read
 *    MPI_Accumulate(NULL, .......)
 *    MPI_Accumulate(v, .......)
 * 
 *    // Aktualisiere Writeindex
 *    MPI_Accumulate(....)
 * 
 *    // Tausche Tail mit Adresse des neu erstellten Knoten aus
 *    MPI_Fetch_and_op(Tail, Knoten ...)
 * 
 *    // Teste ob Liste leer war
 *    if (Tail == NULL)
 *      MPI_Replace(Head.next, Knoten)     
 * 
 *    // Liste war nicht leer
 *    else
 *      MPI_Replace(Tail.next, Knoten)
 * 
 *    // Entsperre window aller Prozesse
 *    MPI_Lock_all(...)   
 * 
 */



/* 01.06.21
 *
 *
 * Nonblocking Queue Implementierung noch nicht fertig
 *  - Probleme mit effizienten und gleichzeitigen Zugriff auf Listenknoten anderer Prozesse 
 * 
 * Kleinere Laufzeittests gemacht:
 *  - MPI_LOCK_EXCLUSIVE  vs MPI_LOCK_SHARED 
 *  - Bei kleiner Prozessanzahl gleiche Laufzeit, mit steigender Prozessanzahl besseres Laufzeitverhalten mit MPI_LOCK_SHARED und verteilten
 *    Listenlock
 * 
 * Grundlagenkapitel weiter ausformuliert
 * 
 * 
 * Schwierigkeiten mit klarer/einheitlicher Channeldefinition:
 *  - In älterer als auch neuerer Literatur keine klare Channeldefinition
 *  - Ursprünglich: (Tony Hoare, CSP, 1978) Synchrone Kommunikation zwischne zwei Prozessen mittels Input/Output-Befehlen über Channels
 *    => Channel als "Verbindungskanal" zwischen zwei Prozessen, über welche Nachrichten aktiv ausgetauscht werden (Message Passing)
 * 
 *  - (Kahn, Determinate Parallel Programming, 1974) Threadkommunikation über getypte Channels mit FIFO Queue Semantik; Empfangen ist blockierend, 
 *    Senden nicht-blockierenden
 *    => konkrete eigenschaften eines Channels beschrieben
 * 
 *  - Heutzutage: (Golang) Getypt, Threadkommunikation, kann (un)gepuffert, (a)synchron, uni-/bidirektional sein, 
 *    eher für Intraprozesskommunikation, also Kommunikation von Threads innerhalb eines Prozesses, gedacht
 *  - (Rust) Getypt, Threadkommunikation, asynchron, unidirektional und MPSC
 *    => Channel als Kommunikationsmechanismus zwischen Threads im gemeinsamen Adressraum
 * 
 *  - Allgemeine Definition hilfreich um Channels von anderen Kommunikationsmechanismen (Sockets, Shared Memory, Pipes) abzugrenzen
 * 
 * Frage zur Abgabe: Müssen alle drei ausgedruckten Exemplare bis zur spätesten Abgabe abgegeben werden?
 * Oder reicht z.B. eine digitale Ausgabe und nachgedruckte Exemplare können nachgereicht werden
 * 
 * Für nächste Woche:
 *  - Bachelorarbeit weiter ausformulieren (Grundlagen, Channeldesign und Implementierung)
 *  - Nonblocking Queue fertig implementieren
 *  - Tests auf dem Unirechner laufen lassen
 * 
 */






/* 24.05.21
 * 
 * RMA MPSC und MPMC SYNC Channels fertig implementiert
 *  - Probleme mit fehlerhaften Zugriff auf lokalen Window Speicher
 * 
 * Bugfixes von bereits implementierten Channels
 *  - z.B. PT2PT MPMC SYNC: Nicht initialisierte Variable führte unter bestimmten Bedingungen zu Fehlern bei MPI_Issend()
 * 
 * Code "richtig" dokumentiert (noch nicht vollständig)
 *  - im Header kurze Channelbeschreibung und Anwenderhinweise zu den Funktionen (Parameter, Return, Error, Seiteneffekte)
 *  - in der Implementierungsdatei Funktionsweise des Channels und weitere Implementierungsdetails
 * 
 * RMA MPSC und MPMC BUF Channels noch nicht fertig implementiert
 *  - Queue mit gleichzeitiger Lese/Schreibmöglichkeit vs Nonblocking Queue
 *  
 */






/* 18.05.21
 *
 * Probleme in der RMA Sync Implementierung mit mehreren Sendern: MPSC und MPMC
 *  - In einer Implementierung im Buch "Using Advanced MPI" wird folgender Code Ausschnitt für das lokale Warten eines Prozesses 
 *  innerhalb eines verteilten Locks verwendet:
 * 
 *    do
 *   {
 *       MPI_Win_sync(ch->win);      // Aktualisiere Prozessspeicher mit "Public Window Copy", wichtig bei non-unified memory model
 *   } while (lmem[nextRank] != -1); // Solange -1, hat sich noch kein neuer Prozess mit seiner Prozessnummer eingeschrieben
 * 
 *   // Wecke nachfolgenden Prozess auf mit atomarer Replace-Funktion
 *   MPI_Accumulate(&zero, 1, MPI_INT, lmem[nextRank], blocked, 1, MPI_INT, MPI_REPLACE, win);
 * 
 *  - Wird benutzt, wenn ein Prozess Zugriff zu einem verteilten Lock will: der vorherige Prozess hat den Lock, der nachfolgende Prozess muss 
 *  sich beim vorherigen Lockholder in den lokalen Speicherbereich (lmem[nextRank]) atomar einschreiben. Gibt der Lockholder den Lock nun zurück 
 *  und weiß, dass sich ein nächster Prozess eingeschrieben hat, ruft dieser den Code auf
 * 
 * 
 *  - Es kann nun aber passieren, dass während lmem[nextRank] aktualisiert wird, der Vergleich mit != -1 mehr als einmal 
 *  aufgerufen wird und die Kontrolle zurückgibt obwohl das Aktualisieren noch nicht beendet ist (trotz Atomarität). Als Konsequenz können Werte 
 *  wie 65535 (2^16-1) in lmem[nextRank] stehen, woraufhin MPI_Accumulate() einen Fehler erzeugt, da es diesen Rang nicht gibt
 *   
 * - Eventuelle Lösung des Problems: 
 *   - Prozess muss seinen lokalen Speicher mittels atomarer MPI Operation lesen:
 *   - MPI_Accumulate(&minus_one, 1, MPI_INT, myrank, nextRank, 1, MPI_INT, MPI_REPLACE, win);
 *   - Kann Implementierung ordentlich verlangsamen
 * 
 * 
 * 
 * Designüberlegung zu RMA Buf Implementierung mit mehreren Sendern (und mehreren Receivern): MPSC und MPMC
 *  - RMA BUF SPSC ist so implementiert, dass Daten gleichzeitig in den Speicher am Write-Pointer geschrieben und am Read-Pointer gelesen werden
 *  können. Umgesetzt durch shared lock und der Prämisse, dass Receiver nur Read- und Producer nur Write-Pointer verändern können. 
 *  - Implementierung ist also wait-free: Jede Operation wird in einer festgelegten Anzahl an Schritten beendet
 * 
 * - Designentscheidung für MPSC und MPMC:
 *    - Wird weiterhin ein Ringbuffer benutzt, kann immernoch gleichzeitig gelesen und geschrieben werden, allerdings müssen bei mehreren
 *    Receivern bzw. Producern diese auf die Beendigung des vorherigen Prozesses warten. 
 *      - Algorithmus ist nicht mehr lock-free/non-blocking, da beim Ausfall von Lockholdern kein anderer Prozess mehr Fortschritt machen kann
 *    
 *    - Alternative ist ein Algorithmus nach Michael und Scott, der eine non-blocking Queue beschreibt
 *      - Umsetzung durch atomatares Compare and Swap und Fetch and Op
 *      - Grundidee: 
 *        - Receiver:
 *          - Tausche mittels CAS atomar Head mit nächsten Knoten         
 *          - Bei Erfolg (Head == erster Knoten)
 *            - Receiver hat alleine Kontrolle über Knoten
 *          - Bei Misserfolg (Head != erster Knoten) 
 *            - Ein anderer Receiver war schneller
 *            - Wiederhole: Tausche mittels CAS atomar Head mit nächsten Knoten         
 *        - Producer:
 *          - Erzeuge Knoten mit neuem Inhalt
 *          - Tausche atomar momentanen Tail mit Adresse des neuen Knoten
 *          - Aktualisiere next im ehemaligen Tailknoten
 * 
 *   - Vorteil des Algorithmus:
 *    - Auf Producerseite: wait-free
 *      - Auf Receiverseite (MPMC): lock-free (kein waitfree, da ein anderer Receiver schneller sein kann)
 *      - Auf Receiverseite (MPSC): wait-free (da nur ein receiver)
     
 *  - Probleme für die MPI-Implementierung:
 *    - Jeder Prozess muss Anzahl an erstellten Knoten abspeichern und nicht mehr Knoten erstellen können als Channel-Kapazität
 *    - Jeder Producer muss sich merken, an welche Speicherstelle er neuen Knoten erzeugen kann
 *    - Anzahl an erstellten Knoten eines Producers muss von jedem Receiver verändert werden können
 *    - Der Speicher für die Knoten sollte bei Channelallokation festgelegt werden und nicht bei jedem channel_send() neu erzeugt werden
 *    - Wie können Knoten im verteilten Speicher miteinander verbunden werden? Können Adressen mehrfach vorkommen?
 * 
 * 
 * 
 * 
 */






/* 11.05.21
 *
 * - Scriptkompatiblen Throughput-Test geschrieben
 *    - Anzahl an Wiederholungen, Start, Ende, Channelkapazität, Modus, Anzahl Sender, Anzahl Receiver, Assertion per Kommandozeilen-Übergabe
 *    beim Programmstart
 *      - Wiederholungen: Gibt an, wie oft der Test wiederholt wird. Durchschnitt der Laufzeiten kann ausgegeben werden
 *      - Start und Ende:  Gibt für jede Wiederholung an, mit wie vielen Ints der Throughput-Test startet und wie viele Ints maximal gesendet 
 *      werden:
 *        - In jeder Wiederholung wird die momentane Anzahl an gesendeten Ints verdoppelt (i*=2). Beispielsweise 2, 4, 8, 16, 32, etc. Ints
 *      - Channelkapazität: Bestimmt die Kapazität des Channels
 *      - Modus:  Sollen PT2PT oder RMA Channels verwendet werden
 *      - Anzahl Sender/Receiver: Wird gebraucht, um mit Programmstart dynamisch den weiteren Typ (SPSC, MPSC, MPMC) des Channels zu bestimmen
 *      - Assertion: Senden und empfangen Sender und Empfänger die gleichen Zahlen in der selben Reihenfolge (SPSC), kann der Empfangspuffer
 *      auf Fehler überprüft werden
 * 
 *    - Problem mit MPMC BUF: 
 *      - Es kann garantiert werden, dass bei x Sendaufrufen jeder Sender x mal channel_send() aufruft aber es kann nicht garantiert werden
 *      dass jeder Receiver genau x mal channel_receive() erfolgreich aufrufen kann. 
 *      - Grund: Sender kann bei vollen Puffer eines Receivers diesen überspringen und zum nächsten gehen: Keine gleichmäßige Aufteilung der Daten
 * 
 * - Design und Implementierung von RMA SPSC und MPSC
 * 
 * - Als nächstes: 
 *    - RMA MPMC fertig implementieren
 *    - Halo Test implementieren
 *    - Testen
 * 
 * ssh stXXX@ai2srv.inf.uni-bayreuth.de -X
 * shh node19 -X
 * users
 * top
 * last
 * 
 */




/* 04.05.21
 *
 * Done:
 * - Design und Implementierung des PT2PT MPSC BUF Channels
 * - Design und Implementierung des PT2PT MPMC BUF Channels
 * 
 * - Für beide Channels gilt: Puffergröße bei Allokation bestimmt, wie viele Daten jeder(!) Sender für alle Receiver versenden kann
 * - Allokation mit Puffergröße 10:
 * 
 *  - MPSC: 
 *    - Jeder Sender s_i hat Puffergröße 10 für Receiver r_1
 *    - Receiver r_1 kann somit 10 * i Daten puffern
 * 
 *  - MPMC:
 *    - Jeder s_i hat Puffergröße 10 für alle Receiver r_j
 *    - Es gilt: Puffergröße += Anzahl_Receiver - Puffergröße % Anzahl_Receiver
 *    - Damit wird garantiert, dass die Puffergröße für jeden Sender s_i auf alle Receiver r_j aufgeteilt werden kann
 * 
 *    - Bei 2 Sender und 2 Receiver und Puffergröße 10:
 *      - Sender s_i hat Puffer 10, wobei 5 für r_1 und 5 für r_2 reserviert sind
 *      - Receiver r_j kann 10 * i Daten puffern
 * 
 *    - Bei 2 Sendern und 3 Receivern und Puffergröße 10:
 *      - 10 += 3 - 10 % 3 => 10 += 3 - 1 => 10 += 2 => 12
 *      - Sender s_i hat Puffer 12 wobei 4 für r_1, 4 für r_2 und 4 für r_3 reserviert sind
 *      - Receiver r_j kann 12 * i Daten puffern
 * 
 * - Alle PT2PT Channels implementiert
 * 
 * - Fehlersicherheit:
 *  - Channels vergleichen mittels MPI_Allreduce und MPI_BAND ihre Datengröße und Channelkapazität
 *    - Bei Problemen (beispielsweise global_data_size != data_size) gibt channel_alloc bei allen Prozessen des übergebenen
 *    Kommunikators NULL zurück
 *    - Weiterhin wird durch ein kollektives MPI_Allreduce am Ende der Channel Allokation überprüft, ob die Allokation für alle Prozesse
 *    erfolgreich war. Kommt es bei einem Prozess zu einem Fehler (malloc(), buffer_attach(), etc.) gibt die channel_alloc() Funktion für
 *    alle beteiligten Prozesse des Kommunikators NULL zurück
 * 
 * 
 * - Semantik von channel_send() bzw. channel_receive() im Modus BUF:
 *    - Jeweils blockierend, bis Daten gesendet oder empfangen werden können
 * - Semantik von channel_peek():
 *    - Gibt Kontrolle nach Berechnung der Puffergröße (Sender) bzw. Setzen der Flag, ob Nachricht(en) empfangen werden können (Receiver) 
 *    sofort zurück. Blockierung ist nicht abhängig von anderen Prozessen (wie bei channel_send() und channel_receive())
 *    => Simuliere nicht blockierendes Verhalten, indem nur bei erfolgreichem channel_peek() channel_send() bzw. channel_receive() aufgerufen wird 
 * 
 *  
 * - RMA SPSC BUF und SYNC implementiert
 * - Es fehlen noch MPSC und MPMC
 * - Design-Idee vorhanden, müssen größtenteils nur noch implementiert werden
 * 
 * - Tests/Benchmarks für Channels:
 *  - Throughput (evt. mit zufälliger Zeitspanne zwischen Send/Receive-Aufrufen; Ausfall eines Prozesses): 
 *      - Zeitmessung bis n Nachrichten der Länge k durch den Channel gesendet wurden bzw. empfangen wurden
 *      - Alle Channeltypen und modi (BUF, SYNC) können getestet werden
 *  - Halo Exchange (Datenaustausch mit jedem Nachbarn innerhalb einer Topologie): 
 *      - Eher nur BUF:
 *        - Jeder Prozess hat einen SPSC Channel zu seinem Nachbarn
 *        Oder
 *        - Jeder Prozess ist Consumer eines MPSC Channels, jeder Nachbar ist Producer des Channels
 *        - MPMC eher schlecht, da Producer an einem beliebigen Consumer Daten sendet
 * 
 *  - Weitere Testverfahren/Benchmarks?
 *  - Sollte Möglichkeit bieten, jeden Channeltypen gleichermaßen zu testen wobei Tests vermutlich nur für SYNC oder BUF geeignet ist
 * 
 *  - Kollektiven Aufrufe über Channels implementieren
 * 
 * 
 * 
 *  - Integration in Tasking-Framework als Ausblick
 * 
 * 
 * 
 * 
 */


/* 27.04.21
 *
 *
 * Design und Implementierung des PT2PT MPMC SYNC Channels 
 * 
 * Funktionsweise:
 * Sender sendet allen Receivern den Wunsch, Daten senden zu wollen (Send Request), wartet auf mindestens eine Antwort, sendet diesem die Daten 
 * und allen anderen eine Ablehnungsnachricht (Failure). 
 * 
 * Sender merkt sich, welchem Receiver er als letztes ein Send Request gesendet hat und welcher Receiver, der ein Send Request enthalten hat, 
 * die Failure Nachricht angenommen hat
 * 
 * Nachrichteninhalt des Send Requests: lokaler Nachrichtencounter (um Antworten zu älteren Send Requests unterscheiden zu können) 
 * 
 * Sender Request => Receiver Request
 * 
 * Genauer Ablauf:
 * Sender:
 * - Solange eine passende Antwortnachricht noch nicht empfangen wurde:
 *  - Prüfe, ob Receiver r letzte Failure Nachricht empfangen hat und noch kein Send Request empfangen hat
 *    - Falls ja:
 *      - Sende Send Request (MPI_Bsend) und aktualisiere requests_sent[receiver]
 *    
 * - Sende Receiver der ersten passenden Antwortnachricht Daten mittels MPI_Issend()
 * 
 * - Für alle Receiver r:
 *    Falls requests_sent[r] == 1:
 *      Sende Failure Nachricht mittels MPI_Issend()
 * 
 * - Nachrichtencounter++
 * - Warte auf Beendigung von der Datenübertragung mittels MPI_Wait()
 * 
 * 
 * Receiver:
 * channel_receive()
 * - While true:
 *    - Warte auf Empfang eines Send Requests
 *    - Antworte Sender des Send Requests
 *    - Warte auf eine Nachricht vom Sender des Send Requests
 *    - Falls Nachricht Failure Nachricht ist:
 *       - Empfange Failure Nachricht 
 *       - continue
 *    - Falls Nachricht Daten sind:
 *       - Empfange Daten
 *       - return
 * 
 * 
 * - Vorteil: Es sind pro Sender nie mehr als |Receiver| * 2 Nachrichten unterwegs
 *  => Keine unnötige Verstopfung des Netzwerks und der internen Empfangswarteschlangen
 * 
 * 
 */



















/* 20.04.21
 * 
 * Done:
 *  
 *  - Neue Übergabelogik für Festlegen der Channeltypen
 *    - Channel_alloc() erhält über den Übergabeparameter is_receiver von Typ int die Information, ob aufrufender Prozess innerhalb des zu 
 *    allokierenden Channels ein Receiver sein soll oder nicht
 *    - Dann erhält jeder Prozess mittels des kollektiven Aufrufes von MPI_Iallgather() das is_receiver Flag von jedem anderen Prozess
 *    - Aus den empfangenen Flags können neben der Anzahl an Sendern und Receivern auch zwei Arrays konstruiert werden, welche konsekutiv 
 *    den Rang/die Ränge der Sender und Receiver speichern (Wichtig für MPSC und MPMC Channels)
 * 
 *  - Vorteil:
 *    - Es wird sichergestellt, dass alle beteiligten Prozesse eines Kommunikators einen Channel mit einheitlicher Spezifikation erstellen,
 *    da jeder Prozess die gleiche Liste an Sender und Receiver Rängen erhält
 *      - Hätte bei Übergabe als Receiver Rank auch kollektiv sichergestellt werden müssen
 * 
 *    - Channeltyp wird implizit durch Anzahl an Sendern und Receivern bestimmt; Als Typ muss (im Moment) nur noch RMA oder PT2PT übergeben
 *    werden (um die Implementierung zu bestimmen)
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 */







/* 13.04.21
 *
 * Done:
 * 
 *  - Design und Implementierung des PT2PT MPSC SYNC Channels
 *    - Sender senden wie gewohnt über MPI_Ssend im synchronen Modus
 *    - Receiver iteriert mittels MPI_Iprobe über alle Procs (außer sich selbst) und empfängt Nachricht bei positiven flag mittels MPI_Recv
 * 
 * 
 * - Design des PT2PT MPSC BUF Channels
 *  - Grundlegende Frage: Wie ist Message Passing, Multiple Producer Single Consumer und Nachrichtenpuffer vereinbar?
 * 
 *  - Bedingung: Es gibt eine Channelkapazität k und alle Sender sollen Nachrichten senden können bis Kapazität beim Receiver erschöpft ist
 *    - Problem:
 *      - Sender müssen sich absprechen, da ansonsten nur überprüft werden kann, wie viele eigene Nachrichten versendet wurden, aber nicht
 *      wie viele Nachrichten von allen anderen Sendern versendet wurden
 *        => Kann leicht zum "Pufferüberlauf" beim Receiver führen
 * 
 *      - Will ein Sender eine Nachricht senden, müsste dieser das allen anderen Sendern mitteilen, welche darauf die lokal 
 *      gespeicherte Puffergröße um 1 dekrementieren und und bei Erfolg eine Bestätigung versenden
 *        => Extremer Overhead, Versenden der Daten findet nicht asynchron statt, da sich erst mit allen Sendern synchronisiert werden muss
 *        (Jeder Sender muss zuerst channel_send() aufrufen, bis ein Sender Daten schicken kann) 
 *   
 *      - Einhalten des Puffers über Tags ebenfalls schwierig
 *        - Sender senden Nachrichten mit Tag 0 bis max. Channelkapazität; Receiver empfängt Nachrichten von Tag 0 bis max. Channelkapazität
 *        - Problem: Nachrichten würden sich überschreiben, keine Garantie für Ankunft
 * 
 *    - Zusammenfassung: Reine Asynchronität nicht möglich, da Absprache mit allen Sendern nötig ODER Einhalten der Pufferkapazität nicht 
 *    garantiert
 * 
 *  - Bedingung: Es gibt eine Channelkapazität (k / |Sender|) für alle Sender und Channelkapazität k für Receiver; alle Sender sollen 
 *  Nachrichten senden können bis ihre lokale Kapazität erschöpft ist
 * 
 *    - Implementierung ansonsten wie bei SPSC BUF:
 *      - Sender sendet Nachrichten bis lokaler Puffer erschöpft
 *      - Receiver versendet bei Erhalt von Nachricht Acknowledgment-Nachricht zurück
 *      - Sender empfängt Acknowledgment-Nachrichten und aktualisiert lokalen Puffer
 * 
 *    - Garantiert das Einhalten des Puffers, da jeder Sender sich nur um seine lokal gespeicherte Pufferkapazität kümmern muss
 *    - Nachteil: Puffergröße unterscheiden sich zwischen Sendern und Receiver
 * 
 *    - Optimierungsidee:
 *      - Receiver speichert den Ursprungsrang der letzten k Nachrichten ab und aktualisiert regelmäßig die Pufferkapazität anhand des
 *      Verhältnisses zwischen empfangen Nachrichten und Nachrichten von Sender s:
 *      - Beispiel: 
 *        - Channel mit Kapazität k=100 und zwei Sendern mit lokaler Kapazität k_l = 100/2 = 50
 *        - Receiver empfängt 100 Nachrichten, wobei 90 Nachrichten von Sender s1 und 10 von Sender s2 stammen
 *        - Receiver sendet Sendern neue lokale Puffergröße 90 für s1 und 10 für s2
 *          - Sender s1 muss lediglich seine Puffergröße erhöhen und auf Bestätigung aller anderen Sender warten, um neue Kapazität zu verwenden
 *          - Sender s2 wartet, bis 91 Nachrichten bestätigt wurden (Acknowledgment-Nachrichten) und kann dann wieder senden
 * 
 *    - Vorteil: 
 *      - Eventuell bessere Performance (wenn Intervall für das Aktualisieren der lokalen Puffer nicht zu klein)
 *      - Unterschiedliche Puffergröße der Sender passt sich an Leistungsprofil an, übergebene Pufferkapazität bei Channelallokation kann
 *      notfalls als Untergrenze verwendet werden, um mit channel_alloc(..., capacity, ...) konform zu sein
 * 
 * 
 * 
 * 
 * 
 * 
 *  - Design des PT2PT MPMC SYNC Channels
 * 
 *    - Grundproblematik: Sender muss passenden Receiver finden; Kein Polling; Synchroner Datenaustausch; Daten sollen genau an einen Receiver
 * 
 *    - Idee:
 *      - Sender sendet Nachricht an alle Receiver (Wunsch, Daten senden zu wollen)
 *      - Receiver senden Nachricht zurück, warten auf Daten oder auf Abbruch Nachricht
 *      - Sender sendet an ersten Receiver Daten, an allen anderen eine Abbruch-Nachricht
 *  
 *    - Problem: 
 *      - Großer Nachrichtenoverhead je nach Anzahl an Receivern
 *      - Receiver muss vor dem Empfangen alle gepufferten Nachrichten verarbeiten
 * 
 *    - Alternative:
 *      - Nutze MPI_Issend und MPI_Cancel, wobei MPI_Cancel nicht sehr effizient ist
 *      - Sende an ersten Receiver, der Issend angenommen hat, Daten
 *      - Breche alle anderen Issend ab und sende Abbruch-Nachricht an Receiver, die trotzdem angenommen haben
 * 
 * 
 * 
 * 
 * - Design des PT2PT MPMC BUF Channels
 *  - Grundlegende Frage: Wie können Receiver einen gemeinsamen Puffer "simulieren" und Sender mittels Message Passing Daten asnychron an den
 *    Receiver senden, der als nächstes Daten empfangen will 
 * 
 *  - Nutze Idee von PT2PT MPSC BUF:
 *    - Jeder Receiver hat Kapazität (k / |Receiver|) = k_r, jeder Sender hat |Receiver| mal (k_r / |Sender|) = k_s_i
 *    - Jeder Receiver hat lokalen Puffer, jeder Sender hat einen Teil von jedem lokalen Puffer eines Receivers
 *    - Will ein Sender Daten verschicken, sendet er stets an den Receiver mit der höchsten, momentanen Pufferkapazität
 * 
 *  - Je mehr Sender/Receiver, desto größer ist der Overhead mit Message Passing, um gewünschte Semantik umzusetzen (falls möglich)
 * 
 * 
 * 
 * - Design des RMA MPSC SYNC Channels
 *    - Sender schreibt seinen Rang in ausgezeichneten Speicherbereich von Receiver, um zu signalisieren, dass dieser Senden will; iteriert über
 *    lokale Variable, bis diese vom Receiver aktualisiert wird
 *    - Receiver überprüft, ob jemand bereit ist zu schreiben; Bei Erfolg ändert Receiver lokale Variable des Senders und iteriert selbst über
 *    lokale Variable, bis Übertragen der Daten abgeschlossen ist
 *    - Sender sieht, dass lokale Variable aktualisiert wurde, sendet Daten zum Receiver und ändert lokale Variable des Receivers
 * 
 * 
 * Todo:
 * 
 *  - Optimierungsmöglichkeit für Send und Receive Aufrufe mit persistenten Parametern:
 *    - z.B. Acknowledgment-Nachrichten 
 *    - Nutze "persistente" send und receive Objekte mittels MPI_Recv_init oder MPI_Bsend_init
 *    - Einschränkung: Adresse des Send/Receive Puffers muss für jeden Aufruf gleich sein
 * 
 *  - Implementierung der genannten Designs (PT2PT MPSC BUF, PT2PT MPMC SYNC)
 * 
 * 
 *  Frage:
 *  
 *    - Buch / Paper / Informationen bezüglich Datenstrukturen mittels Message Passing
 *    - Wenig bis keine Informationen zu Algorithmen für die Implementierung von MPMC/MPSC Queues im verteilten Speicher (mittels Message Passing)
 *    - Die meisten Quellen behandeln gemeinsamen Speicher (Prozessoren mit shared memory, RMA, etc.)
 * 
 * Algorithmen für shared memory
 *
 * thomson leighton parallel and architectures
 * introduction to parallel computing
 * ananth 
 * scheduling in distributed memory multicomputers
 * Darlegen, Stand der Forschung darlegen (das ist bekannt, das nicht)
 * keywords: queue distributed message passing
 * 
 * 
 */




/*
 * - Design von RMA BUF (MPSC/MPMC)
 *  - Simple: Mittels verketten Lock und einem lokalen Puffer eines Prozesses
 * 

 * 
 */




/* 08.04.21
 * 
 * 
 * Done:
 *  - Implementierung von zwei Hilfsfunktionen für das Vergrößern und Verkleinern des Puffers, den MPI_Bsend() verwendet
 *  MPI_Bsend()
 * 
 * 
 *  - Überarbeiten aller bereits implementierten SPSC Channels (PT2PT und RMA)
 * 
 *  - Überarbeiten der Channelspezifikation:
 *    - Kein channel_{send_receive}_multiple() da keine einheitliche Semantik und teilweise umständlichere Implementierung
 * 
 *      - Kann manche Channels effizienter machen aber kaum Anwendungsfall, da dann Channel mit größerer Kapazität verwendet werden kann
 * 
 *      - Umständlich bei synchronen Channels, da nur gleiche Aufrufe funktionieren würden (channel_send() mit channel_receive() und 
 *        channel_send_multiple() mit channel_receive_multiple())
 * 
 *      - Bei synchronen RMA Channels: Anstatt eine müssen zwei Nachrichten versendet werden: Zuerst die Anzahl an Daten, dann die Daten 
 *        selbst; muss zusätzlichen Speicher allokieren
 * 
 *      - Würde bei gepufferten PT2PT Channels "nur" ein Wrapper für mehrmaliges Aufrufen von channel_{send|receive} sein
 * 
 * 
 *      - Bezüglich Semantik: 
 *        - Bei synchronen Channels wird entweder nichts oder alles gesendet
 *        - Bei gepufferten Channels wird so viel gesendet wie Platz im Puffer ist und so viel empfangen wie Daten im Puffer sind  
 * 
 * 
 *    - channel_peek nur für gepufferte Channels implementiert
 *      - Bei synchronen Channels keine einheitliche Semantik möglich: 
 *        - Sender erhält keine Information
 *        - Receiver (PT2PT) kann überprüfen ob eine(!) Nachricht empfangen werden kann
 *        - Receiver (RMA) kann je nach Synchronisationsart (Win_fence()) nicht überprüfen, ob ein Access Epoch angefangen wurde (keine 
 *          Informationen) oder durch umständiges Synchronisationskonstrukt (Win_lock, atomar lokale Variable ändern, Win_unlock, Win_fence, 
 *          etc.)
 * 
 *      - Bei gepufferten Channels sehr wohl eine einheitliche Semantik möglich:
 *        - Sender erhält Information über Anzahl von Nachrichten welche noch verschickt werden können (momentane Kapazität)
 *        - Receiver erhält Information über Anzahl an gepufferten Nachrichten
 * 
 * 
 * - TODO: 
 *  - SPSC Channels weiter ausformulieren 
 *  - MPSC Channels weiter implementieren
 * 
 * 
 */

/* 01.04.21
 *
 * - Channelspezifikation an die Arbeit von Herrn Dr. Prell angepasst:
 *    - channel_peek() gibt lediglich die Anzahl an empfangbare Nachrichten zurück
 *      - Dadurch geht Synchronität beim Peeken eines synchronen Channels nicht verloren, MPI unterstützt das Überprüfen des
 *        "Message_Count"
 *    - bei Aufruf des Senders gibt peek die Anzahl an Nachrichten zurück, die gesendet werden können
 *       - asynchroner, gepufferter Channel: Momentane Pufferkapazität
 *       - synchroner, ungepufferter Channel: Immer 1
 *   
 * - Überarbeiten der bereits erstellten Channels mit neuer Channelanforderungen:
 *  - MPI garantiert Reihenfolge der Nachrichten (non overtaking messages):
 *    - Werden zwei Nachrichten n1,n2 von Prozess s1 gesendet, und Prozess r1 empfängt die Nachrichten mit einem passenden Aufruf von MPI_Recv, 
 *      welcher beide Nachrichten empfangen kann (gleicher Tag, gleicher Comm, gleiche Datentyp und Länge), dann werden die Nachrichten in
 *      der Reihenfolge empfangen, in der sie versendet wurden: n1, n2
 *    - Gilt nicht, wenn innerhalb eines Prozess mehrere Threads mit gleichen Kontext (Kommunikator + Tag) verschicken
 * 
 *    - Wird MPI_ANY_SOURCE verwendet und ein dritter Prozess s2 versendet eine Nachricht n3, kann keine Aussage darüber getroffen werden, in
 *      welcher Reihenfolge die Nachrichten nun ankommen (außer dass n1 vor n2 ankommt): n1, n3, n2 oder n3, n1, n2 ...
 *      => Nachvollziehbar, Zeitpunkt der Ankunft der Nachricht hat Einfluss auf Reihenfolge
 * 
 *  - MPI garantiert per Spezifikation keine Fairness bezüglich der Kommunikationsabhandlung bei MPI_ANY_SOURCE
 * 
 *  - Anforderungen:
 *    - Reihenfolge:
 *      - Muss zumindest innerhalb eines Senders eingehalten werden
 *      - Da MPI die Reihenfolge der Nachrichten bezüglich eines Senders einhält, besteht hier kein Handlungsbedarf
 *        => Die Bedingungen für das Einhalten der Reihenfolge sollten dokumentiert werden (Gleicher Channelkontext, nur bei einem Sender)
 *        => SPSC Channels können für das Verschicken von Daten, welche auf das Ankommen in der richtigen Reihenfolge angewiesen sind,  
 *           verwendet werden
 * 
 *    - Fairness: 
 *      - Muss durch bestimmte Mechanismen implementiert werden
 *        - Message Passing: 
 *          - Momentan: Empfänger loopt durch alle Sender durch
 *          - Eventuell besserer Mechanismus?
 *        - Remote Memory Access: 
 *          - Mechanismus für gepufferte Channels: Sender hängen bei Nachrichtenversand ihren Rang atomar in verteilte Liste (Locksemantik),
 *          Prozess mit Rang im Zielprozess ist sofort dran, weckt nach Nachrichtenversand den nächsten in der Liste auf
 *          - Kann auch für synchrone Channels verwendet werden
 *          => Garantiert Fairness, da jeder Senderrang genau einmal in der Liste vorkommen kann 
 * 
 *  
 *  - Ausformulieren der SPSC Channels
 *  - Tipp für Gliederung:
 * 
 * 
 * 
 * 
 *  - Todo:
 *    - Anforderungen Quelle
 *    - Channels bezüglich Fairness aktualisieren
 *    - Ausformulieren
 *    - Weitere Channels implementieren und testen
 * 
 */



/* 24.03.21
 *
 *  - Design und Implementierung des pt2pt synchronen Multi-Producer-Single-Consumer Channel
 *  - Grundlegendes Problem: MPI_Recv(..., MPI_ANY_SOURCE, ...) ist nicht fair bezüglich der Reihenfolge der Prozesse 
 *      - Um Fairness zu garantieren, müsste Consumerprozess durch alle Sender gehen und kontrollieren, welcher Sender am längsten
 *        wartet
 *      - Weiteres Problem: Laufzeit der Senderkontrolle skaliert linear mit der Anzahl an Sendern
 *      - Möglichkeit: Nutze MPI_ANY_SOURCE und mache Fairness von MPI Implementierung abhängig
 *          => Tradeoff bezüglich Effizienz und Fairness
 * 
 *  - (Unfairer) Algorithmus:
 *  - Sender:
 *    - Sende Nachricht synchron (MPI_Ssend(...));
 * 
 *  - Receiver:
 *    - Empfange Nachricht blockierend (MPI_Recv(...));
 * 
 *  - Bewertung:
 *    + Effizienz alleine abhängig von MPI Implementierung, Laufzeit skaliert nicht mit Anzahl an Prozessen
 *    - Kann keine Fairness garantieren
 * 
 *  - Fairer Algorithmus:
 *  - Sender:
 *    - Sende Nachricht synchron;
 * 
 *  - Receiver:
 *    - Für jeden Sender s_i (starte ab Rang des letzten Senders last_rank):
 *      - Falls eine Nachricht von betrachtetem Sender s_i gesendet wurde:
 *        - Empfange Nachricht blockierend von s_i;
 *        - Speichere Rang lokal in last_rank;
 * 
 *  - Bewertung?
 *      - Laufzeit skaliert mit Anzahl an Prozessen
 *      + Garantiert Fairness
 * 
 *  - Dokumentation (abhängig von MPI Implementierung fair oder nicht fair)
 * 
 * 
 * 
 * 
 * 
 * 
 * - Design und Implementierung des pt2pt synchronen, gepufferten Multi-Producer-Single-Consumer Channel
 * - Zu lösendes Problem: Konsens über momentane Puffergröße
 * - Wie kann garantiert werden, dass Nachrichten nur bei ausreichender Puffergröße gesendet werden und ungültige Nachrichten ignoriert
 * werden?
 * 
 * 
 *  - Receiver benachrichtigt nach Erhalt alle Sender bezüglich neuer Puffergröße
 *  - Probleme: 
 *    - Laufzeit skaliert mit Anzahl Sender, vermeidbar? 
 *    - Receiver kann nur bei Aufruf von channel_receive benachrichtigen, da kein Threadpolling
 * 
 * - Idee:
 *  - Receiver benachrichtigt nach Erhalt den Sender der Nachricht. Dieser benachrichtigt alle anderen Sender
 *  - Vorteil: Benachrichtigung wird auf einen von mehreren Producern ausgelagert, Receiver kann sich auf konsumieren konzentrieren
 * 
 *  - Sender benachrichtigt nach Senden der Nachricht ebenfalls alle anderen Sender
 *  - Erhofftes Ziel: Schnelles Updaten der momentanen Puffergröße
 *  - Aber: Problematik über das Ignorieren ungültiger Nachrichten
 * 
 *  - Channels müssen eine ausreichende Fehlertoleranz besitzen:
 *      - Puffer für ausgehende Nachrichten pro Prozess wenn MPI_Bsend verwendet wird: 
 *        => Kann von Nutzer spezifiziert werden: MPI_Buffer_{attach|detach}
 *      - Puffer für eingehende Nachrichten pro Prozess abhängig von MPI Laufzeitumgebung
 *        => System und Implementierungsabhängig 
 *  => Gestalte Channels so, dass Verlust einer Nachricht Channel nicht ungültig macht
 * 
 *  Problem: Andere Aufrufe von MPI_Bsend und channel_send() nutzen ebenfalls diesen Puffer
 *  => Überlauf führt potentiell zum Verlust von ausgehenden Nachrichten anderer Channels
 *  - Überlegung: Sender muss zusätzlichen Pufferspeicher allokieren:
 *    -> Sender kann theoretisch die komplette Pufferkapazität des Channels an Nachrichten senden: Puffergröße * Größe eines Datums
 *    -> Sender benachrichtigt andere Sender bezüglich neuer Pufferkapazität: Anzahl Sender * Nachricht neue Puffergröße
 *      -> Maximale Anzahl ausgehender Nachrichten: Pufferkapazität * Datengröße + Anzahl Sender * Nachricht neue Puffergröße 
 * 
 *  Algorithmus:
 *  Sender:
 *  - Empfange neue Puffergröße von Sender und/oder Empfänger;
 *  - Falls Puffergröße ok:
 *    - Sende Nachricht an Receiver;
 *    - Sende Nachricht an alle Sender;
 * 
 *  - Receiver:
 *  - While Nachricht in Puffer:
 *    - Entnehme Nachricht;
 *    - Speichere Nachricht in lokalen Puffer;
 *    - Speichere Nachrichtenanzahl lokal;
 * 
 *  - Sende Größe des lokalen Puffers an Sender der Nachricht;
 *  - Entnehme Nachricht;
 * 
 * 
 * - MPI Pufferüberlauf: Konsequenz?
 * 
 *  TODO: 
 *  - Channels weiterimplementieren
 */






/* 18.03.21
 *
 * - Wegen Prüfungsvorbereitung leider nicht so viel geschafft
 * - Letzte Prüfung nächste Woche, dann hoffentlich besser
 * 
 * - Gliederung etwas überarbeitet
 *    - Entwicklung eines parallelen Laufzeitsystems in welchem Channels als Kommunikationsmittel für Threads im verteilten 
 *      Adressraum verwendet werden. 
 *    - Durch Portabilität der MPI-Implementierung folgt Portabilität des Laufzeitsystems
 *    - Außerdem: Effiziente, intuitive und relativ benutzerfreundliche Implementierung von Message Passing durch Channels im Vergleich zu 
 *    MPI
 *  - Grundlagenkapitel überarbeiten: Welche Kapitel sind relevant für das Verständnis des Designs und der Implementierung der Channels?
 * 
 * 
 * - Literaturrecherche betrieben
 * - Teilweise State of the Art Implementierungen im gemeinsamen Adressraum für FIFO Queues, die mit und ohne Lock arbeiten
 *    - Bisherige Implementierungen anpassen bzw. optimieren
 * - Literaturrecherche bezüglich effizienter Algorithmen im Message Passing
 *    - "Distributed Algorithms for Message-Passing Systems" Michel Raynal
 *    - Einlesen 
 * 
 * - Frage stellen: Welche Informationen sind für die eigene Leistung besonderns wichtig, um das zu verstehen?
 * - Wo habe ich das größte geistige Wissen hineingesteckt? Welche Probleme wurden gelöst?
 * - Hier Schwerpunkt auf Grundlagen legen
 * - Nicht einfach MPI Tutorial beschreiben, irgendwas wird man scho brauchen
 * - Von "hinten nach vorne" denken
 * - Wie realisiere ich die Channels? Wo ist hier die besondere Schwierigkeit?
 * - Synchronisation, Queue, Nonblocking?
 * - Es gibt MPI als Standard ... dann herausarbeiten, welche Punkte für das weitere Verständnis der Arbeit besonders wichtig sind
 *
 * - Mit späteren Kapitel anfangen?  
 *
 * 
 * - Grundlagenkapitel schreiben
 * - Channelimplementierung fortsetzen
 * 
 * 
 * 
 * Monat 6: Fertig schreiben
 * Monat 5: Testen + Fertig schreiben
 * Monat 4: Implementierung fertig machen + Recherche + Grundlagenkapitel schreiben
 * Monat 3: Implementierung + Recherche + Grundlagenkapitel schreiben
 * Monat 2:
 * Monat 1:
 * 
 * 
 */





/*
 *  18.02.2021
 * 
 *   Hauptsächlich Literaturrecherche, Einlesen in grundlegende Definitionen, etc. 
 *    - Shared Objects and synchronization
 *    - Producer-Consumer/Reader-Writer problem
 *    - Critical Sections and Deadlocks
 *    - Concurrency and correctness
 * 
 * 
 *   Non-blocking Algorithm (Definitions from "The Art of Multiprocessor Programming"):
 *    - Definition: An algorithm is non blocking if the delay or failure of one or more threads cannot prevent other threads from 
 *    making progress
 *    - In contrast to locks (mutex, semaphore, etc.): Does a lock owning process delay, progress can no longer be guaranteed
 *    - Rely on atomic operations (fetch and op, compare and swap (CAS), etc.)
 *    - Used in shared memory models:
 *        - Reference to thesis: Non blocking algorithms in shared memory systems can be used in RMA supported systems (both support atomic ops)
 *        - In fact the only key difference between shared memory and rma is the duration of operations (and address space of course)
 * 
 *  Obstruction-free (frei von Hinternissen): Guarantees progress for any thread that executes in isolation
 *    - Does not guarantee progress if two or more threads are running
 *    - If another thread interferes, some kind of "rollback" must be done (e.g. load atomically, calculate locally, finish with a CAS,
 *        ,CAS fails, retry)
 *        ,CAS succeds, return)
 * 
 *  Lock-free: Guarantees that at least one thread is making progress in a finite number of steps
 *    - Minimal progress is guaranteed because the system as a whole makes progress
 *    - But: Lock-free does not guarantee no starvation or fairness
 *    - Example:
 *        - Spinlock is not "non-blocking": One thead can have the lock and be blocked by the scheduler
 *        - w/o lock but CAS: A failing CAS from one thread exactly means that another process IS making progress
 * 
 *  Wait-free: Guarantees that at all time all threads are making progress in a finite number of steps
 *    - Maximum progress is guaranteed because every thread makes progress
 *    - "Truly" nonblocking since every thread is making progress
 *    - Might be difficult to implement efficently
 *    - Example:
 *        - Single FIFO Queue with atomic enqueue/dequeue operations, no lock needs to coordinate access:
 *          - Enqueue: Returns true if there is space, false if queue is full
 *          - Dequeue: Returns true if queue is full, false if queue is empty
 *        - There is progress done at all time
 * 
 *  There are already lock-free/non-blocking algorithms for spsc, mpsc and mpmc
 *    - https://www.linuxjournal.com/content/lock-free-multi-producer-multi-consumer-queue-ring-buffer
 *    - https://github.com/utaal/spsc-bip-buffer
 *    - https://ferrous-systems.com/blog/lock-free-ring-buffer/
 *    - https://github.com/rmind/ringbuf
 * 
 * 
 *  Problem:
 *    - Login für Cluster funktioniert nicht, Probleme beim Verbinden/Einloggen über SSH
 */










 /* 11.02.2021
  *
  * Done: 
  * 
  *     - Alle SPSC Channels fertig implementiert      
  * 
  *     - Synchroner SPSC RMA Channel:
  *         - Paramter für Buffergröße bei Channelallokation sollte 0 (ungebuffert, synchron) oder >0 (gepufferet, asynchron) sein 
  *         - Problemstellung: Wie viel Speicher muss allokiert werden, wenn mehrere Items geschickt werden?
  * 
  *         - Lösung: Nutze Buffergröße-Parameter n in channel_alloc(size_t size, int n, MPI_Comm comm, int target_rank)
  *             - n > 0:    Channel ist gepuffert und asynchron
  *             - n = 0|-1: Channel ist ungepuffert und synchron, es kann pro Send/Receive Aufruf maximal 1 Element geschickt werden
  *             - n < -1:   Channel ist ungepuffert und synchron, es kann pro Send/Receive Aufruf maxilam abs(n) Elemente geschickt werden
  *         - Vorteil: Es bleibt bei einem Parameter, mit welchem auch der Channeltyp bestimmt werden kann
  *   
  *         - Warum kein dynamisches Anfügen des Speichers an das Window Objekt? 
  *             - Braucht mehrere Nachrichten (P sendet Anzahl zu sendender Elemente, C allokiert Speicher und sendet neue Pufferadresse, 
  *                 P sendet an neue Pufferadresse) 
  *             - ... und (relativ) langsame Aufrufe (Allokieren von Speicher, Anfügen und Abhängen an das Window Objekt)
  * 
  *         - Alternativ oder ergänzend: Buffergröße wird bei channel_allocation mit n = 0 initialisiert; sobald mehr als ein Element 
  *         gesendet oder empfangen werden soll (send/recv multiple), wird der Windowspeicher auf die benötigte Größe erhöht 
  *           - Erfordert aber aufwendige Kommunikation zwischen Producer und Consumer
  * 
  *         - channel_peek(...) wurde verworfen, da eine Implementierung bei vielen Channeltypen starke Auswirkungen auf Effizienz hat
  *             - SPSPC RMA SYNC: Damit Synchronität erhalten bleibt, braucht es nach einem channel_send() eine Nachricht vom Consumer 
  *             zum Producer, ob dieser die Nachricht von channel_send() empfangen hat (channel_receive()) oder gepeekt hat (channel_peek())
  *              
  *         - channel_cansend und channel_canrecv wäre bei einem synchronen RMA Channel obsolet:
  *           - Wenn cansend/canrecv zurückgeben soll, ob aufgrund interner Restriktionen (Buffergröße, etc.) gesendet/emfpangen werden kann
  *             - synchrone Channel können immer empfangen und senden (keine internen Hindernisse wie voller/leerer Buffer)
  *           - Wenn cansend/canrecv zurückgeben soll, ob Consumer bzw. Producer bereits passende Funktion aufgerufen hat:
  *             - es kann nicht überprüft werden kann, ob ein Consumer bzw. Producer einen Access Epoch bzw. eine RMA Operation gestartet hat
  * 
  * 
  * Designvorschlag:
  *   - Synchroner MPSC RMA Channel:
  *         - Kein Synchronisation mittels MPI_Fence, da Aufruf kollektiv über alle Procs des Kommunikators geschehen muss und damit
  *         wie eine Barriere wirkt: Es sollten nur ein Consumer und ein Producer synchronisiert werden
  * 
  *         - Entweder Synchronisation mittels Post-Start-Complete-Wait: Barriere wird verhindert und sowohl Consumer als auch mindestens 
  *         einen Producer nehmen an Synchronisation teil
  *           - Consumer öffnet Access Epoch (MPI_Win_post), wartet, bis erster Producer mittels MPI_Win_{start|complete} das lokale 
  *           Window geupdated hat und beendet Access Epoch (MPI_Win_wait)
  *           - Potentielles Problem: Consumer könnte endlos im Access Epoch verweilen, wenn ein Producer permanent Senden will und damit
  *           MPI_win_start aufruft; Consumer beendet erst Access Epoch, wenn alle RMA Operationen beendet sind
  * 
  *         - Oder passiver Lock
  *           - Consumer spinnt über lokale Variable, bis diese von einem anderen Producer verändert wird
  *           - Producer trägt sicher über fetch_and_replace ein; ist Producer erster, sendet dieser Daten und weckt Consumer auf
  *           - Hier: Kein Problem, dass Consumer ewig schläft bzw. synchronisiert bleibt
  * 
  * Todo:
  *   - Algorithmus für Lock fertig implementieren   
  *   - Erste Tests am Cluster
  *   - Erste MPSC Channels designen und implementieren
  */













/* 04.02.2021
 * 
 * 
 * Cluster: Verschiedene filesysteme an linuxclustern
 * 
 * 
 * 
 * Done:
 *      - Gepufferten SPSC RMA Channel fertig implementiert
 *          - Neue Implementierung mit verteilter Kommunikation (Receiver sendet Read-Index, Sender sendet Write-Index) ist
 *          etwas effizienter
 * 
 *      - Design eines fairen und intelligenten Lock-Algorithmus für RMA Multiple Producer:
 *          - Bei mehreren Producern/Consumern gibt es folgende Probleme:
 *              - Prozesse müssen "atomar" wissen, wie viele Items im Puffer sind, wenn der Prozess selbst den Lock hat
 *              (die zukünftige Pufferkapazität)
 *              - Es gibt keine (oder eine nicht bekannte, unfaire) Reihenfolge (MPI_Win_lock()), in welcher Prozesse ein Window sperren
 *              - Ein Producer oder Consumer kann leer ausgehen und damit unnötig und iterativ den Lock sperren 
 * 
 *          - Implementierung des Locks als verkettete Liste
 *          - Jeder Prozess hält in seinem Window die Attribute nextRank und blocked, Prozess 0 zusätzlich lastRank
 * 
 *          - Grobe Ablaufskizze es Locks:
 *          - Ein Zielprozess (Rang 0) hält den Rang des Prozesses (mem[last_rank]), der als letztes versucht hat, den Lock zu bekommen
 *          (Anfangswert -1)
 *          - Will ein Prozess den Lock bekommen, holt er sich den letzten Rang von Prozess 0 und ersetzt diesen durch den eigenen Rang
 *          (atomare fetch_and_op Funktion, garantiert Fairness durch verkettete Liste)
 *          - Falls der geholte Rang -1 ist, hat der momentante Prozess den Lock
 *          - Andernfalls fügt der momentane Prozess sich beim Prozess des geholten Rangs hinzu [mem[next_rank]] und wartet bis er aufgeweckt
 *          wird
 *          - Gibt ein Prozess den Lock zurück, muss er lediglich den Prozess aufwecken, der sich lokal in mem[next_rank] eingetragen hat
 *          (oder einen Prozess aus der Liste der blockierten Receiver)
 * 
 *      - Durch atomare Operationen wird sichergestellt, dass jeder Prozess an die Reihe kommt
 *      - Jeder Prozess speichert bei Lockantrag die (nachdem die Send/Receive Operation ausgeführt wurde) zukünftige Kapazität des Puffers
 *      (Prozess P1 holt sich mom. Kapazität, addiert Veränderung, trägt diese bei sich ein, Prozess P2 ...)
 *      - Receiver werden bei Puffergröße < count blockiert und bei ausreichender Größe aufgeweckt
 *         => Wurde Puffer genügend gefüllt, kommt ein Receiverprozess als nächstes dran
 *      - Sender blockieren bei vollem Puffer nicht
 *          => hatte verschiedene Ideen (Liste für blockierte Sender und Receiver, etc.), konnten aber entweder zu einem Deadlock führen
 *          oder waren aufgrund von extremen Messaging Overhead zu ineffizient
 * 
 *      - Vorteil: Aufgrund der Nebenläufigkeit, kann ein Prozess den Lock besitzen während weitere Prozesse parallel berechnen können, ob
 *      genügend Kapazität vorhanden ist und falls nicht, den Lockantrag wieder zurückgeben
 * 
 *      - Implementierung steht noch an
 *      
 *      - Im Gegensatz dazu: Für Umsetzung eines Locks mittels Message Passing (pt2pt) für Multiple Producer ist eine Art periodisches 
 *      Polling oder Nutzen eines Slave-Prozess nötig
 * 
 *  
 *      - Designentscheidung bezüglich des synchronen RMA SPSC Channels:
 *          - Verschiedene Implementierungsmöglichkeiten eines synchronen Channels bezüglich der Speicherverwaltung
 *          - Für synchrones Send/Receive braucht es trotzdem einen Speicher, der zur Zwischenspeicherung verwendet wird
 *          - Wie und wie groß wird der Speicher angelegt? 
 * 
 *              1. Channel hat einen vordefinierten, maximalen Speicher, der global gesetzt wird
 *                  - Vorteil: 
 *                      - Schnellste Variante, da nur eine Put-Operation nötig ist
 *                  - Nachteil:
 *                      - Problem bei Nachrichten, die größer als der reservierte Speicher sind
 *                      - Jeder Channel muss dementsprechend viel Speicher allokieren, obwohl dieser eventuell nicht gebraucht wird
 *                  - Kann mit einer Channel_init(...) Funktion gesetzt werden
 * 
 *              2. Channel hat einen vordefinierten, maximalen Speicher, der für jede Channelallokation gesetzt wird
 *                  - Vorteil: 
 *                      - Schnellste Variante, da nur eine Put-Operation nötig ist
 *                  - Nachteil:
 *                      - Problem bei Nachrichten, die größer als der reservierte Speicher sind
 *                      - Aufruf braucht einen weiteren Parameter für die Größe des Speichers:
 *                          - Unschön, da andere Channels das nicht brauchen: 
 *                              => channel_create(capacity, communicator, target_rank, max_message_size)
 * 
 *              3. Channel besitzt ein dynamisches Window ohne vorallokierten Speicher; als Speicher wird der übergebene Puffer verwendet
 *                  - Folgender Ablauf bei Channel_{send|receive}_multiple(...):
 *                      - Sender verschickt zuerst die Anzahl an zu sendenden Items
 *                      - Empfänger fügt Speicher mit der benötigten Größe an das Window (MPI_Win_attach(...)) 
 *                      - Empfänger sendet neue Startadresse an Sender
 *                      - Sender verschickt die Daten an die empfangene Adresse
 *                  - Vorteil: 
 *                      - Channel verbraucht zu jedem Zeitpunkt nur so viel Speicher wie nötig
 *                      - Copy-Behaviour wird auf das Minimum reduziert, da als Empfangspuffer der übergebene Puffer genutzt wird
 *                  - Nachteil: 
 *                      - Im Vergleich deutlich langsamer, da mehrere Kommunikationsaufrufe nötig sind
 *
 *              - 3 Möglichkeiten für jeden Channel:
 *                  - Fester, maximaler Speicher 
 *                  - Variabler, maximaler Speicher aber zusätzlicher Parameter
 *                  - Dynamischer, tatsächlicher Speicher durch Anhängen der passenden Speichergröße an das Window Object
 *                  MPI_Win_{attach|detach}, aber erhöhter Kommunikationsoverhead
 *  
 */















/* 28.01.2021
 *
 *  Done:
 *      - Zugang zum Cluster AI2 geholt
 *      - Zugang nicht genutzt, da finale RMA Implementierung noch Probleme verursacht
 * 
 *      - RMA SPSC Buffered Channel umstrukturiert:
 *          - Davor: Sender holt sich Indices, prüft diese, sendet Daten, aktualisiert Indices, sendet neue Indices zum Receiver
 *          - Jetzt wurde Workload auf Sender und Receiver aufgeteilt:
 *              - Receiver überprüft lokale Indices, liest Daten, aktualisiert Indices, sendet neue Indices zum Sender
 *              - Sender überprüft lokale Indices, aktualisiert Indices, sendet Daten
 *          - Konnte noch keine Zeitmessung ausführen, da noch etwas verbuggt, hoffe aber auf eine gleichmäßigere Zeitmessung.
 * 
 *          - Mögliche Verbesserung: Designe intelligenten Lock, der eine Liste von Prozessen enthält, die auf Buffer zugreifen wollen.
 *          Lock selbst soll aber Indices des Buffers enthalten, damit ein "sinnloses" Locken eines leeren oder vollen Buffers
 *          verhindert werden kann (analog Receiver):
 *              - Sender beantragt Lock (get_access(ptr_lock, count_of_sending_items))
 *              - Lock überprüft Indices
 *                  - Falls Indices einen vollen Buffer propagieren, stelle Sender hinter einem Receiver-Lockantrag, damit Buffer leer genug wird
 *                  - Falls kein Receiver-Lockantrag vorhanden ist, blockieren Prozess bis Receiver-Lockantrag in Liste kommt
 *                  - Anderfalls füge Sender an letzter Position in Lockliste hinzu
 * 
 * 
 * 
 *      - Konzept RMA SPSC Sync:
 *          - Synchronisation über Win_fence (Aufruf startet oder beendet RMA Access Epoch, erzwingt das Ende aller RMA-Operationen): 
 *              - Bei Aufruf von Send/Receive kollektiver MPI_Win_fence Aufruf zu Beginn
 *              - Sender beginnt Daten zu senden und endet mit 2x MPI_Win_fence
 *              - Receiver beginnt mit MPI_Win_fence, holt sich Daten, endet mit MPI_Win_fence
 * 
 *      - Konzept PT2PT MPSC Sync:
 *          - Prinzipiell recht einfach:
 *              - Receiver ruft MPI_Recv mit MPI_ANY_SOURCE auf dem Communicator des Channels auf
 *              - Sender rufen MPI_Ssend mit ch->target_rank auf dem Communicator des Channels auf 
 *          - Probleme:
 *              - Sicherstellen, dass die Reihenfolge der Aufrufe von MPI_Bsend beim Receiver berücksichtigt wird (Fairness)
 *              - Gleiches Problem wie bei SPSC: channel_peek() würde Synchronität zerstören, da bei MPI eine Nachricht angenommen
 *              werden muss, um den Inhalt zu bekommen (gilt nicht für Größe der Nachricht)
 * 
 *      - Konzept PT2PT MPSC Buf:
 *          - Problem: Sender müssen vor jedem channel_send die momentane freie Kapazität des Buffers wissen
 *              - Nach jedem MPI_Recv sendet der Receiver die neue Buffergröße an alle Sender (MPI_Bsend, MPI_Broadcast)
 *              - Könnte bei mehreren Producern Probleme verursachen: 
 *                  - Sender warten auf Leeren des vollen Buffers, bekommen neue Buffergröße, senden alle gleichzeitig Daten
 *              - Alternativ: Für jedes Senden muss ein Sender eine Art Lock bekommen, erhöht aber Overhead.
 * 
 *      - Konzept PT2PT MPMC
 *          - Probleme: 
 *              - Sowohl Sender als auch Receiver müssen die momentane freie Kapazität des Buffers wissen
 *              - Inkonsistenzen wenn z.B. Receiver denkt, dass mom. Buffergröße 1 ist und Daten annimmt, 
 *              obwohl Buffergröße eigentlich bereits 0 ist => Erfordert Lockmechanismus bezüglich Buffergröße
 *              - Sender muss entscheiden, zu welchem Producer gesendet wird 
 * 
 *      - Vermutlich einfacher bei RMA MPMC, da ein Receiver R1 den Buffer halten kann und alle restlichen Sender und Receiver darauf 
 *      zugreifen können ohne aktive Teilnahme von Receiver R1
 * 
 * 
 *      - Da Peek mehr Effizienzprobleme schafft als Anwendungsprobleme löst, steht die Frage, ob es weggelassen werden soll
 * 
 * 
 *  Todo:
 *      - RMA SPSC BUF bugfrei machen und auf Cluster testen, Laufzeiten einer pt2pt Implementierung gegenüberstellen
 *      - Literaturrecherche bezüglicher effizienter und fairer Implementierungen von MPSC und MPMC
 *          - Netzwerkprotokolle
 *          - Verteilung des Buffers?
 *      - Auf Basis der Literaturrecherche Konzepte der restlichen Channels "verfeinern"
 *      - Ergebnisse der Recherche aufschreiben
 */








/* 21.01.2021
 *  Done:
 *      - Test implementiert, der essentielle Kombinationen von Channeloperationen überdeckt
 *          - Leerer Channel bzw. Channelerzeugung und Operationen mit ungültige Parameter 
 *          - Send/Receive, Send/Receive Multiple, Cansend/Canrecv und Peek mit verschiedenen Buffergrößen
 * 
 *      - Error/Warning/Debug-Ausgabe überarbeitet
 *          - Können abgeschalten werden
 *          - Anstatt Error werden Warnings geworfen, wenn Aufruf prinzipiell kein Problem verursacht (Sender ruft receive in SPSC Channel, 
 *          bei MPI_Bsend wurde noch kein Buffer angehängt)
 * 
 *      - Alle SPSC PT2PT Channel implementiert
 * 
 * Todo:
 *      - RMA SPSC Channel fertig implementieren    
 *  
 *      - Da nun alle Funktionen der Channel stehen, können Konzepte für andere Channeltypen erarbeitet werden
 *          - Problem bei MPSC: Wird Buffer bei Consumer gehalten, muss Consumer bei Receive jedem Producer eine Bestätigung schicken.
 *          - Andere Producer müssen sich bei Ablage in den Buffer benachrichtigen
 *          => Eventuell hoher Overhead (durch Nachrichtenaustausch) für Multi-Producer/Consumer Semantik  
 *      
 *      - Groben Aufbau der Arbeit überlegen
 *          - Channels in GO, Producer-Consumer Problematik, MPI
 *          - Design der Channeltypen
 *          - Implementierung der Channeltypen
 *          - Laufzeitmessungen und Fazit
 * 
 *      - Finaler Name: Design und Implementierung von Channels zur Interprozesskommunikation im verteilten Adressraum
 */

/*
 *  Kolloquium:
 *  - 20 min Vortrag, angehende Diskussion mit Fragen
 * 
 */

/* 14.01.2021
 *  Done:
 *      - Programmcode neu strukturiert und modularisiert: Jeder Channeltyp ist in eine eigene Datei ausgekapselt und
 *      kann ausgetauscht werden. Hinzufügen neuer Channelimplementierung wurde vereinfacht. 
 * 
 *      - Festlegung auf nur zwei Channeltypen: Synchronisierend und nicht synchronisierend aber gepuffert.
 * 
 *      - Extensive Fehlerüberprüfung: Channel und data-Buffer nicht NULL, Kommunikator und Größe stimmen, 
 *      aufrufender Prozess ist möglicher Sender (Channel_send) analog Empfänger, count 1 oder größer, 
 *      Zielprozess bei Erstellen eines Channels ist im Kommunikator vorhanden, etc. (Prinzipiell kann jeder Prozess jede
 *      Funktion aufrufen)
 * 
 *      - Hinzufügen neuer Funktionen (damit verschiedene Channel(implementierungen) effizienter werden)
 *          => send_multiple und receive_multiple; zeigt bessere Laufzeit, da nur eine Synchronisation nötig ist
 *          => can_send, can_receive und peek; etwas problematisch, da z.B. 
 *              _> bei synchronisierendem Senden ein peek dem Sender der Nachricht die Kontrolle zurückgibt 
 *              (kann umgegangen werden, erfordert aber ineffizientes Senden von ack)
 *              _> einen zusätzlichen Buffer benötigt, da bei MPI Nachrichten immer komplett "received" werden müssen (Overhead
 *              vor allem bei send_multiple und peek (Lange Nachricht wird gesendet, ein Element wird gepeekt, die ganze Nachricht 
 *              muss zwischengespeichert werden)
 *          
 *      - Dokumentation mittels Doxygen 
 * 
 *      - Laufzeitmessungen von MPI_Send vs RMA vs RMA Shared Memory; Shared Memory zeigt bessere Laufzeit als MPI_Send bei
 *      mittelgroße Datengröße (~8000 Bytes bis 320000 Bytes)
 * 
 *      - Paper "Notified Access" schlecht umsetzbar, da dies Portabilität von MPI zerstört. 
 *          => Ziel war es, ein Prozess über Ende einer Nachricht zu benachrichtigen ohne Teilnahme des anderen
 *          Prozess (Vorteil im Producer/Consumer Problem)
 *          => MPI ist standartmäßig dafür nicht vorgesehen; alles läuft über Nachrichten
 *          => Implementierung durch RMA-Hardwaresupport oder Shared Memory Modell, etc.
 * 
 * 
 * Probleme:
 *      - Suche nach Möglichkeit, Laufzeitmessungen der verschiedenen Implementierungen vom MPI-Standard zu machen, um Effizienz meiner 
 *      Implementierung zu testen:
 *          => MPI RMA fußt prinzipiell auf zwei Möglichkeiten:
 *              _> Implementierung durch pt2pt (PML Modul), in Wirklichkeit wird zweiseitige Kommunikation verwendet
 *              _> Implementierung durch Remote Memory Access (OSC Modul), setzt aber RMA Hardware Support voraus 
 *          => Überprüfe Möglichkeit, in C Hardwareinformationen zu erfragen und besten Channel auszuwählen (z.B. in Channel_init-Funktion)
 * 
 *          => MPI Implementierungen (OpenMPI, etc.) sind leider nicht sehr transparent; FAQ bzw. Dokumentationen geben nur grob Überblick
 *          über die Struktur aber nicht über die Implementierung; 
 * 
 * Todo:
 *      - Weitere Channeltypen implementieren
 *          => RMA Channelimplementierung durch MCS lock effizienter machen. Löst das Problem, dass z.B. 
 *          Producer mehrmals hintereinander den vollen Channel sperrt
 * 
 *      - Weitere Tests entwickeln (im Moment nur Durchsatz) 
 * 
 *      - Weitere Literatur recherchieren
 * 
 *      - Wenn alle Channeltypen fertig sind, andere MPI-Implementierungen testen (MPICH, ...)
 * 
 * Titel:   
 *      - Design und Implementierung von Channels in parallelen Systemen
 *      - Design und Implementierung von Channels im verteilten Adressraum
 *      - Design und Implementierung von Channels zur Interprozesskommunikation im verteilten Adressraum
 *      - ZeroMQ eventuell als Ausblick
 * 
 * 
 */

/* 17.12.20
 *
 * done:
 *      - Implementation of SPSC RMA (un)buffered unsynchronized Channels (also works for MPSC/MPMC)
 *      - Implementation of SPSC NAT synchronized Channels
 *      - Implementation of SPSC NAT (un)buffered unsynchronized Channels
 *      - Multiple benchmarks of implementations and basic rma/native operations
 * 
 * problems:
 *      - Time measurement of RMA Channels shows bad results
 *      - 1000 Send/Receive Ops with buffersize 100:
 *          - RMA: ~33.000 microseconds
 *          - NAT: ~ 1.500 microseconds
 *      - Any optimization done?
 *      - Reasons: Useless blocking of window for checking if data is ready
 *      - Solution: Use a notification mechanism
 *      - Reason:  Fewer locks, for checking indeces of read/write no lock is required
 *      - Solution: Use locks where they are needed
 * 
 * to do:
 *     - Try to tweak rma implementation (literature, documentation, etc.)
 *     - Read Performance and Notified Access paper for tweaking rma implementations
 *     - Implement synchronized rma version (Fence?)
 *     - Try zeromp
 *     - mpi benchmarks 
 * 
 * name:
 *      - Design und Implementierung von Channels in parallelen und verteilten Systemen (falls ZeroMQ klappt)
 *      - Design und Implementierung von Channels in parallelen Systemen (mittels MPI)
 *      - Design and Implementation of Channels in Parallel (and Distributed) Systems
 */

/* 10.12.20
 *
 * done:
 *      - Implemented SPSC BUU, UUU and BUS ((B)locked, (U)nbuffered, (S)ynchronised)
 *      - Structured the code to get it work (might be better patterns)
 *      - Read some more literature (MPI-3 Standard, Go’s Concurrency Constructs on the SCC, Dr. Prells thesis, etc.)
 *      - Made a first strategy for rma approach and buffer (cirucular queue)
 * 
 * problems:
 *      - Had a annoying bug (data was uninitialised), MPI crashed with nondescript error -> MPI is kind of hard to debug
 *      - MPI_Iprobe wont work with all MPI Send modes, needs to check for messages already send to ensure no two send calls
 * 
 * - should a channel receive and send any types of data as long as the size is not exceeded?
 * - implement channel as follows: MPI_Channel channel_alloc (size_t sz, size_t n) where sz is the size of data and n states the buffer (0 unbuf, >0 buf)
 * 
 * to do:
 *      - implement the channel types dr prell uses
 *      - implement rma channels to test them against native mpi_send/recv channels
 *      - read zeromq
*/

/* 03.12.20
 * quote Dr. Prell [S. 47]
 * "Combined with other recent developments, such as the extended RMA model of MPI-3 [119] and improved
 * producer-consumer communication [43], MPI may permit increasingly efficient channel implementations.
 * 
 * different ways to implement channels for interprocess communication:
 *      - implement unbuffered blocked and unblocked SPSC channels with MPI_Send/Receive and MPI_Isend/Ireceive respectively
 *          ~ needs to save rank of receiver in struct MPI_Channel, tag and source can be MPI_ANY_TAG and MPI_ANY_SOURCE 
 *      - use Remote Memory Access (RMA) for the other types of MPI_Channel
 *      - use RMA Shared for processes with shared memory (no "copy behaviour" but needs synchronization)
 *       
 * some problems:
 *      - where will the buffer be saved? RMA creates a window for every process in the communicator
 *          ~ does the location of the memory of the buffer have an impact on message passing speed?
 *      - how to implement the synchronization mechanism
 *          ~ is passive synchronization better than active?
 *          ~ does build-in lock/unlock take too much time?
 *          ~ usage of atomic operations (MPI_Compare_and_swap)
 *      - how to secure the order of channel_send() calls and avoid race conditions?
 *      - how to implement the queue?
 *      - what is the sequence of mpi calls? (Lock - Retrieve Index i - Get Data At Index i - Unlock), ow does channel_receive() 
 *          know, which data to pick? where is the index saved? does the receiving process needs to clean up the target buffer?
 *  
 * to do:
 *      - implement basic MPI_Channel functions (especially functions using two-sided communication)
 *      - get an idea about implementing RMA and solving synchronization issues 
 *      - read the following literature:
 *          ~ Andreas Prell and Thomas Rauber. Go’s Concurrency Constructs on the SCC. In MARC Symposium, pages 2–6, 2012.
 *              -> channels implementation with RMA approach
 *          ~ Ownership Passing: Efficient Distributed Memory Programming on Multi-core Systems. 
 *              -> better approach for avoiding costly copy semantics in MPI
 *          ~ Remote Memory Access Programming in MPI-3.
 *              -> more knowledge on RMA
 *          ~ Notified Access: Extending Remote Memory Access Programming Models for Producer-Consumer Synchronization.
 *              -> more knowledge on using RMA for producer-consumer problem and synchronization
*/
