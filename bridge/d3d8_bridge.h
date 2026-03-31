#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>
#include "opengl.h"
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <map>
#include <string>
#include <string.h>

#include "d3d8.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif
#ifndef GL_TEXTURE2
#define GL_TEXTURE2 0x84C2
#endif
#ifndef GL_TEXTURE3
#define GL_TEXTURE3 0x84C3
#endif
#ifndef GL_BGR_EXT
#define GL_BGR_EXT 0x80E0
#endif

#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24 0x81A6
#endif
#ifndef GL_GENERATE_MIPMAP
#define GL_GENERATE_MIPMAP 0x8191
#endif

#ifndef D3DERR_INVALIDCALL
#define D3DERR_INVALIDCALL ((HRESULT)0x8876086cL)
#endif
#ifndef D3DERR_NOTAVAILABLE
#define D3DERR_NOTAVAILABLE ((HRESULT)0x8876086aL)
#endif
#ifndef D3DERR_OUTOFVIDEOMEMORY
#define D3DERR_OUTOFVIDEOMEMORY ((HRESULT)0x8876017cL)
#endif
#ifndef D3D_OK
#define D3D_OK S_OK
#endif

class QD3D8Device8;
class QD3D8Direct3D8;
class QD3D8Resource8;
class QD3D8BaseTexture8;
class QD3D8Texture8;
class QD3D8CubeTexture8;
class QD3D8VolumeTexture8;
class QD3D8Surface8;
class QD3D8Volume8;
class QD3D8VertexBuffer8;
class QD3D8IndexBuffer8;
class QD3D8SwapChain8;

struct QGLFormatInfo
{
  GLenum internalFormat;
  GLenum format;
  GLenum type;
  uint32_t bytesPerPixel;
  bool supported;
};

struct QD3D8LockedRegion
{
  void* ptr;
  uint32_t pitch;
  RECT rect;
  bool active;
  bool readonly;

  QD3D8LockedRegion() : ptr(NULL), pitch(0), active(false), readonly(false)
  {
    rect.left = rect.top = rect.right = rect.bottom = 0;
  }
};

struct QD3D8TextureStageState
{
  DWORD colorOp;
  DWORD colorArg1;
  DWORD colorArg2;
  DWORD alphaOp;
  DWORD alphaArg1;
  DWORD alphaArg2;
  DWORD texCoordIndex;
  DWORD addressU;
  DWORD addressV;
  DWORD magFilter;
  DWORD minFilter;
  DWORD mipFilter;
  DWORD mipMapLodBias;
  DWORD maxMipLevel;
  DWORD maxAnisotropy;
  DWORD borderColor;

  QD3D8TextureStageState()
  {
    colorOp = D3DTOP_MODULATE;
    colorArg1 = D3DTA_TEXTURE;
    colorArg2 = D3DTA_DIFFUSE;
    alphaOp = D3DTOP_SELECTARG1;
    alphaArg1 = D3DTA_TEXTURE;
    alphaArg2 = D3DTA_DIFFUSE;
    texCoordIndex = 0;
    addressU = D3DTADDRESS_WRAP;
    addressV = D3DTADDRESS_WRAP;
    magFilter = D3DTEXF_LINEAR;
    minFilter = D3DTEXF_LINEAR;
    mipFilter = D3DTEXF_LINEAR;
    mipMapLodBias = 0;
    maxMipLevel = 0;
    maxAnisotropy = 1;
    borderColor = 0;
  }
};

struct QD3D8StreamSource
{
  class QD3D8VertexBuffer8* vb;
  UINT stride;
  QD3D8StreamSource() : vb(NULL), stride(0) {}
};

struct QD3D8PrivateData
{
  std::vector<uint8_t> data;
  DWORD flags;
};

class QD3D8Unknown
{
protected:
  volatile long m_refCount;
public:
  QD3D8Unknown() : m_refCount(1) {}
  virtual ~QD3D8Unknown() {}
  ULONG InternalAddRef() { return (ULONG)InterlockedIncrement(&m_refCount); }
  ULONG InternalRelease()
  {
    ULONG r = (ULONG)InterlockedDecrement(&m_refCount);
    if (!r) delete this;
    return r;
  }
};

class QD3D8ResourceBase : public QD3D8Unknown
{
protected:
  QD3D8Device8* m_device;
  D3DRESOURCETYPE m_type;
  DWORD m_priority;
  std::map<GUID, QD3D8PrivateData, bool(*)(const GUID&, const GUID&)> m_privateData;

  static bool GuidLess(const GUID& a, const GUID& b)
  {
    return memcmp(&a, &b, sizeof(GUID)) < 0;
  }

public:
  QD3D8ResourceBase(QD3D8Device8* device, D3DRESOURCETYPE type)
    : m_device(device), m_type(type), m_priority(0), m_privateData(GuidLess)
  {
  }

  virtual ~QD3D8ResourceBase() {}

  HRESULT GetDeviceCommon(IDirect3DDevice8** ppDevice);
  HRESULT SetPrivateDataCommon(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags);
  HRESULT GetPrivateDataCommon(REFGUID refguid, void* pData, DWORD* pSizeOfData);
  HRESULT FreePrivateDataCommon(REFGUID refguid);
  DWORD SetPriorityCommon(DWORD PriorityNew) { DWORD old = m_priority; m_priority = PriorityNew; return old; }
  DWORD GetPriorityCommon() const { return m_priority; }
  void PreLoadCommon() {}
  D3DRESOURCETYPE GetTypeCommon() const { return m_type; }
};

class QD3D8Direct3D8 : public IDirect3D8, public QD3D8Unknown
{
public:
  QD3D8Direct3D8();
  virtual ~QD3D8Direct3D8();

  STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj);
  STDMETHOD_(ULONG, AddRef)();
  STDMETHOD_(ULONG, Release)();

  STDMETHOD(RegisterSoftwareDevice)(void* pInitializeFunction);
  STDMETHOD_(UINT, GetAdapterCount)();
  STDMETHOD(GetAdapterIdentifier)(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER8* pIdentifier);
  STDMETHOD_(UINT, GetAdapterModeCount)(UINT Adapter);
  STDMETHOD(EnumAdapterModes)(UINT Adapter, UINT Mode, D3DDISPLAYMODE* pMode);
  STDMETHOD(GetAdapterDisplayMode)(UINT Adapter, D3DDISPLAYMODE* pMode);
  STDMETHOD(CheckDeviceType)(UINT Adapter, D3DDEVTYPE CheckType, D3DFORMAT DisplayFormat, D3DFORMAT BackBufferFormat, BOOL Windowed);
  STDMETHOD(CheckDeviceFormat)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat);
  STDMETHOD(CheckDeviceMultiSampleType)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType);
  STDMETHOD(CheckDepthStencilMatch)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat);
  STDMETHOD(GetDeviceCaps)(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS8* pCaps);
  STDMETHOD_(HMONITOR, GetAdapterMonitor)(UINT Adapter);
  STDMETHOD(CreateDevice)(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface);
};

class QD3D8Surface8 : public IDirect3DSurface8, public QD3D8ResourceBase
{
public:
  GLuint m_glTex;
  GLuint m_glRB;
  bool m_isRenderTarget;
  bool m_isDepth;
  bool m_ownsObject;
  UINT m_width;
  UINT m_height;
  D3DFORMAT m_format;
  QGLFormatInfo m_fmt;
  std::vector<uint8_t> m_sysmem;
  QD3D8LockedRegion m_lock;

  QD3D8Surface8(QD3D8Device8* dev, UINT w, UINT h, D3DFORMAT fmt, bool rt, bool ds, bool ownObj);
  virtual ~QD3D8Surface8();

  HRESULT InitializeTextureBacked();
  HRESULT InitializeRenderbufferBacked();

  STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj);
  STDMETHOD_(ULONG, AddRef)();
  STDMETHOD_(ULONG, Release)();

  STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice);
  STDMETHOD(SetPrivateData)(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags);
  STDMETHOD(GetPrivateData)(REFGUID refguid, void* pData, DWORD* pSizeOfData);
  STDMETHOD(FreePrivateData)(REFGUID refguid);
  STDMETHOD_(DWORD, SetPriority)(DWORD PriorityNew);
  STDMETHOD_(DWORD, GetPriority)();
  STDMETHOD_(void, PreLoad)();
  STDMETHOD_(D3DRESOURCETYPE, GetType)();
  STDMETHOD(GetContainer)(REFIID riid, void** ppContainer);
  STDMETHOD(GetDesc)(D3DSURFACE_DESC* pDesc);
  STDMETHOD(LockRect)(D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD Flags);
  STDMETHOD(UnlockRect)();
};

class QD3D8BaseTexture8 : public IDirect3DBaseTexture8, public QD3D8ResourceBase
{
protected:
  DWORD m_lod;
  DWORD m_levels;
  GLuint m_glTex;
  GLenum m_glTarget;
public:
  QD3D8BaseTexture8(QD3D8Device8* dev, D3DRESOURCETYPE type, DWORD levels, GLenum glTarget);
  virtual ~QD3D8BaseTexture8();

  STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj);
  STDMETHOD_(ULONG, AddRef)();
  STDMETHOD_(ULONG, Release)();

  STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice);
  STDMETHOD(SetPrivateData)(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags);
  STDMETHOD(GetPrivateData)(REFGUID refguid, void* pData, DWORD* pSizeOfData);
  STDMETHOD(FreePrivateData)(REFGUID refguid);
  STDMETHOD_(DWORD, SetPriority)(DWORD PriorityNew);
  STDMETHOD_(DWORD, GetPriority)();
  STDMETHOD_(void, PreLoad)();
  STDMETHOD_(D3DRESOURCETYPE, GetType)();
  STDMETHOD_(DWORD, SetLOD)(DWORD LODNew);
  STDMETHOD_(DWORD, GetLOD)();
  STDMETHOD_(DWORD, GetLevelCount)();

  GLuint GetGLTex() const { return m_glTex; }
  GLenum GetGLTarget() const { return m_glTarget; }
};

class QD3D8Texture8 : public IDirect3DTexture8, public QD3D8BaseTexture8
{
public:
  UINT m_width;
  UINT m_height;
  D3DFORMAT m_format;
  QGLFormatInfo m_fmt;
  std::vector< QD3D8Surface8* > m_surfaces;

  QD3D8Texture8(QD3D8Device8* dev, UINT w, UINT h, UINT levels, D3DFORMAT fmt);
  virtual ~QD3D8Texture8();
  HRESULT Initialize();

  STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj);
  STDMETHOD_(ULONG, AddRef)();
  STDMETHOD_(ULONG, Release)();
  STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice);
  STDMETHOD(SetPrivateData)(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags);
  STDMETHOD(GetPrivateData)(REFGUID refguid, void* pData, DWORD* pSizeOfData);
  STDMETHOD(FreePrivateData)(REFGUID refguid);
  STDMETHOD_(DWORD, SetPriority)(DWORD PriorityNew);
  STDMETHOD_(DWORD, GetPriority)();
  STDMETHOD_(void, PreLoad)();
  STDMETHOD_(D3DRESOURCETYPE, GetType)();
  STDMETHOD_(DWORD, SetLOD)(DWORD LODNew);
  STDMETHOD_(DWORD, GetLOD)();
  STDMETHOD_(DWORD, GetLevelCount)();

  STDMETHOD(GetLevelDesc)(UINT Level, D3DSURFACE_DESC* pDesc);
  STDMETHOD(GetSurfaceLevel)(UINT Level, IDirect3DSurface8** ppSurfaceLevel);
  STDMETHOD(LockRect)(UINT Level, D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD Flags);
  STDMETHOD(UnlockRect)(UINT Level);
  STDMETHOD(AddDirtyRect)(const RECT* pDirtyRect);
};

class QD3D8CubeTexture8 : public IDirect3DCubeTexture8, public QD3D8BaseTexture8
{
public:
  UINT m_edgeLength;
  D3DFORMAT m_format;
  QGLFormatInfo m_fmt;
  QD3D8Surface8* m_faces[6];

  QD3D8CubeTexture8(QD3D8Device8* dev, UINT edge, UINT levels, D3DFORMAT fmt);
  virtual ~QD3D8CubeTexture8();

  STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj);
  STDMETHOD_(ULONG, AddRef)();
  STDMETHOD_(ULONG, Release)();
  STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice);
  STDMETHOD(SetPrivateData)(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags);
  STDMETHOD(GetPrivateData)(REFGUID refguid, void* pData, DWORD* pSizeOfData);
  STDMETHOD(FreePrivateData)(REFGUID refguid);
  STDMETHOD_(DWORD, SetPriority)(DWORD PriorityNew);
  STDMETHOD_(DWORD, GetPriority)();
  STDMETHOD_(void, PreLoad)();
  STDMETHOD_(D3DRESOURCETYPE, GetType)();
  STDMETHOD_(DWORD, SetLOD)(DWORD LODNew);
  STDMETHOD_(DWORD, GetLOD)();
  STDMETHOD_(DWORD, GetLevelCount)();

  STDMETHOD(GetLevelDesc)(UINT Level, D3DSURFACE_DESC* pDesc);
  STDMETHOD(GetCubeMapSurface)(D3DCUBEMAP_FACES FaceType, UINT Level, IDirect3DSurface8** ppCubeMapSurface);
  STDMETHOD(LockRect)(D3DCUBEMAP_FACES FaceType, UINT Level, D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD Flags);
  STDMETHOD(UnlockRect)(D3DCUBEMAP_FACES FaceType, UINT Level);
  STDMETHOD(AddDirtyRect)(D3DCUBEMAP_FACES FaceType, const RECT* pDirtyRect);
};

class QD3D8VolumeTexture8 : public IDirect3DVolumeTexture8, public QD3D8BaseTexture8
{
public:
  UINT m_width;
  UINT m_height;
  UINT m_depth;
  D3DFORMAT m_format;

  QD3D8VolumeTexture8(QD3D8Device8* dev, UINT w, UINT h, UINT d, UINT levels, D3DFORMAT fmt);
  virtual ~QD3D8VolumeTexture8();

  STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj);
  STDMETHOD_(ULONG, AddRef)();
  STDMETHOD_(ULONG, Release)();
  STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice);
  STDMETHOD(SetPrivateData)(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags);
  STDMETHOD(GetPrivateData)(REFGUID refguid, void* pData, DWORD* pSizeOfData);
  STDMETHOD(FreePrivateData)(REFGUID refguid);
  STDMETHOD_(DWORD, SetPriority)(DWORD PriorityNew);
  STDMETHOD_(DWORD, GetPriority)();
  STDMETHOD_(void, PreLoad)();
  STDMETHOD_(D3DRESOURCETYPE, GetType)();
  STDMETHOD_(DWORD, SetLOD)(DWORD LODNew);
  STDMETHOD_(DWORD, GetLOD)();
  STDMETHOD_(DWORD, GetLevelCount)();

  STDMETHOD(GetLevelDesc)(UINT Level, D3DVOLUME_DESC* pDesc);
  STDMETHOD(GetVolumeLevel)(UINT Level, IDirect3DVolume8** ppVolumeLevel);
  STDMETHOD(LockBox)(UINT Level, D3DLOCKED_BOX* pLockedVolume, const D3DBOX* pBox, DWORD Flags);
  STDMETHOD(UnlockBox)(UINT Level);
  STDMETHOD(AddDirtyBox)(const D3DBOX* pDirtyBox);
};

class QD3D8Volume8 : public IDirect3DVolume8, public QD3D8ResourceBase
{
public:
  UINT m_width;
  UINT m_height;
  UINT m_depth;
  D3DFORMAT m_format;
  std::vector<uint8_t> m_sysmem;
  QD3D8LockedRegion m_lock;

  QD3D8Volume8(QD3D8Device8* dev, UINT w, UINT h, UINT d, D3DFORMAT fmt);
  virtual ~QD3D8Volume8();

  STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj);
  STDMETHOD_(ULONG, AddRef)();
  STDMETHOD_(ULONG, Release)();
  STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice);
  STDMETHOD(SetPrivateData)(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags);
  STDMETHOD(GetPrivateData)(REFGUID refguid, void* pData, DWORD* pSizeOfData);
  STDMETHOD(FreePrivateData)(REFGUID refguid);
  STDMETHOD_(DWORD, SetPriority)(DWORD PriorityNew);
  STDMETHOD_(DWORD, GetPriority)();
  STDMETHOD_(void, PreLoad)();
  STDMETHOD_(D3DRESOURCETYPE, GetType)();
  STDMETHOD(GetContainer)(REFIID riid, void** ppContainer);
  STDMETHOD(GetDesc)(D3DVOLUME_DESC* pDesc);
  STDMETHOD(LockBox)(D3DLOCKED_BOX* pLockedVolume, const D3DBOX* pBox, DWORD Flags);
  STDMETHOD(UnlockBox)();
};

class QD3D8VertexBuffer8 : public IDirect3DVertexBuffer8, public QD3D8ResourceBase
{
public:
  GLuint m_glBuffer;
  std::vector<uint8_t> m_data;
  UINT m_length;
  DWORD m_usage;
  DWORD m_fvf;
  D3DPOOL m_pool;
  bool m_locked;
  bool m_dirty;
  UINT m_lockOffset;
  UINT m_lockSize;

  QD3D8VertexBuffer8(QD3D8Device8* dev, UINT length, DWORD usage, DWORD fvf, D3DPOOL pool);
  virtual ~QD3D8VertexBuffer8();

  STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj);
  STDMETHOD_(ULONG, AddRef)();
  STDMETHOD_(ULONG, Release)();
  STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice);
  STDMETHOD(SetPrivateData)(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags);
  STDMETHOD(GetPrivateData)(REFGUID refguid, void* pData, DWORD* pSizeOfData);
  STDMETHOD(FreePrivateData)(REFGUID refguid);
  STDMETHOD_(DWORD, SetPriority)(DWORD PriorityNew);
  STDMETHOD_(DWORD, GetPriority)();
  STDMETHOD_(void, PreLoad)();
  STDMETHOD_(D3DRESOURCETYPE, GetType)();
  STDMETHOD(Lock)(UINT OffsetToLock, UINT SizeToLock, BYTE** ppbData, DWORD Flags);
  STDMETHOD(Unlock)();
  STDMETHOD(GetDesc)(D3DVERTEXBUFFER_DESC* pDesc);
};

class QD3D8IndexBuffer8 : public IDirect3DIndexBuffer8, public QD3D8ResourceBase
{
public:
  GLuint m_glBuffer;
  std::vector<uint8_t> m_data;
  UINT m_length;
  DWORD m_usage;
  D3DFORMAT m_format;
  D3DPOOL m_pool;
  bool m_locked;
  bool m_dirty;
  UINT m_lockOffset;
  UINT m_lockSize;

  QD3D8IndexBuffer8(QD3D8Device8* dev, UINT length, DWORD usage, D3DFORMAT fmt, D3DPOOL pool);
  virtual ~QD3D8IndexBuffer8();

  STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj);
  STDMETHOD_(ULONG, AddRef)();
  STDMETHOD_(ULONG, Release)();
  STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice);
  STDMETHOD(SetPrivateData)(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags);
  STDMETHOD(GetPrivateData)(REFGUID refguid, void* pData, DWORD* pSizeOfData);
  STDMETHOD(FreePrivateData)(REFGUID refguid);
  STDMETHOD_(DWORD, SetPriority)(DWORD PriorityNew);
  STDMETHOD_(DWORD, GetPriority)();
  STDMETHOD_(void, PreLoad)();
  STDMETHOD_(D3DRESOURCETYPE, GetType)();
  STDMETHOD(Lock)(UINT OffsetToLock, UINT SizeToLock, BYTE** ppbData, DWORD Flags);
  STDMETHOD(Unlock)();
  STDMETHOD(GetDesc)(D3DINDEXBUFFER_DESC* pDesc);
};

class QD3D8SwapChain8 : public IDirect3DSwapChain8, public QD3D8Unknown
{
public:
  QD3D8Device8* m_device;
  QD3D8SwapChain8(QD3D8Device8* device);
  virtual ~QD3D8SwapChain8();

  STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj);
  STDMETHOD_(ULONG, AddRef)();
  STDMETHOD_(ULONG, Release)();
  STDMETHOD(Present)(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion);
  STDMETHOD(GetBackBuffer)(UINT BackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface8** ppBackBuffer);
};

class QD3D8Device8 : public IDirect3DDevice8, public QD3D8Unknown
{
public:
  struct ShaderStub
  {
    std::vector<DWORD> declaration;
    std::vector<DWORD> function;
  };

  HWND m_hwnd;
  HDC m_hdc;
  HGLRC m_glrc;
  D3DPRESENT_PARAMETERS m_pp;
  QD3D8Direct3D8* m_parent;

  D3DMATRIX m_world;
  D3DMATRIX m_view;
  D3DMATRIX m_proj;
  D3DVIEWPORT8 m_viewport;
  D3DMATERIAL8 m_material;
  D3DCLIPSTATUS8 m_clipStatus;

  DWORD m_renderStates[256];
  QD3D8TextureStageState m_stageStates[8];
  IDirect3DBaseTexture8* m_boundTextures[8];
  QD3D8StreamSource m_streams[16];
  QD3D8IndexBuffer8* m_indices;
  UINT m_baseVertexIndex;
  DWORD m_currentVertexShader;
  DWORD m_currentPixelShader;
  DWORD m_currentFVF;
  std::map<DWORD, ShaderStub> m_vertexShaders;
  std::map<DWORD, ShaderStub> m_pixelShaders;
  DWORD m_nextShaderHandle;

  QD3D8Surface8* m_backBuffer;
  QD3D8Surface8* m_depthBuffer;
  QD3D8Surface8* m_renderTarget;
  QD3D8Surface8* m_depthStencil;
  GLuint m_fbo;
  bool m_sceneBegun;

  D3DLIGHT8 m_lights[8];
  BOOL m_lightEnabled[8];
  float m_clipPlanes[6][4];
  PALETTEENTRY m_palettes[256][256];
  UINT m_currentPalette;
  POINT m_cursorPos;
  bool m_cursorVisible;

  QD3D8Device8(QD3D8Direct3D8* parent, HWND hwnd, const D3DPRESENT_PARAMETERS& pp);
  virtual ~QD3D8Device8();

  HRESULT InitializeGL();
  void ShutdownGL();
  void ApplyRenderStates();
  void ApplyTextureStageStates();
  void ApplyTransforms();
  void ApplyViewport();
  void SyncFramebufferBinding();
  void BindTextureStage(DWORD stage, IDirect3DBaseTexture8* tex);
  HRESULT UploadVB(QD3D8VertexBuffer8* vb);
  HRESULT UploadIB(QD3D8IndexBuffer8* ib);
  GLenum TranslatePrimitiveType(D3DPRIMITIVETYPE pt, UINT primitiveCount, GLsizei* outCount) const;
  UINT PrimitiveVertexCount(D3DPRIMITIVETYPE pt, UINT primitiveCount) const;
  UINT PrimitiveIndexCount(D3DPRIMITIVETYPE pt, UINT primitiveCount) const;
  HRESULT SetupFVFStream(DWORD fvf, const uint8_t* base, UINT stride);
  HRESULT SetupStreamSources(INT baseVertexIndex);
  void DisableAllClientStates();
  void ApplyMaterialFixedFunction();
  void ApplyLightsFixedFunction();

  STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj);
  STDMETHOD_(ULONG, AddRef)();
  STDMETHOD_(ULONG, Release)();
  STDMETHOD(TestCooperativeLevel)();
  STDMETHOD_(UINT, GetAvailableTextureMem)();
  STDMETHOD(ResourceManagerDiscardBytes)(DWORD Bytes);
  STDMETHOD(GetDirect3D)(IDirect3D8** ppD3D8);
  STDMETHOD(GetDeviceCaps)(D3DCAPS8* pCaps);
  STDMETHOD(GetDisplayMode)(D3DDISPLAYMODE* pMode);
  STDMETHOD(GetCreationParameters)(D3DDEVICE_CREATION_PARAMETERS* pParameters);
  STDMETHOD(SetCursorProperties)(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface8* pCursorBitmap);
  STDMETHOD_(void, SetCursorPosition)(UINT XScreenSpace, UINT YScreenSpace, DWORD Flags);
  STDMETHOD_(BOOL, ShowCursor)(BOOL bShow);
  STDMETHOD(CreateAdditionalSwapChain)(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain8** pSwapChain);
  STDMETHOD(Reset)(D3DPRESENT_PARAMETERS* pPresentationParameters);
  STDMETHOD(Present)(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion);
  STDMETHOD(GetBackBuffer)(UINT BackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface8** ppBackBuffer);
  STDMETHOD(GetRasterStatus)(D3DRASTER_STATUS* pRasterStatus);
  STDMETHOD_(void, SetGammaRamp)(DWORD Flags, const D3DGAMMARAMP* pRamp);
  STDMETHOD_(void, GetGammaRamp)(D3DGAMMARAMP* pRamp);
  STDMETHOD(CreateTexture)(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture8** ppTexture);
  STDMETHOD(CreateVolumeTexture)(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture8** ppVolumeTexture);
  STDMETHOD(CreateCubeTexture)(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture8** ppCubeTexture);
  STDMETHOD(CreateVertexBuffer)(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer8** ppVertexBuffer);
  STDMETHOD(CreateIndexBuffer)(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer8** ppIndexBuffer);
  STDMETHOD(CreateRenderTarget)(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, BOOL Lockable, IDirect3DSurface8** ppSurface);
  STDMETHOD(CreateDepthStencilSurface)(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, IDirect3DSurface8** ppSurface);
  STDMETHOD(CreateImageSurface)(UINT Width, UINT Height, D3DFORMAT Format, IDirect3DSurface8** ppSurface);
  STDMETHOD(CopyRects)(IDirect3DSurface8* pSourceSurface, const RECT* pSourceRectsArray, UINT cRects, IDirect3DSurface8* pDestinationSurface, const POINT* pDestPointsArray);
  STDMETHOD(UpdateTexture)(IDirect3DBaseTexture8* pSourceTexture, IDirect3DBaseTexture8* pDestinationTexture);
  STDMETHOD(GetFrontBuffer)(IDirect3DSurface8* pDestSurface);
  STDMETHOD(SetRenderTarget)(IDirect3DSurface8* pRenderTarget, IDirect3DSurface8* pNewZStencil);
  STDMETHOD(GetRenderTarget)(IDirect3DSurface8** ppRenderTarget);
  STDMETHOD(GetDepthStencilSurface)(IDirect3DSurface8** ppZStencilSurface);
  STDMETHOD(BeginScene)();
  STDMETHOD(EndScene)();
  STDMETHOD(Clear)(DWORD Count, const D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil);
  STDMETHOD(SetTransform)(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix);
  STDMETHOD(GetTransform)(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix);
  STDMETHOD(MultiplyTransform)(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix);
  STDMETHOD(SetViewport)(const D3DVIEWPORT8* pViewport);
  STDMETHOD(GetViewport)(D3DVIEWPORT8* pViewport);
  STDMETHOD(SetMaterial)(const D3DMATERIAL8* pMaterial);
  STDMETHOD(GetMaterial)(D3DMATERIAL8* pMaterial);
  STDMETHOD(SetLight)(DWORD Index, const D3DLIGHT8* pLight);
  STDMETHOD(GetLight)(DWORD Index, D3DLIGHT8* pLight);
  STDMETHOD(LightEnable)(DWORD Index, BOOL Enable);
  STDMETHOD(GetLightEnable)(DWORD Index, BOOL* pEnable);
  STDMETHOD(SetClipPlane)(DWORD Index, const float* pPlane);
  STDMETHOD(GetClipPlane)(DWORD Index, float* pPlane);
  STDMETHOD(SetRenderState)(D3DRENDERSTATETYPE State, DWORD Value);
  STDMETHOD(GetRenderState)(D3DRENDERSTATETYPE State, DWORD* pValue);
  STDMETHOD(BeginStateBlock)();
  STDMETHOD(EndStateBlock)(DWORD* pToken);
  STDMETHOD(ApplyStateBlock)(DWORD Token);
  STDMETHOD(CaptureStateBlock)(DWORD Token);
  STDMETHOD(DeleteStateBlock)(DWORD Token);
  STDMETHOD(CreateStateBlock)(D3DSTATEBLOCKTYPE Type, DWORD* pToken);
  STDMETHOD(SetClipStatus)(const D3DCLIPSTATUS8* pClipStatus);
  STDMETHOD(GetClipStatus)(D3DCLIPSTATUS8* pClipStatus);
  STDMETHOD(GetTexture)(DWORD Stage, IDirect3DBaseTexture8** ppTexture);
  STDMETHOD(SetTexture)(DWORD Stage, IDirect3DBaseTexture8* pTexture);
  STDMETHOD(GetTextureStageState)(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue);
  STDMETHOD(SetTextureStageState)(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value);
  STDMETHOD(ValidateDevice)(DWORD* pNumPasses);
  STDMETHOD(GetInfo)(DWORD DevInfoID, void* pDevInfoStruct, DWORD DevInfoStructSize);
  STDMETHOD(SetPaletteEntries)(UINT PaletteNumber, const PALETTEENTRY* pEntries);
  STDMETHOD(GetPaletteEntries)(UINT PaletteNumber, PALETTEENTRY* pEntries);
  STDMETHOD(SetCurrentTexturePalette)(UINT PaletteNumber);
  STDMETHOD(GetCurrentTexturePalette)(UINT* PaletteNumber);
  STDMETHOD(DrawPrimitive)(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount);
  STDMETHOD(DrawIndexedPrimitive)(D3DPRIMITIVETYPE PrimitiveType, UINT minIndex, UINT NumVertices, UINT startIndex, UINT primCount);
  STDMETHOD(DrawPrimitiveUP)(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride);
  STDMETHOD(DrawIndexedPrimitiveUP)(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertexIndices, UINT PrimitiveCount, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride);
  STDMETHOD(ProcessVertices)(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer8* pDestBuffer, DWORD Flags);
  STDMETHOD(CreateVertexShader)(const DWORD* pDeclaration, const DWORD* pFunction, DWORD* pHandle, DWORD Usage);
  STDMETHOD(SetVertexShader)(DWORD Handle);
  STDMETHOD(GetVertexShader)(DWORD* pHandle);
  STDMETHOD(DeleteVertexShader)(DWORD Handle);
  STDMETHOD(SetVertexShaderConstant)(DWORD Register, const void* pConstantData, DWORD ConstantCount);
  STDMETHOD(GetVertexShaderConstant)(DWORD Register, void* pConstantData, DWORD ConstantCount);
  STDMETHOD(GetVertexShaderDeclaration)(DWORD Handle, void* pData, DWORD* pSizeOfData);
  STDMETHOD(GetVertexShaderFunction)(DWORD Handle, void* pData, DWORD* pSizeOfData);
  STDMETHOD(SetStreamSource)(UINT StreamNumber, IDirect3DVertexBuffer8* pStreamData, UINT Stride);
  STDMETHOD(GetStreamSource)(UINT StreamNumber, IDirect3DVertexBuffer8** ppStreamData, UINT* pStride);
  STDMETHOD(SetIndices)(IDirect3DIndexBuffer8* pIndexData, UINT BaseVertexIndex);
  STDMETHOD(GetIndices)(IDirect3DIndexBuffer8** ppIndexData, UINT* pBaseVertexIndex);
  STDMETHOD(CreatePixelShader)(const DWORD* pFunction, DWORD* pHandle);
  STDMETHOD(SetPixelShader)(DWORD Handle);
  STDMETHOD(GetPixelShader)(DWORD* pHandle);
  STDMETHOD(DeletePixelShader)(DWORD Handle);
  STDMETHOD(SetPixelShaderConstant)(DWORD Register, const void* pConstantData, DWORD ConstantCount);
  STDMETHOD(GetPixelShaderConstant)(DWORD Register, void* pConstantData, DWORD ConstantCount);
  STDMETHOD(GetPixelShaderFunction)(DWORD Handle, void* pData, DWORD* pSizeOfData);
  STDMETHOD(DrawRectPatch)(UINT Handle, const float* pNumSegs, const D3DRECTPATCH_INFO* pRectPatchInfo);
  STDMETHOD(DrawTriPatch)(UINT Handle, const float* pNumSegs, const D3DTRIPATCH_INFO* pTriPatchInfo);
  STDMETHOD(DeletePatch)(UINT Handle);
};

QGLFormatInfo QD3D8_TranslateFormat(D3DFORMAT fmt);
UINT QD3D8_CalcBitsPerPixel(D3DFORMAT fmt);
UINT QD3D8_CalcMipCount(UINT w, UINT h);
void QD3D8_IdentityMatrix(D3DMATRIX * m);
D3DMATRIX QD3D8_MultiplyMatrix(const D3DMATRIX & a, const D3DMATRIX & b);
BOOL QD3D8_IsFVFHandle(DWORD handle);

extern "C" IDirect3D8* WINAPI Direct3DCreate8(UINT SDKVersion);
