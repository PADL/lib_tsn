// Copyright (c) 2013-2017, XMOS Ltd, All rights reserved

#pragma once

#ifdef __XC__
#define unsafe unsafe
#else
#define unsafe
#endif

#ifdef __XC__
#define alias alias
#else
#define alias
#endif
