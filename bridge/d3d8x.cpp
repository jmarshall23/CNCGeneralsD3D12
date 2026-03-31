#include "d3dx8.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifndef D3D_OK
#define D3D_OK S_OK
#endif

#ifndef D3DERR_INVALIDCALL
#define D3DERR_INVALIDCALL MAKE_HRESULT(1, 0x876, 2156)
#endif

#ifndef E_NOTIMPL
#define E_NOTIMPL 0x80004001L
#endif

// ------------------------------------------------------------
// Minimal internal ID3DXBuffer implementation
// ------------------------------------------------------------
class D3DXBufferImpl : public ID3DXBuffer
{
public:
  D3DXBufferImpl()
    : m_refCount(1), m_data(NULL), m_size(0)
  {
  }

  virtual ~D3DXBufferImpl()
  {
    if (m_data)
    {
      free(m_data);
      m_data = NULL;
    }
    m_size = 0;
  }

  HRESULT Init(const void* src, DWORD size)
  {
    if (size == 0)
    {
      m_data = NULL;
      m_size = 0;
      return S_OK;
    }

    m_data = malloc(size);
    if (!m_data)
      return E_OUTOFMEMORY;

    if (src)
      memcpy(m_data, src, size);
    else
      memset(m_data, 0, size);

    m_size = size;
    return S_OK;
  }

  STDMETHOD(QueryInterface)(REFIID, void** ppvObject)
  {
    if (!ppvObject)
      return E_POINTER;
    *ppvObject = NULL;
    return E_NOINTERFACE;
  }

  STDMETHOD_(ULONG, AddRef)()
  {
    return ++m_refCount;
  }

  STDMETHOD_(ULONG, Release)()
  {
    ULONG ref = --m_refCount;
    if (ref == 0)
      delete this;
    return ref;
  }

  STDMETHOD_(LPVOID, GetBufferPointer)()
  {
    return m_data;
  }

  STDMETHOD_(DWORD, GetBufferSize)()
  {
    return m_size;
  }

private:
  ULONG m_refCount;
  void* m_data;
  DWORD m_size;
};

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static float D3DX8_Determinant3x3(
  float a1, float a2, float a3,
  float b1, float b2, float b3,
  float c1, float c2, float c3)
{
  return
    a1 * (b2 * c3 - b3 * c2) -
    a2 * (b1 * c3 - b3 * c1) +
    a3 * (b1 * c2 - b2 * c1);
}

static D3DXMATRIX* D3DX8_MatrixIdentity(D3DXMATRIX* pOut)
{
  if (!pOut)
    return NULL;

  memset(pOut, 0, sizeof(D3DXMATRIX));
  pOut->_11 = 1.0f;
  pOut->_22 = 1.0f;
  pOut->_33 = 1.0f;
  pOut->_44 = 1.0f;
  return pOut;
}

  // ------------------------------------------------------------
  // Vector
  // ------------------------------------------------------------
  D3DXVECTOR4* WINAPI D3DXVec4Transform(
    D3DXVECTOR4* pOut,
    CONST D3DXVECTOR4* pV,
    CONST D3DXMATRIX* pM)
  {
    if (!pOut || !pV || !pM)
      return NULL;

    D3DXVECTOR4 v = *pV;
    D3DXVECTOR4 out;

    out.x = v.x * pM->_11 + v.y * pM->_21 + v.z * pM->_31 + v.w * pM->_41;
    out.y = v.x * pM->_12 + v.y * pM->_22 + v.z * pM->_32 + v.w * pM->_42;
    out.z = v.x * pM->_13 + v.y * pM->_23 + v.z * pM->_33 + v.w * pM->_43;
    out.w = v.x * pM->_14 + v.y * pM->_24 + v.z * pM->_34 + v.w * pM->_44;

    *pOut = out;
    return pOut;
  }

  // ------------------------------------------------------------
  // Matrix ops
  // ------------------------------------------------------------
  D3DXMATRIX* WINAPI D3DXMatrixMultiply(
    D3DXMATRIX* pOut,
    CONST D3DXMATRIX* pM1,
    CONST D3DXMATRIX* pM2)
  {
    if (!pOut || !pM1 || !pM2)
      return NULL;

    D3DXMATRIX a = *pM1;
    D3DXMATRIX b = *pM2;
    D3DXMATRIX r;

    r._11 = a._11 * b._11 + a._12 * b._21 + a._13 * b._31 + a._14 * b._41;
    r._12 = a._11 * b._12 + a._12 * b._22 + a._13 * b._32 + a._14 * b._42;
    r._13 = a._11 * b._13 + a._12 * b._23 + a._13 * b._33 + a._14 * b._43;
    r._14 = a._11 * b._14 + a._12 * b._24 + a._13 * b._34 + a._14 * b._44;

    r._21 = a._21 * b._11 + a._22 * b._21 + a._23 * b._31 + a._24 * b._41;
    r._22 = a._21 * b._12 + a._22 * b._22 + a._23 * b._32 + a._24 * b._42;
    r._23 = a._21 * b._13 + a._22 * b._23 + a._23 * b._33 + a._24 * b._43;
    r._24 = a._21 * b._14 + a._22 * b._24 + a._23 * b._34 + a._24 * b._44;

    r._31 = a._31 * b._11 + a._32 * b._21 + a._33 * b._31 + a._34 * b._41;
    r._32 = a._31 * b._12 + a._32 * b._22 + a._33 * b._32 + a._34 * b._42;
    r._33 = a._31 * b._13 + a._32 * b._23 + a._33 * b._33 + a._34 * b._43;
    r._34 = a._31 * b._14 + a._32 * b._24 + a._33 * b._34 + a._34 * b._44;

    r._41 = a._41 * b._11 + a._42 * b._21 + a._43 * b._31 + a._44 * b._41;
    r._42 = a._41 * b._12 + a._42 * b._22 + a._43 * b._32 + a._44 * b._42;
    r._43 = a._41 * b._13 + a._42 * b._23 + a._43 * b._33 + a._44 * b._43;
    r._44 = a._41 * b._14 + a._42 * b._24 + a._43 * b._34 + a._44 * b._44;

    *pOut = r;
    return pOut;
  }

  D3DXMATRIX* WINAPI D3DXMatrixTranspose(
    D3DXMATRIX* pOut,
    CONST D3DXMATRIX* pM)
  {
    if (!pOut || !pM)
      return NULL;

    D3DXMATRIX m = *pM;
    D3DXMATRIX r;

    r._11 = m._11; r._12 = m._21; r._13 = m._31; r._14 = m._41;
    r._21 = m._12; r._22 = m._22; r._23 = m._32; r._24 = m._42;
    r._31 = m._13; r._32 = m._23; r._33 = m._33; r._34 = m._43;
    r._41 = m._14; r._42 = m._24; r._43 = m._34; r._44 = m._44;

    *pOut = r;
    return pOut;
  }

  D3DXMATRIX* WINAPI D3DXMatrixScaling(
    D3DXMATRIX* pOut,
    FLOAT sx,
    FLOAT sy,
    FLOAT sz)
  {
    if (!pOut)
      return NULL;

    D3DX8_MatrixIdentity(pOut);
    pOut->_11 = sx;
    pOut->_22 = sy;
    pOut->_33 = sz;
    return pOut;
  }

  D3DXMATRIX* WINAPI D3DXMatrixTranslation(
    D3DXMATRIX* pOut,
    FLOAT x,
    FLOAT y,
    FLOAT z)
  {
    if (!pOut)
      return NULL;

    D3DX8_MatrixIdentity(pOut);
    pOut->_41 = x;
    pOut->_42 = y;
    pOut->_43 = z;
    return pOut;
  }

  D3DXMATRIX* WINAPI D3DXMatrixInverse(
    D3DXMATRIX* pOut,
    FLOAT* pDeterminant,
    CONST D3DXMATRIX* pM)
  {
    if (!pOut || !pM)
      return NULL;

    const D3DXMATRIX& m = *pM;

    float c11 = D3DX8_Determinant3x3(m._22, m._23, m._24, m._32, m._33, m._34, m._42, m._43, m._44);
    float c12 = -D3DX8_Determinant3x3(m._21, m._23, m._24, m._31, m._33, m._34, m._41, m._43, m._44);
    float c13 = D3DX8_Determinant3x3(m._21, m._22, m._24, m._31, m._32, m._34, m._41, m._42, m._44);
    float c14 = -D3DX8_Determinant3x3(m._21, m._22, m._23, m._31, m._32, m._33, m._41, m._42, m._43);

    float det = m._11 * c11 + m._12 * c12 + m._13 * c13 + m._14 * c14;

    if (pDeterminant)
      *pDeterminant = det;

    if (fabs(det) < 1.0e-8f)
      return NULL;

    float invDet = 1.0f / det;

    D3DXMATRIX r;

    r._11 = c11 * invDet;
    r._12 = -D3DX8_Determinant3x3(m._12, m._13, m._14, m._32, m._33, m._34, m._42, m._43, m._44) * invDet;
    r._13 = D3DX8_Determinant3x3(m._12, m._13, m._14, m._22, m._23, m._24, m._42, m._43, m._44) * invDet;
    r._14 = -D3DX8_Determinant3x3(m._12, m._13, m._14, m._22, m._23, m._24, m._32, m._33, m._34) * invDet;

    r._21 = c12 * invDet;
    r._22 = D3DX8_Determinant3x3(m._11, m._13, m._14, m._31, m._33, m._34, m._41, m._43, m._44) * invDet;
    r._23 = -D3DX8_Determinant3x3(m._11, m._13, m._14, m._21, m._23, m._24, m._41, m._43, m._44) * invDet;
    r._24 = D3DX8_Determinant3x3(m._11, m._13, m._14, m._21, m._23, m._24, m._31, m._33, m._34) * invDet;

    r._31 = c13 * invDet;
    r._32 = -D3DX8_Determinant3x3(m._11, m._12, m._14, m._31, m._32, m._34, m._41, m._42, m._44) * invDet;
    r._33 = D3DX8_Determinant3x3(m._11, m._12, m._14, m._21, m._22, m._24, m._41, m._42, m._44) * invDet;
    r._34 = -D3DX8_Determinant3x3(m._11, m._12, m._14, m._21, m._22, m._24, m._31, m._32, m._34) * invDet;

    r._41 = c14 * invDet;
    r._42 = D3DX8_Determinant3x3(m._11, m._12, m._13, m._31, m._32, m._33, m._41, m._42, m._43) * invDet;
    r._43 = -D3DX8_Determinant3x3(m._11, m._12, m._13, m._21, m._22, m._23, m._41, m._42, m._43) * invDet;
    r._44 = D3DX8_Determinant3x3(m._11, m._12, m._13, m._21, m._22, m._23, m._31, m._32, m._33) * invDet;

    *pOut = r;
    return pOut;
  }

  D3DXMATRIX* WINAPI D3DXMatrixRotationZ(
    D3DXMATRIX* pOut,
    FLOAT Angle)
  {
    if (!pOut)
      return NULL;

    float s = sinf(Angle);
    float c = cosf(Angle);

    D3DX8_MatrixIdentity(pOut);
    pOut->_11 = c;
    pOut->_12 = s;
    pOut->_21 = -s;
    pOut->_22 = c;
    return pOut;
  }

  D3DXVECTOR4* WINAPI D3DXVec3Transform(
    D3DXVECTOR4* pOut,
    CONST D3DXVECTOR3* pV,
    CONST D3DXMATRIX* pM)
  {
    if (!pOut || !pV || !pM)
      return NULL;

    D3DXVECTOR4 out;
    out.x = pV->x * pM->_11 + pV->y * pM->_21 + pV->z * pM->_31 + pM->_41;
    out.y = pV->x * pM->_12 + pV->y * pM->_22 + pV->z * pM->_32 + pM->_42;
    out.z = pV->x * pM->_13 + pV->y * pM->_23 + pV->z * pM->_33 + pM->_43;
    out.w = pV->x * pM->_14 + pV->y * pM->_24 + pV->z * pM->_34 + pM->_44;
    *pOut = out;
    return pOut;
  }

  UINT WINAPI D3DXGetFVFVertexSize(DWORD FVF)
  {
    UINT size = 0;

    switch (FVF & D3DFVF_POSITION_MASK)
    {
    case D3DFVF_XYZ:      size += 3 * sizeof(float); break;
    case D3DFVF_XYZRHW:   size += 4 * sizeof(float); break;
    case D3DFVF_XYZB1:    size += 4 * sizeof(float); break;
    case D3DFVF_XYZB2:    size += 5 * sizeof(float); break;
    case D3DFVF_XYZB3:    size += 6 * sizeof(float); break;
    case D3DFVF_XYZB4:    size += 7 * sizeof(float); break;
    case D3DFVF_XYZB5:    size += 8 * sizeof(float); break;
    default: break;
    }

    if (FVF & D3DFVF_NORMAL)
      size += 3 * sizeof(float);

    if (FVF & D3DFVF_PSIZE)
      size += sizeof(float);

    if (FVF & D3DFVF_DIFFUSE)
      size += sizeof(DWORD);

    if (FVF & D3DFVF_SPECULAR)
      size += sizeof(DWORD);

    UINT texCount = (FVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
    for (UINT i = 0; i < texCount; ++i)
    {
      DWORD fmt = (FVF >> (16 + i * 2)) & 0x3;
      switch (fmt)
      {
      default:
      case 0: size += 2 * sizeof(float); break; // default = 2D
      case 1: size += 3 * sizeof(float); break; // TEXCOORDSIZE3
      case 2: size += 4 * sizeof(float); break; // TEXCOORDSIZE4
      case 3: size += 1 * sizeof(float); break; // TEXCOORDSIZE1
      }
    }

    return size;
  }

  HRESULT WINAPI D3DXFilterTexture(
    LPDIRECT3DBASETEXTURE8 pBaseTexture,
    CONST PALETTEENTRY* pPalette,
    UINT SrcLevel,
    DWORD Filter)
  {
    (void)pBaseTexture;
    (void)pPalette;
    (void)SrcLevel;
    (void)Filter;

    // Stubbed out by request.
    return D3D_OK;
  }

  HRESULT WINAPI D3DXGetErrorStringA(HRESULT             hr,
    LPSTR               pBuffer,
    UINT                BufferLen)
  {
    return S_OK;
  }

  HRESULT WINAPI D3DXLoadSurfaceFromSurface(
    LPDIRECT3DSURFACE8 pDestSurface,
    CONST PALETTEENTRY* pDestPalette,
    CONST RECT* pDestRect,
    LPDIRECT3DSURFACE8 pSrcSurface,
    CONST PALETTEENTRY* pSrcPalette,
    CONST RECT* pSrcRect,
    DWORD Filter,
    D3DCOLOR ColorKey)
  {
    (void)pDestPalette;
    (void)pSrcPalette;
    (void)Filter;
    (void)ColorKey;

    if (!pDestSurface || !pSrcSurface)
      return D3DERR_INVALIDCALL;

    D3DSURFACE_DESC dstDesc;
    D3DSURFACE_DESC srcDesc;
    if (FAILED(pDestSurface->GetDesc(&dstDesc)) || FAILED(pSrcSurface->GetDesc(&srcDesc)))
      return D3DERR_INVALIDCALL;

    // For now keep this simple and safe:
    // - no format conversion
    // - no scaling/filtering
    // - no color keying
    if (dstDesc.Format != srcDesc.Format)
      return D3DERR_INVALIDCALL;

    RECT srcR = { 0, 0, (LONG)srcDesc.Width, (LONG)srcDesc.Height };
    RECT dstR = { 0, 0, (LONG)dstDesc.Width, (LONG)dstDesc.Height };

    if (pSrcRect) srcR = *pSrcRect;
    if (pDestRect) dstR = *pDestRect;

    // Clamp source rect to source surface.
    if (srcR.left < 0) srcR.left = 0;
    if (srcR.top < 0) srcR.top = 0;
    if (srcR.right > (LONG)srcDesc.Width)  srcR.right = (LONG)srcDesc.Width;
    if (srcR.bottom > (LONG)srcDesc.Height) srcR.bottom = (LONG)srcDesc.Height;

    // Clamp dest rect to dest surface.
    if (dstR.left < 0) dstR.left = 0;
    if (dstR.top < 0) dstR.top = 0;
    if (dstR.right > (LONG)dstDesc.Width)  dstR.right = (LONG)dstDesc.Width;
    if (dstR.bottom > (LONG)dstDesc.Height) dstR.bottom = (LONG)dstDesc.Height;

    LONG srcW = srcR.right - srcR.left;
    LONG srcH = srcR.bottom - srcR.top;
    LONG dstW = dstR.right - dstR.left;
    LONG dstH = dstR.bottom - dstR.top;

    if (srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0)
      return D3D_OK;

    // No scaling support yet, so copy only the overlapping area.
    LONG copyW = (srcW < dstW) ? srcW : dstW;
    LONG copyH = (srcH < dstH) ? srcH : dstH;

    // Figure out bytes-per-pixel for the formats your wrapper already supports.
    UINT bytesPerPixel = 0;
    switch (srcDesc.Format)
    {
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8:
    case D3DFMT_D24S8:
      bytesPerPixel = 4;
      break;

    case D3DFMT_R5G6B5:
    case D3DFMT_X1R5G5B5:
    case D3DFMT_A1R5G5B5:
    case D3DFMT_A4R4G4B4:
    case D3DFMT_D16:
      bytesPerPixel = 2;
      break;

    case D3DFMT_A8:
    case D3DFMT_L8:
      bytesPerPixel = 1;
      break;

    default:
      return D3DERR_INVALIDCALL;
    }

    D3DLOCKED_RECT srcLock;
    D3DLOCKED_RECT dstLock;

    RECT lockSrc = { srcR.left, srcR.top, srcR.left + copyW, srcR.top + copyH };
    RECT lockDst = { dstR.left, dstR.top, dstR.left + copyW, dstR.top + copyH };

    HRESULT hr = pSrcSurface->LockRect(&srcLock, &lockSrc, D3DLOCK_READONLY);
    if (FAILED(hr))
      return hr;

    hr = pDestSurface->LockRect(&dstLock, &lockDst, 0);
    if (FAILED(hr))
    {
      pSrcSurface->UnlockRect();
      return hr;
    }

    const BYTE* srcBase = static_cast<const BYTE*>(srcLock.pBits);
    BYTE* dstBase = static_cast<BYTE*>(dstLock.pBits);
    const UINT rowBytes = (UINT)copyW * bytesPerPixel;

    for (LONG y = 0; y < copyH; ++y)
    {
      memcpy(
        dstBase + y * dstLock.Pitch,
        srcBase + y * srcLock.Pitch,
        rowBytes);
    }

    pDestSurface->UnlockRect();
    pSrcSurface->UnlockRect();
    return D3D_OK;
  }

  HRESULT WINAPI D3DXCreateTexture(
    LPDIRECT3DDEVICE8 pDevice,
    UINT Width,
    UINT Height,
    UINT MipLevels,
    DWORD Usage,
    D3DFORMAT Format,
    D3DPOOL Pool,
    LPDIRECT3DTEXTURE8* ppTexture)
  {
    if (!pDevice || !ppTexture)
      return D3DERR_INVALIDCALL;

    *ppTexture = NULL;
    return pDevice->CreateTexture(Width, Height, MipLevels, Usage, Format, Pool, ppTexture);
  }

  HRESULT WINAPI D3DXCreateCubeTexture(
    LPDIRECT3DDEVICE8 pDevice,
    UINT EdgeLength,
    UINT MipLevels,
    DWORD Usage,
    D3DFORMAT Format,
    D3DPOOL Pool,
    LPDIRECT3DCUBETEXTURE8* ppCubeTexture)
  {
    if (!pDevice || !ppCubeTexture)
      return D3DERR_INVALIDCALL;

    *ppCubeTexture = NULL;
    return pDevice->CreateCubeTexture(EdgeLength, MipLevels, Usage, Format, Pool, ppCubeTexture);
  }

  HRESULT WINAPI D3DXCreateVolumeTexture(
    LPDIRECT3DDEVICE8 pDevice,
    UINT Width,
    UINT Height,
    UINT Depth,
    UINT MipLevels,
    DWORD Usage,
    D3DFORMAT Format,
    D3DPOOL Pool,
    LPDIRECT3DVOLUMETEXTURE8* ppVolumeTexture)
  {
    if (!pDevice || !ppVolumeTexture)
      return D3DERR_INVALIDCALL;

    *ppVolumeTexture = NULL;
    return pDevice->CreateVolumeTexture(Width, Height, Depth, MipLevels, Usage, Format, Pool, ppVolumeTexture);
  }

  HRESULT WINAPI D3DXCreateTextureFromFileExA(
    LPDIRECT3DDEVICE8 pDevice,
    LPCSTR pSrcFile,
    UINT Width,
    UINT Height,
    UINT MipLevels,
    DWORD Usage,
    D3DFORMAT Format,
    D3DPOOL Pool,
    DWORD Filter,
    DWORD MipFilter,
    D3DCOLOR ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DTEXTURE8* ppTexture)
  {
    (void)pSrcFile;
    (void)Filter;
    (void)MipFilter;
    (void)ColorKey;
    (void)pPalette;

    if (!pDevice || !ppTexture)
      return D3DERR_INVALIDCALL;

    *ppTexture = NULL;

    UINT createW = Width;
    UINT createH = Height;

    if (createW == D3DX_DEFAULT || createW == 0)
      createW = 4;
    if (createH == D3DX_DEFAULT || createH == 0)
      createH = 4;
    if (MipLevels == D3DX_DEFAULT)
      MipLevels = 1;
    if (Format == D3DFMT_UNKNOWN)
      Format = D3DFMT_A8R8G8B8;

    if (pSrcInfo)
    {
      memset(pSrcInfo, 0, sizeof(*pSrcInfo));
      pSrcInfo->Width = createW;
      pSrcInfo->Height = createH;
      pSrcInfo->Depth = 1;
      pSrcInfo->MipLevels = MipLevels;
      pSrcInfo->Format = Format;
      pSrcInfo->ResourceType = D3DRTYPE_TEXTURE;
      pSrcInfo->ImageFileFormat = D3DXIFF_BMP;
    }

    // Stubbed loader: creates a placeholder texture instead of actually decoding file data.
    return pDevice->CreateTexture(createW, createH, MipLevels, Usage, Format, Pool, ppTexture);
  }

  HRESULT WINAPI
    D3DXAssembleShader(
      LPCVOID               pSrcData,
      UINT                  SrcDataLen,
      DWORD                 Flags,
      LPD3DXBUFFER* ppConstants,
      LPD3DXBUFFER* ppCompiledShader,
      LPD3DXBUFFER* ppCompilationErrors) {

    return S_FALSE;
  }

