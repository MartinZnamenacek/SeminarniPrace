//////////////////////////// KNIHOVNY A JEJICH UVEDENÍ DO CHODU
#include <Servo.h>        // umožňuje implementaci servomotorů
Servo servomotor[4][3];   // definuje uspořádané pole hodnot: noha-XYZ, tedy 0-012, 1-012, 2-012, 3-012
#include <FlexiTimer2.h>  // časovač pro přesné ovládání servomotorů

//////////////////////////////////////////////////////////////////////////////// POZICE/SOUŘADNICE PRO POHYB
const float x_start = 60, x_krok = 0; x_otoceni = 31.1; x_pretoceni = 62.1;   // startovní pozice pro X & pozice pro krok na X, dále pak pro otáčení a přetáčení
const float y_start = 0, y_krok = 40; y_otoceni = 56.8; y_pretoceni = 20.0;   // startovní pozice pro Y & pozice pro krok na Y, dále pak pro otáčení a přetáčení
const float z_start = -30, z_krok = -50;                                      // startovní pozice pro Z & pozice pro krok na Z
const byte nehybat = 255;                                                     // hodnota, při které se pozice motoru nemění
volatile float soucasna_pozice[4][3], budouci_pozice[4][3];                   // proměnná pro uchovávání: současné pozice dané části (XYZ) určité nohy (0123) & předpokládané pozice, kam se bude daná část (XYZ) určité nohy (0123) hýbat

///////////////////////////////////////////////////////// RYCHLOSTI POHYBU
const int pocet_kroku = 0;                             // nastaví, kolik kroků (pohybů nohou) za 1 sekvenci robot vykoná
const int prodleva = 250;                              // zavede, jakým časovým intervalem budou jednotlivé kroky oddělovány
const float modifikator_rychlosti = 1.1;               // násobič celkové rychlosti - umožnujě obecné zpomalení/zrychlení
const float rychlost_pohybu_nohy = 10;                 // rychlost, kterou se hýbe noha při chůzi
const float rychlost_pohybu_tela = 5;                  // rychlost, kterou se hýbou všechny nohy naráz směrem k tělu
const float rychlost_pohybu_vstavani = 2;              // rychlost, dle které probíhá vstávání/sedání
const float rychlost_otaceni = 3;                      // rychlost, dle které se robot otáčí okolo vlastní osy vpravo/vlevo
float rychlost_pohybu, budouci_rychlost_pohybu[4][3];  // proměnná pro uchování: konečné rychlosti pohybu & momentální/krátkodobé rychlosti pohybu dané části určité nohy

////////////////////////////////////// PROMĚNNÉ K MĚŘENÍ PH
const int pH_pin = A0;              // pin zapojene pH sondy
const float pH_kalibrace = 25.032;  // kalibrace, pri ktere meric ukazuje presne hodnoty
const byte U_merice = 5;            // standardni napeti na merici, nutne k prepocteni signalu
const int kanal_merice = 1024;      // pracujeme v 10-bit rozliseni - dle <analogread>, a proto 2^10 moznosti, to je 1024
const int cetnost_vysledku = 120;   // kolik vysledku pripadne na 1 minutu mereni
float pH = 0;                       // proměnná, která bude uchovávat spočítanou hodnotu pH
float pH_signal[10];                // list pro přijímané pH hodnoty - bude mít velikost 10, takže se bude vži konat 10 měření -> pro lepší přesnost
int zaloha;                         // pomocná proměnná pro přehazování hodnot při seřazování pH měření dle jejich velikosti 
unsigned long int pH_prumer = 0;    // průměrná hodnota V signálu pH proby pro budoucí výpočet reálné hodnoty pH

////////////////////////// PROMĚNNÉ K BLUETOOTH (BT) OVLÁDÁNÍ
char BT_prikaz;         // písmeno, které je přijato z BT modulu
char BT_zmena;          // mediátor pro výpis změny stavu při novém příkazu ze strany BT
int BT_iterace = 1;     // nosič hodnot, ve kterých budou iterovány BT příkazy

//////////////////////////////////////////////////////////////////////////////// INICIALIZACE
void setup(){                                                                 // prvotní blok kódu, který je nezbytný pro samotné ovládání robota
  Serial.begin(9600);                                                         // pro komunikaci - budeme pracovat na rychlosti 9 600 baudů
  int pin_servomotoru[4][3] = {{2, 3, 4},{5, 6, 7},{8, 9, 10},{11, 12, 13}};  // definuje piny servomotorů, a to pro 4 nohy (0123), každá se 3 motory (X, Y, Z)
  for (int i = 0; i < 4; i++){                                                // opakovač pro 4 nohy
    for (int j = 0; j < 3; j++){                                              // opakovač pro XYZ každé z nohou
      servomotor[i][j].attach(pin_servomotoru[i][j]);                         // připojení všech seromovorů k jejich příslušným pinům
      nastavit_pozici(i, x_start, y_start, z_start, 0);                       // přednastavení pozice, aby nedošlo k automatickému načtení 90°, jež zasekne nohy do těla
      soucasna_pozice[i][j] = budouci_pozice[i][j];}}                         // nastaví momentální pozici jako budoucí, a tak zamezí pohybu
  FlexiTimer2::set(20, ovladac_servomotoru);                                  // přiřadí 2. knihovně ovladač, dle kterého bude kontrola pohybu probíhat
  FlexiTimer2::start();}                                                      // nastartuje 2. knihovnu, která je třeba k přesnému (časovanému) ovládání servomotorů

///////////////////////////////////////////////////////////////////// ITINERÁŘ
void loop(){                                                       // plán chování robota
  if (Serial.available()) BT_prikaz = Serial.read();               // pokud je zapojený Bluetooth modul HC-06, proměnné BT_prikaz se přiřadí data, která modul přijme
  if (isDigit(BT_prikaz)){                                         // zkontroluje se, jestli se namísto charakteru nejedná o číslovku, a pokud ano, tak:
    BT_iterace = BT_prikaz - '0';                                  // proměnné BT_iterace se přiřadí v desítkové soustavě zadaná číslice (proto se odečítá char pro 0 -> jednoduchý převod na int)
    Serial.println(F("NOVÁ HODNOTA ITERACE NASTAVENA NA: "));      // do konzole se vypíše, že byla hodnota iterace přeměněna 
    Serial.println(BT_iterace);                                    // dále se vypíše hodnota, na kterou se iterace změnila
    BT_prikaz = 'z';}                                              // protože jsme BT_prikaz používali jako de facto mediátor, přiřadímě mu "prázndou" hodnotu, tedy příkaz z, který nic nedělá
  if (BT_prikaz == 'v'){                                           // pokud chceme vykonat příkaz v, to je chůze vpřed:
    Serial.println(F("CHŮZE VPŘED ... 1 (ZAP)"));                  // do konzole se vypíše, že byla započnuta chůze dopředu                                 
    vstat(1);                                                      // robot vstane - uvede se do pozice, ze které se umí rozejít
    pohyb_vpred(BT_iterace);}                                      // a začne chodit, přičemž bude dělat tolik kroků najednou, podle toho, jaké číslo jsme v minulém kroku nastavili pro proměnnou BT_iterace - standardně 1krát
  if (BT_prikaz == 'q'){                                           // pokud chceme vykonat příkaz q, to je sednutí si do pozice, ze které se nebudeme moci bezprostředně rozejít, ale bude z ní možné pohodlně, to je bez ztráty rovnováhy, zacílit a ponořit měřicí pH probu:
    Serial.println(F("... SEDNUTÍ ... "));                         // do konzole se vypíše, že si robot sedne, tedy uvede do stabilní klidové pozice
    vstat(0);}                                                     // a robot přestane stát, čili se posadí
  if (BT_prikaz == 'l'){                                           // pokud chceme vykonat příkaz l, to je započít rotaci v levém směru:
    Serial.println(F(" ... OTÁČENÍ VLEVO ... "));                  // do konzole se vypíše, že se bude robot otáčet doleva            
    otocit_vlevo(BT_iterace);}                                     // a robot se začně doleva otáčet, a to opět v pořadí kroků dle stanovené proměnné BT_iterace
  if (BT_prikaz == 'r'){                                           // pokud chceme vykonat příkaz r, to je započít rotaci v pravém směru:
    Serial.println(F(" ... OTÁČENÍ VPRAVO ... "));                 // do konzole se vypíše, že se robot bude otáčet doprava                                      
    otocit_vpravo(BT_iterace);}                                    // a robot se začně doprava otáčet, a to opět v pořadí kroků dle stanovené proměnné BT_iterace              
  if (BT_prikaz == 's'){                                           // pokud chceme vykonat příkaz s, to je jakoby pojistka, tedy stop všem probíhajícím příkazům:      
    Serial.println(F("UKONČUJI FUNKCE... 0 (VYP)"));               // do konzole se vypíše, že dochází k ukončení všech probíhajících funkcí
    vstat(0);                                                      // a robot se posadí, tedy zamezí pohybu vpřed či dokola
    pH_mereni(0);}                                                 // a zastaví potenciální měření pH
  if (BT_prikaz == 'p'){                                           // pokud chceme vykonat příkaz p, to je započít měření pH:  
    pH_mereni(1);                                                  // začneme pH měřit
    pH_seriovy_vypis(1, 2);                                        // a vypíšeme do konzole usměrněné a z napětí přepočtené skutečné pH, které rovněž zaokrouhlíme na 2 desetinná místa (dle 2. parametru) 
    BT_prikaz = 's';}                                              // vyvoláme příkaz stop, který měření zruší, tedy se vykoná pouze 1krát -> pakliže bychom chtěli kontinuální měření, lze tento řádek kódu smazat, ačkoliv bude třeba měření manuálně pomocí příkazu s vypnout
  if (BT_prikaz == 'f'){                                           // pokud chceme vykonat příkaz f, to je od zacílení pH proby:              
    Serial.println(F("... ZACÍLENÍ ..."));                         // do konzole se vypíše, že se proba zacílila                                
    zacilit(1);}                                                   // pH proba se zamíří a připraví pro ponoření a následné měření     
  if (BT_prikaz == 'u'){                                           // pokud chceme vykonat příkaz u, to je od ponoření pH proby:             
    Serial.println(F("... PONOŘENÍ ..."));                         // do konzole se vypíše, že se proba ponořila                 
    ponorit(1);}                                                   // pH proba provede úkon ponoření   
  BT_prikaz = 'z';}                                                // provedeme jakoby reset všech příkazů - hodnotu BT_prikaz stanovíme na "prázdnou" hodnotu, tedy příkaz z, který nic nedělá      

//////////////////////////////////////////////////////// OTÁČENÍ VLEVO
void otocit_vlevo(unsigned int krok){                 // celkový plán otáčení vlevo pro daný počet opakování - kroků
  rychlost_pohybu = rychlost_otaceni;                 // rychlost vykonávaného pohybu, tedy otáčení, se nastaví na požadovanou rychlost pro rotační pohyb robota
  while (krok-- > 0){                                 // počet kroků, tedy opakování -> zkontroluje, zda má i nadále probíhat otáčení
    if (soucasna_pozice[3][1] == y_start){            // pokud je současná pozice Y nohy pro set 3 rovna startovní pozici -> abychom zamezeili přeskočení mezikroku, jenž by vedl ke ztrátě rovnováhy:
      otoceni(3, 1, 0, 1, +1, 1, 0, 1, 0, 0, 0);      // provedeme otočení pro nohu 3 (a = 3), dále pak pro jí opačnou nohu 1 (b = 1); stanovíme, že proběhne až sekundárně otočení (c = 0), nejrpve totiž přetočení (d = 1); vyřešíme, že se budou nohy na X souřadnici nejprve pohybovat od sebe (e = 1); dále ověříme, že se v 1. bloku nejprve na Z souřadnici rozejde (f = 1), až pak vrátí do startovní pozice (g = 0) [u třetího a finálního bloku to je zase naopak] - přičemž pro f a g máme podmínku (fg = 1), jež v tomto případě určí, že budeme v inicializačním bloku rotovat na nohou 3 a 0, namísto 2 a 1 (proto je také trojice h = 0; i = 0; spolu s hlavním hi = 0 nepravdou) 
    }else{                                            // v opačném případě, je-li současná pozice pro set nohou 0-1:
      otoceni(0, 2, 1, 0, -1, 0, 1, 1, 0, 0, 0);}}}   // provedeme otočení pro nohu 0 (a = 0), dále pak pro jí opačnou nohu 2 (b = 2); stanovíme, že proběhne už primárně otočení (c = 1), až posléze přetočení (d = 0); vyřešíme, že se budou nohy na X souřadnici nejprve pohybovat k sobě (e = -1); dále ověříme, že se v 1. bloku na Z souřadnici až sekundárně rozejde (f = 0), primárně se totiž nastaví do startovní pozice (g = 1 ) [u třetího a finálního bloku to je zase naopak] - přičemž pro f a g máme podmínku (fg = 1), jež v tomto případě určí, že budeme v inicializačním bloku rotovat na nohou 0 a 3, namísto 1 a 2 (proto je také trojice h = 0; i = 0; spolu s hlavním hi = 0 nepravdou) 

//////////////////////////////////////////////////////// OTÁČENÍ VPRAVO (ANTI-VLEVO)
void otocit_vpravo(unsigned int krok){                // celkový plán otáčení vpravo pro daný počet opakování - kroků
  rychlost_pohybu = rychlost_otaceni;                 // rychlost vykonávaného pohybu, tedy otáčení, se nastaví na požadovanou rychlost pro rotační pohyb robota
  while (krok-- > 0){                                 // počet kroků, tedy opakování -> zkontroluje, zda má i nadále probíhat otáčení
    if (soucasna_pozice[2][1] == y_start){            // pokud je současná pozice Y nohy pro set 2 rovna startovní pozici -> abychom zamezili přeskočení mezikroku, jenž by vedl ke ztrátě rovnováhy:
      otoceni(2, 0, 1, 0, +1, 0, 0, 0, 1, 0, 1);      // provedeme otočení pro nohu 2 (a = 2), dále pak pro jí opačnou nohu 0 (b = 0); stanovíme, že proběhne už primárně otočení (c = 1), až posléze přetočení (d = 0); vyřešíme, že se budou nohy na X souřadnici nejprve pohybovat od sebe (e = 1); dále ověříme, že se v 1. bloku u párových nohou popojde, u protichůdných ve krocích 2 a 3 rozejde (h = 1) a až sekundárně uvede do startovní klidové pozice (i = 0) [u třetího a finálního bloku to je zase naopak] - přičemž pro h a i máme podmínku (hi = 1), jež v tomto případě určí, že budeme v inicializačním bloku rotovat na nohou 2 a 1, namísto 3 a 0 (proto je také trojice f = 0; g = 0; spolu s hlavním fg = 0 nepravdou) 
    }else{                                            // v opačném případě, je-li současná pozice pro set nohou 1-1:
      otoceni(1, 3, 0, 1, -1, 0, 0, 0, 0, 1, 1);}}}   // provedeme otočení pro nohu 1 (a = 1), dále pak pro jí opačnou nohu 3 (b = 3); stanovíme, že proběhne až sekundárně otočení (c = 0), nejrpve totiž přetočení (d = 1); vyřešíme, že se budou nohy na X souřadnici nejprve pohybovat k sobě (e = -1); dále ověříme, že se v 1. bloku u párových nohou popojde, u protichůdných ve krocích 2 a 3 nejprve uvede do startovní pozice (h = 0) a až sekundárně do pohybu (i = 1) [u třetího a finálního bloku to je zase naopak] - přičemž pro h a i máme podmínku (hi = 1), jež v tomto případě určí, že budeme v inicializačním bloku rotovat na nohou 1 a 2, namísto 0 a 3 (proto je také trojice f = 0; g = 0; spolu s hlavním fg = 0 nepravdou) 

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// OTÁČIVÝ POHYB
void otoceni(byte a, byte b, byte c, byte d, byte e, byte f, byte g, byte fg, byte h, byte i, byte hi){                                                     // plán otáčení dle definovaných parametrů pro levotočivý a naopak pravotočivý pohyb, a to dle dané počáteční pozice nohou 3, 0, respektive 2, 1
  nastavit_pozici(a, x_start + x_krok, y_start, z_start, 1);                                                                                                // pro danou nohu A se rozejdeme na X souřadnici + počkáme na dokončení pohybu všech částí nohy
  nastavit_pozici(0, x_otoceni * c + x_pretoceni * d - x_krok * e, y_otoceni * c + y_pretoceni * d, (z_krok * f + z_start * g) * fg + (z_krok) * hi, 0);    // pro nohu 0 »| z klidové a stabilní pozice se rozejdou 2 nohy
  nastavit_pozici(1, x_otoceni * d + x_pretoceni * c - x_krok * e, y_otoceni * d + y_pretoceni * c, (z_krok) * fg + (z_krok * h + z_start * i) * hi, 0);    // pro nohu 1 »| napřímí se dozadu a připraví na budoucí rotaci
  nastavit_pozici(2, x_otoceni * c + x_pretoceni * d + x_krok * e, y_otoceni * c + y_pretoceni * d, (z_krok) * fg + (z_krok * i + z_start * h) * hi, 0);    // pro nohu 2 »| opačný pár nohou se zase připaží k tělu 
  nastavit_pozici(3, x_otoceni * d + x_pretoceni * c + x_krok * e, y_otoceni * d + y_pretoceni * c, (z_krok * g + z_start * f) * fg + (z_krok) * hi, 1);    // pro nohu 3 »| těžiště se tak posune dále od v budoucnosti rotujících nohou + nakonec počkáme na dokončení pohybu všech částí nohy - jinak by se mohl vykonávat další krok bez dokončení, a to by vedlo ke kolapsu
  nastavit_pozici(a, x_otoceni + x_krok, y_otoceni, z_krok, 1);                                                                                             // pro danou nohu A se otočíme na X a Y souřadnici, přitáhneme nohu na Z souřadnici + počkáme na dokončení pohybu všech částí nohy
  nastavit_pozici(0, x_otoceni * c + x_pretoceni * d + x_krok * e, y_otoceni * c + y_pretoceni * d, z_krok, 0);                                             // pro nohu 0 »| pár nohuo na levé či naopak pravé strane se k sobě přitáhne
  nastavit_pozici(1, x_otoceni * d + x_pretoceni * c + x_krok * e, y_otoceni * d + y_pretoceni * c, z_krok, 0);                                             // pro nohu 1 »| opačný pár se zase vychýlí
  nastavit_pozici(2, x_otoceni * c + x_pretoceni * d - x_krok * e, y_otoceni * c + y_pretoceni * d, z_krok, 0);                                             // pro nohu 2 »| tímto se těžiště přesune dále od vyhýleného páru nohou 
  nastavit_pozici(3, x_otoceni * d + x_pretoceni * c - x_krok * e, y_otoceni * d + y_pretoceni * c, z_krok, 1);                                             // pro nohu 3 »| to umožní v budoucím kroku ladnější rotaci na opoziční noze (anti-A noze) + opět nakonec počkáme na dokončení pohybu všech částí nohy - jinak by se mohl vykonávat další krok bez dokončení, a to by vedlo ke kolapsu
  nastavit_pozici(b, x_otoceni + x_krok, y_otoceni, z_start, 1);                                                                                            // pro danou nohu B se otočíme na X a Y souřadnici, přičemž část nohy ovládající Z souřadnici nastavíme do klidové startovní pozice - pro zachování stability + počkáme na dokončení pohybu všech částí nohy
  nastavit_pozici(0, x_start + e * x_krok, y_start + y_krok * c, (z_krok) * fg + (z_krok * i + z_start * h) * hi, 0);                                       // pro nohu 0 »| naprostý opak 1. bloku pro 4 nohy
  nastavit_pozici(1, x_start + e * x_krok, y_start + y_krok * c, (z_krok * g + z_start * f) * fg + (z_krok) * hi, 0);                                       // pro nohu 1 »| rozejdou se 2 nohy (přední či zadní), které se v 1. bloku uvedly do klidové pozice
  nastavit_pozici(2, x_start - e * x_krok, y_start + y_krok * d, (z_krok * f + z_start * g) * fg + (z_krok) * hi, 0);                                       // pro nohu 2 »| opačný pár se připaží k tělu
  nastavit_pozici(3, x_start - e * x_krok, y_start + y_krok * d, (z_krok) * fg + (z_krok * h + z_start * i) * hi, 1);                                       // pro nohu 3 »| těžiště se posune na druhou stranu nežli v 1. 4nohém bloku, tentokráte pro uvedení do lidové pozice, ze které se zase započně pohyb pro opačný pár nohou + rovněž nakonec počkáme na dokončení pohybu všech částí nohy - jinak by se mohl vykonávat další krok bez dokončení, a to by vedlo ke kolapsu
  nastavit_pozici(b, x_start + x_krok, y_start, z_krok, 1);}                                                                                                // pro danou nohu B se rozejdeme na X a Z souřadnici + počkáme na dokončení pohybu všech částí nohy

//////////////////////////////////////////////////// ZACÍLENÍ PROBY
void zacilit(boolean zacileni){                   // plán uvedení proby do zacílené pozice - nepravda proměnné zacileni automaticky v následující if podmínce zapříčiní, že nebude pohyb vykonán
  if (zacileni){                                  // pokud ale chceme, aby se pH proba zacílila a připravila k následnému ponoření:
    nastavit_pozici(0, x_start, 110, 40, 1);}}    // nastaveíme pro pravou přední nohu (0): startovní X souřadinici, dále Y souřadnici, do napřímené pozice a následně jakoby nadzdvihneme, tedy zacílíme Z souřadnici, aby se mohla ponořit

//////////////////////////////////////////////////// PONOŘENÍ PROBY
void ponorit(boolean ponoreni){                   // plán ponoření - nepravda proměnné ponoreni automaticky v následující if podmínce zapříčiní, že nebude pohyb vykonán
  if (ponoreni){                                  // pokud ale chceme, aby se pH proba ponořila:
    nastavit_pozici(0, x_start, 110, -50, 1);}}   // ponecháme X a Y ze zacílené pozice a pouze změníme Z souřadnici do ponořené pozice

////////////////////////////////////////////////////////////////////// VSTÁVÁNÍ/SEDÁNÍ
void vstat(int orientace){                                          // přepínač vstávání (1) a sedání (0)
  orientace = (orientace == 1) ? (z_krok) : (z_start);              // má-li vstát, Z pozice se změní na "z_krok", v opačném případě na "z_start"
  rychlost_pohybu = rychlost_pohybu_vstavani;                       // stanoví rychlost pohybu pro vstávání
  for (int noha = 0; noha < 4; noha++)                              // opakovač pro 4 nohy
    nastavit_pozici(noha, nehybat, nehybat, orientace, noha / 3);}  // začnou se měnit Z nohou, přičemž se počká na dokončení pohybu poslední (4.) nohy

////////////////////////////////////////////// POHYB VPŘED
void pohyb_vpred(unsigned int krok){        // celkový plán pohybu vpřed
  while (krok-- > 0){                       // počet kroků, tedy opakování
    if (soucasna_pozice[2][1] == y_start){  // pohyb primární strany (pravé)
      pohyb_vpredA(2);                      // situovaní pravé přední nohy ve směru chodu
      pohyb_vpredB(0,2);                    // přitažení ostatních nohou na levé straně (opačná pohybu 1A), pak až na straně pravé
      pohyb_vpredC(1);}                     // situování levé zadní nohy ve směru chodu (křížová pohybu 1A)
    else{                                   // pohyb sekundární strany (levé)
      pohyb_vpredA(0);                      // situovaní levé přední nohy ve směru chodu
      pohyb_vpredB(2,0);                    // přitažení ostatních nohou na pravé straně (opačné pohybu 2A), pak až na straně levé
      pohyb_vpredC(3);}}}                   // situování pravé zadní nohy ve směru chodu (křížová pohybu 2A)

/////////////////////////////////////////////////////////////////////// POHYB VPŘED A
void pohyb_vpredA(byte noha){                                        // pohyb pouze přední nohy
  rychlost_pohybu = rychlost_pohybu_nohy;                            // rychlost pohybu, pakliže se hýbe pouze 1 noha
  nastavit_pozici(noha, x_start, y_start, z_start, 1);               // nadzvednutí nohy
  nastavit_pozici(noha, x_start, y_start + 2 * y_krok, z_start, 1);  // pootočení nohy ve směru pohybu
  nastavit_pozici(noha, x_start, y_start + 2 * y_krok, z_krok, 1);}  // dopadnutí nohy

/////////////////////////////////////////////////////////////////////////// POHYB VPŘED B
void pohyb_vpredB(byte noha0, byte noha1){                               // pohyb všech nohou - de facto se přitáhnou k noze z pohybu A
  rychlost_pohybu = rychlost_pohybu_tela;                                // rychlost pohybu, pakliže se hýbou všechny nohy (což je de facto celé tělo)
  nastavit_pozici(noha0, x_start, y_start, z_krok, 0);                   // oddálení přední nohy od pohybu A, což tělo robota posune opačným směrem
  nastavit_pozici(noha0 + 1, x_start, y_start + 2 * y_krok, z_krok, 0);  // oddálení zadní nohy od pohybu A, což tělo robota posune opačným směrem
  nastavit_pozici(noha1, x_start, y_start + y_krok, z_krok, 0);          // přisunutí nohy zpět k tělu pro stabilitu
  nastavit_pozici(noha1 + 1, x_start, y_start + y_krok, z_krok, 1);}     // přisunutí nohy zpět k tělu pro stabilitu

/////////////////////////////////////////////////////////////////////// POHYB VPŘED C
void pohyb_vpredC(byte noha){                                        // pohyb pouze přední nohy, která je křížová pro nohu z pohybu A (tedy vzadu a na druhé straně)
  rychlost_pohybu = rychlost_pohybu_nohy;                            // rychlost pohybu, pakliže se hýbe pouze 1 noha
  nastavit_pozici(noha, x_start, y_start + 2 * y_krok, z_start, 1);  // nadzvednutí nohy
  nastavit_pozici(noha, x_start, y_start, z_start, 1);               // pootočení ve směru pohybu
  nastavit_pozici(noha, x_start, y_start, z_krok, 1);}               // dopadnutí nohy

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// KALKULÁTOR A OVLADAČ POHYBU
void ovladac_servomotoru(void){                                                                                                          // umožní ovládat servomotory, a to z kartézské soustavy, přes polární na příslušné úhly
  static float alfa, beta, gama;                                                                                                         // definuje úhly alfa X, beta Y a gama Z pro pohyb motorů na osách
  for (int i = 0; i < 4; i++){                                                                                                           // opakovač pro 4 nohy
    for (int j = 0; j < 3; j++){                                                                                                         // opakovač pro XYZ
      if (abs(soucasna_pozice[i][j] - budouci_pozice[i][j]) >= abs(budouci_rychlost_pohybu[i][j]))                                       // má-li se servomotor dostat na pozici, která je dále, než stanovená rychlost,
        soucasna_pozice[i][j] += budouci_rychlost_pohybu[i][j];                                                                          // tak se začne k budoucí pozici pohybovat
      else                                                                                                                               // nemá-li se nikam hnout,
        soucasna_pozice[i][j] = budouci_pozice[i][j];}                                                                                   // tak se současná pozice zamění za budoucí
    volatile float x = soucasna_pozice[i][0], y = soucasna_pozice[i][1], z = soucasna_pozice[i][2];                                      // X, Y a Z se přisadí současné pozice
    const float delka_a = 56.5, delka_b = 79.1, pi = 3.1415926;                                                                          // šířka frontální části základny robota, délka boku základny robota, Ludolfovo číslo pro převod na úhly
    int k = ((i == 0) ||(i == 3)) ? (-1) : (1);                                                                                          // definuje orientaci motoru, a to na bázi dvojic nohou 0,3 a 1,2 pro přechod z polárni soustavy na zorientované úhly robota
    float v = ((x >= 0 ? 1 : -1) * (sqrt(sq(x) + sq(y)))) - 27.5;                                                                        // spočte úhel v a tím pak úhly alfa, beta a gama, a to jako přechod z kartézské soustavy na polární - což umožňujou kosinovy věty pro inverzní kinematiku
    alfa = 90 + k * ((atan2(z, v) + acos((sq(delka_a) - sq(delka_b) + sq(v) + sq(z)) / 2 / delka_a / sqrt(sq(v) + sq(z)))) / pi * 180);  // spočte úhel alfa X a převede jej na stupně s příslušnou orientací nohy
    beta = 90 + k * 90 - k * ((acos((sq(delka_a) + sq(delka_b) - sq(v) - sq(z)) / 2 / delka_a / delka_b)) / pi * 180);                   // spočte úhel beta Y a převede jej na stupně s příslušnou orientací nohy
    gama = 90 - k * (((v >= -27.5) ? atan2(y, x) : atan2(-y, -x)) / pi * 180);                                                           // spočte úhel gama X a převede jej na stupně s příslušnou orientací nohy
    const float uhel[3] = {alfa, beta, gama};                                                                                            // nastaví pole hodnot úhlů, na které se pohnou servomotory
    for (int j = 0; j < 3; j++){                                                                                                         // opakovač pro XYZ
      servomotor[i][j].write(uhel[j]);}}}                                                                                                // pohne servomotorem nohy na pšíslušné ose o spočtený úhel
    
///////////////////////////////////////////////////////////////////////////////////////////////////////// NASTAVOVAČ POZICE
void nastavit_pozici(int noha, float x, float y, float z, boolean pockat_na_dokonceni_pohybu){         // nastaví pozici, kam se servomotor má hýbat
  volatile float xyz[3] = {x, y, z};                                                                   // nastaví pole hodnot pro xyz[0] = X, xyz[1] = Y, xyz[2] = Z
  float delka_x, delka_y, delka_z;                                                                     // definuje délky Z, Y, a Z, po kterých se na příslušných osách budou servomotory hýbat
  float delkaxyz[3] = {delka_x, delka_y, delka_z};                                                     // nastaví pole hodnot pro delkaxyz[0] = delka_x, delkaxyz[1] = delka_y, delkaxyz[2] = delka_z
  for (int i = 0; i < 3; i++){                                                                         // opakovač pro XYZ
    if (xyz[i] != nehybat) delkaxyz[i] = xyz[i] - soucasna_pozice[noha][i];                            // má-li se na dané ose servomotor hýbat, změří délku, kterou má na ose překonat
    float delka = sqrt(sq(delkaxyz[0]) + sq(delkaxyz[1]) + sq(delkaxyz[2]));                           // z X, Y a Z stanoví celkovou délku pohybu
    budouci_rychlost_pohybu[noha][i] = delkaxyz[i] / delka * rychlost_pohybu * modifikator_rychlosti;  // nastaví, jakou rychlostí se má servomotor na dané ose hýbat
    if (xyz[i] != nehybat) budouci_pozice[noha][i] = xyz[i];}                                          // má-li se na dané ose servomotor hýbat, nastaví budoucí pozici pro danou osu
  if (pockat_na_dokonceni_pohybu)                                                                      // má-li se počkat na dokončení veškerého pohybu, započne ověřování dokončení pohybu
    for (int i = 0; i < 4; i++)                                                                        // opakovač pro 4 nohy
      for (int j = 0; j < 3; j++)                                                                      // opakovač pro XYZ každé z nohou
        while (1)                                                                                      // testuje, dokavaď je blok pravdivý
          if (soucasna_pozice[i][j] == budouci_pozice[i][j])                                           // zkontroluje, zda byl na dané ose pohyb servomotoru dokončen
            break;}                                                                                    // byl-li veškerý pohyb dokončen, ukončí se podmínka, a tak je umožněno započít další pohyb

////////////////////////////////////////////////////////////////////////////////////////// SBĚR HODNOT A PŘEPOČET NA PH
void pH_mereni(int pH_meric){                                                           // nastaví hodnotu, která určuje, zda se bude měřit pH
  if (pH_meric){                                                                        // pokud chceme měřit pH:
    delay(60000 / cetnost_vysledku);                                                    // definujeme, jakou dobu se bude čekat na zahájení samotného měření, de facto tedy: kolikrát za 1 minutu bude pH sonda měřit vzorek
    for (int i = 0; i < 10; i++){                                                       // vytvořímě loop, který nám umožní učinit 10 měření najednou
      pH_signal[i] = analogRead(pH_pin);                                                // do 2-rozměrné řady začneme přiřazovat měřené hodnoty z analogového pinu měřicí proby
      delay(5);}                                                                        // zanedbatelná 5ms pauza mezi každým měřením, aby teoreticky nedošlo k přehřátí komponent
    for (int i = 0; i < 9; i++){                                                        // vytvoříme loop pro 8 výsledků - lokálního naměřeného maxima a minima se takto zbavíme
      for (int j = i + 1; j < 10; j++){                                                 // dále vytvoříme 2. loop, opět pro 8 výsledků, ale vždy pro takový výsledek, který je ve 2-D poli o jeno pole dále (proto i++)
        if (pH_signal[i] > pH_signal[j]){                                               // otestujeme, zda se v pořadí hodnoty zvětšují, tedy zda jsou již seřazeny vzestupně, tudíž zkontrolujeme, jestli v pořadí nižší měření má menší reálnou hodnotu než to v pořadí za ním, pokud to je v pořádku, nic se neděje, v opačném případě ale tyto dvě hodnoty musíme navzájem prohodit
          zaloha = pH_signal[i];                                                        // do mediátorové proměnné zaloha načteme větší naměřenou hodnotu (i)
          pH_signal[i] = pH_signal[j];                                                  // této větší hodnotě (i) přiřadíme měnší hodnotu (j)
          pH_signal[j] = zaloha;}}}                                                     // hodnotě j konečně přiřadíme mediátorovou "záložní" hodnotu rovnou i, jinými slovy: pokud je například původní hodnota (i = 3) větší nežli ta (j = 4), která jde v 2-D řadě po ní, tyto dvě hodnoty se navzájem prohodí, takže skončíme loop s tím, že budeme mít v poli tu větší hodnotu jakoby "nad" tou menší, tedy seřazenou vzestupným způsobem, takže z 3 bude 4 a ze 4 bude naopak 3 v řadovém pořádku
    for (int i = 3; i < 7; i++){                                                        // loop pro uložení pouze 4 "prostředních" měření, které se od sebe nejméně liší
      pH_prumer += pH_signal[i];}                                                       // přičtení všech 4 měření do proměnné pH_prumer, která bude v následujícím kroku vydělena 4, tedy se z ní reálně stane aritmetický průměr a bude z ní vypočteno pH
    pH = ((-5.7 * U_merice * (float)pH_prumer / kanal_merice / 4) + pH_kalibrace);}}    // dojde k přepočtu jakoby o odchylku zbaveného elektrického napětí, a to na skutečnou hodnotu pH -> dělíme 4, protože oracujeme s pH_prumer, ktere v teto instanci uchovává sumu 4 nejpřesnějších hodnot

/////////////////////////////////////////////////////////////////// VÝPIS PH
void pH_seriovy_vypis(boolean vypsat, byte pH_desetinna_mista){  // nastaví, zda se bude vypisovat pH do sériového počítače a nastaví, na kolik desetinných míst se bude zaokrouhlovat
  if ((vypsat == true) && (pH > 0)){                             // chceme-li vypisovat do sériového počítače naměřené a spočítané hodnoty pH, přejde k dalšímu kroku
    Serial.print(F("pH: "));                                     // předepíše do konzole následující text -> pH:
    Serial.println(pH, pH_desetinna_mista);}}                    // pokračuje v předchozím řádku textu a doplní jej o spočtenou hodnotu pH, kterou zaokrouhlí na definovanou hodnotu chtěnného počtu desetinných míst
