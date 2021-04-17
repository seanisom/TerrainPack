// Copyright 2011 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
#pragma once

#include "d3dx11effect.h"

HRESULT CompileShaderFromFile(LPCTSTR str, 
                              const D3D_SHADER_MACRO* pDefines, 
                              LPCSTR profile, 
                              ID3DBlob **ppBlobOut,
                              LPCSTR functionName = NULL);

enum EFFECT_VARIABLE_TYPE
{
    SCALAR = 0,
    VECTOR,
    MATRIX, 
    SHADER_RESOURCE,
    TECHNIQUE,
    RASTERIZER
};

BOOL GetEffectVar(ID3DX11Effect *pEffect, LPCSTR VarName, EFFECT_VARIABLE_TYPE Type, void** ppVar);

#define GET_EFFECT_VAR(pEffect, Name, Type, Var) \
    if( !GetEffectVar(pEffect, Name, Type, (void**)&Var) ) return E_FAIL;
