/*
	This code is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This code is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
	Lesser General Public License for more details.
*/

#pragma once

#include <Windows.h>

namespace DXTC
{
	// DXT compressor (scalar version).
	__declspec(dllexport) void CompressImageDXT1(const BYTE* inBuf, BYTE* outBuf, int width, int height);
	__declspec(dllexport) void CompressImageDXT5(const BYTE* inBuf, BYTE* outBuf, int width, int height, unsigned int rowPitch = 0);
	__declspec(dllexport) WORD ColorTo565(const BYTE* color);
	__declspec(dllexport) void EmitByte(BYTE*& dest, BYTE b);
	__declspec(dllexport) void EmitWord(BYTE*& dest, WORD s);
	__declspec(dllexport) void EmitDoubleWord(BYTE*& dest, DWORD i);
	__declspec(dllexport) void ExtractBlock(const BYTE* inPtr, int width, BYTE* colorBlock);
	__declspec(dllexport) void GetMinMaxColors(const BYTE* colorBlock, BYTE* minColor, BYTE* maxColor);
	__declspec(dllexport) void GetMinMaxColorsWithAlpha(const BYTE* colorBlock, BYTE* minColor, BYTE* maxColor);
	__declspec(dllexport) void EmitColorIndices(const BYTE* colorBlock, BYTE*& outBuf, const BYTE* minColor, const BYTE* maxColor);
	__declspec(dllexport) void EmitAlphaIndices(const BYTE* colorBlock,  BYTE*& outBuf, const BYTE minAlpha, const BYTE maxAlpha);

	// DXT compressor (SSE2 version).
	__declspec(dllexport) void CompressImageDXT1SSE2(const BYTE* inBuf, BYTE* outBuf, int width, int height);
	__declspec(dllexport) void CompressImageDXT5SSE2(const BYTE* inBuf, BYTE* outBuf, int width, int height, unsigned int rowPitch = 0);
	__declspec(dllexport) void ExtractBlock_SSE2(const BYTE* inPtr, int width, BYTE* colorBlock);
	__declspec(dllexport) void GetMinMaxColors_SSE2(const BYTE* colorBlock, BYTE* minColor, BYTE* maxColor);
	__declspec(dllexport) void EmitColorIndices_SSE2(const BYTE* colorBlock, BYTE*& outBuf, const BYTE* minColor, const BYTE* maxColor);
	__declspec(dllexport) void EmitAlphaIndices_SSE2(const BYTE* colorBlock, BYTE*& outBuf, const BYTE minAlpha, const BYTE maxAlpha);
}
