# Do it Asphalt – herný dizajn (GDD)

Verzia 1.0 · 2026-11-19 · vychádza z `Do it Asphalt - dokumentacia.docx`

Tento dokument je záväzný popis hry. Všetko, čo pôvodná špecka neurčila, je tu rozhodnuté a zhrnuté v kapitole 13, aby sa to dalo jedným pohľadom skontrolovať a zmeniť.

---

## 1. Prehľad

| | |
|---|---|
| Žáner | inkrementálna simulácia (pokojné „satisfying" asfaltovanie + vylepšovanie) |
| Platforma | PC – macOS (vývoj) a Windows, cieľovo Steam |
| Engine | Unreal Engine 5.8 (C++) |
| Kamera | tretia osoba, kamera za hráčom |
| Hráči | 1 |
| Cieľová dĺžka | 60 – 90 minút na dohranie |
| Vizuál | štylizovaný low-poly, ploché farby, stavenisko za dňa |

**Jednou vetou:** hráč nosí asfalt z výrobne v batohu na chrbte a vylieva ním parkovisko v tvare veľkého šesťuholníka; za vyliaty asfalt dostáva peniaze, za dokončené bunky shardy, a za ne si kupuje väčší batoh, širší nástroj, rýchlejšiu výrobňu a šesť špeciálnych vylepšení na príjazdových cestách – posledným je východ a výhra.

## 2. Herná slučka

1. Výrobňa priebežne vyrába asfalt do zásobníka.
2. Hráč vojde do odbernej zóny výrobne, batoh sa mu naplní.
3. Hráč podrží tlačidlo a pri chôdzi vylieva asfalt na zem v šírke svojho nástroja.
4. Každý vyliaty liter = peniaze. Každá dokončená bunka = shard.
5. Za peniaze a shardy kupuje vylepšenia → nosí viac, leje širšie, výrobňa stíha.
6. Vyasfaltovaná príjazdová cesta odomkne kiosk so špeciálnym vylepšením.
7. Keď je vyasfaltované všetko, otvorí sa východ. Prechod bránou = výhra.

## 3. Mapa

### 3.1 Malý šesťuholník (bunka)

- Plocha presne **1 m²**, hrúbka vrstvy 2 cm → **20 l asfaltu** na bunku (zo špecky).
- Strana 62,04 cm, šírka cez hrany 107,46 cm. Bunky sú orientované hrotom nahor (pointy-top).
- Bunka má na zemi viditeľný obrys, kým nie je zaliata.

### 3.2 Parkovisko

- Veľký šesťuholník s polomerom **12 buniek** okolo stredovej bunky = **469 buniek**.
- Rozmer približne 27 × 24 m.

> Presne 500 buniek sa do pravidelného šesťuholníka poskladať nedá (možné počty sú … 397, 469, 547 …). Zvolených je 469 a spolu s cestami vychádza celok na 499. Viď rozhodnutie R1.

### 3.3 Príjazdové cesty

- Zo stredu každej zo 6 strán veľkého šesťuholníka vybieha jedna cesta.
- Každá cesta má **5 buniek** (rad 2 bunky + rad 3 bunky), spolu 30 buniek.
- Za piatimi bunkami pokračuje už hotová stará cesta ako kulisa až k plotu, kde ju uzatvára zábrana (pri východe brána).
- Bunky ciest sa asfaltujú a odmeňujú rovnako ako bunky parkoviska.

**Spolu: 499 buniek, 9 980 l asfaltu.**

### 3.4 Rozmiestnenie

```
                        cesta 1
                   (auto-asfaltovač)
                           |
                   ________|________
    cesta 2       /                 \       cesta 0
   (dve ruky) ───/                   \─── (asfaltovacie auto)
                /                     \
 [VÝROBŇA] ●   <      PARKOVISKO       >
    štart       \     469 buniek      /
    cesta 3 ─────\                   /─── cesta 5
    (šprint)      \_________________/      (VÝCHOD)
                           |
                        cesta 4
                   (prenosné miesto)
```

- Výrobňa stojí mimo parkoviska pri západnom rohu; hráč začína v jej odbernej zóne.
- Najlacnejšie špeciály sú na cestách najbližšie k výrobni, východ je najďalej.
- Areál je ohradený plotom (hráč nemôže odísť inak než východom).
- Po nevyasfaltovanej zemi sa dá voľne chodiť – hráč si sám volí, čo vyleje skôr.

## 4. Asfaltovanie

- Hráč pred sebou tlačí **asfaltovačku** – rozstrekovaciu lištu na kolieskach, ktorej šírka je daná úrovňou nástroja. Lišta je 80 cm pred hráčom.
- Kým hráč drží tlačidlo liatia a má v batohu asfalt, plocha, cez ktorú lišta prejde, sa zaleje. Pri státí na mieste sa zaleje pás hlboký 10 cm.
- Spotreba je presne úmerná novo zaliatej ploche: 20 l/m². Už zaliata plocha a plocha mimo buniek nespotrebuje nič a nič nezarobí.
- Keď sa batoh minie, liatie sa okamžite zastaví.
- **Bunka je dokončená pri 97 % pokrytia** – zvyšné škáry sa zalejú samy a zadarmo (bez peňazí). Hráč tak nemusí naháňať posledné milimetre.
- Počas liatia hráč kráča pomalšie (2 m/s oproti 4,5 m/s).
- Spätná väzba: asfalt je viditeľne tmavý a lesklý, dokončená bunka krátko blikne a vyskočí „+1 shard".

## 5. Výrobňa asfaltu

- Vyrába nepretržite podľa svojej úrovne (15 → 1000 l/min) do vlastného zásobníka.
- **Kapacita zásobníka = 4 minúty výroby** (základ 60 l). Plný zásobník výrobu zastaví.
- **Odber je automatický:** stačí stáť v odbernej zóne (kruh s polomerom 2 m pri výpuste). Prázdny batoh sa naplní za 1,5 s, ak je v zásobníku dosť.
- Hra začína s prázdnym zásobníkom aj batohom – prvých 20 sekúnd hráč čaká na prvých 5 l (krátky „tutoriálový" moment, HUD ukáže šípku k výrobni).

## 6. Meny

| Mena | Ako sa získava | Na čo je |
|---|---|---|
| **Peniaze** | 1 za každý vyliaty liter | nástroj, výrobňa, špeciálne vylepšenia |
| **Shardy** | 1 za každú dokončenú bunku | batoh |

Maximum v celej hre: 499 shardov a približne 9 700 – 9 980 peňazí (podľa toho, koľko škár dokončí hra sama). Všetky vylepšenia spolu stoja 253 shardov a 5 885 peňazí, takže hráč stihne kúpiť všetko zhruba v 60 % hry a zvyšok si užije na plnom výkone.

## 7. Bežné vylepšenia

Kupujú sa kdekoľvek cez obchod (kláves Tab). Úrovne idú postupne, nedajú sa preskakovať.

### 7.1 Batoh (shardy)

| Úroveň | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Kapacita (l) | 5 | 7 | 10 | 15 | 20 | 30 | 40 | 60 | 80 | 100 | 135 | 150 | 200 |
| Cena (shardy) | – | 1 | 2 | 4 | 6 | 9 | 13 | 18 | 24 | 31 | 39 | 48 | 58 |

Batoh má 4 vizuálne veľkosti: S (5 – 10 l), M (15 – 40 l), L (60 – 100 l), XL (135 – 200 l).

### 7.2 Nástroj na asfaltovanie (peniaze)

| Úroveň | 0 | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|---|
| Šírka (m) | 0,2 | 0,3 | 0,5 | 0,8 | 1,0 | 1,5 | 2,0 |
| Cena (peniaze) | – | 25 | 60 | 120 | 220 | 380 | 600 |

### 7.3 Výrobňa (peniaze)

| Úroveň | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Výroba (l/min) | 15 | 20 | 25 | 35 | 50 | 80 | 100 | 125 | 180 | 300 | 500 | 750 | 1000 |
| Cena (peniaze) | – | 30 | 50 | 75 | 105 | 145 | 190 | 240 | 290 | 335 | 370 | 395 | 405 |

Výrobni pribudne viditeľný prístavok na úrovni 5 a ďalší na úrovni 10.

> Ceny výrobne sú doladené simuláciou celého prechodu hry (test `DoItAsphalt.Core.BalanceSim.Playthrough`): hra trvá približne 50 minút a všetkých 35 vylepšení je kúpených pred dokončením 60 % buniek. Ostatné ceny sú východiskové a po prvom hraní sa budú ladiť. V kóde žijú na jednom mieste (`AsphaltBalance`).

## 8. Špeciálne vylepšenia

Každá cesta má na konci kiosk. Kiosk je zamknutý, kým nie je **všetkých 5 buniek tej cesty** dokončených. Potom sa pri ňom dá špeciál kúpiť (kláves E). Poradie ciest si hráč volí sám.

| Cesta | Špeciál | Cena | Správanie |
|---|---|---|---|
| 3 | **Šprint** | 100 | Držaním Shift beží hráč 1,6× rýchlejšie. Nedá sa počas liatia. |
| 2 | **Dve ruky** | 200 | Hráč tlačí dve asfaltovačky vedľa seba → dvojnásobná šírka liatia. |
| 4 | **Prenosné odberné miesto** | 300 | Nádrž na kolieskach. Hráč ju klávesom E zdvihne a položí kdekoľvek; v okruhu 2 m okolo nej sa batoh plní rovnako ako pri výrobni. Čerpá zo zásobníka hlavnej výrobne. |
| 1 | **Automatizovaný asfaltovač** | 450 | Robot, ktorý sám leje 20 l/min, teda 5 buniek za 5 minút. Vždy si vyberie najbližšiu nedokončenú bunku. Má vlastný asfalt, odmeny idú hráčovi. |
| 0 | **Asfaltovacie auto** | 800 | Vozidlo s nádržou 500 l a lištou širokou 3 m. Nastupuje sa klávesom E, plní sa v odbernej zóne, leje sa rovnakým tlačidlom. |
| 5 | **Východ** | zadarmo | Brána sa otvorí, keď je dokončených všetkých 499 buniek. |

## 9. Výhra

- Podmienka: všetkých 499 buniek dokončených → brána na ceste 5 sa otvorí a HUD to oznámi.
- Hráč prejde (alebo prejde autom) bránou → obrazovka výhry: čas hry, vyliate litre, zarobené peniaze, počet kúpených vylepšení.
- Z obrazovky výhry: „Nová hra" alebo „Koniec". Prehrať sa nedá.

## 10. Ovládanie

| Akcia | Klávesnica + myš |
|---|---|
| Pohyb | W A S D |
| Kamera | myš |
| Liať asfalt | ľavé tlačidlo myši (držať) |
| Šprint (po kúpe) | Shift (držať) |
| Interakcia (kiosk, prenosné miesto, auto) | E |
| Obchod | Tab |
| Pauza / menu | Esc |

V aute: W/S plyn a spiatočka, A/D zatáčanie, ľavé tlačidlo leje, E vystúpiť. Gamepad je mimo rozsahu v1.

## 11. HUD a obrazovky

- **Vľavo hore:** peniaze, shardy.
- **Vpravo hore:** postup „137 / 499 buniek · 27 %".
- **Dole v strede:** ukazovateľ batohu „3,2 / 5 l" (v aute nádrž auta).
- **Pri výrobni / prenosnom mieste:** zásobník „42 / 60 l".
- **Kontextová výzva:** „E – Kúpiť Šprint (100)", „Dokonči cestu (3 / 5)", „E – Nastúpiť".
- **Obchod (Tab):** tri riadky – Batoh, Nástroj, Výrobňa; aktuálna hodnota → ďalšia hodnota, cena, tlačidlo. Hra počas obchodu beží ďalej.
- **Hlavné menu:** Pokračovať (ak existuje uložená hra), Nová hra, Koniec.
- **Pauza (Esc):** Pokračovať, Nová hra, Koniec.
- **Výhra:** štatistiky podľa kapitoly 9.

Texty v hre sú v prvej verzii po anglicky, pripravené na lokalizáciu (viď R9).

## 12. Ukladanie

- Jedna pozícia. Automatické uloženie každých 30 sekúnd, pri každom nákupe a pri ukončení hry.
- Ukladá sa: pokrytie všetkých buniek, peniaze, shardy, úrovne, kúpené špeciály, stav zásobníka, batohu a nádrže auta, čas hry, poloha hráča, prenosného miesta, auta a robota.
- Poškodený alebo starší súbor → hra ponúkne len „Nová hra", nespadne.

## 13. Rozhodnutia nad rámec špecky

Pôvodná špecka tieto veci neurčila. Vľavo otázka, vpravo čo platí, kým nepovieš inak.

| # | Otázka | Rozhodnutie | Prečo |
|---|---|---|---|
| R1 | 500 buniek v tvare šesťuholníka | 469 buniek parkovisko + 30 buniek ciest = 499 | Presných 500 geometricky nejde; alternatíva je polomer 13 = 547 buniek (+17 % hry). |
| R2 | Aká veľká je bunka | 1 m², vrstva 2 cm | 20 l z toho vyjde presne a šírky nástroja 0,2 – 2 m dávajú zmysel voči bunke širokej 1,07 m. |
| R3 | Koľko peňazí a shardov | 1 peniaz za liter, 1 shard za bunku | Najjednoduchšie čitateľné pravidlo. |
| R4 | Za čo sa vylepšuje výrobňa | Za peniaze | Shardov je spolu len 499 a celé ich minie batoh. |
| R5 | Za čo sa kupujú špeciály | Za peniaze, po dokončení cesty | „Po vyasfaltovaní bude možné nakupovať". |
| R6 | Kde sa kupujú bežné vylepšenia | Kdekoľvek cez Tab | Špeciály sú viazané na miesto, bežné nemajú prečo brzdiť. |
| R7 | Prenosné odberné miesto – vyrába samo? | Nie, čerpá zo zásobníka hlavnej výrobne | Účel je skrátiť chodenie; druhá výroba by rozbila balans výrobne. |
| R8 | Automatizovaný asfaltovač – berie asfalt hráčovi? | Nie, má vlastný; odmeny dáva hráčovi | Je to odmena, nie ďalšia starosť. |
| R9 | Jazyk hry | Angličtina, pripravené na lokalizáciu | Názov hry je anglický a cieľom je Steam. |
| R10 | Parametre auta | Nádrž 500 l, lišta 3 m, 6 m/s | Má byť jasne silnejšie než hráč s batohom 200 l a lištou 2 m (4 m s dvoma rukami). |
| R11 | Kedy sa otvorí východ | Až po dokončení všetkých 499 buniek, bez ceny | „Výhra na konci". |
| R12 | Dá sa prehrať? | Nie | Špecka nespomína žiadny tlak ani časovač. |
| R13 | Postava | Epic Mannequin (kostra a animácie zo šablóny UE) + vlastná prilba, batoh a nástroj | Rigovanú animovanú postavu kódom spoľahlivo nevyrobíme; výstroj áno. |

## 14. Mimo rozsahu v1

Zvuk a hudba, gamepad, nastavenia grafiky, viac uložených pozícií, Steam integrácia (achievementy, cloud), lokalizácia do slovenčiny, vlastná animácia tlačenia asfaltovačky (v1 používa bežnú chôdzu Mannequina), Windows build (z Macu sa nedá zostaviť).
