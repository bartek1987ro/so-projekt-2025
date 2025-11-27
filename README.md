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

### Test 1: Podstawowy przepływ przesyłek
- **Cel:** Weryfikacja kolejności FIFO na taśmie
- **Oczekiwany wynik:** Przesyłki zjeżdżają w kolejności położenia

### Test 2: Ograniczenie udźwigu taśmy
- **Cel:** Taśma nie przekracza maksymalnego udźwigu M
- **Oczekiwany wynik:** Pracownicy czekają gdy waga przekroczyłaby M

### Test 3: Priorytet przesyłek ekspresowych
- **Cel:** Ekspresowe ładowane przed standardowymi
- **Oczekiwany wynik:** P4 dostarcza ekspres bezpośrednio do ciężarówki

### Test 4: Działanie sygnałów
- **Cel:** Weryfikacja działania sygnałów

### Test 5: Brak zakleszczenia
- **Cel:** Symulacja nie zawiesza się
- **Warunki:** Długi czas pracy
- **Oczekiwany wynik:** Płynne działanie bez zawieszenia

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

