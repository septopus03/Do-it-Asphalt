# Do it Asphalt – herný dizajn

Verzia 2.1 · 2026-11-24 · popisuje hru Asphalt-self tak, ako je naprogramovaná (vrátane druhej vlny nákupných budov: vylepšenia po robotoch, nový obchod Roboti, mapa na `M`, legenda ovládania)

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
| Cieľová dĺžka | asi 2 h 15 min bola pôvodná cieľová dĺžka; simulácia s tromi robotmi a ich vylepšeniami dáva asi 1 h 48 min (okno testu je 105 – 165 min, pozri kapitolu 9) |
| Vzhľad | parkovisko pred nákupným centrom, tlmené farby, jedna akcentová (oranžová) |

**Jednou vetou:** hráč vyleje asfalt na parkovisko v tvare písmena L (30 000 štvorcov 1 × 1 m), najprv z misky lyžicou, neskôr z vedra, vozíčka, vozíka, robotov a auta; za každý hotový štvorec dostane 10 kreditov, za kredity kupuje lepšie vybavenie a výrobňu a po vyasfaltovaní všetkého sa otvorí východ.

## 2. Mapa

- Parkovisko je štvorec 200 × 200 m bez jedného kvadrantu 100 × 100 m: v ňom stojí nákupné centrum. Zostáva tvar L s 30 000 štvorcami (políčkami) 1 × 1 m.
- Okolo parkoviska je 12 m široký pás s obrubníkom, plotom a vstupnými cestami.
- Políčka sú zoskupené do 75 **sektorov** 20 × 20 políčok (400 políčok v sektore). Sektory sa v hre číslujú od 1 (v súbore kódu od 0).
- Okolo L je **6 vstupných úsekov**, každý široký 12 políčok. Pred každým stojí tabuľa, ktorá hovorí, aký krok odomyká (kapitola 7), a koľko z 12 políčok úseku je hotových („Entry k / 12").
- Výrobňa asfaltu stojí pri štarte, na okraji parkoviska. Pred ňou je koryto (trough) s asfaltom, z ktorého sa nabera. Hráč začína tesne pred nimi a je k nim obrátený.
- Na južnom pruhu pri výrobni stoja **tri nákupné budovy** (kapitola 7): Garáž, Náradie a Roboti. Nakupuje sa len v nich.
- Východ je vo vstupnom úseku 6. Je to brána s ramenom; vpustí hráča na cestu von, až keď je hotové celé parkovisko.

## 3. Herná slučka

1. Hráč nabere asfalt z koryta (alebo z výpustu výrobne) do nádoby v ruke.
2. Vylieva ho na voľné políčka. Zem ukazuje, či je políčko voľné alebo hotové.
3. Keď je vyliate dosť (95 % plochy políčka), políčko sa dokončí, zvyšok sa doleje samo a hráč dostane 10 kreditov.
4. Za kredity kupuje lepšie nádoby a stroje a vylepšuje výrobňu.
5. Počet hotových políčok odomyká ďalšie kroky: rýchla výmena (10), šprint (100), pásové stroje (350), robot (800), auto (6 000) a východ (30 000).
6. Keď je hotových všetkých 30 000 políčok, otvorí sa východ. Prejsť cez bránu znamená výhru.

## 4. Asfaltovanie

### 4.1 Políčko

- Políčko má 1 × 1 m a potrebuje **10 l** asfaltu. Je hotové, keď je pokrytých **95 %** (9,5 l) jeho plochy; zvyšok sa dokončí samo.
- Za každé hotové políčko je odmena **10 kreditov**, raz. Navyše je **1 kredit za každý vyliaty liter** (pripisuje sa hneď pri liatí, nenahrádza odmenu za políčko).
- Každý texel zeme je buď voľný, alebo **hotový**. Asfalt sa nehladí: čo hráč vyleje, je rovno hotové.
- Liať sa dá len na voľnú zem. Na hotový asfalt sa neleje a litre, ktoré by naň padli, sa neplatia.

### 4.1a Tekutý asfalt

Čo sa leje **rukou** (miska, lyžica, vedro, kanister, vaňa), nelegne na zem hneď. Zaplatené litre sa položia ako niekoľko horúcich kôp na prvé texely výlevu a kopy **tečú**:

- Tečie len asfalt nad hrúbkou filmu (1 texel = 0,025 l). Hrúbka sa riadi výškovým rozdielom medzi susednými texelmi, tok má zotrvačnosť, takže kopa sa rozlieva, preleje cez okraj a vlní sa (živšie než skutočná kvapalina).
- Asfalt chladne: teplota klesne na tretinu za 8 s. Čím je chladnejší, tým pomalšie tečie. Pod 12 % teploty zatuhne tam, kde leží (ostatná tenká vrstva sa stratí).
- Voľný texel, ktorý zasiahne dosť asfaltu, sa stane hotovou zemou. Pokrytie teda závisí od toho, kam asfalt steká: pri liatí na hranu políčka vyjde asfalt aj na susedné políčka a políčko sa nemusí hneď dokončiť.
- Litre sa platia pri liatí, nie pri pokrytí. Pokrytie vyjde zhruba na zaplatenú plochu (pri skúške 466 texelov za 400 zaplatených).
- Stroje (vozík, auto, robot) kladú pás **hneď** a rovnomerne; tekutina sa ich netýka.
- Uložená hra drží len zem: asfalt, ktorý ešte tečie, sa pri ukladaní nechá zatuhnúť na kópii hry.

### 4.2 Nádoby v ruke

Hráč drží vždy jednu „ruku" a prepína ich klávesmi (kapitola 5). Všetky štyri sú nádoby; malá naberačka (Scoop) sa z hry odstránila, lebo nedávala zmysel popri miske.

| Ruka | Kapacita | Liatie | Kruh | Cena |
|---|---|---|---|---|
| Miska (Bowl) | 2,5 l | 1 l/s | 15 cm | štart |
| Vedro (Bucket) | 10 l | 4 l/s | 30 cm | 35 |
| Kanister (Canister) | 20 l | 6 l/s | 40 cm | 70 (od šprintu) |
| Vaňa (Tub) | 36 l | 9 l/s | 55 cm | 160 (od šprintu) |

- Misku plní lyžica: **`R` (držať)** pri koryte opakuje záber 0,25 l každých 0,5 s. V obraze lyžica ide zo strany nad miskou do koryta a späť; ruka (rukavica) ide s ňou.
- Vedro, kanister a vaňa sa plnia **pod prúdom**: pri výpuste výrobne, kohútiku stojaceho vozíčka či vozíka alebo pri výdajni (`R` držať). Nádoba sa pri plnení drží vpredu a mierne sa kolíše.
- Liatie: ľavé tlačidlo (držať). Nádoba sa nakloní o prednú hranu a asfalt dopadá ako prúd na bod pod zameriavačom (`PourHand`); rýchlosť a šírka kruhu sú z tabuľky. Zem, ktorá je už pokrytá, nestojí nič, takže čo sa nepoložilo, ostáva v nádobe.
- Miska a lyžica sú v **dvoch rukách**: miska v ľavej, lyžica v pravej.
- Výmena ruky trvá 1,5 s, po odomknutí rýchlej výmeny 0 s.
- Dosah ruky je 250 cm.

### 4.3 Vozíček a vozík

Dva predmety kladú asfalt rovno hotový:

- **Vozíček** (Trolley, 110 kreditov; v návrhu to bol kufor): malý vozík na asfaltovanie, ktorý hráč tlačí pred sebou za rukoväť. Správa sa ako vozík, len je menší: s podržaným ľavým tlačidlom kladie pás široký 45 cm (rýchlosťou do 260 cm/s, bez tlačenia 430 cm/s). Zásoba je 80 l. Kufor a klapka (`F`) z hry zmizli.
- **Vozík** (Cart): hráč ho drží pred sebou, podrží ľavé tlačidlo a tlačí ho; kladie pás široký 100 cm, kým sa vozík hýbe rýchlosťou do 200 cm/s. Zásoba je 200 l. Na políčko dá 9,5 l (prah dokončenia), takže plný vozík pokryje 20 políčok a 10 l mu ostane.

Oba slúžia aj ako zásoba pre vedro (kohútik, 5 l/s). Hráč smie mať v rukách len jeden predmet naraz.

### 4.4 Bez hladenia

Hra nemá hladidlo ani finišer. Asfalt sa nehladí: každý vyliaty asfalt je hotový, len čo na zem dopadne. Políčko sa dokončí pri 95 % pokrytia a zvyšok sa dolepí samo (kapitola 4.1).

### 4.5 Výrobňa a zásoba

- Výrobňa vyrába asfalt do zásobníka. Zásobník má kapacitu **4 minúty výroby** a na štarte je v ňom 80 l.
- Úrovne výrobne (litre za minútu): **150 / 300 / 750 / 2 000 / 4 200 / 5 200**. Ceny vylepšení: 100 / 400 / 1 400 / 8 000 / 24 000 kreditov.
- Na úrovniach 2 (750 l/min) a 4 (4 200 l/min) pribudne k výrobni viditeľná prístavba. Kód a oznam „Plant level N" po nákupe číslujú úrovne od 0 (štart, 150 l/min, je úroveň 0); špecifikácia ich číslovala od 1.
- Naberanie z koryta a plnenie vedra ťahá zo zásobníka. Keď je prázdny, nič sa nenabere.

### 4.6 Stroje a výdajne

**Výdajne.** Hra má dve výdajne asfaltu pre stroje: **pevnú** (bránový stojan na pruhu pod parkoviskom, vľavo od výrobne, `LotLayout::VehicleStationPos`) a **presuvnú** (Station, 900 kreditov, od 350 políčok). Presuvnú hráč ťahá za sebou (105 cm) a postaví kdekoľvek; kým ju drží, neráta sa. Platí „2 výdajne": pevná je od začiatku, druhú treba kúpiť. Výdajňa núti stroje chodiť si po asfalt:

- Vedro, kanister, vaňa, vozíček aj vozík sa pri nej plnia ako pri výpuste výrobne.
- **Auto** aj **robot** majú **nádrž** a plnia ju len pri výdajni, do vzdialenosti 500 cm od jej stredu, rýchlosťou 500 l/s zo zásobníka výrobne.

**Roboti** (AutoPaver, v obchode „Auto Paver 1", 3 000, od 800 políčok; ďalší dvaja sú „Auto Paver 2" za 40 000 a „Auto Paver 3" za 80 000, každý vyžaduje predchádzajúceho): hráč môže vlastniť najviac **troch** (`LotBalance::MaxAutoPavers`), kupujú sa v poradí. Každý má vlastný stav (sektor, krok, poloha, fáza, nádrž) a **vlastné vylepšenia** (pozri nižšie); nový robot začína na rýchlosti 120 políčok/min a s nádržou 2 000 l. Kúpený robot sa objaví v **státí** svojho čísla v stanici robotov (`LotLayout::PaverDock`), otočený k parkovisku. Roboti po kúpe **pracujú sami** (prepínač **AUTO**, jeden pre všetkých, predvolene zapnutý): robot bez sektora vyberie najbližší sektor, v ktorom je nehotové políčko a **ktorý nemá pridelený iný robot**, a vyasfaltuje ho; potom ide na ďalší. Dvaja roboti nikdy nemajú ten istý sektor: ak hráč pridelí robotovi sektor iného robota, ten druhý ho stratí, ostane nečinný a v režime AUTO si nájde iný. Pri výdajni sa môže plniť viac robotov naraz.

Hráč môže robota poslať aj ručne: otvorí **mapu sektorov** (`M`; predtým `G`), vyberie robota prepínačom 1 / 2 / 3 (ukáže len kúpených) a klikne na sektor (75 sektorov, farba hovorí, koľko je hotové); sektor dostane **vybraný** robot. Mapa ukazuje pridelené sektory všetkých robotov, vybraný je zvýraznený (oranžový), ostatní sú biele; legenda dole na mape končí „M / Esc close". `H` pošle vybraného robota do sektora, v ktorom hráč stojí; po štarte je vybraný robot 1. Tlačidlo „AUTO: ON/OFF" vedľa mapy automatiku vypne (roboti dokončia rozrobený sektor a čakajú na hráča). Stĺpec s tlačidlom AUTO, nápovedou a prepínačom robotov stojí vľavo od mapy. Riadok v HUD to hovorí slovami: „Paver: AUTO – no unpaved sector to go to", „Paver: waiting at the station – the plant has no asphalt" a pod.; s jedným robotom sa riadok volá „Paver: …", s viacerými je číslovaný („Paver 1: …", „Paver 2: …") a každý robot má vlastný riadok. Oznamy fungujú rovnako („Paver → sector 12", s viacerými „Paver 2 → sector 12").

Robot potom pracuje v tejto slučke:

1. **ToSector** – ide do sektora (ak má prázdnu nádrž, najprv do výdajne),
2. **Working** – ide po políčkach sektora od začiatku, hotové preskakuje a ďalej kladí na nehotové (120 / 150 / 180 políčok za minútu podľa úrovne rýchlosti tohto robota); asfalt berie z vlastnej nádrže,
3. keď sa nádrž minie: **ToStation** – ide k najbližšej výdajni (od seba) a zastaví sa,
4. **Loading** – plní nádrž (500 l/s) a vyrazí, keď je nádrž plná (2 000 / 3 000 / 4 500 l podľa úrovne kapacity); ak výrobňa uchová menej, než sa do nádrže zmestí (nízke úrovne), vyrazí aj s tým, čo zbehlo, najmenej s 150 l (predtým pri malej výrobni čakal navždy – hlavná príčina, že „robot nefungoval"),
5. vráti sa na rozpracované políčko a pokračuje.

**Vylepšenia robotov.** Staré globálne vylepšenia „Fast Auto Paver" a „Faster Auto Paver" (`AutoPaverFast`, `AutoPaverFaster`; platili pre všetkých robotov naraz) z hry zmizli. Každý robot sa vylepšuje **samostatne** a dvoma smermi, každý má **tri úrovne** (0, 1, 2; teda dva nákupy):

| Vylepšenie | Úroveň 0 → 1 | Úroveň 1 → 2 | Čo mení |
|---|---|---|---|
| Speed (rýchlosť) | 20 000 | 60 000 | políčok za minútu: 120 → 150 → 180 |
| Capacity (kapacita) | 5 000 | 15 000 | nádrž robota: 2 000 → 3 000 → 4 500 l |

(`LotBalance::AutoPaverRateCellsPerMin`, `AutoPaverTankLByLevel`, `PaverUpgradePrice`.) Vylepšenie sa dá kúpiť, až keď hráč robota vlastní, v budove Roboti (kapitola 7). Kúpená rýchlosť platí hneď; kúpená kapacita zväčší nádrž, ale nenaplní ju (doplní sa vo výdajni). Úrovne robota 2 a 3 sa kupujú zvlášť, takže vylepšený robot 1 nerobí nič pre ostatných. Sektor každého robota má na zemi oranžový obrys (10 cm): obrys robota 1 kreslí materiál zeme, obrysy robotov 2 a 3 tvoria štyri tenké oranžové pásy na zemi. Riadok v HUD ukazuje fázu a nádrž.

**Auto** (Car, 12 000, od 6 000 políčok): hráč doň nastúpi (`E`) a jazdí z **pohľadu tretej osoby** (kamera na ramene za autom, myš ju otáča okolo auta; väčšie stupne majú dlhšie rameno a vyšší pivot, takže sú v zábere rovnako veľké). Auto má **nádrž** (6 000 l v stupni 1), z ktorej leje, kým hráč drží ľavé tlačidlo, pásom širokým 300 cm (260 políčok/min). Nádrž sa **plní pri výdajni**: hráč do nej zajde (pevná výdajňa je priejazdná bránka dlhá 8 m) a nádrž sa sama plní, kým tam stojí; HUD ukazuje stav nádrže a „Station: loading", resp. „Tank low – drive to a station". Auto má **štyri stupne**, kupujú sa po jednom ako vylepšenia toho istého auta (stupeň 2 je namiesto bývalej širokej lišty). Každý má vlastný model, väčšiu kolíznu obálku a lištu ďalej za stredom:

| Stupeň | Položka | Cena | Vyžaduje | Políčok/min | Lišta (cm) | Nádrž (l) | Lišta za stredom (cm) | Model | Celý model D × Š × V (cm) | Karoséria D × Š × V (cm) |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Car | 12 000 | odomknutie Car | 260 | 300 | 6 000 | 230 | `SM_PaverCar` | 460 × 300 × 240 | 460 × 220 × 240 |
| 2 | CarT2 („Paver Car II“) | 24 000 | Car | 300 | 360 | 8 000 | 280 | `SM_PaverCar_T2` | 560 × 360 × 270 | 560 × 250 × 270 |
| 3 | CarT3 („Paver Car III“) | 48 000 | CarT2 | 380 | 420 | 12 000 | 340 | `SM_PaverCar_T3` | 680 × 420 × 300 | 680 × 280 × 300 |
| 4 | CarT4 („Paver Car IV“) | 96 000 | CarT3 | 480 | 500 | 18 000 | 410 | `SM_PaverCar_T4` | 820 × 500 × 340 | 820 × 310 × 340 |

Všetky štyri sa odomykajú krokom Car (6 000 políčok). Po kúpe vyššieho stupňa sa auto vymení na mieste; ak by sa väčšia obálka prekrývala so stenou či strojom, auto sa presunie na svoje východisko (`LotLayout::ItemSpawn(Car)`). Kúpené auto sa objaví v **garáži** (na jej počiatku, otočené von z brány, `LotLayout::CarSpawnYawDeg`), takže z nej vyjde cez bránu k výdajni a na parkovisko. Lišta stupňov 3 a 4 je širšia než 4 m jazdný pruh výdajne a nemá kolíziu, takže prechádza stĺpikmi bránky; karoséria (najviac 310 cm) sa zmestí a nakladanie ide podľa polomeru. **Výrobňa ostáva na 5 200 l/min**: stupeň 4 (480 políčok/min = 4 800 l/min) spolu s jedným najlepším robotom (180 políčok/min = 1 800 l/min, spolu 6 600 l/min) ju dokáže predbehnúť, zásobník potom stačí len na krátky čas. Pri liatí sa rýchlosť auta obmedzí tak, aby sa nelialo viac, než sa stíha.

## 5. Ovládanie

| Vstup | Čo robí |
|---|---|
| `W` `A` `S` `D` | chôdza; v aute `W`/`S` plyn a spiatočka, `A`/`D` zatáčanie |
| Myš | pohľad; v aute otáča kamerou okolo auta (−70° až +10° hore/dole, do strán dookola) |
| Ľavé tlačidlo myši | použiť ruku: liať (vedro ako prúd na zameraný bod); podržané s vozíkom kladie pás, v aute leje |
| Pravé tlačidlo myši (držať) | pracovný pohľad: zorné pole 90° → 60°, naklonenie o 15°, prechod 0,2 s |
| `Q` | ďalšia ruka |
| `1` `2` `3` `4` | miska / vedro / kanister / vaňa (len z toho, čo hráč vlastní; vybrať sa dá aj v obchode s náradím) |
| `E` | chytiť predmet, pustiť ho, nastúpiť do auta alebo vystúpiť; pri bode nákupu budovy otvoriť jej obchod, pri otvorenom obchode ho zavrieť |
| `R` (držať) | naberať: z koryta do misky lyžicou, naplniť vedro, kanister či vaňu pri výrobni, kohútiku vozíčka či vozíka a pri výdajni |
| `M` | mapa sektorov robotov (prepínač robota 1 / 2 / 3, klik na sektor ho tam pošle; `M` / `Esc` zavrie); predtým bola na `G`, ktoré teraz nerobí nič |
| `H` | poslať vybraného robota do sektora, v ktorom hráč stojí |
| Ľavý `Shift` | šprint (po odomknutí) |
| `V` (držať) | kontrolný pohľad (nie počas jazdy) |
| `Esc` | zavrie obchod alebo mapu, inak otvorí pauzu; obrazovka výhry ho ignoruje |

- Chôdza je 450 cm/s, šprint 1,6-krát rýchlejší. Vozíček sa tlačí rýchlosťou 430 cm/s, vozík 400 cm/s a výdajňa sa nesie 400 cm/s.
- Počas jazdy fungujú chôdza (plyn, zatáčanie), myš, ľavé tlačidlo, `E` a `Esc`. Klávesy rúk, `R`, pracovný pohľad a `V` nerobia nič; `M` otvorí mapu sektorov aj z auta.
- Ak hráč niečo drží, `E` patrí držanému predmetu (pustí ho). Do auta sa nastupuje len s voľnými rukami. Z auta sa nenakupuje: hráč musí vystúpiť.
- Kamera auta je v tretej osobe: 9 m za pivotom vo výške 170 cm nad stredom auta, sklonená o 16° nadol; myš ňou otáča okolo auta (nevidí cez steny, rameno sa skráti). Auto je vždy celé vidieť.
- Kontrolný pohľad (`V`) zobrazí šípku k najbližšiemu nedokončenému políčku.
- Legenda ovládania dole vľavo na obrazovke (kapitola 6) vypisuje tieto klávesy; je stále zapnutá (vypínač, napríklad `F1`, hra nemá).

## 6. Obrazovka (HUD)

Celé rozhranie (HUD, obchod, menu, nastavenia aj výhra) je štylizované ako žlto-čierne výstražné tabule: žlté alebo čierne zaoblené tabule s tenkou čiarou vo vnútri, text písmom Barlow Condensed veľkými písmenami (výnimkou sú riadky s hodnotou a dôvodom na kartách obchodu, ktoré ostávajú v pôvodnej veľkosti písmen), bez tieňov. Pruhy (šrafovaný pás žltej a čiernej) zdobia panel obchodu, obrazovku výhry a spodok lišty predmetov.

- **Vľavo hore:** žltá tabuľa s kreditmi (čierne číslo) a popisom „credits".
- **V strede obrazovky:** malý zameriavač (biela bodka v tmavom kruhu, bez zmeny); pri otvorenom menu, obchode a obrazovke výhry sa skryje.
- **Vpravo hore:** čierna tabuľa so žltou linkou. Hore biely text „x / 30,000 · n %", pod ním žltý popis „CELLS PAVED" a ukazovateľ postupu (žltý rámik, šrafovaná dráha, žltá výplň). Pod ním riadok ďalšieho kroku: žltý štítok „NEXT" a biely text kroku, napríklad „Quick swap – entry 0 / 12, cells 0 / 10" (ďalší odomykaný krok, koľko z 12 políčok jeho vstupného úseku je hotových a koľko hotových políčok z potrebných je celkovo). Slovo „Next:" na začiatku textu widget odreže, nahrádza ho štítok; po odomknutí všetkých krokov sa riadok skryje. Každý robot má vlastný riadok (s jedným sa volá „Paver", s viacerými „Paver 1", „Paver 2", „Paver 3"). Riadok robota ukazuje fázu: „Paver: waiting – press M for the map", „Paver → station (tank empty) · sector N", „Paver: loading at the station · nádrž", „Paver → sector N · nádrž" a pri práci „Paver: sector N · k / 400 · tank X / Y l" (Y je nádrž tohto robota podľa jeho úrovne kapacity); kým hráč robota nevlastní, riadok sa nezobrazí.
- **Legenda ovládania („CONTROLS") dole vľavo:** čierna tabuľa so žltou linkou (rovnaký vzhľad ako ostatné tabule), nadpis „CONTROLS" žltou a pod ním dvanásť riadkov: žltá klávesová čiapka a biely krátky text. Riadky (kľúče presne ako v `DIAInput.cpp`): `W A S D` move, `Shift` sprint, `Mouse` look, `LMB` scoop / pour, `RMB` work view, `Q` a `1-4` next / pick tool, `R` fill, `E` use / shop, `H` send robot, `M` paver map, `V` check view, `Esc` pause. Je stále zapnutá; skrýva sa v menu (hlavné, pauza, nastavenia), na obrazovke výhry, pri otvorenom obchode a pri mape robotov. V aute ukáže **kratší zoznam** len toho, čo v aute funguje: `W S` throttle, `A D` steer, `Mouse` look, `LMB` pour, `E` get out, `M` paver map, `Esc` pause. Tabuľa sa neprekrýva s lištou predmetov, výzvou, oznamami ani kontrolným pohľadom (overené na snímke 1280 × 720). Zdroj riadkov je jedna tabuľka v `UI/DIAControlsLegend.cpp`; test kontroluje, že každý uvedený kláves je naozaj namapovaný na danú akciu.
- **Oznamy (toasty):** žlté tabule s čiernym textom, najviac 3 naraz, každý 2 s plus 0,5 s dobehu (dobeh priehľadnosti platí pre celú tabuľu). Dlhý oznam sa zalamuje a je na stred. Kredity sa spájajú do jedného bežiaceho súčtu („+40 credits"). Pri odomknutí: „Unlocked: X" a pod tým účinok kroku.
- **Kontrolný pohľad (`V`):** oranžový kosoštvorec (štvorcová tabuľa otočená o 45°) s čiernou šípkou k najbližšiemu nedokončenému políčku, vedľa čierna tabuľa s oranžovou linkou a oranžovým textom.
- **Tabule pri vstupoch (vo svete):** názov kroku, „Entry k / 12" (hotové políčka úseku) a „Cells a / b" (hotové políčka z potrebných). Kiosky stoja pri vstupoch 1, 4, 5 a 6; vstupy 2 a 3 majú tabuľu na nákupnom centre.
- **Dole v strede zhora nadol:** zásoba výrobne (čierna tabuľa, popis „Plant stock" žltou; zobrazí sa, len keď je výrobňa v dosahu), výzva, čo robí `E` pri predmete, riadok ruky, lišta predmetov a pod ňou šrafovaný pás.
- **Výzva** je pre každý riadok žltá štvorcová tabuľa s klávesom a vedľa nej čierna tabuľa s činnosťou (napríklad `E` a „Push"). Riadok bez klávesa má len čiernu tabuľu. Kým je výzva prázdna, nezobrazí sa.
- **Riadok ruky** je čierna tabuľa nad lištou predmetov: názov bielou a obsah žltou (napríklad „Bowl" a „0 / 2.5 L"); platí aj pre držaný vozíček či vozík.
- **Lišta predmetov:** políčka (`1` miska, `2` vedro, `3` kanister, `4` vaňa) s medzerou 10; zobrazia sa **len tie, ktoré hráč vlastí** (miska je vždy; vedro sa objaví až po kúpe). Políčko je čierna tabuľa so žltou linkou, žltým štvorčekom s číslom klávesu, názvom bielou, **obrázkom** predmetu (`/Game/Icons/ICON_*`, vyrobené `Tools/ModelGen/_icons.py`) a (pri nádobách) ukazovateľom naplnenia. Vybrané políčko je opačne: žltá tabuľa s čiernou linkou, čiernym štvorčekom, čiernym názvom a čiernou výplňou. V rukách je vždy len jeden predmet.
- **Obchod (panel budovy):** čierny panel široký 1300, hore s šrafovaným pásom, ktorý otvorí `E` pri bode nákupu niektorej z troch budov (kapitola 7). Nadpis je názov budovy žltou („Tools", „Robots", „Garage"), pod ním biely podnadpis („Hand tools and the plant", „Robots and the portable station", „Trucks") a vpravo žltá tabuľa s kreditmi. Pod tým karty len tejto budovy. Náradie a Garáž majú rovnomernú mriežku po 4 v riadku (karta je vysoká 172, medzera 12), v poradí položiek: pri náradí najprv výrobňa, potom vedro, kanister, vaňa, vozíček a vozík (6 kariet); v garáži štyri stupne auta. **Roboti** majú vlastné rozloženie (10 kariet): štyri stĺpce rovnakej šírky. Stĺpec 0 má hore kartu presuvnej výdajne (Portable Station), zvyšok stĺpca je prázdny. Stĺpce 1 až 3 sú roboti „Auto Paver 1", „Auto Paver 2" a „Auto Paver 3": hore karta robota (výška `ShopRobotCardHeight` 200, lebo hodnota sa zalomí na dva riadky), pod ňou karta **Speed** a karta **Capacity** (výška `ShopUpgradeCardHeight` 156). Karta nekúpeného robota ukazuje hodnoty úrovne 0 („120 cells/min · 2,000 l tank"), cenu a BUY / NO CREDITS / LOCKED s dôvodom („needs Auto Paver 1"); karta kúpeného robota ukazuje jeho **aktuálne** hodnoty (napríklad „150 cells/min · 3,000 l tank") a OWNED. Karty Speed a Capacity ukážu cenu ďalšej úrovne a text ako „120 → 150 cells/min" či „2,000 → 3,000 l tank"; na najvyššej úrovni je len aktuálna hodnota („180 cells/min", „4,500 l tank") a OWNED. Kým robota hráč nevlastní, **ich miesto je rezervované** (nevidno ich, ale panel sa nemení ani neposúva) a stojí tam tlmený text „Upgrades open once the robot is bought". Panel sa tak nemení veľkosť, keď niečo kúpiš, a na 1280 × 720 sa zmestí s asi 95 px voľnými hore aj dole. Nákup vylepšenia ohlási jeden oznam („Paver 2: speed 150 cells/min", „Paver 2: tank 3,000 l"; s jedným robotom „Paver: …"). Dole stojí „E / Esc - close" (s obyčajnou pomlčkou). Okolo panela sa obraz nestmavuje a hra ostáva klikateľná. Každý nákup sa ukladá. Pás **„IN HAND"** nad mriežkou (tlačidlo s obrázkom a názvom pre každý vlastnený predmet do rúk: miska, vedro, kanister, vaňa; použitý je žltý, kliknutím sa vyberie iný, nejde to, kým hráč drží vozík alebo je v aute) je len v paneli budovy s náradím. Panel sa zavrie klávesom `E`, `Esc`, keď hráč odíde od bodu nákupu ďalej než 300 cm (`DIAShopPoint::CloseRangeCm`), keď nastúpi do auta, alebo pri novej hre.
- **Výzva pri budove:** keď hráč stojí pešo pri bode nákupu, výzva ukáže „E – Tools", „E – Robots" alebo „E – Garage".
- **Karta obchodu** je tabuľa, ktorej vzhľad závisí od stavu. Vľavo hore je dlaždica (kruh s prvým písmenom názvu), vedľa nej názov, čo vec dáva a prípadná podmienka odomknutia (žltou, napríklad „350 cells + entry 3"), dole vľavo cena a vpravo tlačidlo alebo štítok:

| Stav | Tabuľa | Linka | Dlaždica | Cena | Vpravo dole |
|---|---|---|---|---|---|
| Dá sa kúpiť | žltá | čierna | čierna, písmeno žlté | čierna | čierne tlačidlo „BUY" so žltým textom |
| Chýbajú kredity | tmavosivá | biela (45 %) | biela, písmeno čierne | svetločervená | vypnuté tlačidlo „BUY" s tenkým obrysom |
| Zamknuté | takmer čierna | biela (18 %) | biela (22 %), písmeno biele | biela (70 %) | vypnuté tlačidlo „LOCKED" s tenkým obrysom |
| Vlastnené | čierna | biela | biela, písmeno čierne | bez ceny | biely štítok „OWNED" s fajkou |

  Kúpená vec **ostáva na svojom mieste** a len sa zmení na čiernu kartu so štítkom OWNED. Farba dlaždice nezávisí od druhu predmetu.
- **Pauza a menu:** žltá tabuľa na stmavenom pozadí s čiernym nadpisom („Do it Asphalt", pri pauze „Paused", v nastaveniach „Settings") a čiernymi tlačidlami s bielym textom. Prvé viditeľné tlačidlo (Continue alebo Resume, bez uloženej hry New Game) má text žltý. V hlavnom menu je mimo Shipping buildu aj tlačidlo Dev Start. Pod kurzorom je tlačidlo svetlejšie, stlačené čierne. Otázka „Start over? Progress will be lost." nahradí nadpis (čierne písmo, na stred) a tlačidlá Yes (žlté) a No stoja vedľa seba.
- **Nastavenia** (`Esc` ich zavrie) sú na rovnakej žltej tabuli a posúvajú sa, ak sa nezmestia. Riadky: *Mouse sensitivity* (0,1 – 3,0, štandardne 1,0), *Invert Y* (prepínač), *Field of view* (70 – 110°, štandardne 90°; pracovný pohľad a auto ho sledujú), *Master / Effects / Ambience volume* (0 – 100 %; štandardne 80 / 100 / 70 %), *Fullscreen*, *V-Sync* a *Quality* (Low / Medium / High / Epic, tlačidlo prepína dookola, štandardne High). Dole je tlačidlo *Defaults* a *Back*. Hodnota sa mení hneď, ukladá sa pri pustení posuvníka a pri odchode z nastavení do sekcie `DoItAsphalt.Settings` súboru `GameUserSettings.ini`, nie do uloženej hry. Okno, V-Sync, kvalita a hlasitosť sa odovzdávajú enginu len v hre (nie v editore a nie v testoch). Tlačidlá majú zvuk kliknutia.
- **Výhra:** žltá tabuľa s hore šrafovaným pásom (čierna a žltá) a čiernym nadpisom „Parking lot complete!". Pod ním riadky Time, Cells paved, Asphalt poured, Credits earned a Items bought: čierny popis vľavo, hodnota vpravo, každý riadok nad čiernou čiarou. Pod poslednou čiarou sú tlačidlá New Game (text žltý) a Quit.

## 7. Obchod a odomykanie

Obchod je rozdelený do **troch budov** na južnom pruhu pod parkoviskom (každá je aktér `ADIAShopBuilding` s modelom, kolíziou a tabuľou s názvom). Kupuje sa len v nich: hráč stojí pešo pri **bode nákupu** budovy, výzva ukáže „E – …" a `E` otvorí panel s kartami jej tovaru (kapitola 6). Každá vec patrí práve jednej budove (`LotBalance::ItemShop`, `PlantShop`):

| Budova | Model | Poloha (cm) | Otočenie | Predáva |
|---|---|---|---|---|
| Garáž (Garage) | `SM_Garage` | (−3 000; −10 450) | 0° (predná strana k východu, k výdajni) | Car, CarT2, CarT3, CarT4 |
| Náradie (Tools) | `SM_ToolShop` | (900; −10 500) | 90° (k parkovisku) | Bucket, Canister, Tub, Trolley, Cart a úrovne výrobne |
| Roboti (Robots) | `SM_PaverDepot` | (1 800; −10 550) | 90° (k parkovisku) | Station, AutoPaver, AutoPaver2, AutoPaver3 a ich vylepšenia Speed a Capacity (po dve na robota) |

Bod nákupu je Náradie pred pultom, Roboti pred ovládacím pultom a Garáž pri termináli na vnútornej strane pravej steny hneď za bránou (`LotLayout::ShopPoint`, pri garáži (−2 570; −10 155)). Garáž je vjazdná: auto do nej vojde. Os jej státia leží na osi jazdnej dráhy pevnej výdajne (Y −10 450), takže auto vyjde z garáže rovno cez bránku výdajne a nezachytí jej stĺpiky. Auto sa po kúpe objaví v nej a robot v státí.

Predmety (ceny v kreditoch):

| Predmet | Cena | Potrebuje |
|---|---|---|
| Bucket (vedro) | 35 | – |
| Canister (kanister) | 70 | – |
| Tub (vaňa) | 160 | odomknutie Sprint |
| Trolley (vozíček) | 110 | – |
| Cart (vozík) | 340 | – |
| Station (výdajňa) | 900 | odomknutie StripWork |
| AutoPaver („Auto Paver 1") | 3 000 | odomknutie AutoPaver |
| AutoPaver2 („Auto Paver 2") | 40 000 | AutoPaver |
| AutoPaver3 („Auto Paver 3") | 80 000 | AutoPaver2 |
| Car | 12 000 | odomknutie Car |
| CarT2 | 24 000 | Car |
| CarT3 | 48 000 | CarT2 |
| CarT4 | 96 000 | CarT3 |

Kroky sa odomykajú, keď je hotových dosť políčok a zároveň je celý príslušný vstupný úsek (12 políčok):

| Vstup | Krok | Políčok |
|---|---|---|
| 1 | QuickSwap (výmena ruky 0 s) | 10 |
| 2 | Sprint | 100 |
| 3 | StripWork (výdajňa) | 350 |
| 4 | AutoPaver | 800 |
| 5 | Car | 6 000 |
| 6 | Exit | 30 000 |

Vylepšenia robotov nie sú predmety (`ELotItem`), ale úrovne robota; cenník je v `LotBalance::PaverUpgradePrice` (tabuľka v kapitole 4.6). Vyžadujú, aby hráč robota, ktorého sa týkajú, vlastnil.

Spolu je v obchodoch 18 vecí: 5 úrovní výrobne a 13 predmetov, a k tomu 6 vylepšení robotov (Speed a Capacity pre troch robotov). Kariet je 20 (úrovne výrobne sú jedna karta, ktorá ukazuje tú ďalšiu): Náradie 6, Roboti 10 (výdajňa, traja roboti a šesť vylepšení), Garáž 4. Celý obchod stojí 304 615 kreditov za predmety, 300 000 za vylepšenia robotov (3 × 100 000) a 33 900 za úrovne výrobne, spolu asi 638 500. To je viac, než hra vyplatí (simulovaný hráč zarobí za celú hru asi 585 000): kúpi všetko okrem najvyššieho stupňa auta (CarT4, 96 000), takže hráč sa musí rozhodovať, čo kúpi skôr.

## 8. Ukladanie

- Hra ukladá do jedného slotu s kópiou. Ukladá sa každých 30 s hry, po každom nákupe, po výhre a pri ukončení hry.
- Uložia sa kredity, čas, výrobňa, nádoby, kúpené veci, polohy predmetov, auto, traja roboti a celá zem. Ak hráč v aute uloží, po načítaní sedí v aute.
- Predmet držaný v ruke sa uloží položený na mieste, kde je, ale len ak by sa tam dal položiť (rovnaká kontrola ako pri pustení). Inak sa uloží na poslednom platnom mieste: kde bol naposledy položený, alebo kde sa objavil po kúpe.
- Poškodený slot hra odmietne a použije zálohu; ak sú poškodené obe, začne novú hru.
- Formát uloženia je verzia 7 (zem je bitová mapa hotových políčok a rozpracované políčka; podrobnosti v `02-technicky-navrh.md`, kapitola 6). Verzia 7 ukladá pri každom robotovi aj úroveň rýchlosti a kapacity. Načítajú sa aj uloženia vo verziách 4, 5 a 6. Staré globálne vylepšenia (`AutoPaverFast`, `AutoPaverFaster`) sa pri načítaní zmenia na úroveň rýchlosti 1, resp. 2 **každého vlastneného robota** (rýchlosť bola predtým spoločná, takže hra ostane taká rýchla, aká bola); kapacita starých uložení je úroveň 0. Uloženia vo verziách 4 a 5 poznali len jedného robota: ten sa stane robotom 1 a ostatní nie sú kúpení. Uloženie vo formáte v1, ktorý poznal surový asfalt, sa odmietne a treba začať novú hru.

## 9. Dĺžka hry (simulácia)

Test `DoItAsphalt.Lot.BalanceSim` hrá hru vopred naprogramovaným správaním a meria **simulovaný** čas, nie skutočnú hru. Simulovaný hráč kupuje vždy to najlacnejšie, na čo má kredity (predmety, vylepšenia každého robota a výrobňu); roboti 2 a 3 aj všetky vylepšenia robotov sa kupujú. Robot 1 dostáva sektory od hráča, roboti 2 a 3 pracujú v automatickom režime. V jednom behu vyšlo:

| Míľnik | Simulovaný čas |
|---|---|
| prvé políčko | 0:42 |
| 10 políčok | 1:55 |
| 100 políčok | 6:34 |
| 350 políčok | 13:33 |
| 800 políčok (odomkne sa robot) | 23:04 (limit testu 42:00) |
| 6 000 políčok (odomkne sa auto) | 52:34 (limit 78:00) |
| všetkých 30 000 políčok (otvorí sa východ) | 1:48:24 (limit 1:45 – 2:45) |

Kúpy robotov v tom istom behu: Auto Paver 1 o 23:05, Auto Paver 2 o 52:59 a Auto Paver 3 o 1:35:22. Vylepšenia prvého robota prídu medzi 24. a 35. minútou, robota 2 medzi 54. a 61. minútou a pri 1:25, robota 3 medzi 1:36 a 1:44. Auto sa kúpi o 56:09, stupne II a III o 1:04:36 a 1:10:44. Najvyšší stupeň auta (CarT4, 96 000) simulovaný hráč pred východom nekúpi (zostane mu 42 485 kreditov); cena celého obchodu je vyššia než to, čo hra za celý čas vyplatí, takže sa treba rozhodovať.

Ako sa k tomu prišlo: s pôvodnými cenami (roboti 2 a 3 za 6 000 a 12 000, rýchlosť 9 000 a 22 000, kapacita 4 000 a 11 000, rýchlosti 120 / 170 / 220) dával simulovaný hráč s tromi robotmi východ už o 1:26:02, pod oknom 105 – 165 min. Preto sa cena robota 2 zvýšila na 40 000, robota 3 na 80 000, ceny rýchlosti na 20 000 a 60 000, kapacity na 5 000 a 15 000 a rýchlosť úrovní 1 a 2 klesla zo 170 / 220 na 150 / 180 políčok/min (najúčinnejšia jednotlivá úprava: asi 8 minút). Skoršie míľniky ostali nedotknuté. Pre porovnanie: experiment s jedným robotom, všetkými jeho vylepšeniami a staršími rýchlosťami 170 / 220 dal asi 2 h 02 min; každý ďalší robot teda hru skracuje.

**Čo to znamená.** Východ vychádza o 1:48:24, teda len asi 3 minúty nad dolným okrajom okna (105 min); cieľ 120 – 140 min sa s troma robotmi nedal dosiahnuť bez toho, aby robot 3 prišiel tak neskoro, že by nič nerobil. Druhá polovica hry je riadená kreditmi a nakoniec stropom výrobne (úroveň 5, 5 200 l/min = 520 políčok/min); preto sa čas východu hýbe o asi 10 s na každých 1 000 kreditov celkovej ceny a každá ďalšia zmena ceny ho posunie (test okno hlása). Predchádzajúce vyhlásenia „hra trvá asi 2 h 20 min", „simulácia kupuje len jedného robota" a „tempo s tromi robotmi je otvorená otázka" už neplatia. Čas ručnej hry je iný: človek hrá pomalšie alebo rýchlejšie než simulácia a skutočná dĺžka hry sa celá nehrala. Simulácia počíta liatie rukou ako okamžité, takže tekutinu nemeria. Kapacita nádrže skráti čas pri výdajni asi o 34 % (test `Lot.AutoPaver.CapacitySavesStationTime`: 79 s → 52 s, o 5 % viac políčok za 20 min), takže je skôr pohodlie než skutočné zrýchlenie; preto je lacnejšia než rýchlosť.

## 10. Zmeny oproti návrhu

Kde sa hotová hra líši od `04-plan-zmien-asphalt-self.md`:

| Čo | Návrh | Hra | Prečo |
|---|---|---|---|
| Hladidlo a finišer | v návrhu boli | **v hre nie sú** | asfalt sa nehladí, vyliaty je rovno hotový |
| Záber lyžice | 0,6 s | **0,5 s** | doladené pri skúšaní hry |
| Tlačenie vozíka | 250 cm/s | **400 cm/s** | doladené pri skúšaní hry |
| Kladenie pásu vozíkom | 100 cm/s | **200 cm/s** | doladené pri skúšaní hry |
| Obrys sektora robota | v návrhu nebol | **oranžový, 10 cm** | hráč vidí, v ktorom sektore robot pracuje |
| Tabuľa pri vstupe | názov kroku a účinok | **názov kroku, vstup, počet políčok**; účinok je v oznamoch | tabuľa sa musí zmestiť |
| Obchod | jeden obchod na `Tab` | **tri budovy** s bodom nákupu a klávesom `E`; `Tab` nerobí nič | nákup má miesto vo svete (`04-plan-zmien-asphalt-self.md` ešte hovorí o obchode na `Tab`) |
| Mapa robotov | `G` (stav pred druhou vlnou) | **`M`** | `G` stojí ešte v špecke nákupných budov a v starších textoch |
| Vylepšenia robota | jedno vylepšenie rýchlosti pre všetkých robotov (9 000 a 22 000) | **každý robot má vlastnú rýchlosť (120 / 150 / 180) a kapacitu (2 000 / 3 000 / 4 500 l)**, kupované v budove Roboti | roboti 2 a 3 dostanú vlastný vývoj; ceny pre tempo hry (kapitola 9) |
| Nápoveda ovládania | v návrhu nebola | **legenda „CONTROLS" dole vľavo** | hráč nemusí hľadať klávesy |

Rýchlosť výdajne má v kóde vlastnú pomenovanú hodnotu (`StationCarrySpeedCmS`).

Zastaralé zmienky, ktoré sa zámerne neopravili (dokument 4 je história a `.docx` sa nemení): `04-plan-zmien-asphalt-self.md` hovorí o „obchode na Tab" v riadkoch 37, 94 a 170 a v riadku 84 o vylepšeniach automatu za 9 000 (170) a 22 000 (220); v špecke `superpowers/specs/2026-11-23-nakupne-budovy-design.md` stoja pôvodné ceny robotov a globálne zrýchlenia Fast a Faster, mapa na `G` a obchod na `Tab` (oprava je v jej časti „Vlna 2").

## 11. Cheaty

Cheaty sú konzolové príkazy `dia.*`. Je ich 28 a existujú len mimo Shipping buildu. Skoršia kontrola Shipping binárky nenašla žiadny reťazec `dia.*`; urobila sa ešte predtým, než pribudol `dia.Screen`, a odvtedy sa nezopakovala. Všetky cheaty sa kompilujú len mimo Shipping buildu (`#if !UE_BUILD_SHIPPING`). Spúšťajú sa v konzole editora alebo cez `-ExecCmds` (skript `Scripts/shot.sh`).

| Príkaz | Čo robí |
|---|---|
| `dia.Give <kredity>` | pridá kredity |
| `dia.Complete <n>` | dokončí prvých n nedokončených políčok podľa indexu |
| `dia.CompleteEntry <0..5>` | dokončí vstupný úsek (číslovanie od 0) |
| `dia.Item <názov>` | dá predmet: Bucket, Canister, Tub, Trolley, Cart, Station, AutoPaver, Car, CarT2, CarT3, CarT4, AutoPaver2, AutoPaver3 (vylepšenie dá aj všetko pod ním, takže `CarT3` dá Car, CarT2 aj CarT3 a `AutoPaver3` dá troch robotov) |
| `dia.PaverUpgrade <robot 1..3> <Speed\|Capacity> <úroveň 0..2>` | nastaví úroveň rýchlosti alebo kapacity robota; ak robot (alebo jeho predchodcovia) nie je kúpený, dá ho |
| `dia.Grab`, `dia.Drop` | chytí najbližší predmet, pustí držaný |
| `dia.CartStrip <x> <y> [dĺžka m=4]` | dá a naplní vozík a položí ním pás od bodu smerom na +X |
| `dia.Place <druh> <x> <y> [yaw]` | dá a postaví vozíček, vozík alebo výdajňu |
| `dia.Plant <0..5>` | nastaví úroveň výrobne |
| `dia.EnterCar` | nastúpi do auta, v aute z neho vystúpi |
| `dia.Teleport <x> <y> [yaw] [pitch]` | presunie hráča (v aute auto) |
| `dia.Prompt`, `dia.Toast` | vypíše výzvu / oznam |
| `dia.SendPaver` | pošle vybraného robota do sektora hráča |
| `dia.Save`, `dia.Load`, `dia.NewGame` | uloží, načíta, začne novú hru |
| `dia.SkipMenu` | preskočí hlavné menu |
| `dia.Camera [vzdialenosť] [yaw] [pitch]` | kamera mimo hráča |
| `dia.WorkView <1\|0>`, `dia.CheckView <1\|0>` | zapne alebo vypne pracovný / kontrolný pohľad |
| `dia.Shop <Tools\|Robots\|Garage>` | otvorí panel danej budovy, ale len kým hráč stojí v dosahu jej bodu nákupu (najprv `dia.Teleport`); bez názvu panel zavrie |
| `dia.Screen <Pause\|Settings\|Question\|Win\|Map>` | otvorí danú obrazovku (pauzu, nastavenia, otázku, výhru alebo mapu robota), aby sa dala odfotiť; ladiaca pomôcka |
| `dia.Hand <názov>` | zmení ruku |
| `dia.Fill` | naplní ruku |
| `dia.TopView [výška m=260]` | pohľad zhora |
| `dia.ShotAndQuit <sekundy> <názov>` | uloží snímku do `Saved/Screenshots/` a skončí |

## 12. Známe obmedzenia

- V prvej osobe nie je animácia pohybu do strán.
- Zvuky sú syntetizované skriptom (`Tools/Audio/gen_sounds.py`), nie nahrávky; hlasitosť a zmiešanie nikto nepočul. Hrajú: okolie, hum výrobne, motor auta (výška a hlasitosť podľa rýchlosti), liatie, plnenie pod prúdom a lyžica (len kým sa litre naozaj presúvajú: prázdna nádoba alebo nádrž hneď stíchne), kroky podľa prejdenej vzdialenosti, hluk pri chytení, pustení a nastupovaní, čerpadlo výdajne, motor robota a zvuky nákupu, odomknutia a výhry.
- Animácie naberania a liatia (naklonenie nádoby o prednú hranu, kolísanie pod prúdom, pohyb lyžice do koryta a späť) majú pózy nastavené bez ladenia podľa snímok.
- Uložená hra má verziu 7 (traja roboti s vlastnými vylepšeniami); načítajú sa verzie 4, 5 a 6, staršie uloženia sa nenačítajú.
- Legenda ovládania je stále zapnutá; vypínač (napríklad `F1` alebo položka v nastaveniach) hra nemá.
- Čo sa kúpou vylepšenia robota nemení, je jeho vzhľad (väčšia nádrž sa na modeli neukáže).
- Panel robotov, mapu na `M` v aute a legendu v skutočnej hre nikto ručne neskúšal (len snímky a testy; pozri `README.md`, časť Neoverené).
- Lišta auta sa pri plnom vychýlení volantu môže pohybovať o niečo rýchlejšie, než dovoľuje strop liatia.
- Keď sa minie nádrž auta, auto prestane liať; upozorní len riadok „Tank low – drive to a station" pod 10 % nádrže.
- Obrys sektora (10 cm) je zďaleka zhora tenký.
- Kým sú zobrazené tri iné oznamy, oznamy o kreditoch čakajú.
- Valček, dusadlo a lata s vodováhou zo špecifikácie v hre nie sú (otvorená otázka O1 v `04-plan-zmien-asphalt-self.md`).
- Tekutý asfalt je vidieť len ako postupujúce pokrytie zeme (bez vlastnej hrúbky či lesku). Simulácia vyváženia (`LotBalanceSimTest`) počíta s okamžitým liatím, takže vplyv tekutiny na dĺžku hry nemeria; pokrytie vychádza zhruba na zaplatenú plochu, no políčko sa pri nerovnomernom rozliatí dokončuje ťažšie.
- Zameriavač je pevný (nemení sa podľa toho, na čo hráč mieri).
- Roboti sa navzájom ani s budovami nezrážajú (jadro ich vedie po priamke): robot môže pri výjazde zo státia viditeľne prejsť cez stenu stanice alebo cez výrobňu. Roboti sa po práci nevracajú do státí; státia sú len miesto, kde sa po kúpe objavia. Budovy sa nehýbu (žiadna roleta ani dvere).
