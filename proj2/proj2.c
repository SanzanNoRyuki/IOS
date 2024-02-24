/******************************************************************************/
/***                 Roman Fulla (xfulla00) - IOS projekt 2                 ***/
/***                              Verzia - 0.9                              ***/
/******************************************************************************/

// Kniznice:
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/mman.h>

// Definicie:
#define ALL_OK 0
#define ERROR  1

// Zdielane premenne,
// ktore dostanu svoju hodnotu raz v hlavnom procese,
// ale vyskytuju sa vo vsetkych procesoch, preto su globalne:
FILE *output_file = NULL;     // Subor pre vystup

int R;                        // Maximalna hodnota doby (v ms) plavby
int W;                        // Maximalna hodnota doby (v ms), po ktorej sa osoba vracia spat na molo (ak bolo predtym molo plne)
int C;                        // Kapacita mola

// Zdielane premenne,
// ktorym menia procesy hodnoty za behu:
int *A = 0;                   // Index akcie
int fd_A;                     // File descriptor pre A

int *NH = 0;                  // Hackery na mole
int fd_NH;                    // File descriptor pre NH

int *NS = 0;                  // Serfovia na mole
int fd_NS;                    // File descriptor pre NS

int *ship = 0;                // Ludia na lodi
int fd_ship;                  // File descriptor pre ship

int *empty = 0;               // Indikator prazdnosti lode
int fd_empty;                 // File descriptor pre empty

// Semafory:
sem_t *output_sem   = NULL;   // Semafor pre zapis do suboru pre vystup

sem_t *act_idx_sem  = NULL;   // Semafor pre zapis do indexu akcii            (A)
sem_t *hck_pier_sem = NULL;   // Semafor pre zmenu poctu hackerov na mole     (NH)
sem_t *srf_pier_sem = NULL;   // Semafor pre zmenu poctu serfov na mole       (NS)
sem_t *ship_sem = NULL;       // Semafor pre zmenu poctu ludi na lodi         (ship)
sem_t *empty_sem = NULL;      // Semafor pre zmenu indikatora prazdnosti lode (empty)

// Deklaracie:
int input_check(int argc, char *argv[], int *P, int *H, int *S);

int multiple_hackers(int *P, int *H);
int multiple_serfs(int *P, int *S);

int hacker(int *I);
int serf(int *I);

int init();
void clean();

// Funkcie:
int main(int argc, char *argv[]) {
  int P;                      // Pocet osob generovanych v kazdej kategorii - bude vytvorenych 'P' hackers a 'P' serfs
  int H;                      // Maximalna hodnota doby (v ms), po ktorej je generovany novy proces hackers
  int S;                      // Maximalna hodnota doby (v ms), po ktorej je generovany novy proces serfs
  int hacker_err = ALL_OK;
  int serf_err = ALL_OK;

  int input_err = input_check(argc, argv, &P, &H, &S);                          // Spracovanie vstupu
  if (input_err != ALL_OK) return ERROR;                                        // Kontrola spracovania vstupu

  int init_err = init();                                                        // Inicializacia potrebnych zdrojov
  if (init_err != ALL_OK) return ERROR;                                         // Kontrola inicializacie potrebnych zdrojov

  pid_t hacker = fork();
  if (hacker == 0) {
    hacker_err = multiple_hackers(&P, &H);                                      // multiple_hackers() je vlastne "samostatny proces"
    exit(0);
  }
  pid_t serf = fork();
  if (serf == 0) {
    serf_err = multiple_serfs(&P, &S);                                          // multiple_serfs() je vlastne "samostatny proces"
    exit(0);
  }
  wait(NULL);                                                                   // Cakanie na prvy podporces
  wait(NULL);                                                                   // Cakanie na druhy podporces

  clean();                                                                      // v~ Ukoncenie hlavneho programu
  if (hacker_err != ALL_OK)  return ERROR;
  if (serf_err != ALL_OK)    return ERROR;
  return ALL_OK;
}

int input_check(int argc, char *argv[], int *P, int *H, int *S) {
  int arg_error = ALL_OK;
  char *endptr = NULL;                                                          // Pomocna premenna

  if (argc != 7) {                                                              // Ocakavanie 6 vstupnych parametrov
    fprintf(stderr, "%s\n", "wrong number of arguments");
    return ERROR;
  }
                                                                                // v~ Spracovanie argumentov && Kontrola typu
  *P = strtol (argv[1], &endptr, 10);
  if (*endptr != '\0') {
    fprintf(stderr, "%s\n", "argument 'P' must be an integer");
    arg_error = ERROR;
  }
  *H = strtol (argv[2], &endptr, 10);
  if (*endptr != '\0') {
    fprintf(stderr, "%s\n", "argument 'H' must be an integer");
    arg_error = ERROR;
  }
  *S = strtol (argv[3], &endptr, 10);
  if (*endptr != '\0') {
    fprintf(stderr, "%s\n", "argument 'S' must be an integer");
    arg_error = ERROR;
  }
  R = strtol (argv[4], &endptr, 10);
  if (*endptr != '\0') {
    fprintf(stderr, "%s\n", "argument 'R' must be an integer");
    arg_error = ERROR;
  }
  W = strtol (argv[5], &endptr, 10);
  if (*endptr != '\0') {
    fprintf(stderr, "%s\n", "argument 'W' must be an integer");
    arg_error = ERROR;
  }
  C = strtol (argv[6], &endptr, 10);
  if (*endptr != '\0') {
    fprintf(stderr, "%s\n", "argument 'C' must be an integer");
    arg_error = ERROR;
  }

  if (arg_error != ALL_OK) return arg_error;                                    // Ukoncenie ak bol zadany argument/argumenty nespravneho typu

  if (*P < 2 || (*P % 2 != 0)) {                                                // P >= 2 && (P % 2) == 0
    fprintf(stderr, "%s\n", "wrong 'P' argument value");
    arg_error = ERROR;
  }
  if (*H < 0 || *H > 2000)     {                                                // H >= 0 && H <= 2000
    fprintf(stderr, "%s\n", "wrong 'H' argument value");
    arg_error = ERROR;
  }
  if (*S < 0 || *S > 2000)    {                                                 // S >= 0 && S <= 2000
    fprintf(stderr, "%s\n", "wrong 'S' argument value");
    arg_error = ERROR;
  }
  if (R < 0 || R > 2000)    {                                                   // R >= 0 && R <= 2000
    fprintf(stderr, "%s\n", "wrong 'R' argument value");
    arg_error = ERROR;
  }
  if (W < 20 || W > 2000)   {                                                   // W >= 20 && W <= 2000
    fprintf(stderr, "%s\n", "wrong 'W' argument value");
    arg_error = ERROR;
  }
  if (C < 5)                 {                                                  // C >= 5
    fprintf(stderr, "%s\n", "wrong 'C' argument value");
    arg_error = ERROR;
  }

  return arg_error;
}

int multiple_hackers(int *P, int *H) {
  int hacker_error = ALL_OK;
  srand(time(0));

  for(int i=1;i<=*P;i++) {                                                      // Generovanie 'P' procesov
    usleep(rand()%(*H+1) * 1000);                                               // Náhodné oneskorenie = <0;H> (ms)
    if (fork() == 0) {
      if (hacker(&i) != ALL_OK) {                                               // hacker() je vlastne "samostatny proces" s indexom 'i'
        fprintf(stderr, "%i%s\n", i, ". hacker process failed");
        hacker_error = ERROR;
      }
      exit(0);
    }
  }

  for(int i=1;i<=*P;i++) wait(NULL);                                            // Cakanie na 'P' ukoncenych podprocesov
  return hacker_error;
}
int multiple_serfs(int *P, int *S) {
  int serf_error = ALL_OK;
  srand(time(0));

  for(int i=1;i<=*P;i++) {                                                      // Generovanie 'P' procesov
    usleep(rand()%(*S+1) * 1000);                                               // Nahodne oneskorenie = <0;S> (ms)
    if (fork() == 0) {
      if (serf(&i) != ALL_OK) {                                                 // serf() je vlastne "samostatny proces" s indexom 'i'
        fprintf(stderr, "%i%s\n", i, ". serf process failed");
        serf_error = ERROR;
      }
      exit(0);
    }
  }

  for(int i=1;i<=*P;i++) wait(NULL);                                            // Cakanie na 'P' ukoncenych podprocesov
  return serf_error;
}

int hacker(int *I) {
  srand(time(0));

  sem_wait(output_sem);                                                                                   // START
  sem_wait(act_idx_sem);
  fprintf(output_file, "%i: %s %i: %s\n", ++*A, "HACK", *I, "starts");                                    // A: HACK I: starts
  sem_post(act_idx_sem);
  sem_post(output_sem);

  while(1) {                                                                                              // MOLO
    sem_wait(output_sem);
    sem_wait(act_idx_sem);
    sem_wait(hck_pier_sem);
    sem_wait(srf_pier_sem);

    if ((*NH + *NS) < C) {                                                                                // Kontrola miesta na mole
      fprintf(output_file, "%i: %s %i: %s: %i: %i\n", ++*A, "HACK", *I, "waits", ++*NH, *NS);             // A: HACK I: waits: NH: NS
      sem_post(srf_pier_sem);
      sem_post(hck_pier_sem);
      sem_post(act_idx_sem);
      sem_post(output_sem);
      break;
    }
    else {
      fprintf(output_file, "%i: %s %i: %s: %i: %i\n", ++*A, "HACK", *I, "leavesqueue", *NH, *NS);         // A: HACK I: leavesqueue: NH: NS
      sem_post(srf_pier_sem);
      sem_post(hck_pier_sem);
      sem_post(act_idx_sem);
      sem_post(output_sem);

      usleep(rand()%(W-19) * 1000 + 20000);                                                               // Odchod na nahodnu dobu = <20;W> (ms)

      sem_wait(output_sem);
      sem_wait(act_idx_sem);
      fprintf(output_file, "%i: %s %i: %s\n", ++*A, "HACK", *I, "isback");                                // A: HACK I: isback
      sem_post(act_idx_sem);
      sem_post(output_sem);
    }
  }

  while(1) {                                                                                              // PLAVBA
    sem_wait(empty_sem);
    if (*empty == 0) {
      sem_wait(hck_pier_sem);                                                                             // Lod je prazdna
      sem_wait(srf_pier_sem);
      sem_wait(ship_sem);

      if (*NH == 4) {                                                                                     // Skupina 4 hackerov
        if (*ship < 3) {                                                                                  // Proces je clen posadky
          *ship = *ship + 1;                                                                              // Nalodenie
          sem_post(ship_sem);
          sem_post(srf_pier_sem);
          sem_post(hck_pier_sem);
          sem_post(empty_sem);

          while (*ship != 4);                                                                             // Naplnenie posadky

          sem_wait(output_sem);
          sem_wait(act_idx_sem);
          sem_wait(hck_pier_sem);
          sem_wait(srf_pier_sem);
          fprintf(output_file, "%i: %s %i: %s: %i: %i\n", ++*A, "HACK", *I, "memberexits", *NH, *NS);     // A: HACK I: memberexits: NH: NS
          sem_post(srf_pier_sem);
          sem_post(hck_pier_sem);
          sem_post(act_idx_sem);
          sem_post(output_sem);

          sem_wait(ship_sem);                                                                             // Kapitan zablokuje lod po dobu plavby
          *ship = *ship - 1;                                                                              // Opustenie lode
          sem_post(ship_sem);

          break;
        }
        else if (*ship == 3) {                                                                            // Proces je kapitan
          *empty = 1;                                                                                     // Lod je plna
          *ship = *ship + 1;                                                                              // Nalodenie kapitana (ship == 4)
          *NH = *NH - 4;                                                                                  // Odchod z rady
          sem_post(empty_sem);

          sem_wait(output_sem);
          sem_wait(act_idx_sem);
          fprintf(output_file, "%i: %s %i: %s: %i: %i\n", ++*A, "HACK", *I, "boards", *NH, *NS);          // A: HACK I: boards: NH: NS
          sem_post(srf_pier_sem);
          sem_post(hck_pier_sem);
          sem_post(act_idx_sem);
          sem_post(output_sem);

          usleep(rand()%(R+1) * 1000);                                                                    // Plavba = <0;R> (ms)
          sem_post(ship_sem);

          while (1) {                                                                                     // Kapitan sa vyloduje ako posledny
            sem_wait(ship_sem);
            if (*ship == 1) {
              sem_wait(output_sem);
              sem_wait(act_idx_sem);
              sem_wait(hck_pier_sem);
              sem_wait(srf_pier_sem);
              fprintf(output_file, "%i: %s %i: %s: %i: %i\n", ++*A, "HACK", *I, "captainexits", *NH, *NS);// A: HACK I: captainexits: NH: NS
              sem_post(srf_pier_sem);
              sem_post(hck_pier_sem);
              sem_post(act_idx_sem);
              sem_post(output_sem);

              *ship = *ship - 1;                                                                          // Opustenie lode
              sem_post(ship_sem);

              sem_wait(empty_sem);                                                                        // Lod je prazdna
              *empty = 0;
              sem_post(empty_sem);

              break;
            }
            else {                                                                                        // Cakanie na ostatnych clenov
              sem_post(ship_sem);
            }
          }
          break;
        }
      }
      else if (*NH == 2 && *NS >= 2) {                                                                    // Skupina 2 hackerov a 2 serfov
        if (*ship < 3) {                                                                                  // Proces je clen posadky
          *ship = *ship + 1;                                                                              // Nalodenie
          sem_post(ship_sem);
          sem_post(srf_pier_sem);
          sem_post(hck_pier_sem);
          sem_post(empty_sem);

          while (*ship != 4);                                                                             // Naplnenie posadky

          sem_wait(output_sem);
          sem_wait(act_idx_sem);
          sem_wait(hck_pier_sem);
          sem_wait(srf_pier_sem);
          fprintf(output_file, "%i: %s %i: %s: %i: %i\n", ++*A, "HACK", *I, "memberexits", *NH, *NS);     // A: HACK I: memberexits: NH: NS
          sem_post(srf_pier_sem);
          sem_post(hck_pier_sem);
          sem_post(act_idx_sem);
          sem_post(output_sem);

          sem_wait(ship_sem);                                                                             // Kapitan zablokuje lod po dobu plavby
          *ship = *ship - 1;                                                                              // Opustenie lode
          sem_post(ship_sem);

          break;
        }
        else if (*ship == 3) {                                                                            // Proces je kapitan
          *empty = 1;                                                                                     // Lod je plna
          *ship = *ship + 1;                                                                              // Nalodenie kapitana (ship == 4)
          *NH = *NH - 2;                                                                                  // Odchod z rady
          *NS = *NS - 2;                                                                                  // Odchod z rady
          sem_post(empty_sem);

          sem_wait(output_sem);
          sem_wait(act_idx_sem);
          fprintf(output_file, "%i: %s %i: %s: %i: %i\n", ++*A, "HACK", *I, "boards", *NH, *NS);          // A: HACK I: boards: NH: NS
          sem_post(srf_pier_sem);
          sem_post(hck_pier_sem);
          sem_post(act_idx_sem);
          sem_post(output_sem);

          usleep(rand()%(R+1) * 1000);                                                                    // Plavba = <0;R> (ms)
          sem_post(ship_sem);

          while (1) {                                                                                     // Kapitan sa vyloduje ako posledny
            sem_wait(ship_sem);
            if (*ship == 1) {
              sem_wait(output_sem);
              sem_wait(act_idx_sem);
              sem_wait(hck_pier_sem);
              sem_wait(srf_pier_sem);
              fprintf(output_file, "%i: %s %i: %s: %i: %i\n", ++*A, "HACK", *I, "captainexits", *NH, *NS);// A: HACK I: captainexits: NH: NS
              sem_post(srf_pier_sem);
              sem_post(hck_pier_sem);
              sem_post(act_idx_sem);
              sem_post(output_sem);

              *ship = *ship - 1;                                                                          // Opustenie lode
              sem_post(ship_sem);

              sem_wait(empty_sem);                                                                        // Lod je prazdna
              *empty = 0;
              sem_post(empty_sem);

              break;
            }
            else {                                                                                        // Cakanie na ostatnych clenov
              sem_post(ship_sem);
            }
          }
          break;
        }
      }
      else {                                                                                              // Neexistuje vhodna skupina
        sem_post(ship_sem);
        sem_post(srf_pier_sem);
        sem_post(hck_pier_sem);
        sem_post(empty_sem);
      }
    }
    else {                                                                                                // Lod je plna
      sem_post(empty_sem);
    }
  }

  return ALL_OK;
}
int serf(int *I) {
  srand(time(0));

  sem_wait(output_sem);                                                                                   // START
  sem_wait(act_idx_sem);
  fprintf(output_file, "%i: %s %i: %s\n", ++*A, "SERF", *I, "starts");                                    // A: SERF I: starts
  sem_post(act_idx_sem);
  sem_post(output_sem);

  while(1) {                                                                                              // MOLO
    sem_wait(output_sem);
    sem_wait(act_idx_sem);
    sem_wait(hck_pier_sem);
    sem_wait(srf_pier_sem);

    if ((*NH + *NS) < C) {                                                                                // Kontrola miesta na mole
      fprintf(output_file, "%i: %s %i: %s: %i: %i\n", ++*A, "SERF", *I, "waits", *NH, ++*NS);             // A: SERF I: waits: NH: NS
      sem_post(srf_pier_sem);
      sem_post(hck_pier_sem);
      sem_post(act_idx_sem);
      sem_post(output_sem);
      break;
    }
    else {
      fprintf(output_file, "%i: %s %i: %s: %i: %i\n", ++*A, "SERF", *I, "leavesqueue", *NH, *NS);         // A: SERF I: leavesqueue: NH: NS
      sem_post(srf_pier_sem);
      sem_post(hck_pier_sem);
      sem_post(act_idx_sem);
      sem_post(output_sem);

      usleep(rand()%(W-19) * 1000 + 20000);                                                               // Proces odide na nahodnu dobu = <20;W> (ms)

      sem_wait(output_sem);
      sem_wait(act_idx_sem);
      fprintf(output_file, "%i: %s %i: %s\n", ++*A, "SERF", *I, "isback");                                // A: SERF I: is back
      sem_post(act_idx_sem);
      sem_post(output_sem);
    }
  }

  while(1) {                                                                                              // PLAVBA
    sem_wait(empty_sem);
    if (*empty == 0) {
      sem_wait(hck_pier_sem);                                                                             // Lod je prazdna
      sem_wait(srf_pier_sem);
      sem_wait(ship_sem);

      if (*NS == 4) {                                                                                     // Skupina 4 serfov
        if (*ship < 3) {                                                                                  // Proces je clen posadky
          *ship = *ship + 1;                                                                              // Nalodenie
          sem_post(ship_sem);
          sem_post(srf_pier_sem);
          sem_post(hck_pier_sem);
          sem_post(empty_sem);

          while (*ship != 4);                                                                             // Naplnenie posadky

          sem_wait(output_sem);
          sem_wait(act_idx_sem);
          sem_wait(hck_pier_sem);
          sem_wait(srf_pier_sem);
          fprintf(output_file, "%i: %s %i: %s: %i: %i\n", ++*A, "SERF", *I, "memberexits", *NH, *NS);     // A: SERF I: memberexits: NH: NS
          sem_post(srf_pier_sem);
          sem_post(hck_pier_sem);
          sem_post(act_idx_sem);
          sem_post(output_sem);

          sem_wait(ship_sem);                                                                             // Kapitan zablokuje lod po dobu plavby
          *ship = *ship - 1;                                                                              // Opustenie lode
          sem_post(ship_sem);

          break;
        }
        else if (*ship == 3) {                                                                            // Proces je kapitan
          *empty = 1;                                                                                     // Lod je plna
          *ship = *ship + 1;                                                                              // Nalodenie kapitana (ship == 4)
          *NS = *NS - 4;                                                                                  // Odchod z rady
          sem_post(empty_sem);

          sem_wait(output_sem);
          sem_wait(act_idx_sem);
          fprintf(output_file, "%i: %s %i: %s: %i: %i\n", ++*A, "SERF", *I, "boards", *NH, *NS);          // A: SERF I: boards: NH: NS
          sem_post(srf_pier_sem);
          sem_post(hck_pier_sem);
          sem_post(act_idx_sem);
          sem_post(output_sem);

          usleep(rand()%(R+1) * 1000);                                                                    // Plavba = <0;R> (ms)
          sem_post(ship_sem);

          while (1) {                                                                                     // Kapitan sa vyloduje ako posledny
            sem_wait(ship_sem);
            if (*ship == 1) {
              sem_wait(output_sem);
              sem_wait(act_idx_sem);
              sem_wait(hck_pier_sem);
              sem_wait(srf_pier_sem);
              fprintf(output_file, "%i: %s %i: %s: %i: %i\n", ++*A, "SERF", *I, "captainexits", *NH, *NS);// A: SERF I: captainexits: NH: NS
              sem_post(srf_pier_sem);
              sem_post(hck_pier_sem);
              sem_post(act_idx_sem);
              sem_post(output_sem);

              *ship = *ship - 1;                                                                          // Opustenie lode
              sem_post(ship_sem);

              sem_wait(empty_sem);                                                                        // Lod je prazdna
              *empty = 0;
              sem_post(empty_sem);

              break;
            }
            else {                                                                                        // Cakanie na ostatnych clenov
              sem_post(ship_sem);
            }
          }
          break;
        }
      }
      else if (*NS == 2 && *NH >= 2) {                                                                    // Skupina 2 hackerov a 2 serfov
        if (*ship < 3) {                                                                                  // Proces je clen posadky
          *ship = *ship + 1;                                                                              // Nalodenie
          sem_post(ship_sem);
          sem_post(srf_pier_sem);
          sem_post(hck_pier_sem);
          sem_post(empty_sem);

          while (*ship != 4);                                                                             // Naplnenie posadky

          sem_wait(output_sem);
          sem_wait(act_idx_sem);
          sem_wait(hck_pier_sem);
          sem_wait(srf_pier_sem);
          fprintf(output_file, "%i: %s %i: %s: %i: %i\n", ++*A, "SERF", *I, "memberexits", *NH, *NS);     // A: SERF I: memberexits: NH: NS
          sem_post(srf_pier_sem);
          sem_post(hck_pier_sem);
          sem_post(act_idx_sem);
          sem_post(output_sem);

          sem_wait(ship_sem);                                                                             // Kapitan zablokuje lod po dobu plavby
          *ship = *ship - 1;                                                                              // Opustenie lode
          sem_post(ship_sem);

          break;
        }
        else if (*ship == 3) {                                                                            // Proces je kapitan
          *empty = 1;                                                                                     // Lod je plna
          *ship = *ship + 1;                                                                              // Nalodenie kapitana (ship == 4)
          *NH = *NH - 2;                                                                                  // Odchod z rady
          *NS = *NS - 2;                                                                                  // Odchod z rady
          sem_post(empty_sem);

          sem_wait(output_sem);
          sem_wait(act_idx_sem);
          fprintf(output_file, "%i: %s %i: %s: %i: %i\n", ++*A, "SERF", *I, "boards", *NH, *NS);          // A: SERF I: boards: NH: NS
          sem_post(srf_pier_sem);
          sem_post(hck_pier_sem);
          sem_post(act_idx_sem);
          sem_post(output_sem);

          usleep(rand()%(R+1) * 1000);                                                                    // Plavba = <0;R> (ms)
          sem_post(ship_sem);

          while (1) {                                                                                     // Kapitan sa vyloduje ako posledny
            sem_wait(ship_sem);
            if (*ship == 1) {
              sem_wait(output_sem);
              sem_wait(act_idx_sem);
              sem_wait(hck_pier_sem);
              sem_wait(srf_pier_sem);
              fprintf(output_file, "%i: %s %i: %s: %i: %i\n", ++*A, "SERF", *I, "captainexits", *NH, *NS);// A: SERF I: captainexits: NH: NS
              sem_post(srf_pier_sem);
              sem_post(hck_pier_sem);
              sem_post(act_idx_sem);
              sem_post(output_sem);

              *ship = *ship - 1;                                                                          // Opustenie lode
              sem_post(ship_sem);

              sem_wait(empty_sem);                                                                        // Lod je prazdna
              *empty = 0;
              sem_post(empty_sem);

              break;
            }
            else {                                                                                        // Cakanie na ostatnych clenov
              sem_post(ship_sem);
            }
          }
          break;
        }
      }
      else {                                                                                              // Neexistuje vhodna skupina
        sem_post(ship_sem);
        sem_post(srf_pier_sem);
        sem_post(hck_pier_sem);
        sem_post(empty_sem);
      }
    }
    else {                                                                                                // Lod je plna
      sem_post(empty_sem);
    }
  }

  return ALL_OK;
}

int init() {
  // Vystupny subor:
  output_file = fopen("proj2.out", "w+");                                                   // Otvorenie vystupneho suboru
  if (output_file == NULL) {                                                                // Kontrola otvorenia vystupneho suboru
    fprintf(stderr, "%s\n", "'proj2.out' couldn't be opened");
    return ERROR;
  }
  setbuf(output_file, NULL);

  // output_file = stdout;                                                                  // /*FOR DEBUGGING*/

  // Otvorenie semaforov:
  output_sem   = sem_open("/xfulla00_IOS_proj2_output_sem", O_CREAT | O_EXCL, 0644, 1);     // Semafor pre zapis do suboru pre vystup

  act_idx_sem  = sem_open("/xfulla00_IOS_proj2_act_idx_sem", O_CREAT | O_EXCL, 0644, 1);    // Semafor pre zapis do indexu akcii            (A)
  hck_pier_sem = sem_open("/xfulla00_IOS_proj2_hck_pier_sem", O_CREAT | O_EXCL, 0644, 1);   // Semafor pre zmenu poctu hackerov na mole     (NH)
  srf_pier_sem = sem_open("/xfulla00_IOS_proj2_srf_pier_sem", O_CREAT | O_EXCL, 0644, 1);   // Semafor pre zmenu poctu serfov na mole       (NS)
  ship_sem     = sem_open("/xfulla00_IOS_proj2_ship_sem", O_CREAT | O_EXCL, 0644, 1);       // Semafor pre zmenu poctu ludi na lodi         (ship)
  empty_sem    = sem_open("/xfulla00_IOS_proj2_empty_sem", O_CREAT | O_EXCL, 0644, 1);      // Semafor pre zmenu indikatora prazdnosti lode (empty)

  // Inicializacia zdielanych premennych:
  fd_A = shm_open("/xfulla00_SharedMemory_A", O_CREAT | O_EXCL | O_RDWR, 0644);             // Premenna index akcie (A)
  ftruncate(fd_A, sizeof(int));
  A = (int*) mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd_A, 0);

  fd_NH = shm_open("/xfulla00_SharedMemory_NH", O_CREAT | O_EXCL | O_RDWR, 0644);           // Premenna hackery na mole (NH)
  ftruncate(fd_NH, sizeof(int));
  NH = (int*) mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd_NH, 0);

  fd_NS = shm_open("/xfulla00_SharedMemory_NS", O_CREAT | O_EXCL | O_RDWR, 0644);           // Premenna serfovia na mole (NS)
  ftruncate(fd_NS, sizeof(int));
  NS = (int*) mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd_NS, 0);

  fd_ship = shm_open("/xfulla00_SharedMemory_ship", O_CREAT | O_EXCL | O_RDWR, 0644);       // Premenna pocet ludi na lodi (ship)
  ftruncate(fd_ship, sizeof(int));
  ship = (int*) mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd_ship, 0);

  fd_empty = shm_open("/xfulla00_SharedMemory_empty", O_CREAT | O_EXCL | O_RDWR, 0644);     // Premenna indikator prazdnosti lode (empty)
  ftruncate(fd_empty, sizeof(int));
  empty = (int*) mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd_empty, 0);

  return ALL_OK;
}
void clean() {
  // Vystupny subor:
  if (output_file != NULL) fclose(output_file);                                 // Zatvorenie vystupneho suboru

  // Zatvorenie semaforov:
  sem_close(output_sem);                                                        // Semafor pre zapis do suboru pre vystup
  sem_unlink("/xfulla00_IOS_proj2_output_sem");

  sem_close(act_idx_sem);                                                       // Semafor pre zapis do indexu akcii        (A)
  sem_unlink("/xfulla00_IOS_proj2_act_idx_sem");
  sem_close(hck_pier_sem);                                                      // Semafor pre zmenu poctu hackerov na mole (NH)
  sem_unlink("/xfulla00_IOS_proj2_hck_pier_sem");
  sem_close(srf_pier_sem);                                                      // Semafor pre zmenu poctu serfov na mole   (NS)
  sem_unlink("/xfulla00_IOS_proj2_srf_pier_sem");
  sem_close(ship_sem);                                                          // Semafor pre zmenu poctu ludi na lodi     (ship)
  sem_unlink("/xfulla00_IOS_proj2_ship_sem");
  sem_close(empty_sem);                                                         // Semafor pre zmenu indikatora prazdnosti lode (empty)
  sem_unlink("/xfulla00_IOS_proj2_empty_sem");

  // Deinicializacia zdielanych premennych:
  munmap(A, sizeof(int));                                                       // Premenna index akcie (A)
  shm_unlink("/xfulla00_SharedMemory_A");
  close(fd_A);

  munmap(NH, sizeof(int));                                                      // Premenna hackery na mole (NH)
  shm_unlink("/xfulla00_SharedMemory_NH");
  close(fd_NH);

  munmap(NS, sizeof(int));                                                      // Premenna serfovia na mole (NS)
  shm_unlink("/xfulla00_SharedMemory_NS");
  close(fd_NS);

  munmap(ship, sizeof(int));                                                    // Premenna pocet ludi na lodi (ship)
  shm_unlink("/xfulla00_SharedMemory_ship");

  munmap(empty, sizeof(int));                                                   // Premenna indikator prazdnosti lode (empty)
  shm_unlink("/xfulla00_SharedMemory_empty");

  close(fd_ship);
}

/******************************************************************************/
/***                         End of the IOS_proj2.c                         ***/
/******************************************************************************/
