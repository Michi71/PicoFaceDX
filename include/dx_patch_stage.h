// include/dx_patch_stage.h
//
// Cross-core staging area for a full RDX_Patch (Core 1 writes, Core 0 reads
// after IPC_CMD_DX_PATCH_APPLY). Plain (non-volatile) global: byte-level write
// is not atomic, but FIFO push/pop provides happens-before ordering, matching
// the existing IPC_CMD_DX_PARAM non-atomic-write risk tolerance in this project.

#pragma once

#include "dx_engine/RDX_Types.h"

inline RDX_Patch g_dxStagedPatch;

inline RDX_Patch& dx_patch_stage() { return g_dxStagedPatch; }
