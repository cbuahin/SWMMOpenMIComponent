#ifndef CONTROLS_H
#define CONTROLS_H

//-----------------------------------------------------------------------------
//  Constants
//-----------------------------------------------------------------------------
enum RuleState   {
	r_RULE, r_IF, r_AND, r_OR, r_THEN, r_ELSE, r_PRIORITY,
	r_ERROR
};
enum RuleObject  {
	r_NODE, r_LINK, r_CONDUIT, r_PUMP, r_ORIFICE, r_WEIR,
	r_OUTLET, r_SIMULATION
};
enum RuleAttrib  {
	r_DEPTH, r_HEAD, r_INFLOW, r_FLOW, r_STATUS, r_SETTING,
	r_TIME, r_DATE, r_CLOCKTIME, r_DAY, r_MONTH
};
enum RuleOperand { EQ, NE, LT, LE, GT, GE };
enum RuleSetting { r_CURVE, r_TIMESERIES, r_PID, r_NUMERIC };

static char* ObjectWords[] =
{ "NODE", "LINK", "CONDUIT", "PUMP", "ORIFICE", "WEIR", "OUTLET",
"SIMULATION", NULL };
static char* AttribWords[] =
{ "DEPTH", "HEAD", "INFLOW", "FLOW", "STATUS", "SETTING",
"TIME", "DATE", "CLOCKTIME", "DAY", "MONTH", NULL };
static char* OperandWords[] = { "=", "<>", "<", "<=", ">", ">=", NULL };
static char* StatusWords[] = { "OFF", "ON", NULL };
static char* ConduitWords[] = { "CLOSED", "OPEN", NULL };
static char* SettingTypeWords[] = { "CURVE", "TIMESERIES", "PID", NULL };

//-----------------------------------------------------------------------------                  
// Data Structures
//-----------------------------------------------------------------------------                  
// Rule Premise Clause 
struct  TPremise
{
	int      type;
	int      node;
	int      link;
	int      attribute;
	int      operand;
	double   value;
	struct   TPremise *next;
};

typedef struct TPremise TPremise;

// Rule Action Clause
struct  TAction
{
	int     rule;
	int     link;
	int     attribute;
	int     curve;
	int     tseries;
	double  value;
	double  kp, ki, kd;
	double  e1, e2;
	struct  TAction *next;
};

typedef struct TAction TAction;


// List of Control Actions
struct  TActionList
{
	struct  TAction* action;
	struct  TActionList* next;
};
typedef struct TActionList TActionList;

// Control Rule
struct  TRule
{
	char*    ID;                        // rule ID
	double   priority;                  // Priority level
	struct   TPremise* firstPremise;    // Pointer to first premise of rule
	struct   TPremise* lastPremise;     // Pointer to last premise of rule
	struct   TAction*  thenActions;     // Linked list of actions if true
	struct   TAction*  elseActions;     // Linked list of actions if false
};

typedef struct TRule TRule;

#endif