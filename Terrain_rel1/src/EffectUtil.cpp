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
#include "stdafx.h"
#include "EffectUtil.h"
#include "SDKMisc.h"


HRESULT CompileShaderFromFile(LPCTSTR str, 
                              const D3D_SHADER_MACRO* pDefines, 
                              LPCSTR profile, 
                              ID3DBlob **ppBlobOut,
                              LPCSTR functionName)
{
    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
#endif
	HRESULT hr;
	do
	{
		CComPtr<ID3DBlob> errors;
		hr = D3DX11CompileFromFile(str, pDefines, NULL, functionName, profile, dwShaderFlags, 0, NULL, ppBlobOut, &errors, NULL);
		if( errors )
		{
			OutputDebugStringA((char*) errors->GetBufferPointer());
			if( FAILED(hr) && 
				IDRETRY != MessageBoxA(DXUTGetHWND(), (char*) errors->GetBufferPointer(), "FX Error", MB_ICONERROR|MB_ABORTRETRYIGNORE) )
			{
				break;
			}
		}
	} while( FAILED(hr) );
	return hr;
}

BOOL GetEffectVar(ID3DX11Effect *pEffect, LPCSTR VarName, EFFECT_VARIABLE_TYPE Type, void** ppVar)
{
    if(pEffect == NULL || ppVar == NULL)
        return false;

    switch(Type)
    {
        case SCALAR:
        {
            (*ppVar) = pEffect->GetVariableByName(VarName)->AsScalar();
            if( !((ID3D10EffectVariable*)(*ppVar))->IsValid() )
            {
                printf("Failed to find effect scalar variable \"%s\"\n", VarName);
                return false;
            }
            else
                return true;
        }

        case VECTOR:
        {
            (*ppVar) = pEffect->GetVariableByName(VarName)->AsVector();
            if( !((ID3D10EffectVariable*)(*ppVar))->IsValid() )
            {
                printf("Failed to find effect vector variable \"%s\"\n", VarName);
                return false;
            }
            else
                return true;

        }

        case MATRIX:
        {
            (*ppVar) = pEffect->GetVariableByName(VarName)->AsMatrix();
            if( !((ID3D10EffectVariable*)(*ppVar))->IsValid() )
            {
                printf("Failed to find effect matrix variable \"%s\"\n", VarName);
                return false;
            }
            else
                return true;

        }

        case SHADER_RESOURCE:
        {
            (*ppVar) = pEffect->GetVariableByName(VarName)->AsShaderResource();
            if( !((ID3D10EffectVariable*)(*ppVar))->IsValid() )
            {
                printf("Failed to find effect shader resource variable \"%s\"\n", VarName);
                return false;
            }
            else
                return true;

        }

        case TECHNIQUE:
        {
            (*ppVar) = pEffect->GetTechniqueByName(VarName);
            if( !((ID3D10EffectVariable*)(*ppVar))->IsValid() )
            {
                printf("Failed to find effect technique \"%s\"\n", VarName);
                return false;
            }
            else
                return true;
        }

        case RASTERIZER:
        {
            (*ppVar) = pEffect->GetVariableByName(VarName)->AsRasterizer();
            if( !((ID3D10EffectVariable*)(*ppVar))->IsValid() )
            {
                printf("Failed to find rasterizer \"%s\"\n", VarName);
                return false;
            }
            else
                return true;
        }
    }

    return false;
}
