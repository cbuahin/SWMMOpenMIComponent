/*
 *  swmmOpenMINoGlobalsPriv.h
 *  swmmOpenMINoGlobals
 *
 *  Created by Tony Castronova on 1/28/15.
 *  Copyright (c) 2015 Tony Castronova. All rights reserved.
 *
 */

/* The classes below are not exported */
#pragma GCC visibility push(hidden)

class swmmOpenMINoGlobalsPriv
{
	public:
		void HelloWorldPriv(const char *);
};

#pragma GCC visibility pop
