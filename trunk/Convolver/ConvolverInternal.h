/*
 *  ConvolverInternal.h
 *  Convolvotron
 *
 *  Created by Seth Nickell on 8/11/09.
 *  Copyright 2009 Meatscience. All rights reserved.
 *
 */

#ifdef _CONVOLVER_INTERNAL_H_
#error "You should only use ConvolverInternal.h once, and only inside an implementation .cpp"
#else
#define _CONVOLVER_INTERNAL_H_
#endif

#include "DebugSettings.h"

#include "ConvolverTypes.h"

#include <vector>
#include <map>
#include <iostream>
#include <sstream>
#include <vector>
#include <utility>
#include <list>
#include <math.h>
#include <iostream>
#include <stdio.h>

// Used only for converting channel num to string
#include <sstream>
#include <boost/tuple/tuple.hpp>
#include <boost/shared_ptr.hpp>

using boost::tuple;
using boost::shared_ptr;
using std::vector;
using std::list;
using std::pair;
using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::map;

#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH