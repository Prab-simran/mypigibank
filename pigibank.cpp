// ============================================================================
//  PERSONAL EXPENSE TRACKER  with  30-DAY SAVINGS STREAK & VOUCHER
//  Author : Prab Simran Kaur   UID : 25BCD10165   Section : BCD3B
//  Chandigarh University       Language : C++ (C++11 or later)
//
//  Compile :  g++ -std=c++11 -o expense_tracker expense_tracker.cpp
//  Run     :  ./expense_tracker

// ============================================================================

#include <iostream>

#include <fstream>

#include <sstream>

#include <vector>

#include <map>

#include <string>

#include <iomanip>

#include <ctime>

#include <stdexcept>

using namespace std;

 

// ----------------------------- Global constants -----------------------------

const int    STREAK_TARGET = 30;     // days needed to open a voucher

const double MIN_SAVING    = 1.0;    // even a tiny amount earns a token

const double BASE_REWARD   = 50.0;   // first voucher gives Rs 50

const double REWARD_STEP   = 10.0;   // every later voucher gives Rs 10 more

const double MAX_REWARD    = 100.0;  // upper limit of the reward

const string TX_FILE       = "transactions.txt";

const string STATE_FILE    = "streak_state.txt";

const string VOUCHER_FILE  = "vouchers.txt";

const string SETTINGS_FILE = "settings.txt";

 

const vector<string> CATEGORIES = {"Food", "Travel", "Shopping", "Bills",

"Education", "Health", "Entertainment", "Other"};

 

// ------------------------- Template + Exception classes ---------------------

template <typename T>

T clampValue(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

 

class TrackerException : public runtime_error {

public:

explicit TrackerException(const string &msg) : runtime_error(msg) {}

};

 

struct InputClosed {};   // thrown when the input stream ends (Ctrl+D / piped input)

 

// ================================== Date ====================================

class Date {

int year, month, day;

 

// days since 1970-01-01 (civil calendar algorithm)

static long daysFromCivil(int y, int m, int d) {

y -= (m <= 2);

long era = (y >= 0 ? y : y - 399) / 400;

long yoe = y - era * 400;

long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;

long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

return era * 146097 + doe - 719468;

}

 

public:

Date(int y = 1970, int m = 1, int d = 1) : year(y), month(m), day(d) {}

 

static Date fromDays(long z) {

z += 719468;

long era = (z >= 0 ? z : z - 146096) / 146097;

long doe = z - era * 146097;

long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;

long y = yoe + era * 400;

long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);

long mp = (5 * doy + 2) / 153;

long d = doy - (153 * mp + 2) / 5 + 1;

long m = mp + (mp < 10 ? 3 : -9);

return Date((int)(y + (m <= 2)), (int)m, (int)d);

}

 

static Date today(int offsetDays) {

time_t t = time(nullptr);

tm *lt = localtime(&t);

Date base(lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday);

return fromDays(base.toDays() + offsetDays);

}

 

static Date parse(const string &s) {

int y, m, d; char a, b;

istringstream in(s);

if (!(in >> y >> a >> m >> b >> d) || a != '-' || b != '-' ||

m < 1 || m > 12 || d < 1 || d > 31)

throw TrackerException("Invalid date: " + s);

return Date(y, m, d);

}

 

long toDays() const { return daysFromCivil(year, month, day); }

int  getYear() const { return year; }

int  getMonth() const { return month; }

bool sameMonth(const Date &o) const { return year == o.year && month == o.month; }

 

string toString() const {

ostringstream o;

o << year << '-' << setfill('0') << setw(2) << month << '-' << setw(2) << day;

return o.str();

}

bool operator==(const Date &o) const { return toDays() == o.toDays(); }

long operator-(const Date &o) const { return toDays() - o.toDays(); }

friend ostream &operator<<(ostream &os, const Date &d) { return os << d.toString(); }

};

 

// ============================ Transaction hierarchy =========================

class Transaction {                       // abstract base class

protected:

Date   date;

string category;

double amount;

string note;

public:

Transaction(const Date &d, const string &c, double a, const string &n)

: date(d), category(c), amount(a), note(n) {}

virtual ~Transaction() {}

 

virtual string type() const = 0;           // pure virtual

virtual double walletEffect() const = 0;   // +ve adds to wallet, -ve reduces

 

virtual void display() const {

cout << left << setw(12) << date.toString() << setw(9) << type()

<< setw(16) << category << right << setw(10) << amount

<< "  " << note << "\n";

}

string serialize() const {

ostringstream o;

o << type() << '|' << date.toString() << '|' << category << '|'

<< fixed << setprecision(2) << amount << '|' << note;

return o.str();

}

const Date &getDate() const { return date; }

const string &getCategory() const { return category; }

double getAmount() const { return amount; }

 

friend ostream &operator<<(ostream &os, const Transaction &t) {

t.display();

return os;

}

};

 

class Income : public Transaction {

public:

Income(const Date &d, const string &c, double a, const string &n)

: Transaction(d, c, a, n) {}

string type() const override { return "INCOME"; }

double walletEffect() const override { return amount; }

};

 

class Expense : public Transaction {

public:

Expense(const Date &d, const string &c, double a, const string &n)

: Transaction(d, c, a, n) {}

string type() const override { return "EXPENSE"; }

double walletEffect() const override { return -amount; }

};

 

class Saving : public Transaction {       // money moved into the savings jar

public:

Saving(const Date &d, double a, const string &n)

: Transaction(d, "Savings Jar", a, n) {}

string type() const override { return "SAVING"; }

double walletEffect() const override { return -amount; }

void display() const override {

Transaction::display();

cout << "            ^ token earned for this day\n";

}

};

 

Transaction *createTransaction(const string &type, const Date &d, const string &c,

double a, const string &n) {

if (type == "INCOME")  return new Income(d, c, a, n);

if (type == "EXPENSE") return new Expense(d, c, a, n);

if (type == "SAVING")  return new Saving(d, a, n);

throw TrackerException("Unknown transaction type: " + type);

}

 

// ================================= Voucher ==================================

class Voucher {

int    id;

Date   earnedOn;

double amount;

bool   claimed;

public:

Voucher(int i = 0, const Date &d = Date(), double a = 0, bool c = false)

: id(i), earnedOn(d), amount(a), claimed(c) {}

int getId() const { return id; }

double getAmount() const { return amount; }

bool isClaimed() const { return claimed; }

void claim() { claimed = true; }

string serialize() const {

ostringstream o;

o << id << '|' << earnedOn.toString() << '|' << fixed << setprecision(2)

<< amount << '|' << (claimed ? 1 : 0);

return o.str();

}

void display() const {

cout << "  Voucher #" << id << "  earned " << earnedOn << "  Rs " << amount

<< "  [" << (claimed ? "CLAIMED" : "READY TO OPEN") << "]\n";

}

};

 

// ========================= StreakManager (unique feature) ===================

enum SaveStatus { ALREADY_COUNTED, STREAK_STARTED, STREAK_CONTINUED,

STREAK_RESTARTED, VOUCHER_UNLOCKED };

 

class StreakManager {

int  current;        // tokens collected in the running cycle (0..29)

int  best;           // best streak ever reached in a cycle

int  totalTokens;    // all tokens ever collected

int  cycles;         // number of completed 30-day cycles

long lastDay;        // day number of the last token (-1 = none)

vector<Voucher> vouchers;

public:

StreakManager() : current(0), best(0), totalTokens(0), cycles(0), lastDay(-1) {}

 

int getCurrent() const { return current; }

int getBest() const { return best; }

int getTotalTokens() const { return totalTokens; }

int getCycles() const { return cycles; }

const vector<Voucher> &getVouchers() const { return vouchers; }

bool savedOn(const Date &d) const { return lastDay == d.toDays(); }

bool hasLast() const { return lastDay >= 0; }

double nextReward() const {

return clampValue(BASE_REWARD + REWARD_STEP * cycles, BASE_REWARD, MAX_REWARD);

}

 

// If at least one whole day was missed, the streak resets to zero.

// Returns the length of the streak that was lost (0 if nothing was lost).

int checkBroken(const Date &today) {

if (lastDay < 0 || current == 0) return 0;

if (today.toDays() - lastDay > 1) {

int lost = current;

current = 0;

return lost;

}

return 0;

}

 

SaveStatus recordSaving(const Date &today, int &lost) {

long day = today.toDays();

if (lastDay >= 0 && day < lastDay)

throw TrackerException("Today's date is earlier than the last saving date.");

if (day == lastDay) return ALREADY_COUNTED;   // one token per day only

 

lost = checkBroken(today);

current++;

totalTokens++;

lastDay = day;

if (current > best) best = current;

 

if (current == STREAK_TARGET) {                // 30 tokens collected!

cycles++;

double reward = clampValue(BASE_REWARD + REWARD_STEP * (cycles - 1),

BASE_REWARD, MAX_REWARD);

vouchers.push_back(Voucher((int)vouchers.size() + 1, today, reward, false));

current = 0;                               // next 30-day cycle begins

return VOUCHER_UNLOCKED;

}

if (lost > 0) return STREAK_RESTARTED;

return current == 1 ? STREAK_STARTED : STREAK_CONTINUED;

}

 

int pendingCount() const {

int n = 0;

for (size_t i = 0; i < vouchers.size(); i++)

if (!vouchers[i].isClaimed()) n++;

return n;

}

 

// Opens the oldest unclaimed voucher. Returns its id, or 0 if none is ready.

int claimNext(double &amount) {

for (size_t i = 0; i < vouchers.size(); i++) {

if (!vouchers[i].isClaimed()) {

vouchers[i].claim();

amount = vouchers[i].getAmount();

return vouchers[i].getId();

}

}

return 0;

}

 

void save() const {

ofstream s(STATE_FILE.c_str());

s << current << ' ' << best << ' ' << totalTokens << ' ' << cycles << ' '

<< lastDay << '\n';

ofstream v(VOUCHER_FILE.c_str());

for (size_t i = 0; i < vouchers.size(); i++) v << vouchers[i].serialize() << '\n';

}

 

void load() {

ifstream s(STATE_FILE.c_str());

if (s) s >> current >> best >> totalTokens >> cycles >> lastDay;

ifstream v(VOUCHER_FILE.c_str());

string line;

while (getline(v, line)) {

if (line.empty()) continue;

stringstream ss(line);

string id, dt, amt, cl;

getline(ss, id, '|'); getline(ss, dt, '|');

getline(ss, amt, '|'); getline(ss, cl, '|');

vouchers.push_back(Voucher(stoi(id), Date::parse(dt), stod(amt), cl == "1"));

}

}

};

 

// ============================= Input helpers ================================

string readLine(const string &prompt) {

cout << prompt;

string s;

if (!getline(cin, s)) throw InputClosed();

return s;

}

 

double readAmount(const string &prompt) {

while (true) {

string s = readLine(prompt);

try {

size_t pos = 0;

double v = stod(s, &pos);

if (pos != s.size()) throw invalid_argument("extra characters");

if (v <= 0) { cout << "  Amount must be greater than zero.\n"; continue; }

return v;

} catch (const exception &) {

cout << "  Please enter a valid number.\n";

}

}

}

 

int readInt(const string &prompt, int lo, int hi) {

while (true) {

string s = readLine(prompt);

try {

size_t pos = 0;

int v = stoi(s, &pos);

if (pos != s.size()) throw invalid_argument("extra characters");

if (v >= lo && v <= hi) return v;

} catch (const exception &) {}

cout << "  Enter a whole number between " << lo << " and " << hi << ".\n";

}

}

 

string cleanText(string s) {

for (size_t i = 0; i < s.size(); i++)

if (s[i] == '|' || s[i] == '\n' || s[i] == '\r') s[i] = '/';

return s;

}

 

void printHeader(const string &title) {

cout << "\n" << string(62, '=') << "\n  " << title << "\n" << string(62, '=') << "\n";

}

 

// ============================== ExpenseTracker ==============================

class ExpenseTracker {

vector<Transaction *> txs;

StreakManager streak;

double monthlyBudget;

int    dayOffset;          // demo clock: lets you "advance" days for testing

public:

ExpenseTracker() : monthlyBudget(0), dayOffset(0) {}

~ExpenseTracker() { for (size_t i = 0; i < txs.size(); i++) delete txs[i]; }

ExpenseTracker(const ExpenseTracker &) = delete;

ExpenseTracker &operator=(const ExpenseTracker &) = delete;

 

Date today() const { return Date::today(dayOffset); }

 

double balance() const {

double b = 0;

for (size_t i = 0; i < txs.size(); i++) b += txs[i]->walletEffect();

return b;

}

double totalSaved() const {

double t = 0;

for (size_t i = 0; i < txs.size(); i++)

if (txs[i]->type() == "SAVING") t += txs[i]->getAmount();

return t;

}

double monthExpense(const Date &ref) const {

double t = 0;

for (size_t i = 0; i < txs.size(); i++)

if (txs[i]->type() == "EXPENSE" && txs[i]->getDate().sameMonth(ref))

t += txs[i]->getAmount();

return t;

}

 

// ----------------------------- persistence -----------------------------

void load() {

ifstream f(TX_FILE.c_str());

string line;

while (getline(f, line)) {

if (line.empty()) continue;

stringstream ss(line);

string type, dt, cat, amt, note;

getline(ss, type, '|'); getline(ss, dt, '|'); getline(ss, cat, '|');

getline(ss, amt, '|');  getline(ss, note);

txs.push_back(createTransaction(type, Date::parse(dt), cat, stod(amt), note));

}

ifstream s(SETTINGS_FILE.c_str());

if (s) s >> monthlyBudget >> dayOffset;

streak.load();

}

void saveAll() const {

ofstream f(TX_FILE.c_str());

for (size_t i = 0; i < txs.size(); i++) f << txs[i]->serialize() << '\n';

ofstream s(SETTINGS_FILE.c_str());

s << monthlyBudget << ' ' << dayOffset << '\n';

streak.save();

}

 

// ------------------------------ menu actions ---------------------------

void refreshStreak() {

int lost = streak.checkBroken(today());

if (lost > 0) {

cout << "\n  !! STREAK BROKEN !!  You missed a day and lost your " << lost

<< "-day streak.\n  Don't worry - save today and start again from Day 1.\n";

saveAll();

}

}

 

void statusLine() const {

cout << "\n  Date: " << today() << "   Wallet: Rs " << balance()

<< "   Savings jar: Rs " << totalSaved() << "   Streak: "

<< streak.getCurrent() << "/" << STREAK_TARGET << "\n";

if (streak.pendingCount() > 0)

cout << "  *** You have " << streak.pendingCount()

<< " voucher(s) ready to open! (menu option 8) ***\n";

}

 

void addIncome() {

printHeader("ADD INCOME");

string src = cleanText(readLine("  Source (e.g. Pocket money, Salary): "));

if (src.empty()) src = "Income";

double amt = readAmount("  Amount (Rs): ");

txs.push_back(new Income(today(), src, amt, "Income added"));

saveAll();

cout << "  Income recorded. Wallet balance: Rs " << balance() << "\n";

}

 

void addExpense() {

printHeader("ADD EXPENSE");

for (size_t i = 0; i < CATEGORIES.size(); i++)

cout << "   " << i + 1 << ". " << CATEGORIES[i] << "\n";

int c = readInt("  Choose category: ", 1, (int)CATEGORIES.size());

double amt = readAmount("  Amount (Rs): ");

if (amt > balance())

throw TrackerException("Not enough wallet balance (Rs " +

to_string((long)balance()) + ").");

string note = cleanText(readLine("  Note (optional): "));

txs.push_back(new Expense(today(), CATEGORIES[c - 1], amt, note));

saveAll();

cout << "  Expense recorded. Wallet balance: Rs " << balance() << "\n";

 

if (monthlyBudget > 0) {

double spent = monthExpense(today());

if (spent > monthlyBudget)

cout << "  WARNING: monthly budget exceeded! (Rs " << spent << " of Rs "

<< monthlyBudget << ")\n";

else if (spent >= 0.8 * monthlyBudget)

cout << "  Caution: you have used " << (int)(spent * 100 / monthlyBudget)

<< "% of your monthly budget.\n";

}

}

 

void saveMoney() {

printHeader("SAVE MONEY  (earn today's streak token)");

cout << "  Even a small amount (Rs " << MIN_SAVING << " or more) earns a token.\n";

double amt = readAmount("  Amount to save today (Rs): ");

if (amt < MIN_SAVING) throw TrackerException("Minimum saving is Rs 1.");

if (amt > balance())

throw TrackerException("Not enough wallet balance to save this amount.");

 

Date t = today();

int lost = 0;

SaveStatus st = streak.recordSaving(t, lost);   // may throw on bad date

txs.push_back(new Saving(t, amt, "Daily saving"));

saveAll();

cout << "  Rs " << amt << " moved to your savings jar.\n";

 

switch (st) {

case ALREADY_COUNTED:

cout << "  Today's token is already collected. Extra amount saved - great!\n";

break;

case STREAK_STARTED:

cout << "  TOKEN 1/" << STREAK_TARGET << " collected. Your streak has begun!\n";

break;

case STREAK_CONTINUED:

cout << "  TOKEN " << streak.getCurrent() << "/" << STREAK_TARGET

<< " collected. Keep going! " << STREAK_TARGET - streak.getCurrent()

<< " day(s) to your voucher.\n";

break;

case STREAK_RESTARTED:

cout << "  You had lost a " << lost << "-day streak, but a new one starts now."

<< "\n  TOKEN 1/" << STREAK_TARGET << " collected.\n";

break;

case VOUCHER_UNLOCKED:

cout << "\n  ************************************************\n"

<< "  *   CONGRATULATIONS!  30 TOKENS COLLECTED!     *\n"

<< "  *   Your 30-DAY VOUCHER has been UNLOCKED!     *\n"

<< "  *   Choose menu option 8 to open it.           *\n"

<< "  ************************************************\n"

<< "  A fresh 30-day cycle starts tomorrow (cycle "

<< streak.getCycles() + 1 << ").\n";

break;

}

}

 

void openVoucher() {

printHeader("OPEN VOUCHER");

double amt = 0;

int id = streak.claimNext(amt);

if (id == 0) {

cout << "  No voucher is ready yet. You have collected "

<< streak.getCurrent() << "/" << STREAK_TARGET << " tokens.\n"

<< "  Reward waiting at the end of this cycle: Rs " << streak.nextReward()

<< "\n";

return;

}

txs.push_back(new Income(today(), "Voucher Bonus", amt,

"30-day streak voucher #" + to_string(id)));

saveAll();

cout << "  Voucher #" << id << " opened!  Rs " << amt

<< " has been added to your wallet as encouragement.\n"

<< "  New wallet balance: Rs " << balance() << "\n"

<< "  Now collect 30 new tokens for the next voucher (Rs "

<< streak.nextReward() << ").\n";

}

 

void streakDashboard() const {

printHeader("SAVINGS STREAK DASHBOARD");

int cur = streak.getCurrent();

cout << "  Cycle number     : " << streak.getCycles() + 1 << "\n"

<< "  Current streak   : " << cur << " / " << STREAK_TARGET << " days\n"

<< "  Progress         : [" << string(cur, '#')

<< string(STREAK_TARGET - cur, '.') << "] " << cur * 100 / STREAK_TARGET

<< "%\n";

cout << "\n  Token board:\n";

for (int i = 1; i <= STREAK_TARGET; i++) {

cout << (i <= cur ? " [*]" : " [ ]");

if (i % 10 == 0) cout << "\n";

}

cout << "\n  Days to voucher  : " << STREAK_TARGET - cur << "\n"

<< "  Next reward      : Rs " << streak.nextReward() << "\n"

<< "  Best streak      : " << streak.getBest() << " days\n"

<< "  Total tokens     : " << streak.getTotalTokens() << "\n"

<< "  Cycles completed : " << streak.getCycles() << "\n";

if (!streak.savedOn(today()) && cur > 0)

cout << "\n  Reminder: save something TODAY or your streak will break!\n";

else if (streak.savedOn(today()))

cout << "\n  Today's token is already collected.\n";

if (!streak.getVouchers().empty()) {

cout << "\n  Your vouchers:\n";

for (size_t i = 0; i < streak.getVouchers().size(); i++)

streak.getVouchers()[i].display();

}

}

 

void viewAll() const {

printHeader("ALL TRANSACTIONS");

if (txs.empty()) { cout << "  No transactions yet.\n"; return; }

cout << left << setw(12) << "  Date" << setw(9) << "Type" << setw(16) << "Category"

<< right << setw(10) << "Amount" << "  Note\n  " << string(58, '-') << "\n";

for (size_t i = 0; i < txs.size(); i++) cout << "  " << *txs[i];

}

 

void monthlySummary() const {

printHeader("MONTHLY SUMMARY");

string s = readLine("  Month (YYYY-MM) or press Enter for current month: ");

Date ref = s.empty() ? today() : Date::parse(s + "-01");

double inc = 0, exp = 0, sav = 0;

for (size_t i = 0; i < txs.size(); i++) {

if (!txs[i]->getDate().sameMonth(ref)) continue;

if (txs[i]->type() == "INCOME") inc += txs[i]->getAmount();

else if (txs[i]->type() == "EXPENSE") exp += txs[i]->getAmount();

else sav += txs[i]->getAmount();

}

cout << "\n  Month            : " << ref.getYear() << "-" << setfill('0') << setw(2)

<< ref.getMonth() << setfill(' ') << "\n"

<< "  Total income     : Rs " << inc << "\n"

<< "  Total expenses   : Rs " << exp << "\n"

<< "  Total saved      : Rs " << sav << "\n"

<< "  Left from income : Rs " << inc - exp - sav << "\n";

if (monthlyBudget > 0) {

cout << "  Monthly budget   : Rs " << monthlyBudget << "\n"

<< "  Budget remaining : Rs " << monthlyBudget - exp << "\n";

}

}

 

void categoryReport() const {

printHeader("CATEGORY-WISE EXPENSE REPORT");

map<string, double> totals;

double all = 0;

for (size_t i = 0; i < txs.size(); i++)

if (txs[i]->type() == "EXPENSE") {

totals[txs[i]->getCategory()] += txs[i]->getAmount();

all += txs[i]->getAmount();

}

if (totals.empty()) { cout << "  No expenses recorded yet.\n"; return; }

for (map<string, double>::const_iterator it = totals.begin(); it != totals.end(); ++it) {

int pct = (int)(it->second * 100 / all);

cout << "  " << left << setw(15) << it->first << right << setw(10) << it->second

<< "  " << setw(3) << pct << "%  " << string(pct / 4, '*') << "\n";

}

cout << "  " << string(40, '-') << "\n  " << left << setw(15) << "TOTAL" << right

<< setw(10) << all << "\n";

}

 

void setBudget() {

printHeader("SET MONTHLY BUDGET");

monthlyBudget = readAmount("  Monthly spending limit (Rs): ");

saveAll();

cout << "  Budget saved.\n";

}

 

void advanceDay() {

printHeader("DEMO MODE: ADVANCE THE CLOCK");

int n = readInt("  How many days to move forward (1-60)? ", 1, 60);

dayOffset += n;

saveAll();

cout << "  Date is now " << today() << ".\n";

refreshStreak();

}

};

 

// ================================== main ====================================

void showMenu() {

cout << "\n  ----------------- MAIN MENU -----------------\n"

<< "   1. Add income            6. Category report\n"

<< "   2. Add expense           7. Streak dashboard\n"

<< "   3. Save money (token)    8. Open voucher\n"

<< "   4. View transactions     9. Set monthly budget\n"

<< "   5. Monthly summary      10. Advance day (demo)\n"

<< "   0. Exit\n";

}

 

int main() {

cout << fixed << setprecision(2);

ExpenseTracker app;

try {

app.load();

} catch (const exception &e) {

cout << "Could not read saved data: " << e.what() << "\n";

return 1;

}

 

printHeader("PERSONAL EXPENSE TRACKER  -  30-Day Savings Streak");

try {

while (true) {

app.refreshStreak();

app.statusLine();

showMenu();

int ch = readInt("  Enter choice: ", 0, 10);

if (ch == 0) break;

try {

switch (ch) {

case 1: app.addIncome();       break;

case 2: app.addExpense();      break;

case 3: app.saveMoney();       break;

case 4: app.viewAll();         break;

case 5: app.monthlySummary();  break;

case 6: app.categoryReport();  break;

case 7: app.streakDashboard(); break;

case 8: app.openVoucher();     break;

case 9: app.setBudget();       break;

case 10: app.advanceDay();     break;

}

} catch (const TrackerException &e) {

cout << "  Error: " << e.what() << "\n";

}

}

} catch (const InputClosed &) {

cout << "\n  Input closed.\n";

}

app.saveAll();

cout << "\n  Data saved. Keep saving every day - see you tomorrow!\n";

return 0;

}