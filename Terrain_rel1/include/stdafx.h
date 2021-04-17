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
#define WIN32_LEAN_AND_MEAN

#pragma warning (disable: 4100 4127) // warning C4100:  unreferenced formal parameter;  warning C4127:  conditional expression is constant

#pragma warning (push)
#pragma warning (disable: 4201) // nonstandard extension used : nameless struct/union

//
// windows headers
//
#include <sdkddkver.h>
#include <Windows.h>
#include <tchar.h>
#include <atlcomcli.h> // for CComPtr support

//
// C++ headers
//

#include <cassert>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <map>
#include <set>
#include <vector>
#include <list>
#include <ctime>

//
// DirectX headers
//
#include <D3D11.h>
#include <D3DX10math.h>
#include <xnamath.h>

#include <d3dx11effect.h>

#include <DXUT.h>
#include <DXUTgui.h>
#include <DXUTsettingsDlg.h>
#include <DXUTmisc.h>
#include <DXUTcamera.h>
#include <SDKmisc.h>
#include <SDKMesh.h>

#pragma warning (pop)

#include "Errors.h"


//#define assert(x) if(!(x)) _CrtDbgBreak(); else {}

// end of file
