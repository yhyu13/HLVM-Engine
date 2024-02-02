/**
 * Copyright (c) 2024. MIT License. All rights reserved.
 */

#ifndef HLVM_MIMALLOC_OVERRIDE
#define HLVM_MIMALLOC_OVERRIDE 1
#endif

#if HLVM_MIMALLOC_OVERRIDE
// Guide to override global new and delete : https://microsoft.github.io/mimalloc/using.html
// MIMALLOC_SHOW_STATS=1 ./Engine/Source/Common/Test/Test3rdParty
#include <mimalloc-new-delete.h>
#endif