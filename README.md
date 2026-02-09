# Temat 10 – Magazyn firmy spedycyjnej

# *Bartłomiej Rojek*


## Link do repozytorium GitHub
https://github.com/bartek1987ro/so-projekt-2025

---

## 1. Opis zadania

W magazynie przy taśmie transportowej pracuje trzech pracowników oznaczonych przez P1, P2 i P3. Pracownicy układają na taśmę przesyłki o gabarytach:
- **A** (64×38×8 cm) - waga 0.1-8.0 kg
- **B** (64×38×19 cm) - waga 0.1-16.0 kg
- **C** (64×38×41 cm) - waga 0.1-25.0 kg

Wszystkie paczki mają maksymalną wagę 25 kg. Zakładamy im mniejsza paczka, tym mniejszy ciężar.

W magazynie znajduje się również **pracownik P4**, odpowiedzialny za załadunek przesyłek ekspresowych. Przesyłki ekspresowe:
- Dostarczane są osobno (nie przez taśmę)
- Mają wyższy priorytet – ich załadunek musi odbyć się w pierwszej kolejności
- Mogą zawierać tylko przesyłki gabarytów A, B i C o wadze < 25 kg

Przesyłki następnie z taśmy są ładowane na ciężarówke. Ciężarówki po osiągnięciu maksymalnej pojemności lub ładowności wyjeżdzają rozwieźć paczki. Pod rampę od razu podjeżdża następna ciężarówka.

### Taśma transportowa
- Maksymalna pojemność: **K** przesyłek
- Maksymalny udźwig: **M** kg (gdzie K×25kg > M)
- Przesyłki zjeżdżają w kolejności 'pierwsze weszło, pierwsze wyszło' (FIFO)

### Ciężarówki
- Ładowność: **W** kg
- Objętość: **V** m³
- Liczba ciężarówek: **N**
- Muszą być załadowane do pełna wby wyjechać
- Po rozładunku wracają po czasie **Ti**

### Sterowanie przez dyspozytora
- **Sygnał 1:** Ciężarówka odjeżdża z niepełnym ładunkiem
- **Sygnał 2:** P4 dostarcza pakiet ekspresowy
- **Sygnał 3:** Pracownicy kończą pracę, ciężarówki rozwożą pozostałe

---

## 2. Parametry konfiguracyjne

| Parametr | Opis                           |
|-|--------------------------------|
| K | Maksymalna liczba przesyłek na taśmie |
| M | Maksymalny udźwig taśmy        |
| W | Ładowność ciężarówki           |
| V | Objętość ładunkowa ciężarówki  |
| N | Liczba ciężarówek              |
| Ti | Czas kursu ciężarówki (losowy) |

---

## 3. Budowa symulacji

### 3.1 Procesy

| Proces    | Opis              |
|-----------|-------------------|
| Dyspozytor | Proces główny, inicjalizacja IPC, sterowanie |
| Pracownik P1 |  Układa przesyłki typu A |
| Pracownik P2 |  Układa przesyłki typu B |
| Pracownik P3 |  Układa przesyłki typu C |
| Pracownik P4 |  Obsługuje przesyłki ekspresowe |
| Ciężarówka i |  Transport przesyłek |

### 3.2 Mechanizmy IPC

1. **Pamięć dzielona** - stan taśmy, status ciężarówek
2. **Kolejka komunikatów** - powiadomienia o załadunku/odjeździe
3. **Semafory** - synchronizacja dostępu do taśmy i rampy
4. **Sygnały** - sterowanie przez dyspozytora

---

## 4. Testy funkcjonalne

### Test 1: Działanie symulacji bez opóźniania ciężarówek
- **Cel:** Weryfikacja działania symulacji po zakomentowaniu części kodu odpowiadającego za usypianiu ciężarówek na czas trasy
```c++
  alarm(course_time);

        sigset_t alarm_mask;
        sigfillset(&alarm_mask);
        sigdelset(&alarm_mask, SIGALRM);
        sigdelset(&alarm_mask, SIGUSR2);

        while (!g_alarm_fired && !g_shutdown)
            sigsuspend(&alarm_mask);
        alarm(0);
```
- **Wynik**
```shell
[05:23:39][DYSPOZYTOR](PID 156875): === RAPORT KONCOWY ===
[05:23:39][DYSPOZYTOR](PID 156875): Czas trwania: 0 sekund
[05:23:39][DYSPOZYTOR](PID 156875): --- Ciezarowki ---
[05:23:39][DYSPOZYTOR](PID 156875):   Ciezarowka 0 (PID 156876): kursow=61, paczek=1022
[05:23:39][DYSPOZYTOR](PID 156875):   Ciezarowka 1 (PID 156877): kursow=61, paczek=1002
[05:23:39][DYSPOZYTOR](PID 156875):   Ciezarowka 2 (PID 156878): kursow=60, paczek=977
[05:23:39][DYSPOZYTOR](PID 156875): Paczek przetworzonych lacznie: ~3000
[05:23:39][DYSPOZYTOR](PID 156875): === KONIEC RAPORTU ===

=== Symulacja zakonczona. Raport: raport_20260209_052339.txt ===
bartekr@BartekPC:~/projekty/so-projekt-2025$ ps
    PID TTY          TIME CMD
 144522 pts/7    00:00:00 bash
 157058 pts/7    00:00:00 ps
bartekr@BartekPC:~/projekty/so-projekt-2025$ ipcs -a

------ Message Queues --------
key        msqid      owner      perms      used-bytes   messages

------ Shared Memory Segments --------
key        shmid      owner      perms      bytes      nattch     status

------ Semaphore Arrays --------
key        semid      owner      perms      nsems
```
- **Wniosek:** Symulacja przebiega prawidłowo i wykonuje się natychmiast.


### Test 2: Prawidłowe zamykanie sygnałem SIGINT (ctrl+c)
- **Cel:** Sprawdzenie czy po wciśnięciu ctrl + c symulacja zamyka się prawidłowo.
- **Wynik**
```shell
^C[SIGNAL] PID 187791 otrzymal sygnal 2 (SIGINT)
[06:35:33][DYSPOZYTOR](PID 187791): === ZAMYKANIE ===
[SIGNAL] PID 187796 otrzymal sygnal 12 (SIGUSR2)
[SIGNAL] PID 187795 otrzymal sygnal 12 (SIGUSR2)
[SIGNAL] PID 187797 otrzymal sygnal 12 (SIGUSR2)
[SIGNAL] PID 187798 otrzymal sygnal 12 (SIGUSR2)
[06:35:33][P2](PID 187796): Pracownik konczy prace (utworzono 38 paczek)
[06:35:33][P1](PID 187795): Pracownik konczy prace (utworzono 41 paczek)
[06:35:33][P3](PID 187797): Pracownik konczy prace (utworzono 36 paczek)
[06:35:33][P4](PID 187798): P4 konczy prace (dostarczono 0 ekspresow)
[06:35:33][DYSPOZYTOR](PID 187791): Pracownicy zakonczyli
[SIGNAL] PID 187792 otrzymal sygnal 12 (SIGUSR2)
[SIGNAL] PID 187793 otrzymal sygnal 12 (SIGUSR2)
[SIGNAL] PID 187794 otrzymal sygnal 12 (SIGUSR2)
[SIGNAL] PID 187793 otrzymal sygnal 34 (SIGRTMIN)
[SIGNAL] PID 187792 otrzymal sygnal 34 (SIGRTMIN)
[SIGNAL] PID 187792 otrzymal sygnal 10 (SIGUSR1)
[SIGNAL] PID 187793 otrzymal sygnal 10 (SIGUSR1)
[SIGNAL] PID 187794 otrzymal sygnal 34 (SIGRTMIN)
[SIGNAL] PID 187794 otrzymal sygnal 10 (SIGUSR1)
[06:35:33][TRUCK1](PID 187793): Wrocil z kursu #2
[06:35:33][TRUCK0](PID 187792): Wrocil z kursu #3
[06:35:33][TRUCK2](PID 187794): Wrocil z kursu #1
[06:35:33][TRUCK1](PID 187793): Ciezarowka konczy prace (kursow: 2, paczek: 36)
[06:35:33][TRUCK0](PID 187792): Ciezarowka konczy prace (kursow: 3, paczek: 54)
[06:35:33][TRUCK2](PID 187794): Ciezarowka konczy prace (kursow: 1, paczek: 15)
[06:35:33][DYSPOZYTOR](PID 187791): Ciezarowki zakonczyli
[06:35:33][DYSPOZYTOR](PID 187791): === RAPORT KONCOWY ===
[06:35:33][DYSPOZYTOR](PID 187791): Czas trwania: 4 sekund
[06:35:33][DYSPOZYTOR](PID 187791): --- Ciezarowki ---
[06:35:33][DYSPOZYTOR](PID 187791):   Ciezarowka 0 (PID 187792): kursow=3, paczek=54
[06:35:33][DYSPOZYTOR](PID 187791):   Ciezarowka 1 (PID 187793): kursow=2, paczek=36
[06:35:33][DYSPOZYTOR](PID 187791):   Ciezarowka 2 (PID 187794): kursow=1, paczek=15
[06:35:33][DYSPOZYTOR](PID 187791): Paczek przetworzonych lacznie: ~115
[06:35:33][DYSPOZYTOR](PID 187791): === KONIEC RAPORTU ===

=== Symulacja zakonczona. Raport: raport_20260209_063529.txt ===
bartekr@BartekPC:~/projekty/so-projekt-2025$ ps
    PID TTY          TIME CMD
 185118 pts/6    00:00:00 bash
 188955 pts/6    00:00:00 ps
bartekr@BartekPC:~/projekty/so-projekt-2025$ ipcs -a

------ Message Queues --------
key        msqid      owner      perms      used-bytes   messages

------ Shared Memory Segments --------
key        shmid      owner      perms      bytes      nattch     status

------ Semaphore Arrays --------
key        semid      owner      perms      nsems
```
- **Wniosek** Dyspozytor prawidłowo przechwytuje sygnał, a następnie wysyła sygnały do procesów potomnych aby te zakończyły swoje działanie. Polecenia ps i ipcs -a potwierdzają brak zawieszonych procesów i poprawne zwolnienie zasobów IPC.
### Test 3: Działania polecenia dyspozytora 'odjazd z niepełnym ładunkiem'
- **Cel:** Sprawdzenie czy po wysłaniu sygnału SIGUSR1 do dyspozytora ten wyślę ciężarówkę z niepełnym ładunkiem w trasę
- **Wynik:**
```shell
kill -SIGUSR1 197918
```
```shell
[07:00:03][TRUCK0](PID 197919): Zaladowano paczke #113 typ B, waga 9.2 kg [ciezarowka: 85.3/150 kg, 0.516/1.00 m3, 10 szt]
[SIGNAL] PID 197918 otrzymal sygnal 10 (SIGUSR1)
[07:00:03][DYSPOZYTOR](PID 197918): Sygnal 1: ciezarowka 0 odjedza z niepelnym
[SIGNAL] PID 197919 otrzymal sygnal 10 (SIGUSR1)
[07:00:03][TRUCK0](PID 197919): Dyspozytor nakazal odjazd z niepelnym
[07:00:03][TRUCK0](PID 197919): Odjazd: 10 paczek, 85.3 kg, 0.516 m3 (kurs #3)
[07:00:03][TRUCK1](PID 197920): Podjezdzam pod rampe
[...]
[07:00:11][DYSPOZYTOR](PID 197918): === RAPORT KONCOWY ===
[07:00:11][DYSPOZYTOR](PID 197918): Czas trwania: 27 sekund
[07:00:11][DYSPOZYTOR](PID 197918): --- Ciezarowki ---
[07:00:11][DYSPOZYTOR](PID 197918):   Ciezarowka 0 (PID 197919): kursow=4, paczek=61
[07:00:11][DYSPOZYTOR](PID 197918):   Ciezarowka 1 (PID 197920): kursow=4, paczek=61
[07:00:11][DYSPOZYTOR](PID 197918):   Ciezarowka 2 (PID 197921): kursow=3, paczek=43
[07:00:11][DYSPOZYTOR](PID 197918): Paczek przetworzonych lacznie: ~165
[07:00:11][DYSPOZYTOR](PID 197918): === KONIEC RAPORTU ===

=== Symulacja zakonczona. Raport: raport_20260209_065944.txt ===
bartekr@BartekPC:~/projekty/so-projekt-2025$ ps
    PID TTY          TIME CMD
 185118 pts/6    00:00:00 bash
 201462 pts/6    00:00:00 ps
bartekr@BartekPC:~/projekty/so-projekt-2025$ ipcs -a

------ Message Queues --------
key        msqid      owner      perms      used-bytes   messages

------ Shared Memory Segments --------
key        shmid      owner      perms      bytes      nattch     status

------ Semaphore Arrays --------
key        semid      owner      perms      nsems
```
- **Wniosek:** Dyspozytor prawidłowo obsługuje sygnał SIGUSR1

### Test 4: Działania polecenia dyspozytora 'dostarcz paczki ekspresowe'
- **Cel:** Sprawdzenie czy po wysłaniu sygnału SIGUSR2 do dyspozytora ten poleci pracownikowi P4 załadunek paczek ekspresowych.
- **Wynik:**
```shell
kill -SIGUSR2 235100
```
```shell
[SIGNAL] PID 223107 otrzymal sygnal 10 (SIGUSR1)
[08:02:10][P4](PID 223107): Pakiet ekspresowy (2 paczek)
[SIGNAL] PID 223101 otrzymal sygnal 14 (SIGALRM)
[08:02:10][TRUCK0](PID 223101): Wrocil z kursu #11
[08:02:10][DYSPOZYTOR](PID 223100): Ciezarowka 0 jest wolna
[08:02:10][DYSPOZYTOR](PID 223100): Wysylam ciezarowke 0 pod rampe
[SIGNAL] PID 223101 otrzymal sygnal 34 (SIGRTMIN)
[08:02:10][TRUCK0](PID 223101): Podjezdzam pod rampe
[08:02:10][TRUCK0](PID 223101): Zaladowano EKSPRES typ B, waga 12.6 kg [ciezarowka: 12.6/150 kg, 0.046/1.00 m3]
[08:02:10][P4](PID 223107): Dostarczono ekspres #1 typ B, waga 12.6 kg
[08:02:10][TRUCK0](PID 223101): Zaladowano paczke #498 typ C, waga 22.9 kg [ciezarowka: 35.5/150 kg, 0.146/1.00 m3, 2 szt]
[08:02:10][P2](PID 223105): Polozono paczke #508 typ B, waga 6.1 kg [tasma: 10/10 szt, 77.8/120 kg]
[08:02:10][TRUCK0](PID 223101): Zaladowano EKSPRES typ A, waga 4.5 kg [ciezarowka: 40.0/150 kg, 0.165/1.00 m3]
[08:02:10][P4](PID 223107): Dostarczono ekspres #2 typ A, waga 4.5 kg
[08:02:10][TRUCK0](PID 223101): Zaladowano paczke #499 typ B, waga 12.4 kg [ciezarowka: 52.4/150 kg, 0.212/1.00 m3, 4 szt]
[08:02:10][P1](PID 223104): Polozono paczke #509 typ A, waga 7.6 kg [tasma: 10/10 szt, 73.0/120 kg]
[...]
[08:31:16][DYSPOZYTOR](PID 235100): === RAPORT KONCOWY ===
[08:31:16][DYSPOZYTOR](PID 235100): Czas trwania: 22 sekund
[08:31:16][DYSPOZYTOR](PID 235100): --- Ciezarowki ---
[08:31:16][DYSPOZYTOR](PID 235100):   Ciezarowka 0 (PID 235101): kursow=8, paczek=132
[08:31:16][DYSPOZYTOR](PID 235100):   Ciezarowka 1 (PID 235102): kursow=7, paczek=115
[08:31:16][DYSPOZYTOR](PID 235100):   Ciezarowka 2 (PID 235103): kursow=7, paczek=113
[08:31:16][DYSPOZYTOR](PID 235100): Paczek przetworzonych lacznie: ~362
[08:31:16][DYSPOZYTOR](PID 235100): === KONIEC RAPORTU ===

=== Symulacja zakonczona. Raport: raport_20260209_083054.txt ===
bartekr@BartekPC:~/projekty/so-projekt-2025$ ps
    PID TTY          TIME CMD
 185118 pts/6    00:00:00 bash
 237283 pts/6    00:00:00 ps
bartekr@BartekPC:~/projekty/so-projekt-2025$ ipcs -a

------ Message Queues --------
key        msqid      owner      perms      used-bytes   messages

------ Shared Memory Segments --------
key        shmid      owner      perms      bytes      nattch     status

------ Semaphore Arrays --------
key        semid      owner      perms      nsems

```
- **Wniosek** Dyspozytor prawidłowo obsługuje sygnał SIGUSR2
### Test 5: Brak zakleszczenia
- **Cel:** Symulacja nie zawiesza się
- **Warunki:** Długi czas pracy
- **wynik:**
```shell
[08:46:48][DYSPOZYTOR](PID 238198): === RAPORT KONCOWY ===
[08:46:48][DYSPOZYTOR](PID 238198): Czas trwania: 513 sekund
[08:46:48][DYSPOZYTOR](PID 238198): --- Ciezarowki ---
[08:46:48][DYSPOZYTOR](PID 238198):   Ciezarowka 0 (PID 238199): kursow=167, paczek=2785
[08:46:48][DYSPOZYTOR](PID 238198):   Ciezarowka 1 (PID 238200): kursow=178, paczek=3015
[08:46:48][DYSPOZYTOR](PID 238198):   Ciezarowka 2 (PID 238201): kursow=175, paczek=2925
[08:46:48][DYSPOZYTOR](PID 238198): Paczek przetworzonych lacznie: ~8724
[08:46:48][DYSPOZYTOR](PID 238198): === KONIEC RAPORTU ===

=== Symulacja zakonczona. Raport: raport_20260209_083815.txt ===
```
**Symulacja działa bez zawieszenia**

---

## 5. Raport symulacji

System generuje raport do pliku zawierający:
- Czas startu i zakończenia symulacji
- Liczbę przetworzonych przesyłek (wg typu)
- Liczbę kursów ciężarówek
- Statystyki załadunku (waga, objętość)
- Zdarzenia sygnałowe
- Ewentualne błędy

---

