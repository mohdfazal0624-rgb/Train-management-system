/*
* TRAIN MANAGEMENT SYSTEM
* -----------------------
* A menu-driven C program for railway ticket booking.
*
* Roles:
*   Guest     - view all trains and search for trains (no login needed)
*   Passenger - register, login, search, book tickets, cancel tickets,
*               view own bookings and tickets
*   Admin     - add / update / delete trains, view all bookings,
*               view passengers, generate a summary report
*
* C topics used: variables and data types, input/output, conditionals and
* loops, switch-case, arrays, functions, strings, structures, error
* handling and file handling (binary files).
*
* Data files (created automatically in the folder you run the program from):
*   trains.dat, users.dat, bookings.dat
*
* Default admin password: admin123
*
* Compile:  gcc train_management.c -o train_management
* Run:      ./train_management        (Windows: train_management.exe)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TRAIN_FILE      "trains.dat"
#define USER_FILE       "users.dat"
#define BOOKING_FILE    "bookings.dat"

#define ADMIN_PASSWORD  "admin123"
#define MAX_SEATS       500
#define MAX_PER_BOOKING 6
#define REFUND_PERCENT  75
#define FIRST_PNR       2400000001LL

/* ------------------------------------------------------------------ */
/*                          DATA STRUCTURES                          */
/* ------------------------------------------------------------------ */

typedef struct {
   int   id;
   char  name[40];
   char  source[30];
   char  destination[30];
   char  date[11];          /* DD-MM-YYYY */
   char  depTime[6];        /* HH:MM      */
   int   totalSeats;
   int   availableSeats;
   float fare;
} Train;

typedef struct {
   char          username[30];
   unsigned long passHash;  /* password is stored as a hash, not as text */
   char          fullName[50];
   char          phone[16];
   char          email[50];
} User;

typedef struct {
   long long pnr;
   char      username[30];
   int       trainId;
   char      passengerName[50];
   int       age;
   int       seatNo;
   float     fare;
   int       active;        /* 1 = confirmed, 0 = cancelled */
} Booking;

/* ------------------------------------------------------------------ */
/*                           INPUT HELPERS                             */
/* ------------------------------------------------------------------ */

static void readLine(const char *prompt, char *buf, size_t size)
{
   size_t len;
   int c;

   printf("%s", prompt);
   if (fgets(buf, (int)size, stdin) == NULL) {
       buf[0] = '\0';
       return;
   }
   len = strlen(buf);
   if (len > 0 && buf[len - 1] == '\n') {
       buf[len - 1] = '\0';
   } else {
       /* line was longer than the buffer: throw away the rest */
       while ((c = getchar()) != '\n' && c != EOF) { }
   }
}

static void readNonEmpty(const char *prompt, char *buf, size_t size)
{
   do {
       readLine(prompt, buf, size);
       if (buf[0] == '\0') {
           printf("  Input cannot be empty. Try again.\n");
       }
   } while (buf[0] == '\0');
}

static int readInt(const char *prompt, int min, int max)
{
   char tmp[32];
   char *end;
   long v;

   for (;;) {
       readLine(prompt, tmp, sizeof tmp);
       v = strtol(tmp, &end, 10);
       if (tmp[0] != '\0' && *end == '\0' && v >= min && v <= max) {
           return (int)v;
       }
       printf("  Please enter a whole number between %d and %d.\n", min, max);
   }
}

static float readFloat(const char *prompt, float min)
{
   char tmp[32];
   char *end;
   double v;

   for (;;) {
       readLine(prompt, tmp, sizeof tmp);
       v = strtod(tmp, &end);
       if (tmp[0] != '\0' && *end == '\0' && v >= min && v <= 1000000.0) {
           return (float)v;
       }
       printf("  Please enter a valid amount (at least %.2f).\n", min);
   }
}

static void waitForEnter(void)
{
   char tmp[8];
   readLine("\nPress Enter to continue...", tmp, sizeof tmp);
}

/* ------------------------------------------------------------------ */
/*                          VALIDATION HELPERS                         */
/* ------------------------------------------------------------------ */

static int equalsIgnoreCase(const char *a, const char *b)
{
   while (*a && *b) {
       if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
           return 0;
       }
       a++;
       b++;
   }
   return *a == *b;
}

static int validDate(const char *s)
{
   int i, day, month, year;

   if (strlen(s) != 10 || s[2] != '-' || s[5] != '-') {
       return 0;
   }
   for (i = 0; i < 10; i++) {
       if (i != 2 && i != 5 && !isdigit((unsigned char)s[i])) {
           return 0;
       }
   }
   day   = (s[0] - '0') * 10 + (s[1] - '0');
   month = (s[3] - '0') * 10 + (s[4] - '0');
   year  = (s[6] - '0') * 1000 + (s[7] - '0') * 100 +
           (s[8] - '0') * 10 + (s[9] - '0');

   if (year < 2024 || year > 2100 || month < 1 || month > 12 || day < 1) {
       return 0;
   }
   {
       int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
       int leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
       int maxDay = days[month - 1] + ((month == 2 && leap) ? 1 : 0);
       return day <= maxDay;
   }
}

static int validTime(const char *s)
{
   int h, m;

   if (strlen(s) != 5 || s[2] != ':' ||
       !isdigit((unsigned char)s[0]) || !isdigit((unsigned char)s[1]) ||
       !isdigit((unsigned char)s[3]) || !isdigit((unsigned char)s[4])) {
       return 0;
   }
   h = (s[0] - '0') * 10 + (s[1] - '0');
   m = (s[3] - '0') * 10 + (s[4] - '0');
   return h <= 23 && m <= 59;
}

static int validUsername(const char *s)
{
   size_t i, len = strlen(s);

   if (len < 3) {
       return 0;
   }
   for (i = 0; i < len; i++) {
       if (!isalnum((unsigned char)s[i]) && s[i] != '_') {
           return 0;
       }
   }
   return 1;
}

static void readDate(const char *prompt, char *buf, size_t size)
{
   do {
       readNonEmpty(prompt, buf, size);
       if (!validDate(buf)) {
           printf("  Invalid date. Use DD-MM-YYYY (e.g. 25-12-2026).\n");
       }
   } while (!validDate(buf));
}

static void readTime(const char *prompt, char *buf, size_t size)
{
   do {
       readNonEmpty(prompt, buf, size);
       if (!validTime(buf)) {
           printf("  Invalid time. Use 24-hour HH:MM (e.g. 18:45).\n");
       }
   } while (!validTime(buf));
}

/* Simple djb2 hash so passwords are not saved as plain text.
* (Educational only - real systems use bcrypt/argon2.) */
static unsigned long hashPassword(const char *s)
{
   unsigned long h = 5381;
   int c;

   while ((c = (unsigned char)*s++) != 0) {
       h = ((h << 5) + h) + (unsigned long)c;
   }
   return h;
}

/* ------------------------------------------------------------------ */
/*                           FILE HANDLING                             */
/* ------------------------------------------------------------------ */

/* Reads every record from a binary file into a malloc'd array.
* Returns NULL (and *count = 0) if the file is missing or empty.
* The caller must free() the result. */
static void *loadAll(const char *file, size_t recSize, int *count)
{
   FILE *fp;
   long size;
   int n;
   void *buf;

   *count = 0;
   fp = fopen(file, "rb");
   if (fp == NULL) {
       return NULL;
   }
   fseek(fp, 0, SEEK_END);
   size = ftell(fp);
   rewind(fp);
   if (size <= 0) {
       fclose(fp);
       return NULL;
   }
   n = (int)(size / (long)recSize);
   if (n == 0) {
       fclose(fp);
       return NULL;
   }
   buf = malloc((size_t)n * recSize);
   if (buf == NULL) {
       printf("  Error: out of memory.\n");
       fclose(fp);
       return NULL;
   }
   *count = (int)fread(buf, recSize, (size_t)n, fp);
   fclose(fp);
   return buf;
}

/* Overwrites a file with the given records. Returns 1 on success. */
static int saveAll(const char *file, const void *data, size_t recSize, int count)
{
   FILE *fp = fopen(file, "wb");

   if (fp == NULL) {
       printf("  Error: could not write to %s.\n", file);
       return 0;
   }
   if (count > 0 && fwrite(data, recSize, (size_t)count, fp) != (size_t)count) {
       printf("  Error: failed while writing to %s.\n", file);
       fclose(fp);
       return 0;
   }
   fclose(fp);
   return 1;
}

/* Adds one record to the end of a file. Returns 1 on success. */
static int appendRecord(const char *file, const void *rec, size_t recSize)
{
   FILE *fp = fopen(file, "ab");

   if (fp == NULL) {
       printf("  Error: could not write to %s.\n", file);
       return 0;
   }
   if (fwrite(rec, recSize, 1, fp) != 1) {
       printf("  Error: failed while writing to %s.\n", file);
       fclose(fp);
       return 0;
   }
   fclose(fp);
   return 1;
}

/* ------------------------------------------------------------------ */
/*                         LOOKUP / DISPLAY HELPERS                    */
/* ------------------------------------------------------------------ */

static int findTrainIndex(const Train *t, int n, int id)
{
   int i;

   for (i = 0; i < n; i++) {
       if (t[i].id == id) {
           return i;
       }
   }
   return -1;
}

static void printTrainHeader(void)
{
   printf("\n%-6s %-20s %-14s %-14s %-11s %-6s %-8s %-9s\n",
          "No.", "Train Name", "From", "To", "Date", "Dep", "Seats", "Fare(Rs)");
   printf("---------------------------------------------------------------"
          "-----------------------\n");
}

static void printTrainRow(const Train *t)
{
   char seats[24];

   snprintf(seats, sizeof seats, "%d/%d", t->availableSeats, t->totalSeats);
   printf("%-6d %-20.20s %-14.14s %-14.14s %-11s %-6s %-8s %-9.2f\n",
          t->id, t->name, t->source, t->destination,
          t->date, t->depTime, seats, t->fare);
}

static void printTicket(const Booking *b, const Train *t)
{
   printf("\n  ============== E-TICKET ==============\n");
   printf("  PNR         : %lld\n", b->pnr);
   printf("  Status      : %s\n", b->active ? "CONFIRMED" : "CANCELLED");
   printf("  Passenger   : %s (Age %d)\n", b->passengerName, b->age);
   if (t != NULL) {
       printf("  Train       : %d - %s\n", t->id, t->name);
       printf("  Route       : %s -> %s\n", t->source, t->destination);
       printf("  Date / Time : %s  %s\n", t->date, t->depTime);
   } else {
       printf("  Train       : %d (no longer in the system)\n", b->trainId);
   }
   printf("  Seat No.    : %d\n", b->seatNo);
   printf("  Fare        : Rs %.2f\n", b->fare);
   printf("  ======================================\n");
}

static long long nextPnr(const Booking *b, int n)
{
   long long max = FIRST_PNR - 1;
   int i;

   for (i = 0; i < n; i++) {
       if (b[i].pnr > max) {
           max = b[i].pnr;
       }
   }
   return max + 1;
}

/* Returns the lowest free seat number for a train, or 0 if it is full. */
static int allocateSeat(const Booking *b, int n, int trainId, int totalSeats)
{
   char used[MAX_SEATS + 2];
   int i, s;

   memset(used, 0, sizeof used);
   for (i = 0; i < n; i++) {
       if (b[i].active && b[i].trainId == trainId &&
           b[i].seatNo >= 1 && b[i].seatNo <= MAX_SEATS) {
           used[b[i].seatNo] = 1;
       }
   }
   for (s = 1; s <= totalSeats && s <= MAX_SEATS; s++) {
       if (!used[s]) {
           return s;
       }
   }
   return 0;
}

/* ------------------------------------------------------------------ */
/*                      TRAIN LISTING AND SEARCH                       */
/* ------------------------------------------------------------------ */

static void seedTrains(void)
{
   FILE *fp = fopen(TRAIN_FILE, "rb");
   Train sample[] = {
       {101, "Rajdhani Express", "New Delhi", "Mumbai",    "15-11-2026", "16:30", 60, 60, 2150.00f},
       {102, "Shatabdi Express", "New Delhi", "Lucknow",   "16-11-2026", "06:10", 50, 50, 1250.00f},
       {103, "Duronto Express",  "Kolkata",   "New Delhi", "17-11-2026", "22:00", 60, 60, 1950.00f},
       {104, "Chennai Mail",     "Bengaluru", "Chennai",   "18-11-2026", "07:45", 40, 40,  650.00f},
       {105, "Garib Rath",       "Jaipur",    "New Delhi", "20-11-2026", "14:20", 80, 80,  480.00f}
   };

   if (fp != NULL) {          /* data file already exists */
       fclose(fp);
       return;
   }
   saveAll(TRAIN_FILE, sample, sizeof(Train), (int)(sizeof sample / sizeof sample[0]));
}

static void viewAllTrains(void)
{
   int n, i;
   Train *t = (Train *)loadAll(TRAIN_FILE, sizeof(Train), &n);

   if (t == NULL) {
       printf("\n  No trains available right now.\n");
       return;
   }
   printf("\n  ALL TRAINS");
   printTrainHeader();
   for (i = 0; i < n; i++) {
       printTrainRow(&t[i]);
   }
   printf("\n  (Seats column shows available/total)\n");
   free(t);
}

static void searchTrains(void)
{
   char src[30], dst[30], date[11];
   int n, i, found = 0;
   Train *t;

   readNonEmpty("  Source station      : ", src, sizeof src);
   readNonEmpty("  Destination station : ", dst, sizeof dst);
   for (;;) {
       readLine("  Journey date DD-MM-YYYY (Enter = any date): ", date, sizeof date);
       if (date[0] == '\0' || validDate(date)) {
           break;
       }
       printf("  Invalid date. Use DD-MM-YYYY or press Enter for any date.\n");
   }

   t = (Train *)loadAll(TRAIN_FILE, sizeof(Train), &n);
   if (t == NULL) {
       printf("\n  No trains available right now.\n");
       return;
   }
   for (i = 0; i < n; i++) {
       if (equalsIgnoreCase(t[i].source, src) &&
           equalsIgnoreCase(t[i].destination, dst) &&
           (date[0] == '\0' || strcmp(t[i].date, date) == 0)) {
           if (!found) {
               printf("\n  SEARCH RESULTS");
               printTrainHeader();
           }
           printTrainRow(&t[i]);
           found++;
       }
   }
   if (!found) {
       printf("\n  No trains found for that route/date.\n");
   } else {
       printf("\n  %d train(s) found. (Seats column shows available/total)\n", found);
   }
   free(t);
}

/* ------------------------------------------------------------------ */
/*                         PASSENGER OPERATIONS                        */
/* ------------------------------------------------------------------ */

static void bookTicket(const User *u)
{
   int tn, bn, ti, count, i, maxCount;
   int id;
   long long pnr;
   Train *t;
   Booking *b;

   t = (Train *)loadAll(TRAIN_FILE, sizeof(Train), &tn);
   if (t == NULL) {
       printf("\n  No trains available right now.\n");
       return;
   }
   printf("\n  BOOK A TICKET");
   printTrainHeader();
   for (i = 0; i < tn; i++) {
       printTrainRow(&t[i]);
   }

   id = readInt("\n  Enter train number to book (0 to go back): ", 0, 99999);
   if (id == 0) {
       free(t);
       return;
   }
   ti = findTrainIndex(t, tn, id);
   if (ti < 0) {
       printf("  Train %d does not exist.\n", id);
       free(t);
       return;
   }
   if (t[ti].availableSeats <= 0) {
       printf("  Sorry, train %d is fully booked.\n", id);
       free(t);
       return;
   }

   maxCount = t[ti].availableSeats < MAX_PER_BOOKING ? t[ti].availableSeats
                                                     : MAX_PER_BOOKING;
   count = readInt("  Number of passengers: ", 1, maxCount);

   b = (Booking *)loadAll(BOOKING_FILE, sizeof(Booking), &bn);
   pnr = nextPnr(b, bn);

   for (i = 0; i < count; i++) {
       Booking nb;
       Booking *grown;

       memset(&nb, 0, sizeof nb);
       printf("\n  Passenger %d of %d\n", i + 1, count);
       readNonEmpty("    Name : ", nb.passengerName, sizeof nb.passengerName);
       nb.age = readInt("    Age  : ", 1, 120);

       nb.seatNo = allocateSeat(b, bn, id, t[ti].totalSeats);
       if (nb.seatNo == 0) {
           printf("  No seats left. Stopping booking.\n");
           break;
       }
       nb.pnr = pnr++;
       strncpy(nb.username, u->username, sizeof nb.username - 1);
       nb.trainId = id;
       nb.fare = t[ti].fare;
       nb.active = 1;

       grown = (Booking *)realloc(b, (size_t)(bn + 1) * sizeof(Booking));
       if (grown == NULL) {
           printf("  Error: out of memory. Booking stopped.\n");
           break;
       }
       b = grown;
       b[bn++] = nb;
       t[ti].availableSeats--;
       printf("\n  Booking confirmed!");
       printTicket(&nb, &t[ti]);
   }

   if (saveAll(BOOKING_FILE, b, sizeof(Booking), bn)) {
       saveAll(TRAIN_FILE, t, sizeof(Train), tn);
   }
   free(b);
   free(t);
}

static void viewMyBookings(const User *u)
{
   int bn, tn, i, shown = 0;
   Booking *b = (Booking *)loadAll(BOOKING_FILE, sizeof(Booking), &bn);
   Train *t = (Train *)loadAll(TRAIN_FILE, sizeof(Train), &tn);

   for (i = 0; i < bn; i++) {
       if (strcmp(b[i].username, u->username) == 0) {
           int ti = findTrainIndex(t, tn, b[i].trainId);
           printTicket(&b[i], ti >= 0 ? &t[ti] : NULL);
           shown++;
       }
   }
   if (!shown) {
       printf("\n  You have no bookings yet.\n");
   }
   free(b);
   free(t);
}

static void viewTicketByPnr(const User *u)
{
   char tmp[32];
   char *end;
   long long pnr;
   int bn, tn, i, ti;
   Booking *b;
   Train *t;

   readNonEmpty("  Enter PNR: ", tmp, sizeof tmp);
   pnr = strtoll(tmp, &end, 10);
   if (*end != '\0') {
       printf("  Invalid PNR.\n");
       return;
   }
   b = (Booking *)loadAll(BOOKING_FILE, sizeof(Booking), &bn);
   t = (Train *)loadAll(TRAIN_FILE, sizeof(Train), &tn);
   for (i = 0; i < bn; i++) {
       if (b[i].pnr == pnr && strcmp(b[i].username, u->username) == 0) {
           ti = findTrainIndex(t, tn, b[i].trainId);
           printTicket(&b[i], ti >= 0 ? &t[ti] : NULL);
           free(b);
           free(t);
           return;
       }
   }
   printf("  No ticket with that PNR found in your account.\n");
   free(b);
   free(t);
}

static void cancelTicket(const User *u)
{
   char tmp[32], answer[8];
   char *end;
   long long pnr;
   int bn, tn, i, ti;
   Booking *b;
   Train *t;

   readNonEmpty("  Enter PNR to cancel: ", tmp, sizeof tmp);
   pnr = strtoll(tmp, &end, 10);
   if (*end != '\0') {
       printf("  Invalid PNR.\n");
       return;
   }
   b = (Booking *)loadAll(BOOKING_FILE, sizeof(Booking), &bn);
   t = (Train *)loadAll(TRAIN_FILE, sizeof(Train), &tn);

   for (i = 0; i < bn; i++) {
       if (b[i].pnr == pnr && strcmp(b[i].username, u->username) == 0) {
           break;
       }
   }
   if (i == bn) {
       printf("  No ticket with that PNR found in your account.\n");
   } else if (!b[i].active) {
       printf("  That ticket is already cancelled.\n");
   } else {
       ti = findTrainIndex(t, tn, b[i].trainId);
       printTicket(&b[i], ti >= 0 ? &t[ti] : NULL);
       readLine("\n  Cancel this ticket? (y/n): ", answer, sizeof answer);
       if (tolower((unsigned char)answer[0]) == 'y') {
           b[i].active = 0;
           if (ti >= 0) {
               t[ti].availableSeats++;
           }
           if (saveAll(BOOKING_FILE, b, sizeof(Booking), bn) && ti >= 0) {
               saveAll(TRAIN_FILE, t, sizeof(Train), tn);
           }
           printf("  Ticket cancelled. Refund: Rs %.2f (%d%% of fare).\n",
                  b[i].fare * REFUND_PERCENT / 100.0f, REFUND_PERCENT);
       } else {
           printf("  Cancellation aborted.\n");
       }
   }
   free(b);
   free(t);
}

static void passengerMenu(const User *u)
{
   int choice;

   do {
       printf("\n========== PASSENGER MENU (%s) ==========\n", u->username);
       printf("  1. View all trains\n");
       printf("  2. Search trains\n");
       printf("  3. Book ticket\n");
       printf("  4. Cancel ticket\n");
       printf("  5. My bookings\n");
       printf("  6. View ticket by PNR\n");
       printf("  0. Logout\n");
       choice = readInt("Enter choice: ", 0, 6);

       switch (choice) {
           case 1: viewAllTrains();      waitForEnter(); break;
           case 2: searchTrains();       waitForEnter(); break;
           case 3: bookTicket(u);        waitForEnter(); break;
           case 4: cancelTicket(u);      waitForEnter(); break;
           case 5: viewMyBookings(u);    waitForEnter(); break;
           case 6: viewTicketByPnr(u);   waitForEnter(); break;
           case 0: printf("  Logged out.\n"); break;
       }
   } while (choice != 0);
}

/* ------------------------------------------------------------------ */
/*                      REGISTRATION AND LOGIN                         */
/* ------------------------------------------------------------------ */

static void registerPassenger(void)
{
   User nu;
   char pass[40];
   int n, i;
   User *users;

   memset(&nu, 0, sizeof nu);
   printf("\n  PASSENGER REGISTRATION\n");

   users = (User *)loadAll(USER_FILE, sizeof(User), &n);
   for (;;) {
       readNonEmpty("  Choose username (letters/digits/_ , min 3): ",
                    nu.username, sizeof nu.username);
       if (!validUsername(nu.username)) {
           printf("  Invalid username.\n");
           continue;
       }
       for (i = 0; i < n; i++) {
           if (equalsIgnoreCase(users[i].username, nu.username)) {
               break;
           }
       }
       if (i < n) {
           printf("  That username is already taken.\n");
           continue;
       }
       break;
   }
   free(users);

   do {
       readNonEmpty("  Choose password (min 4 characters): ", pass, sizeof pass);
       if (strlen(pass) < 4) {
           printf("  Password too short.\n");
       }
   } while (strlen(pass) < 4);
   nu.passHash = hashPassword(pass);

   readNonEmpty("  Full name : ", nu.fullName, sizeof nu.fullName);
   readNonEmpty("  Phone     : ", nu.phone, sizeof nu.phone);
   readNonEmpty("  Email     : ", nu.email, sizeof nu.email);

   if (appendRecord(USER_FILE, &nu, sizeof nu)) {
       printf("\n  Registration successful! You can now log in.\n");
   }
}

static void passengerLogin(void)
{
   char username[30], pass[40];
   int n, i, attempt;
   User *users = (User *)loadAll(USER_FILE, sizeof(User), &n);

   if (users == NULL) {
       printf("\n  No registered passengers yet. Please register first.\n");
       return;
   }
   printf("\n  PASSENGER LOGIN\n");
   for (attempt = 1; attempt <= 3; attempt++) {
       readNonEmpty("  Username: ", username, sizeof username);
       readNonEmpty("  Password: ", pass, sizeof pass);
       for (i = 0; i < n; i++) {
           if (equalsIgnoreCase(users[i].username, username) &&
               users[i].passHash == hashPassword(pass)) {
               User logged = users[i];
               printf("\n  Welcome, %s!\n", logged.fullName);
               free(users);
               passengerMenu(&logged);
               return;
           }
       }
       printf("  Wrong username or password (attempt %d of 3).\n", attempt);
   }
   printf("  Too many failed attempts. Returning to main menu.\n");
   free(users);
}

/* ------------------------------------------------------------------ */
/*                           ADMIN OPERATIONS                          */
/* ------------------------------------------------------------------ */

static void adminAddTrain(void)
{
   int n;
   Train nt;
   Train *t = (Train *)loadAll(TRAIN_FILE, sizeof(Train), &n);

   memset(&nt, 0, sizeof nt);
   printf("\n  ADD NEW TRAIN\n");
   nt.id = readInt("  Train number (1-99999): ", 1, 99999);
   if (findTrainIndex(t, n, nt.id) >= 0) {
       printf("  A train with that number already exists.\n");
       free(t);
       return;
   }
   free(t);

   readNonEmpty("  Train name          : ", nt.name, sizeof nt.name);
   readNonEmpty("  Source station      : ", nt.source, sizeof nt.source);
   readNonEmpty("  Destination station : ", nt.destination, sizeof nt.destination);
   readDate("  Journey date DD-MM-YYYY : ", nt.date, sizeof nt.date);
   readTime("  Departure time HH:MM    : ", nt.depTime, sizeof nt.depTime);
   nt.totalSeats = readInt("  Total seats (1-500) : ", 1, MAX_SEATS);
   nt.availableSeats = nt.totalSeats;
   nt.fare = readFloat("  Fare per seat (Rs)  : ", 1.0f);

   if (appendRecord(TRAIN_FILE, &nt, sizeof nt)) {
       printf("  Train added successfully.\n");
   }
}

static void adminUpdateTrain(void)
{
   int n, ti, id, choice, booked, newTotal;
   Train *t = (Train *)loadAll(TRAIN_FILE, sizeof(Train), &n);

   if (t == NULL) {
       printf("\n  No trains in the system.\n");
       return;
   }
   id = readInt("\n  Enter train number to update: ", 1, 99999);
   ti = findTrainIndex(t, n, id);
   if (ti < 0) {
       printf("  Train not found.\n");
       free(t);
       return;
   }
   printTrainHeader();
   printTrainRow(&t[ti]);

   printf("\n  1. Change journey date\n");
   printf("  2. Change departure time\n");
   printf("  3. Change fare\n");
   printf("  4. Change total seats\n");
   printf("  0. Cancel\n");
   choice = readInt("  Enter choice: ", 0, 4);

   switch (choice) {
       case 1:
           readDate("  New date DD-MM-YYYY: ", t[ti].date, sizeof t[ti].date);
           break;
       case 2:
           readTime("  New departure time HH:MM: ", t[ti].depTime, sizeof t[ti].depTime);
           break;
       case 3:
           t[ti].fare = readFloat("  New fare (Rs): ", 1.0f);
           break;
       case 4:
           booked = t[ti].totalSeats - t[ti].availableSeats;
           newTotal = readInt("  New total seats: ", booked > 1 ? booked : 1, MAX_SEATS);
           t[ti].totalSeats = newTotal;
           t[ti].availableSeats = newTotal - booked;
           break;
       default:
           free(t);
           return;
   }
   if (saveAll(TRAIN_FILE, t, sizeof(Train), n)) {
       printf("  Train updated successfully.\n");
   }
   free(t);
}

static void adminDeleteTrain(void)
{
   int tn, bn, ti, id, i;
   Train *t = (Train *)loadAll(TRAIN_FILE, sizeof(Train), &tn);
   Booking *b;

   if (t == NULL) {
       printf("\n  No trains in the system.\n");
       return;
   }
   id = readInt("\n  Enter train number to delete: ", 1, 99999);
   ti = findTrainIndex(t, tn, id);
   if (ti < 0) {
       printf("  Train not found.\n");
       free(t);
       return;
   }
   b = (Booking *)loadAll(BOOKING_FILE, sizeof(Booking), &bn);
   for (i = 0; i < bn; i++) {
       if (b[i].trainId == id && b[i].active) {
           printf("  Cannot delete: this train still has confirmed bookings.\n");
           free(b);
           free(t);
           return;
       }
   }
   free(b);

   memmove(&t[ti], &t[ti + 1], (size_t)(tn - ti - 1) * sizeof(Train));
   tn--;
   if (saveAll(TRAIN_FILE, t, sizeof(Train), tn)) {
       printf("  Train deleted.\n");
   }
   free(t);
}

static void adminViewBookings(void)
{
   int bn, i;
   Booking *b = (Booking *)loadAll(BOOKING_FILE, sizeof(Booking), &bn);

   if (b == NULL) {
       printf("\n  No bookings yet.\n");
       return;
   }
   printf("\n  ALL BOOKINGS\n");
   printf("  %-11s %-14s %-6s %-20s %-4s %-5s %-9s %-10s\n",
          "PNR", "User", "Train", "Passenger", "Age", "Seat", "Fare", "Status");
   printf("  -------------------------------------------------------------"
          "--------------------------\n");
   for (i = 0; i < bn; i++) {
       printf("  %-11lld %-14.14s %-6d %-20.20s %-4d %-5d %-9.2f %-10s\n",
              b[i].pnr, b[i].username, b[i].trainId, b[i].passengerName,
              b[i].age, b[i].seatNo, b[i].fare,
              b[i].active ? "CONFIRMED" : "CANCELLED");
   }
   free(b);
}

static void adminViewPassengers(void)
{
   int n, i;
   User *u = (User *)loadAll(USER_FILE, sizeof(User), &n);

   if (u == NULL) {
       printf("\n  No registered passengers yet.\n");
       return;
   }
   printf("\n  REGISTERED PASSENGERS\n");
   printf("  %-14s %-22s %-14s %-28s\n", "Username", "Name", "Phone", "Email");
   printf("  -------------------------------------------------------------"
          "-------------\n");
   for (i = 0; i < n; i++) {
       printf("  %-14.14s %-22.22s %-14.14s %-28.28s\n",
              u[i].username, u[i].fullName, u[i].phone, u[i].email);
   }
   free(u);
}

static void adminReport(void)
{
   int tn, bn, i, j;
   int sumBooked = 0, sumCancelled = 0;
   float sumRevenue = 0.0f;
   Train *t = (Train *)loadAll(TRAIN_FILE, sizeof(Train), &tn);
   Booking *b = (Booking *)loadAll(BOOKING_FILE, sizeof(Booking), &bn);

   if (t == NULL) {
       printf("\n  No trains in the system.\n");
       free(b);
       return;
   }
   printf("\n  SUMMARY REPORT\n");
   printf("  %-6s %-20s %-7s %-7s %-10s %-10s %-12s\n",
          "No.", "Train Name", "Total", "Booked", "Free", "Cancelled", "Revenue(Rs)");
   printf("  -------------------------------------------------------------"
          "--------------------\n");
   for (i = 0; i < tn; i++) {
       int booked = 0, cancelled = 0;
       float revenue = 0.0f;

       for (j = 0; j < bn; j++) {
           if (b[j].trainId == t[i].id) {
               if (b[j].active) {
                   booked++;
                   revenue += b[j].fare;
               } else {
                   cancelled++;
               }
           }
       }
       printf("  %-6d %-20.20s %-7d %-7d %-10d %-10d %-12.2f\n",
              t[i].id, t[i].name, t[i].totalSeats, booked,
              t[i].availableSeats, cancelled, revenue);
       sumBooked += booked;
       sumCancelled += cancelled;
       sumRevenue += revenue;
   }
   printf("  -------------------------------------------------------------"
          "--------------------\n");
   printf("  Totals: %d trains | %d confirmed tickets | %d cancelled | Rs %.2f revenue\n",
          tn, sumBooked, sumCancelled, sumRevenue);
   free(t);
   free(b);
}

static void adminMenu(void)
{
   int choice;

   do {
       printf("\n============== ADMIN MENU ==============\n");
       printf("  1. View all trains\n");
       printf("  2. Add train\n");
       printf("  3. Update train\n");
       printf("  4. Delete train\n");
       printf("  5. View all bookings\n");
       printf("  6. View registered passengers\n");
       printf("  7. Summary report\n");
       printf("  0. Logout\n");
       choice = readInt("Enter choice: ", 0, 7);

       switch (choice) {
           case 1: viewAllTrains();       waitForEnter(); break;
           case 2: adminAddTrain();       waitForEnter(); break;
           case 3: adminUpdateTrain();    waitForEnter(); break;
           case 4: adminDeleteTrain();    waitForEnter(); break;
           case 5: adminViewBookings();   waitForEnter(); break;
           case 6: adminViewPassengers(); waitForEnter(); break;
           case 7: adminReport();         waitForEnter(); break;
           case 0: printf("  Admin logged out.\n"); break;
       }
   } while (choice != 0);
}

static void adminLogin(void)
{
   char pass[40];
   int attempt;

   printf("\n  ADMIN LOGIN\n");
   for (attempt = 1; attempt <= 3; attempt++) {
       readNonEmpty("  Admin password: ", pass, sizeof pass);
       if (hashPassword(pass) == hashPassword(ADMIN_PASSWORD)) {
           adminMenu();
           return;
       }
       printf("  Wrong password (attempt %d of 3).\n", attempt);
   }
   printf("  Too many failed attempts. Returning to main menu.\n");
}

/* ------------------------------------------------------------------ */
/*                               GUEST                                 */
/* ------------------------------------------------------------------ */

static void guestMenu(void)
{
   int choice;

   do {
       printf("\n============== GUEST MENU ==============\n");
       printf("  1. View all trains\n");
       printf("  2. Search trains\n");
       printf("  0. Back\n");
       choice = readInt("Enter choice: ", 0, 2);

       switch (choice) {
           case 1: viewAllTrains(); waitForEnter(); break;
           case 2: searchTrains();  waitForEnter(); break;
           case 0: break;
       }
   } while (choice != 0);
}

/* ------------------------------------------------------------------ */
/*                                MAIN                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
   int choice;

   seedTrains();

   printf("==========================================\n");
   printf("        TRAIN MANAGEMENT SYSTEM\n");
   printf("==========================================\n");

   do {
       printf("\n============== MAIN MENU ===============\n");
       printf("  1. Passenger login\n");
       printf("  2. New passenger registration\n");
       printf("  3. Continue as guest (check availability)\n");
       printf("  4. Admin login\n");
       printf("  0. Exit\n");
       choice = readInt("Enter choice: ", 0, 4);

       switch (choice) {
           case 1: passengerLogin();      break;
           case 2: registerPassenger();   waitForEnter(); break;
           case 3: guestMenu();           break;
           case 4: adminLogin();          break;
           case 0: printf("\n  Thank you for using the Train Management System. Goodbye!\n"); break;
       }
   } while (choice != 0);

   return 0;
}
