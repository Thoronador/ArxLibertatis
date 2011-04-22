/*
===========================================================================
ARX FATALIS GPL Source Code
Copyright (C) 1999-2010 Arkane Studios SA, a ZeniMax Media company.

This file is part of the Arx Fatalis GPL Source Code ('Arx Fatalis Source Code').

Arx Fatalis Source Code is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

Arx Fatalis Source Code is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with Arx Fatalis Source Code.  If not, see
<http://www.gnu.org/licenses/>.

In addition, the Arx Fatalis Source Code is also subject to certain additional terms. You should have received a copy of these
additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Arx
Fatalis Source Code. If not, please request a copy in writing from Arkane Studios at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing Arkane Studios, c/o
ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/
//////////////////////////////////////////////////////////////////////////////////////
//   @@        @@@        @@@                @@                           @@@@@     //
//   @@@       @@@@@@     @@@     @@        @@@@                         @@@  @@@   //
//   @@@       @@@@@@@    @@@    @@@@       @@@@      @@                @@@@        //
//   @@@       @@  @@@@   @@@  @@@@@       @@@@@@     @@@               @@@         //
//  @@@@@      @@  @@@@   @@@ @@@@@        @@@@@@@    @@@            @  @@@         //
//  @@@@@      @@  @@@@  @@@@@@@@         @@@@ @@@    @@@@@         @@ @@@@@@@      //
//  @@ @@@     @@  @@@@  @@@@@@@          @@@  @@@    @@@@@@        @@ @@@@         //
// @@@ @@@    @@@ @@@@   @@@@@            @@@@@@@@@   @@@@@@@      @@@ @@@@         //
// @@@ @@@@   @@@@@@@    @@@@@@           @@@  @@@@   @@@ @@@      @@@ @@@@         //
// @@@@@@@@   @@@@@      @@@@@@@@@@      @@@    @@@   @@@  @@@    @@@  @@@@@        //
// @@@  @@@@  @@@@       @@@  @@@@@@@    @@@    @@@   @@@@  @@@  @@@@  @@@@@        //
//@@@   @@@@  @@@@@      @@@      @@@@@@ @@     @@@   @@@@   @@@@@@@    @@@@@ @@@@@ //
//@@@   @@@@@ @@@@@     @@@@        @@@  @@      @@   @@@@   @@@@@@@    @@@@@@@@@   //
//@@@    @@@@ @@@@@@@   @@@@             @@      @@   @@@@    @@@@@      @@@@@      //
//@@@    @@@@ @@@@@@@   @@@@             @@      @@   @@@@    @@@@@       @@        //
//@@@    @@@  @@@ @@@@@                          @@            @@@                  //
//            @@@ @@@                           @@             @@        STUDIOS    //
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
// EERIETexture.cpp
//////////////////////////////////////////////////////////////////////////////////////
//
// Description:
//		Texture Management Functions: Create, Restore Lost Surfaces, Invalidate
//      Surfaces, Destroy Surfaces.
//
// Updates: (date) (person) (update)
//
// Code:	Cyril Meynier
//			Sébastien Scieux	(JPEG & PNG)
//
// Copyright (c) 1999 ARKANE Studios SA. All rights reserved
//////////////////////////////////////////////////////////////////////////////////////

#include "graphics/data/Texture.h"

#include <cstdio>
#include <cassert>
#include <limits>
#include <iomanip>
#include <map>
#include <sstream>

#include <IL/il.h>

#include "core/Application.h"

#include "graphics/GraphicsUtility.h"
#include "graphics/GraphicsEnum.h"
#include "graphics/Math.h"
#include "graphics/Renderer.h"

#include "io/FilePath.h"
#include "io/PakManager.h"
#include "io/Logger.h"

#include "platform/Platform.h"
#include "platform/String.h"

using std::string;

long GLOBAL_EERIETEXTUREFLAG_LOADSCENE_RELEASE = 0;

/*-----------------------------------------------------------------------------*/
// Local list of textures
//-----------------------------------------------------------------------------
// Macros, function prototypes and static variable
//-----------------------------------------------------------------------------
 
TextureContainer	* g_ptcTextureList = NULL;
bool				bGlobalTextureStretch;

/*-----------------------------------------------------------------------------*/

class DevilLib
{
public:
    DevilLib()
    {
        ilInit();

		// Set the origin to be used when loading all images, 
		// so that any image with a different origin will be
		// flipped to have the set origin
		ilOriginFunc( IL_ORIGIN_UPPER_LEFT );
		ilEnable( IL_ORIGIN_SET );
    }

    ~DevilLib()
    {
        ilShutDown();
    }
} gDevilLib;


TextureContainer * MakeTCFromFile(const std::string& tex, long flag)
{
	long old = GLOBAL_EERIETEXTUREFLAG_LOADSCENE_RELEASE;
	GLOBAL_EERIETEXTUREFLAG_LOADSCENE_RELEASE = -1;

	TextureContainer * tc = GetTextureFile(tex, flag);

	if (tc)
	{
		if (tc->m_pddsSurface == NULL)
			tc->Restore();
	}

	GLOBAL_EERIETEXTUREFLAG_LOADSCENE_RELEASE = old;
	return tc;
}

TextureContainer * MakeTCFromFile_NoRefinement(const std::string& tex, long flag)
{
	long old = GLOBAL_EERIETEXTUREFLAG_LOADSCENE_RELEASE;
	GLOBAL_EERIETEXTUREFLAG_LOADSCENE_RELEASE = -1;

	TextureContainer * tc = GetTextureFile_NoRefinement(tex, flag);

	if (tc)
	{
		if (tc->m_pddsSurface == NULL)
			tc->Restore();
	}

	GLOBAL_EERIETEXTUREFLAG_LOADSCENE_RELEASE = old;
	return tc;
}

//-----------------------------------------------------------------------------
// Name: struct TEXTURESEARCHINFO
// Desc: Structure used to search for texture formats
//-----------------------------------------------------------------------------
struct TEXTURESEARCHINFO
{
	DWORD dwDesiredBPP;   // Input for texture format search
	bool  bUseAlpha;
	bool  bUsePalette;
	bool  bFoundGoodFormat;

	DDPIXELFORMAT * pddpf; // Output of texture format search
};

//-----------------------------------------------------------------------------
// Name: TextureSearchCallback()
// Desc: Enumeration callback routine to find a best-matching texture format.
//       The param data is the DDPIXELFORMAT of the best-so-far matching
//       texture. Note: the desired BPP is passed in the dwSize field, and the
//       default BPP is passed in the dwFlags field.
//-----------------------------------------------------------------------------
static HRESULT CALLBACK TextureSearchCallback(DDPIXELFORMAT * pddpf,
		VOID * param)
{
	if (NULL == pddpf || NULL == param)
		return DDENUMRET_OK;

	TEXTURESEARCHINFO * ptsi = (TEXTURESEARCHINFO *)param;

	// Skip any funky modes
	if (pddpf->dwFlags & (DDPF_LUMINANCE | DDPF_BUMPLUMINANCE | DDPF_BUMPDUDV))
		return DDENUMRET_OK;

	// Check for palettized formats
	if (ptsi->bUsePalette)
	{
		if (!(pddpf->dwFlags & DDPF_PALETTEINDEXED8))
			return DDENUMRET_OK;

		// Accept the first 8-bit palettized format we get
		memcpy(ptsi->pddpf, pddpf, sizeof(DDPIXELFORMAT));
		ptsi->bFoundGoodFormat = true;
		return DDENUMRET_CANCEL;
	}

	// Else, skip any paletized formats (all modes under 16bpp)
	if (pddpf->dwRGBBitCount < 16)
		return DDENUMRET_OK;

	// Skip any FourCC formats
	if (pddpf->dwFourCC != 0)
		return DDENUMRET_OK;

	// Skip any ARGB 4444 formats (which are best used for pre-authored
	// content designed speciafically for an ARGB 4444 format).
	if (pddpf->dwRGBAlphaBitMask == 0x0000f000)
		return DDENUMRET_OK;

	// Make sure current alpha format agrees with requested format type
	if ((ptsi->bUseAlpha == true) && !(pddpf->dwFlags & DDPF_ALPHAPIXELS))
		return DDENUMRET_OK;

	if ((ptsi->bUseAlpha == false) && (pddpf->dwFlags & DDPF_ALPHAPIXELS))
		return DDENUMRET_OK;

	// Check if we found a good match
	if (pddpf->dwRGBBitCount == ptsi->dwDesiredBPP)
	{
		if ((pddpf->dwRBitMask == ptsi->pddpf->dwRBitMask) &&
				(pddpf->dwGBitMask == ptsi->pddpf->dwGBitMask) &&
				(pddpf->dwBBitMask == ptsi->pddpf->dwBBitMask))
		{
			memcpy(ptsi->pddpf, pddpf, sizeof(DDPIXELFORMAT));
			ptsi->bFoundGoodFormat = true;
			return DDENUMRET_CANCEL;
		}
		else return DDENUMRET_OK;
	}

	return DDENUMRET_OK;
}

//-----------------------------------------------------------------------------
// Name: FindTexture()
// Desc: Searches the internal list of textures for a texture specified by
//       its name. Returns the structure associated with that texture.
//-----------------------------------------------------------------------------
TextureContainer * GetTextureList()
{
	return g_ptcTextureList;
}

TextureContainer * GetAnyTexture()
{
	return g_ptcTextureList;
}

TextureContainer * FindTexture( const std::string& strTextureName)
{
		TextureContainer * ptcTexture = g_ptcTextureList;

		while (ptcTexture)
		{
			if ( strTextureName.find( ptcTexture->m_texName) != std::string::npos )
				return ptcTexture;

			ptcTexture = ptcTexture->m_pNext;
		}

		return NULL;
}

long BLURTEXTURES = 0;
long NOMIPMAPS = 0;

#define NB_MIPMAP_LEVELS 5

// tex Must be of sufficient size...
long CountTextures( std::string& tex, long * memsize, long * memmip)
{
		std::string temp;
		TextureContainer * ptcTexture = g_ptcTextureList;
		long count = 0;
		*memsize = 0;
		*memmip = 0;

		tex.clear();

		while (ptcTexture)
		{
			count++;

			if (ptcTexture->m_dwFlags & D3DTEXTR_NO_MIPMAP) {
				std::stringstream ss;
				ss << std::setw(3) << count << std::setw(0) << ' ' << ptcTexture->m_strName << ' ' << ptcTexture->m_dwWidth << 'x' << ptcTexture->m_dwHeight << 'x' << ptcTexture->m_dwBPP << ' ' << ptcTexture->locks << ' ' << GetName(ptcTexture->m_texName) << "\r\n";
					temp = ss.str();
				//sprintf(temp, "%3ld %s %dx%dx%d %ld %s\r\n", count, ptcTexture->m_strName, ptcTexture->m_dwWidth, ptcTexture->m_dwHeight, ptcTexture->m_dwBPP, ptcTexture->locks, GetName(ptcTexture->m_texName));
			}
			else
			{
				std::stringstream ss;
				ss << std::setw(3) << count << ' ' << std::setw(0) << ptcTexture->m_strName << ' ' << ptcTexture->m_dwWidth << 'x' << ptcTexture->m_dwHeight << 'x' << ptcTexture->m_dwBPP << ' ' << ptcTexture->locks << " MIP " << GetName(ptcTexture->m_texName) << "\r\n";
				temp = ss.str();
				//sprintf(temp, "%3ld %s %dx%dx%ld %d MIP %s\r\n", count, ptcTexture->m_strName, ptcTexture->m_dwWidth, ptcTexture->m_dwHeight, ptcTexture->m_dwBPP, ptcTexture->locks, GetName(ptcTexture->m_texName));

				for (long k = 1; k <= NB_MIPMAP_LEVELS; k++)
				{
					*memmip += ((long)(ptcTexture->m_dwWidth * ptcTexture->m_dwHeight * ptcTexture->m_dwBPP) >> 3) / (4 * k);
				}
			}

			*memsize += (long)(ptcTexture->m_dwWidth * ptcTexture->m_dwHeight * ptcTexture->m_dwBPP) >> 3;
			tex = temp;
		}

		ptcTexture = ptcTexture->m_pNext;

		return count;
}

void ResetVertexLists(TextureContainer * ptcTexture)
{
	if(!ptcTexture)
		return;

	ptcTexture->ulNbVertexListCull = 0;
	ptcTexture->ulNbVertexListCull_TNormalTrans = 0;
	ptcTexture->ulNbVertexListCull_TAdditive = 0;
	ptcTexture->ulNbVertexListCull_TSubstractive = 0;
	ptcTexture->ulNbVertexListCull_TMultiplicative = 0;
	ptcTexture->ulNbVertexListCull_TMetal = 0;

	ptcTexture->ulMaxVertexListCull = 0;
	ptcTexture->ulMaxVertexListCull_TNormalTrans = 0;
	ptcTexture->ulMaxVertexListCull_TAdditive = 0;
	ptcTexture->ulMaxVertexListCull_TSubstractive = 0;
	ptcTexture->ulMaxVertexListCull_TMultiplicative = 0;
	ptcTexture->ulMaxVertexListCull_TMetal = 0;

	ptcTexture->vPolyBump.clear();
	ptcTexture->vPolyInterBump.clear();
	ptcTexture->vPolyInterZMap.clear();
	ptcTexture->vPolyZMap.clear();

	if (ptcTexture->pVertexListCull)
	{
		free((void *)ptcTexture->pVertexListCull);
		ptcTexture->pVertexListCull = NULL;
	}

	if (ptcTexture->pVertexListCull_TNormalTrans)
	{
		free((void *)ptcTexture->pVertexListCull_TNormalTrans);
		ptcTexture->pVertexListCull_TNormalTrans = NULL;
	}

	if (ptcTexture->pVertexListCull_TAdditive)
	{
		free((void *)ptcTexture->pVertexListCull_TAdditive);
		ptcTexture->pVertexListCull_TAdditive = NULL;
	}

	if (ptcTexture->pVertexListCull_TSubstractive)
	{
		free((void *)ptcTexture->pVertexListCull_TSubstractive);
		ptcTexture->pVertexListCull_TSubstractive = NULL;
	}

	if (ptcTexture->pVertexListCull_TMultiplicative)
	{
		free((void *)ptcTexture->pVertexListCull_TMultiplicative);
		ptcTexture->pVertexListCull_TMultiplicative = NULL;
	}

	if (ptcTexture->pVertexListCull_TMetal)
	{
		free((void *)ptcTexture->pVertexListCull_TMetal);
		ptcTexture->pVertexListCull_TMetal = NULL;
	}
}

void ReloadTexture(TextureContainer * ptcTexture)
{
	if (ptcTexture)
	{
		SAFE_RELEASE(ptcTexture->m_pddsSurface);
		ptcTexture->m_pddsSurface = NULL;
		SAFE_RELEASE(ptcTexture->m_pddsBumpMap);
		ptcTexture->m_pddsBumpMap = NULL;

		SAFE_DELETE_TAB(ptcTexture->m_pRGBAData);

		ptcTexture->m_pddsSurface = NULL;
		ptcTexture->m_pddsBumpMap = NULL;

		ptcTexture->m_pRGBAData   = NULL;

		ptcTexture->LoadImageData();

		ResetVertexLists(ptcTexture);
	}
}

void ReloadAllTextures()
{
	TextureContainer * ptcTexture = g_ptcTextureList;

	while (ptcTexture)
	{
		ReloadTexture(ptcTexture);

		ptcTexture = ptcTexture->m_pNext;
	}

	D3DTextr_RestoreAllTextures();
}


//-----------------------------------------------------------------------------
// Name: TextureContainer()
// Desc: Constructor for a texture object
//-----------------------------------------------------------------------------
TextureContainer::TextureContainer( const std::string& strName, DWORD dwStage,
								   DWORD dwFlags)
{
		m_texName = strName;
		m_strName = strName;
		MakeUpcase( m_texName );

		m_dwWidth		= 0;
		m_dwHeight		= 0;
		m_dwStage		= dwStage;
		m_dwBPP			= 0;
		m_dwFlags		= dwFlags;
		m_bHasAlpha		= 0;
		bColorKey		= false;
		bColorKey2D		= false;

		m_pddsSurface = NULL;
		m_pddsBumpMap = NULL;

		m_pRGBAData   = NULL;

		userflags = 0;
		TextureRefinement = NULL;

		// Add the texture to the head of the global texture list
		if (!(dwFlags & D3DTEXTR_NO_INSERT))
		{
			m_pNext = g_ptcTextureList;
			g_ptcTextureList = this;
		}

		delayed = NULL;
		delayed_nb = 0;
		delayed_max = 0;

		locks = 0;
		systemflags = 0;

		halodecalX = 0.f;
		halodecalY = 0.f;
		TextureHalo = NULL;

		ulMaxVertexListCull = 0;
		ulNbVertexListCull = 0;
		pVertexListCull = NULL; 

		ulMaxVertexListCull_TNormalTrans = 0; 
		ulNbVertexListCull_TNormalTrans = 0;
		pVertexListCull_TNormalTrans = NULL; 

		ulMaxVertexListCull_TAdditive = 0; 
		ulNbVertexListCull_TAdditive = 0;
		pVertexListCull_TAdditive = NULL; 

		ulMaxVertexListCull_TSubstractive = 0; 
		ulNbVertexListCull_TSubstractive = 0;
		pVertexListCull_TSubstractive = NULL; 

		ulMaxVertexListCull_TMultiplicative = 0; 
		ulNbVertexListCull_TMultiplicative = 0;
		pVertexListCull_TMultiplicative = NULL; 

		ulMaxVertexListCull_TMetal = 0;
		ulNbVertexListCull_TMetal = 0;
		pVertexListCull_TMetal = NULL; 

		tMatRoom = NULL;

		vPolyBump.clear();
		vPolyInterBump.clear();
		vPolyInterZMap.clear();
		vPolyZMap.clear();
}

bool TextureContainer_Exist(TextureContainer * tc)
{
		TextureContainer * ptcTexture = g_ptcTextureList;

		while (ptcTexture)
		{
			if (tc == ptcTexture)
				return true;

			ptcTexture = ptcTexture->m_pNext;
		}

		return false;
}


//-----------------------------------------------------------------------------
// Name: ~TextureContainer()
// Desc: Destructs the contents of the texture container
//-----------------------------------------------------------------------------
TextureContainer::~TextureContainer()
{
	if (!TextureContainer_Exist(this))
		return;

	SAFE_RELEASE(m_pddsSurface);
	SAFE_RELEASE(m_pddsBumpMap);

	SAFE_DELETE_TAB(m_pRGBAData);

	if (delayed)
	{
		free(delayed);
		delayed = NULL;
	}

	// Remove the texture container from the global list
	if (g_ptcTextureList == this)
		g_ptcTextureList = m_pNext;
	else
	{
		for (TextureContainer * ptc = g_ptcTextureList; ptc; ptc = ptc->m_pNext)
			if (ptc->m_pNext == this)
				ptc->m_pNext = m_pNext;
	}

	ResetVertexLists(this);
}

//-----------------------------------------------------------------------------
// Name: LoadImageData()
// Desc: Loads the texture map's image data
//-----------------------------------------------------------------------------
HRESULT TextureContainer::LoadImageData()
{
	std::string tempstrPathname;
	
	HRESULT hres;
	tempstrPathname = m_strName;

	SetExt(tempstrPathname, ".png");
	if ((hres = LoadFile(tempstrPathname)) != E_FAIL) return hres;

	SetExt(tempstrPathname, ".jpg");
	if ((hres = LoadFile(tempstrPathname)) != E_FAIL) return hres;

	SetExt(tempstrPathname, ".jpeg");
	if ((hres = LoadFile(tempstrPathname)) != E_FAIL) return hres;

	SetExt(tempstrPathname, ".bmp");
	if ((hres = LoadFile(tempstrPathname)) != E_FAIL) return hres;

	SetExt(tempstrPathname, ".tga");
	if ((hres = LoadFile(tempstrPathname)) != E_FAIL) return hres;

	// Can add code here to check for other file formats before failing
	LogError << m_strName << " not found";
	
	return DDERR_UNSUPPORTED;
}



HRESULT TextureContainer::LoadFile(const std::string& strPathname)
{
	size_t size = 0;
	char * dat = (char *)PAK_FileLoadMalloc(strPathname, size);
	
	if(!dat) 
	{
		return E_FAIL;
	}

	ILuint  imageName;
    ilGenImages( 1, &imageName );
    ilBindImage( imageName );    

	ILenum imageType = ilTypeFromExt(strPathname.c_str());
	ILboolean bLoaded = ilLoadL(imageType, dat, size);
	if(!bLoaded) {
		free(dat);
		return E_FAIL;
	}
	
	m_dwWidth   = ilGetInteger( IL_IMAGE_WIDTH );
	m_dwHeight  = ilGetInteger( IL_IMAGE_HEIGHT );
	m_dwBPP		= ilGetInteger( IL_IMAGE_BYTES_PER_PIXEL );
	m_bHasAlpha = m_dwBPP == 32;

	ilConvertImage( IL_RGBA, IL_UNSIGNED_BYTE );
	m_dwBPP = ilGetInteger( IL_IMAGE_BYTES_PER_PIXEL );

	m_pRGBAData = new DWORD[m_dwWidth*m_dwHeight];
	if (m_pRGBAData == NULL)
	{
		ilDeleteImages( 1, &imageName );
		free(dat);
		return E_FAIL;
	}

    memcpy( m_pRGBAData, ilGetData(), m_dwWidth*m_dwHeight*m_dwBPP );

	RemoveFakeBlack();
		
    ilDeleteImages( 1, &imageName );

	free(dat);
	return S_OK;
}

void CopySurfaceToBumpMap(LPDIRECTDRAWSURFACE7 sSurface, LPDIRECTDRAWSURFACE7 dSurface)
{
		DDSURFACEDESC2 ddesc, ddesc2;
		ddesc.dwSize  = sizeof(ddesc);
		ddesc2.dwSize = sizeof(ddesc2);
		sSurface->Lock(NULL, &ddesc , DDLOCK_WAIT, NULL);
		dSurface->Lock(NULL, &ddesc2, DDLOCK_WAIT, NULL);

		if (((32 != ddesc.ddpfPixelFormat.dwRGBBitCount)
				&&	(16 != ddesc.ddpfPixelFormat.dwRGBBitCount))
				||
				((32 != ddesc2.ddpfPixelFormat.dwRGBBitCount)
				&&	(16 != ddesc2.ddpfPixelFormat.dwRGBBitCount)))
		{
			dSurface->Unlock(NULL);
			sSurface->Unlock(NULL);
		}

		BYTE * sBytes = (BYTE *)ddesc.lpSurface;
		BYTE * dBytes = (BYTE *)ddesc2.lpSurface;

		DWORD dwRMask = ddesc.ddpfPixelFormat.dwRBitMask;
		DWORD dwGMask = ddesc.ddpfPixelFormat.dwGBitMask;
		DWORD dwBMask = ddesc.ddpfPixelFormat.dwBBitMask;
		DWORD dwAMask = ddesc.ddpfPixelFormat.dwRGBAlphaBitMask;

		DWORD dwRShiftL = 8, dwRShiftR = 0;
		DWORD dwGShiftL = 8, dwGShiftR = 0;
		DWORD dwBShiftL = 8, dwBShiftR = 0;
			DWORD dwAShiftL = 8, dwAShiftR = 0;

		DWORD dwMask;

		for (dwMask = dwRMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwRShiftR++;

		for (; dwMask ; dwMask >>= 1) dwRShiftL--;

		for (dwMask = dwGMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwGShiftR++;

		for (; dwMask ; dwMask >>= 1) dwGShiftL--;

		for (dwMask = dwBMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwBShiftR++;

		for (; dwMask ; dwMask >>= 1) dwBShiftL--;

		for (dwMask = dwAMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwAShiftR++;

		for (; dwMask ; dwMask >>= 1) dwAShiftL--;


		DWORD dwRMask2 = ddesc2.ddpfPixelFormat.dwRBitMask;
		DWORD dwGMask2 = ddesc2.ddpfPixelFormat.dwGBitMask;
		DWORD dwBMask2 = ddesc2.ddpfPixelFormat.dwBBitMask;
		DWORD dwAMask2 = ddesc2.ddpfPixelFormat.dwRGBAlphaBitMask;

		DWORD dwRShiftL2 = 8, dwRShiftR2 = 0;
		DWORD dwGShiftL2 = 8, dwGShiftR2 = 0;
		DWORD dwBShiftL2 = 8, dwBShiftR2 = 0;
		DWORD dwAShiftL2 = 8, dwAShiftR2 = 0;

		DWORD dwMask2;

		for (dwMask2 = dwRMask2 ; dwMask2 && !(dwMask2 & 0x1) ; dwMask2 >>= 1) dwRShiftR2++;

		for (; dwMask2 ; dwMask2 >>= 1) dwRShiftL2--;

		for (dwMask2 = dwGMask2 ; dwMask2 && !(dwMask2 & 0x1) ; dwMask2 >>= 1) dwGShiftR2++;

		for (; dwMask2 ; dwMask2 >>= 1) dwGShiftL2--;

		for (dwMask2 = dwBMask2 ; dwMask2 && !(dwMask2 & 0x1) ; dwMask2 >>= 1) dwBShiftR2++;

		for (; dwMask2 ; dwMask2 >>= 1) dwBShiftL2--;

		for (dwMask2 = dwAMask2 ; dwMask2 && !(dwMask2 & 0x1) ; dwMask2 >>= 1) dwAShiftR2++;

		for (; dwMask2 ; dwMask2 >>= 1) dwAShiftL2--;

		long LineOffset = ddesc.dwWidth;
		DWORD dwPixel;
		long rr, gg, bb;
		long posx, posy;
		DWORD * pDstData32;
		WORD * pDstData16;
		DWORD * pSrcData32;
		WORD * pSrcData16;
		
		DWORD dr, dg, db, da;

		for (ULONG y = 0 ; y < ddesc2.dwHeight ; y++)
		{
			pDstData32 = (DWORD *)dBytes;
			pDstData16 = (WORD *)dBytes;
			pSrcData32 = (DWORD *)sBytes;
			pSrcData16 = (WORD *)sBytes;

			for (ULONG x = 0 ; x < ddesc2.dwWidth ; x++)
			{		
				assert(y  * LineOffset <= LONG_MAX);
				posx = x;
				posy = y * LineOffset;

				// Original Pixel
				if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
					dwPixel = pSrcData32[posx+posy];
				else dwPixel = pSrcData16[posx+posy];

				rr = (BYTE)(((dwPixel & dwRMask2) >> dwRShiftR2) << dwRShiftL2);
				gg = (BYTE)(((dwPixel & dwGMask2) >> dwGShiftR2) << dwGShiftL2);
				bb = (BYTE)(((dwPixel & dwBMask2) >> dwBShiftR2) << dwBShiftL2);

				long val = ARX_CLEAN_WARN_CAST_LONG((rr + gg + bb) * ( 1.0f / 6 ));
				rr = gg = bb = val;

				dr = ((rr >> (dwRShiftL)) << dwRShiftR) & dwRMask;
				dg = ((gg >> (dwGShiftL)) << dwGShiftR) & dwGMask;
				db = ((bb >> (dwBShiftL)) << dwBShiftR) & dwBMask;
				da = ((255 >> (dwAShiftL)) << dwAShiftR) & dwAMask;

				if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
					pDstData32[x] = (DWORD)(dr + dg + db + da);
				else pDstData16[x] = (WORD)(dr + dg + db + da);
			}

			dBytes += ddesc2.lPitch;
		}

		dSurface->Unlock(NULL);
		sSurface->Unlock(NULL);
}

bool IsColorKeyInSurface(LPDIRECTDRAWSURFACE7 _pSurface)
{
		DDSURFACEDESC2 ddesc;
		ddesc.dwSize = sizeof(ddesc);
		_pSurface->Lock(NULL, &ddesc, DDLOCK_WAIT, NULL);

		if ((32 != ddesc.ddpfPixelFormat.dwRGBBitCount)
				&&	(16 != ddesc.ddpfPixelFormat.dwRGBBitCount))
		{
			_pSurface->Unlock(NULL);
		}

		BYTE * sBytes = (BYTE *)ddesc.lpSurface;

		DWORD dwRMask = ddesc.ddpfPixelFormat.dwRBitMask;
		DWORD dwGMask = ddesc.ddpfPixelFormat.dwGBitMask;
		DWORD dwBMask = ddesc.ddpfPixelFormat.dwBBitMask;
		DWORD dwAMask = ddesc.ddpfPixelFormat.dwRGBAlphaBitMask;

		DWORD dwRShiftL = 8, dwRShiftR = 0;
		DWORD dwGShiftL = 8, dwGShiftR = 0;
		DWORD dwBShiftL = 8, dwBShiftR = 0;
		DWORD dwAShiftL = 8, dwAShiftR = 0;

		DWORD dwMask;

		for (dwMask = dwRMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwRShiftR++;

		for (; dwMask ; dwMask >>= 1) dwRShiftL--;

		for (dwMask = dwGMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwGShiftR++;

		for (; dwMask ; dwMask >>= 1) dwGShiftL--;

		for (dwMask = dwBMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwBShiftR++;

		for (; dwMask ; dwMask >>= 1) dwBShiftL--;

		for (dwMask = dwAMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwAShiftR++;

		for (; dwMask ; dwMask >>= 1) dwAShiftL--;

		long LineOffset = ddesc.dwWidth;
		DWORD dwPixel;
		long rr, gg, bb;
		long posx, posy;
		DWORD * pSrcData32;
		WORD * pSrcData16;

		pSrcData32 = (DWORD *)sBytes;
		pSrcData16 = (WORD *) sBytes;
	 
	 

		for (ULONG y = 0 ; y < ddesc.dwHeight ; y++)
		{
			for (ULONG x = 0 ; x < ddesc.dwWidth ; x++)
			{
				assert(y * LineOffset <= LONG_MAX);
				posx = static_cast<long>(x);
				posy = static_cast<long>(y * LineOffset);

				// Original Pixel
				if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
					dwPixel = pSrcData32[posx+posy];
				else dwPixel = pSrcData16[posx+posy];

				rr = gg = bb = 0;

				rr = (BYTE)(((dwPixel & dwRMask) >> dwRShiftR) << dwRShiftL);
				gg = (BYTE)(((dwPixel & dwGMask) >> dwGShiftR) << dwGShiftL);      
				bb = (BYTE)(((dwPixel & dwBMask) >> dwBShiftR) << dwBShiftL);      

				if ((rr == 0) &&
						(gg == 0) &&
						(bb == 0))
				{
					_pSurface->Unlock(NULL);
					return true;
				}
			}
		}

		_pSurface->Unlock(NULL);
		return false;
}


void StretchCopySurfaceToSurface(LPDIRECTDRAWSURFACE7 sSurface, LPDIRECTDRAWSURFACE7 dSurface)
{
		DDSURFACEDESC2 ddesc, ddesc2;
		ddesc.dwSize  = sizeof(ddesc);
		ddesc2.dwSize = sizeof(ddesc2);
		sSurface->Lock(NULL, &ddesc , DDLOCK_WAIT, NULL);
		dSurface->Lock(NULL, &ddesc2, DDLOCK_WAIT, NULL);

		if (((32 != ddesc.ddpfPixelFormat.dwRGBBitCount)
				&& (16 != ddesc.ddpfPixelFormat.dwRGBBitCount))
				||
				((32 != ddesc2.ddpfPixelFormat.dwRGBBitCount)
				&& (16 != ddesc2.ddpfPixelFormat.dwRGBBitCount)))
		{
			dSurface->Unlock(NULL);
			sSurface->Unlock(NULL);
			return;
		}

		BYTE * sBytes = (BYTE *)ddesc.lpSurface;
		BYTE * dBytes = (BYTE *)ddesc2.lpSurface;

		float rx = (float)ddesc.dwWidth / (float)ddesc2.dwWidth ;
		float ry = (float)ddesc.dwHeight / (float)ddesc2.dwHeight ;

		DWORD dwRMask = ddesc.ddpfPixelFormat.dwRBitMask;
		DWORD dwGMask = ddesc.ddpfPixelFormat.dwGBitMask;
		DWORD dwBMask = ddesc.ddpfPixelFormat.dwBBitMask;
		DWORD dwAMask = ddesc.ddpfPixelFormat.dwRGBAlphaBitMask;

		DWORD dwRShiftL = 8, dwRShiftR = 0;
		DWORD dwGShiftL = 8, dwGShiftR = 0;
		DWORD dwBShiftL = 8, dwBShiftR = 0;
		DWORD dwAShiftL = 8, dwAShiftR = 0;

		DWORD dwMask;

		for (dwMask = dwRMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwRShiftR++;

		for (; dwMask ; dwMask >>= 1) dwRShiftL--;

		for (dwMask = dwGMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwGShiftR++;

		for (; dwMask ; dwMask >>= 1) dwGShiftL--;

		for (dwMask = dwBMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwBShiftR++;

		for (; dwMask ; dwMask >>= 1) dwBShiftL--;

		for (dwMask = dwAMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwAShiftR++;

		for (; dwMask ; dwMask >>= 1) dwAShiftL--;


		DWORD dwRMask2 = ddesc2.ddpfPixelFormat.dwRBitMask;
		DWORD dwGMask2 = ddesc2.ddpfPixelFormat.dwGBitMask;
		DWORD dwBMask2 = ddesc2.ddpfPixelFormat.dwBBitMask;
		DWORD dwAMask2 = ddesc2.ddpfPixelFormat.dwRGBAlphaBitMask;

		DWORD dwRShiftL2 = 8, dwRShiftR2 = 0;
		DWORD dwGShiftL2 = 8, dwGShiftR2 = 0;
		DWORD dwBShiftL2 = 8, dwBShiftR2 = 0;
		DWORD dwAShiftL2 = 8, dwAShiftR2 = 0;

		DWORD dwMask2;

		for (dwMask2 = dwRMask2 ; dwMask2 && !(dwMask2 & 0x1) ; dwMask2 >>= 1) dwRShiftR2++;

		for (; dwMask2 ; dwMask2 >>= 1) dwRShiftL2--;

		for (dwMask2 = dwGMask2 ; dwMask2 && !(dwMask2 & 0x1) ; dwMask2 >>= 1) dwGShiftR2++;

		for (; dwMask2 ; dwMask2 >>= 1) dwGShiftL2--;

		for (dwMask2 = dwBMask2 ; dwMask2 && !(dwMask2 & 0x1) ; dwMask2 >>= 1) dwBShiftR2++;

		for (; dwMask2 ; dwMask2 >>= 1) dwBShiftL2--;

		for (dwMask2 = dwAMask2 ; dwMask2 && !(dwMask2 & 0x1) ; dwMask2 >>= 1) dwAShiftR2++;

		for (; dwMask2 ; dwMask2 >>= 1) dwAShiftL2--;

		long LineOffset = ddesc.dwWidth;
		DWORD dwPixel[10];
		BYTE r[10];
		BYTE g[10];
		BYTE b[10];
		BYTE a[10];
		long rr, gg, bb;
		long posx, posy, offset;
		DWORD * pDstData32;
		WORD * pDstData16;
		DWORD * pSrcData32;
		WORD * pSrcData16;

		DWORD dr, dg, db, da;

		for (ULONG y = 0 ; y < ddesc2.dwHeight ; y++)
		{
			pDstData32 = (DWORD *)dBytes;
			pDstData16 = (WORD *)dBytes;
			pSrcData32 = (DWORD *)sBytes;
			pSrcData16 = (WORD *)sBytes;

			for (ULONG x = 0 ; x < ddesc2.dwWidth ; x++)
			{
				posx = x * rx;
				posy = y * ry * LineOffset;

				// Pixel Up Left
				if ((y <= 0.f) || (x <= 0.f)) offset = posx + posy;
				else offset = posx + posy - LineOffset - 1;

				if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
					dwPixel[6] = pSrcData32[offset];
				else dwPixel[6] = pSrcData16[offset];

				// Pixel Up
				if (y <= 0) offset = posx + posy;
				else offset = posx + posy - LineOffset;

				if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
					dwPixel[5] = pSrcData32[offset];
				else dwPixel[5] = pSrcData16[offset];

				// Pixel Up Right
				if ((y <= 0.f) || (x >= ddesc2.dwWidth - 1)) offset = posx + posy;
				else offset = posx + posy - LineOffset + 1;

				if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
					dwPixel[8] = pSrcData32[offset];
				else dwPixel[8] = pSrcData16[offset];

				// Pixel Left
				if (x <= 0) offset = posx + posy;
				else offset = posx + posy - 1;

				if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
					dwPixel[4] = pSrcData32[offset];
				else dwPixel[4] = pSrcData16[offset];

				// Original Pixel
				if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
					dwPixel[0] = pSrcData32[posx+posy];
				else dwPixel[0] = pSrcData16[posx+posy];

				// Pixel Right
				if (x >= ddesc2.dwWidth - 1) offset = posx + posy;
				else offset = posx + posy + 1;

				if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
					dwPixel[1] = pSrcData32[offset];
				else dwPixel[1] = pSrcData16[offset];

				// Pixel Down Left
				if ((x <= 0) || (y >= ddesc2.dwHeight - 1)) offset = posx + posy;
				else offset = posx + posy + LineOffset - 1;

				if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
					dwPixel[7] = pSrcData32[offset];
				else dwPixel[7] = pSrcData16[offset];

				// Pixel Down
				if (y >= ddesc2.dwHeight - 1) offset = posx + posy;
				else offset = posx + posy + LineOffset;

				if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
					dwPixel[2] = pSrcData32[offset];
				else dwPixel[2] = pSrcData16[offset];

				// Pixel Down Right
				if ((x >= ddesc2.dwWidth - 1) || (y >= ddesc2.dwHeight - 1)) offset = posx + posy;
				else offset = posx + posy + LineOffset + 1;

				if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
					dwPixel[3] = pSrcData32[offset];
				else dwPixel[3] = pSrcData16[offset];

				rr = gg = bb = 0;
				long aa = 0;
	 
				for (long n = 0 ; n < 9 ; n++)
				{
					r[n] = (BYTE)(((dwPixel[n] & dwRMask2) >> dwRShiftR2) << dwRShiftL2);
					g[n] = (BYTE)(((dwPixel[n] & dwGMask2) >> dwGShiftR2) << dwGShiftL2);      
					b[n] = (BYTE)(((dwPixel[n] & dwBMask2) >> dwBShiftR2) << dwBShiftL2);     
					a[n] = (BYTE)(((dwPixel[n] & dwAMask2) >> dwAShiftR2) << dwAShiftL2);

					rr += r[n];
					gg += g[n];
					bb += b[n];
					aa += a[n];
				}

				rr += r[4] + r[5] + r[0] * 3 + r[2] + r[1];
				gg += g[4] + g[5] + g[0] * 3 + g[2] + g[1];
				bb += b[4] + b[5] + b[0] * 3 + b[2] + b[1];
				aa += a[4] + a[5] + a[0] * 3 + a[2] + a[1];
				rr >>= 4;
				gg >>= 4;
				bb >>= 4;
				aa >>= 4;

				dr = ((rr >> (dwRShiftL)) << dwRShiftR) & dwRMask;
				dg = ((gg >> (dwGShiftL)) << dwGShiftR) & dwGMask;
				db = ((bb >> (dwBShiftL)) << dwBShiftR) & dwBMask;
				da = ((255 >> (dwAShiftL)) << dwAShiftR) & dwAMask;

				if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
					pDstData32[x] = (DWORD)(dr + dg + db + da);
				else pDstData16[x] = (WORD)(dr + dg + db + da);
			}

			dBytes += ddesc2.lPitch;
		}

		dSurface->Unlock(NULL);
		sSurface->Unlock(NULL);
}

long SPECIAL_PNUX = 0; 

void PnuxSurface(LPDIRECTDRAWSURFACE7 sSurface)
{
		DDSURFACEDESC2 ddesc;
		ddesc.dwSize = sizeof(ddesc);
		sSurface->Lock(NULL, &ddesc, DDLOCK_WAIT, NULL);

		BYTE * sBytes = (BYTE *)ddesc.lpSurface;

		DWORD dwRMask = ddesc.ddpfPixelFormat.dwRBitMask;
		DWORD dwGMask = ddesc.ddpfPixelFormat.dwGBitMask;
		DWORD dwBMask = ddesc.ddpfPixelFormat.dwBBitMask;
		DWORD dwAMask = ddesc.ddpfPixelFormat.dwRGBAlphaBitMask;

		DWORD dwRShiftL = 8, dwRShiftR = 0;
		DWORD dwGShiftL = 8, dwGShiftR = 0;
		DWORD dwBShiftL = 8, dwBShiftR = 0;
		DWORD dwAShiftL = 8, dwAShiftR = 0;

		DWORD dwMask;

		for (dwMask = dwRMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwRShiftR++;

		for (; dwMask ; dwMask >>= 1) dwRShiftL--;

		for (dwMask = dwGMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwGShiftR++;

		for (; dwMask ; dwMask >>= 1) dwGShiftL--;

		for (dwMask = dwBMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwBShiftR++;

		for (; dwMask ; dwMask >>= 1) dwBShiftL--;

		for (dwMask = dwAMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwAShiftR++;

		for (; dwMask ; dwMask >>= 1) dwAShiftL--;


		long LineOffset = ddesc.dwWidth;
		DWORD dwPixel;
		BYTE r;
		BYTE g;
		BYTE b;
		long posx, posy, offset;
		long aa, rr, gg, bb;
		DWORD * pSrcData32;
		WORD * pSrcData16;

		DWORD dr, dg, db, da;
		pSrcData32 = (DWORD *)sBytes;
		pSrcData16 = (WORD *)sBytes;

		for (ULONG y = 0 ; y < ddesc.dwHeight ; y++)
		{

			for (ULONG x = 0 ; x < ddesc.dwWidth ; x++)
			{
				posx = x;
				posy = y * LineOffset;

				// Original Pixel
				offset = posx + posy;

				if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
					dwPixel = pSrcData32[offset];
				else dwPixel = pSrcData16[offset];

				r = (BYTE)(((dwPixel & dwRMask) >> dwRShiftR) << dwRShiftL);
				g = (BYTE)(((dwPixel & dwGMask) >> dwGShiftR) << dwGShiftL);       
				b = (BYTE)(((dwPixel & dwBMask) >> dwBShiftR) << dwBShiftL);      
				aa = 255;
				float fr, fg, fb;

				rr = r;
				gg = g;
				bb = b;

				fr = ARX_CLEAN_WARN_CAST_FLOAT(rr);
				fg = ARX_CLEAN_WARN_CAST_FLOAT(gg);
				fb = ARX_CLEAN_WARN_CAST_FLOAT(bb);

				if (SPECIAL_PNUX == 3)
				{
					float power = (fr + fg + fb) * ( 1.0f / 3 ) * 1.2f;
					fr = power;
					fg = power;
					fb = power;
				}
				else if (SPECIAL_PNUX == 2)
				{
					float power = (fr + fg + fb) * ( 1.0f / 3 );

					if (power > fr * 0.75f)
					{
						fg = fr * 0.6f;
						fb = fr * 0.5f;
					}
					else
					{
						fg = fr * 0.3f;
						fb = fr * 0.1f;
					}

					fr *= 1.3f;
					fg *= 1.5f;
					fb *= 1.5f;

					if (power > 200.f)
					{
						fr += (power - 200.f) * ( 1.0f / 5 );
						fg += (power - 200.f) * ( 1.0f / 4 );
						fb += (power - 200.f) * ( 1.0f / 3 );
					}
				}
				else
				{
					float power = (fr + fg + fb) * 0.6f;

					if (power > 190.f)
					{
						fr = power;
						fg = power * 0.3f;
						fb = power * 0.7f;
					}
					else
					{
						fr = power * 0.7f;
						fg = power * 0.5f;
						fb = power;
					}

				}

				if (fr > 255.f) fr = 255.f;

				if (fg > 255.f) fg = 255.f;

				if (fb > 255.f) fb = 255.f;

				rr = fr;
				gg = fg;
				bb = fb;
				dr = ((rr >> (dwRShiftL)) << dwRShiftR) & dwRMask;
				dg = ((gg >> (dwGShiftL)) << dwGShiftR) & dwGMask;
				db = ((bb >> (dwBShiftL)) << dwBShiftR) & dwBMask;
				da = ((aa >> (dwAShiftL)) << dwAShiftR) & dwAMask;
				offset = posx + posy;

				if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
					pSrcData32[offset] = (DWORD)(dr + dg + db + da);
				else pSrcData16[offset] = (WORD)(dr + dg + db + da);

			}
		}

		sSurface->Unlock(NULL);
}

void SmoothSurface(LPDIRECTDRAWSURFACE7 sSurface)
{
	DDSURFACEDESC2 ddesc;
	ddesc.dwSize = sizeof(ddesc);
	sSurface->Lock(NULL, &ddesc, DDLOCK_WAIT, NULL);

	BYTE * sBytes = (BYTE *)ddesc.lpSurface;

	DWORD dwRMask = ddesc.ddpfPixelFormat.dwRBitMask;
	DWORD dwGMask = ddesc.ddpfPixelFormat.dwGBitMask;
	DWORD dwBMask = ddesc.ddpfPixelFormat.dwBBitMask;
	DWORD dwAMask = ddesc.ddpfPixelFormat.dwRGBAlphaBitMask;

	DWORD dwRShiftL = 8, dwRShiftR = 0;
	DWORD dwGShiftL = 8, dwGShiftR = 0;
	DWORD dwBShiftL = 8, dwBShiftR = 0;
	DWORD dwAShiftL = 8, dwAShiftR = 0;

	DWORD dwMask;

	for (dwMask = dwRMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwRShiftR++;

	for (; dwMask ; dwMask >>= 1) dwRShiftL--;

	for (dwMask = dwGMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwGShiftR++;

	for (; dwMask ; dwMask >>= 1) dwGShiftL--;

	for (dwMask = dwBMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwBShiftR++;

	for (; dwMask ; dwMask >>= 1) dwBShiftL--;

	for (dwMask = dwAMask; dwMask && !(dwMask & 0x1); dwMask >>= 1) dwAShiftR++;

	for (; dwMask; dwMask >>= 1) dwAShiftL--;


	long LineOffset = ddesc.dwWidth;
	DWORD dwPixel[10];
	BYTE r[10];
	BYTE g[10];
	BYTE b[10];
	long posx, posy, offset;
	long rr, gg, bb;
	DWORD * pSrcData32;
	WORD * pSrcData16;

	DWORD dr, dg, db, da;
	pSrcData32 = (DWORD *)sBytes;
	pSrcData16 = (WORD *)sBytes;

	for (ULONG y = 0 ; y < ddesc.dwHeight ; y++)
	{

		for (ULONG x = 0 ; x < ddesc.dwWidth ; x++)
		{
			posx = x;
			posy = y * LineOffset;

			// Pixel Up Left
			if ((y <= 0.f) || (x <= 0.f)) offset = posx + posy;
			else offset = posx + posy - LineOffset - 1;

			if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
				dwPixel[6] = pSrcData32[offset];
			else dwPixel[6] = pSrcData16[offset];

			// Pixel Up
			if (y <= 0) offset = posx + posy;
			else offset = posx + posy - LineOffset;

			if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
				dwPixel[5] = pSrcData32[offset];
			else dwPixel[5] = pSrcData16[offset];

			// Pixel Up Right
			if ((y <= 0.f) || (x >= ddesc.dwWidth - 1)) offset = posx + posy;
			else offset = posx + posy - LineOffset + 1;

			if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
				dwPixel[8] = pSrcData32[offset];
			else dwPixel[8] = pSrcData16[offset];

			// Pixel Left
			if (x <= 0) offset = posx + posy;
			else offset = posx + posy - 1;

			if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
				dwPixel[4] = pSrcData32[offset];
			else dwPixel[4] = pSrcData16[offset];

			// Original Pixel
			offset = posx + posy;

			if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
				dwPixel[0] = pSrcData32[offset];
			else dwPixel[0] = pSrcData16[offset];

			// Pixel Right
			if (x >= ddesc.dwWidth - 1) offset = posx + posy;
			else offset = posx + posy + 1;

			if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
				dwPixel[1] = pSrcData32[offset];
			else dwPixel[1] = pSrcData16[offset];

			// Pixel Down Left
			if ((x <= 0) || (y >= ddesc.dwHeight - 1)) offset = posx + posy;
			else offset = posx + posy + LineOffset - 1;

			if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
				dwPixel[7] = pSrcData32[offset];
			else dwPixel[7] = pSrcData16[offset];

			// Pixel Down
			if (y >= ddesc.dwHeight - 1) offset = posx + posy;
			else offset = posx + posy + LineOffset;

			if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
				dwPixel[2] = pSrcData32[offset];
			else dwPixel[2] = pSrcData16[offset];

			// Pixel Down Right
			if ((x >= ddesc.dwWidth - 1) || (y >= ddesc.dwHeight - 1)) offset = posx + posy;
			else offset = posx + posy + LineOffset + 1;

			if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
				dwPixel[3] = pSrcData32[offset];
			else dwPixel[3] = pSrcData16[offset];

			rr = gg = bb = 0;
			long nbincrust = 0;

			for (long n = 0 ; n < 9 ; n++)
			{
				r[n] = (BYTE)(((dwPixel[n] & dwRMask) >> dwRShiftR) << dwRShiftL);
				g[n] = (BYTE)(((dwPixel[n] & dwGMask) >> dwGShiftR) << dwGShiftL);      
				b[n] = (BYTE)(((dwPixel[n] & dwBMask) >> dwBShiftR) << dwBShiftL);      

				rr += r[n];
				gg += g[n];
				bb += b[n];

				if ((r[n] == 0) && (g[n] == 0) && (b[n] == 0))
					nbincrust++;
			}

			rr += r[0] * 7;
			gg += g[0] * 7;
			bb += b[0] * 7;

			rr >>= 4;
			gg >>= 4;
			bb >>= 4;

			long aa = 255 - nbincrust * 28;

			if (aa < 30)
				aa = 0;

			dr = ((rr >> (dwRShiftL)) << dwRShiftR) & dwRMask;
			dg = ((gg >> (dwGShiftL)) << dwGShiftR) & dwGMask;
			db = ((bb >> (dwBShiftL)) << dwBShiftR) & dwBMask;
			da = ((aa >> (dwAShiftL)) << dwAShiftR) & dwAMask;
			offset = posx + posy;

			if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
				pSrcData32[offset] = (DWORD)(dr + dg + db + da);
			else
				pSrcData16[offset] = (WORD)(dr + dg + db + da);

		}
	}

	sSurface->Unlock(NULL);
}

void SpecialBorderSurface(TextureContainer * tc, unsigned long x0, unsigned long y0)
{
	if (!tc->m_pddsSurface)
		return;

	LPDIRECTDRAWSURFACE7 sSurface = tc->m_pddsSurface;
	tc->m_dwFlags |= D3DTEXTR_FAKE_BORDER;
	DDSURFACEDESC2 ddesc;
	memset(&ddesc, 0, sizeof(ddesc));
	ddesc.dwSize = sizeof(ddesc);

	if (FAILED(sSurface->Lock(NULL, &ddesc, DDLOCK_WAIT, NULL))) return;

	BYTE * sBytes = (BYTE *)ddesc.lpSurface;

	DWORD dwRMask = ddesc.ddpfPixelFormat.dwRBitMask;
	DWORD dwGMask = ddesc.ddpfPixelFormat.dwGBitMask;
	DWORD dwBMask = ddesc.ddpfPixelFormat.dwBBitMask;
	DWORD dwAMask = ddesc.ddpfPixelFormat.dwRGBAlphaBitMask;

	DWORD dwRShiftL = 8, dwRShiftR = 0;
	DWORD dwGShiftL = 8, dwGShiftR = 0;
	DWORD dwBShiftL = 8, dwBShiftR = 0;
	DWORD dwAShiftL = 8, dwAShiftR = 0;

	DWORD dwMask;

	for (dwMask = dwRMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwRShiftR++;

	for (; dwMask ; dwMask >>= 1) dwRShiftL--;

	for (dwMask = dwGMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwGShiftR++;

	for (; dwMask ; dwMask >>= 1) dwGShiftL--;

	for (dwMask = dwBMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwBShiftR++;

	for (; dwMask ; dwMask >>= 1) dwBShiftL--;

	for (dwMask = dwAMask ; dwMask && !(dwMask & 0x1) ; dwMask >>= 1) dwAShiftR++;

	for (; dwMask ; dwMask >>= 1) dwAShiftL--;


	long LineOffset = ddesc.dwWidth;
	DWORD dwPixel;
	long posx, posy, offset;
	DWORD * pSrcData32;
	WORD * pSrcData16;

	pSrcData32 = (DWORD *)sBytes;
	pSrcData16 = (WORD *)sBytes;

	if (ddesc.dwHeight > y0)
	{
		for (ULONG x = 0 ; x < ddesc.dwWidth ; x++)
		{
			posx = x;
			posy = y0 * LineOffset;

			// Pixel Up
			if (y0 <= 0) offset = posx + posy;
			else offset = posx + posy - LineOffset;

			if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
				dwPixel = (DWORD)pSrcData32[offset];
			else dwPixel = (WORD)pSrcData16[offset];

			for (ULONG y = y0 ; y < ddesc.dwHeight ; y++)
			{
				posy = y * LineOffset;
				offset = posx + posy;

				if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
					pSrcData32[offset] = (DWORD)(dwPixel);
				else pSrcData16[offset] = (WORD)(dwPixel);
			}
		}
	}

	if (ddesc.dwWidth > x0)
	{
		for (ULONG y = 0 ; y < ddesc.dwHeight ; y++)
		{
			posx = x0 - 1;
			posy = y * LineOffset;

			// Pixel Up
			offset = posx + posy;

			if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
				dwPixel = pSrcData32[offset];
			else dwPixel = pSrcData16[offset];

			for (ULONG x = x0 ; x < ddesc.dwWidth ; x++)
			{
				posx = x;
				offset = posx + posy;

				if (32 == ddesc.ddpfPixelFormat.dwRGBBitCount)
					pSrcData32[offset] = (DWORD)(dwPixel);
				else pSrcData16[offset] = (WORD)(dwPixel);
			}
		}
	}

	sSurface->Unlock(NULL);
}

bool EERIE_USES_BUMP_MAP = false;

void EERIE_ActivateBump(void)
{
	EERIE_USES_BUMP_MAP = true;
}

void EERIE_DesactivateBump(void)
{
	EERIE_USES_BUMP_MAP = false;
}

HRESULT TextureContainer::Use()
{
	this->locks++;
	return S_OK;
}

extern long GLOBAL_FORCE_MINI_TEXTURE;
extern long GORE_MODE;
//-----------------------------------------------------------------------------
// Name: Restore()
// Desc: Rebuilds the texture surface using the new device.
//-----------------------------------------------------------------------------
HRESULT TextureContainer::Restore()
{
	bGlobalTextureStretch = true;
	bool bActivateBump = true;
	std::string tTxt = m_strName;
	MakeUpcase(tTxt);

	//TEXTURE_STRETCH
	if ( ( tTxt.find("INTERFACE") != std::string::npos ) ||
	     ( tTxt.find("LEVELS") != std::string::npos ) ||
	     ( tTxt.find("ITEMS") != std::string::npos ) ||
	     ( tTxt.find("REFINEMENT") != std::string::npos ) ||
	     ( tTxt.find("LOGO.BMP") != std::string::npos ) )
	{
		bGlobalTextureStretch = false;
		this->m_dwFlags |= D3DTEXTR_NO_MIPMAP;
	}

	//END_TEXTURE_STRETCH

	//BUMP
	if ( ( tTxt.find("ITEMS") != std::string::npos ) ||
	     ( tTxt.find("INTERFACE") != std::string::npos )||
	     ( tTxt.find("REFINEMENT") != std::string::npos ) ||
	     ( tTxt.find("LOGO.BMP") != std::string::npos ) )
	{
		bActivateBump = false;
	}

	//END_BUMP

	LPDIRECTDRAWSURFACE7 t_m_pddsSurface;
	// Release any previously created objects

	SAFE_RELEASE(m_pddsSurface);
	SAFE_RELEASE(m_pddsBumpMap);

	// Get the device caps
	D3DDEVICEDESC7 ddDesc;

	if (FAILED(GDevice->GetCaps(&ddDesc)))
		return E_FAIL;

	// Setup the new surface desc
	DDSURFACEDESC2 ddsd;
	D3DUtil_InitSurfaceDesc(ddsd);

	if (((this->m_dwFlags & D3DTEXTR_NO_MIPMAP)) || NOMIPMAPS || !bGlobalTextureStretch)
	{
		ddsd.dwFlags         = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH |
							   DDSD_PIXELFORMAT | DDSD_TEXTURESTAGE | DDSD_CKSRCBLT | D3DTEXTR_TRANSPARENTBLACK;
		ddsd.ddsCaps.dwCaps  = DDSCAPS_TEXTURE ;
		ddsd.dwTextureStage  = m_dwStage;
		ddsd.dwWidth         = m_dwWidth;
		ddsd.dwHeight        = m_dwHeight;
	}
	else
	{
		ddsd.dwFlags         = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH |
							   DDSD_PIXELFORMAT | DDSD_TEXTURESTAGE | DDSD_CKSRCBLT | D3DTEXTR_TRANSPARENTBLACK
							   | DDSD_MIPMAPCOUNT ;
		ddsd.ddsCaps.dwCaps  = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP | DDSCAPS_COMPLEX ;
		ddsd.dwTextureStage  = m_dwStage;
		ddsd.dwWidth         = m_dwWidth;
		ddsd.dwHeight        = m_dwHeight;
		ddsd.dwMipMapCount   = NB_MIPMAP_LEVELS;
	}

	ddsd.ddckCKSrcBlt.dwColorSpaceHighValue = RGB(0, 0, 0);
	ddsd.ddckCKSrcBlt.dwColorSpaceLowValue = RGB(0, 0, 0);


	// Turn on texture management for hardware devices
	if (ddDesc.deviceGUID == IID_IDirect3DHALDevice)
		ddsd.ddsCaps.dwCaps2 = DDSCAPS2_TEXTUREMANAGE;
	else if (ddDesc.deviceGUID == IID_IDirect3DTnLHalDevice)
		ddsd.ddsCaps.dwCaps2 = DDSCAPS2_TEXTUREMANAGE;
	else
		ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;

	// Adjust width and height to be powers of 2, if the device requires it
	if ((ddDesc.dpcTriCaps.dwTextureCaps & D3DPTEXTURECAPS_POW2) || 1)
	{
		for (ddsd.dwWidth = 1;  m_dwWidth > ddsd.dwWidth;   ddsd.dwWidth <<= 1);

		for (ddsd.dwHeight = 1; m_dwHeight > ddsd.dwHeight; ddsd.dwHeight <<= 1);
	}

	// Limit max texture sizes, if the driver can't handle large textures
	DWORD dwMaxWidth  = ddDesc.dwMaxTextureWidth;
	DWORD dwMaxHeight = ddDesc.dwMaxTextureHeight;

	if ((Project.TextureSize) && bGlobalTextureStretch)
	{
		// Max quality
		if (Project.TextureSize == 0)
		{
			ddsd.dwWidth  = m_dwWidth;
			ddsd.dwHeight = m_dwHeight;
		}
		else if (Project.TextureSize == 2)
		{
			float fRatio = 1;

			if ((ddsd.dwWidth > 128) || (ddsd.dwHeight > 128))
			{
				float fVal = ARX_CLEAN_WARN_CAST_FLOAT(max(ddsd.dwWidth, ddsd.dwHeight));
				fRatio = 128.0f / fVal;
			}

			ddsd.dwWidth  = ARX_CLEAN_WARN_CAST_DWORD(ddsd.dwWidth * fRatio);
			ddsd.dwHeight = ARX_CLEAN_WARN_CAST_DWORD(ddsd.dwHeight * fRatio);
		}
		else if (Project.TextureSize == 64)
		{
			float fRatio = 1;

			if ((ddsd.dwWidth > 64) || (ddsd.dwHeight > 64))
			{
				fRatio = 0.5f;
			}

			ddsd.dwWidth  = ARX_CLEAN_WARN_CAST_DWORD(ddsd.dwWidth * fRatio);
			ddsd.dwHeight = ARX_CLEAN_WARN_CAST_DWORD(ddsd.dwHeight * fRatio);
		}
	}

	if (ddsd.dwWidth > dwMaxWidth)
		ddsd.dwWidth = dwMaxWidth;

	if (ddsd.dwHeight > dwMaxHeight)
		ddsd.dwHeight = dwMaxHeight;

	if (GLOBAL_FORCE_MINI_TEXTURE)
	{
		ddsd.dwWidth	= 8;
		ddsd.dwHeight	= 8;
	}

	if ( ( tTxt.find("FILLED_GAUGE_BLUE") != std::string::npos ) ||
	     ( tTxt.find("FILLED_GAUGE_RED") != std::string::npos ) )
	{
		ddsd.dwWidth = 32;

		if (ddDesc.dpcTriCaps.dwTextureCaps & D3DPTEXTURECAPS_POW2)
			ddsd.dwHeight = 128;
		else
			ddsd.dwHeight = 96;
	}

	//-------------------------------------------------------------------------

	// Make the texture square, if the driver requires it
	if (ddDesc.dpcTriCaps.dwTextureCaps & D3DPTEXTURECAPS_SQUAREONLY)
	{
		if (ddsd.dwWidth > ddsd.dwHeight) ddsd.dwHeight = ddsd.dwWidth;
		else                               ddsd.dwWidth  = ddsd.dwHeight;
	}

	// Setup the structure to be used for texture enumration.
	TEXTURESEARCHINFO tsi;
	tsi.bFoundGoodFormat = false;
	tsi.pddpf            = &ddsd.ddpfPixelFormat;
	tsi.dwDesiredBPP     = m_dwBPP;
	tsi.bUsePalette      = false;
	tsi.bUseAlpha        = m_bHasAlpha;

	tsi.dwDesiredBPP = Project.TextureBits;

	if (m_dwFlags & (D3DTEXTR_TRANSPARENTWHITE | D3DTEXTR_TRANSPARENTBLACK))
	{
		if (tsi.bUsePalette)
		{
			if (ddDesc.dpcTriCaps.dwTextureCaps & D3DPTEXTURECAPS_ALPHAPALETTE)
			{
				tsi.bUseAlpha   = true;
				tsi.bUsePalette = true;
			}
			else
			{
				tsi.bUseAlpha   = true;
				tsi.bUsePalette = false;
			}
		}
	}

	// Enumerate the texture formats, and find the closest device-supported
	// texture pixel format
	tsi.pddpf->dwRBitMask = 0x0000F800;
	tsi.pddpf->dwGBitMask = 0x000007E0;
	tsi.pddpf->dwBBitMask = 0x0000001F;
	GDevice->EnumTextureFormats(TextureSearchCallback, &tsi);

	if (! tsi.bFoundGoodFormat)
	{
		tsi.pddpf->dwRBitMask = 0x00007C00;
		tsi.pddpf->dwGBitMask = 0x000003E0;
		tsi.pddpf->dwBBitMask = 0x0000001F;
		GDevice->EnumTextureFormats(TextureSearchCallback, &tsi);
	}

	// If we couldn't find a format, let's try a default format
	if (false == tsi.bFoundGoodFormat)
	{
		tsi.bUsePalette  = false;
		tsi.dwDesiredBPP = 16;
		tsi.pddpf->dwRBitMask = 0x0000F800;
		tsi.pddpf->dwGBitMask = 0x000007E0;
		tsi.pddpf->dwBBitMask = 0x0000001F;
		GDevice->EnumTextureFormats(TextureSearchCallback, &tsi);

		if (! tsi.bFoundGoodFormat)
		{
			tsi.pddpf->dwRBitMask = 0x00007C00;
			tsi.pddpf->dwGBitMask = 0x000003E0;
			tsi.pddpf->dwBBitMask = 0x0000001F;
			GDevice->EnumTextureFormats(TextureSearchCallback, &tsi);
		}

		// If we still fail, we cannot create this texture
		if (false == tsi.bFoundGoodFormat)
			return E_FAIL;
	}

	// Get the DirectDraw interface for creating surface
	LPDIRECTDRAW7        pDD;
	LPDIRECTDRAWSURFACE7 pddsRender;
	GDevice->GetRenderTarget(&pddsRender);
	pddsRender->GetDDInterface((VOID **)&pDD);
	pddsRender->Release();

	if ((bActivateBump) &&
			(EERIE_USES_BUMP_MAP))
	{
		DDSURFACEDESC2 ddsdbump = ddsd;
		ddsdbump.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_TEXTURESTAGE | DDSD_CKSRCBLT | D3DTEXTR_TRANSPARENTBLACK;
		ddsdbump.ddsCaps.dwCaps  = DDSCAPS_TEXTURE ;

		HRESULT hr = pDD->CreateSurface(&ddsdbump, &m_pddsBumpMap, NULL);

		if (FAILED(hr))
		{
			m_pddsBumpMap = NULL;
		}
	}
	else
	{
		m_pddsBumpMap = NULL;
	}

	// Create a new surface for the texture
	HRESULT hr = pDD->CreateSurface(&ddsd, &t_m_pddsSurface, NULL);

	// Done with DDraw
	pDD->Release();

	if (FAILED(hr))
		return hr;

	if (m_pRGBAData)
		CopyRGBADataToSurface(t_m_pddsSurface);

	if ((!(this->m_dwFlags & D3DTEXTR_NO_MIPMAP)) && BLURTEXTURES) SmoothSurface(t_m_pddsSurface);

	if (SPECIAL_PNUX) PnuxSurface(t_m_pddsSurface);

	if ( tTxt.find("ARKANE.") != std::string::npos )
	{
		bColorKey = false;
		bColorKey2D = true;
	}
	else
	{
		if (IsColorKeyInSurface(t_m_pddsSurface))
		{
			bColorKey = true;

			if ( ( tTxt.find("INTERFACE") != std::string::npos ) ||
			     ( tTxt.find("ICON") != std::string::npos ) )
			{
				bColorKey2D = true;
			}


			t_m_pddsSurface->SetColorKey(DDCKEY_SRCBLT , &ddsd.ddckCKSrcBlt);

			if (m_pddsBumpMap)
			{
				m_pddsBumpMap->Release();
				m_pddsBumpMap = NULL;
			}
		}
		else
		{
			if ((EERIE_USES_BUMP_MAP) &&
					(m_pddsBumpMap)) CopySurfaceToBumpMap(t_m_pddsSurface, m_pddsBumpMap);
		}
	}

	/**************/
	long ox = ddsd.dwWidth;
	long oy = ddsd.dwHeight;

	this->m_dwDeviceWidth = ox;
	this->m_dwDeviceHeight = oy;

	// Loop through each surface in the mipmap, copying the bitmap to the temp
	// surface, and then blitting the temp surface to the real one.

	LPDIRECTDRAWSURFACE7 pddsDest = t_m_pddsSurface;
	LPDIRECTDRAWSURFACE7 MipDest[NB_MIPMAP_LEVELS];
	LPDIRECTDRAWSURFACE7 pddsNextDest;

	if (bGlobalTextureStretch)
	{
		if ((!(this->m_dwFlags & D3DTEXTR_NO_MIPMAP)) && (!NOMIPMAPS))
		{
			pddsDest->AddRef();

			for (WORD wNum = 0; wNum < NB_MIPMAP_LEVELS; wNum++)
			{
				if (wNum)
				{
					StretchCopySurfaceToSurface(MipDest[wNum-1], pddsDest);
				}

				// Get the next surface in the chain. Do a Release() call, though, to
				// avoid increasing the ref counts on the surfaces.
				DDSCAPS2 ddsCaps;
				ddsCaps.dwCaps  = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP;
				ddsCaps.dwCaps2 = 0;
				ddsCaps.dwCaps3 = 0;
				ddsCaps.dwCaps4 = 0;
				MipDest[wNum] = pddsDest;

				if (SUCCEEDED(pddsDest->GetAttachedSurface(&ddsCaps, &pddsNextDest)))
				{
					if (wNum + 1 < NB_MIPMAP_LEVELS)
						MipDest[wNum+1] = pddsDest;

					pddsDest->Release();
					pddsDest = pddsNextDest;
				}
				else if (wNum + 1 < NB_MIPMAP_LEVELS)
					MipDest[wNum+1] = NULL;
			}
		}
	}

	m_pddsSurface = t_m_pddsSurface;

	this->m_odx = (1.f / (float)m_dwWidth);
	this->m_hdx = 0.5f * (1.f / (float)ddsd.dwWidth);
	this->m_dx += this->m_hdx;
	this->m_ody = (1.f / (float)m_dwHeight);
	this->m_hdy = 0.5f * (1.f / (float)ddsd.dwHeight);
	this->m_dy += this->m_hdy;

	if (TextureHalo)
	{
		D3DTextr_KillTexture(TextureHalo);
		TextureHalo = AddHalo(iHaloNbCouche, fHaloRed, fHaloGreen, fHaloBlue, halodecalX, halodecalY);
	}

	if (m_dwFlags & D3DTEXTR_FAKE_BORDER)
		SpecialBorderSurface(this, this->m_dwOriginalWidth, this->m_dwOriginalHeight);

	return S_OK;
}

void TextureContainer::RemoveFakeBlack()
{
	BYTE * sBytes = (BYTE *)m_pRGBAData;

	DWORD dwRShiftL		= 0;
	DWORD dwRShiftR		= 0;
	DWORD dwRMask		= 0x000000FF;
	DWORD dwGShiftL		= 0;
	DWORD dwGShiftR		= 8;
	DWORD dwGMask		= 0x0000FF00;
	DWORD dwBShiftL		= 0;
	DWORD dwBShiftR		= 16;
	DWORD dwBMask		= 0x00FF0000;
	DWORD dwAShiftL		= 0;
	DWORD dwAShiftR		= 24;
	DWORD dwAMask		= 0xFF000000;
	DWORD * pSrcData32	= (DWORD *)sBytes;

	for (DWORD y = 0 ; y < static_cast<unsigned long>(m_dwHeight) ; y++)
	{

		for (DWORD x = 0 ; x < static_cast<unsigned long>(m_dwWidth) ; x++)
		{
			// Original Pixel
			DWORD dwPixel;
			long offset = x + y * m_dwWidth ;

			dwPixel = pSrcData32[offset];

			BYTE r1 = (BYTE)(((dwPixel & dwRMask) >> dwRShiftR) << dwRShiftL);    
			BYTE g1 = (BYTE)(((dwPixel & dwGMask) >> dwGShiftR) << dwGShiftL);     
			BYTE b1 = (BYTE)(((dwPixel & dwBMask) >> dwBShiftR) << dwBShiftL); 
			BYTE a1 = (BYTE)(((dwPixel & dwAMask) >> dwAShiftR) << dwAShiftL);

			if ((r1 != 0) && (r1 < 15)) r1 = 15;

			if ((g1 != 0) && (g1 < 15)) g1 = 15;

			if ((b1 != 0) && (b1 < 15)) b1 = 15;

			long rr	= r1;
			BYTE r	= (BYTE)rr;
			long gg	= g1;
			BYTE g	= (BYTE)gg;
			long bb	= b1;
			BYTE b	= (BYTE)bb;
			long aa	= a1;
			BYTE a	= (BYTE)aa;

			if ((r1 == 0) && (g1 == 0) && (b1 == 0)) a = 0;

			DWORD dr = ((r >> (dwRShiftL)) << dwRShiftR) & dwRMask;
			DWORD dg = ((g >> (dwGShiftL)) << dwGShiftR) & dwGMask;
			DWORD db = ((b >> (dwBShiftL)) << dwBShiftR) & dwBMask;
			DWORD da = ((a >> (dwAShiftL)) << dwAShiftR) & dwAMask;

			pSrcData32[offset] = (DWORD)(dr + dg + db + da);
		}
	}
}


//-----------------------------------------------------------------------------
// Name: CopyBitmapToSurface()
// Desc: Copy a bitmap section to the direct draw surface, a restore must have been done prior to calling this function
//-----------------------------------------------------------------------------
HRESULT TextureContainer::CopyBitmapToSurface(HBITMAP hbitmap, int depx, int depy, int largeur, int hauteur)
{
	LPDIRECTDRAWSURFACE7 Surface;

	Surface = this->m_pddsSurface;

	// Get a DDraw object to create a temporary surface
	LPDIRECTDRAW7 pDD;
	Surface->GetDDInterface((VOID **)&pDD);

	// Get the bitmap structure (to extract width, height, and bpp)
	BITMAP bm;
	GetObject(hbitmap, sizeof(BITMAP), &bm);

	// Setup the new surface desc
	DDSURFACEDESC2 ddsd;
	ddsd.dwSize = sizeof(ddsd);
	Surface->GetSurfaceDesc(&ddsd);
	float fWidthSurface = (float)ddsd.dwWidth;
	float fHeightSurface = (float)ddsd.dwHeight;
	ddsd.dwFlags          = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT |
							DDSD_TEXTURESTAGE;
	ddsd.ddsCaps.dwCaps   = DDSCAPS_TEXTURE | DDSCAPS_SYSTEMMEMORY;
	ddsd.ddsCaps.dwCaps2  = 0L;
	ddsd.dwWidth          = largeur;
	ddsd.dwHeight         = hauteur;

	// Create a new surface for the texture
	LPDIRECTDRAWSURFACE7 pddsTempSurface;
	HRESULT hr;

	if (FAILED(hr = pDD->CreateSurface(&ddsd, &pddsTempSurface, NULL)))
	{
		pDD->Release();
		return hr;
	}

	// Get a DC for the bitmap
	HDC hdcBitmap = CreateCompatibleDC(NULL);

	if (NULL == hdcBitmap)
	{
		pddsTempSurface->Release();
		pDD->Release();
		return hr;
	}

	SelectObject(hdcBitmap, hbitmap);

	// Handle palettized textures. Need to attach a palette
	if (ddsd.ddpfPixelFormat.dwRGBBitCount == 8)
	{
		LPDIRECTDRAWPALETTE  pPalette;
		DWORD dwPaletteFlags = DDPCAPS_8BIT | DDPCAPS_ALLOW256;
		DWORD pe[256];

		UINT uiRes = GetDIBColorTable(hdcBitmap, 0, 256, (RGBQUAD *)pe);
		assert(uiRes <= std::numeric_limits<WORD>::max());
		WORD  wNumColors = static_cast<WORD>(uiRes);

		// Create the color table
		for (WORD i = 0; i < wNumColors; i++)
		{
			pe[i] = RGB(GetBValue(pe[i]), GetGValue(pe[i]), GetRValue(pe[i]));

			// Handle textures with transparent pixels
			if (m_dwFlags & (D3DTEXTR_TRANSPARENTWHITE | D3DTEXTR_TRANSPARENTBLACK))
			{
				// Set alpha for opaque pixels
				if (m_dwFlags & D3DTEXTR_TRANSPARENTBLACK)
				{
					if (pe[i] != 0x00000000)
						pe[i] |= 0xff000000;
				}
				else if (m_dwFlags & D3DTEXTR_TRANSPARENTWHITE)
				{
					if (pe[i] != 0x00ffffff)
						pe[i] |= 0xff000000;
				}
			}
		}

		// Add DDPCAPS_ALPHA flag for textures with transparent pixels
		if (m_dwFlags & (D3DTEXTR_TRANSPARENTWHITE | D3DTEXTR_TRANSPARENTBLACK))
			dwPaletteFlags |= DDPCAPS_ALPHA;

		// Create & attach a palette
		pDD->CreatePalette(dwPaletteFlags, (PALETTEENTRY *)pe, &pPalette, NULL);
		pddsTempSurface->SetPalette(pPalette);
		Surface->SetPalette(pPalette);
		SAFE_RELEASE(pPalette);
	}

	// Copy the bitmap image to the surface.
	HDC hdcSurface;

	if (SUCCEEDED(pddsTempSurface->GetDC(&hdcSurface)))
	{
		BitBlt(hdcSurface, 0, 0, largeur, hauteur, hdcBitmap, depx, depy,
			   SRCCOPY);
		pddsTempSurface->ReleaseDC(hdcSurface);

	}

	DeleteDC(hdcBitmap);

	if (bGlobalTextureStretch)
	{
		m_dx = .999999f;
		m_dy = .999999f;
		Surface->Blt(NULL, pddsTempSurface, NULL, DDBLT_WAIT, NULL);
	}
	else
	{
		RECT rBlt;
		SetRect(&rBlt, 0, 0, ddsd.dwWidth, ddsd.dwHeight);

		if (ddsd.dwWidth < fWidthSurface)
		{
			m_dx = ((float)ddsd.dwWidth) / fWidthSurface;
		}
		else
		{
			rBlt.right = (int)fWidthSurface;
			m_dx = .999999f;
		}

		if (ddsd.dwHeight < fHeightSurface)
		{
			m_dy = ((float)ddsd.dwHeight) / fHeightSurface;
		}
		else
		{
			rBlt.bottom = (int)fHeightSurface;
			m_dy = 0.999999f;
		}

		Surface->Blt(&rBlt, pddsTempSurface, NULL, DDBLT_WAIT, NULL);
	}

	DDSURFACEDESC2 ddesc, ddesc2;
	ddesc.dwSize = sizeof(ddesc);
	ddesc2.dwSize = sizeof(ddesc2);

	// Done with the temp surface
	pddsTempSurface->Release();

	// For textures with real alpha (not palettized), set transparent bits
	if (ddsd.ddpfPixelFormat.dwRGBAlphaBitMask)
	{
		if (m_dwFlags & (D3DTEXTR_TRANSPARENTWHITE | D3DTEXTR_TRANSPARENTBLACK))
		{
			// Lock the texture surface
			DDSURFACEDESC2 ddsd;
			ddsd.dwSize = sizeof(ddsd);

			while (Surface->Lock(NULL, &ddsd, 0, NULL) ==
					DDERR_WASSTILLDRAWING);

			DWORD dwAlphaMask = ddsd.ddpfPixelFormat.dwRGBAlphaBitMask;
			DWORD dwRGBMask   = (ddsd.ddpfPixelFormat.dwRBitMask |
								 ddsd.ddpfPixelFormat.dwGBitMask |
								 ddsd.ddpfPixelFormat.dwBBitMask);
			DWORD dwColorkey  = 0x00000000; // Colorkey on black

			if (m_dwFlags & D3DTEXTR_TRANSPARENTWHITE)
				dwColorkey = dwRGBMask;     // Colorkey on white

			// Add an opaque alpha value to each non-colorkeyed pixel
			for (DWORD y = 0; y < ddsd.dwHeight; y++)
			{
				WORD * p16 = (WORD *)((BYTE *)ddsd.lpSurface + y * ddsd.lPitch);
				DWORD * p32 = (DWORD *)((BYTE *)ddsd.lpSurface + y * ddsd.lPitch);

				for (DWORD x = 0; x < ddsd.dwWidth; x++)
				{
					if (ddsd.ddpfPixelFormat.dwRGBBitCount == 16)
					{
						if ((*p16 &= dwRGBMask) != dwColorkey)
							* p16 |= dwAlphaMask;

						p16++;
					}

					if (ddsd.ddpfPixelFormat.dwRGBBitCount == 32)
					{
						if ((*p32 &= dwRGBMask) != dwColorkey)
							* p32 |= dwAlphaMask;

						p32++;
					}
				}
			}

			Surface->Unlock(NULL);
		}
	}

	pDD->Release();

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: CopyRGBADataToSurface()
// Desc: Invalidates the current texture objects and rebuilds new ones
//       using the new device.
//-----------------------------------------------------------------------------
HRESULT TextureContainer::CopyRGBADataToSurface(LPDIRECTDRAWSURFACE7 Surface)
{
	// Get a DDraw object to create a temporary surface
	LPDIRECTDRAW7 pDD;
	Surface->GetDDInterface((VOID **)&pDD);

	// Setup the new surface desc
	DDSURFACEDESC2 ddsd;
	ddsd.dwSize = sizeof(ddsd);
	Surface->GetSurfaceDesc(&ddsd);
	float fWidthSurface = (float)ddsd.dwWidth;
	float fHeightSurface = (float)ddsd.dwHeight;
	ddsd.dwFlags         = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_TEXTURESTAGE;
	ddsd.ddsCaps.dwCaps  = DDSCAPS_TEXTURE | DDSCAPS_SYSTEMMEMORY;
	ddsd.ddsCaps.dwCaps2 = 0L;
	ddsd.dwWidth         = m_dwWidth;
	ddsd.dwHeight        = m_dwHeight;

	// Create a new surface for the texture
	LPDIRECTDRAWSURFACE7 pddsTempSurface;
	HRESULT hr;

	if (FAILED(hr = pDD->CreateSurface(&ddsd, &pddsTempSurface, NULL)))
	{
		pDD->Release();
		return 0;
	}

	while (pddsTempSurface->Lock(NULL, &ddsd, 0, 0) == DDERR_WASSTILLDRAWING);

	//    DWORD lPitch = ddsd.lPitch;
	BYTE * pBytes = (BYTE *)ddsd.lpSurface;

	DWORD dwRMask = ddsd.ddpfPixelFormat.dwRBitMask;
	DWORD dwGMask = ddsd.ddpfPixelFormat.dwGBitMask;
	DWORD dwBMask = ddsd.ddpfPixelFormat.dwBBitMask;
	DWORD dwAMask = ddsd.ddpfPixelFormat.dwRGBAlphaBitMask;

	DWORD dwRShiftL = 8, dwRShiftR = 0;
	DWORD dwGShiftL = 8, dwGShiftR = 0;
	DWORD dwBShiftL = 8, dwBShiftR = 0;
	DWORD dwAShiftL = 8, dwAShiftR = 0;

	DWORD dwMask;

	for (dwMask = dwRMask; dwMask && !(dwMask & 0x1); dwMask >>= 1) dwRShiftR++;

	for (; dwMask; dwMask >>= 1) dwRShiftL--;

	for (dwMask = dwGMask; dwMask && !(dwMask & 0x1); dwMask >>= 1) dwGShiftR++;

	for (; dwMask; dwMask >>= 1) dwGShiftL--;

	for (dwMask = dwBMask; dwMask && !(dwMask & 0x1); dwMask >>= 1) dwBShiftR++;

	for (; dwMask; dwMask >>= 1) dwBShiftL--;

	for (dwMask = dwAMask; dwMask && !(dwMask & 0x1); dwMask >>= 1) dwAShiftR++;

	for (; dwMask; dwMask >>= 1) dwAShiftL--;

	for (DWORD y = 0; y < ddsd.dwHeight; y++)
	{
		DWORD * pDstData32 = (DWORD *)pBytes;
		WORD * pDstData16 = (WORD *)pBytes;

		for (DWORD x = 0; x < ddsd.dwWidth; x++)
		{
			DWORD dwPixel = m_pRGBAData[y*ddsd.dwWidth+x];

			BYTE r = (BYTE)((dwPixel >> 0)  & 0x000000ff);
			BYTE g = (BYTE)((dwPixel >> 8)  & 0x000000ff);
			BYTE b = (BYTE)((dwPixel >> 16) & 0x000000ff);
			BYTE a = (BYTE)((dwPixel >> 24) & 0x000000ff);

			DWORD dr = ((r >> (dwRShiftL)) << dwRShiftR)&dwRMask;
			DWORD dg = ((g >> (dwGShiftL)) << dwGShiftR)&dwGMask;
			DWORD db = ((b >> (dwBShiftL)) << dwBShiftR)&dwBMask;
			DWORD da = ((a >> (dwAShiftL)) << dwAShiftR)&dwAMask;

			if (32 == ddsd.ddpfPixelFormat.dwRGBBitCount)
				pDstData32[x] = (DWORD)(dr + dg + db + da);
			else
				pDstData16[x] = (WORD)(dr + dg + db + da);
		}

		pBytes += ddsd.lPitch;
	}

	pddsTempSurface->Unlock(0);

	// Copy the temp surface to the real texture surface
	if (bGlobalTextureStretch)
	{
		m_dx = .999999f;
		m_dy = .999999f;
		Surface->Blt(NULL, pddsTempSurface, NULL, DDBLT_WAIT, NULL);
	}
	else
	{
		RECT rBlt;
		SetRect(&rBlt, 0, 0, ddsd.dwWidth, ddsd.dwHeight);

		if (ddsd.dwWidth < fWidthSurface)
		{
			m_dx = ((float)ddsd.dwWidth) / fWidthSurface;
		}
		else
		{
			rBlt.right = (int)fWidthSurface;
			m_dx = .999999f;
		}

		if (ddsd.dwHeight < fHeightSurface)
		{
			m_dy = ((float)ddsd.dwHeight) / fHeightSurface;
		}
		else
		{
			rBlt.bottom = (int)fHeightSurface;
			m_dy = 0.999999f;
		}

		Surface->Blt(&rBlt, pddsTempSurface, NULL, DDBLT_WAIT, NULL);
	}

	// Done with the temp objects
	pddsTempSurface->Release();
	pDD->Release();

	return S_OK;
}

void ConvertData( std::string& dat)
{
	size_t substrStart = 0;
	size_t substrLen = std::string::npos;

	size_t posStart = dat.find_first_of('"');
	if( posStart != std::string::npos )
		substrStart = posStart + 1;

	size_t posEnd = dat.find_last_of('"');
	if( posEnd != std::string::npos )
		substrLen = posEnd - substrStart;

	dat = dat.substr(substrStart, substrLen);
}

void LoadRefinementMap(const std::string& fileName, std::map<string, string>& refinementMap)
{
	char * fileContent = NULL;
	size_t fileSize = 0;
	
	if (PAK_FileExist(fileName))
		fileContent = (char *)PAK_FileLoadMallocZero(fileName, fileSize);

	if (fileContent)
	{
		unsigned char * from = (unsigned char *)fileContent;
		u32 fromSize = fileSize;
		std::string data;
		u32 pos = 0;
		long count = 0;	

		std::string str1;
		
		while (pos < fileSize)
		{
			data.resize(0);

			while ((from[pos] != '\n') && (pos < fromSize))
			{
				data += from[pos++];

				if (pos >= fileSize) break;
			}

			while ((pos < fromSize) && (from[pos] < 32)) pos++;

			if (count == 2)
			{
				count = 0;
				continue;
			}

			ConvertData(data);

			if (count == 0)
				str1 = data;
			
			if (count == 1)
			{
				MakeUpcase( str1 );
				MakeUpcase( data );

				if( data.compare( "NONE" ) != 0 ) // If the string does not contain "NONE"
					refinementMap[str1] = data;
			}

			count++;
		}

		free(fileContent);
	}
}

typedef std::map<string, string> RefinementMap;
RefinementMap g_GlobalRefine;
RefinementMap g_Refine;

void LookForRefinementMap(TextureContainer * tc)
{
	std::string str1;
	std::string str2;
	tc->TextureRefinement = NULL;

	static bool loadedRefinements = false;
	if (!loadedRefinements)
	{
		const char INI_REFINEMENT_GLOBAL[] = "Graph\\Obj3D\\Textures\\Refinement\\GlobalRefinement.ini";
		const char INI_REFINEMENT[] = "Graph\\Obj3D\\Textures\\Refinement\\Refinement.ini";

		LoadRefinementMap(INI_REFINEMENT_GLOBAL, g_GlobalRefine);
		LoadRefinementMap(INI_REFINEMENT, g_Refine);

        loadedRefinements = true;
	}

	std::string name = GetName(tc->m_strName);
	MakeUpcase( name );

	RefinementMap::const_iterator it = g_GlobalRefine.find(name);
	if( it != g_GlobalRefine.end() )
	{
		str2 = "Graph\\Obj3D\\Textures\\Refinement\\" + (*it).second + ".bmp";
		tc->TextureRefinement = D3DTextr_CreateTextureFromFile(str2, 0, D3DTEXTR_16BITSPERPIXEL);
	}

	it = g_Refine.find(name);
	if( it != g_Refine.end() )
	{
		str2 = "Graph\\Obj3D\\Textures\\Refinement\\" + (*it).second + ".bmp";
		tc->TextureRefinement = D3DTextr_CreateTextureFromFile(str2, 0, D3DTEXTR_16BITSPERPIXEL);
	}
}

void ReleaseAllTCWithFlag(long flag)
{
	restart:
	;
	TextureContainer * ptcTexture = g_ptcTextureList;

	while (ptcTexture)
	{
		if (ptcTexture->systemflags & flag)
		{
			D3DTextr_KillTexture(ptcTexture);
			goto restart;
		}
		ptcTexture = ptcTexture->m_pNext;
	}

}

extern void MakeUserFlag(TextureContainer * tc);

//-----------------------------------------------------------------------------
// Name: D3DTextr_CreateTextureFromFile()
// Desc: Is passed a filename and creates a local Bitmap from that file.
//       The texture can not be used until it is restored, however.
//-----------------------------------------------------------------------------

extern long DEBUGSYS;

TextureContainer * D3DTextr_CreateTextureFromFile( const std::string& _strName, DWORD dwStage,
		DWORD dwFlags, long sysflags)
{
	std::string strName = _strName;

	if(strName.empty()) {
		return NULL;
	}

	// Check first to see if the texture is already loaded
	MakeUpcase(strName);

	TextureContainer * LastTextureContainer;
	if(NULL != (LastTextureContainer = FindTexture(strName))) {
		return LastTextureContainer;
	}

	// Allocate and add the texture to the linked list of textures;
	TextureContainer * ptcTexture = new TextureContainer(strName, dwStage,
			dwFlags);

	if(NULL == ptcTexture) {
		return NULL;
	}

	ptcTexture->systemflags = sysflags;

	if (GLOBAL_EERIETEXTUREFLAG_LOADSCENE_RELEASE == 1)
		ptcTexture->systemflags |= EERIETEXTUREFLAG_LOADSCENE_RELEASE;
	else if (GLOBAL_EERIETEXTUREFLAG_LOADSCENE_RELEASE == -1)
		ptcTexture->systemflags &= ~EERIETEXTUREFLAG_LOADSCENE_RELEASE;

	// Create a bitmap and load the texture file into it,
	if(FAILED(ptcTexture->LoadImageData())) {
		delete ptcTexture;
		return NULL;
	}

	ptcTexture->m_dwDeviceWidth = 0;
	ptcTexture->m_dwDeviceHeight = 0;
	ptcTexture->m_dwOriginalWidth = ptcTexture->m_dwWidth;
	ptcTexture->m_dwOriginalHeight = ptcTexture->m_dwHeight;

	if (ptcTexture->m_dwWidth > 0.f)
	{
		ptcTexture->m_odx = 1.f / (float)ptcTexture->m_dwWidth;
		ptcTexture->m_dx = ptcTexture->m_odx;
	}
	else
		ptcTexture->m_odx = ptcTexture->m_dx = ( 1.0f / 256 );

	ptcTexture->m_hdx = 0.5f * ptcTexture->m_dx;

	if (ptcTexture->m_dwHeight > 0.f)
	{
		ptcTexture->m_ody = 1.f / (float)ptcTexture->m_dwHeight;
		ptcTexture->m_dy = ptcTexture->m_ody;
	}
	else
		ptcTexture->m_ody = ptcTexture->m_dy = ( 1.0f / 256 );

	ptcTexture->m_hdy = 0.5f * ptcTexture->m_dy;

	TextureContainer * ReturnValue = ptcTexture;

	if (!(dwFlags & D3DTEXTR_NO_REFINEMENT))
		LookForRefinementMap(ReturnValue);

	ReturnValue = ptcTexture;

	ptcTexture->Use();
	MakeUserFlag(ptcTexture);
	return ReturnValue;
}



//-----------------------------------------------------------------------------
// Name: D3DTextr_CreateEmptyTexture()
// Desc: Creates an empty texture.
//-----------------------------------------------------------------------------
HRESULT D3DTextr_CreateEmptyTexture( const std::string& _strName, DWORD dwWidth,
									DWORD dwHeight, DWORD dwStage,
									DWORD dwFlags, DWORD flags)
{
	std::string strName = _strName;

	if (!(flags & 1)) // no name check
	{
		// Check parameters
		if ( strName.empty() )
			return E_INVALIDARG;

		// Check first to see if the texture is already loaded
		MakeUpcase(strName);

		if (NULL != FindTexture(strName))
			return E_FAIL;
	}

	// Allocate and add the texture to the linked list of textures;
	TextureContainer * ptcTexture = new TextureContainer(strName, dwStage,
			dwFlags);

	if (NULL == ptcTexture)
		return E_OUTOFMEMORY;

	// Save dimensions
	ptcTexture->m_dwWidth  = dwWidth;
	ptcTexture->m_dwHeight = dwHeight;
	ptcTexture->m_dwBPP    = 32;

	// Save alpha usage flag
	if (dwFlags & D3DTEXTR_CREATEWITHALPHA)
		ptcTexture->m_bHasAlpha = true;

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: D3DTextr_Restore()
// Desc: Invalidates the current texture objects and rebuilds new ones
//       using the new device.
//-----------------------------------------------------------------------------
HRESULT D3DTextr_Restore( const std::string& strName)
{
	std::string temp = strName;
	MakeUpcase(temp);
	TextureContainer * ptcTexture = FindTexture(temp);

	if (NULL == ptcTexture)
		return DDERR_NOTFOUND;

	// Restore the texture (this recreates the new surface for this device).
	return ptcTexture->Restore();
}

//-----------------------------------------------------------------------------
// Name: D3DTextr_RestoreAllTextures()
// Desc: This function is called when a mode is changed. It updates all
//       texture objects to be valid with the new device.
//-----------------------------------------------------------------------------
HRESULT D3DTextr_RestoreAllTextures()
{
	TextureContainer * ptcTexture = g_ptcTextureList;

	while (ptcTexture)
	{
		D3DTextr_Restore(ptcTexture->m_strName);
		ptcTexture = ptcTexture->m_pNext;
	}

	GRenderer->RestoreAllTextures();

	return S_OK;
}

HRESULT D3DTextr_TESTRestoreAllTextures()
{
	TextureContainer * ptcTexture = g_ptcTextureList;

	while (ptcTexture)
	{
		if (!ptcTexture->m_pddsSurface)
			D3DTextr_Restore(ptcTexture->m_strName);

	ptcTexture = ptcTexture->m_pNext;
}

return S_OK;
}

//-----------------------------------------------------------------------------
// Name: D3DTextr_InvalidateAllTextures()
// Desc: This function is called when a mode is changed. It invalidates
//       all texture objects so their device can be safely released.
//-----------------------------------------------------------------------------
HRESULT D3DTextr_InvalidateAllTextures()
{
	TextureContainer * ptcTexture = g_ptcTextureList;

	while (ptcTexture)
	{
		if (ptcTexture->m_pddsSurface)
		{
			ptcTexture->m_pddsSurface->DeleteAttachedSurface(0, NULL);
			ptcTexture->m_pddsSurface->Release();
			ptcTexture->m_pddsSurface = NULL;
		}

		if (ptcTexture->m_pddsBumpMap)
		{
			ptcTexture->m_pddsBumpMap->Release();
			ptcTexture->m_pddsBumpMap = NULL;
		}

		ptcTexture = ptcTexture->m_pNext;
	}

	GRenderer->ReleaseAllTextures();

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: D3DTextr_DestroyTexture()
// Desc: Frees the resources for the specified texture container
//-----------------------------------------------------------------------------

/*
	Destroyed a charged surface
	seb
*/
void D3DTextr_KillTexture(TextureContainer * tex)
{
	D3DTextr_DestroyContainer(tex);
	tex = NULL;
}

/*
	Destroy all the charged surfaces
*/
void D3DTextr_KillAllTextures()
{
	while (g_ptcTextureList)
	{
		D3DTextr_KillTexture(g_ptcTextureList);
	}
}


//-----------------------------------------------------------------------------
// Name: D3DTextr_DestroyContainer()
// Desc: Frees the resources for the specified texture container
//-----------------------------------------------------------------------------
HRESULT D3DTextr_DestroyContainer(TextureContainer * ptcTexture)
{
	SAFE_DELETE(ptcTexture);

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: D3DTextr_GetSurface()
// Desc: Returns a pointer to a d3dSurface from the name of the texture
//-----------------------------------------------------------------------------

TextureContainer * D3DTextr_GetSurfaceContainer( const std::string& _strName)
{
	std::string strName = _strName;
	MakeUpcase(strName);
	TextureContainer * ptcTexture = FindTexture(strName);
	return ptcTexture;
}

//*************************************************************************************
//*************************************************************************************
TextureContainer * GetTextureFile( const std::string& tex, long flag)
{
	long old = GLOBAL_EERIETEXTUREFLAG_LOADSCENE_RELEASE;
	GLOBAL_EERIETEXTUREFLAG_LOADSCENE_RELEASE = -1;
	TextureContainer * returnTc;

	if (flag & 1)
		returnTc = D3DTextr_CreateTextureFromFile(tex, 0, D3DTEXTR_32BITSPERPIXEL);
	else
		returnTc = D3DTextr_CreateTextureFromFile(tex, 0, D3DTEXTR_NO_MIPMAP);

	GLOBAL_EERIETEXTUREFLAG_LOADSCENE_RELEASE = old;
	return returnTc;
}
//*************************************************************************************
//*************************************************************************************
TextureContainer * GetTextureFile_NoRefinement( const std::string& tex, long flag)
{
	long old = GLOBAL_EERIETEXTUREFLAG_LOADSCENE_RELEASE;
	GLOBAL_EERIETEXTUREFLAG_LOADSCENE_RELEASE = -1;
	TextureContainer * returnTc;

	if (flag & 1)
		returnTc = D3DTextr_CreateTextureFromFile(tex, 0, D3DTEXTR_32BITSPERPIXEL | D3DTEXTR_NO_REFINEMENT);
	else
		returnTc = D3DTextr_CreateTextureFromFile(tex, 0, D3DTEXTR_NO_MIPMAP | D3DTEXTR_NO_REFINEMENT);

	GLOBAL_EERIETEXTUREFLAG_LOADSCENE_RELEASE = old;
	return returnTc;
}
//*************************************************************************************
//*************************************************************************************
bool TextureContainer::CreateHalo()
{
	if (this->TextureHalo) return true;

	this->TextureHalo = this->AddHalo(8, 255.f, 255.f, 255.f, this->halodecalX, this->halodecalY);

	if (this->TextureHalo) return true;

	return false;
}

//-----------------------------------------------------------------------------
TextureContainer * TextureContainer::AddHalo(int _iNbCouche, float _fR, float _fG, float _fB, float & _iDecalX, float & _iDecalY)
{
	if ((!m_pddsSurface) ||
			(!_iNbCouche)) return NULL;

	iHaloNbCouche = _iNbCouche;
	fHaloRed = _fR;
	fHaloGreen = _fG;
	fHaloBlue = _fB;

	D3DDEVICEDESC7 ddDesc;

	if (FAILED(GDevice->GetCaps(&ddDesc))) return NULL;

	LPDIRECTDRAW7 pDD;

	if (FAILED(m_pddsSurface->GetDDInterface((VOID **)&pDD)))
	{
		return NULL;
	}


	float fDR = _fR / ((float)_iNbCouche);
	float fDG = _fG / ((float)_iNbCouche);
	float fDB = _fB / ((float)_iNbCouche);

	DDSURFACEDESC2 ddsdSurfaceDesc;
	ddsdSurfaceDesc.dwSize = sizeof(ddsdSurfaceDesc);
	m_pddsSurface->GetSurfaceDesc(&ddsdSurfaceDesc);

	ddsdSurfaceDesc.dwFlags          = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
	ddsdSurfaceDesc.ddsCaps.dwCaps   = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	ddsdSurfaceDesc.ddsCaps.dwCaps2	= 0;

	DWORD dwRMask = ddsdSurfaceDesc.ddpfPixelFormat.dwRBitMask;
	DWORD dwGMask = ddsdSurfaceDesc.ddpfPixelFormat.dwGBitMask;
	DWORD dwBMask = ddsdSurfaceDesc.ddpfPixelFormat.dwBBitMask;
	DWORD dwAMask = ddsdSurfaceDesc.ddpfPixelFormat.dwRGBAlphaBitMask;
	DWORD dwRShiftL = 8, dwRShiftR = 0;
	DWORD dwGShiftL = 8, dwGShiftR = 0;
	DWORD dwBShiftL = 8, dwBShiftR = 0;
	DWORD dwAShiftL = 8, dwAShiftR = 0;
	DWORD dwMask;

	for (dwMask = dwRMask; dwMask && !(dwMask & 0x1); dwMask >>= 1) dwRShiftR++;

	for (; dwMask; dwMask >>= 1) dwRShiftL--;

	for (dwMask = dwGMask; dwMask && !(dwMask & 0x1); dwMask >>= 1) dwGShiftR++;

	for (; dwMask; dwMask >>= 1) dwGShiftL--;

	for (dwMask = dwBMask; dwMask && !(dwMask & 0x1); dwMask >>= 1) dwBShiftR++;

	for (; dwMask; dwMask >>= 1) dwBShiftL--;

	for (dwMask = dwAMask; dwMask && !(dwMask & 0x1); dwMask >>= 1) dwAShiftR++;

	for (; dwMask; dwMask >>= 1) dwAShiftL--;

	int iOldWidth	= ddsdSurfaceDesc.dwWidth;
	int iOldHeight	= ddsdSurfaceDesc.dwHeight;
	UINT iHaloWidth	= ddsdSurfaceDesc.dwWidth + _iNbCouche;
	UINT iHaloHeight = ddsdSurfaceDesc.dwHeight + _iNbCouche;

	if (ddDesc.dpcTriCaps.dwTextureCaps & D3DPTEXTURECAPS_POW2)
	{
		for (ddsdSurfaceDesc.dwWidth  = 1 ; iHaloWidth > ddsdSurfaceDesc.dwWidth ; ddsdSurfaceDesc.dwWidth  <<= 1);

		for (ddsdSurfaceDesc.dwHeight = 1 ; iHaloHeight > ddsdSurfaceDesc.dwHeight ; ddsdSurfaceDesc.dwHeight <<= 1);
	}

	if (ddDesc.dpcTriCaps.dwTextureCaps & D3DPTEXTURECAPS_SQUAREONLY)
	{
		if (ddsdSurfaceDesc.dwWidth > ddsdSurfaceDesc.dwHeight)
			ddsdSurfaceDesc.dwHeight = ddsdSurfaceDesc.dwWidth;
		else
			ddsdSurfaceDesc.dwWidth = ddsdSurfaceDesc.dwHeight;
	}

	iHaloWidth = ddsdSurfaceDesc.dwWidth;
	iHaloHeight = ddsdSurfaceDesc.dwHeight;

	LPDIRECTDRAWSURFACE7 pddsTempSurface;

	if (FAILED(pDD->CreateSurface(&ddsdSurfaceDesc, &pddsTempSurface, NULL)))
	{
		pDD->Release();
		return NULL;
	}

	_iDecalX = (float)((iHaloWidth - iOldWidth) >> 1);
	_iDecalY = (float)((iHaloHeight - iOldHeight) >> 1);

	if (FAILED(pddsTempSurface->BltFast((int)_iDecalX,
										(int)_iDecalY,
										m_pddsSurface,
										NULL,
										DDBLTFAST_NOCOLORKEY | DDBLTFAST_WAIT)))
	{
		pddsTempSurface->Release();
		pDD->Release();
		return NULL;
	}

	ddsdSurfaceDesc.dwSize = sizeof(ddsdSurfaceDesc);

	if (FAILED(pddsTempSurface->Lock(NULL,
									 &ddsdSurfaceDesc,
									 DDLOCK_WRITEONLY | DDLOCK_WAIT,
									 NULL)))
	{
		pddsTempSurface->Release();
		pDD->Release();
		return NULL;
	}

	int iNb = ddsdSurfaceDesc.dwWidth * ddsdSurfaceDesc.dwHeight;
	int iNb2 = iNb;

	if (ddsdSurfaceDesc.ddpfPixelFormat.dwRGBBitCount == 32)
	{
		unsigned int uiColorMask;
		uiColorMask =	((((int)(255.f - _fR)) >> dwRShiftL) << dwRShiftR) |
						((((int)(255.f - _fG)) >> dwGShiftL) << dwGShiftR) |
						((((int)(255.f - _fB)) >> dwBShiftL) << dwBShiftR) |
						(0xFF000000);

		unsigned int * puiMem = (unsigned int *)ddsdSurfaceDesc.lpSurface;

		while (iNb--)
		{
			if (*puiMem) *puiMem = uiColorMask;

			puiMem++;
		}

		char * pcMem;
		unsigned int uiColorCmp;
		unsigned int uiColorWrite;
		uiColorCmp = uiColorMask;

		while (_iNbCouche--)
		{
			uiColorWrite =	((((int)_fR) >> dwRShiftL) << dwRShiftR) |
							((((int)_fG) >> dwGShiftL) << dwGShiftR) |
							((((int)_fB) >> dwBShiftL) << dwBShiftR) |
							(0xFF000000);

			unsigned int * puiOneLineDown;
			unsigned int * puiOneLineUp;
			pcMem = (char *)ddsdSurfaceDesc.lpSurface;

			puiMem = (unsigned int *)pcMem;

			if (*puiMem == uiColorCmp)
			{
				puiOneLineUp = (unsigned int *)(((char *)puiMem) + ddsdSurfaceDesc.lPitch);

				if (!(*(puiMem + 1))) *(puiMem + 1) = uiColorWrite;

				if (!(*puiOneLineUp)) *puiOneLineUp = uiColorWrite;
			}

			puiMem++;

			for (UINT iX = 1 ; iX < (ddsdSurfaceDesc.dwWidth - 1) ; iX++)
			{
				if (*puiMem == uiColorCmp)
				{
					puiOneLineUp = (unsigned int *)(((char *) puiMem) + ddsdSurfaceDesc.lPitch);

					if (!(*(puiMem - 1))) *(puiMem - 1) = uiColorWrite;

					if (!(*(puiMem + 1))) *(puiMem + 1) = uiColorWrite;

					if (!(*puiOneLineUp)) *puiOneLineUp = uiColorWrite;
				}

				puiMem++;
			}

			if (*puiMem == uiColorCmp)
			{
				puiOneLineUp = (unsigned int *)(((char *)puiMem) + ddsdSurfaceDesc.lPitch);

				if (!(*(puiMem - 1))) *(puiMem - 1) = uiColorWrite;

				if (!(*puiOneLineUp)) *puiOneLineUp = uiColorWrite;
			}

			for (UINT iY = 1 ; iY < (ddsdSurfaceDesc.dwHeight - 1) ; iY++)
			{
				pcMem += ddsdSurfaceDesc.lPitch;
				puiMem = (unsigned int *)pcMem;

				if (*puiMem == uiColorCmp)
				{
					puiOneLineDown = (unsigned int *)(((char *)puiMem) - ddsdSurfaceDesc.lPitch);
					puiOneLineUp   = (unsigned int *)(((char *)puiMem) + ddsdSurfaceDesc.lPitch);

					if (!(*(puiMem + 1))) *(puiMem + 1) = uiColorWrite;

					if (!(*puiOneLineDown)) *puiOneLineDown = uiColorWrite;

					if (!(*puiOneLineUp)) *puiOneLineUp	 = uiColorWrite;
				}

				puiMem++;

				for (UINT iX = 1 ; iX < (ddsdSurfaceDesc.dwWidth - 1) ; iX++)
				{
					if (*puiMem == uiColorCmp)
					{
						puiOneLineDown	= (unsigned int *)(((char *)puiMem) - ddsdSurfaceDesc.lPitch);
						puiOneLineUp	= (unsigned int *)(((char *)puiMem) + ddsdSurfaceDesc.lPitch);

						if (!(*(puiMem - 1))) *(puiMem - 1) = uiColorWrite;

						if (!(*(puiMem + 1))) *(puiMem + 1) = uiColorWrite;

						if (!(*puiOneLineDown)) *puiOneLineDown = uiColorWrite;

						if (!(*puiOneLineUp)) *puiOneLineUp   = uiColorWrite;
					}

					puiMem++;
				}

				if (*puiMem == uiColorCmp)
				{
					puiOneLineDown = (unsigned int *)(((char *)puiMem) - ddsdSurfaceDesc.lPitch);
					puiOneLineUp = (unsigned int *)(((char *)puiMem) + ddsdSurfaceDesc.lPitch);

					if (!(*(puiMem - 1))) *(puiMem - 1) = uiColorWrite;

					if (!(*puiOneLineDown)) *puiOneLineDown = uiColorWrite;

					if (!(*puiOneLineUp)) *puiOneLineUp   = uiColorWrite;
				}
			}

			pcMem += ddsdSurfaceDesc.lPitch;
			puiMem = (unsigned int *)pcMem;

			if (*puiMem == uiColorCmp)
			{
				puiOneLineDown = (unsigned int *)(((char *)puiMem) - ddsdSurfaceDesc.lPitch);

				if (!(*(puiMem + 1))) *(puiMem + 1) = uiColorWrite;

				if (!(*puiOneLineDown)) *puiOneLineDown = uiColorWrite;
			}

			puiMem++;

			for (UINT iX = 1 ; iX < (ddsdSurfaceDesc.dwWidth - 1) ; iX++)
			{
				if (*puiMem == uiColorCmp)
				{
					puiOneLineDown = (unsigned int *)(((char *)puiMem) - ddsdSurfaceDesc.lPitch);

					if (!(*(puiMem - 1))) *(puiMem - 1) = uiColorWrite;

					if (!(*(puiMem + 1))) *(puiMem + 1) = uiColorWrite;

					if (!(*puiOneLineDown)) *puiOneLineDown = uiColorWrite;
				}

				puiMem++;
			}

			if (*puiMem == uiColorCmp)
			{
				puiOneLineDown = (unsigned int *)(((char *)puiMem) - ddsdSurfaceDesc.lPitch);

				if (!(*(puiMem - 1))) *(puiMem - 1) = uiColorWrite;

				if (!(*puiOneLineDown)) *puiOneLineDown = uiColorWrite;
			}

			uiColorCmp = uiColorWrite;
			_fR -= fDR;
			_fG -= fDG;
			_fB -= fDB;
		}

		puiMem = (unsigned int *)ddsdSurfaceDesc.lpSurface;

		while (iNb2--)
		{
			if (*puiMem == uiColorMask) *puiMem = 0;

			puiMem++;
		}
	}
	else
	{
		unsigned short usColorMask;

		unsigned int uiDec = ((((unsigned int)(255.f - _fR)) >> dwRShiftL) << dwRShiftR) |
							 ((((unsigned int)(255.f - _fG)) >> dwGShiftL) << dwGShiftR) |
							 ((((unsigned int)(255.f - _fB)) >> dwBShiftL) << dwBShiftR) |
							 (0x8000);
		assert(uiDec <= USHRT_MAX);
		usColorMask = static_cast<unsigned short>(uiDec);

		unsigned short * pusMem = (unsigned short *)ddsdSurfaceDesc.lpSurface;

		while (iNb--)
		{
			if (*pusMem) *pusMem = usColorMask;

			pusMem++;
		}

		char * pcMem;
		unsigned short usColorCmp;
		unsigned short usColorWrite;
		usColorCmp = usColorMask;

		while (_iNbCouche--)
		{
			int iColor = ((((int)_fR) >> dwRShiftL) << dwRShiftR) |
						 ((((int)_fG) >> dwGShiftL) << dwGShiftR) |
						 ((((int)_fB) >> dwBShiftL) << dwBShiftR) |
						 (0x8000);
			ARX_CHECK_USHORT(iColor);

			usColorWrite =	ARX_CLEAN_WARN_CAST_USHORT(iColor);

			unsigned short * pusOneLineDown;
			unsigned short * pusOneLineUp;
			pcMem = (char *)ddsdSurfaceDesc.lpSurface;

			pusMem = (unsigned short *)pcMem;

			if (*pusMem == usColorCmp)
			{
				pusOneLineUp = (unsigned short *)(((char *)pusMem) + ddsdSurfaceDesc.lPitch);

				if (!(*(pusMem + 1))) *(pusMem + 1) = usColorWrite;

				if (!(*pusOneLineUp)) *pusOneLineUp = usColorWrite;
			}

			pusMem++;

			for (UINT iX = 1 ; iX < (ddsdSurfaceDesc.dwWidth - 1) ; iX++)
			{
				if (*pusMem == usColorCmp)
				{
					pusOneLineUp = (unsigned short *)(((char *) pusMem) + ddsdSurfaceDesc.lPitch);

					if (!(*(pusMem - 1))) *(pusMem - 1) = usColorWrite;

					if (!(*(pusMem + 1))) *(pusMem + 1) = usColorWrite;

					if (!(*pusOneLineUp)) *pusOneLineUp   = usColorWrite;
				}

				pusMem++;
			}

			if (*pusMem == usColorCmp)
			{
				pusOneLineUp = (unsigned short *)(((char *)pusMem) + ddsdSurfaceDesc.lPitch);

				if (!(*(pusMem - 1))) *(pusMem - 1) = usColorWrite;

				if (!(*pusOneLineUp)) *pusOneLineUp = usColorWrite;
			}

			for (UINT iY = 1 ; iY < (ddsdSurfaceDesc.dwHeight - 1) ; iY++)
			{
				pcMem += ddsdSurfaceDesc.lPitch;
				pusMem = (unsigned short *)pcMem;

				if (*pusMem == usColorCmp)
				{
					pusOneLineDown = (unsigned short *)(((char *)pusMem) - ddsdSurfaceDesc.lPitch);
					pusOneLineUp   = (unsigned short *)(((char *)pusMem) + ddsdSurfaceDesc.lPitch);

					if (!(*(pusMem + 1))) *(pusMem + 1) = usColorWrite;

					if (!(*pusOneLineDown)) *pusOneLineDown = usColorWrite;

					if (!(*pusOneLineUp)) *pusOneLineUp = usColorWrite;
				}

				pusMem++;

				for (UINT iX = 1 ; iX < (ddsdSurfaceDesc.dwWidth - 1) ; iX++)
				{
					if (*pusMem == usColorCmp)
					{
						pusOneLineDown	= (unsigned short *)(((char *)pusMem) - ddsdSurfaceDesc.lPitch);
						pusOneLineUp	= (unsigned short *)(((char *)pusMem) + ddsdSurfaceDesc.lPitch);

						if (!(*(pusMem - 1))) *(pusMem - 1)	= usColorWrite;

						if (!(*(pusMem + 1))) *(pusMem + 1)	= usColorWrite;

						if (!(*pusOneLineDown)) *pusOneLineDown	= usColorWrite;

						if (!(*pusOneLineUp)) *pusOneLineUp	= usColorWrite;
					}

					pusMem++;
				}

				if (*pusMem == usColorCmp)
				{
					pusOneLineDown = (unsigned short *)(((char *)pusMem) - ddsdSurfaceDesc.lPitch);
					pusOneLineUp = (unsigned short *)(((char *)pusMem) + ddsdSurfaceDesc.lPitch);

					if (!(*(pusMem - 1))) *(pusMem - 1) = usColorWrite;

					if (!(*pusOneLineDown)) *pusOneLineDown = usColorWrite;

					if (!(*pusOneLineUp)) *pusOneLineUp = usColorWrite;
				}
			}

			pcMem += ddsdSurfaceDesc.lPitch;
			pusMem = (unsigned short *)pcMem;

			if (*pusMem == usColorCmp)
			{
				pusOneLineDown = (unsigned short *)(((char *)pusMem) - ddsdSurfaceDesc.lPitch);

				if (!(*(pusMem + 1))) *(pusMem + 1) = usColorWrite;

				if (!(*pusOneLineDown)) *pusOneLineDown = usColorWrite;
			}

			pusMem++;

			for (UINT iX = 1 ; iX < (ddsdSurfaceDesc.dwWidth - 1) ; iX++)
			{
				if (*pusMem == usColorCmp)
				{
					pusOneLineDown = (unsigned short *)(((char *)pusMem) - ddsdSurfaceDesc.lPitch);

					if (!(*(pusMem - 1))) *(pusMem - 1) = usColorWrite;

					if (!(*(pusMem + 1))) *(pusMem + 1) = usColorWrite;

					if (!(*pusOneLineDown)) *pusOneLineDown = usColorWrite;
				}

				pusMem++;
			}

			if (*pusMem == usColorCmp)
			{
				pusOneLineDown = (unsigned short *)(((char *)pusMem) - ddsdSurfaceDesc.lPitch);

				if (!(*(pusMem - 1))) *(pusMem - 1) = usColorWrite;

				if (!(*pusOneLineDown)) *pusOneLineDown = usColorWrite;
			}


			usColorCmp = usColorWrite;
			_fR -= fDR;
			_fG -= fDG;
			_fB -= fDB;
		}

		pusMem = (unsigned short *)ddsdSurfaceDesc.lpSurface;

		while (iNb2--)
		{
			if (*pusMem == usColorMask) *pusMem = 0;

			pusMem++;
		}
	}

	if (FAILED(pddsTempSurface->Unlock(NULL)))
	{
		pddsTempSurface->Release();
		pDD->Release();
		return NULL;
	}

	SmoothSurface(pddsTempSurface);
	SmoothSurface(pddsTempSurface);

	//on cree un bitmap
	void * pData = NULL;

	BITMAPINFO bmi;
	bmi.bmiHeader.biSize			= sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth			= iHaloWidth;
	bmi.bmiHeader.biHeight			= iHaloHeight;
	bmi.bmiHeader.biPlanes			= 1;
	bmi.bmiHeader.biBitCount		= 24;
	bmi.bmiHeader.biCompression		= BI_RGB;
	bmi.bmiHeader.biSizeImage		= iHaloWidth * iHaloHeight * 3;
	bmi.bmiHeader.biXPelsPerMeter	= 0;
	bmi.bmiHeader.biYPelsPerMeter	= 0;
	bmi.bmiHeader.biClrUsed			= 0;
	bmi.bmiHeader.biClrImportant	= 0;

	HDC memDC = CreateCompatibleDC(NULL);
	HBITMAP hBitmap = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &pData, NULL, 0);

	if (!hBitmap)
	{
		DeleteDC(memDC);
		pddsTempSurface->Release();
		pDD->Release();
		return NULL;
	}

	SelectObject(memDC, hBitmap);

	ddsdSurfaceDesc.dwSize = sizeof(ddsdSurfaceDesc);

	if (FAILED(pddsTempSurface->Lock(NULL,
									 &ddsdSurfaceDesc,
									 DDLOCK_WRITEONLY | DDLOCK_WAIT,
									 NULL)))
	{
		DeleteDC(memDC);
		DeleteObject(hBitmap);
		pddsTempSurface->Release();
		pDD->Release();
		return NULL;
	}

	ddsdSurfaceDesc.ddpfPixelFormat.dwRBitMask >>= dwRShiftR;
	ddsdSurfaceDesc.ddpfPixelFormat.dwGBitMask >>= dwGShiftR;
	ddsdSurfaceDesc.ddpfPixelFormat.dwBBitMask >>= dwBShiftR;
	dwRShiftL = dwGShiftL = dwBShiftL = 0;

	while (!(ddsdSurfaceDesc.ddpfPixelFormat.dwRBitMask & 0x80))
	{
		ddsdSurfaceDesc.ddpfPixelFormat.dwRBitMask <<= 1;
		dwRShiftL++;
	}

	while (!(ddsdSurfaceDesc.ddpfPixelFormat.dwGBitMask & 0x80))
	{
		ddsdSurfaceDesc.ddpfPixelFormat.dwGBitMask <<= 1;
		dwGShiftL++;
	}

	while (!(ddsdSurfaceDesc.ddpfPixelFormat.dwBBitMask & 0x80))
	{
		ddsdSurfaceDesc.ddpfPixelFormat.dwBBitMask <<= 1;
		dwBShiftL++;
	}

	unsigned char * pucData = (unsigned char *)pData;
	unsigned char usR, usG, usB;
	ddsdSurfaceDesc.lpSurface = (void *)(((char *)ddsdSurfaceDesc.lpSurface) + (ddsdSurfaceDesc.lPitch * (ddsdSurfaceDesc.dwHeight - 1)));

	if (ddsdSurfaceDesc.ddpfPixelFormat.dwRGBBitCount == 16)
	{
		for (UINT iY = 0 ; iY < iHaloHeight ; iY++)
		{
			unsigned short * pusMem = (unsigned short *) ddsdSurfaceDesc.lpSurface;

			for (UINT iX = 0 ; iX < iHaloWidth ; iX++)
			{
				usR = (unsigned char)((*pusMem >> dwRShiftR) << dwRShiftL);
				usG = (unsigned char)((*pusMem >> dwGShiftR) << dwGShiftL);
				usB = (unsigned char)((*pusMem >> dwBShiftR) << dwBShiftL);
				*pucData++ = usB;
				*pucData++ = usG;
				*pucData++ = usR;
				pusMem++;
			}

			ddsdSurfaceDesc.lpSurface = (void *)(((char *)ddsdSurfaceDesc.lpSurface) - ddsdSurfaceDesc.lPitch);
		}
	}
	else
	{
		for (UINT iY = 0 ; iY < iHaloHeight ; iY++)
		{
			unsigned int * puiMem = (unsigned int *)ddsdSurfaceDesc.lpSurface;

			for (UINT iX = 0 ; iX < iHaloWidth ; iX++)
			{
				usR = (unsigned char)((*puiMem >> dwRShiftR) << dwRShiftL);
				usG = (unsigned char)((*puiMem >> dwGShiftR) << dwGShiftL);
				usB = (unsigned char)((*puiMem >> dwBShiftR) << dwBShiftL);
				*pucData++ = usB;
				*pucData++ = usG;
				*pucData++ = usR;
				puiMem++;
			}

			ddsdSurfaceDesc.lpSurface = (void *)(((char *)ddsdSurfaceDesc.lpSurface) - ddsdSurfaceDesc.lPitch);
		}
	}

	if (FAILED(pddsTempSurface->Unlock(NULL)))
	{
		DeleteDC(memDC);
		DeleteObject(hBitmap);
		pddsTempSurface->Release();
		pDD->Release();
		return NULL;
	}

	std::string tText;
	tText = m_strName + "_H";
	TextureContainer * pTex;

	if (FAILED(D3DTextr_CreateEmptyTexture(
				   tText,
				   iHaloWidth,
				   iHaloHeight,
				   0,
				   0,
				   1)))
	{
		DeleteDC(memDC);
		DeleteObject(hBitmap);
		pddsTempSurface->Release();
		pDD->Release();
		return NULL;
	}


	pTex = FindTexture(tText);

	if (!pTex)
	{
		DeleteDC(memDC);
		DeleteObject(hBitmap);
		pddsTempSurface->Release();
		pDD->Release();
		return NULL;
	}

	pTex->m_hbmBitmap = hBitmap;

	DeleteDC(memDC);
	pddsTempSurface->Release();
	pDD->Release();

	pTex->Restore();
	ddsdSurfaceDesc.dwSize = sizeof(ddsdSurfaceDesc);
	pTex->m_pddsSurface->GetSurfaceDesc(&ddsdSurfaceDesc);
	return pTex;
}
