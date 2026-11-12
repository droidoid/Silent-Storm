#include "StdAfx.h"
#include "ImagePack.h"
#include "ImageMMP.h"
externA5 "C" { 
#include <s3tc.h>
}
namespace NImage
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddMipDXTN( CImageMMP *pDst, const CImage &src, NGfx::EPixelFormat format )
{
	float fWeights[] = { 0.309f, 0.609f, 0.082f, 0, 0, 0, 0, 0 };
	DWORD dwEncodeType = 0;
	// compose encoding type
	switch ( format )
	{
		case NGfx::CF_DXT1:
			dwEncodeType = S3TC_ENCODE_RGB_COLOR_KEY;
			break;
		case NGfx::CF_DXT2:
			dwEncodeType = S3TC_ENCODE_RGB_ALPHA_COMPARE | S3TC_ENCODE_ALPHA_EXPLICIT;
			break;
		case NGfx::CF_DXT3:
			dwEncodeType = S3TC_ENCODE_RGB_FULL | S3TC_ENCODE_ALPHA_EXPLICIT;
			break;
		case NGfx::CF_DXT4:
			dwEncodeType = S3TC_ENCODE_RGB_ALPHA_COMPARE | S3TC_ENCODE_ALPHA_INTERPOLATED;
			break;
		case NGfx::CF_DXT5:
			dwEncodeType = S3TC_ENCODE_RGB_FULL | S3TC_ENCODE_ALPHA_INTERPOLATED;
			break;
	}
	vector<DWORD> image;
	Convert( src, &image );
	// compose in header
	DDSURFACEDESC ddsdIn;
	Zero( ddsdIn );
	ddsdIn.dwSize = sizeof( DDSURFACEDESC );

	ddsdIn.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_LINEARSIZE | DDSD_PIXELFORMAT | DDSD_LPSURFACE;
	ddsdIn.dwWidth = src.GetXSize();//pImage->GetSizeX();
	ddsdIn.dwHeight = src.GetYSize();//pImage->GetSizeY();
	ddsdIn.lPitch = src.GetXSize() * 4;//pImage->GetSizeX() * 4;
	ddsdIn.lpSurface = &image[0];//const_cast<SColor*>( pImage->GetLFB() );

	ddsdIn.ddpfPixelFormat.dwSize = sizeof( DDPIXELFORMAT );
	ddsdIn.ddpfPixelFormat.dwRGBBitCount = 32;
	ddsdIn.ddpfPixelFormat.dwFlags = DDPF_ALPHAPIXELS | DDPF_RGB;
	ddsdIn.ddpfPixelFormat.dwRBitMask = 0x00FF0000;
	ddsdIn.ddpfPixelFormat.dwGBitMask = 0x0000FF00;
	ddsdIn.ddpfPixelFormat.dwBBitMask = 0x000000FF;
	ddsdIn.ddpfPixelFormat.dwRGBAlphaBitMask = 0xFF000000;
	// compose out header
	DDSURFACEDESC ddsdOut;
	Zero( ddsdOut );
	ddsdOut.dwSize = sizeof( DDSURFACEDESC );
	int nNumCompressedBytes = S3TCgetEncodeSize( &ddsdIn, dwEncodeType );
	std::vector<BYTE> outdata( nNumCompressedBytes );
	//
	S3TCsetAlphaReference( 0 );
	S3TCencode( &ddsdIn, 0, &ddsdOut, &(outdata[0]), dwEncodeType, fWeights );
	//
	//CImageMMP *pImageMMP = new CImageMMP( pImage->GetSizeX(), pImage->GetSizeY(), format, format );
	pDst->AddMipLevel( &(outdata[0]), outdata.size() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddMipRGBA( CImageMMP *pDst, const CImage &src, NGfx::EPixelFormat format )
{
	SPixelConvertInfo pci;
	switch ( format )
	{
		case NGfx::CF_R5G6B5:
			pci.InitMaskInfo( 0x00000000, 0x0000F800, 0x000007E0, 0x0000001F );
			break;
		case NGfx::CF_A1R5G5B5:
			pci.InitMaskInfo( 0x00008000, 0x00007C00, 0x000003E0, 0x0000001F );
			break;
		case NGfx::CF_A4R4G4B4:
			pci.InitMaskInfo( 0x0000F000, 0x00000F00, 0x000000F0, 0x0000000F );
			break;
		case NGfx::CF_A8R8G8B8:
			pci.InitMaskInfo( 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF );
			break;
		default:
			ASSERT( 0 );
			return;
	}
	//
	int nSizeX = src.GetXSize();//pImage->GetSizeX();
	int nSizeY = src.GetYSize();//pImage->GetSizeY();
	int nBPP = NGfx::GetBPP( format );
	int nBytePerPixel = nBPP / 8;
	std::vector<BYTE> outdata( nSizeX * nSizeY * nBytePerPixel );
	{
		BYTE *pBuffer = reinterpret_cast<BYTE*>( &( outdata[0] ) );
		for ( int y = 0; y < src.GetYSize(); ++y )
		{
			for ( int x = 0; x < src.GetXSize(); ++x )
			{
				DWORD dwConverted = pci.ComposeColorSlow( src[y][x] );
				memcpy( pBuffer, &dwConverted, nBytePerPixel );
				pBuffer += nBytePerPixel;
			}
		}
	}
	pDst->AddMipLevel( &(outdata[0]), outdata.size() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void AddMip( CImageMMP *pDst, const CImage &src, NGfx::EPixelFormat format )
{
	if ( (format >= NGfx::CF_DXT1) && (format <= NGfx::CF_DXT5) )
	{
		if ( src.GetXSize() < 4 || src.GetYSize() < 4 )
			return;
		AddMipDXTN( pDst, src, format );
	}
	AddMipRGBA( pDst, src, format );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static DWORD CalcAverageColor( const CImage &src )
{
	CVec4 vAvrg(0,0,0,0);
	for ( int y = 0; y < src.GetYSize(); ++y )
	{
		for ( int x = 0; x < src.GetXSize(); ++x )
			vAvrg += src[y][x];
	}
	vAvrg /= src.GetXSize() * src.GetYSize();
	return NGfx::GetDWORDColor( vAvrg );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CImageMMP* Pack( const SMippedImage &src, NGfx::EPixelFormat format )
{
	CImageMMP *pRes = new CImageMMP( src.levels[0].GetXSize(), src.levels[0].GetYSize(), format, CalcAverageColor( src.levels[0] ) );
	for ( int nMip = 0; nMip < src.levels.size(); ++nMip )
		AddMip( pRes, src.levels[nMip], format );
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
