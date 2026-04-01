
#include "d3d8_bridge.h"
#include <algorithm>
#include <math.h>

static void QD3D8_SetGLMatrix(GLenum mode, const D3DMATRIX & m)
{
  glMatrixMode(mode);
  glLoadMatrixf(&m._11);
}

static GLenum QD3D8_TextureAddressToGL(DWORD a)
{
  switch (a)
  {
  case D3DTADDRESS_WRAP: return GL_REPEAT;
  case D3DTADDRESS_CLAMP: return GL_CLAMP_TO_EDGE;
  case D3DTADDRESS_BORDER: return GL_CLAMP;
  case D3DTADDRESS_MIRROR: return GL_REPEAT;
  default: return GL_REPEAT;
  }
}

static GLenum QD3D8_MinFilterToGL(DWORD minF, DWORD mipF)
{
  if (minF == D3DTEXF_POINT)
  {
    if (mipF == D3DTEXF_NONE) return GL_NEAREST;
    if (mipF == D3DTEXF_POINT) return GL_NEAREST_MIPMAP_NEAREST;
    return GL_NEAREST_MIPMAP_LINEAR;
  }

  if (mipF == D3DTEXF_NONE) return GL_LINEAR;
  if (mipF == D3DTEXF_POINT) return GL_LINEAR_MIPMAP_NEAREST;
  return GL_LINEAR_MIPMAP_LINEAR;
}

static GLenum QD3D8_MagFilterToGL(DWORD magF)
{
  return (magF == D3DTEXF_POINT) ? GL_NEAREST : GL_LINEAR;
}

UINT QD3D8_CalcBitsPerPixel(D3DFORMAT fmt)
{
  switch (fmt)
  {
  case D3DFMT_A8R8G8B8:
  case D3DFMT_X8R8G8B8:
  case D3DFMT_D24S8:
    return 32;
  case D3DFMT_R5G6B5:
  case D3DFMT_X1R5G5B5:
  case D3DFMT_A1R5G5B5:
  case D3DFMT_A4R4G4B4:
  case D3DFMT_D16:
    return 16;
  case D3DFMT_A8:
  case D3DFMT_L8:
    return 8;
  default:
    return 32;
  }
}

QGLFormatInfo QD3D8_TranslateFormat(D3DFORMAT fmt)
{
  QGLFormatInfo info;
  info.internalFormat = GL_RGBA8;
  info.format = GL_BGRA_EXT;
  info.type = GL_UNSIGNED_BYTE;
  info.bytesPerPixel = 4;
  info.supported = true;

  switch (fmt)
  {
  case D3DFMT_DXT1:
    info.internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
    info.format = GL_BGRA_EXT;
    info.type = GL_UNSIGNED_BYTE;
    info.bytesPerPixel = 4; // source upload is still RGBA/BGRA 32-bit
    break;
  case D3DFMT_DXT5:
    info.internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    info.format = GL_BGRA_EXT;
    info.type = GL_UNSIGNED_BYTE;
    info.bytesPerPixel = 4; // source data is still 4 bytes per pixel before compression
    break;
  case D3DFMT_A8R8G8B8:
    info.internalFormat = GL_RGBA8;
    info.format = GL_BGRA_EXT;
    info.type = GL_UNSIGNED_BYTE;
    info.bytesPerPixel = 4;
    break;
  case D3DFMT_X8R8G8B8:
    info.internalFormat = GL_RGBA8;
    info.format = GL_BGRA_EXT;
    info.type = GL_UNSIGNED_BYTE;
    info.bytesPerPixel = 4;
    break;
  case D3DFMT_R5G6B5:
    info.internalFormat = GL_RGB5;
    info.format = GL_RGB;
    info.type = GL_UNSIGNED_SHORT_5_6_5;
    info.bytesPerPixel = 2;
    break;
  case D3DFMT_X1R5G5B5:
    info.internalFormat = GL_RGB5;
    info.format = GL_BGRA_EXT;
    info.type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
    info.bytesPerPixel = 2;
    break;
  case D3DFMT_A1R5G5B5:
    info.internalFormat = GL_RGBA;
    info.format = GL_BGRA_EXT;
    info.type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
    info.bytesPerPixel = 2;
    break;
  case D3DFMT_A4R4G4B4:
    info.internalFormat = GL_RGBA4;
    info.format = GL_BGRA_EXT;
    info.type = GL_UNSIGNED_SHORT_4_4_4_4_REV;
    info.bytesPerPixel = 2;
    break;
  case D3DFMT_A8:
    info.internalFormat = GL_ALPHA8;
    info.format = GL_ALPHA;
    info.type = GL_UNSIGNED_BYTE;
    info.bytesPerPixel = 1;
    break;
  case D3DFMT_L8:
    info.internalFormat = GL_LUMINANCE8;
    info.format = GL_LUMINANCE;
    info.type = GL_UNSIGNED_BYTE;
    info.bytesPerPixel = 1;
    break;
  case D3DFMT_D16:
    info.internalFormat = GL_DEPTH_COMPONENT16;
    info.format = GL_DEPTH_COMPONENT;
    info.type = GL_UNSIGNED_SHORT;
    info.bytesPerPixel = 2;
    break;
  case D3DFMT_D24S8:
    info.internalFormat = GL_DEPTH_COMPONENT24;
    info.format = GL_DEPTH_COMPONENT;
    info.type = GL_UNSIGNED_INT;
    info.bytesPerPixel = 4;
    break;
  default:
    info.supported = false;
    break;
  }

  return info;
}

UINT QD3D8_CalcMipCount(UINT w, UINT h)
{
  UINT levels = 1;
  while (w > 1 || h > 1)
  {
    w = (w > 1) ? (w >> 1) : 1;
    h = (h > 1) ? (h >> 1) : 1;
    ++levels;
  }
  return levels;
}

void QD3D8_IdentityMatrix(D3DMATRIX* m)
{
  memset(m, 0, sizeof(*m));
  m->_11 = 1.0f;
  m->_22 = 1.0f;
  m->_33 = 1.0f;
  m->_44 = 1.0f;
}

D3DMATRIX QD3D8_MultiplyMatrix(const D3DMATRIX& a, const D3DMATRIX& b)
{
  D3DMATRIX o;
  const float* A = &a._11;
  const float* B = &b._11;
  float* O = &o._11;
  for (int r = 0; r < 4; ++r)
  {
    for (int c = 0; c < 4; ++c)
    {
      O[r * 4 + c] =
        A[r * 4 + 0] * B[0 * 4 + c] +
        A[r * 4 + 1] * B[1 * 4 + c] +
        A[r * 4 + 2] * B[2 * 4 + c] +
        A[r * 4 + 3] * B[3 * 4 + c];
    }
  }
  return o;
}

BOOL QD3D8_IsFVFHandle(DWORD handle)
{
  return (handle & D3DFVF_RESERVED0) != 0 ||
    (handle & D3DFVF_XYZ) != 0 ||
    (handle & D3DFVF_XYZRHW) != 0 ||
    (handle & D3DFVF_NORMAL) != 0 ||
    (handle & D3DFVF_DIFFUSE) != 0 ||
    (handle & D3DFVF_SPECULAR) != 0 ||
    ((handle & D3DFVF_TEXCOUNT_MASK) != 0);
}

HRESULT QD3D8ResourceBase::GetDeviceCommon(IDirect3DDevice8** ppDevice)
{
  if (!ppDevice || !m_device) return D3DERR_INVALIDCALL;
  *ppDevice = m_device;
  m_device->AddRef();
  return D3D_OK;
}

HRESULT QD3D8ResourceBase::SetPrivateDataCommon(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags)
{
  if (!pData || SizeOfData == 0) return D3DERR_INVALIDCALL;
  QD3D8PrivateData pd;
  pd.flags = Flags;
  pd.data.resize(SizeOfData);
  memcpy(&pd.data[0], pData, SizeOfData);
  m_privateData[refguid] = pd;
  return D3D_OK;
}

HRESULT QD3D8ResourceBase::GetPrivateDataCommon(REFGUID refguid, void* pData, DWORD* pSizeOfData)
{
  if (!pSizeOfData) return D3DERR_INVALIDCALL;
  std::map<GUID, QD3D8PrivateData, bool(*)(const GUID&, const GUID&)>::iterator it = m_privateData.find(refguid);
  if (it == m_privateData.end()) return D3DERR_NOTAVAILABLE;
  if (!pData || *pSizeOfData < it->second.data.size())
  {
    *pSizeOfData = (DWORD)it->second.data.size();
    return D3DERR_MOREDATA;
  }
  memcpy(pData, &it->second.data[0], it->second.data.size());
  *pSizeOfData = (DWORD)it->second.data.size();
  return D3D_OK;
}

HRESULT QD3D8ResourceBase::FreePrivateDataCommon(REFGUID refguid)
{
  std::map<GUID, QD3D8PrivateData, bool(*)(const GUID&, const GUID&)>::iterator it = m_privateData.find(refguid);
  if (it == m_privateData.end()) return D3DERR_NOTAVAILABLE;
  m_privateData.erase(it);
  return D3D_OK;
}

QD3D8Direct3D8::QD3D8Direct3D8() {}
QD3D8Direct3D8::~QD3D8Direct3D8() {}

STDMETHODIMP QD3D8Direct3D8::QueryInterface(REFIID riid, void** ppvObj)
{
  if (!ppvObj) return E_POINTER;
  *ppvObj = NULL;
  if (riid == IID_IUnknown || riid == IID_IDirect3D8)
  {
    *ppvObj = static_cast<IDirect3D8*>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}
STDMETHODIMP_(ULONG) QD3D8Direct3D8::AddRef() { return InternalAddRef(); }
STDMETHODIMP_(ULONG) QD3D8Direct3D8::Release() { return InternalRelease(); }
STDMETHODIMP QD3D8Direct3D8::RegisterSoftwareDevice(void*) { return D3D_OK; }
STDMETHODIMP_(UINT) QD3D8Direct3D8::GetAdapterCount() { return 1; }
STDMETHODIMP QD3D8Direct3D8::GetAdapterIdentifier(UINT Adapter, DWORD, D3DADAPTER_IDENTIFIER8* pIdentifier)
{
  if (!pIdentifier || Adapter != 0) return D3DERR_INVALIDCALL;
  memset(pIdentifier, 0, sizeof(*pIdentifier));
  lstrcpyA(pIdentifier->Driver, "opengl32.dll");
  lstrcpyA(pIdentifier->Description, "D3D8 OpenGL Compatibility Wrapper");
//  lstrcpyA(pIdentifier->DeviceName, "\\\\.\\DISPLAY1");
  pIdentifier->VendorId = 0;
  pIdentifier->DeviceId = 0;
  pIdentifier->SubSysId = 0;
  pIdentifier->Revision = 0;
  pIdentifier->DeviceIdentifier = IID_IDirect3D8;
  pIdentifier->WHQLLevel = 1;
  return D3D_OK;
}
STDMETHODIMP_(UINT) QD3D8Direct3D8::GetAdapterModeCount(UINT Adapter)
{
  UINT foundIndex = 0;

  DEVMODEA dm;
  ZeroMemory(&dm, sizeof(dm));
  dm.dmSize = sizeof(dm);

  for (DWORD win32Mode = 0; EnumDisplaySettingsA(NULL, win32Mode, &dm); ++win32Mode)
  {
    // Only keep modes with width/height/bpp information.
    if ((dm.dmFields & DM_PELSWIDTH) == 0 ||
      (dm.dmFields & DM_PELSHEIGHT) == 0 ||
      (dm.dmFields & DM_BITSPERPEL) == 0)
    {
      ZeroMemory(&dm, sizeof(dm));
      dm.dmSize = sizeof(dm);
      continue;
    }

    foundIndex++;
  }

  return foundIndex;
}
STDMETHODIMP QD3D8Direct3D8::EnumAdapterModes(UINT Adapter, UINT Mode, D3DDISPLAYMODE* pMode)
{
  if (!pMode || Adapter != 0)
    return D3DERR_INVALIDCALL;

  DEVMODEA dm;
  ZeroMemory(&dm, sizeof(dm));
  dm.dmSize = sizeof(dm);

  UINT foundIndex = 0;

  for (DWORD win32Mode = 0; EnumDisplaySettingsA(NULL, win32Mode, &dm); ++win32Mode)
  {
    // Only keep modes with width/height/bpp information.
    if ((dm.dmFields & DM_PELSWIDTH) == 0 ||
      (dm.dmFields & DM_PELSHEIGHT) == 0 ||
      (dm.dmFields & DM_BITSPERPEL) == 0)
    {
      ZeroMemory(&dm, sizeof(dm));
      dm.dmSize = sizeof(dm);
      continue;
    }

    D3DFORMAT format = D3DFMT_UNKNOWN;

    switch (dm.dmBitsPerPel)
    {
    case 16:
      // Safest common 16-bit fullscreen mode for old D3D8-era games.
      format = D3DFMT_R5G6B5;
      break;

    case 24:
      format = D3DFMT_R8G8B8;
      break;

    case 32:
      format = D3DFMT_A8R8G8B8;
      break;

    default:
      ZeroMemory(&dm, sizeof(dm));
      dm.dmSize = sizeof(dm);
      continue;
    }

    if (foundIndex == Mode)
    {
      pMode->Width = dm.dmPelsWidth;
      pMode->Height = dm.dmPelsHeight;
      pMode->RefreshRate = (dm.dmFields & DM_DISPLAYFREQUENCY) ? dm.dmDisplayFrequency : 0;
      pMode->Format = format;
      return D3D_OK;
    }

    ++foundIndex;

    ZeroMemory(&dm, sizeof(dm));
    dm.dmSize = sizeof(dm);
  }

  return D3DERR_INVALIDCALL;
}
STDMETHODIMP QD3D8Direct3D8::GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE* pMode)
{
  return EnumAdapterModes(Adapter, 0, pMode);
}
STDMETHODIMP QD3D8Direct3D8::CheckDeviceType(UINT Adapter, D3DDEVTYPE, D3DFORMAT DisplayFormat, D3DFORMAT BackBufferFormat, BOOL)
{
  if (Adapter != 0) return D3DERR_INVALIDCALL;
  if ((DisplayFormat == D3DFMT_X8R8G8B8 || DisplayFormat == D3DFMT_A8R8G8B8 || DisplayFormat == D3DFMT_R5G6B5) &&
    (BackBufferFormat == D3DFMT_X8R8G8B8 || BackBufferFormat == D3DFMT_A8R8G8B8 || BackBufferFormat == D3DFMT_R5G6B5))
    return D3D_OK;
  return D3DERR_NOTAVAILABLE;
}
STDMETHODIMP QD3D8Direct3D8::CheckDeviceFormat(UINT Adapter, D3DDEVTYPE, D3DFORMAT, DWORD, D3DRESOURCETYPE, D3DFORMAT CheckFormat)
{
  if (Adapter != 0) return D3DERR_INVALIDCALL;
  QGLFormatInfo fi = QD3D8_TranslateFormat(CheckFormat);
  return fi.supported ? D3D_OK : D3DERR_NOTAVAILABLE;
}
STDMETHODIMP QD3D8Direct3D8::CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE, D3DFORMAT, BOOL, D3DMULTISAMPLE_TYPE MultiSampleType)
{
  if (Adapter != 0) return D3DERR_INVALIDCALL;
  return (MultiSampleType == D3DMULTISAMPLE_NONE) ? D3D_OK : D3DERR_NOTAVAILABLE;
}
STDMETHODIMP QD3D8Direct3D8::CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, D3DFORMAT DepthStencilFormat)
{
  if (Adapter != 0) return D3DERR_INVALIDCALL;
  return QD3D8_TranslateFormat(DepthStencilFormat).supported ? D3D_OK : D3DERR_NOTAVAILABLE;
}
STDMETHODIMP QD3D8Direct3D8::GetDeviceCaps(UINT Adapter, D3DDEVTYPE, D3DCAPS8* pCaps)
{
  if (!pCaps || Adapter != 0) return D3DERR_INVALIDCALL;
  memset(pCaps, 0, sizeof(*pCaps));
  pCaps->DeviceType = D3DDEVTYPE_HAL;
  pCaps->AdapterOrdinal = 0;
  pCaps->Caps = D3DCAPS_READ_SCANLINE;
  pCaps->Caps2 = D3DCAPS2_CANRENDERWINDOWED | D3DCAPS2_FULLSCREENGAMMA;
  pCaps->Caps3 = 0;
  pCaps->PresentationIntervals = D3DPRESENT_INTERVAL_IMMEDIATE | D3DPRESENT_INTERVAL_ONE;
  pCaps->CursorCaps = D3DCURSORCAPS_COLOR | D3DCURSORCAPS_LOWRES;
  pCaps->DevCaps = D3DDEVCAPS_EXECUTESYSTEMMEMORY | D3DDEVCAPS_HWTRANSFORMANDLIGHT;
  pCaps->PrimitiveMiscCaps = D3DPMISCCAPS_CULLNONE | D3DPMISCCAPS_CULLCW | D3DPMISCCAPS_CULLCCW | D3DPMISCCAPS_COLORWRITEENABLE;
  pCaps->RasterCaps = D3DPRASTERCAPS_DITHER | D3DPRASTERCAPS_FOGVERTEX | D3DPRASTERCAPS_MIPMAPLODBIAS | D3DPRASTERCAPS_ZTEST | D3DPRASTERCAPS_ZBIAS;
  pCaps->ZCmpCaps = 0xFFFF;
  pCaps->SrcBlendCaps = 0xFFFF;
  pCaps->DestBlendCaps = 0xFFFF;
  pCaps->AlphaCmpCaps = 0xFFFF;
  pCaps->ShadeCaps = D3DPSHADECAPS_COLORGOURAUDRGB | D3DPSHADECAPS_SPECULARGOURAUDRGB | D3DPSHADECAPS_ALPHAGOURAUDBLEND;
  pCaps->TextureCaps = D3DPTEXTURECAPS_ALPHA | D3DPTEXTURECAPS_MIPMAP | D3DPTEXTURECAPS_PERSPECTIVE;
  pCaps->TextureFilterCaps = D3DPTFILTERCAPS_MINFPOINT | D3DPTFILTERCAPS_MINFLINEAR | D3DPTFILTERCAPS_MIPFPOINT | D3DPTFILTERCAPS_MIPFLINEAR | D3DPTFILTERCAPS_MAGFPOINT | D3DPTFILTERCAPS_MAGFLINEAR;
  pCaps->TextureAddressCaps = D3DPTADDRESSCAPS_WRAP | D3DPTADDRESSCAPS_CLAMP;
  pCaps->MaxTextureWidth = 4096;
  pCaps->MaxTextureHeight = 4096;
  pCaps->MaxVolumeExtent = 256;
  pCaps->MaxTextureRepeat = 8192;
  pCaps->MaxTextureAspectRatio = 4096;
  pCaps->MaxAnisotropy = 1;
  pCaps->MaxVertexW = 1e10f;
  pCaps->GuardBandLeft = -1e8f;
  pCaps->GuardBandTop = -1e8f;
  pCaps->GuardBandRight = 1e8f;
  pCaps->GuardBandBottom = 1e8f;
  pCaps->ExtentsAdjust = 0.0f;
  pCaps->StencilCaps = D3DSTENCILCAPS_KEEP | D3DSTENCILCAPS_REPLACE | D3DSTENCILCAPS_INCRSAT | D3DSTENCILCAPS_DECRSAT | D3DSTENCILCAPS_INVERT;
  pCaps->FVFCaps = 8;
  pCaps->TextureOpCaps = 0xFFFFFFFFu;
  pCaps->MaxTextureBlendStages = 8;
  pCaps->MaxSimultaneousTextures = 8;
  pCaps->VertexProcessingCaps = D3DVTXPCAPS_DIRECTIONALLIGHTS | D3DVTXPCAPS_POSITIONALLIGHTS | D3DVTXPCAPS_TEXGEN | D3DVTXPCAPS_MATERIALSOURCE7;
  pCaps->MaxActiveLights = 8;
  pCaps->MaxUserClipPlanes = 6;
  pCaps->MaxVertexBlendMatrices = 0;
  pCaps->MaxVertexBlendMatrixIndex = 0;
  pCaps->MaxPointSize = 64.0f;
  pCaps->MaxPrimitiveCount = 0xFFFFF;
  pCaps->MaxVertexIndex = 0xFFFFF;
  pCaps->MaxStreams = 16;
  pCaps->MaxStreamStride = 255;
  pCaps->VertexShaderVersion = D3DVS_VERSION(1, 1);
  pCaps->MaxVertexShaderConst = 96;
  pCaps->PixelShaderVersion = D3DPS_VERSION(1, 1);
  pCaps->MaxPixelShaderValue = 1.0f;
  return D3D_OK;
}
STDMETHODIMP_(HMONITOR) QD3D8Direct3D8::GetAdapterMonitor(UINT Adapter)
{
  return (Adapter == 0) ? MonitorFromPoint(POINT{ 0,0 }, MONITOR_DEFAULTTOPRIMARY) : NULL;
}
STDMETHODIMP QD3D8Direct3D8::CreateDevice(UINT Adapter, D3DDEVTYPE, HWND hFocusWindow, DWORD, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface)
{
  if (!ppReturnedDeviceInterface || !pPresentationParameters || Adapter != 0) return D3DERR_INVALIDCALL;
  *ppReturnedDeviceInterface = NULL;
  QD3D8Device8* dev = new QD3D8Device8(this, hFocusWindow, *pPresentationParameters);
  if (FAILED(dev->InitializeGL()))
  {
    dev->Release();
    return D3DERR_NOTAVAILABLE;
  }
  *ppReturnedDeviceInterface = dev;
  return D3D_OK;
}

// ---------------- Device ctor/dtor ----------------
QD3D8Device8::QD3D8Device8(QD3D8Direct3D8* parent, HWND hwnd, const D3DPRESENT_PARAMETERS& pp)
  : m_hwnd(hwnd), m_hdc(NULL), m_glrc(NULL), m_pp(pp), m_parent(parent), m_indices(NULL), m_baseVertexIndex(0),
  m_currentVertexShader(0), m_currentPixelShader(0), m_currentFVF(0), m_nextShaderHandle(1),
  m_backBuffer(NULL), m_depthBuffer(NULL), m_renderTarget(NULL), m_depthStencil(NULL), m_fbo(0), m_sceneBegun(false),
  m_currentPalette(0), m_cursorVisible(true)
{
  if (m_parent) m_parent->AddRef();
  memset(m_renderStates, 0, sizeof(m_renderStates));
  memset(m_boundTextures, 0, sizeof(m_boundTextures));
  memset(m_lightEnabled, 0, sizeof(m_lightEnabled));
  memset(m_clipPlanes, 0, sizeof(m_clipPlanes));
  QD3D8_IdentityMatrix(&m_world);
  QD3D8_IdentityMatrix(&m_view);
  QD3D8_IdentityMatrix(&m_proj);
  memset(&m_material, 0, sizeof(m_material));
  m_material.Diffuse.r = m_material.Diffuse.g = m_material.Diffuse.b = m_material.Diffuse.a = 1.0f;
  m_material.Ambient = m_material.Diffuse;
  m_material.Specular = m_material.Diffuse;
  m_material.Emissive.r = m_material.Emissive.g = m_material.Emissive.b = m_material.Emissive.a = 0.0f;
  m_material.Power = 0.0f;

  m_viewport.X = 0;
  m_viewport.Y = 0;
  m_viewport.Width = (DWORD)pp.BackBufferWidth;
  m_viewport.Height = (DWORD)pp.BackBufferHeight;
  m_viewport.MinZ = 0.0f;
  m_viewport.MaxZ = 1.0f;

  m_renderStates[D3DRS_ZENABLE] = TRUE;
  m_renderStates[D3DRS_ZWRITEENABLE] = TRUE;
  m_renderStates[D3DRS_ZFUNC] = D3DCMP_LESSEQUAL;
  m_renderStates[D3DRS_CULLMODE] = D3DCULL_CCW;
  m_renderStates[D3DRS_ALPHABLENDENABLE] = FALSE;
  m_renderStates[D3DRS_SRCBLEND] = D3DBLEND_ONE;
  m_renderStates[D3DRS_DESTBLEND] = D3DBLEND_ZERO;
  m_renderStates[D3DRS_ALPHATESTENABLE] = FALSE;
  m_renderStates[D3DRS_ALPHAFUNC] = D3DCMP_ALWAYS;
  m_renderStates[D3DRS_ALPHAREF] = 0;
  m_renderStates[D3DRS_LIGHTING] = FALSE;
  m_renderStates[D3DRS_FOGENABLE] = FALSE;
  m_renderStates[D3DRS_SPECULARENABLE] = FALSE;
  m_renderStates[D3DRS_FILLMODE] = D3DFILL_SOLID;
  m_renderStates[D3DRS_SHADEMODE] = D3DSHADE_GOURAUD;
}

QD3D8Device8::~QD3D8Device8()
{
  for (int i = 0; i < 8; ++i)
    if (m_boundTextures[i]) m_boundTextures[i]->Release();
  for (int i = 0; i < 16; ++i)
    if (m_streams[i].vb) m_streams[i].vb->Release();
  if (m_indices) m_indices->Release();
  if (m_backBuffer) m_backBuffer->Release();
  if (m_depthBuffer) m_depthBuffer->Release();
  if (m_renderTarget) m_renderTarget->Release();
  if (m_depthStencil) m_depthStencil->Release();
//  if (m_fbo) glDeleteFramebuffersEXT(1, &m_fbo);
  ShutdownGL();
  if (m_parent) m_parent->Release();
}

HRESULT QD3D8Device8::InitializeGL()
{
  m_hdc = GetDC(m_hwnd);
  if (!m_hdc) return D3DERR_NOTAVAILABLE;

  PIXELFORMATDESCRIPTOR pfd;
  memset(&pfd, 0, sizeof(pfd));
  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 32;
  pfd.cDepthBits = 24;
  pfd.cStencilBits = 8;
  pfd.iLayerType = PFD_MAIN_PLANE;

  int pf = ChoosePixelFormat(m_hdc, &pfd);
  if (!pf) return D3DERR_NOTAVAILABLE;
  if (!SetPixelFormat(m_hdc, pf, &pfd)) return D3DERR_NOTAVAILABLE;

  m_glrc = (HGLRC) wglCreateContext(m_hdc);
  if (!m_glrc) return D3DERR_NOTAVAILABLE;
  if (!wglMakeCurrent(m_hdc, m_glrc)) return D3DERR_NOTAVAILABLE;

  glViewport(0, 0, m_pp.BackBufferWidth, m_pp.BackBufferHeight);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  if (m_pp.BackBufferWidth == 0) m_pp.BackBufferWidth = 640;
  if (m_pp.BackBufferHeight == 0) m_pp.BackBufferHeight = 480;

  m_backBuffer = new QD3D8Surface8(this, m_pp.BackBufferWidth, m_pp.BackBufferHeight,
    m_pp.BackBufferFormat ? m_pp.BackBufferFormat : D3DFMT_X8R8G8B8, true, false, false);
  m_backBuffer->AddRef();
  m_renderTarget = m_backBuffer;
  m_renderTarget->AddRef();

  D3DFORMAT dsFmt = (m_pp.AutoDepthStencilFormat != D3DFMT_UNKNOWN) ? m_pp.AutoDepthStencilFormat : D3DFMT_D24S8;
  m_depthBuffer = new QD3D8Surface8(this, m_pp.BackBufferWidth, m_pp.BackBufferHeight, dsFmt, false, true, false);
  m_depthBuffer->AddRef();
  m_depthStencil = m_depthBuffer;
  m_depthStencil->AddRef();

  return D3D_OK;
}

void QD3D8Device8::ShutdownGL()
{
  if (m_glrc)
  {
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(m_glrc);
    m_glrc = NULL;
  }
  if (m_hdc)
  {
    ReleaseDC(m_hwnd, m_hdc);
    m_hdc = NULL;
  }
}

void QD3D8Device8::ApplyTransforms()
{
  D3DMATRIX wv = QD3D8_MultiplyMatrix(m_world, m_view);
  QD3D8_SetGLMatrix(GL_MODELVIEW, wv);
  QD3D8_SetGLMatrix(GL_PROJECTION, m_proj);
}

void QD3D8Device8::ApplyViewport()
{
  glViewport((GLint)m_viewport.X,
    (GLint)m_viewport.Y,
    (GLsizei)m_viewport.Width,
    (GLsizei)m_viewport.Height);
  glDepthRange(m_viewport.MinZ, m_viewport.MaxZ);
}

static GLenum QD3D8_CmpToGL(DWORD func)
{
  switch (func)
  {
  case D3DCMP_NEVER: return GL_NEVER;
  case D3DCMP_LESS: return GL_LESS;
  case D3DCMP_EQUAL: return GL_EQUAL;
  case D3DCMP_LESSEQUAL: return GL_LEQUAL;
  case D3DCMP_GREATER: return GL_GREATER;
  case D3DCMP_NOTEQUAL: return GL_NOTEQUAL;
  case D3DCMP_GREATEREQUAL: return GL_GEQUAL;
  case D3DCMP_ALWAYS: return GL_ALWAYS;
  default: return GL_ALWAYS;
  }
}

static GLenum QD3D8_BlendToGL(DWORD b)
{
  switch (b)
  {
  case D3DBLEND_ZERO: return GL_ZERO;
  case D3DBLEND_ONE: return GL_ONE;
  case D3DBLEND_SRCCOLOR: return GL_SRC_COLOR;
  case D3DBLEND_INVSRCCOLOR: return GL_ONE_MINUS_SRC_COLOR;
  case D3DBLEND_SRCALPHA: return GL_SRC_ALPHA;
  case D3DBLEND_INVSRCALPHA: return GL_ONE_MINUS_SRC_ALPHA;
  case D3DBLEND_DESTALPHA: return GL_DST_ALPHA;
  case D3DBLEND_INVDESTALPHA: return GL_ONE_MINUS_DST_ALPHA;
  case D3DBLEND_DESTCOLOR: return GL_DST_COLOR;
  case D3DBLEND_INVDESTCOLOR: return GL_ONE_MINUS_DST_COLOR;
  default: return GL_ONE;
  }
}

void QD3D8Device8::ApplyRenderStates()
{
  if (m_renderStates[D3DRS_ZENABLE]) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
  glDepthMask(m_renderStates[D3DRS_ZWRITEENABLE] ? GL_TRUE : GL_FALSE);
  glDepthFunc(QD3D8_CmpToGL(m_renderStates[D3DRS_ZFUNC]));

  switch (m_renderStates[D3DRS_CULLMODE])
  {
  case D3DCULL_NONE:
    glDisable(GL_CULL_FACE);
    break;
  case D3DCULL_CW:
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glCullFace(GL_FRONT);
    break;
  case D3DCULL_CCW:
  default:
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    break;
  }

  if (m_renderStates[D3DRS_ALPHABLENDENABLE])
  {
    glEnable(GL_BLEND);
    glBlendFunc(QD3D8_BlendToGL(m_renderStates[D3DRS_SRCBLEND]), QD3D8_BlendToGL(m_renderStates[D3DRS_DESTBLEND]));
  }
  else
  {
    glDisable(GL_BLEND);
  }

  switch (m_renderStates[D3DRS_FILLMODE])
  {
  case D3DFILL_POINT: glPolygonMode(GL_FRONT_AND_BACK, GL_POINT); break;
  case D3DFILL_WIREFRAME: glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); break;
  default: glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); break;
  }

  if (m_renderStates[D3DRS_LIGHTING]) glEnable(GL_LIGHTING); else glDisable(GL_LIGHTING);
  if (m_renderStates[D3DRS_FOGENABLE]) glEnable(GL_FOG); else glDisable(GL_FOG);
}

void QD3D8Device8::ApplyTextureStageStates()
{
  for (int i = 0; i < 8; ++i)
  {
    glActiveTextureARB(GL_TEXTURE0 + i);
    IDirect3DBaseTexture8* tex = m_boundTextures[i];
    if (!tex)
    {
      glDisable(GL_TEXTURE_2D);
     // glDisable(GL_TEXTURE_CUBE_MAP);
      continue;
    }

    QD3D8BaseTexture8* base = dynamic_cast<QD3D8BaseTexture8*>(tex);
    GLenum target = base->GetGLTarget();
    glEnable(target);
    glBindTexture(target, base->GetGLTex());

    glTexParameteri(target, GL_TEXTURE_WRAP_S, QD3D8_TextureAddressToGL(m_stageStates[i].addressU));
    glTexParameteri(target, GL_TEXTURE_WRAP_T, QD3D8_TextureAddressToGL(m_stageStates[i].addressV));
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, QD3D8_MinFilterToGL(m_stageStates[i].minFilter, m_stageStates[i].mipFilter));
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, QD3D8_MagFilterToGL(m_stageStates[i].magFilter));

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
      (m_stageStates[i].colorOp == D3DTOP_MODULATE) ? GL_MODULATE : GL_REPLACE);
  }
  glActiveTextureARB(GL_TEXTURE0);
  glEnable(GL_TEXTURE_2D);
}

void QD3D8Device8::DisableAllClientStates()
{
  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_NORMAL_ARRAY);
  glDisableClientState(GL_COLOR_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

HRESULT QD3D8Device8::SetupFVFStream(DWORD fvf, const uint8_t* base, UINT stride)
{
  if (!base || !stride) return D3DERR_INVALIDCALL;
  DisableAllClientStates();

  // Default vertex color when no D3DFVF_DIFFUSE is present.
  glColor4f(1, 1, 1, 1);

  size_t offset = 0;

  if (fvf & D3DFVF_XYZRHW)
  {
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(4, GL_FLOAT, stride, base + offset);
    offset += sizeof(float) * 4;
  }
  else if (fvf & D3DFVF_XYZ)
  {
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, stride, base + offset);
    offset += sizeof(float) * 3;
  }
  else
  {
    return D3DERR_INVALIDCALL;
  }

  if (fvf & D3DFVF_NORMAL)
  {
    glEnableClientState(GL_NORMAL_ARRAY);
    glNormalPointer(GL_FLOAT, stride, base + offset);
    offset += sizeof(float) * 3;
  }

  if (fvf & D3DFVF_DIFFUSE)
  {
    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(4, GL_UNSIGNED_BYTE, stride, base + offset);
    offset += sizeof(DWORD);
  }

  DWORD texCount = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
  if (texCount > 0)
  {
    glClientActiveTextureARB(GL_TEXTURE0);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, stride, base + offset);
  }

  return D3D_OK;
}

HRESULT QD3D8Device8::SetupStreamSources(INT baseVertexIndex)
{
  if (!m_streams[0].vb) return D3DERR_INVALIDCALL;

  QD3D8VertexBuffer8* vb = m_streams[0].vb;
  if (vb->m_data.empty()) return D3DERR_INVALIDCALL;

  UINT stride = m_streams[0].stride;
  const uint8_t* base = &vb->m_data[0];

  if (baseVertexIndex < 0) return D3DERR_INVALIDCALL; // for now
  base += baseVertexIndex * stride;

  return SetupFVFStream(m_currentFVF, base, stride);
}

HRESULT QD3D8Device8::UploadVB(QD3D8VertexBuffer8* vb)
{
  if (!vb) return D3DERR_INVALIDCALL;

  // No GL buffer-object upload in this shim path.
  // Keep the dirty bit semantics so Draw* paths still behave.
  vb->m_dirty = false;
  return D3D_OK;
}

HRESULT QD3D8Device8::UploadIB(QD3D8IndexBuffer8* ib)
{
  if (!ib) return D3DERR_INVALIDCALL;

  // No GL element-array buffer upload in this shim path.
  ib->m_dirty = false;
  return D3D_OK;
}

GLenum QD3D8Device8::TranslatePrimitiveType(D3DPRIMITIVETYPE pt, UINT, GLsizei* outCount) const
{
  if (outCount) *outCount = 0;
  switch (pt)
  {
  case D3DPT_POINTLIST: return GL_POINTS;
  case D3DPT_LINELIST: return GL_LINES;
  case D3DPT_LINESTRIP: return GL_LINE_STRIP;
  case D3DPT_TRIANGLELIST: return GL_TRIANGLES;
  case D3DPT_TRIANGLESTRIP: return GL_TRIANGLE_STRIP;
  case D3DPT_TRIANGLEFAN: return GL_TRIANGLE_FAN;
  default: return 0;
  }
}

UINT QD3D8Device8::PrimitiveVertexCount(D3DPRIMITIVETYPE pt, UINT primitiveCount) const
{
  switch (pt)
  {
  case D3DPT_POINTLIST: return primitiveCount;
  case D3DPT_LINELIST: return primitiveCount * 2;
  case D3DPT_LINESTRIP: return primitiveCount + 1;
  case D3DPT_TRIANGLELIST: return primitiveCount * 3;
  case D3DPT_TRIANGLESTRIP:
  case D3DPT_TRIANGLEFAN: return primitiveCount + 2;
  default: return 0;
  }
}

UINT QD3D8Device8::PrimitiveIndexCount(D3DPRIMITIVETYPE pt, UINT primitiveCount) const
{
  return PrimitiveVertexCount(pt, primitiveCount);
}

void QD3D8Device8::ApplyMaterialFixedFunction()
{
  GLfloat diff[4] = { m_material.Diffuse.r, m_material.Diffuse.g, m_material.Diffuse.b, m_material.Diffuse.a };
  GLfloat amb[4] = { m_material.Ambient.r, m_material.Ambient.g, m_material.Ambient.b, m_material.Ambient.a };
  GLfloat spec[4] = { m_material.Specular.r, m_material.Specular.g, m_material.Specular.b, m_material.Specular.a };
  GLfloat emis[4] = { m_material.Emissive.r, m_material.Emissive.g, m_material.Emissive.b, m_material.Emissive.a };
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diff);
  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, amb);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
  glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emis);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, m_material.Power);
}

void QD3D8Device8::ApplyLightsFixedFunction()
{
  for (int i = 0; i < 8; ++i)
  {
    GLenum lid = GL_LIGHT0 + i;
    if (!m_lightEnabled[i])
    {
      glDisable(lid);
      continue;
    }
    glEnable(lid);
    GLfloat pos[4] = { m_lights[i].Position.x, m_lights[i].Position.y, m_lights[i].Position.z,
        (m_lights[i].Type == D3DLIGHT_DIRECTIONAL) ? 0.0f : 1.0f };
    GLfloat diff[4] = { m_lights[i].Diffuse.r, m_lights[i].Diffuse.g, m_lights[i].Diffuse.b, m_lights[i].Diffuse.a };
    GLfloat amb[4] = { m_lights[i].Ambient.r, m_lights[i].Ambient.g, m_lights[i].Ambient.b, m_lights[i].Ambient.a };
    GLfloat spec[4] = { m_lights[i].Specular.r, m_lights[i].Specular.g, m_lights[i].Specular.b, m_lights[i].Specular.a };
    glLightfv(lid, GL_POSITION, pos);
    glLightfv(lid, GL_DIFFUSE, diff);
    glLightfv(lid, GL_AMBIENT, amb);
    glLightfv(lid, GL_SPECULAR, spec);
    glLightf(lid, GL_CONSTANT_ATTENUATION, m_lights[i].Attenuation0);
    glLightf(lid, GL_LINEAR_ATTENUATION, m_lights[i].Attenuation1);
    glLightf(lid, GL_QUADRATIC_ATTENUATION, m_lights[i].Attenuation2);
    glLightf(lid, GL_SPOT_CUTOFF, (m_lights[i].Type == D3DLIGHT_SPOT) ? (GLfloat)(m_lights[i].Phi * 57.2957795f) : 180.0f);
  }
}

// Remaining COM/resource methods follow the same pattern:
// - validate args
// - copy/store state
// - map to OpenGL fixed-function equivalents
// - stub unsupported shader-era features gracefully
//
// To keep this starter practical, the most important paths are fully implemented below,
// while less common interfaces return D3D_OK or D3DERR_NOTAVAILABLE instead of crashing.

STDMETHODIMP QD3D8Device8::QueryInterface(REFIID riid, void** ppvObj)
{
  if (!ppvObj) return E_POINTER;
  *ppvObj = NULL;
  if (riid == IID_IUnknown || riid == IID_IDirect3DDevice8)
  {
    *ppvObj = static_cast<IDirect3DDevice8*>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}
STDMETHODIMP_(ULONG) QD3D8Device8::AddRef() { return InternalAddRef(); }
STDMETHODIMP_(ULONG) QD3D8Device8::Release() { return InternalRelease(); }
STDMETHODIMP QD3D8Device8::TestCooperativeLevel() { return D3D_OK; }
STDMETHODIMP_(UINT) QD3D8Device8::GetAvailableTextureMem() { return 256 * 1024 * 1024; }
STDMETHODIMP QD3D8Device8::ResourceManagerDiscardBytes(DWORD) { return D3D_OK; }
STDMETHODIMP QD3D8Device8::GetDirect3D(IDirect3D8** ppD3D8)
{
  if (!ppD3D8 || !m_parent) return D3DERR_INVALIDCALL;
  *ppD3D8 = m_parent;
  m_parent->AddRef();
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::GetDeviceCaps(D3DCAPS8* pCaps) { return m_parent->GetDeviceCaps(0, D3DDEVTYPE_HAL, pCaps); }
STDMETHODIMP QD3D8Device8::GetDisplayMode(D3DDISPLAYMODE* pMode)
{
  if (!pMode) return D3DERR_INVALIDCALL;
  pMode->Width = m_pp.BackBufferWidth;
  pMode->Height = m_pp.BackBufferHeight;
  pMode->RefreshRate = 60;
  pMode->Format = m_pp.BackBufferFormat ? m_pp.BackBufferFormat : D3DFMT_X8R8G8B8;
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* pParameters)
{
  if (!pParameters) return D3DERR_INVALIDCALL;
  pParameters->AdapterOrdinal = 0;
  pParameters->DeviceType = D3DDEVTYPE_HAL;
  pParameters->hFocusWindow = m_hwnd;
  pParameters->BehaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::SetCursorProperties(UINT, UINT, IDirect3DSurface8*) { return D3D_OK; }
STDMETHODIMP_(void) QD3D8Device8::SetCursorPosition(UINT X, UINT Y, DWORD) { m_cursorPos.x = X; m_cursorPos.y = Y; }
STDMETHODIMP_(BOOL) QD3D8Device8::ShowCursor(BOOL bShow) { m_cursorVisible = !!bShow; return bShow; }
STDMETHODIMP QD3D8Device8::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS*, IDirect3DSwapChain8** pSwapChain)
{
  if (!pSwapChain) return D3DERR_INVALIDCALL;
  *pSwapChain = new QD3D8SwapChain8(this);
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters)
{
  if (!pPresentationParameters) return D3DERR_INVALIDCALL;
  m_pp = *pPresentationParameters;
  glViewport(0, 0, m_pp.BackBufferWidth, m_pp.BackBufferHeight);
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::Present(const RECT*, const RECT*, HWND, const RGNDATA*)
{
  glFlush();
  SwapBuffers(m_hdc);
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::GetBackBuffer(UINT BackBuffer, D3DBACKBUFFER_TYPE, IDirect3DSurface8** ppBackBuffer)
{
  if (!ppBackBuffer || BackBuffer != 0 || !m_backBuffer) return D3DERR_INVALIDCALL;
  *ppBackBuffer = m_backBuffer;
  m_backBuffer->AddRef();
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::GetRasterStatus(D3DRASTER_STATUS* pRasterStatus)
{
  if (!pRasterStatus) return D3DERR_INVALIDCALL;
  memset(pRasterStatus, 0, sizeof(*pRasterStatus));
  return D3D_OK;
}
STDMETHODIMP_(void) QD3D8Device8::SetGammaRamp(DWORD, const D3DGAMMARAMP*) {}
STDMETHODIMP_(void) QD3D8Device8::GetGammaRamp(D3DGAMMARAMP* pRamp) { if (pRamp) memset(pRamp, 0, sizeof(*pRamp)); }

STDMETHODIMP QD3D8Device8::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD, D3DFORMAT Format, D3DPOOL, IDirect3DTexture8** ppTexture)
{
  if (!ppTexture) return D3DERR_INVALIDCALL;
  if (Levels == 0) Levels = QD3D8_CalcMipCount(Width, Height);
  QD3D8Texture8* tex = new QD3D8Texture8(this, Width, Height, Levels, Format);
  if (FAILED(tex->Initialize()))
  {
    tex->Release();
    return D3DERR_NOTAVAILABLE;
  }
  *ppTexture = tex;
  return D3D_OK;
}

STDMETHODIMP QD3D8Device8::CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD, D3DFORMAT Format, D3DPOOL, IDirect3DVolumeTexture8** ppVolumeTexture)
{
  if (!ppVolumeTexture) return D3DERR_INVALIDCALL;
  *ppVolumeTexture = new QD3D8VolumeTexture8(this, Width, Height, Depth, Levels ? Levels : 1, Format);
  return D3D_OK;
}

STDMETHODIMP QD3D8Device8::CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD, D3DFORMAT Format, D3DPOOL, IDirect3DCubeTexture8** ppCubeTexture)
{
  if (!ppCubeTexture) return D3DERR_INVALIDCALL;
  *ppCubeTexture = new QD3D8CubeTexture8(this, EdgeLength, Levels ? Levels : 1, Format);
  return D3D_OK;
}

STDMETHODIMP QD3D8Device8::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer8** ppVertexBuffer)
{
  if (!ppVertexBuffer) return D3DERR_INVALIDCALL;
  *ppVertexBuffer = new QD3D8VertexBuffer8(this, Length, Usage, FVF, Pool);
  return D3D_OK;
}

STDMETHODIMP QD3D8Device8::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer8** ppIndexBuffer)
{
  if (!ppIndexBuffer) return D3DERR_INVALIDCALL;
  *ppIndexBuffer = new QD3D8IndexBuffer8(this, Length, Usage, Format, Pool);
  return D3D_OK;
}

STDMETHODIMP QD3D8Device8::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, BOOL, IDirect3DSurface8** ppSurface)
{
  if (!ppSurface || MultiSample != D3DMULTISAMPLE_NONE) return D3DERR_INVALIDCALL;
  QD3D8Surface8* s = new QD3D8Surface8(this, Width, Height, Format, true, false, true);
  if (FAILED(s->InitializeTextureBacked()))
  {
    s->Release();
    return D3DERR_NOTAVAILABLE;
  }
  *ppSurface = s;
  return D3D_OK;
}

STDMETHODIMP QD3D8Device8::CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, IDirect3DSurface8** ppSurface)
{
  if (!ppSurface || MultiSample != D3DMULTISAMPLE_NONE) return D3DERR_INVALIDCALL;
  QD3D8Surface8* s = new QD3D8Surface8(this, Width, Height, Format, false, true, true);
  if (FAILED(s->InitializeRenderbufferBacked()))
  {
    s->Release();
    return D3DERR_NOTAVAILABLE;
  }
  *ppSurface = s;
  return D3D_OK;
}

STDMETHODIMP QD3D8Device8::CreateImageSurface(UINT Width, UINT Height, D3DFORMAT Format, IDirect3DSurface8** ppSurface)
{
  if (!ppSurface) return D3DERR_INVALIDCALL;
  *ppSurface = new QD3D8Surface8(this, Width, Height, Format, false, false, true);
  return D3D_OK;
}

STDMETHODIMP QD3D8Device8::CopyRects(
  IDirect3DSurface8* pSourceSurface,
  const RECT* pSourceRectsArray,
  UINT cRects,
  IDirect3DSurface8* pDestinationSurface,
  const POINT* pDestPointsArray)
{
  if (!pSourceSurface || !pDestinationSurface)
    return D3DERR_INVALIDCALL;

  QD3D8Surface8* src = static_cast<QD3D8Surface8*>(pSourceSurface);
  QD3D8Surface8* dst = static_cast<QD3D8Surface8*>(pDestinationSurface);
  if (!src || !dst)
    return D3DERR_INVALIDCALL;

  if (!src->m_fmt.supported || !dst->m_fmt.supported)
    return D3DERR_INVALIDCALL;

  if (src->m_fmt.bytesPerPixel != dst->m_fmt.bytesPerPixel)
    return D3DERR_INVALIDCALL;

  RECT defaultSrcRect = {
      0, 0,
      (LONG)std::min(src->m_width,  dst->m_width),
      (LONG)std::min(src->m_height, dst->m_height)
  };
  POINT defaultDstPoint = { 0, 0 };

  if (cRects == 0 || !pSourceRectsArray || !pDestPointsArray)
  {
    pSourceRectsArray = &defaultSrcRect;
    pDestPointsArray = &defaultDstPoint;
    cRects = 1;
  }

  const UINT bpp = src->m_fmt.bytesPerPixel;

  for (UINT i = 0; i < cRects; ++i)
  {
    RECT sr = pSourceRectsArray[i];
    POINT dp = pDestPointsArray[i];

    // Basic source rect validation.
    if (sr.left < 0 || sr.top < 0 || sr.right <= sr.left || sr.bottom <= sr.top)
      return D3DERR_INVALIDCALL;
    if ((UINT)sr.right > src->m_width || (UINT)sr.bottom > src->m_height)
      return D3DERR_INVALIDCALL;
    if (dp.x < 0 || dp.y < 0)
      return D3DERR_INVALIDCALL;

    UINT copyW = (UINT)(sr.right - sr.left);
    UINT copyH = (UINT)(sr.bottom - sr.top);

    // Clip against destination bounds.
    if ((UINT)dp.x >= dst->m_width || (UINT)dp.y >= dst->m_height)
      continue;

    if ((UINT)dp.x + copyW > dst->m_width)
      copyW = dst->m_width - (UINT)dp.x;
    if ((UINT)dp.y + copyH > dst->m_height)
      copyH = dst->m_height - (UINT)dp.y;

    if (copyW == 0 || copyH == 0)
      continue;

    RECT dstLockRect = {
        dp.x,
        dp.y,
        dp.x + (LONG)copyW,
        dp.y + (LONG)copyH
    };

    D3DLOCKED_RECT dstLocked = {};
    HRESULT hr = dst->LockRect(&dstLocked, &dstLockRect, 0);
    if (FAILED(hr))
      return hr;

    const uint8_t* srcBase = src->m_sysmem.data();

    for (UINT y = 0; y < copyH; ++y)
    {
      const uint8_t* srow =
        srcBase + (((UINT)sr.top + y) * src->m_width + (UINT)sr.left) * bpp;

      uint8_t* drow =
        static_cast<uint8_t*>(dstLocked.pBits) + y * dstLocked.Pitch;

      memcpy(drow, srow, copyW * bpp);
    }

    hr = dst->UnlockRect();
    if (FAILED(hr))
      return hr;
  }

  return D3D_OK;
}

STDMETHODIMP QD3D8Device8::UpdateTexture(
  IDirect3DBaseTexture8* pSourceTexture,
  IDirect3DBaseTexture8* pDestinationTexture)
{
  if (!pSourceTexture || !pDestinationTexture)
    return D3DERR_INVALIDCALL;

  QD3D8BaseTexture8* srcBaseTex = static_cast<QD3D8BaseTexture8*>(pSourceTexture);
  QD3D8BaseTexture8* dstBaseTex = static_cast<QD3D8BaseTexture8*>(pDestinationTexture);
  if (!srcBaseTex || !dstBaseTex)
    return D3DERR_INVALIDCALL;

  QD3D8Texture8* src = dynamic_cast<QD3D8Texture8*>(srcBaseTex);
  QD3D8Texture8* dst = dynamic_cast<QD3D8Texture8*>(dstBaseTex);
  if (!src || !dst)
    return D3DERR_NOTAVAILABLE;

  const UINT levels = (UINT)std::min(src->m_surfaces.size(), dst->m_surfaces.size());

  for (UINT i = 0; i < levels; ++i)
  {
    QD3D8Surface8* srcSurf = src->m_surfaces[i];
    QD3D8Surface8* dstSurf = dst->m_surfaces[i];
    if (!srcSurf || !dstSurf)
      return D3DERR_INVALIDCALL;

    if (!srcSurf->m_fmt.supported || !dstSurf->m_fmt.supported)
      return D3DERR_INVALIDCALL;

    if (srcSurf->m_fmt.bytesPerPixel != dstSurf->m_fmt.bytesPerPixel)
      return D3DERR_INVALIDCALL;

    const UINT copyW = std::min(srcSurf->m_width, dstSurf->m_width);
    const UINT copyH = std::min(srcSurf->m_height, dstSurf->m_height);
    const UINT bpp = srcSurf->m_fmt.bytesPerPixel;

    RECT dstLockRect = {
        0, 0,
        (LONG)copyW,
        (LONG)copyH
    };

    D3DLOCKED_RECT dstLocked = {};
    HRESULT hr = dstSurf->LockRect(&dstLocked, &dstLockRect, 0);
    if (FAILED(hr))
      return hr;

    const uint8_t* srcBase = srcSurf->m_sysmem.data();

    for (UINT y = 0; y < copyH; ++y)
    {
      const uint8_t* srow = srcBase + (y * srcSurf->m_width * bpp);
      uint8_t* drow = static_cast<uint8_t*>(dstLocked.pBits) + y * dstLocked.Pitch;
      memcpy(drow, srow, copyW * bpp);
    }

    hr = dstSurf->UnlockRect();
    if (FAILED(hr))
      return hr;
  }

  return D3D_OK;
}

STDMETHODIMP QD3D8Device8::GetFrontBuffer(IDirect3DSurface8* pDestSurface)
{
  if (!pDestSurface) return D3DERR_INVALIDCALL;
  QD3D8Surface8* dst = static_cast<QD3D8Surface8*>(pDestSurface);
  glReadPixels(0, 0, dst->m_width, dst->m_height, dst->m_fmt.format, dst->m_fmt.type, dst->m_sysmem.empty() ? NULL : &dst->m_sysmem[0]);
  return D3D_OK;
}

STDMETHODIMP QD3D8Device8::SetRenderTarget(IDirect3DSurface8* pRenderTarget, IDirect3DSurface8* pNewZStencil)
{
  if (!pRenderTarget) return D3DERR_INVALIDCALL;
  if (m_renderTarget) m_renderTarget->Release();
  if (m_depthStencil) m_depthStencil->Release();
  m_renderTarget = static_cast<QD3D8Surface8*>(pRenderTarget); m_renderTarget->AddRef();
  m_depthStencil = static_cast<QD3D8Surface8*>(pNewZStencil ? pNewZStencil : m_depthBuffer); m_depthStencil->AddRef();
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::GetRenderTarget(IDirect3DSurface8** ppRenderTarget)
{
  if (!ppRenderTarget || !m_renderTarget) return D3DERR_INVALIDCALL;
  *ppRenderTarget = m_renderTarget; m_renderTarget->AddRef(); return D3D_OK;
}
STDMETHODIMP QD3D8Device8::GetDepthStencilSurface(IDirect3DSurface8** ppZStencilSurface)
{
  if (!ppZStencilSurface || !m_depthStencil) return D3DERR_INVALIDCALL;
  *ppZStencilSurface = m_depthStencil; m_depthStencil->AddRef(); return D3D_OK;
}
STDMETHODIMP QD3D8Device8::BeginScene() { m_sceneBegun = true; glEnable(GL_TEXTURE_2D); return D3D_OK; }
STDMETHODIMP QD3D8Device8::EndScene() { m_sceneBegun = false; return D3D_OK; }
STDMETHODIMP QD3D8Device8::Clear(DWORD Count, const D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
{
  GLbitfield mask = 0;
  if (Flags & D3DCLEAR_TARGET)
  {
    mask |= GL_COLOR_BUFFER_BIT;
    float a = ((Color >> 24) & 0xFF) / 255.0f;
    float r = ((Color >> 16) & 0xFF) / 255.0f;
    float g = ((Color >> 8) & 0xFF) / 255.0f;
    float b = ((Color >> 0) & 0xFF) / 255.0f;
    glClearColor(r, g, b, a);
  }
  if (Flags & D3DCLEAR_ZBUFFER) { mask |= GL_DEPTH_BUFFER_BIT; glClearDepth(Z); }
  if (Flags & D3DCLEAR_STENCIL) { mask |= GL_STENCIL_BUFFER_BIT; glClearStencil((GLint)Stencil); }

  if (Count && pRects)
  {
    glEnable(GL_SCISSOR_TEST);
    for (DWORD i = 0; i < Count; ++i)
    {
      const D3DRECT& r = pRects[i];
      glScissor(r.x1, r.y1, r.x2 - r.x1, r.y2 - r.y1);
      glClear(mask);
    }
    glDisable(GL_SCISSOR_TEST);
  }
  else glClear(mask);
  return D3D_OK;
}

STDMETHODIMP QD3D8Device8::SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix)
{
  if (!pMatrix) return D3DERR_INVALIDCALL;
  switch (State)
  {
  case D3DTS_WORLD: m_world = *pMatrix; break;
  case D3DTS_VIEW: m_view = *pMatrix; break;
  case D3DTS_PROJECTION: m_proj = *pMatrix; break;
  default: return D3DERR_NOTAVAILABLE;
  }
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix)
{
  if (!pMatrix) return D3DERR_INVALIDCALL;
  switch (State)
  {
  case D3DTS_WORLD: *pMatrix = m_world; break;
  case D3DTS_VIEW: *pMatrix = m_view; break;
  case D3DTS_PROJECTION: *pMatrix = m_proj; break;
  default: return D3DERR_NOTAVAILABLE;
  }
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::MultiplyTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix)
{
  if (!pMatrix) return D3DERR_INVALIDCALL;
  D3DMATRIX* target = NULL;
  switch (State)
  {
  case D3DTS_WORLD: target = &m_world; break;
  case D3DTS_VIEW: target = &m_view; break;
  case D3DTS_PROJECTION: target = &m_proj; break;
  default: return D3DERR_NOTAVAILABLE;
  }
  *target = QD3D8_MultiplyMatrix(*target, *pMatrix);
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::SetViewport(const D3DVIEWPORT8* pViewport) { if (!pViewport) return D3DERR_INVALIDCALL; m_viewport = *pViewport; return D3D_OK; }
STDMETHODIMP QD3D8Device8::GetViewport(D3DVIEWPORT8* pViewport) { if (!pViewport) return D3DERR_INVALIDCALL; *pViewport = m_viewport; return D3D_OK; }
STDMETHODIMP QD3D8Device8::SetMaterial(const D3DMATERIAL8* pMaterial) { if (!pMaterial) return D3DERR_INVALIDCALL; m_material = *pMaterial; return D3D_OK; }
STDMETHODIMP QD3D8Device8::GetMaterial(D3DMATERIAL8* pMaterial) { if (!pMaterial) return D3DERR_INVALIDCALL; *pMaterial = m_material; return D3D_OK; }
STDMETHODIMP QD3D8Device8::SetLight(DWORD Index, const D3DLIGHT8* pLight) { if (Index >= 8 || !pLight) return D3DERR_INVALIDCALL; m_lights[Index] = *pLight; return D3D_OK; }
STDMETHODIMP QD3D8Device8::GetLight(DWORD Index, D3DLIGHT8* pLight) { if (Index >= 8 || !pLight) return D3DERR_INVALIDCALL; *pLight = m_lights[Index]; return D3D_OK; }
STDMETHODIMP QD3D8Device8::LightEnable(DWORD Index, BOOL Enable) { if (Index >= 8) return D3DERR_INVALIDCALL; m_lightEnabled[Index] = Enable; return D3D_OK; }
STDMETHODIMP QD3D8Device8::GetLightEnable(DWORD Index, BOOL* pEnable) { if (Index >= 8 || !pEnable) return D3DERR_INVALIDCALL; *pEnable = m_lightEnabled[Index]; return D3D_OK; }
STDMETHODIMP QD3D8Device8::SetClipPlane(DWORD Index, const float* pPlane) { if (Index >= 6 || !pPlane) return D3DERR_INVALIDCALL; memcpy(m_clipPlanes[Index], pPlane, sizeof(float) * 4); return D3D_OK; }
STDMETHODIMP QD3D8Device8::GetClipPlane(DWORD Index, float* pPlane) { if (Index >= 6 || !pPlane) return D3DERR_INVALIDCALL; memcpy(pPlane, m_clipPlanes[Index], sizeof(float) * 4); return D3D_OK; }
STDMETHODIMP QD3D8Device8::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) { if ((UINT)State >= 256) return D3DERR_INVALIDCALL; m_renderStates[State] = Value; return D3D_OK; }
STDMETHODIMP QD3D8Device8::GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue) { if ((UINT)State >= 256 || !pValue) return D3DERR_INVALIDCALL; *pValue = m_renderStates[State]; return D3D_OK; }
STDMETHODIMP QD3D8Device8::BeginStateBlock() { return D3D_OK; }
STDMETHODIMP QD3D8Device8::EndStateBlock(DWORD* pToken) { if (pToken) *pToken = 1; return D3D_OK; }
STDMETHODIMP QD3D8Device8::ApplyStateBlock(DWORD) { return D3D_OK; }
STDMETHODIMP QD3D8Device8::CaptureStateBlock(DWORD) { return D3D_OK; }
STDMETHODIMP QD3D8Device8::DeleteStateBlock(DWORD) { return D3D_OK; }
STDMETHODIMP QD3D8Device8::CreateStateBlock(D3DSTATEBLOCKTYPE, DWORD* pToken) { if (pToken) *pToken = 1; return D3D_OK; }
STDMETHODIMP QD3D8Device8::SetClipStatus(const D3DCLIPSTATUS8* pClipStatus) { if (!pClipStatus) return D3DERR_INVALIDCALL; m_clipStatus = *pClipStatus; return D3D_OK; }
STDMETHODIMP QD3D8Device8::GetClipStatus(D3DCLIPSTATUS8* pClipStatus) { if (!pClipStatus) return D3DERR_INVALIDCALL; *pClipStatus = m_clipStatus; return D3D_OK; }
STDMETHODIMP QD3D8Device8::GetTexture(DWORD Stage, IDirect3DBaseTexture8** ppTexture)
{
  if (Stage >= 8 || !ppTexture) return D3DERR_INVALIDCALL;
  *ppTexture = m_boundTextures[Stage];
  if (*ppTexture) (*ppTexture)->AddRef();
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::SetTexture(DWORD Stage, IDirect3DBaseTexture8* pTexture)
{
  if (Stage >= 8) return D3DERR_INVALIDCALL;
  if (m_boundTextures[Stage])
    m_boundTextures[Stage]->Release();
  m_boundTextures[Stage] = pTexture;
  if (pTexture) pTexture->AddRef();
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue)
{
  if (Stage >= 8 || !pValue) return D3DERR_INVALIDCALL;
  QD3D8TextureStageState& s = m_stageStates[Stage];
  switch (Type)
  {
  case D3DTSS_COLOROP: *pValue = s.colorOp; break;
  case D3DTSS_COLORARG1: *pValue = s.colorArg1; break;
  case D3DTSS_COLORARG2: *pValue = s.colorArg2; break;
  case D3DTSS_ALPHAOP: *pValue = s.alphaOp; break;
  case D3DTSS_ALPHAARG1: *pValue = s.alphaArg1; break;
  case D3DTSS_ALPHAARG2: *pValue = s.alphaArg2; break;
  case D3DTSS_TEXCOORDINDEX: *pValue = s.texCoordIndex; break;
  case D3DTSS_ADDRESSU: *pValue = s.addressU; break;
  case D3DTSS_ADDRESSV: *pValue = s.addressV; break;
  case D3DTSS_MAGFILTER: *pValue = s.magFilter; break;
  case D3DTSS_MINFILTER: *pValue = s.minFilter; break;
  case D3DTSS_MIPFILTER: *pValue = s.mipFilter; break;
  default: return D3DERR_NOTAVAILABLE;
  }
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
  if (Stage >= 8) return D3DERR_INVALIDCALL;
  QD3D8TextureStageState& s = m_stageStates[Stage];
  switch (Type)
  {
  case D3DTSS_COLOROP: s.colorOp = Value; break;
  case D3DTSS_COLORARG1: s.colorArg1 = Value; break;
  case D3DTSS_COLORARG2: s.colorArg2 = Value; break;
  case D3DTSS_ALPHAOP: s.alphaOp = Value; break;
  case D3DTSS_ALPHAARG1: s.alphaArg1 = Value; break;
  case D3DTSS_ALPHAARG2: s.alphaArg2 = Value; break;
  case D3DTSS_TEXCOORDINDEX: s.texCoordIndex = Value; break;
  case D3DTSS_ADDRESSU: s.addressU = Value; break;
  case D3DTSS_ADDRESSV: s.addressV = Value; break;
  case D3DTSS_MAGFILTER: s.magFilter = Value; break;
  case D3DTSS_MINFILTER: s.minFilter = Value; break;
  case D3DTSS_MIPFILTER: s.mipFilter = Value; break;

  case D3DTSS_TEXTURETRANSFORMFLAGS: break; // jmarshall: TODO!
  default: return D3DERR_NOTAVAILABLE;
  }
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::ValidateDevice(DWORD* pNumPasses) { if (pNumPasses) *pNumPasses = 1; return D3D_OK; }
STDMETHODIMP QD3D8Device8::GetInfo(DWORD, void*, DWORD) { return D3DERR_NOTAVAILABLE; }
STDMETHODIMP QD3D8Device8::SetPaletteEntries(UINT PaletteNumber, const PALETTEENTRY* pEntries)
{
  if (!pEntries || PaletteNumber >= 256) return D3DERR_INVALIDCALL;
  memcpy(m_palettes[PaletteNumber], pEntries, sizeof(PALETTEENTRY) * 256);
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries)
{
  if (!pEntries || PaletteNumber >= 256) return D3DERR_INVALIDCALL;
  memcpy(pEntries, m_palettes[PaletteNumber], sizeof(PALETTEENTRY) * 256);
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::SetCurrentTexturePalette(UINT PaletteNumber) { if (PaletteNumber >= 256) return D3DERR_INVALIDCALL; m_currentPalette = PaletteNumber; return D3D_OK; }
STDMETHODIMP QD3D8Device8::GetCurrentTexturePalette(UINT* PaletteNumber) { if (!PaletteNumber) return D3DERR_INVALIDCALL; *PaletteNumber = m_currentPalette; return D3D_OK; }

STDMETHODIMP QD3D8Device8::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
{
  GLenum mode = TranslatePrimitiveType(PrimitiveType, PrimitiveCount, NULL);
  if (!mode) return D3DERR_INVALIDCALL;
  if (FAILED(SetupStreamSources((INT)m_baseVertexIndex))) return D3DERR_INVALIDCALL;
  ApplyRenderStates();
  ApplyTransforms();
  ApplyViewport();
  ApplyMaterialFixedFunction();
  ApplyLightsFixedFunction();
  ApplyTextureStageStates();
//  glDrawArrays(mode, StartVertex, PrimitiveVertexCount(PrimitiveType, PrimitiveCount));
  return D3D_OK;
}

STDMETHODIMP QD3D8Device8::DrawIndexedPrimitive(
  D3DPRIMITIVETYPE PrimitiveType,
  UINT,
  UINT,
  UINT startIndex,
  UINT primCount)
{
  if (!m_indices) return D3DERR_INVALIDCALL;

  GLenum mode = TranslatePrimitiveType(PrimitiveType, primCount, NULL);
  if (!mode) return D3DERR_INVALIDCALL;

  if (FAILED(SetupStreamSources((INT)m_baseVertexIndex))) return D3DERR_INVALIDCALL;

  if (m_indices->m_dirty)
    UploadIB(m_indices);

  if (m_indices->m_data.empty())
    return D3DERR_INVALIDCALL;

  ApplyRenderStates();
  ApplyTransforms();
  ApplyViewport();
  ApplyMaterialFixedFunction();
  ApplyLightsFixedFunction();
  ApplyTextureStageStates();

  GLenum idxType = (m_indices->m_format == D3DFMT_INDEX16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
  uint32_t idxSize = (m_indices->m_format == D3DFMT_INDEX16) ? 2 : 4;

  const uint8_t* indexBase = &m_indices->m_data[0];
  const void* indexPtr = indexBase + (startIndex * idxSize);

  glDrawElements(
    mode,
    PrimitiveIndexCount(PrimitiveType, primCount),
    idxType,
    indexPtr);

  return D3D_OK;
}

STDMETHODIMP QD3D8Device8::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
  GLenum mode = TranslatePrimitiveType(PrimitiveType, PrimitiveCount, NULL);
  if (!mode || !pVertexStreamZeroData) return D3DERR_INVALIDCALL;
  SetupFVFStream(m_currentFVF, (const uint8_t*)pVertexStreamZeroData, VertexStreamZeroStride);
  ApplyRenderStates(); ApplyTransforms(); ApplyViewport(); ApplyTextureStageStates();
//  glDrawArrays(mode, 0, PrimitiveVertexCount(PrimitiveType, PrimitiveCount));
  return D3D_OK;
}

STDMETHODIMP QD3D8Device8::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT, UINT, UINT PrimitiveCount, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
  GLenum mode = TranslatePrimitiveType(PrimitiveType, PrimitiveCount, NULL);
  if (!mode || !pIndexData || !pVertexStreamZeroData) return D3DERR_INVALIDCALL;
  SetupFVFStream(m_currentFVF, (const uint8_t*)pVertexStreamZeroData, VertexStreamZeroStride);
  ApplyRenderStates(); ApplyTransforms(); ApplyViewport(); ApplyTextureStageStates();
  GLenum idxType = (IndexDataFormat == D3DFMT_INDEX16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
  glDrawElements(mode, PrimitiveIndexCount(PrimitiveType, PrimitiveCount), idxType, pIndexData);
  return D3D_OK;
}

STDMETHODIMP QD3D8Device8::ProcessVertices(UINT, UINT, UINT, IDirect3DVertexBuffer8*, DWORD) { return D3DERR_NOTAVAILABLE; }
STDMETHODIMP QD3D8Device8::CreateVertexShader(const DWORD* pDeclaration, const DWORD* pFunction, DWORD* pHandle, DWORD)
{
  if (!pHandle) return D3DERR_INVALIDCALL;
  ShaderStub s;
  if (pDeclaration)
  {
    const DWORD* p = pDeclaration;
    while (*p != D3DVSD_END()) { s.declaration.push_back(*p++); }
    s.declaration.push_back(D3DVSD_END());
  }
  if (pFunction)
  {
    const DWORD* p = pFunction;
    for (int i = 0; i < 256 && p[i] != 0x0000FFFF; ++i) s.function.push_back(p[i]);
  }
  DWORD h = m_nextShaderHandle++;
  m_vertexShaders[h] = s;
  *pHandle = h;
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::SetVertexShader(DWORD Handle) { m_currentVertexShader = Handle; if (QD3D8_IsFVFHandle(Handle)) m_currentFVF = Handle; return D3D_OK; }
STDMETHODIMP QD3D8Device8::GetVertexShader(DWORD* pHandle) { if (!pHandle) return D3DERR_INVALIDCALL; *pHandle = m_currentVertexShader; return D3D_OK; }
STDMETHODIMP QD3D8Device8::DeleteVertexShader(DWORD Handle) { m_vertexShaders.erase(Handle); return D3D_OK; }
STDMETHODIMP QD3D8Device8::SetVertexShaderConstant(DWORD, const void*, DWORD) { return D3D_OK; }
STDMETHODIMP QD3D8Device8::GetVertexShaderConstant(DWORD, void*, DWORD) { return D3D_OK; }
STDMETHODIMP QD3D8Device8::GetVertexShaderDeclaration(DWORD Handle, void* pData, DWORD* pSizeOfData)
{
  if (!pSizeOfData) return D3DERR_INVALIDCALL;
  std::map<DWORD, ShaderStub>::iterator it = m_vertexShaders.find(Handle);
  if (it == m_vertexShaders.end()) return D3DERR_INVALIDCALL;
  DWORD bytes = (DWORD)(it->second.declaration.size() * sizeof(DWORD));
  if (!pData || *pSizeOfData < bytes) { *pSizeOfData = bytes; return D3DERR_MOREDATA; }
  memcpy(pData, &it->second.declaration[0], bytes); *pSizeOfData = bytes; return D3D_OK;
}
STDMETHODIMP QD3D8Device8::GetVertexShaderFunction(DWORD Handle, void* pData, DWORD* pSizeOfData)
{
  if (!pSizeOfData) return D3DERR_INVALIDCALL;
  std::map<DWORD, ShaderStub>::iterator it = m_vertexShaders.find(Handle);
  if (it == m_vertexShaders.end()) return D3DERR_INVALIDCALL;
  DWORD bytes = (DWORD)(it->second.function.size() * sizeof(DWORD));
  if (!pData || *pSizeOfData < bytes) { *pSizeOfData = bytes; return D3DERR_MOREDATA; }
  memcpy(pData, &it->second.function[0], bytes); *pSizeOfData = bytes; return D3D_OK;
}
STDMETHODIMP QD3D8Device8::SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer8* pStreamData, UINT Stride)
{
  if (StreamNumber >= 16) return D3DERR_INVALIDCALL;
  if (m_streams[StreamNumber].vb) m_streams[StreamNumber].vb->Release();
  m_streams[StreamNumber].vb = static_cast<QD3D8VertexBuffer8*>(pStreamData);
  if (m_streams[StreamNumber].vb) m_streams[StreamNumber].vb->AddRef();
  m_streams[StreamNumber].stride = Stride;
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer8** ppStreamData, UINT* pStride)
{
  if (StreamNumber >= 16 || !ppStreamData || !pStride) return D3DERR_INVALIDCALL;
  *ppStreamData = m_streams[StreamNumber].vb;
  if (*ppStreamData) (*ppStreamData)->AddRef();
  *pStride = m_streams[StreamNumber].stride;
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::SetIndices(IDirect3DIndexBuffer8* pIndexData, UINT BaseVertexIndex)
{
  if (m_indices) m_indices->Release();
  m_indices = static_cast<QD3D8IndexBuffer8*>(pIndexData);
  if (m_indices) m_indices->AddRef();
  m_baseVertexIndex = BaseVertexIndex;
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::GetIndices(IDirect3DIndexBuffer8** ppIndexData, UINT* pBaseVertexIndex)
{
  if (!ppIndexData || !pBaseVertexIndex) return D3DERR_INVALIDCALL;
  *ppIndexData = m_indices; if (*ppIndexData) (*ppIndexData)->AddRef();
  *pBaseVertexIndex = m_baseVertexIndex;
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::CreatePixelShader(const DWORD* pFunction, DWORD* pHandle)
{
  if (!pHandle) return D3DERR_INVALIDCALL;
  ShaderStub s;
  if (pFunction)
    for (int i = 0; i < 256 && pFunction[i] != 0x0000FFFF; ++i) s.function.push_back(pFunction[i]);
  DWORD h = m_nextShaderHandle++;
  m_pixelShaders[h] = s;
  *pHandle = h;
  return D3D_OK;
}
STDMETHODIMP QD3D8Device8::SetPixelShader(DWORD Handle) { m_currentPixelShader = Handle; return D3D_OK; }
STDMETHODIMP QD3D8Device8::GetPixelShader(DWORD* pHandle) { if (!pHandle) return D3DERR_INVALIDCALL; *pHandle = m_currentPixelShader; return D3D_OK; }
STDMETHODIMP QD3D8Device8::DeletePixelShader(DWORD Handle) { m_pixelShaders.erase(Handle); return D3D_OK; }
STDMETHODIMP QD3D8Device8::SetPixelShaderConstant(DWORD, const void*, DWORD) { return D3D_OK; }
STDMETHODIMP QD3D8Device8::GetPixelShaderConstant(DWORD, void*, DWORD) { return D3D_OK; }
STDMETHODIMP QD3D8Device8::GetPixelShaderFunction(DWORD Handle, void* pData, DWORD* pSizeOfData)
{
  if (!pSizeOfData) return D3DERR_INVALIDCALL;
  std::map<DWORD, ShaderStub>::iterator it = m_pixelShaders.find(Handle);
  if (it == m_pixelShaders.end()) return D3DERR_INVALIDCALL;
  DWORD bytes = (DWORD)(it->second.function.size() * sizeof(DWORD));
  if (!pData || *pSizeOfData < bytes) { *pSizeOfData = bytes; return D3DERR_MOREDATA; }
  memcpy(pData, &it->second.function[0], bytes); *pSizeOfData = bytes; return D3D_OK;
}
STDMETHODIMP QD3D8Device8::DrawRectPatch(UINT, const float*, const D3DRECTPATCH_INFO*) { return D3DERR_NOTAVAILABLE; }
STDMETHODIMP QD3D8Device8::DrawTriPatch(UINT, const float*, const D3DTRIPATCH_INFO*) { return D3DERR_NOTAVAILABLE; }
STDMETHODIMP QD3D8Device8::DeletePatch(UINT) { return D3D_OK; }

// ---------------- Resource / Texture / Surface / Buffer boilerplate ----------------
// These are implemented with the same policy: straightforward COM behavior + simple GL/system-memory backing.
// For brevity in this starter, repeated one-line delegations are compact but complete.

#define QD3D8_IMPL_RESOURCE_COMMON(cls, iidname) \
STDMETHODIMP cls::QueryInterface(REFIID riid, void** ppvObj){ if(!ppvObj) return E_POINTER; *ppvObj=NULL; if(riid==IID_IUnknown || riid==iidname){ *ppvObj=this; AddRef(); return S_OK; } return E_NOINTERFACE; } \
STDMETHODIMP_(ULONG) cls::AddRef(){ return InternalAddRef(); } \
STDMETHODIMP_(ULONG) cls::Release(){ return InternalRelease(); } \
STDMETHODIMP cls::GetDevice(IDirect3DDevice8** ppDevice){ return GetDeviceCommon(ppDevice);} \
STDMETHODIMP cls::SetPrivateData(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags){ return SetPrivateDataCommon(refguid,pData,SizeOfData,Flags);} \
STDMETHODIMP cls::GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData){ return GetPrivateDataCommon(refguid,pData,pSizeOfData);} \
STDMETHODIMP cls::FreePrivateData(REFGUID refguid){ return FreePrivateDataCommon(refguid);} \
STDMETHODIMP_(DWORD) cls::SetPriority(DWORD PriorityNew){ return SetPriorityCommon(PriorityNew);} \
STDMETHODIMP_(DWORD) cls::GetPriority(){ return GetPriorityCommon();} \
STDMETHODIMP_(void) cls::PreLoad(){ PreLoadCommon(); } \
STDMETHODIMP_(D3DRESOURCETYPE) cls::GetType(){ return GetTypeCommon(); }

QD3D8Surface8::QD3D8Surface8(QD3D8Device8* dev, UINT w, UINT h, D3DFORMAT fmt, bool rt, bool ds, bool ownObj)
  : QD3D8ResourceBase(dev, D3DRTYPE_SURFACE), m_glTex(0), m_glRB(0), m_isRenderTarget(rt), m_isDepth(ds), m_ownsObject(ownObj), m_width(w), m_height(h), m_format(fmt)
{
  m_fmt = QD3D8_TranslateFormat(fmt);
  m_sysmem.resize(w * h * std::max(1u, m_fmt.bytesPerPixel));
}
QD3D8Surface8::~QD3D8Surface8()
{
  if (m_ownsObject)
  {
    if (m_glTex) glDeleteTextures(1, &m_glTex);
    //if (m_glRB) glDeleteRenderbuffersEXT(1, &m_glRB);
  }
}
HRESULT QD3D8Surface8::InitializeTextureBacked()
{
  if (!m_fmt.supported) return D3DERR_NOTAVAILABLE;
  glGenTextures(1, &m_glTex);
  glBindTexture(GL_TEXTURE_2D, m_glTex);
  glTexImage2D(GL_TEXTURE_2D, 0, m_fmt.internalFormat, m_width, m_height, 0, m_fmt.format, m_fmt.type, NULL);
  return D3D_OK;
}
HRESULT QD3D8Surface8::InitializeRenderbufferBacked()
{
//  glGenRenderbuffersEXT(1, &m_glRB);
//  glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_glRB);
 // glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, m_fmt.internalFormat, m_width, m_height);
  return D3D_OK;
}
QD3D8_IMPL_RESOURCE_COMMON(QD3D8Surface8, IID_IDirect3DSurface8)
STDMETHODIMP QD3D8Surface8::GetContainer(REFIID, void** ppContainer) { if (ppContainer) *ppContainer = NULL; return D3DERR_NOTAVAILABLE; }
STDMETHODIMP QD3D8Surface8::GetDesc(D3DSURFACE_DESC* pDesc)
{
  if (!pDesc) return D3DERR_INVALIDCALL;
  memset(pDesc, 0, sizeof(*pDesc));
  pDesc->Format = m_format;
  pDesc->Type = D3DRTYPE_SURFACE;
  pDesc->Usage = m_isRenderTarget ? D3DUSAGE_RENDERTARGET : (m_isDepth ? D3DUSAGE_DEPTHSTENCIL : 0);
  pDesc->Pool = D3DPOOL_DEFAULT;
  pDesc->Size = (UINT)m_sysmem.size();
  pDesc->MultiSampleType = D3DMULTISAMPLE_NONE;
  pDesc->Width = m_width;
  pDesc->Height = m_height;
  return D3D_OK;
}
STDMETHODIMP QD3D8Surface8::LockRect(D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD Flags)
{
  if (!pLockedRect || m_lock.active) return D3DERR_INVALIDCALL;
  RECT r = { 0,0,(LONG)m_width,(LONG)m_height };
  if (pRect) r = *pRect;
  uint32_t bpp = m_fmt.bytesPerPixel;
  m_lock.active = true;
  m_lock.readonly = (Flags & D3DLOCK_READONLY) != 0;
  m_lock.pitch = m_width * bpp;
  m_lock.rect = r;
  m_lock.ptr = &m_sysmem[(r.top * m_width + r.left) * bpp];
  pLockedRect->Pitch = m_lock.pitch;
  pLockedRect->pBits = m_lock.ptr;
  return D3D_OK;
}
STDMETHODIMP QD3D8Surface8::UnlockRect()
{
  if (!m_lock.active) return D3DERR_INVALIDCALL;
  if (!m_lock.readonly && m_glTex)
  {
    glBindTexture(GL_TEXTURE_2D, m_glTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, m_fmt.format, m_fmt.type, &m_sysmem[0]);
  }
  m_lock.active = false;
  return D3D_OK;
}

QD3D8BaseTexture8::QD3D8BaseTexture8(QD3D8Device8* dev, D3DRESOURCETYPE type, DWORD levels, GLenum glTarget)
  : QD3D8ResourceBase(dev, type), m_lod(0), m_levels(levels), m_glTex(0), m_glTarget(glTarget)
{
  glGenTextures(1, &m_glTex);
}
QD3D8BaseTexture8::~QD3D8BaseTexture8() { if (m_glTex) glDeleteTextures(1, &m_glTex); }
QD3D8_IMPL_RESOURCE_COMMON(QD3D8BaseTexture8, IID_IDirect3DBaseTexture8)
STDMETHODIMP_(DWORD) QD3D8BaseTexture8::SetLOD(DWORD LODNew) { DWORD old = m_lod; m_lod = LODNew; return old; }
STDMETHODIMP_(DWORD) QD3D8BaseTexture8::GetLOD() { return m_lod; }
STDMETHODIMP_(DWORD) QD3D8BaseTexture8::GetLevelCount() { return m_levels; }

QD3D8Texture8::QD3D8Texture8(QD3D8Device8* dev, UINT w, UINT h, UINT levels, D3DFORMAT fmt)
  : QD3D8BaseTexture8(dev, D3DRTYPE_TEXTURE, levels, GL_TEXTURE_2D), m_width(w), m_height(h), m_format(fmt)
{
  m_fmt = QD3D8_TranslateFormat(fmt);
}
QD3D8Texture8::~QD3D8Texture8() { for (size_t i = 0; i < m_surfaces.size(); ++i) if (m_surfaces[i]) m_surfaces[i]->Release(); }
HRESULT QD3D8Texture8::Initialize()
{
//  if (!m_fmt.supported) return D3DERR_NOTAVAILABLE;
  glBindTexture(GL_TEXTURE_2D, m_glTex);
  UINT w = m_width, h = m_height;
  m_levels = 1;
  for (UINT i = 0; i < m_levels; ++i)
  {
    glTexImage2D(GL_TEXTURE_2D, i, m_fmt.internalFormat, w, h, 0, m_fmt.format, m_fmt.type, NULL);
    QD3D8Surface8* s = new QD3D8Surface8(m_device, w, h, m_format, false, false, false);

    if(i == 0)
      s->m_glTex = m_glTex;

    m_surfaces.push_back(s);
    w = std::max(1u, w >> 1);
    h = std::max(1u, h >> 1);
  }
  return D3D_OK;
}
QD3D8_IMPL_RESOURCE_COMMON(QD3D8Texture8, IID_IDirect3DTexture8)
STDMETHODIMP_(DWORD) QD3D8Texture8::SetLOD(DWORD LODNew) { return QD3D8BaseTexture8::SetLOD(LODNew); }
STDMETHODIMP_(DWORD) QD3D8Texture8::GetLOD() { return QD3D8BaseTexture8::GetLOD(); }
STDMETHODIMP_(DWORD) QD3D8Texture8::GetLevelCount() { return QD3D8BaseTexture8::GetLevelCount(); }
STDMETHODIMP QD3D8Texture8::GetLevelDesc(UINT Level, D3DSURFACE_DESC* pDesc) { if (Level >= m_surfaces.size()) return D3DERR_INVALIDCALL; return m_surfaces[Level]->GetDesc(pDesc); }
STDMETHODIMP QD3D8Texture8::GetSurfaceLevel(UINT Level, IDirect3DSurface8** ppSurfaceLevel) { if (!ppSurfaceLevel || Level >= m_surfaces.size()) return D3DERR_INVALIDCALL; *ppSurfaceLevel = m_surfaces[Level]; (*ppSurfaceLevel)->AddRef(); return D3D_OK; }
STDMETHODIMP QD3D8Texture8::LockRect(UINT Level, D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD Flags) { if (Level >= m_surfaces.size()) return D3DERR_INVALIDCALL; return m_surfaces[Level]->LockRect(pLockedRect, pRect, Flags); }
STDMETHODIMP QD3D8Texture8::UnlockRect(UINT Level) {
  if (Level >= m_surfaces.size())
    return D3DERR_INVALIDCALL;

  if (Level > 0)
    return S_OK;

  glBindTexture(GL_TEXTURE_2D, m_glTex);
  HRESULT hr = S_OK;
  UINT w = m_surfaces[Level]->m_width, h = m_surfaces[Level]->m_height;
  glTexSubImage2D(GL_TEXTURE_2D, Level, 0, 0, w, h, m_fmt.format, m_fmt.type, &m_surfaces[Level]->m_sysmem[0]);
  return hr;
}
STDMETHODIMP QD3D8Texture8::AddDirtyRect(const RECT*) { return D3D_OK; }

QD3D8CubeTexture8::QD3D8CubeTexture8(QD3D8Device8* dev, UINT edge, UINT levels, D3DFORMAT fmt)
  : QD3D8BaseTexture8(dev, D3DRTYPE_CUBETEXTURE, levels, 0), m_edgeLength(edge), m_format(fmt)
{
  memset(m_faces, 0, sizeof(m_faces));
  m_fmt = QD3D8_TranslateFormat(fmt);
}
QD3D8CubeTexture8::~QD3D8CubeTexture8() { for (int i = 0; i < 6; ++i) if (m_faces[i]) m_faces[i]->Release(); }
QD3D8_IMPL_RESOURCE_COMMON(QD3D8CubeTexture8, IID_IDirect3DCubeTexture8)
STDMETHODIMP_(DWORD) QD3D8CubeTexture8::SetLOD(DWORD LODNew) { return QD3D8BaseTexture8::SetLOD(LODNew); }
STDMETHODIMP_(DWORD) QD3D8CubeTexture8::GetLOD() { return QD3D8BaseTexture8::GetLOD(); }
STDMETHODIMP_(DWORD) QD3D8CubeTexture8::GetLevelCount() { return QD3D8BaseTexture8::GetLevelCount(); }
STDMETHODIMP QD3D8CubeTexture8::GetLevelDesc(UINT, D3DSURFACE_DESC* pDesc) { if (!pDesc) return D3DERR_INVALIDCALL; memset(pDesc, 0, sizeof(*pDesc)); pDesc->Format = m_format; pDesc->Type = D3DRTYPE_CUBETEXTURE; pDesc->Width = m_edgeLength; pDesc->Height = m_edgeLength; return D3D_OK; }
STDMETHODIMP QD3D8CubeTexture8::GetCubeMapSurface(D3DCUBEMAP_FACES FaceType, UINT, IDirect3DSurface8** ppCubeMapSurface) { if (!ppCubeMapSurface || FaceType > D3DCUBEMAP_FACE_NEGATIVE_Z) return D3DERR_INVALIDCALL; if (!m_faces[FaceType]) m_faces[FaceType] = new QD3D8Surface8(m_device, m_edgeLength, m_edgeLength, m_format, false, false, false); *ppCubeMapSurface = m_faces[FaceType]; (*ppCubeMapSurface)->AddRef(); return D3D_OK; }
STDMETHODIMP QD3D8CubeTexture8::LockRect(D3DCUBEMAP_FACES FaceType, UINT Level, D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD Flags) { IDirect3DSurface8* s = NULL; HRESULT hr = GetCubeMapSurface(FaceType, Level, &s); if (FAILED(hr)) return hr; hr = ((QD3D8Surface8*)s)->LockRect(pLockedRect, pRect, Flags); s->Release(); return hr; }
STDMETHODIMP QD3D8CubeTexture8::UnlockRect(D3DCUBEMAP_FACES FaceType, UINT Level) { IDirect3DSurface8* s = NULL; HRESULT hr = GetCubeMapSurface(FaceType, Level, &s); if (FAILED(hr)) return hr; hr = ((QD3D8Surface8*)s)->UnlockRect(); s->Release(); return hr; }
STDMETHODIMP QD3D8CubeTexture8::AddDirtyRect(D3DCUBEMAP_FACES, const RECT*) { return D3D_OK; }

QD3D8VolumeTexture8::QD3D8VolumeTexture8(QD3D8Device8* dev, UINT w, UINT h, UINT d, UINT levels, D3DFORMAT fmt)
  : QD3D8BaseTexture8(dev, D3DRTYPE_VOLUMETEXTURE, levels, 0), m_width(w), m_height(h), m_depth(d), m_format(fmt) {
}
QD3D8VolumeTexture8::~QD3D8VolumeTexture8() {}
QD3D8_IMPL_RESOURCE_COMMON(QD3D8VolumeTexture8, IID_IDirect3DVolumeTexture8)
STDMETHODIMP_(DWORD) QD3D8VolumeTexture8::SetLOD(DWORD LODNew) { return QD3D8BaseTexture8::SetLOD(LODNew); }
STDMETHODIMP_(DWORD) QD3D8VolumeTexture8::GetLOD() { return QD3D8BaseTexture8::GetLOD(); }
STDMETHODIMP_(DWORD) QD3D8VolumeTexture8::GetLevelCount() { return QD3D8BaseTexture8::GetLevelCount(); }
STDMETHODIMP QD3D8VolumeTexture8::GetLevelDesc(UINT, D3DVOLUME_DESC* pDesc) { if (!pDesc) return D3DERR_INVALIDCALL; memset(pDesc, 0, sizeof(*pDesc)); pDesc->Format = m_format; pDesc->Type = D3DRTYPE_VOLUMETEXTURE; pDesc->Width = m_width; pDesc->Height = m_height; pDesc->Depth = m_depth; return D3D_OK; }
STDMETHODIMP QD3D8VolumeTexture8::GetVolumeLevel(UINT, IDirect3DVolume8** ppVolumeLevel) { if (!ppVolumeLevel) return D3DERR_INVALIDCALL; *ppVolumeLevel = new QD3D8Volume8(m_device, m_width, m_height, m_depth, m_format); return D3D_OK; }
STDMETHODIMP QD3D8VolumeTexture8::LockBox(UINT Level, D3DLOCKED_BOX* pLockedVolume, const D3DBOX* pBox, DWORD Flags) { IDirect3DVolume8* v = NULL; HRESULT hr = GetVolumeLevel(Level, &v); if (FAILED(hr)) return hr; hr = ((QD3D8Volume8*)v)->LockBox(pLockedVolume, pBox, Flags); v->Release(); return hr; }
STDMETHODIMP QD3D8VolumeTexture8::UnlockBox(UINT) { return D3D_OK; }
STDMETHODIMP QD3D8VolumeTexture8::AddDirtyBox(const D3DBOX*) { return D3D_OK; }

QD3D8Volume8::QD3D8Volume8(QD3D8Device8* dev, UINT w, UINT h, UINT d, D3DFORMAT fmt)
  : QD3D8ResourceBase(dev, D3DRTYPE_VOLUME), m_width(w), m_height(h), m_depth(d), m_format(fmt)
{
  m_sysmem.resize(w * h * d * (QD3D8_CalcBitsPerPixel(fmt) / 8));
}
QD3D8Volume8::~QD3D8Volume8() {}
QD3D8_IMPL_RESOURCE_COMMON(QD3D8Volume8, IID_IDirect3DVolume8)
STDMETHODIMP QD3D8Volume8::GetContainer(REFIID, void** ppContainer) { if (ppContainer) *ppContainer = NULL; return D3DERR_NOTAVAILABLE; }
STDMETHODIMP QD3D8Volume8::GetDesc(D3DVOLUME_DESC* pDesc) { if (!pDesc) return D3DERR_INVALIDCALL; memset(pDesc, 0, sizeof(*pDesc)); pDesc->Format = m_format; pDesc->Type = D3DRTYPE_VOLUME; pDesc->Width = m_width; pDesc->Height = m_height; pDesc->Depth = m_depth; return D3D_OK; }
STDMETHODIMP QD3D8Volume8::LockBox(D3DLOCKED_BOX* pLockedVolume, const D3DBOX*, DWORD) { if (!pLockedVolume) return D3DERR_INVALIDCALL; pLockedVolume->RowPitch = m_width * (QD3D8_CalcBitsPerPixel(m_format) / 8); pLockedVolume->SlicePitch = pLockedVolume->RowPitch * m_height; pLockedVolume->pBits = &m_sysmem[0]; return D3D_OK; }
STDMETHODIMP QD3D8Volume8::UnlockBox() { return D3D_OK; }

QD3D8VertexBuffer8::QD3D8VertexBuffer8(QD3D8Device8* dev, UINT length, DWORD usage, DWORD fvf, D3DPOOL pool)
  : QD3D8ResourceBase(dev, D3DRTYPE_VERTEXBUFFER), m_glBuffer(0), m_length(length), m_usage(usage), m_fvf(fvf), m_pool(pool), m_locked(false), m_dirty(true), m_lockOffset(0), m_lockSize(0)
{
  m_data.resize(length);
//  glGenBuffersARB(1, &m_glBuffer);
}
QD3D8VertexBuffer8::~QD3D8VertexBuffer8() { }
QD3D8_IMPL_RESOURCE_COMMON(QD3D8VertexBuffer8, IID_IDirect3DVertexBuffer8)
STDMETHODIMP QD3D8VertexBuffer8::Lock(UINT OffsetToLock, UINT SizeToLock, BYTE** ppbData, DWORD) { if (!ppbData || m_locked) return D3DERR_INVALIDCALL; if (SizeToLock == 0) SizeToLock = m_length - OffsetToLock; m_locked = true; m_lockOffset = OffsetToLock; m_lockSize = SizeToLock; *ppbData = &m_data[OffsetToLock]; return D3D_OK; }
STDMETHODIMP QD3D8VertexBuffer8::Unlock() { if (!m_locked) return D3DERR_INVALIDCALL; m_locked = false; m_dirty = true; return D3D_OK; }
STDMETHODIMP QD3D8VertexBuffer8::GetDesc(D3DVERTEXBUFFER_DESC* pDesc) { if (!pDesc) return D3DERR_INVALIDCALL; memset(pDesc, 0, sizeof(*pDesc)); pDesc->Format = D3DFMT_VERTEXDATA; pDesc->Type = D3DRTYPE_VERTEXBUFFER; pDesc->Usage = m_usage; pDesc->Pool = m_pool; pDesc->Size = m_length; pDesc->FVF = m_fvf; return D3D_OK; }

QD3D8IndexBuffer8::QD3D8IndexBuffer8(QD3D8Device8* dev, UINT length, DWORD usage, D3DFORMAT fmt, D3DPOOL pool)
  : QD3D8ResourceBase(dev, D3DRTYPE_INDEXBUFFER), m_glBuffer(0), m_length(length), m_usage(usage), m_format(fmt), m_pool(pool), m_locked(false), m_dirty(true), m_lockOffset(0), m_lockSize(0)
{
  m_data.resize(length);

}
QD3D8IndexBuffer8::~QD3D8IndexBuffer8() { }
QD3D8_IMPL_RESOURCE_COMMON(QD3D8IndexBuffer8, IID_IDirect3DIndexBuffer8)
STDMETHODIMP QD3D8IndexBuffer8::Lock(UINT OffsetToLock, UINT SizeToLock, BYTE** ppbData, DWORD) { if (!ppbData || m_locked) return D3DERR_INVALIDCALL; if (SizeToLock == 0) SizeToLock = m_length - OffsetToLock; m_locked = true; m_lockOffset = OffsetToLock; m_lockSize = SizeToLock; *ppbData = &m_data[OffsetToLock]; return D3D_OK; }
STDMETHODIMP QD3D8IndexBuffer8::Unlock() { if (!m_locked) return D3DERR_INVALIDCALL; m_locked = false; m_dirty = true; return D3D_OK; }
STDMETHODIMP QD3D8IndexBuffer8::GetDesc(D3DINDEXBUFFER_DESC* pDesc) { if (!pDesc) return D3DERR_INVALIDCALL; memset(pDesc, 0, sizeof(*pDesc)); pDesc->Format = m_format; pDesc->Type = D3DRTYPE_INDEXBUFFER; pDesc->Usage = m_usage; pDesc->Pool = m_pool; pDesc->Size = m_length; return D3D_OK; }

QD3D8SwapChain8::QD3D8SwapChain8(QD3D8Device8* device) : m_device(device) { if (m_device) m_device->AddRef(); }
QD3D8SwapChain8::~QD3D8SwapChain8() { if (m_device) m_device->Release(); }
STDMETHODIMP QD3D8SwapChain8::QueryInterface(REFIID riid, void** ppvObj) { if (!ppvObj) return E_POINTER; *ppvObj = NULL; if (riid == IID_IUnknown || riid == IID_IDirect3DSwapChain8) { *ppvObj = this; AddRef(); return S_OK; } return E_NOINTERFACE; }
STDMETHODIMP_(ULONG) QD3D8SwapChain8::AddRef() { return InternalAddRef(); }
STDMETHODIMP_(ULONG) QD3D8SwapChain8::Release() { return InternalRelease(); }
STDMETHODIMP QD3D8SwapChain8::Present(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) { return m_device->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion); }
STDMETHODIMP QD3D8SwapChain8::GetBackBuffer(UINT BackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface8** ppBackBuffer) { return m_device->GetBackBuffer(BackBuffer, Type, ppBackBuffer); }

extern "C" IDirect3D8* WINAPI WWDirect3DCreate8(UINT SDKVersion)
{
  if (SDKVersion != D3D_SDK_VERSION)
    return NULL;
  return new QD3D8Direct3D8();
}


extern "C"
{

  void  glFrontFace(GLenum mode)
  {
    (void)mode;
  }

  void  glTexEnvi(GLenum target, GLenum pname, GLint param)
  {
    (void)target;
    (void)pname;
    (void)param;
  }

  void  glMaterialfv(GLenum face, GLenum pname, const GLfloat* params)
  {
    (void)face;
    (void)pname;
    (void)params;
  }

  void  glMaterialf(GLenum face, GLenum pname, GLfloat param)
  {
    (void)face;
    (void)pname;
    (void)param;
  }

  void  glLightfv(GLenum light, GLenum pname, const GLfloat* params)
  {
    (void)light;
    (void)pname;
    (void)params;
  }

  void  glLightf(GLenum light, GLenum pname, GLfloat param)
  {
    (void)light;
    (void)pname;
    (void)param;
  }

  void  glFlush(void)
  {
  }

} // extern "C"
