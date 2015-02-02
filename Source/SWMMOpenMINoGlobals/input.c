//-----------------------------------------------------------------------------
//   input.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/20/14  (Build 5.1.001)
//   Author:   L. Rossman
//
//   Input data processing functions.
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE


#include <stdlib.h>
#include <string.h>
//#include <malloc.h>
#include <math.h>
#include "headers.h"
#include "lid.h"

//-----------------------------------------------------------------------------
//  Constants
//-----------------------------------------------------------------------------
static const int MAXERRS = 100;        // Max. input errors reported

//-----------------------------------------------------------------------------
//  Shared variables
//-----------------------------------------------------------------------------
//static char *Tok[MAXTOKS];             // String tokens from line of input
//static int  Ntokens;                   // Number of tokens in line of input
//static int  Mobjects[MAX_OBJ_TYPES];   // Working number of objects of each type
//static int  Mnodes[MAX_NODE_TYPES];    // Working number of node objects
//static int  Mlinks[MAX_LINK_TYPES];    // Working number of link objects

//-----------------------------------------------------------------------------
//  External Functions (declared in funcs.h)
//-----------------------------------------------------------------------------
//  input_countObjects  (called by swmm_open in swmm5.c)
//  input_readData      (called by swmm_open in swmm5.c)

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------
static int  addObject(Project* project, int objType, char* id);
static int  getTokens(Project* project, char *s);
static int  parseLine(Project* project, int sect, char* line);
static int  readOption(Project* project, char* line);
static int  readTitle(Project* project, char* line);
static int  readControl(Project* project, char* tok[], int ntoks);
static int  readNode(Project* project, int type);
static int  readLink(Project* project, int type);


//=============================================================================

int input_countObjects(Project* project)
//
//  Input:   none
//  Output:  returns error code
//  Purpose: reads input file to determine number of system objects.
//
{
    char  line[MAXLINE+1];             // line from input data file     
    char  wLine[MAXLINE+1];            // working copy of input line   
    char  *tok;                        // first string token of line          
    int   sect = -1, newsect;          // input data sections          
    int   errcode = 0;                 // error code
    int   errsum = 0;                  // number of errors found                   
    int   i;
    long  lineCount = 0;

    // --- initialize number of objects & set default values
    if ( project->ErrorCode ) return project->ErrorCode;
    error_setInpError(0, "");
    for (i = 0; i < MAX_OBJ_TYPES; i++)
		project->Nobjects[i] = 0;
	for (i = 0; i < MAX_NODE_TYPES; i++) project->Nnodes[i] = 0;
	for (i = 0; i < MAX_LINK_TYPES; i++) project->Nlinks[i] = 0;

    // --- make pass through data file counting number of each object
	while (fgets(line, MAXLINE, project->Finp.file) != NULL)
    {
        // --- skip blank lines & those beginning with a comment
        lineCount++;
        strcpy(wLine, line);           // make working copy of line
        tok = strtok(wLine, SEPSTR);   // get first text token on line
        if ( tok == NULL ) continue;
        if ( *tok == ';' ) continue;

        // --- check if line begins with a new section heading
        if ( *tok == '[' )
        {
            // --- look for heading in list of section keywords
            newsect = findmatch(tok, SectWords);
            if ( newsect >= 0 )
            {
                sect = newsect;
                continue;
            }
            else
            {
                sect = -1;
                errcode = ERR_KEYWORD;
            }
        }

        // --- if in OPTIONS section then read the option setting
        //     otherwise add object and its ID name (tok) to project
        if ( sect == s_OPTION ) 
			errcode = readOption(project, line);
        else if ( sect >= 0 )   
			errcode = addObject(project, sect, tok);

        // --- report any error found
        if ( errcode )
        {
            report_writeInputErrorMsg(project,errcode, sect, line, lineCount);
            errsum++;
            if (errsum >= MAXERRS ) break;
        }
    }

    // --- set global error code if input errors were found
	if (errsum > 0) project->ErrorCode = ERR_INPUT;
	return project->ErrorCode;
}

//=============================================================================

int input_readData(Project* project)
//
//  Input:   none
//  Output:  returns error code
//  Purpose: reads input file to determine input parameters for each object.
//
{
    char  line[MAXLINE+1];        // line from input data file
    char  wLine[MAXLINE+1];       // working copy of input line
    char* comment;                // ptr. to start of comment in input line
    int   sect, newsect;          // data sections
    int   inperr, errsum;         // error code & total error count
    int   lineLength;             // number of characters in input line
    int   i;
    long  lineCount = 0;

    // --- initialize working item count arrays
    //     (final counts in Mobjects, Mnodes & Mlinks should
    //      match those in Nobjects, Nnodes and Nlinks).
	if (project->ErrorCode) return project->ErrorCode;
    error_setInpError(0, "");
	for (i = 0; i < MAX_OBJ_TYPES; i++)  project->Mobjects[i] = 0;
	for (i = 0; i < MAX_NODE_TYPES; i++) project->Mnodes[i] = 0;
	for (i = 0; i < MAX_LINK_TYPES; i++) project->Mlinks[i] = 0;

    // --- initialize starting date for all time series
	for (i = 0; i < project->Nobjects[TSERIES]; i++)
    {
		project->Tseries[i].lastDate = project->StartDate + project->StartTime;
    }

    // --- read each line from input file
    sect = 0;
    errsum = 0;
	rewind(project->Finp.file);
	while (fgets(line, MAXLINE, project->Finp.file) != NULL)
    {
        // --- make copy of line and scan for tokens
        lineCount++;
        strcpy(wLine, line);
		project->Ntokens = getTokens(project, wLine);

        // --- skip blank lines and comments
		if (project->Ntokens == 0) continue;
		if (*project->Tok[0] == ';') continue;

        // --- check if max. line length exceeded
        lineLength = strlen(line);
        if ( lineLength >= MAXLINE )
        {
            // --- don't count comment if present
            comment = strchr(line, ';');
            if ( comment ) lineLength = comment - line;    // Pointer math here
            if ( lineLength >= MAXLINE )
            {
                inperr = ERR_LINE_LENGTH;
                report_writeInputErrorMsg(project,inperr, sect, line, lineCount);
                errsum++;
            }
        }

        // --- check if at start of a new input section
		if (*project->Tok[0] == '[')
        {
            // --- match token against list of section keywords
			newsect = findmatch(project->Tok[0], SectWords);
            if (newsect >= 0)
            {
                // --- SPECIAL CASE FOR TRANSECTS
                //     finish processing the last set of transect data
                if ( sect == s_TRANSECT )
					transect_validate(project,project->Nobjects[TRANSECT] - 1);

                // --- begin a new input section
                sect = newsect;
                continue;
            }
            else
            {
				inperr = error_setInpError(ERR_KEYWORD, project->Tok[0]);
                report_writeInputErrorMsg(project,inperr, sect, line, lineCount);
                errsum++;
                break;
            }
        }

        // --- otherwise parse tokens from input line
        else
        {
            inperr = parseLine(project,sect, line);
            if ( inperr > 0 )
            {
                errsum++;
                if ( errsum > MAXERRS ) report_writeLine(project, FMT19);
                else report_writeInputErrorMsg(project,inperr, sect, line, lineCount);
            }
        }

        // --- stop if reach end of file or max. error count
        if (errsum > MAXERRS) break;
    }   /* End of while */

    // --- check for errors
	if (errsum > 0)  project->ErrorCode = ERR_INPUT;
	return project->ErrorCode;
}

//=============================================================================

int  addObject(Project* project, int objType, char* id)
//
//  Input:   objType = object type index
//           id = object's ID string
//  Output:  returns an error code
//  Purpose: adds a new object to the project.
//
{
    int errcode = 0;
    switch( objType )
    {
      case s_RAINGAGE:
		  if (!project_addObject(project,GAGE, id, project->Nobjects[GAGE]))
            errcode = error_setInpError(ERR_DUP_NAME, id);
		  project->Nobjects[GAGE]++;
        break;

      case s_SUBCATCH:
		  if (!project_addObject(project,SUBCATCH, id, project->Nobjects[SUBCATCH]))
            errcode = error_setInpError(ERR_DUP_NAME, id);
		  project->Nobjects[SUBCATCH]++;
        break;

      case s_AQUIFER:
		  if (!project_addObject(project,AQUIFER, id, project->Nobjects[AQUIFER]))
            errcode = error_setInpError(ERR_DUP_NAME, id);
		  project->Nobjects[AQUIFER]++;
        break;

      case s_UNITHYD:
        // --- the same Unit Hydrograph can span several lines
        if ( project_findObject(project, UNITHYD, id) < 0 )
        {
			if (!project_addObject(project,UNITHYD, id, project->Nobjects[UNITHYD]))
                errcode = error_setInpError(ERR_DUP_NAME, id);
			project->Nobjects[UNITHYD]++;
        }
        break;

      case s_SNOWMELT:
        // --- the same Snowmelt object can appear on several lines
        if ( project_findObject(project,SNOWMELT, id) < 0 )
        {
			if (!project_addObject(project,SNOWMELT, id, project->Nobjects[SNOWMELT]))
                errcode = error_setInpError(ERR_DUP_NAME, id);
			project->Nobjects[SNOWMELT]++;
        }
        break;

      case s_JUNCTION:
		  if (!project_addObject(project,NODE, id, project->Nobjects[NODE]))
            errcode = error_setInpError(ERR_DUP_NAME, id);
		  project->Nobjects[NODE]++;
		  project->Nnodes[JUNCTION]++;
        break;

      case s_OUTFALL:
		  if (!project_addObject(project, NODE, id, project->Nobjects[NODE]))
            errcode = error_setInpError(ERR_DUP_NAME, id);
		  project->Nobjects[NODE]++;
		  project->Nnodes[OUTFALL]++;
        break;

      case s_STORAGE:
		  if (!project_addObject(project, NODE, id, project->Nobjects[NODE]))
            errcode = error_setInpError(ERR_DUP_NAME, id);
		  project->Nobjects[NODE]++;
		  project->Nnodes[STORAGE]++;
        break;

      case s_DIVIDER:
		  if (!project_addObject(project,NODE, id, project->Nobjects[NODE]))
            errcode = error_setInpError(ERR_DUP_NAME, id);
		  project->Nobjects[NODE]++;
		  project->Nnodes[DIVIDER]++;
        break;

      case s_CONDUIT:
		  if (!project_addObject(project,LINK, id, project->Nobjects[LINK]))
            errcode = error_setInpError(ERR_DUP_NAME, id);
		  project->Nobjects[LINK]++;
		  project->Nlinks[CONDUIT]++;
        break;

      case s_PUMP:
		  if (!project_addObject(project,LINK, id, project->Nobjects[LINK]))
            errcode = error_setInpError(ERR_DUP_NAME, id);
		  project->Nobjects[LINK]++;
		  project->Nlinks[PUMP]++;
        break;

      case s_ORIFICE:
		  if (!project_addObject(project,LINK, id, project->Nobjects[LINK]))
            errcode = error_setInpError(ERR_DUP_NAME, id);
		  project->Nobjects[LINK]++;
		  project->Nlinks[ORIFICE]++;
        break;

      case s_WEIR:
		  if (!project_addObject(project,LINK, id, project->Nobjects[LINK]))
            errcode = error_setInpError(ERR_DUP_NAME, id);
		  project->Nobjects[LINK]++;
		  project->Nlinks[WEIR]++;
        break;

      case s_OUTLET:
		  if (!project_addObject(project,LINK, id, project->Nobjects[LINK]))
            errcode = error_setInpError(ERR_DUP_NAME, id);
		  project->Nobjects[LINK]++;
		  project->Nlinks[OUTLET]++;
        break;

      case s_POLLUTANT:
		  if (!project_addObject(project,POLLUT, id, project->Nobjects[POLLUT]))
            errcode = error_setInpError(ERR_DUP_NAME, id);
		  project->Nobjects[POLLUT]++;
        break;

      case s_LANDUSE:
		  if (!project_addObject(project,LANDUSE, id, project->Nobjects[LANDUSE]))
            errcode = error_setInpError(ERR_DUP_NAME, id);
		  project->Nobjects[LANDUSE]++;
        break;

      case s_PATTERN:
        // --- a time pattern can span several lines
        if ( project_findObject(project,TIMEPATTERN, id) < 0 )
        {
			if (!project_addObject(project,TIMEPATTERN, id, project->Nobjects[TIMEPATTERN]))
                errcode = error_setInpError(ERR_DUP_NAME, id);
			project->Nobjects[TIMEPATTERN]++;
        }
        break;

      case s_CURVE:
        // --- a Curve can span several lines
        if ( project_findObject(project,CURVE, id) < 0 )
        {
			if (!project_addObject(project,CURVE, id, project->Nobjects[CURVE]))
                errcode = error_setInpError(ERR_DUP_NAME, id);
			project->Nobjects[CURVE]++;

            // --- check for a conduit shape curve
            id = strtok(NULL, SEPSTR);
            if ( findmatch(id, CurveTypeWords) == SHAPE_CURVE )
				project->Nobjects[SHAPE]++;
        }
        break;

      case s_TIMESERIES:
        // --- a Time Series can span several lines
        if ( project_findObject(project,TSERIES, id) < 0 )
        {
			if (!project_addObject(project,TSERIES, id, project->Nobjects[TSERIES]))
                errcode = error_setInpError(ERR_DUP_NAME, id);
			project->Nobjects[TSERIES]++;
        }
        break;

      case s_CONTROL:
		  if (match(id, w_RULE)) project->Nobjects[CONTROL]++;
        break;

      case s_TRANSECT:
        // --- for TRANSECTS, ID name appears as second entry on X1 line
        if ( match(id, "X1") )
        {
            id = strtok(NULL, SEPSTR);
            if ( id ) 
            {
				if (!project_addObject(project,TRANSECT, id, project->Nobjects[TRANSECT]))
                    errcode = error_setInpError(ERR_DUP_NAME, id);
				project->Nobjects[TRANSECT]++;
            }
        }
        break;

      case s_LID_CONTROL:
        // --- an LID object can span several lines
        if ( project_findObject(project,LID, id) < 0 )
        {
			if (!project_addObject(project, LID, id, project->Nobjects[LID]))
            {
                errcode = error_setInpError(ERR_DUP_NAME, id);
            }
			project->Nobjects[LID]++;
        }
        break;
    }
    return errcode;
}

//=============================================================================

int  parseLine(Project* project, int sect, char *line)
//
//  Input:   sect  = current section of input file
//           *line = line of text read from input file
//  Output:  returns error code or 0 if no error found
//  Purpose: parses contents of a tokenized line of text read from input file.
//
{
    int j, err;
    switch (sect)
    {
      case s_TITLE:
		  return readTitle(project, line);

      case s_RAINGAGE:
        j = project->Mobjects[GAGE];
		err = gage_readParams(project, j, project->Tok, project->Ntokens);
		project->Mobjects[GAGE]++;
        return err;

      case s_TEMP:
		  return climate_readParams(project, project->Tok, project->Ntokens);

      case s_EVAP:
		  return climate_readEvapParams(project, project->Tok, project->Ntokens);

      case s_SUBCATCH:
        j = project->Mobjects[SUBCATCH];
		err = subcatch_readParams(project, j, project->Tok, project->Ntokens);
		project->Mobjects[SUBCATCH]++;
        return err;

      case s_SUBAREA:
		  return subcatch_readSubareaParams(project,project->Tok, project->Ntokens);

      case s_INFIL:
		  return infil_readParams(project,project->InfilModel, project->Tok, project->Ntokens);

      case s_AQUIFER:
		  j = project->Mobjects[AQUIFER];
		  err = gwater_readAquiferParams(project, j, project->Tok, project->Ntokens);
		  project->Mobjects[AQUIFER]++;
        return err;

      case s_GROUNDWATER:
		  return gwater_readGroundwaterParams(project, project->Tok, project->Ntokens);

	  case s_GWFLOW:
		  return gwater_readFlowExpression(project, project->Tok, project->Ntokens);

      case s_SNOWMELT:
		  return snow_readMeltParams(project,project->Tok, project->Ntokens);

      case s_JUNCTION:
		  return readNode(project, JUNCTION);

      case s_OUTFALL:
		  return readNode(project, OUTFALL);

      case s_STORAGE:
		  return readNode(project, STORAGE);

      case s_DIVIDER:
		  return readNode(project, DIVIDER);

      case s_CONDUIT:
		  return readLink(project, CONDUIT);

      case s_PUMP:
		  return readLink(project, PUMP);

      case s_ORIFICE:
		  return readLink(project, ORIFICE);

      case s_WEIR:
		  return readLink(project, WEIR);

      case s_OUTLET:
		  return readLink(project, OUTLET);

      case s_XSECTION:
		  return link_readXsectParams(project,project->Tok, project->Ntokens);

      case s_TRANSECT:
		  return transect_readParams(project,&project->Mobjects[TRANSECT], project->Tok, project->Ntokens);

      case s_LOSSES:
		  return link_readLossParams(project,project->Tok, project->Ntokens);

      case s_POLLUTANT:
		  j = project->Mobjects[POLLUT];
		  err = landuse_readPollutParams(project,j, project->Tok, project->Ntokens);
		  project->Mobjects[POLLUT]++;
        return err;

      case s_LANDUSE:
		  j = project->Mobjects[LANDUSE];
		err = landuse_readParams(project,j, project->Tok, project->Ntokens);
		project->Mobjects[LANDUSE]++;
        return err;

      case s_BUILDUP:
		  return landuse_readBuildupParams(project,project->Tok, project->Ntokens);

      case s_WASHOFF:
		  return landuse_readWashoffParams(project,project->Tok, project->Ntokens);

      case s_COVERAGE:
		  return subcatch_readLanduseParams(project,project->Tok, project->Ntokens);

      case s_INFLOW:
		  return inflow_readExtInflow(project,project->Tok, project->Ntokens);

      case s_DWF:
		  return inflow_readDwfInflow(project,project->Tok, project->Ntokens);

      case s_PATTERN:
		  return inflow_readDwfPattern(project,project->Tok, project->Ntokens);

      case s_RDII:
		  return rdii_readRdiiInflow(project,project->Tok, project->Ntokens);

      case s_UNITHYD:
		  return rdii_readUnitHydParams(project,project->Tok, project->Ntokens);

      case s_LOADING:
		  return subcatch_readInitBuildup(project,project->Tok, project->Ntokens);

      case s_TREATMENT:
		  return treatmnt_readExpression(project, project->Tok, project->Ntokens);

      case s_CURVE:
		  return table_readCurve(project, project->Tok, project->Ntokens);

      case s_TIMESERIES:
		  return table_readTimeseries(project, project->Tok, project->Ntokens);

      case s_CONTROL:
		  return readControl(project, project->Tok, project->Ntokens);

      case s_REPORT:
		  return report_readOptions(project, project->Tok, project->Ntokens);

      case s_FILE:
		  return iface_readFileParams(project,project->Tok, project->Ntokens);

      case s_LID_CONTROL:
		  return lid_readProcParams(project, project->Tok, project->Ntokens);

      case s_LID_USAGE:
		  return lid_readGroupParams(project, project->Tok, project->Ntokens);

      default: return 0;
    }
}

//=============================================================================

int readControl(Project* project ,char* tok[], int ntoks)
//
//  Input:   tok[] = array of string tokens
//           ntoks = number of tokens
//  Output:  returns error code
//  Purpose: reads a line of input for a control rule.
//
{
    int index;
    int keyword;

    // --- check for minimum number of tokens
    if ( ntoks < 2 ) return error_setInpError(ERR_ITEMS, "");

    // --- get index of control rule keyword
    keyword = findmatch(tok[0], RuleKeyWords);
    if ( keyword < 0 ) return error_setInpError(ERR_KEYWORD, tok[0]);

    // --- if line begins a new control rule, add rule ID to the database
    if ( keyword == 0 )
    {
		if (!project_addObject(project, CONTROL, tok[1], project->Mobjects[CONTROL]))
        {
			return error_setInpError(ERR_DUP_NAME, project->Tok[1]);
        }
		project->Mobjects[CONTROL]++;
    }

    // --- get index of last control rule processed
	index = project->Mobjects[CONTROL] - 1;
    if ( index < 0 ) return error_setInpError(ERR_RULE, "");

    // --- add current line as a new clause to the control rule
	return controls_addRuleClause(project, index, keyword, project->Tok, project->Ntokens);
}

//=============================================================================

int readOption(Project* project, char* line)
//
//  Input:   line = line of input data
//  Output:  returns error code
//  Purpose: reads an input line containing a project option.
//
{
	project->Ntokens = getTokens(project, line);
	if (project->Ntokens < 2) return 0;
	return project_readOption(project, project->Tok[0], project->Tok[1]);
}

//=============================================================================

int readTitle(Project* project, char* line)
//
//  Input:   line = line from input file
//  Output:  returns error code
//  Purpose: reads project title from line of input.
//
{
    int i, n;
    for (i = 0; i < MAXTITLE; i++)
    {
        // --- find next empty Title entry
		if (strlen(project->Title[i]) == 0)
        {
            // --- strip line feed character from input line
            n = strlen(line);
            if (line[n-1] == 10) line[n-1] = ' ';

            // --- copy input line into Title entry
			sstrncpy(project->Title[i], line, MAXMSG);
            break;
        }
    }
    return 0;
}

//=============================================================================
    
int readNode(Project* project, int type)
//
//  Input:   type = type of node
//  Output:  returns error code
//  Purpose: reads data for a node from a line of input.
//
{
	int j = project->Mobjects[NODE];
	int k = project->Mnodes[type];
	int err = node_readParams(project,j, type, k, project->Tok, project->Ntokens);
	project->Mobjects[NODE]++;
	project->Mnodes[type]++;
    return err;
}

//=============================================================================

int readLink(Project* project, int type)
//
//  Input:   type = type of link
//  Output:  returns error code
//  Purpose: reads data for a link from a line of input.
//
{
	int j = project->Mobjects[LINK];
	int k = project->Mlinks[type];
	int err = link_readParams(project,j, type, k, project->Tok, project->Ntokens);
	project->Mobjects[LINK]++;
	project->Mlinks[type]++;
    return err;
}

//=============================================================================

int  findmatch(char *s, char *keyword[])
//
//  Input:   s = character string
//           keyword = array of keyword strings
//  Output:  returns index of matching keyword or -1 if no match found  
//  Purpose: finds match between string and array of keyword strings.
//
{
   int i = 0;
   while (keyword[i] != NULL)
   {
      if (match(s, keyword[i])) return(i);
      i++;
   }
   return(-1);
}

//=============================================================================

int  match(char *str, char *substr)
//
//  Input:   str = character string being searched
//           substr = sub-string being searched for
//  Output:  returns 1 if sub-string found, 0 if not
//  Purpose: sees if a sub-string of characters appears in a string
//           (not case sensitive).
//
{
    int i,j;

    // --- fail if substring is empty
    if (!substr[0]) return(0);

    // --- skip leading blanks of str
    for (i = 0; str[i]; i++)
    {
        if (str[i] != ' ') break;
    }

    // --- check if substr matches remainder of str
    for (i = i,j = 0; substr[j]; i++,j++)
    {
        if (!str[i] || UCHAR(str[i]) != UCHAR(substr[j])) return(0);
    }
    return(1);
}

//=============================================================================

int  getInt(char *s, int *y)
//
//  Input:   s = a character string
//  Output:  y = converted value of s,
//           returns 1 if conversion successful, 0 if not
//  Purpose: converts a string to an integer number.
//
{
    double x;
    if ( getDouble(s, &x) )
    {
        if ( x < 0.0 ) x -= 0.01;
        else x += 0.01;
        *y = (int)x;
        return 1;
    }
    *y = 0;
    return 0;
}

//=============================================================================

int  getFloat(char *s, float *y)
//
//  Input:   s = a character string
//  Output:  y = converted value of s,
//           returns 1 if conversion successful, 0 if not
//  Purpose: converts a string to a single precision floating point number.
//
{
    char *endptr;
    *y = (float) strtod(s, &endptr);
    if (*endptr > 0) return(0);
    return(1);
}

//=============================================================================

int  getDouble(char *s, double *y)
//
//  Input:   s = a character string
//  Output:  y = converted value of s,
//           returns 1 if conversion successful, 0 if not
//  Purpose: converts a string to a double precision floating point number.
//
{
    char *endptr;
    *y = strtod(s, &endptr);
    if (*endptr > 0) return(0);
    return(1);
}

//=============================================================================

int  getTokens(Project* project, char *s)
//
//  Input:   s = a character string
//  Output:  returns number of tokens found in s
//  Purpose: scans a string for tokens, saving pointers to them
//           in shared variable Tok[].
//
//  Notes:   Tokens can be separated by the characters listed in SEPSTR
//           (spaces, tabs, newline, carriage return) which is defined
//           in CONSTS.H. Text between quotes is treated as a single token.
//
{
    int  len, m, n;
    char *c;

    // --- begin with no tokens
	for (n = 0; n < MAXTOKS; n++) project->Tok[n] = NULL;
    n = 0;

    // --- truncate s at start of comment 
    c = strchr(s,';');
    if (c) *c = '\0';
    len = strlen(s);

    // --- scan s for tokens until nothing left
    while (len > 0 && n < MAXTOKS)
    {
        m = strcspn(s,SEPSTR);              // find token length 
        if (m == 0) s++;                    // no token found
        else
        {
            if (*s == '"')                  // token begins with quote
            {
                s++;                        // start token after quote
                len--;                      // reduce length of s
                m = strcspn(s,"\"\n");      // find end quote or new line
            }
            s[m] = '\0';                    // null-terminate the token
			project->Tok[n] = s;                     // save pointer to token 
            n++;                            // update token count
            s += m+1;                       // begin next token
        }
        len -= m+1;                         // update length of s
    }
    return(n);
}

//=============================================================================
