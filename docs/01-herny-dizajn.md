# Do it Asphalt – herný dizajn

Verzia 2.0 · 2026-11-21 · popisuje hru Asphalt-self tak, ako je naprogramovaná

Tento dokument hovorí, ako sa hra hrá: pravidlá, ovládanie, obrazovka, cheaty a známe obmedzenia. Všetky čísla sú prevzaté z kódu (`Source/AsphaltCore/Public/LotBalance.h` a `LotLayout.h`), nie z plánu. Ako je hra postavená, je v `02-technicky-navrh.md`. Kde sa hotová hra líši od návrhu v `04-plan-zmien-asphalt-self.md`, vysvetľuje to kapitola 10.

---

## 1. Prehľad

| | |
|---|---|
| Žáner | pokojná inkrementálna simulácia asfaltovania |
| Platforma | macOS (vývoj), cieľovo aj Windows |
| Engine | Unreal Engine 5.8 (C++) |
| Kamera | prvá osoba, pracovný pohľad podržaním pravého tlačidla myši |
| Hráči | 1 |
| Cieľová dĺžka | asi 2 h 15 min (simulácia dáva 2 h 21 min, pozri kapitolu 9) |
| Vzhľad | parkovisko pred nákupným centrom, tlmené farby, jedna akcentová (oranžová) |

**Jednou vetou:** hráč vyleje asfalt na parkovisko v tvare písmena L (30 000 štvorcov 1 × 1 m), najprv z misky lyžicou, neskôr z vedra, kufra, vozíka, finišera, robota a auta; za každý hotový štvorec dostane 10 kreditov, za kredity kupuje lepšie vybavenie a výrobňu a po vyasfaltovaní všetkého sa otvorí východ.

## 2. Mapa

- Parkovisko je štvorec 200 × 200 m bez jedného kvadrantu 100 × 100 m: v ňom stojí nákupné centrum. Zostáva tvar L s 30 000 štvorcami (políčkami) 1 × 1 m.
- Okolo parkoviska je 12 m široký pás s obrubníkom, plotom a vstupnými cestami.
- Políčka sú zoskupené do 75 **sektorov** 20 × 20 políčok (400 políčok v sektore). Sektory sa v hre číslujú od 1 (v súbore kódu od 0).
- Okolo L je **6 vstupných úsekov**, každý široký 12 políčok. Pred každým stojí tabuľa, ktorá hovorí, aký krok odomyká (kapitola 7), a koľko z 12 políčok úseku je hotových („Entry k / 12").
- Výrobňa asfaltu stojí pri štarte, na okraji parkoviska. Pred ňou je koryto (trough) s asfaltom, z ktorého sa nabera. Hráč začína tesne pred nimi a je k nim obrátený.
- Východ je vo vstupnom úseku 6. Je to brána s ramenom; vpustí hráča na cestu von, až keď je hotové celé parkovisko.

## 3. Herná slučka

1. Hráč nabere asfalt z koryta (alebo z výpustu výrobne) do nádoby v ruke.
2. Vylieva ho na voľné políčka. Zem ukazuje, či je políčko voľné, surové alebo hotové.
3. Keď je vyliate dosť (95 % plochy políčka), políčko sa dokončí, zvyšok sa doleje samo a hráč dostane 10 kreditov.
4. Za kredity kupuje lepšie nádoby a stroje a vylepšuje výrobňu.
5. Počet hotových políčok odomyká ďalšie kroky: rýchla výmena (10), šprint (100), pásové stroje (350), robot (800), auto (6 000) a východ (30 000).
6. Keď je hotových všetkých 30 000 políčok, otvorí sa východ. Prejsť cez bránu znamená výhru.

## 4. Asfaltovanie

### 4.1 Políčko

- Políčko má 1 × 1 m a potrebuje **10 l** asfaltu. Je hotové, keď je pokrytých **95 %** (9,5 l) jeho plochy; zvyšok sa dokončí samo.
- Za každé hotové políčko je odmena **10 kreditov**, raz.
- Každé políčko má tri stavy zeme: voľné, **surové** (asfalt je rozložený, ale nie uhladený) a **hotové**. Surový asfalt sa samotný nikdy nepočíta ako hotový: dokončí ho až hladidlo alebo finišer.
- Liať sa dá len na voľnú zem. Na surový ani hotový asfalt sa neleje.

### 4.2 Nádoby v ruke

Hráč drží vždy jednu „ruku" a prepína ich klávesmi (kapitola 5).

| Ruka | Kapacita | Poznámka |
|---|---|---|
| Miska (Bowl) | 2,5 l | štart; berie sa lyžicou po 0,25 l |
| Lopatka (Scoop) | 1 l | kúpi sa za 10; záber 1 l |
| Vedro (Bucket) | 10 l | kúpi sa za 35; vylieva sa naraz do políčka pod zámerným bodom |
| Hladidlo (Trowel) | – | štart; nenesie asfalt, hladí surový asfalt na hotový |

- Naberanie: ľavé tlačidlo pri koryte opakuje záber každých 0,5 s.
- Liatie z misky a lopatky: ľavé tlačidlo, 1 l za sekundu, kruh s polomerom 15 cm (miska) alebo 10 cm (lopatka). Z ruky sa leje rovno **hotový** asfalt.
- Vedro sa plní v ruke zo zásoby: z kohútika stojaceho kufra alebo vozíka, z výpustu výrobne alebo z presuvnej výdajne.
- Výmena ruky trvá 1,5 s, po odomknutí rýchlej výmeny 0 s.
- Dosah ruky je 250 cm.

### 4.3 Surový pás

Surový asfalt kladú dva predmety:

- **Kufor** (Suitcase): hráč ho potiahne za sebou (stojí 62 cm za ním, naklonený o 25°), postaví sa a podržaním klávesu `F` otvorí klapku, ktorá sype 2 l/s do políčka pred ním. Zásoba je 60 l.
- **Vozík** (Cart): hráč ho drží pred sebou, podrží ľavé tlačidlo a tlačí ho; kladie surový pás široký 100 cm, kým sa vozík hýbe rýchlosťou do 200 cm/s. Zásoba je 200 l.

Oba slúžia aj ako zásoba pre vedro (kohútik, 5 l/s).

### 4.4 Uhladenie

- **Hladidlo** (ruka 4): ľavé tlačidlo uhladí surový asfalt v pruhu širokom **40 cm**. Bod dotyku začína tam, kam hráč mieri, a sleduje zámerný bod rýchlosťou najviac 200 cm/s.
- **Finišer** (Finisher): hráč ho tlačí pred sebou (165 cm pred sebou) a uhladí pás široký 100 cm, kým drží ľavé tlačidlo. Bez tlačidla sa len tlačí. Rýchlosť uhladzovania je 85 políčok za minútu (asi 142 cm/s).
- Uhladenie je zadarmo, nestojí asfalt ani kredity. Keď sa políčko dokončí, zvyšok sa uhladí samo.

### 4.5 Výrobňa a zásoba

- Výrobňa vyrába asfalt do zásobníka. Zásobník má kapacitu **4 minúty výroby** a na štarte je v ňom 80 l.
- Úrovne výrobne (litre za minútu): **150 / 300 / 750 / 2 000 / 4 200 / 5 200**. Ceny vylepšení: 100 / 400 / 1 400 / 8 000 / 24 000 kreditov.
- Na úrovniach 2 a 4 pribudne k výrobni viditeľná prístavba.
- Naberanie z koryta a plnenie vedra ťahá zo zásobníka. Keď je prázdny, nič sa nenabere.

### 4.6 Stroje

- **Presuvná výdajňa** (Station, 900 kreditov, od 350 políčok): hráč ju ťahá za sebou (105 cm) a vedro, kufor aj vozík sa pri nej plnia ako pri výrobni.
- **Finišer** (1 400, od 350 políčok): kapitola 4.4.
- **Robot** (AutoPaver, 3 000, od 800 políčok): sám asfaltuje pridelený sektor. Hráč sa postaví do sektora a stlačí `G`. Robot ide k prvému nedokončenému políčku a kladie hotový asfalt rýchlosťou 120 políčok za minútu, pričom čerpá zo zásobníka výrobne (v kuse najviac 10 l). Vylepšenia: **AutoPaverFast** (9 000, 170 políčok/min) a **AutoPaverFaster** (22 000, 220 políčok/min). Pridelený sektor má na zemi oranžový obrys (10 cm). Ak hráč stlačí `G` v sektore, kde robot už je, zobrazí sa „Paver is already here".
- **Auto** (Car, 12 000, od 6 000 políčok): hráč doň nastúpi (`E`) a jazdí. Auto nemá nádrž; leje hotový asfalt zo zásobníka výrobne, kým hráč drží ľavé tlačidlo, pásom širokým 300 cm (260 políčok/min). **CarWideBar** (24 000) rozšíri pás na 350 cm (300 políčok/min). Pri liatí sa rýchlosť auta obmedzí tak, aby sa nelialo viac, než sa stíha.

## 5. Ovládanie

| Vstup | Čo robí |
|---|---|
| `W` `A` `S` `D` | chôdza; v aute `W`/`S` plyn a spiatočka, `A`/`D` zatáčanie |
| Myš | pohľad; v aute otáča hlavou (±100° do strán, −60° až +45° hore/dole) |
| Ľavé tlačidlo myši | použiť ruku: nabrať, liať, vyliať vedro, hladiť; podržané s vozíkom kladie pás, s finišerom hladí, v aute leje |
| Pravé tlačidlo myši (držať) | pracovný pohľad: zorné pole 90° → 60°, naklonenie o 15°, prechod 0,2 s |
| `Q` | ďalšia ruka |
| `1` `2` `3` `4` | miska / lopatka / vedro / hladidlo |
| `E` | chytiť predmet, pustiť ho, nastúpiť do auta alebo vystúpiť |
| `F` (držať) | klapka kufra |
| `G` | poslať robota do sektora, v ktorom hráč stojí |
| Ľavý `Shift` | šprint (po odomknutí) |
| `Tab` | obchod |
| `V` (držať) | kontrolný pohľad (nie počas jazdy) |
| `Esc` | zavrie obchod, inak otvorí pauzu; obrazovka výhry ho ignoruje |

- Chôdza je 450 cm/s, šprint 1,6-krát rýchlejší. Kufor sa ťahá rýchlosťou 350 cm/s, vozík sa tlačí 400 cm/s, finišer 400 cm/s a výdajňa sa nesie 400 cm/s.
- Počas jazdy fungujú chôdza (plyn, zatáčanie), myš, ľavé tlačidlo, `E`, `Tab` a `Esc`. Klávesy rúk, `G`, `F`, pracovný pohľad a `V` nerobia nič.
- Ak hráč niečo drží, `E` patrí držanému predmetu (pustí ho). Do auta sa nastupuje len s voľnými rukami.
- Kamera je v kabíne auta vo výške 190 cm, 120 cm pred stredom auta, v pokoji sklonená o 8° nadol. Karoséria modelu je z pohľadu z kabíny skrytá (jej sklá by zvnútra zakryli výhľad), preto je dole v obraze plochá žltá kapota; zvonku je auto celé.
- Kontrolný pohľad (`V`) zobrazí šípku k najbližšiemu nedokončenému políčku.

## 6. Obrazovka (HUD)

- **Vľavo hore:** kredity.
- **Vpravo hore:** „x / 30,000 · n %" hotových políčok s pruhom; pod ním „Next: <krok> – entry k / 12, cells a / b", teda ďalší odomykaný krok, koľko z 12 políčok jeho vstupného úseku je hotových a koľko hotových políčok z potrebných je celkovo. Ak je robot pridelený: „Paver: sector N · k / 400", inak „Paver: waiting – press G".
- **Vpravo dole:** názov ruky a hladina nádoby (napríklad „Bowl 0 / 2.5 l").
- **V strede dole:** výzva, čo robí `E` pri predmete.
- **Oznamy (toasty):** najviac 3 naraz, každý 2 s plus 0,5 s dobehu. Kredity sa spájajú do jedného bežiaceho súčtu („+40 credits"). Pri odomknutí: „Unlocked: X" a pod tým účinok kroku.
- **Tabule pri vstupoch:** názov kroku, „Entry k / 12" (hotové políčka úseku) a „Cells a / b" (hotové políčka z potrebných). Kiosky stoja pri vstupoch 1, 4, 5 a 6; vstupy 2 a 3 majú tabuľu na nákupnom centre.
- **Obchod (`Tab`):** riadky sú výrobňa (najprv), potom predmety; kúpené riadky idú na koniec. Zamknuté riadky ukazujú podmienku, napríklad „350 cells + entry 3". Poradie sa určí pri otvorení obchodu. Každý nákup sa ukladá.
- **Pauza a menu:** tlačidlá Continue, Resume, New Game, Quit, Yes, No.
- **Výhra:** obrazovka „Parking lot complete!" so riadkami Time, Cells paved, Asphalt poured, Credits earned, Items bought a tlačidlami New Game a Quit.

## 7. Obchod a odomykanie

Predmety (ceny v kreditoch):

| Predmet | Cena | Potrebuje |
|---|---|---|
| Scoop (lopatka) | 10 | – |
| Bucket (vedro) | 35 | – |
| Suitcase (kufor) | 110 | – |
| Cart (vozík) | 340 | – |
| Station (výdajňa) | 900 | odomknutie StripWork |
| Finisher | 1 400 | odomknutie StripWork |
| AutoPaver | 3 000 | odomknutie AutoPaver |
| AutoPaverFast | 9 000 | AutoPaver |
| AutoPaverFaster | 22 000 | AutoPaverFast |
| Car | 12 000 | odomknutie Car |
| CarWideBar | 24 000 | Car |

Kroky sa odomykajú, keď je hotových dosť políčok a zároveň je celý príslušný vstupný úsek (12 políčok):

| Vstup | Krok | Políčok |
|---|---|---|
| 1 | QuickSwap (výmena ruky 0 s) | 10 |
| 2 | Sprint | 100 |
| 3 | StripWork (výdajňa a finišer) | 350 |
| 4 | AutoPaver | 800 |
| 5 | Car | 6 000 |
| 6 | Exit | 30 000 |

Spolu je v obchode 16 vecí: 5 úrovní výrobne a 11 predmetov.

## 8. Ukladanie

- Hra ukladá do jedného slotu s kópiou. Ukladá sa každých 30 s hry, po každom nákupe, po výhre a pri ukončení hry.
- Uložia sa kredity, čas, výrobňa, nádoby, kúpené veci, polohy predmetov, auto, robot a celá zem. Ak hráč v aute uloží, po načítaní sedí v aute.
- Predmet držaný v ruke sa uloží položený na miesto, kde je.
- Poškodený slot hra odmietne a použije zálohu; ak sú poškodené obe, začne novú hru.
- Uloženie z predchádzajúcej verzie hry (v1) sa odmietne.

## 9. Dĺžka hry (simulácia)

Test `DoItAsphalt.Lot.BalanceSim` hrá hru vopred naprogramovaným správaním a meria **simulovaný** čas, nie skutočnú hru. V jednom behu vyšlo:

| Míľnik | Simulovaný čas |
|---|---|
| prvé políčko | 0:42 |
| 10 políčok | 2:39 |
| 100 políčok | 8:07 |
| 350 políčok | 22:34 |
| 800 políčok | 41:17 (limit testu 42:00) |
| 6 000 políčok | 1:13:07 |
| všetkých 30 000 políčok (otvorí sa východ) | 2:21:27 |

Auto sa v simulácii kupuje v čase 1:15:06, široký pás o 1:19:58. Hráč ručne hrá pomalšie alebo rýchlejšie než simulácia; skutočná dĺžka hry sa celá nehrala.

## 10. Zmeny oproti návrhu

Kde sa hotová hra líši od `04-plan-zmien-asphalt-self.md`:

| Čo | Návrh | Hra | Prečo |
|---|---|---|---|
| Šírka hladidla | 30 cm | **40 cm** | doladené pri skúšaní hry |
| Záber lyžice | 0,6 s | **0,5 s** | doladené pri skúšaní hry |
| Tlačenie vozíka | 250 cm/s | **400 cm/s** | doladené pri skúšaní hry |
| Kladenie pásu vozíkom | 100 cm/s | **200 cm/s** | doladené pri skúšaní hry |
| Prístavby výrobne | úrovne 3 a 5 | **úrovne 2 a 4** | tak je to nastavené v kóde |
| Obrys sektora robota | v návrhu nebol | **oranžový, 10 cm** | hráč vidí, v ktorom sektore robot pracuje |
| Tabuľa pri vstupe | názov kroku a účinok | **názov kroku, vstup, počet políčok**; účinok je v oznamoch | tabuľa sa musí zmestiť |

Rýchlosti hladidla, finišera a výdajne majú v kóde vlastné pomenované hodnoty (`TrowelSpeedCmS`, `FinisherPushCmS`, `StationCarryCmS`).

## 11. Cheaty

Cheaty sú konzolové príkazy `dia.*`. Existujú len mimo Shipping buildu (v Shipping binárke sa nenachádza žiadny z 27 reťazcov). Spúšťajú sa v konzole editora alebo cez `-ExecCmds` (skript `Scripts/shot.sh`).

| Príkaz | Čo robí |
|---|---|
| `dia.Give <kredity>` | pridá kredity |
| `dia.Complete <n>` | dokončí prvých n nedokončených políčok podľa indexu |
| `dia.CompleteEntry <0..5>` | dokončí vstupný úsek (číslovanie od 0) |
| `dia.Item <názov>` | dá predmet: Scoop, Bucket, Suitcase, Cart, Station, Finisher, AutoPaver, AutoPaverFast, AutoPaverFaster, Car, CarWideBar (`CarWideBar` samotný auto nedá) |
| `dia.Grab`, `dia.Drop` | chytí najbližší predmet, pustí držaný |
| `dia.RawStrip <x> <y> [dĺžka m=4]` | položí surový pás |
| `dia.FinishStrip` | uhladí surové |
| `dia.Place <druh> <x> <y> [yaw]` | dá a postaví kufor, vozík, výdajňu alebo finišer |
| `dia.Plant <0..5>` | nastaví úroveň výrobne |
| `dia.EnterCar` | nastúpi do auta, v aute z neho vystúpi |
| `dia.Teleport <x> <y> [yaw] [pitch]` | presunie hráča (v aute auto) |
| `dia.Prompt`, `dia.Toast` | vypíše výzvu / oznam |
| `dia.SendPaver` | pošle robota do sektora hráča |
| `dia.Save`, `dia.Load`, `dia.NewGame` | uloží, načíta, začne novú hru |
| `dia.SkipMenu` | preskočí hlavné menu |
| `dia.Camera [vzdialenosť] [yaw] [pitch]` | kamera mimo hráča |
| `dia.WorkView <1\|0>`, `dia.CheckView <1\|0>` | zapne alebo vypne pracovný / kontrolný pohľad |
| `dia.Shop` | prepne obchod |
| `dia.Hand <názov>` | zmení ruku |
| `dia.Fill` | naplní ruku |
| `dia.TopView [výška m=260]` | pohľad zhora |
| `dia.ShotAndQuit <sekundy> <názov>` | uloží snímku do `Saved/Screenshots/` a skončí |

## 12. Známe obmedzenia

- V prvej osobe nie sú vidieť ruky ani paže a nie je animácia pohybu do strán.
- Hra nemá zvuk.
- Lištu finišera pred očami zakrýva jeho kryt, takže hráč nevidí, kde presne hladí.
- Lišta auta sa pri plnom vychýlení volantu môže pohybovať o niečo rýchlejšie, než dovoľuje strop liatia.
- Pri liatí z auta hráča nič neupozorní, že zásoba výrobne sa minula.
- Obrys sektora (10 cm) je zďaleka zhora tenký.
- Kým sú zobrazené tri iné oznamy, oznamy o kreditoch čakajú.
- Predmet uložený v čase, keď ho hráč drží, sa uloží na polohe v ruke bez kontroly voľného miesta.
- Valček, dusadlo a lata s vodováhou zo špecifikácie v hre nie sú (otvorená otázka O1 v `04-plan-zmien-asphalt-self.md`).
- Míľnik 800 políčok v simulácii má rezervu pod minútu (41:17 oproti limitu 42:00).
