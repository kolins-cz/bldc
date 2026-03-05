# Proporcionální řídící algoritmus pro generátor

## Popis algoritmu

Aktuálně implementovaný řídící algoritmus pro generátorový mód používá **lineární proporcionální závislost** mezi otáčkami motoru a požadovaným regeneračním proudem.

### Fyzikální princip

Elektrický generátor (zde BLDC motor v regeneračním režimu) produkuje proud úměrný mechanickému výkonu, který jej roztáčí. Algoritmus využívá této závislosti k vytvoření **samoregulujícího systému** s lineární charakteristikou.

---

## Matematický model

### Vstupní parametry

| Parametr | Fyzikální význam | Jednotka |
|----------|------------------|----------|
| `gen_erpm` | Cílové elektrické otáčky (100% výkon) | ERPM |
| `gen_current` | Maximální regenerační proud | A |
| `gen_start` | Relativní otáčky zahájení generování | bezrozměrné (0-1) |
| `gen_update_rate` | Frekvence aktualizace regulátoru | Hz |

### Algoritmus (C-like zápis)

```c
// Přečti aktuální otáčky
float rpm_now = mc_interface_get_rpm();
float rpm_abs = fabsf(rpm_now);

// Normalizuj na rozsah 0-1 (kde 1.0 = cílové otáčky)
float rpm_rel = rpm_abs / gen_erpm;

// Spočítej proud
float current = rpm_rel - gen_start;

if (current < 0.0) {
    current = 0.0;  // Pod prahem = volnoběh
}

// Přeškáluj na rozsah 0 až gen_current
current = current / (1.0 - gen_start);
current = current * gen_current;

// Aplikuj regenerační proud (záporný = generování)
mc_interface_set_current(-current);
```

### Stavový diagram

```
      ┌─────────────────────────────────┐
      │  Přečti RPM                     │
      └──────────┬──────────────────────┘
                 │
                 v
      ┌──────────────────────┐
      │ RPM < start_rpm?     │
      └──┬──────────────┬────┘
         │ ANO          │ NE
         v              v
    ┌────────┐    ┌─────────────────┐
    │ I = 0  │    │ Vypočti lineárně│
    └────┬───┘    └────────┬─────────┘
         │                 │
         └────────┬────────┘
                  v
           ┌──────────────┐
           │ Aplikuj proud│
           └──────────────┘
```

---

## Charakteristická křivka

```
Proud [A]
    ↑
    │
 20 │                          ┌───────────────────── (gen_current)
    │                         /│
 15 │                        / │
    │                       /  │
 10 │                      /   │
    │                     /    │
  5 │                    /     │
    │                   /      │
  0 ├──────────────────┴───────┼─────────────────────→ Otáčky [ERPM]
    0                3000      6000                 9000
                      ↑          ↑                   ↑
                   prah       cílové            přetočení
                (gen_start)  (gen_erpm)
```

### Oblasti funkce

**Oblast I (0 ... 3000 ERPM):** Volnoběh
- Proud = 0 A
- Motor se volně roztáčí bez zátěže

**Oblast II (3000 ... 6000 ERPM):** Lineární nárůst
- Proud lineárně roste s otáčkami
- Plynulý přechod mezi volnoběhem a maximálním výkonem
- Sklon křivky určen rozdílem (cílové - prah) otáčky

**Oblast III (6000+ ERPM):** Konstantní proud
- Proud = maximální (gen_current)
- Maximální regenerační proud (omezen)
- Motor nadále zrychluje jen pokud má spalovací motor více výkonu

---

## Fyzikální chování systému

### Samoregulace

Systém má **negativní zpětnou vazbu** na otáčky:

1. **Motor přidá výkon** → Otáčky rostou
2. **Algoritmus zvýší proud** → Elektrická zátěž roste  
3. **Mechanická zátěž brzdí motor** → Otáčky se stabilizují
4. **Rovnováha:** $P_{mech} = P_{elec} + P_{ztráty}$

Tento mechanismus je **inherentně stabilní** bez nutnosti PID regulátoru.

### Přechodové jevy

**Start spalovacího motoru:**
```
t=0s:   Motor startuje    → ω ≈ 0     → I = 0 (žádná zátěž)
t=2s:   Motor na volnoběh  → ω ≈ 1000  → I = 0 (pod prahem)
t=5s:   Roztáčení          → ω ≈ 1800  → I začíná růst
t=10s:  Ustálený stav      → ω ≈ 2000  → I = I_max
```

**Změna zátěže baterie:**
- Baterie nízká (48V): P = 48V × 20A = 960W
- Baterie plná (54V): P = 54V × 20A = 1080W
- **Pozorování:** Výkon není konstantní! (závisí na napětí)

---

## Řízení škrtící klapky karburátoru

### Základní princip

Tento **proporcionální algoritmus řídí pouze elektrickou zátěž** generátoru. Neobsahuje žádné pokročilé řízení škrtící klapky (plynu) spalovacího motoru.

### Co algoritmus NEDĚLÁ

❌ **PID regulátor plynu** - Algoritmus nekontroluje polohu škrtící klapky  
❌ **Adaptivní řízení otáček motoru** - Nepřidává ani neubírá plyn podle zátěže  
❌ **Optimalizace spotřeby paliva** - Nemění nastavení karburátoru dynamicky  
❌ **Koordinace se spalovacím motorem** - Neexistuje zpětná vazba na motor  

### Praktické řešení škrtící klapky

**Možnost 1: Pevná pozice plynu (doporučeno pro začátek)**
```
- Nastavit škrtící klapku servopohonem na fixní pozici (např. 60%)
- Elektrická zátěž se přizpůsobuje otáčkám automaticky
- Jednoduché, spolehlivé
```

**Možnost 2: ON/OFF řízení (pro start/stop)**
```
- Servo motor jen otevře nebo zavře plyn (digitální řízení)
- Zapnuto: Pevná pozice plynu
- Vypnuto: Motor na volnoběh nebo stop
- Žádná spojitá regulace, jen dva stavy
```

**Možnost 3: Ruční ovládání přes dálkové řízení**
```
- Pilot ovládá plyn otočným knoflíkem na dálkovém ovladači
- Servo pohon řídí pozici škrtící klapky podle vstupu z RC
- Algoritmus se adaptuje na aktuální otáčky
- Uživatel má plnou kontrolu nad motorem v letu
```

### Proč není pokročilé řízení plynu v základní verzi?

Tento algoritmus je **nejjednodušší možný přístup k řízení generátoru**, nalezený jako referenční implementace. 

**Hlavní důvod:**
- Jedná se o základní testovací implementaci
- Pravděpodobně nejjednodušší ze všech možných algoritmů
- Dobrý pro ověření základní funkčnosti systému
- Slouží jako základ pro budoucí vylepšení

**Dodatečné výhody jednoduchosti:**
1. **Snadné pochopení** - Přímočará lineární závislost
2. **Rychlé nasazení** - Minimální nastavení parametrů
3. **Bezpečnost** - Žádné složité logiky které by mohly selhat
4. **Testování** - Snadné ověření správné funkce

### Interakce s elektrickým řízením

**Řízení otáček:**
```
Pozice plynu (servo): Nastaví přísun paliva
Elektrický algoritmus: Přidá/ubere zátěž → Generuje proud

Výsledek: 
- Otáčky se nastaví pozicí plynu
- Elektrická zátěž se plynule přizpůsobuje aktuálním otáčkám
- Stabilní provoz bez oscilací
```

### Budoucí rozšíření (v plánu)

Pokročilejší verze by mohla obsahovat:
- Automatické řízení plynu s PID regulátorem
- Adaptivní optimalizace spotřeby paliva  
- Automatický start/stop podle stavu baterie
- Koordinace více generátorů na CAN sběrnici

Ale pro základní provoz **není plně automatické řízení plynu nutné**.

---

## Vlastnosti algoritmu

### Výhody

✅ **Jednoduchost implementace**
- Žádný PID regulátor
- Žádná integrace či derivace
- Přímý výpočet z měřených hodnot

✅ **Inherentní stabilita**
- Negativní zpětná vazba
- Nemůže oscilovat
- Robustní vůči poruchám měření

✅ **Samoregulace**
- Systém sám najde rovnováhu
- Přizpůsobí se výkonu spalovacího motoru
- Funguje při různých otáčkách

✅ **Ochrana před přetížením**
- Lineární charakteristika = prediktabilní chování
- Nemůže náhle vytvořit extrémní zátěž
- Maximální proud je pevně omezen

### Nevýhody

❌ **Variabilní výkon**
- Výkon závisí na napětí baterie (P = U × I)
- Při stálém proudu se mění výkon s napětím baterie
- Neoptimální pro konstantní energetický výkon

❌ **Neoptimalizováno pro účinnost**
- Nesleduje optimální pracovní bod spalovacího motoru
- Neexistuje adaptace na měnící se podmínky
- Fixní charakteristika bez učení

❌ **Závislost na nastavení**
- Parametry musí být ručně nastaveny
- Optimální hodnoty závisí na konkrétním spalovacím motoru
- Žádná automatická kalibrace

---

## Možné modifikace

### Nelineární charakteristika

Místo lineární závislosti použít:

**Kvadratická (výkon úměrný otáčkám na druhou):**
```c
float ratio = (rpm_rel - gen_start) / (1.0 - gen_start);
current = gen_current * ratio * ratio;
```

**Exponenciální:**
```c
float k = 5.0;  // konstanta strmosti
current = gen_current * (1.0 - expf(-k * (rpm_rel - gen_start)));
```

**Motivace:** Přiblížit se výkonové charakteristice spalovacích motorů

### Adaptivní prah

```
if spalovací_motor_teplý():
    gen_start = 0.85    // dříve zahájit generování
else:
    gen_start = 0.95    // při startu ponechat motor lehčí
```

### Hystereze

Zabránit oscilacím v přechodové oblasti:

```
if stoupající_otáčky():
    práh = gen_start
else:
    práh = gen_start + 0.05    // větší práh při klesajících otáčkách
```

---

## Otázky k diskusi

### Teoretické
1. Je lineární charakteristika optimální pro daný spalovací motor?
2. Jak by se choval systém s PID regulátorem místo proporcionálního?

### Praktické
4. Jak se změní chování při proměnlivém zatížení baterie (BMS odpojí)?
5. Je možné detekovat optimální pracovní bod motoru automaticky?
6. Měla by charakteristika záviset na teplotě spalovacího motoru?

### Možná vylepšení
7. Konstantní výkon (P = const) místo konstantního proudu (I = const)?
8. MPPT algoritmus pro automatické hledání maxima?
9. Adaptivní ladění parametrů během provozu?

---

*Dokument vytvořen pro technickou diskusi a peer review*  
*Verze: 1.0*  
*Datum: Březen 2026*
