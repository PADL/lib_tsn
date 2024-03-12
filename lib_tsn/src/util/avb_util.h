// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved

#pragma once

#ifndef __XC__
int avb_itoa(int n, char *buf, int base, int fill);

int avb_itoa_fixed(int n, char *buf, int base, int fill1, int fill2, int prec);

char *avb_atoi(char *buf, int *x0);
#endif
