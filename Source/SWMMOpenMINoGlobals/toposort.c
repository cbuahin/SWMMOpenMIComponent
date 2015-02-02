//-----------------------------------------------------------------------------
//   toposort.c
//
//   Project:  EPA SWMM5
//   Version:  5.1
//   Date:     03/20/14   (Build 5.1.001)
//   Author:   L. Rossman
//
//   Topological sorting of conveyance network links
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE


#include <stdlib.h>
#include "headers.h"

//-----------------------------------------------------------------------------
//  Constants
//-----------------------------------------------------------------------------
enum AdjListType {UNDIRECTED, DIRECTED};    // type of nodal adjacency list

////-----------------------------------------------------------------------------
////  Shared variables
////-----------------------------------------------------------------------------
//static int* project->InDegree;                  // number of incoming links to each node
//static int* project->StartPos;                  // start of a node's outlinks in project->AdjList
//static int* project->AdjList;                   // list of outlink indexes for each node
//static int* project->Stack;                     // array of nodes "reached" during sorting
//static int  project->First;                     // position of first node in stack
//static int  project->Last;                      // position of last node added to stack
//
//static char* project->Examined;                 // TRUE if node included in spanning tree
//static char* project->InTree;                   // state of each link in spanning tree:
//                                       // 0 = unexamined,
//                                       // 1 = in spanning tree,
//                                       // 2 = chord of spanning tree
//static int*  project->LoopLinks;                // list of links which forms a loop
//static int   project->LoopLinksLast;            // number of links in a loop

//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)   
//-----------------------------------------------------------------------------
//  toposort_sortLinks (called by routing_open)

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------
static void createAdjList(Project* project, int listType);
static void adjustAdjList(Project* project);
static int  topoSort(Project* project, int sortedLinks[]);
static void findCycles(Project* project);
static void findSpanningTree(Project* project, int startNode);
static void evalLoop(Project* project, int startLink);
static int  traceLoop(Project* project, int i1, int i2, int k);
static void checkDummyLinks(Project* project);
//=============================================================================

void toposort_sortLinks(Project* project, int sortedLinks[])
//
//  Input:   none
//  Output:  sortedLinks = array of link indexes in sorted order
//  Purpose: sorts links from upstream to downstream.
//
{
    int i, n = 0;

    // --- no need to sort links for Dyn. Wave routing
    for ( i=0; i<project->Nobjects[LINK]; i++) sortedLinks[i] = i;
    if ( project->RouteModel == DW )
    {

        // --- check for nodes with both incoming and outgoing
        //     dummy links (creates ambiguous ordering)
        checkDummyLinks(project);
        if ( project->ErrorCode ) return;

        // --- find number of outflow links for each node
        for ( i=0; i<project->Nobjects[NODE]; i++ ) project->Node[i].degree = 0;
        for ( i=0; i<project->Nobjects[LINK]; i++ )
        {
            // --- if upstream node is an outfall, then increment outflow
            //     count for downstream node, otherwise increment count
            //     for upstream node
            n = project->Link[i].node1;
            if ( project->Link[i].direction < 0 ) n = project->Link[i].node2;
            if ( project->Node[n].type == OUTFALL )
            {
                if ( project->Link[i].direction < 0 ) n = project->Link[i].node1;
                else n = project->Link[i].node2;
                project->Node[n].degree++;
            }
            else project->Node[n].degree++;
        }
        return;
    }

    // --- allocate arrays used for topo sorting
    if ( project->ErrorCode ) return;
    project->InDegree = (int *) calloc(project->Nobjects[NODE], sizeof(int));
    project->StartPos = (int *) calloc(project->Nobjects[NODE], sizeof(int));
    project->AdjList  = (int *) calloc(project->Nobjects[LINK], sizeof(int));
    project->Stack    = (int *) calloc(project->Nobjects[NODE], sizeof(int));
    if ( project->InDegree == NULL || project->StartPos == NULL ||
         project->AdjList == NULL || project->Stack == NULL )
    {
        report_writeErrorMsg(project,ERR_MEMORY, "");
    }
    else
    {
        // --- create a directed adjacency list of links leaving each node
        createAdjList(project,DIRECTED);

        // --- adjust adjacency list for DIVIDER nodes
        adjustAdjList(project);

        // --- find number of links entering each node
        for (i = 0; i < project->Nobjects[NODE]; i++) project->InDegree[i] = 0;
        for (i = 0; i < project->Nobjects[LINK]; i++) project->InDegree[ project->Link[i].node2 ]++;

        // --- topo sort the links
        n = topoSort(project,sortedLinks);
    }   

    // --- free allocated memory
    FREE(project->InDegree);
    FREE(project->StartPos);
    FREE(project->AdjList);
    FREE(project->Stack);

    // --- check that all links are included in SortedLinks
    if ( !project->ErrorCode &&  n != project->Nobjects[LINK] )
    {
        report_writeErrorMsg(project,ERR_LOOP, "");
        findCycles(project);
    }
}

//=============================================================================

void createAdjList(Project* project, int listType)
//
//  Input:   lsitType = DIRECTED or UNDIRECTED
//  Output:  none
//  Purpose: creates listing of links incident on each node.
//
{
    int i, j, k;

    // --- determine degree of each node
    //     (for DIRECTED list only count link at its upstream node;
    //      for UNDIRECTED list count link at both end nodes)
    for (i = 0; i < project->Nobjects[NODE]; i++) project->Node[i].degree = 0;
    for (j = 0; j < project->Nobjects[LINK]; j++)
    {
        project->Node[ project->Link[j].node1 ].degree++;
        if ( listType == UNDIRECTED ) project->Node[ project->Link[j].node2 ].degree++;
    }

    // --- determine start position of each node in the adjacency list
    //     (the adjacency list, project->AdjList, is one long vector containing
    //      the individual node lists one after the other)
    project->StartPos[0] = 0;
    for (i = 0; i < project->Nobjects[NODE]-1; i++)
    {
        project->StartPos[i+1] = project->StartPos[i] + project->Node[i].degree;
        project->Node[i].degree = 0;
    }
    project->Node[project->Nobjects[NODE]-1].degree = 0;

    // --- traverse the list of links once more,
    //     adding each link's index to the proper 
    //     position in the adjacency list
    for (j = 0; j < project->Nobjects[LINK]; j++)
    {
        i = project->Link[j].node1;
        k = project->StartPos[i] + project->Node[i].degree;
        project->AdjList[k] = j;
        project->Node[i].degree++;
        if ( listType == UNDIRECTED )
        {
            i = project->Link[j].node2;
            k = project->StartPos[i] + project->Node[i].degree;
            project->AdjList[k] = j;
            project->Node[i].degree++;
        }
    }
}

//=============================================================================

void adjustAdjList(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: adjusts adjacency list for Divider nodes so that non-
//           diversion link appears before diversion link.
//
{
    int i, j, k, m;

    // --- check each node
    for (i=0; i<project->Nobjects[NODE]; i++)
    {
        // --- skip nodes that are not Dividers
        if ( project->Node[i].type != DIVIDER ) continue;
        if ( project->Node[i].degree != 2 ) continue;

        // --- switch position of outgoing links at the node if the
        //     diversion link appears first in the adjacency list
        k = project->Node[i].subIndex;
        m = project->StartPos[i];
        j = project->AdjList[m];
        if ( j == project->Divider[k].link )
        {
            project->AdjList[m] = project->AdjList[m+1];
            project->AdjList[m+1] = j;
        }
    }
}

//=============================================================================

int topoSort(Project* project,int sortedLinks[])
//
//  Input:   none
//  Output:  sortedLinks = array of sorted link indexes,
//           returns number of links successfully sorted
//  Purpose: performs a stack-based topo sort of the drainage network's links.
//
{
    int i, j, k, n;
    int i1, i2, k1, k2;

    // --- initialize a stack which contains nodes with zero in-degree
    project->First = 0;
    project->Last = -1;
    for (i = 0; i < project->Nobjects[NODE]; i++)
    {
        if ( project->InDegree[i] == 0 )
        {
            project->Last++;
            project->Stack[project->Last] = i;
        }
    }

    // --- traverse the stack, adding each node's outgoing link indexes
    //     to the SortedLinks array in the order processed
    n = 0;
    while ( project->First <= project->Last )
    {
        // --- determine range of adjacency list indexes belonging to 
        //     first node remaining on the stack
        i1 = project->Stack[project->First];
        k1 = project->StartPos[i1];
        k2 = k1 + project->Node[i1].degree;

        // --- for each outgoing link from first node on stack
        for (k = k1; k < k2; k++)
        {
            // --- add link index to current position in SortedLinks
            j = project->AdjList[k];
            sortedLinks[n] = j;
            n++;

            // --- reduce in-degree of link's downstream node
            i2 = project->Link[j].node2;
            project->InDegree[i2]--;

            // --- add downstream node to stack if its in-degree is zero
            if ( project->InDegree[i2] == 0 )
            {
                project->Last++;
                project->Stack[project->Last] = i2;
            }  
        }
        project->First++;
    }
    return n;
}

//=============================================================================

void  findCycles(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: finds all cycles in the drainage network (i.e., closed loops that
//           start and end at the same node).
//
{
    int i;

    // --- allocate arrays
    project->AdjList  = (int *) calloc(2*project->Nobjects[LINK], sizeof(int));
    project->StartPos = (int *) calloc(project->Nobjects[NODE], sizeof(int));
    project->Stack    = (int *) calloc(project->Nobjects[NODE], sizeof(int));
    project->Examined = (char *) calloc(project->Nobjects[NODE], sizeof(char));
    project->InTree   = (char *) calloc(project->Nobjects[LINK], sizeof(char));
    project->LoopLinks = (int *) calloc(project->Nobjects[LINK], sizeof(int));
    if ( project->StartPos && project->AdjList && project->Stack && project->Examined && project->InTree && project->LoopLinks )
    {
        // --- create an undirected adjacency list for the nodes
        createAdjList(project,UNDIRECTED);

        // --- set to empty the list of nodes examined and the list
        //     of links in the spanning tree
        for ( i=0; i<project->Nobjects[NODE]; i++) project->Examined[i] = 0;
        for ( i=0; i<project->Nobjects[LINK]; i++) project->InTree[i] = 0;

        // --- find a spanning tree for each unexamined node
        //     (cycles are identified as tree is constructed)
        for ( i=0; i<project->Nobjects[NODE]; i++)
        {
            if ( project->Examined[i] ) continue;
            project->Last = -1;
            findSpanningTree(project,i);
        }
    }
    FREE(project->StartPos);
    FREE(project->AdjList);
    FREE(project->Stack);
    FREE(project->Examined);
    FREE(project->InTree);
    FREE(project->LoopLinks);
}

//=============================================================================

void  findSpanningTree(Project* project, int startNode)
//
//  Input:   i = index of starting node of tree
//  Output:  none
//  Purpose: finds continuation of network's spanning tree of links.
//
{
    int nextNode, j, k, m;

    // --- examine each link connected to node i
    for ( m = project->StartPos[startNode];
          m < project->StartPos[startNode]+project->Node[startNode].degree; m++ )
    {
        // --- find which node (j) connects link k from start node
        k = project->AdjList[m];
        if ( project->Link[k].node1 == startNode ) j = project->Link[k].node2;
        else j = project->Link[k].node1;

        // --- if link k is not in the tree
        if ( project->InTree[k] == 0 )
        {
            // --- if connecting node already examined,
            //     then link k forms a loop; mark it as a chord
            //     and check if loop forms a cycle
            if ( project->Examined[j] )
            {
                project->InTree[k] = 2;
                evalLoop(project,k);
            }

            // --- otherwise mark connected node as being examined,
            //     add it to the node stack, and mark the connecting
            //     link as being in the spanning tree
            else
            {
                project->Examined[j] = 1;
                project->Last++;
                project->Stack[project->Last] = j;
                project->InTree[k] = 1;
            }
        }
    }

    // --- continue to grow the spanning tree from
    //     the last node added to the stack
    if ( project->Last >= 0 )
    {
        nextNode = project->Stack[project->Last];
        project->Last--;
        findSpanningTree(project,nextNode);
    }
}

//=============================================================================

void evalLoop(Project* project, int startLink)
//
//  Input:   startLink = index of starting link of a loop
//  Output:  none
//  Purpose: checks if loop starting with a given link forms a closed cycle.
//
{
    int i;                             // loop list index
    int i1, i2;                        // start & end node indexes
    int j;                             // index of link in loop
    int kount;                         // items per line counter
    int isCycle;                       // TRUE if loop forms a cycle

    // --- make startLink the first link in the loop
    project->LoopLinksLast = 0;
    project->LoopLinks[0] = startLink;

    // --- trace a path on the spanning tree that starts at the
    //     tail node of startLink and ends at its head node
    i1 = project->Link[startLink].node1;
    i2 = project->Link[startLink].node2;
    if ( !traceLoop(project,i1, i2, startLink) ) return;

    // --- check if all links on the path are oriented head-to-tail
    isCycle = TRUE;
    j = project->LoopLinks[0];
    i2 = project->Link[j].node2;
    for (i=1; i<=project->LoopLinksLast; i++)
    {
        j = project->LoopLinks[i];
        i1 = project->Link[j].node1;
        if ( i1 == i2 ) i2 = project->Link[j].node2;
        else
        {
            isCycle = FALSE;
            break;
        }
    }

    // --- print cycle to report file
    if ( isCycle )
    {
        kount = 0;
        for (i = 0; i <= project->LoopLinksLast; i++)
        {
            if ( kount % 5 == 0 ) fprintf(project->Frpt.file, "\n");
            kount++;
            fprintf(project->Frpt.file, "  %s", project->Link[project->LoopLinks[i]].ID);
            if ( i < project->LoopLinksLast ) fprintf(project->Frpt.file, "  -->");
        }
    }
}

//=============================================================================

int traceLoop(Project* project, int i1, int i2, int k1)
//
//  Input:   i1 = index of current node reached while tracing a loop
//           i2 = index of final node on the loop
//           k1 = index of link which extends loop to node i1
//  Output:  returns TRUE if loop can be extended through node i1
//  Purpose: tries to extend closed loop through current node.
//
{
    int j, k, m;

    // --- if current node equals final node then return with loop found
    if ( i1 == i2 ) return TRUE;

    // --- examine each link connected to current node
    for (m = project->StartPos[i1]; m < project->StartPos[i1] + project->Node[i1].degree; m++)
    {
        // --- ignore link if it comes from predecessor node or if
        //     it is not on the spanning tree
        k = project->AdjList[m];
        if ( k == k1 || project->InTree[k] != 1 ) continue;

        // --- identify other node that link is connected to
        if ( project->Link[k].node1 == i1 ) j = project->Link[k].node2;
        else                       j = project->Link[k].node1;

        // --- try to continue tracing the loop from this node;
        //     if successful, then add link to loop and return
        if ( traceLoop(project,j, i2, k) )
        {
            project->LoopLinksLast++;
            project->LoopLinks[project->LoopLinksLast] = k;
            return TRUE;
        }
    }

    // --- return false if loop cannot be continued from current node
    return FALSE;
}

//=============================================================================

void checkDummyLinks(Project* project)
//
//  Input:   none
//  Output:  none
//  Purpose: checks for nodes that have both incoming and outgoing dummy links.
//
{
    int   i, j;
    int* marked;

    // --- create an array that marks nodes
    //     (calloc initializes the array to 0)
    marked = (int *) calloc(project->Nobjects[NODE], sizeof(int));
    if ( marked == NULL )
    {
        report_writeErrorMsg(project,ERR_MEMORY, "");
        return;
    }

    // --- mark nodes that whose incoming links are all
    //     either dummy links or ideal pumps
    for ( i = 0; i < project->Nobjects[LINK]; i++ )
    {
        j = project->Link[i].node2;
        if ( project->Link[i].direction < 0 ) j = project->Link[i].node1;
        if ( (project->Link[i].type == CONDUIT && project->Link[i].xsect.type == DUMMY) ||
             (project->Link[i].type == PUMP &&
              project->Pump[project->Link[i].subIndex].type == IDEAL_PUMP) )
        {
            if ( marked[j] == 0 ) marked[j] = 1;
        }
        else marked[j] = -1;
    }

    // --- find marked nodes with outgoing dummy links or ideal pumps
    for ( i = 0; i < project->Nobjects[LINK]; i++ )
    {
        if ( (project->Link[i].type == CONDUIT && project->Link[i].xsect.type == DUMMY) ||
             (project->Link[i].type == PUMP && 
              project->Pump[project->Link[i].subIndex].type == IDEAL_PUMP) )
        {
            j = project->Link[i].node1;
            if ( marked[j] > 0 )
            {
                report_writeErrorMsg(project,ERR_DUMMY_LINK, project->Node[j].ID);
            }
        }
    }
    FREE(marked);
}

//=============================================================================
