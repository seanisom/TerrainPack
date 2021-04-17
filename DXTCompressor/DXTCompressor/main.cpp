//--------------------------------------------------------------------------------------
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
//
//--------------------------------------------------------------------------------------

#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"

#include "DXTCompressorDLL.h" // DXT compressor DLL.
#include "StopWatch.h" // Timer.
#include "TaskMgrTBB.h" // TBB task manager.

#define ALIGN16(x) __declspec(align(16)) x

// DXT compressor type.
enum CompressorType
{
	DXT1,
	DXT5
};

// Textured vertex.
struct Vertex
{
    D3DXVECTOR3 position;
	D3DXVECTOR2 texCoord;
};

// Global variables
CDXUTDialogResourceManager gDialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg gD3DSettingsDlg; // Device settings dialog
CDXUTDialog gHUD; // manages the 3D   
CDXUTDialog gSampleUI; // dialog for sample specific controls
bool gShowHelp = false; // If true, it renders the UI control text
CDXUTTextHelper* gTxtHelper = NULL;
bool gTBB = true;
bool gSIMD = true;
CompressorType gCompType = DXT1;
double gCompTime = 0.0;
double gCompRate = 0.0;
int gBlocksPerTask = 256;
int gFrameNum = 0;
int gFrameDelay = 100;

ID3D11DepthStencilState* gDepthStencilState = NULL;
UINT gStencilReference = 0;
ID3D11InputLayout* gVertexLayout = NULL;
ID3D11Buffer* gVertexBuffer = NULL;
ID3D11Buffer* gQuadVB = NULL;
ID3D11Buffer* gIndexBuffer = NULL;
ID3D11VertexShader* gVertexShader = NULL;
ID3D11PixelShader* gRenderFramePS = NULL;
ID3D11PixelShader* gRenderTexturePS = NULL;
ID3D11SamplerState* gSamPoint = NULL;
ID3D11ShaderResourceView* gUncompressedSRV = NULL; // Shader resource view for the uncompressed texture resource.
ID3D11ShaderResourceView* gCompressedSRV = NULL; // Shader resource view for the compressed texture resource.
ID3D11ShaderResourceView* gErrorSRV = NULL; // Shader resource view for the error texture.

// UI control IDs
#define IDC_TOGGLEFULLSCREEN          1
#define IDC_TOGGLEREF                 2
#define IDC_CHANGEDEVICE              3
#define IDC_UNCOMPRESSEDTEXT          4
#define IDC_COMPRESSEDTEXT            5
#define IDC_ERRORTEXT                 6
#define IDC_TIMETEXT                  7
#define IDC_RATETEXT                  8
#define IDC_TBB                       9
#define IDC_SIMD                      10
#define IDC_COMPRESSOR                11
#define IDC_BLOCKSPERTASKTEXT         12
#define IDC_BLOCKSPERTASK             13
#define IDC_LOADTEXTURE               14
#define IDC_RECOMPRESS                15

// Forward declarations 
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  float fElapsedTime, void* pUserContext );

void InitApp();
void RenderText();

HRESULT CreateTextures(LPTSTR file);
void DestroyTextures();
HRESULT LoadTexture(LPTSTR file);
HRESULT PadTexture(ID3D11ShaderResourceView** textureSRV);
HRESULT SaveTexture(ID3D11ShaderResourceView* textureSRV, LPTSTR file);
HRESULT CompressTexture(ID3D11ShaderResourceView* uncompressedSRV, ID3D11ShaderResourceView** compressedSRV);
HRESULT ComputeError(ID3D11ShaderResourceView* uncompressedSRV, ID3D11ShaderResourceView* compressedSRV, ID3D11ShaderResourceView** errorSRV);
HRESULT RecompressTexture();

void StoreDepthStencilState();
void RestoreDepthStencilState();
HRESULT DisableDepthTest();

namespace DXTC
{
	struct DXTTaskData
	{
		BYTE* inBuf;
		BYTE* outBuf;
		INT width;
		INT height;
		INT numBlocks;
	};

	VOID CompressImageDXTTBB(BYTE* inBuf, BYTE* outBuf, INT width, INT height, BOOL compressAlpha, BOOL useSIMD);
	VOID CompressImageDXT1Task(VOID* taskData, INT taskContext, UINT taskId, UINT taskCount);
	VOID CompressImageDXT5Task(VOID* taskData, INT taskContext, UINT taskId, UINT taskCount);
	VOID CompressImageDXT1SSE2Task(VOID* taskData, INT taskContext, UINT taskId, UINT taskCount);
	VOID CompressImageDXT5SSE2Task(VOID* taskData, INT taskContext, UINT taskId, UINT taskCount);
}

int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	// Initialize the TBB task manager.
    gTaskMgr.Init();

    // Set DXUT callbacks
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    InitApp();
    DXUTInit( true, true, NULL );
    DXUTSetCursorSettings( true, true );
    DXUTCreateWindow( L"DXT Compressor" );
    DXUTCreateDevice (D3D_FEATURE_LEVEL_10_1, true, 800, 600 );
    DXUTMainLoop();

	// Shutdown the task manager.
    gTaskMgr.Shutdown();

    return DXUTGetExitCode();
}

// Initialize the app 
void InitApp()
{
	// Initialize dialogs
	gD3DSettingsDlg.Init(&gDialogResourceManager);
	gHUD.Init(&gDialogResourceManager);
	gSampleUI.Init(&gDialogResourceManager);

	gHUD.SetCallback(OnGUIEvent);
	int x = 0;
	int y = 10;
	gHUD.AddButton(IDC_TOGGLEFULLSCREEN, L"Toggle full screen", x, y, 170, 23);
	gHUD.AddButton(IDC_TOGGLEREF, L"Toggle REF (F3)", x, y += 26, 170, 23, VK_F3);
	gHUD.AddButton(IDC_CHANGEDEVICE, L"Change device (F2)", x, y += 26, 170, 23, VK_F2);

	gSampleUI.SetCallback(OnGUIEvent);
	x = 0;
	y = 0;
    gSampleUI.AddStatic(IDC_UNCOMPRESSEDTEXT, L"Uncompressed", x, y, 125, 22);
    gSampleUI.AddStatic(IDC_COMPRESSEDTEXT, L"Compressed", x, y, 125, 22);
    gSampleUI.AddStatic(IDC_ERRORTEXT, L"Error", x, y, 125, 22);
	WCHAR wstr[MAX_PATH];
	swprintf_s(wstr, MAX_PATH, L"Compression Time: %0.2f ms", gCompTime);
    gSampleUI.AddStatic(IDC_TIMETEXT, wstr, x, y, 125, 22);
	swprintf_s(wstr, MAX_PATH, L"Compression Rate: %0.2f Mp/s", gCompRate);
    gSampleUI.AddStatic(IDC_RATETEXT, wstr, x, y, 125, 22);
	gSampleUI.AddCheckBox(IDC_TBB, L"TBB", x, y, 60, 22, gTBB);
	gSampleUI.AddCheckBox(IDC_SIMD, L"SIMD", x, y, 60, 22, gSIMD);
	gSampleUI.AddComboBox(IDC_COMPRESSOR, x, y, 125, 22);
	swprintf_s(wstr, MAX_PATH, L"Blocks Per Task: %d", gBlocksPerTask);
	gSampleUI.AddStatic(IDC_BLOCKSPERTASKTEXT, wstr, x, y, 125, 22);	
	gSampleUI.AddSlider(IDC_BLOCKSPERTASK, x, y, 256, 22, 1, 512, gBlocksPerTask);
    CDXUTComboBox* comboBox = gSampleUI.GetComboBox(IDC_COMPRESSOR);
    comboBox->RemoveAllItems();
    comboBox->AddItem(L"DXT1/BC1", (void*)(INT_PTR)DXT1);
	comboBox->AddItem(L"DXT5/BC3", (void*)(INT_PTR)DXT5);
	gSampleUI.AddButton(IDC_LOADTEXTURE, L"Load Texture", x, y, 125, 22);
	gSampleUI.AddButton(IDC_RECOMPRESS, L"Recompress", x, y, 125, 22);
}

// Called right before creating a D3D11 device, allowing the app to modify the device settings as needed
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    // Uncomment this to get debug information from D3D11
    //pDeviceSettings->d3d11.CreateFlags |= D3D11_CREATE_DEVICE_DEBUG;

    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( ( DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
              pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }
    }

    return true;
}

// Handle updates to the scene.
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{

}

// Render the help and statistics text
void RenderText()
{
    UINT nBackBufferHeight = ( DXUTIsAppRenderingWithD3D9() ) ? DXUTGetD3D9BackBufferSurfaceDesc()->Height :
            DXUTGetDXGIBackBufferSurfaceDesc()->Height;

    gTxtHelper->Begin();
    gTxtHelper->SetInsertionPos( 2, 0 );
    gTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    gTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    gTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    // Draw help
    if( gShowHelp )
    {
        gTxtHelper->SetInsertionPos( 2, nBackBufferHeight - 20 * 6 );
        gTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        gTxtHelper->DrawTextLine( L"Controls:" );

        gTxtHelper->SetInsertionPos( 20, nBackBufferHeight - 20 * 5 );
        gTxtHelper->DrawTextLine( L"Hide help: F1\n"
                                    L"Quit: ESC\n" );
    }
    else
    {
        gTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        gTxtHelper->DrawTextLine( L"Press F1 for help" );
    }

    gTxtHelper->End();
}

// Handle messages to the application
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = gDialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( gD3DSettingsDlg.IsActive() )
    {
        gD3DSettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = gHUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = gSampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    return 0;
}

// Handle key presses
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( bKeyDown )
    {
        switch( nChar )
        {
            case VK_F1:
                gShowHelp = !gShowHelp; break;
        }
    }
}

// Handles the GUI events
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
		{
            DXUTToggleFullScreen();
			break;
		}
        case IDC_TOGGLEREF:
		{
            DXUTToggleREF();
			break;
		}
        case IDC_CHANGEDEVICE:
		{
            gD3DSettingsDlg.SetActive( !gD3DSettingsDlg.IsActive() );
			break;
		}
		case IDC_TIMETEXT:
		{
			WCHAR wstr[MAX_PATH];
			swprintf_s(wstr, MAX_PATH, L"Compression Time: %0.2f ms", gCompTime);
			gSampleUI.GetStatic(IDC_TIMETEXT)->SetText(wstr);
			break;
		}
		case IDC_RATETEXT:
		{
			WCHAR wstr[MAX_PATH];
			swprintf_s(wstr, MAX_PATH, L"Compression Rate: %0.2f Mp/s", gCompRate);
			gSampleUI.GetStatic(IDC_RATETEXT)->SetText(wstr);
			break;
		}
		case IDC_TBB:
		{
			gTBB = gSampleUI.GetCheckBox(IDC_TBB)->GetChecked();

			// Recompress the texture.
			RecompressTexture();

			break;
		}
		case IDC_SIMD:
		{
			gSIMD = gSampleUI.GetCheckBox(IDC_SIMD)->GetChecked();

			// Recompress the texture.
			RecompressTexture();

			break;
		}
		case IDC_COMPRESSOR:
		{
			gCompType = (CompressorType)(INT_PTR)gSampleUI.GetComboBox(IDC_COMPRESSOR)->GetSelectedData();

			// Recompress the texture.
			RecompressTexture();

			break;
		}
		case IDC_BLOCKSPERTASK:
		{
			gBlocksPerTask = gSampleUI.GetSlider(IDC_BLOCKSPERTASK)->GetValue();
			WCHAR wstr[MAX_PATH];
			swprintf_s(wstr, MAX_PATH, L"Blocks Per Task: %d", gBlocksPerTask);
			gSampleUI.GetStatic(IDC_BLOCKSPERTASKTEXT)->SetText(wstr);

			// Recompress the texture.
			RecompressTexture();

			break;
		}
		case IDC_LOADTEXTURE:
		{
			OPENFILENAME openFileName;
			WCHAR file[MAX_PATH];
			file[0] = 0;
			ZeroMemory(&openFileName, sizeof(OPENFILENAME));
			openFileName.lStructSize = sizeof(OPENFILENAME);
			openFileName.lpstrFile = file;
			openFileName.nMaxFile = MAX_PATH;
			openFileName.lpstrFilter = L"DDS\0*.dds\0\0";
			openFileName.nFilterIndex = 1;
			openFileName.lpstrInitialDir = NULL;
			openFileName.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
			if(GetOpenFileName(&openFileName))
			{
				CreateTextures(openFileName.lpstrFile);
			}
			break;
		}
		case IDC_RECOMPRESS:
		{
			// Recompress the texture.
			RecompressTexture();

			break;
		}
    }
}

// Reject any D3D11 devices that aren't acceptable by returning false
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}

// Find and compile the specified shader
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
    HRESULT hr = S_OK;

    // find the file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName ) );

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

    ID3DBlob* pErrorBlob;
    hr = D3DX11CompileFromFile( str, NULL, NULL, szEntryPoint, szShaderModel, 
        dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL );
    if( FAILED(hr) )
    {
        if( pErrorBlob != NULL )
            OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        SAFE_RELEASE( pErrorBlob );
        return hr;
    }
    SAFE_RELEASE( pErrorBlob );

    return S_OK;
}

// Create any D3D11 resources that aren't dependent on the back buffer
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN(gDialogResourceManager.OnD3D11CreateDevice(pd3dDevice, pd3dImmediateContext));
    V_RETURN(gD3DSettingsDlg.OnD3D11CreateDevice(pd3dDevice));
    gTxtHelper = new CDXUTTextHelper(pd3dDevice, pd3dImmediateContext, &gDialogResourceManager, 15);

    // Create a vertex shader.
    ID3DBlob* vertexShaderBuffer = NULL;
    V_RETURN(CompileShaderFromFile(L"DXTCompressor.hlsl", "PassThroughVS", "vs_4_0", &vertexShaderBuffer));
    V_RETURN(pd3dDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, &gVertexShader));

	// Create a pixel shader that renders the composite frame.
    ID3DBlob* pixelShaderBuffer = NULL;
    V_RETURN(CompileShaderFromFile(L"DXTCompressor.hlsl", "RenderFramePS", "ps_4_0", &pixelShaderBuffer));
    V_RETURN(pd3dDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &gRenderFramePS));

	// Create a pixel shader that renders the error texture.
    V_RETURN(CompileShaderFromFile(L"DXTCompressor.hlsl", "RenderTexturePS", "ps_4_0", &pixelShaderBuffer));
    V_RETURN(pd3dDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &gRenderTexturePS));

    // Create our vertex input layout
    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    V_RETURN(pd3dDevice->CreateInputLayout(layout, ARRAYSIZE(layout), vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &gVertexLayout));

    SAFE_RELEASE(vertexShaderBuffer);
    SAFE_RELEASE(pixelShaderBuffer);

	// Create a vertex buffer for three textured quads.
	D3DXVECTOR2 quadSize(0.32f, 0.32f);
	D3DXVECTOR2 quadOrigin(-0.66f, -0.0f);
    Vertex tripleQuadVertices[18];
	ZeroMemory(tripleQuadVertices, sizeof(tripleQuadVertices));
	for(int i = 0; i < 18; i += 6)
	{
		tripleQuadVertices[i].position = D3DXVECTOR3(quadOrigin.x - quadSize.x, quadOrigin.y + quadSize.y, 0.0f);
		tripleQuadVertices[i].texCoord = D3DXVECTOR2(0.0f, 0.0f);

		tripleQuadVertices[i + 1].position = D3DXVECTOR3(quadOrigin.x + quadSize.x, quadOrigin.y + quadSize.y, 0.0f);
		tripleQuadVertices[i + 1].texCoord = D3DXVECTOR2(1.0f, 0.0f);

		tripleQuadVertices[i + 2].position = D3DXVECTOR3(quadOrigin.x + quadSize.x, quadOrigin.y - quadSize.y, 0.0f);
		tripleQuadVertices[i + 2].texCoord = D3DXVECTOR2(1.0f, 1.0f);

		tripleQuadVertices[i + 3].position = D3DXVECTOR3(quadOrigin.x + quadSize.x, quadOrigin.y - quadSize.y, 0.0f);
		tripleQuadVertices[i + 3].texCoord = D3DXVECTOR2(1.0f, 1.0f);

		tripleQuadVertices[i + 4].position = D3DXVECTOR3(quadOrigin.x - quadSize.x, quadOrigin.y - quadSize.y, 0.0f);
		tripleQuadVertices[i + 4].texCoord = D3DXVECTOR2(0.0f, 1.0f);

		tripleQuadVertices[i + 5].position = D3DXVECTOR3(quadOrigin.x - quadSize.x, quadOrigin.y + quadSize.y, 0.0f);
		tripleQuadVertices[i + 5].texCoord = D3DXVECTOR2(0.0f, 0.0f);

		quadOrigin.x += 0.66f;
	}

    D3D11_BUFFER_DESC bufDesc;
	ZeroMemory(&bufDesc, sizeof(bufDesc));
    bufDesc.Usage = D3D11_USAGE_DEFAULT;
    bufDesc.ByteWidth = sizeof(tripleQuadVertices);
    bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufDesc.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA data;
	ZeroMemory(&data, sizeof(data));
    data.pSysMem = tripleQuadVertices;
    V_RETURN(pd3dDevice->CreateBuffer(&bufDesc, &data, &gVertexBuffer));

	// Create a vertex buffer for a single textured quad.
	quadSize = D3DXVECTOR2(1.0f, 1.0f);
	quadOrigin = D3DXVECTOR2(0.0f, 0.0f);
	Vertex singleQuadVertices[6];
	singleQuadVertices[0].position = D3DXVECTOR3(quadOrigin.x - quadSize.x, quadOrigin.y + quadSize.y, 0.0f);
	singleQuadVertices[0].texCoord = D3DXVECTOR2(0.0f, 0.0f);
	singleQuadVertices[1].position = D3DXVECTOR3(quadOrigin.x + quadSize.x, quadOrigin.y + quadSize.y, 0.0f);
	singleQuadVertices[1].texCoord = D3DXVECTOR2(1.0f, 0.0f);
	singleQuadVertices[2].position = D3DXVECTOR3(quadOrigin.x + quadSize.x, quadOrigin.y - quadSize.y, 0.0f);
	singleQuadVertices[2].texCoord = D3DXVECTOR2(1.0f, 1.0f);
	singleQuadVertices[3].position = D3DXVECTOR3(quadOrigin.x + quadSize.x, quadOrigin.y - quadSize.y, 0.0f);
	singleQuadVertices[3].texCoord = D3DXVECTOR2(1.0f, 1.0f);
	singleQuadVertices[4].position = D3DXVECTOR3(quadOrigin.x - quadSize.x, quadOrigin.y - quadSize.y, 0.0f);
	singleQuadVertices[4].texCoord = D3DXVECTOR2(0.0f, 1.0f);
	singleQuadVertices[5].position = D3DXVECTOR3(quadOrigin.x - quadSize.x, quadOrigin.y + quadSize.y, 0.0f);
	singleQuadVertices[5].texCoord = D3DXVECTOR2(0.0f, 0.0f);

	ZeroMemory(&bufDesc, sizeof(bufDesc));
    bufDesc.Usage = D3D11_USAGE_DEFAULT;
    bufDesc.ByteWidth = sizeof(singleQuadVertices);
    bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufDesc.CPUAccessFlags = 0;
	ZeroMemory(&data, sizeof(data));
    data.pSysMem = singleQuadVertices;
    V_RETURN(pd3dDevice->CreateBuffer(&bufDesc, &data, &gQuadVB));

    // Create a sampler state
    D3D11_SAMPLER_DESC SamDesc;
    SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.MipLODBias = 0.0f;
    SamDesc.MaxAnisotropy = 1;
    SamDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    SamDesc.BorderColor[0] = SamDesc.BorderColor[1] = SamDesc.BorderColor[2] = SamDesc.BorderColor[3] = 0;
    SamDesc.MinLOD = 0;
    SamDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN(pd3dDevice->CreateSamplerState(&SamDesc, &gSamPoint));

	// Load and initialize the textures.
	V_RETURN(CreateTextures(L"..\\Media\\Images\\HumanSoldier_AltOne_01.dds"));

    return S_OK;
}

// Create any D3D11 resources that depend on the back buffer
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;
    V_RETURN( gDialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( gD3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    gHUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    gHUD.SetSize( 170, 170 );

    gSampleUI.SetLocation( 0, 0 );
    gSampleUI.SetSize( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

	int oneThirdWidth = int(gSampleUI.GetWidth() / 3.0f);
	int oneThirdHeight = int(gSampleUI.GetHeight() / 3.0f);
	int x = 20;
	int y = oneThirdHeight - 20;
    gSampleUI.GetStatic(IDC_UNCOMPRESSEDTEXT)->SetLocation(x, y);
    gSampleUI.GetStatic(IDC_COMPRESSEDTEXT)->SetLocation(x += oneThirdWidth, y);
    gSampleUI.GetStatic(IDC_ERRORTEXT)->SetLocation(x += oneThirdWidth, y);
	x = gSampleUI.GetWidth() - 266;
	y = gSampleUI.GetHeight() - 190;
    gSampleUI.GetStatic(IDC_TIMETEXT)->SetLocation(x, y);
    gSampleUI.GetStatic(IDC_RATETEXT)->SetLocation(x, y += 26);
	gSampleUI.GetCheckBox(IDC_TBB)->SetLocation(x, y += 26);
	gSampleUI.GetCheckBox(IDC_SIMD)->SetLocation(x + 60, y);
	gSampleUI.GetComboBox(IDC_COMPRESSOR)->SetLocation(x + 130, y);
	gSampleUI.GetStatic(IDC_BLOCKSPERTASKTEXT)->SetLocation(x, y += 26);
	gSampleUI.GetSlider(IDC_BLOCKSPERTASK)->SetLocation(x, y += 26);
	gSampleUI.GetButton(IDC_LOADTEXTURE)->SetLocation(x, y += 26);
	gSampleUI.GetButton(IDC_RECOMPRESS)->SetLocation(x + 131, y);

    return S_OK;
}

// Render the scene using the D3D11 device
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  float fElapsedTime, void* pUserContext )
{
	// Recompress the texture gFrameDelay frames after the app has started.  This produces more accurate timing of the
	// compression algorithm.
	if(gFrameNum == gFrameDelay)
	{
		RecompressTexture();
		gFrameNum++;
	}
	else if(gFrameNum < gFrameDelay)
	{
		gFrameNum++;
	}

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( gD3DSettingsDlg.IsActive() )
    {
        gD3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Clear the render target and depth stencil
    float ClearColor[4] = { 0.0f, 106.0f / 255.0f, 1.0f, 1.0f };
    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, ClearColor );
    ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

    // Set the input layout.
    pd3dImmediateContext->IASetInputLayout( gVertexLayout );

    // Set the vertex buffer.
    UINT stride = sizeof( Vertex );
    UINT offset = 0;
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, &gVertexBuffer, &stride, &offset );

    // Set the primitive topology
    pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    // Set the shaders
    pd3dImmediateContext->VSSetShader( gVertexShader, NULL, 0 );
    pd3dImmediateContext->PSSetShader( gRenderFramePS, NULL, 0 );
    
	// Set the texture sampler.
    pd3dImmediateContext->PSSetSamplers( 0, 1, &gSamPoint );

	// Render the uncompressed texture.
	pd3dImmediateContext->PSSetShaderResources( 0, 1, &gUncompressedSRV );
    pd3dImmediateContext->Draw( 6, 0 );

	// Render the compressed texture.
	pd3dImmediateContext->PSSetShaderResources( 0, 1, &gCompressedSRV );
    pd3dImmediateContext->Draw( 6, 6 );

	// Render the error texture.
	pd3dImmediateContext->PSSetShaderResources( 0, 1, &gErrorSRV );
    pd3dImmediateContext->Draw( 6, 12 );

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    HRESULT hr;
    V(gHUD.OnRender( fElapsedTime ));
    V(gSampleUI.OnRender( fElapsedTime ));
    RenderText();
    DXUT_EndPerfEvent();
}

// Release D3D11 resources created in OnD3D11ResizedSwapChain 
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    gDialogResourceManager.OnD3D11ReleasingSwapChain();
}

// Release D3D11 resources created in OnD3D11CreateDevice 
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    gDialogResourceManager.OnD3D11DestroyDevice();
    gD3DSettingsDlg.OnD3D11DestroyDevice();
    //CDXUTDirectionWidget::StaticOnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( gTxtHelper );

    SAFE_RELEASE( gVertexLayout );
    SAFE_RELEASE( gVertexBuffer );
    SAFE_RELEASE( gQuadVB );
    SAFE_RELEASE( gIndexBuffer );
    SAFE_RELEASE( gVertexShader );
    SAFE_RELEASE( gRenderFramePS );
    SAFE_RELEASE( gRenderTexturePS );
    SAFE_RELEASE( gSamPoint );

	DestroyTextures();
}

HRESULT CreateTextures(LPTSTR file)
{
	// Destroy any previously created textures.
	DestroyTextures();

	// Load the uncompressed texture.
	HRESULT hr;
	V_RETURN(LoadTexture(file));

	// Compress the texture.
	V_RETURN(CompressTexture(gUncompressedSRV, &gCompressedSRV));

	// Compute the error in the compressed texture.
	V_RETURN(ComputeError(gUncompressedSRV, gCompressedSRV, &gErrorSRV));

	return S_OK;
}

void DestroyTextures()
{
	SAFE_RELEASE(gErrorSRV);
	SAFE_RELEASE(gCompressedSRV);
	SAFE_RELEASE(gUncompressedSRV);
}

// This functions loads a texture and prepares it for DXT compression. The DXT compressor only works on texture
// dimensions that are divisible by 4.  Textures that are not divisible by 4 are resized and padded with the edge values.
HRESULT LoadTexture(LPTSTR file)
{
	// Load the uncrompressed texture.
	// The loadInfo structure disables mipmapping by setting MipLevels to 1.
	D3DX11_IMAGE_LOAD_INFO loadInfo;
	ZeroMemory(&loadInfo, sizeof(D3DX11_IMAGE_LOAD_INFO));
	loadInfo.Width = D3DX11_DEFAULT;
	loadInfo.Height = D3DX11_DEFAULT;
	loadInfo.Depth = D3DX11_DEFAULT;
	loadInfo.FirstMipLevel = D3DX11_DEFAULT;
	loadInfo.MipLevels = 1;
	loadInfo.Usage = (D3D11_USAGE) D3DX11_DEFAULT;
	loadInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	loadInfo.CpuAccessFlags = D3DX11_DEFAULT;
	loadInfo.MiscFlags = D3DX11_DEFAULT;
	loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	loadInfo.Filter = D3DX11_FILTER_POINT | D3DX11_FILTER_SRGB;
	loadInfo.MipFilter = D3DX11_DEFAULT;
	loadInfo.pSrcInfo = NULL;
	HRESULT hr;
	V_RETURN(D3DX11CreateShaderResourceViewFromFile(DXUTGetD3D11Device(), file, &loadInfo, NULL, &gUncompressedSRV, NULL));

	PadTexture(&gUncompressedSRV);

	return S_OK;
}

HRESULT PadTexture(ID3D11ShaderResourceView** textureSRV)
{
	// Query the texture description.
	ID3D11Texture2D* tex;
	(*textureSRV)->GetResource((ID3D11Resource**)&tex);
	D3D11_TEXTURE2D_DESC texDesc;
	tex->GetDesc(&texDesc);

	// Exit if the texture dimensions are divisible by 4.
	if((texDesc.Width % 4 == 0) && (texDesc.Height % 4 == 0))
	{
		SAFE_RELEASE(tex);
		return S_OK;
	}

	// Compute the size of the padded texture.
	UINT padWidth = texDesc.Width / 4 * 4 + 4;
	UINT padHeight = texDesc.Height / 4 * 4 + 4;

	// Create a buffer for the padded texels.
	BYTE* padTexels = new BYTE[padWidth * padHeight * 4];

	// Create a staging resource for the texture.
	HRESULT hr;
	ID3D11Device* device = DXUTGetD3D11Device();
	D3D11_TEXTURE2D_DESC stgTexDesc;
	memcpy(&stgTexDesc, &texDesc, sizeof(D3D11_TEXTURE2D_DESC));
	stgTexDesc.Usage = D3D11_USAGE_STAGING;
	stgTexDesc.BindFlags = 0;
	stgTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	ID3D11Texture2D* stgTex;
	V_RETURN(device->CreateTexture2D(&stgTexDesc, NULL, &stgTex));

	// Copy the texture into the staging resource.
    ID3D11DeviceContext* deviceContext = DXUTGetD3D11DeviceContext();
	deviceContext->CopyResource(stgTex, tex);

	// Map the staging resource.
	D3D11_MAPPED_SUBRESOURCE texData;
	V_RETURN(deviceContext->Map(stgTex, D3D11CalcSubresource(0, 0, 1), D3D11_MAP_READ_WRITE, 0, &texData));

	// Copy the beginning of each row.
	BYTE* texels = (BYTE*)texData.pData;
	for(UINT row = 0; row < stgTexDesc.Height; row++)
	{
		UINT rowStart = row * texData.RowPitch;
		UINT padRowStart = row * padWidth * 4;
		memcpy(padTexels + padRowStart, texels + rowStart, stgTexDesc.Width * 4); 

		// Pad the end of each row.
		if(padWidth > stgTexDesc.Width)
		{
			BYTE* padVal = texels + rowStart + (stgTexDesc.Width - 1) * 4;
			for(UINT padCol = stgTexDesc.Width; padCol < padWidth; padCol++)
			{
				UINT padColStart = padCol * 4;
				memcpy(padTexels + padRowStart + padColStart, padVal, 4);
			}
		}
	}

	// Pad the end of each column.
	if(padHeight > stgTexDesc.Height)
	{
		UINT lastRow = (stgTexDesc.Height - 1);
		UINT lastRowStart = lastRow * padWidth * 4;
		BYTE* padVal = padTexels + lastRowStart;
		for(UINT padRow = stgTexDesc.Height; padRow < padHeight; padRow++)
		{
			UINT padRowStart = padRow * padWidth * 4;
			memcpy(padTexels + padRowStart, padVal, padWidth * 4);
		}
	}

	// Unmap the staging resources.
	deviceContext->Unmap(stgTex, D3D11CalcSubresource(0, 0, 1));

	// Create a padded texture.
	D3D11_TEXTURE2D_DESC padTexDesc;
	memcpy(&padTexDesc, &texDesc, sizeof(D3D11_TEXTURE2D_DESC));
	padTexDesc.Width = padWidth;
	padTexDesc.Height = padHeight;
	D3D11_SUBRESOURCE_DATA padTexData;
	ZeroMemory(&padTexData, sizeof(D3D11_SUBRESOURCE_DATA));
	padTexData.pSysMem = padTexels;
	padTexData.SysMemPitch = padWidth * sizeof(BYTE) * 4;
	ID3D11Texture2D* padTex;
	V_RETURN(device->CreateTexture2D(&padTexDesc, &padTexData, &padTex));

	// Delete the padded texel buffer.
	delete [] padTexels;

	// Release the shader resource view for the texture.
	SAFE_RELEASE(*textureSRV);

	// Create a shader resource view for the padded texture.
	D3D11_SHADER_RESOURCE_VIEW_DESC padTexSRVDesc;
	padTexSRVDesc.Format = padTexDesc.Format;
	padTexSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	padTexSRVDesc.Texture2D.MipLevels = padTexDesc.MipLevels;
	padTexSRVDesc.Texture2D.MostDetailedMip = padTexDesc.MipLevels - 1;
	V_RETURN(device->CreateShaderResourceView(padTex, &padTexSRVDesc, textureSRV));

	// Release resources.
	SAFE_RELEASE(padTex);
	SAFE_RELEASE(stgTex);
	SAFE_RELEASE(tex);

	return S_OK;
}

HRESULT SaveTexture(ID3D11ShaderResourceView* textureSRV, LPTSTR file)
{
	// Get the texture resource.
	ID3D11Resource* texRes;
	textureSRV->GetResource(&texRes);
	if(texRes == NULL)
	{
		return E_POINTER;
	}

	// Save the texture to a file.
	HRESULT hr;
	V_RETURN(D3DX11SaveTextureToFile(DXUTGetD3D11DeviceContext(), texRes, D3DX11_IFF_DDS, file));

	// Release the texture resources.
	SAFE_RELEASE(texRes);

	return S_OK;
}

HRESULT CompressTexture(ID3D11ShaderResourceView* uncompressedSRV, ID3D11ShaderResourceView** compressedSRV)
{
	// Query the texture description of the uncompressed texture.
	ID3D11Resource* uncompRes;
	gUncompressedSRV->GetResource(&uncompRes);
	D3D11_TEXTURE2D_DESC uncompTexDesc;
	((ID3D11Texture2D*)uncompRes)->GetDesc(&uncompTexDesc);

	// Create a 2D texture for the compressed texture.
	HRESULT hr;
	ID3D11Texture2D* compTex;
	D3D11_TEXTURE2D_DESC compTexDesc;
	memcpy(&compTexDesc, &uncompTexDesc, sizeof(D3D11_TEXTURE2D_DESC));
	compTexDesc.Format = (gCompType == DXT1) ? DXGI_FORMAT_BC1_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM_SRGB;
	ID3D11Device* device = DXUTGetD3D11Device();
	V_RETURN(device->CreateTexture2D(&compTexDesc, NULL, &compTex));

	// Create a shader resource view for the compressed texture.
	SAFE_RELEASE(*compressedSRV);
	D3D11_SHADER_RESOURCE_VIEW_DESC compSRVDesc;
	compSRVDesc.Format = compTexDesc.Format;
	compSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	compSRVDesc.Texture2D.MipLevels = compTexDesc.MipLevels;
	compSRVDesc.Texture2D.MostDetailedMip = compTexDesc.MipLevels - 1;
	V_RETURN(device->CreateShaderResourceView(compTex, &compSRVDesc, compressedSRV));

	// Create a staging resource for the compressed texture.
	compTexDesc.Usage = D3D11_USAGE_STAGING;
	compTexDesc.BindFlags = 0;
	compTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	ID3D11Texture2D* compStgTex;
	V_RETURN(device->CreateTexture2D(&compTexDesc, NULL, &compStgTex));

	// Create a staging resource for the uncompressed texture.
	uncompTexDesc.Usage = D3D11_USAGE_STAGING;
	uncompTexDesc.BindFlags = 0;
	uncompTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	ID3D11Texture2D* uncompStgTex;
	V_RETURN(device->CreateTexture2D(&uncompTexDesc, NULL, &uncompStgTex));

	// Copy the uncompressed texture into the staging resource.
    ID3D11DeviceContext* deviceContext = DXUTGetD3D11DeviceContext();
	deviceContext->CopyResource(uncompStgTex, uncompRes);

	// Map the staging resources.
	D3D11_MAPPED_SUBRESOURCE uncompData;
	V_RETURN(deviceContext->Map(uncompStgTex, D3D11CalcSubresource(0, 0, 1), D3D11_MAP_READ_WRITE, 0, &uncompData));
	D3D11_MAPPED_SUBRESOURCE compData;
	V_RETURN(deviceContext->Map(compStgTex, D3D11CalcSubresource(0, 0, 1), D3D11_MAP_READ_WRITE, 0, &compData));

	// Time the compression.
	StopWatch stopWatch;
	stopWatch.Start();

	// Compress the uncompressed texels. Note: The subresource data pointer is aligned to 16 bytes
	// for D3D_FEATURE_LEVEL_10_0 and higher.
	BOOL compressAlpha = (gCompType == DXT5) ? TRUE : FALSE;
	if(gTBB)
	{
		DXTC::CompressImageDXTTBB((BYTE*)uncompData.pData, (BYTE*)compData.pData, uncompTexDesc.Width, uncompTexDesc.Height, compressAlpha, gSIMD);
	}
	else
	{
		if(gSIMD)
		{
			if(compressAlpha)
			{
				DXTC::CompressImageDXT5SSE2((BYTE*)uncompData.pData, (BYTE*)compData.pData, uncompTexDesc.Width, uncompTexDesc.Height);
			}
			else
			{
				DXTC::CompressImageDXT1SSE2((BYTE*)uncompData.pData, (BYTE*)compData.pData, uncompTexDesc.Width, uncompTexDesc.Height);
			}
		}
		else
		{
			if(compressAlpha)
			{
				DXTC::CompressImageDXT5((BYTE*)uncompData.pData, (BYTE*)compData.pData, uncompTexDesc.Width, uncompTexDesc.Height);
			}
			else
			{
				DXTC::CompressImageDXT1((BYTE*)uncompData.pData, (BYTE*)compData.pData, uncompTexDesc.Width, uncompTexDesc.Height);
			}
		}
	}

	// Update the compression time.
	stopWatch.Stop();
	gCompTime = stopWatch.TimeInMilliseconds();
	gSampleUI.SendEvent(IDC_TIMETEXT, true, gSampleUI.GetStatic(IDC_TIMETEXT));

	// Compute the compression rate.
	INT numPixels = compTexDesc.Width * compTexDesc.Height;
	gCompRate = (double)numPixels / stopWatch.TimeInSeconds() / 1000000.0;
	gSampleUI.SendEvent(IDC_RATETEXT, true, gSampleUI.GetStatic(IDC_RATETEXT));
	stopWatch.Reset();

	// Unmap the staging resources.
	deviceContext->Unmap(compStgTex, D3D11CalcSubresource(0, 0, 1));
	deviceContext->Unmap(uncompStgTex, D3D11CalcSubresource(0, 0, 1));

	// Copy the staging resourse into the compressed texture.
	deviceContext->CopyResource(compTex, compStgTex);

	// Release resources.
	SAFE_RELEASE(uncompStgTex);
	SAFE_RELEASE(compStgTex);
	SAFE_RELEASE(compTex);
	SAFE_RELEASE(uncompRes);

	return S_OK;
}

HRESULT ComputeError(ID3D11ShaderResourceView* uncompressedSRV, ID3D11ShaderResourceView* compressedSRV, ID3D11ShaderResourceView** errorSRV)
{
	// Query the texture description of the uncompressed texture.
	ID3D11Resource* uncompRes;
	gUncompressedSRV->GetResource(&uncompRes);
	D3D11_TEXTURE2D_DESC uncompTexDesc;
	((ID3D11Texture2D*)uncompRes)->GetDesc(&uncompTexDesc);

	// Create a 2D texture for the error texture.
	HRESULT hr;
	ID3D11Texture2D* errorTex;
	D3D11_TEXTURE2D_DESC errorTexDesc;
	memcpy(&errorTexDesc, &uncompTexDesc, sizeof(D3D11_TEXTURE2D_DESC));
	errorTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	ID3D11Device* device = DXUTGetD3D11Device();
	V_RETURN(device->CreateTexture2D(&errorTexDesc, NULL, &errorTex));

	// Create a render target view for the error texture.
	D3D11_RENDER_TARGET_VIEW_DESC errorRTVDesc;
	errorRTVDesc.Format = errorTexDesc.Format;
	errorRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	errorRTVDesc.Texture2D.MipSlice = 0;
	ID3D11RenderTargetView* errorRTV;
	V_RETURN(device->CreateRenderTargetView(errorTex, &errorRTVDesc, &errorRTV));

	// Create a shader resource view for the error texture.
	D3D11_SHADER_RESOURCE_VIEW_DESC errorSRVDesc;
	errorSRVDesc.Format = errorTexDesc.Format;
	errorSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	errorSRVDesc.Texture2D.MipLevels = errorTexDesc.MipLevels;
	errorSRVDesc.Texture2D.MostDetailedMip = errorTexDesc.MipLevels - 1;
	V_RETURN(device->CreateShaderResourceView(errorTex, &errorSRVDesc, errorSRV));

	// Set the viewport to a 1:1 mapping of pixels to texels.
	ID3D11DeviceContext* deviceContext = DXUTGetD3D11DeviceContext();
	D3D11_VIEWPORT viewport;
	viewport.Width = (FLOAT)errorTexDesc.Width;
	viewport.Height = (FLOAT)errorTexDesc.Height;
	viewport.MinDepth = 0;
	viewport.MaxDepth = 1;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	deviceContext->RSSetViewports(1, &viewport);

	// Bind the render target view of the error texture.
	ID3D11RenderTargetView* RTV[1] = { errorRTV };
	deviceContext->OMSetRenderTargets(1, RTV, NULL);

	// Clear the render target.
	FLOAT color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	deviceContext->ClearRenderTargetView(errorRTV, color);

	// Set the input layout.
	deviceContext->IASetInputLayout(gVertexLayout);

	// Set vertex buffer
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &gQuadVB, &stride, &offset);

	// Set the primitive topology
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set the shaders
	deviceContext->VSSetShader(gVertexShader, NULL, 0);
	deviceContext->PSSetShader(gRenderTexturePS, NULL, 0);

	// Set the texture sampler.
	deviceContext->PSSetSamplers(0, 1, &gSamPoint);

	// Bind the textures.
	ID3D11ShaderResourceView* SRV[2] = { gCompressedSRV, gUncompressedSRV };
	deviceContext->PSSetShaderResources(0, 2, SRV);

	// Store the depth/stencil state.
	StoreDepthStencilState();

	// Disable depth testing.
	V_RETURN(DisableDepthTest());

	// Render a quad.
	deviceContext->Draw(6, 0);

	// Restore the depth/stencil state.
	RestoreDepthStencilState();

	// Reset the render target.
	RTV[0] = DXUTGetD3D11RenderTargetView();
    deviceContext->OMSetRenderTargets(1, RTV, DXUTGetD3D11DepthStencilView());

	// Reset the viewport.
	viewport.Width = (FLOAT)DXUTGetDXGIBackBufferSurfaceDesc()->Width;
	viewport.Height = (FLOAT)DXUTGetDXGIBackBufferSurfaceDesc()->Height;
	deviceContext->RSSetViewports(1, &viewport);

	// Release resources.
	SAFE_RELEASE(errorRTV);
	SAFE_RELEASE(errorTex);
	SAFE_RELEASE(uncompRes);

	return S_OK;
}

// Recompresses the already loaded texture and recomputes the error.
HRESULT RecompressTexture()
{
	// Destroy any previously created textures.
	SAFE_RELEASE(gErrorSRV);
	SAFE_RELEASE(gCompressedSRV);

	// Compress the texture.
	HRESULT hr;
	V_RETURN(CompressTexture(gUncompressedSRV, &gCompressedSRV));

	// Compute the error in the compressed texture.
	V_RETURN(ComputeError(gUncompressedSRV, gCompressedSRV, &gErrorSRV));

	return S_OK;
}

void StoreDepthStencilState()
{
	DXUTGetD3D11DeviceContext()->OMGetDepthStencilState(&gDepthStencilState, &gStencilReference);
}

void RestoreDepthStencilState()
{
	DXUTGetD3D11DeviceContext()->OMSetDepthStencilState(gDepthStencilState, gStencilReference);
}

HRESULT DisableDepthTest()
{
	D3D11_DEPTH_STENCIL_DESC depStenDesc;
	ZeroMemory(&depStenDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
	depStenDesc.DepthEnable = FALSE;
	depStenDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depStenDesc.DepthFunc = D3D11_COMPARISON_LESS;
	depStenDesc.StencilEnable = FALSE;
	depStenDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	depStenDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	depStenDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depStenDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	depStenDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depStenDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	depStenDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depStenDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	depStenDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depStenDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	ID3D11DepthStencilState* depStenState;
	HRESULT hr;
	V_RETURN(DXUTGetD3D11Device()->CreateDepthStencilState(&depStenDesc, &depStenState));

    DXUTGetD3D11DeviceContext()->OMSetDepthStencilState(depStenState, 0);

	SAFE_RELEASE(depStenState);

	return S_OK;
}

namespace DXTC
{
	VOID CompressImageDXTTBB(BYTE* inBuf, BYTE* outBuf, INT width, INT height, BOOL compressAlpha, BOOL useSIMD)
	{
		// Initialize the data.
		DXTTaskData data;
		data.inBuf = inBuf;
		data.outBuf = outBuf;
		data.width = width;
		data.height = height;
		data.numBlocks = width * height / 16;

		// Compute the task count.
		UINT taskCount = (UINT)ceil((float)data.numBlocks / gBlocksPerTask);

		// Create the task set.
		TASKSETFUNC taskFunc;
		if(useSIMD)
		{
			taskFunc = compressAlpha ? CompressImageDXT5SSE2Task : CompressImageDXT1SSE2Task;
		}
		else
		{
			taskFunc = compressAlpha ? CompressImageDXT5Task : CompressImageDXT1Task;
		}
		TASKSETHANDLE taskSet;
		gTaskMgr.CreateTaskSet(taskFunc, &data, taskCount, NULL, 0, "DXT Compression", &taskSet);
		if(taskSet == TASKSETHANDLE_INVALID)
		{
			return;
		}

		// Wait for the task set.
		gTaskMgr.WaitForSet(taskSet);

		// Release the task set.
		gTaskMgr.ReleaseHandle(taskSet);
		taskSet = TASKSETHANDLE_INVALID;
	}

	VOID CompressImageDXT1Task(VOID* taskData, INT taskContext, UINT taskId, UINT taskCount)
	{
		DXTTaskData* data = (DXTTaskData*)taskData;

		// Interate over the block set.
		for (int blockOffset = 0; blockOffset < gBlocksPerTask; ++blockOffset)
		{
			// Check for out of bounds.
			INT blockIndex = (INT)taskId * gBlocksPerTask + blockOffset;
			if(blockIndex >= data->numBlocks)
			{
				break;
			}

			// Compute the offsets into the input and output buffers.
			INT blockWidth = data->width / 4;
			INT blockRow = blockIndex / blockWidth;
			INT blockCol = blockIndex % blockWidth;
			INT inOffset = blockRow * blockWidth * 4 * 4 * 4 + blockCol * 4 * 4;
			INT outOffset = blockIndex * 8;
			BYTE* inBuf = data->inBuf + inOffset;
			BYTE* outBuf = data->outBuf + outOffset;

			// Compress the block.
			ALIGN16(BYTE block[64]);
			ALIGN16(BYTE minColor[4]);
			ALIGN16(BYTE maxColor[4]);
			ExtractBlock(inBuf, data->width, block);
			GetMinMaxColors(block, minColor, maxColor);
			EmitWord(outBuf, ColorTo565(maxColor));
			EmitWord(outBuf, ColorTo565(minColor));
			EmitColorIndices(block, outBuf, minColor, maxColor);
		}
	}

	VOID CompressImageDXT5Task(VOID* taskData, INT taskContext, UINT taskId, UINT taskCount)
	{
		DXTTaskData* data = (DXTTaskData*)taskData;

		// Interate over the block set.
		for (int blockOffset = 0; blockOffset < gBlocksPerTask; ++blockOffset)
		{
			// Check for out of bounds.
			INT blockIndex = (INT)taskId * gBlocksPerTask + blockOffset;
			if(blockIndex >= data->numBlocks)
			{
				break;
			}

			// Compute the offsets into the input and output buffers.
			INT blockWidth = data->width / 4;
			INT blockRow = blockIndex / blockWidth;
			INT blockCol = blockIndex % blockWidth;
			INT inOffset = blockRow * blockWidth * 4 * 4 * 4 + blockCol * 4 * 4;
			INT outOffset = blockIndex * 16;
			BYTE* inBuf = data->inBuf + inOffset;
			BYTE* outBuf = data->outBuf + outOffset;

			// Compress the block.
			ALIGN16(BYTE block[64]);
			ALIGN16(BYTE minColor[4]);
			ALIGN16(BYTE maxColor[4]);
			ExtractBlock(inBuf, data->width, block);
			GetMinMaxColorsWithAlpha(block, minColor, maxColor);
			EmitByte(outBuf, maxColor[3]);
			EmitByte(outBuf, minColor[3]);
			EmitAlphaIndices(block, outBuf, minColor[3], maxColor[3]);
			EmitWord(outBuf, ColorTo565(maxColor));
			EmitWord(outBuf, ColorTo565(minColor));
			EmitColorIndices(block, outBuf, minColor, maxColor);
		}
	}

	VOID CompressImageDXT1SSE2Task(VOID* taskData, INT taskContext, UINT taskId, UINT taskCount)
	{
		DXTTaskData* data = (DXTTaskData*)taskData;

		// Interate over the block set.
		for (int blockOffset = 0; blockOffset < gBlocksPerTask; ++blockOffset)
		{
			// Check for out of bounds.
			INT blockIndex = (INT)taskId * gBlocksPerTask + blockOffset;
			if(blockIndex >= data->numBlocks)
			{
				break;
			}

			// Compute the offsets into the input and output buffers.
			INT blockWidth = data->width / 4;
			INT blockRow = blockIndex / blockWidth;
			INT blockCol = blockIndex % blockWidth;
			INT inOffset = blockRow * blockWidth * 4 * 4 * 4 + blockCol * 4 * 4;
			INT outOffset = blockIndex * 8;
			BYTE* inBuf = data->inBuf + inOffset;
			BYTE* outBuf = data->outBuf + outOffset;

			// Compress the block.
			ALIGN16(BYTE block[64]);
			ALIGN16(BYTE minColor[4]);
			ALIGN16(BYTE maxColor[4]);
			ExtractBlock_SSE2(inBuf, data->width, block);
			GetMinMaxColors_SSE2(block, minColor, maxColor);
			EmitWord(outBuf, ColorTo565(maxColor));
			EmitWord(outBuf, ColorTo565(minColor));
			EmitColorIndices_SSE2(block, outBuf, minColor, maxColor);
		}
	}

	VOID CompressImageDXT5SSE2Task(VOID* taskData, INT taskContext, UINT taskId, UINT taskCount)
	{
		DXTTaskData* data = (DXTTaskData*)taskData;

		// Interate over the block set.
		for (int blockOffset = 0; blockOffset < gBlocksPerTask; ++blockOffset)
		{
			// Check for out of bounds.
			INT blockIndex = (INT)taskId * gBlocksPerTask + blockOffset;
			if(blockIndex >= data->numBlocks)
			{
				break;
			}

			// Compute the offsets into the input and output buffers.
			INT blockWidth = data->width / 4;
			INT blockRow = blockIndex / blockWidth;
			INT blockCol = blockIndex % blockWidth;
			INT inOffset = blockRow * blockWidth * 4 * 4 * 4 + blockCol * 4 * 4;
			INT outOffset = blockIndex * 16;
			BYTE* inBuf = data->inBuf + inOffset;
			BYTE* outBuf = data->outBuf + outOffset;

			// Compress the block.
			ALIGN16(BYTE block[64]);
			ALIGN16(BYTE minColor[4]);
			ALIGN16(BYTE maxColor[4]);
			ExtractBlock_SSE2(inBuf, data->width, block);
			GetMinMaxColors_SSE2(block, minColor, maxColor);
			EmitByte(outBuf, maxColor[3]);
			EmitByte(outBuf, minColor[3]);
			EmitAlphaIndices_SSE2(block, outBuf, minColor[3], maxColor[3]);
			EmitWord(outBuf, ColorTo565(maxColor));
			EmitWord(outBuf, ColorTo565(minColor));
			EmitColorIndices_SSE2(block, outBuf, minColor, maxColor);
		}
	}
}
