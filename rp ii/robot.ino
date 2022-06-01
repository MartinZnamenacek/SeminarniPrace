//////////////////////////// KNIHOVNY A JEJICH UVEDENÍ DO CHODU
#include <Servo.h>        // umožňuje implementaci servomotorů
Servo servomotor[4][3];   // definuje uspořádané pole hodnot: noha-XYZ, tedy 0-012, 1-012, 2-012, 3-012
#include <FlexiTimer2.h>  // časovač pro přesné ovládání servomotorů

/////////////////////////////////////////////////////////////// POZICE/SOUŘADNICE
const float x_start = 60;                                    // startovní pozice pro X
const float y_start = 0, y_krok = 40;                        // startovní pozice pro Y & pozice pro krok na Y
const float z_start = -30, z_krok = -50;                     // startovní pozice pro Z & pozice pro krok na Z
const byte nehybat = 255;                                    // hodnota, při které se pozice motoru nemění
volatile float soucasna_pozice[4][3], budouci_pozice[4][3];  // proměnná pro uchovávání: současné pozice dané části (XYZ) určité nohy (0123) & předpokládané pozice, kam se bude daná část (XYZ) určité nohy (0123) hýbat

///////////////////////////////////////////////////////// RYCHLOSTI POHYBU
const int prodleva = 250;                              // zavede, jakým časovým intervalem budou jednotlivé kroky oddělovány
const float modifikator_rychlosti = 1.1;               // násobič celkové rychlosti - umožnujě obecné zpomalení/zrychlení
const float rychlost_pohybu_nohy = 10;                 // rychlost, kterou se hýbe noha při chůzi
const float rychlost_pohybu_tela = 5;                  // rychlost, kterou se hýbou všechny nohy naráz směrem k tělu
const float rychlost_pohybu_vstavani = 2;              // rychlost, dle které probíhá vstávání/sedání
float rychlost_pohybu, budouci_rychlost_pohybu[4][3];  // proměnná pro uchování: konečné rychlosti pohybu & momentální/krátkodobé rychlosti pohybu dané části určité nohy

//////////////////////////////////////////////////////////////////////////////// INICIALIZACE
void setup(){                                                                 // prvotní blok kódu, který je nezbytný pro samotné ovládání robota
  int pin_servomotoru[4][3] = {{2, 3, 4},{5, 6, 7},{8, 9, 10},{11, 12, 13}};  // definuje piny servomotorů, a to pro 4 nohy (0123), každá se 3 motory (X, Y, Z)
  for (int i = 0; i < 4; i++){                                                // opakovač pro 4 nohy
    for (int j = 0; j < 3; j++){                                              // opakovač pro XYZ každé z nohou
      servomotor[i][j].attach(pin_servomotoru[i][j]);                         // připojení všech seromovorů k jejich příslušným pinům
      nastavit_pozici(i, x_start, y_start, z_start, 0);                       // přednastavení pozice, aby nedošlo k automatickému načtení 90°, jež zasekne nohy do těla
      soucasna_pozice[i][j] = budouci_pozice[i][j];}}                         // nastaví momentální pozici jako budoucí, a tak zamezí pohybu
  FlexiTimer2::set(20, ovladac_servomotoru);                                  // přiřadí 2. knihovně ovladač, dle kterého bude kontrola pohybu probíhat
  FlexiTimer2::start();}                                                      // nastartuje 2. knihovnu, která je třeba k přesnému (časovanému) ovládání servomotorů

/////////////////////////////////////////// ITINERÁŘ
void loop(){                             // plán chování robota
  vstat(1); delay(4 * prodleva);         // nejprve vstane
  pohyb_vpred(25); delay(3 * prodleva);  // pak chodí
  vstat(0); delay(6 * prodleva);}        // nakonec si sedne

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
