
#include <iostream>
#include <istream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <cassert>      
#include <algorithm>
#include <format>
#include <vector>
#include <set>
#include <sstream>
#include <math.h>

#define NUM_OP 10            // number of accepted operators
#define NUM_SYSVAR 4        // to allow easier modifiability if new constants are added
#define NUM_COMMAND 5       // numbers of protected command names
#define NUM_OPTIONS 4       // number of option/target keywords

// doubles for vals
const double DISP_SYS =   1.0; // 2^0               // command pass values
const double DISP_USER =  2.0; // 2^1              // doubles so will be accepted as values
const double DISP_ALL =   4.0; // 2^2
const double DISP_OP =    8.0; // 2^3

const double DELETE_ALL= -1.0;

// ints for flag switch
const int DISP_SYS_FLAG =   1;
const int DISP_USER_FLAG =  2;
const int DISP_ALL_FLAG =   4;
const int DISP_OP_FLAG =    8;

const int DELETE_ALL_FLAG= -1;



// Token stuff
// Token “kind” values:
char const number = '9';    // a floating-point number is now 9, because we've gone beyond only positives
char const quit = 'q';      // an exit command
char const print = ';';     // a print command
char const help = 'h';      // display help text
char const disp = 'd';      // a command to display variables, either system, user, or all
char const del = 'k';       // a command to delete user variables
char const setvar = 'v';    // a token assigning a variable, either adding to the set or overwriting
char const empt = '\0';       // a default value for kind_ in token, on resolve will throw an error

    // could define these constants as well, but I prefer the variables for the user-facing system constants
const double e = 2.718281828459045;     
const double g = 9.80665;       // gravitational constant
const double phi = 1.6180339887;
const double pi = 3.1415926535;
const double syscons[NUM_SYSVAR] = {e,g,phi,pi};
// if extending system constants, list constants first in reservedVarNames, then all commands, then all options/targets
const std::string reservedVarNames[NUM_SYSVAR+NUM_COMMAND+NUM_OPTIONS] = {"e","g","phi","pi",     "help","q","quit","delete","display",        "sysvars","uvars","all","operators"}; 
const char operators[NUM_OP] = {'(',')',';','=','+','-','*','/','%','^'};
const char* opdescrip[NUM_OP] = {"Open parentheses","Close parentheses","Print","Assign a user variable","Add","Subtract/Negative","Multiply","Divide","Modulo","Power/Raise"};

// helper functions
const std::string lowcase(const std::string str);   // creates a lowercase string of input
bool is_op(const char ch);                         // checks if ch is an operator character
bool is_sysvar(const std::string varName);          // checks if varName is a system constant
const double get_sysvar(const std::string varName); // returns value of matching system constant
bool is_command(const std::string varName);         // checks if varName is a command
bool is_option(const std::string varName);          // checks if varName is a valid option/target
const double get_option(const std::string varName); // returns the appropriate option value for tokens
bool is_usrvar(const std::string varName);          // checks is user variable varName exists
const double get_usrvar(const std::string varName); // gets value of matching user variable
bool is_break(const char ch);                       // checks if ch is a break character

// class for user defined variables
class UserVar 
{
    std::string name;   // the set name for the variable
    double val;       // its current value

public:
    // constructors
    UserVar()
    {
        name = "";
        val = 0;
    }
    UserVar(const std::string nm, const double v) 
    {
        name=nm; 
        val=v;
    }
    UserVar(const std::string nm) 
    {
        name=nm; 
        val=0;
    }
    // return funcs
    std::string sname() const 
    {
        return name;
    }
    const char* cname() const 
    {
        return name.c_str();
    }
    double getvalue() const 
    {
        return val;
    }

    // set funcs
    void setvalue(double v) 
    {
        val = v;
    }
    void setname(std::string nm) 
    {
        name = nm;   // mainly for testing
    }

    // overload relational operators
    bool operator<(const UserVar& rhs) {return (name < rhs.name);}
    bool operator>(const UserVar& rhs) {return (name > rhs.name);}
    bool operator==(const UserVar& rhs) {return (name==rhs.name);}
    bool operator>=(const UserVar& rhs) {return !(*this<rhs);}
    bool operator<=(const UserVar& rhs) {return !(*this>rhs);}
    bool operator!=(const UserVar& rhs) {return !(*this==rhs);}
    // for c string inputs
    bool operator<(const char* cstr) const {return (strcmp(name.c_str(), cstr)<0);}    //strcmp, if first is less than second
    bool operator>(const char* cstr) const {return (strcmp(name.c_str(), cstr)>0);}    // returns negative. If equal, returns 0
    bool operator==(const char* cstr) const {return (strcmp(name.c_str(), cstr)==0);} // If first greater, returns positive
};


// standalone operator overloads for ordering
bool operator<(const UserVar& lhs, const UserVar& rhs){
    return lhs.cname() < rhs.cname();
}
bool operator>(const UserVar& lhs, const UserVar& rhs){
    return lhs.cname() > rhs.cname();
}
bool operator==(const UserVar& lhs, const UserVar& rhs){
    return (strcmp(lhs.cname() ,rhs.cname())==0);
}
bool operator>=(const UserVar& lhs, const UserVar& rhs){
    return !(lhs<rhs);
}
bool operator<=(const UserVar& lhs, const UserVar& rhs){
    return !(lhs>rhs);
}
bool operator!=(const UserVar& lhs, const UserVar& rhs){
    return !(lhs==rhs);
}

// vector to store reserved variable names, initialized by const array
std::vector<std::string> vecReservedNames (reservedVarNames, reservedVarNames+NUM_SYSVAR+NUM_COMMAND);

// extended to include a name to preserve for user variable assignments
class token
{
    char kind_;       // what kind of token
    double value_;    // for numbers: a value
    std::string name_;// for variable assignments

public:
    // constructors
    token()
      : kind_(empt)      // 'err' = '\0', should only occur in improperly initialized tokens
      , value_(0)
      , name_("")
    {
    }
    token(char ch)
      : kind_(ch)
      , value_(0)
      , name_("")
    {
    }
    token(double val)
      : kind_(number)    // let ‘9’ represent “a number”
      , value_(val)
      , name_("")
    {
    }
    token(char ch, double val)
      : kind_(ch)
      , value_(val)
      , name_("")
    {
    }
    token(char ch, std::string nm) 
    {
        name_ = nm; 
        kind_ = ch; 
        value_ = 0; 
    }
    token(char ch, double val, std::string nm) 
    {
        name_ = nm; 
        kind_ = ch; 
        value_ = val;
    }
    
    char kind() const
    {
        return kind_;
    }
    double value() const
    {
        return value_;
    }
    std::string getname() const
    {
        return name_;
    }
};

// User interaction strings:
std::string const prompt = "Enter one or more expressions to evaluate, ending each expression with ';' (Enter 'q;' or 'quit;' to quit, or 'help;' for more info) > ";
std::string const helptext = "Symbols and commands: \n ; - Use to signify the end of a single expression and parse all input \n q or quit - Quit program \n help; - Display this help text \n display sysvars; - Display a list of built in system variables \n display uvars; - Display a list of current user variables \n display all; - Display a list of all current variables \n display operators; - Display a list of accepted operators \n delete uvars all; - Delete all current user variables \n delete uvars $name; - Delete user variable with name matching $name \n\n User variables must include only alpha characters.\n\n User Variable names are case sensitive, system constants and commands are not. \n To assign a variable, use 'varname = ($expression);'\n\n";
std::string const result = "= ";    // indicate that a result follows

class token_stream
{
    // representation: not directly accessible to users:
    bool full;       // is there a token in the buffer?
    token buffer;    // here is where we keep a Token put back using
                     // putback()
public:
    // user interface:
    token get();            // get a token from std::cin
    void putback(token);    // put a token back into the token_stream
    void ignore(char c);    // discard tokens up to and including a c

    // constructor: make a token_stream, the buffer starts empty
    token_stream()
      : full(false)
      , buffer(empt)
    {
    }
};

// single global instance of the token_stream
token_stream ts;
// global vector to store current user defined variables 
std::vector<UserVar> vecUserVars;

void token_stream::putback(token t)
{
    if (full)
        throw std::runtime_error("putback() into a full buffer");
    buffer = t;
    full = true;
}

// declaration so that primary() and ts.get() can call expression()
double expression();

token token_stream::get()    // read a token from the token_stream
{
    // check if we already have a Token ready
    if (full)
    {
        full = false;
        return buffer;
    }

    // note that >> skips whitespace (space, newline, tab, etc.)
    char ch;      // ch for cin
    std::string vrname, optname; // for constructing names
    vrname.clear();
    optname.clear();
    std::cin >> ch;

    // operator check
    if (is_op(ch))    // is an operator 
        return token(ch);
    if (is_break(ch))   // from cin, can't be ' ', all other breaks should cause print tokens
        return token(print);

    // now numbers
    if (isdigit(ch) || ch=='.')
    {
        std::cin.putback(ch);
        double val;
        std::cin >> val;
        return token(val);
    }

    if (isalpha(ch)) // starts with character, either existing variable or assign
    {
        while (isalpha(ch))
        {
            vrname.push_back(ch);
            std::cin.get(ch);
        }
        std::cin.putback(ch);
        if (is_command(lowcase(vrname))) // a command
        {
            std::string cmnd = lowcase(vrname);
            if (cmnd=="q"||cmnd=="quit")
            {
                return token(quit);
            }
            if (cmnd=="help")
            {
                return token(help);
            }
            if (cmnd=="display")
            {
                std::cin >> ch;     // continue to check command target
                while (isalpha(ch))
                {
                    optname.push_back(ch);
                    std::cin.get(ch);
                }
                std::cin.putback(ch);
                std::string tgt = lowcase(optname);
                if (is_option(tgt))
                {
                    double flag = get_option(tgt);
                    return token(disp,flag);
                }
                throw std::runtime_error("Bad argument for display. Options for display are: sysvars;  uvars;  all;  options;");
            }
            if (cmnd=="delete")
            {
                std::cin >> ch;     // continue to check command target
                while (isalpha(ch))
                {
                    optname.push_back(ch);
                    std::cin.get(ch);
                }
                std::cin.putback(ch);
                std::string tgt = lowcase(optname);
                if (tgt=="all")
                    return token(del,-1);
                else if (is_usrvar(optname))    // a matching variable exists
                    return token(del,0,optname);
                else
                    throw std::runtime_error("Cannot delete a variable that does not exist!");
            }
            // command not in list
            throw std::runtime_error("Command not found.");
        }
        if (is_sysvar(lowcase(vrname))) // a system constant exists
        {
            double d = get_sysvar(lowcase(vrname));
            return token(d);        // resolve constants into number tokens
        }
        if (is_usrvar(vrname))      // one exists
        {
            std::cin >> ch;
            if (ch=='=')        // gonna assign, make a setvar
            {
                return token(setvar, 1, vrname); // set 1 to take from expression, 0 to 0
            }
            else                // just make a number from its value
            {
                std::cin.putback(ch);
                double d = get_usrvar(vrname);
                return token(d);
            }
        }
        ch = 0; // set to allow end of line to declare new var, e.g. "prompt> var" then enter creates var with value 0
        std::cin >> ch; // unitiatied variable, check if assign, print, or nothing after, latter 2 set to zero
        if (ch==0||ch==';')
        {    // create the user variable with initial value 0
            return token(setvar,0,vrname);
        }
        else if (ch=='=')  // create then assign
            return token(setvar,1,vrname);
        else  // trying to use an undeclared variable
        {
            std::cin.putback(ch);
            throw std::runtime_error("Tried to use an undeclared variable!");
        }
    }
    // should only get here if character or token is unmakeable
    throw std::runtime_error("Bad token");    
}


// discard tokens up to and including a c
void token_stream::ignore(char c)
{
    // first look in buffer:
    if (full && c == buffer.kind())    // && means 'and'
    {
        full = false;
        return;
    }
    full = false;    // discard the contents of buffer

    // now search input:
    char ch = 0;
    while (std::cin.get(ch))
    {
        if (ch == c)
            break;
    }
}



double primary()    // Number or ‘(‘ Expression ‘)’ or '-' for negative numbers, or search/set user var. power '^' also managed here 
{
    char ch;       // for negative usage
    std::string varname; // for checking existing user variables
    token t = ts.get();
    token n;        // to use when checking for - or ^
    switch (t.kind())
    {
        case '(':    // handle ‘(’expression ‘)’
        {
            double d = expression();
            t = ts.get();
            if (t.kind() != ')')
                throw std::runtime_error("')' expected");
            return d;
            break;          // unnecessary but safe
        }
        case '-':   // check if folowing token would be number, if so, read in then make negative
        {
            std::cin >> ch;
            if (ch=='(')
            {
                double d = expression();
                t = ts.get();
                if (t.kind() != ')')
                    throw std::runtime_error("')' expected");
                return d*-1;
                break;          // unnecessary but safe
            }
            if (isdigit(ch) || ch=='.') // next token will be number
            {
                std::cin.putback(ch);     // replace ch, read in double, then negate and return
                double dn;
                std::cin >> dn;
                dn *= -1;               // negate        
                std::cin >> ch;          // now we have a number, must check for power ^
                if (ch=='^')      // is a raise ^
                {
                    double rpower = primary();      // evaluate the primary to be raised to
                    double result = pow(dn, rpower);   // then raise value to power
                    return result;      // return, closed out
                }
                else                   // not a raise ^
                {
                    std::cin.putback(ch);      // put next token into buffer
                    return dn;          // return, closed out
                }
            }
            else if (isalpha(ch))       // could be user variable
            {
                varname.push_back(ch);  // add character to varname
                while (isalpha(ch))    // get all sequential alpha characters to compose variable name
                {
                    varname.push_back(ch);  // keep adding chars
                    std::cin.get(ch);
                }
                std::cin.putback(ch);
                // First check if varname is a command, if so throw error
                if (is_command(varname))
                    throw std::runtime_error("Tried to create primary from command name");
                // Now to check if variable exists, otherwise throw error
                if (is_sysvar(varname))
                {       // is a system constant, so get its value then negate
                    double sysv = get_sysvar(varname);    
                    sysv *= -1;             
                    n = ts.get();           // now we have a number, must check for power ^
                    if (n.kind()=='^')      // is a raise ^
                    {
                        double rpower = primary();      // evaluate the expression to be raised to
                        double result = pow(sysv, rpower);   // then raise value to power
                        return result;      // return, closed out
                    }
                    else                   // not a raise ^
                    {
                        ts.putback(n);      // put next token into buffer
                        return sysv;          // return, closed out
                    }
                }
                // Now check for user var, if not that either then throw error
                else if (is_usrvar(varname))    // there is an existing uservar, get its value then negate
                {
                    double usrv = get_usrvar(varname);    
                    usrv *= -1;             
                    n = ts.get();           // now we have a number, must check for power ^
                    if (n.kind()=='^')      // is a raise ^
                    {
                        double rpower = primary();      // evaluate the primary to be raised to
                        double result = pow(usrv, rpower);   // then raise value to power
                        return result;      // return, closed out
                    }
                    else                   // not a raise ^
                    {
                        ts.putback(n);      // put next token into buffer
                        return usrv;          // return, closed out
                    }
                }
                else
                    throw std::runtime_error("Failed to create a negative primary from alpha chars");
            }
            break;          // unnecessary but safe
        }
        case number:    // since we allow power, we must check for next char being ^
        {
            n = ts.get();   // get next token to check for power to preserve precedence
            if (n.kind()=='^')  // next operator is a raise, must raise to an primary
            {
                std::cin >> std::ws;
                double rpower = primary();             // evaluate the primary to be raised to
                double result = pow(t.value(), rpower);   // then raise value to power
                return result;
            }
            else        // not a power, treat as normal number
            {
                ts.putback(n);       // pushback the token
                return t.value();    // return the number’s value
            }
            break;          // unnecessary but safe
        }
        case setvar:    // if valid user variable
        {
            return t.value();
            break;          // unnecessary but safe
        }
        default:
        {
            std::cout << "Hit default in primary with token " << t.kind() << " " << t.value() << " " << t.getname() << std::endl;
            throw std::runtime_error("primary expected");
        }
    }
    // should only reach here if everything failed, throw error
    throw std::runtime_error("Reached bottom of primary, something very wrong.");
}

// exactly like expression(), but for '*', '/', '%', and '^'
double term()
{
    double left = primary();    // get the Primary
    while (true)
    {
        token t = ts.get();    // get the next Token ...
        switch (t.kind())
        {
        case '*':
            left *= primary();
            break;
        case '/':
        {
            double d = primary();
            if (d == 0)
                throw std::runtime_error("divide by zero");
            left /= d;
            break;
        }
        case '%':       // can't modulo doubles, so I'll just do it manually
        {
            double d = primary();
            if (d==0)
                throw std::runtime_error("modulo by zero");
            if (left==0)    // 0 mod anything is still zero, so just grab next token
                break;
            if (left<0)     // left side negative
            {
                if (d<0)    // both negative
                {
                    while (left<=d)
                    {
                        left-=d;    // both negative, will range between (d,0]
                    }
                }
                else        // right side positive
                {
                    while (left+d<=0)
                    {
                        left +=d;
                    }
                }
            }
            else            // left side positive
            {
                if (d<0)    // right side negative
                {
                    while (left>0)
                    {
                        left+=d;    // will range between (d,0] 
                    }
                }
                else        // both positive, trivial case
                {
                    while (left>=d)
                    {
                        left -=d;
                    }
                }
            } 
            break; 
        }
        case '^':
        {
            double d = primary();
            left = pow(left,d);
            return left;
        }
        default:
            ts.putback(t);    // <<< put the unused token back
            return left;      // return the value
        }
    }
}

// read and evaluate: 1   1+2.5   1+2+3.14  etc.
// 	 return the sum (or difference)
double expression()
{
    double left = term();    // get the Term
    while (true)
    {
        token t = ts.get();    // get the next token…
        switch (t.kind())      // ... and do the right thing with it
        {
        case '+':
            left += term();
            break;
        case '-':
            left -= term();
            break;
        default:
            ts.putback(t);    // <<< put the unused token back
            return left;      // return the value of the expression
        }
    }
}

void clean_up_mess()
{
    ts.ignore(print); 
}

// definitions of helper functions

// small helper function return all lowercase equivalent of str
const std::string lowcase(const std::string str){
    std::string lresult;                // result string to build
    char ch;
    for (int i=0; i<str.size();i++)
    {
        ch=str[i];
        if (isalpha(ch))
            ch = tolower(ch);
        lresult.push_back(ch);
    }
    const std::string out = lresult;
    return out;
}

// small helper function to assist peek checking, 
// true if ch is not an operator, false if it is
bool is_op(const char ch){
    for (int i=0; i<NUM_OP; i++)
    {
        if (ch==operators[i])
            return true;
    }
    return false;    
}

// small helper function to determine if a variable is a system constant
// true if varName is a system constant
bool is_sysvar(const std::string varName){
    for (int i=0; i<NUM_SYSVAR; i++)
    {
        if (varName==reservedVarNames[i])       // cycle through sysvar portion of reserved
            return true;
    }
    return false;
}

// small helper function to return value of system constant
// expects you've checked that is_sysvar, will throw error if not is_sysvar
const double get_sysvar(const std::string varName){
    for (int i=0; i<NUM_SYSVAR; i++)
    {
        if (varName==reservedVarNames[i])       // cycle through sysvar portion of reserved
            return syscons[i];
    }
    // only get here if it's not a system var
    throw std::runtime_error(std::string("Attempted to get non-existant system constant ")+varName);
}

// small helper function to determine if a variable is a protected command
// true if varName is a protected command, false otherwise
bool is_command(const std::string varName){
    int offset = NUM_SYSVAR;
    for (int i=0; i<NUM_COMMAND; i++)
    {
        if (varName==reservedVarNames[offset+i])  // cycle through command portion of reserved
            return true;
    }
    return false;
}

// small helper function to determine if a variable is a protected option/target
// true if varName is a valid option/target, false otherwise
bool is_option(const std::string varName){
    int offset = NUM_SYSVAR+NUM_COMMAND;
    for (int i=0; i<NUM_OPTIONS; i++)
    {
        if (varName==reservedVarNames[offset+i])  // cycle through option portion of reserved
            return true;
    }
    return false;
}

// small helper function to return proper flag value
const double get_option(const std::string varName){
    int offset = NUM_SYSVAR+NUM_COMMAND;
    for (int i=0; i<NUM_OPTIONS; i++)
    {
        if (varName==reservedVarNames[offset+i])  // cycle through option portion of reserved
        {
            const double flag = pow(2,i);
            return flag;
        }
    }
    // fail condition
    return 0;
}

// small helper to check if variable is an existing user variable
// returns true if it is, false if it isn't
bool is_usrvar(const std::string varName){
    int select = -1;
    for (int i=0; i<vecUserVars.size(); i++)
    {
        if (vecUserVars[i].sname()==varName)
        {
            select = i;
            break;
        }
    }
    if (select!=-1)  // found
        return true;
    else
        return false;
}

// small helper to get existing user variable value
// error if not in user variables
const double get_usrvar(const std::string varName){
    double result;
    int select = -1;
    for (int i=0; i<vecUserVars.size(); i++)
    {
        if (vecUserVars[i].sname()==varName)
        {
            select = i;
            break;
        }
    }
    if (select<0)
        throw std::runtime_error(std::string("Tried to access non-existant user var ")+varName);
    else
    {
        result = vecUserVars[select].getvalue();
        return result;
    }
}

// small helper function to tell if character is a break character ';' '\n' EOF '\0' ' '
bool is_break(const char ch){
    return (ch==';' || ch=='\n' || ch==EOF );
}



void calculate()
{
    while (std::cin)
    {
        try        
        {
            std::cout << prompt;    // print prompt
            token t = ts.get();
            // first discard all “prints”
            while (t.kind() == print)
                t = ts.get();

            switch (t.kind())
            {
                case quit:
                {
                    return;    // ‘q’ or “quit”
                }
                case help:     // print help message
                {
                    std::cout << std::endl << helptext << std::endl;
                    break;
                }
                case disp:   // command to display something
                {
                    int flag = 0;   //get int to switch based off flags, adjust for float precision
                    flag += t.value();
                    switch (flag)
                    {
                        case DISP_SYS_FLAG:
                        {
                            std::cout << "Displaying system constants:" << std::endl;
                            for (int i=0; i<NUM_SYSVAR; i++)
                            {
                                std::cout << "Constant name: " << reservedVarNames[i] << " = " << syscons[i] <<std::endl;
                            }
                            break;
                        }
                        case DISP_USER_FLAG:
                        {
                            int uvsize = vecUserVars.size();
                            if (uvsize==0)      // no user variables
                            {
                                std::cout << "No user variables to display." << std::endl;
                                break;
                            }
                            std::cout << "Displaying all " << uvsize << " user variables:" << std::endl;
                            for (int i=0; i<uvsize; i++)
                            {
                                std::cout << "Variable name: " << vecUserVars[i].sname() << " = " << vecUserVars[i].getvalue() << std::endl;
                            }
                            break;
                        }
                        case DISP_ALL_FLAG:
                        {
                            std::cout << "Displaying all system constants, then all user variables..." << std::endl;
                            std::cout << "Displaying system constants:" << std::endl;
                            for (int i=0; i<NUM_SYSVAR; i++)
                            {
                                std::cout << "Constant name: " << reservedVarNames[i] << " = " << syscons[i] <<std::endl;
                            }
                            int uvsize = vecUserVars.size();
                            if (uvsize==0)      // no user variables
                            {
                                std::cout << "No user variables to display." << std::endl;
                                break;
                            }
                            std::cout << "Displaying all " << uvsize << " user variables:" << std::endl;
                            for (int i=0; i<uvsize; i++)
                            {
                                std::cout << "Variable name: " << vecUserVars[i].sname() << " = " << vecUserVars[i].getvalue() << std::endl;
                            }
                            break;
                        }
                        case DISP_OP_FLAG:
                        {
                            std::cout << "Displaying valid operators:" << std::endl;
                            for (int i=0; i<NUM_OP; i++)
                                std::cout << operators[i] << " : " << opdescrip[i] << std::endl;
                            break;
                        }
                        default:
                            throw std::runtime_error("Invalid display code for display command.");
                    }
                    break;
                }
                case del:
                {
                    if (t.value()==DELETE_ALL)
                    {
                        vecUserVars.clear();
                        std::cout << "Cleared all user variables." << std::endl;
                        break;
                    }
                    int select = -1;
                    for (int i=0; i<vecUserVars.size(); i++)
                    {
                        if (vecUserVars[i].sname()==t.getname())
                        {
                            select = i;
                            break;
                        }
                    }
                    if (select<0)
                        throw std::runtime_error("Invalid target name for deletion");
                    std::string delname = vecUserVars[select].sname();
                    vecUserVars.erase(vecUserVars.begin()+select);
                    std::cout << "Succesfully erased variable " << delname << std::endl;
                    break;
                }
                case setvar:
                {   
                    int assign = t.value();
                    std::string vname = t.getname();
                    if (assign==0)  // create new var and set to zero
                    {
                        if (is_usrvar(vname)) //already exists
                        {
                            throw std::runtime_error(std::string("Tried to create an existing variable")+vname);
                            break;
                        }
                        else     // create and zero var
                        {
                            UserVar *uv = new UserVar(vname,0);
                            vecUserVars.push_back(*uv);
                            std::cout << "Created new user variable " << vname << " with value 0." << std::endl;
                            break;
                        }
                    }
                    if (assign==1)  // assign with following expression
                    {
                        int select = -1;
                        for (int i=0;i<vecUserVars.size();i++)
                        {
                            if (vecUserVars[i].sname()==t.getname())
                            {
                                select = i;
                                break;
                            }
                        }
                        if (select<0)   // varname doesn't yet exist, get value then create it
                        {   
                            double dval = expression();
                            UserVar *uv = new UserVar(vname,dval);
                            vecUserVars.push_back(*uv);
                            std::cout << "Created new user variable " << vname << " with value " << dval << std::endl;
                            break;
                        }
                        else    // existing var, replace value
                        {
                            double oldval = vecUserVars[select].getvalue();
                            double newval = expression();
                            vecUserVars[select].setvalue(newval);
                            std::cout << "User variable " << vname << " updated, was " << oldval << ", now " << vname << " = " << newval << std::endl;
                            break;
                        }
                    }
                    // only here for invalid assign set, throw error
                    throw std::runtime_error("Invalid setvar assign value, must be 0 or 1");
                }
                // all these are acceptable starts to expression
                case '(':   
                case '-':
                case number:
                {
                    ts.putback(t);
                    std::cout << result << expression() << std::endl;
                    break;
                }
                default:
                    throw std::runtime_error("No matching kind for token");
            }
        }
        catch (std::runtime_error const& e)
        {
            std::cerr << e.what() << std::endl;    // write error message
            clean_up_mess();                       // <<< The tricky part!
        }
    }
}

int main()
{
    try
    {
        calculate();
        return 0;
    }
    catch (...)
    {
        // other errors (don't try to recover)
        std::cerr << "exception\n";
        return 2;
    }
}
