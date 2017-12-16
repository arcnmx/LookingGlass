/*
Looking Glass - KVM FrameRelay (KVMFR) Client
Copyright (C) 2017 Geoffrey McRae <geoff@hostfission.com>
https://looking-glass.hostfission.com

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "Capture/DXGI.h"
using namespace Capture;

#include "common/debug.h"
#include "common/memcpySSE.h"

DXGI::DXGI() :
  m_options(NULL),
  m_initialized(false),
  m_dxgiFactory(),
  m_device(),
  m_deviceContext(),
  m_dup(),
  m_texture(),
  m_surface(),
  m_pointer(NULL)
{
}

DXGI::~DXGI()
{

}

bool DXGI::Initialize(CaptureOptions * options)
{
  if (m_initialized)
    DeInitialize();

  m_options = options;
  HRESULT status;

  status = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)(&m_dxgiFactory));
  if (FAILED(status))
  {
    DEBUG_ERROR("Failed to create DXGIFactory: %08x", (int)status);
    return false;
  }

  bool done = false;
  IDXGIAdapter1Ptr adapter;
  for (int i = 0; m_dxgiFactory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++)
  {
    IDXGIOutputPtr output;
    for (int i = 0; adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND; i++)
    {
      DXGI_OUTPUT_DESC outputDesc;
      output->GetDesc(&outputDesc);
      if (!outputDesc.AttachedToDesktop)
      {
        output.Release();
        continue;
      }

      m_output = output;
      if (!m_output)
      {
        output.Release();
        adapter.Release();
        DEBUG_ERROR("Failed to get IDXGIOutput1");
        DeInitialize();
        return false;
      }

      m_width  = outputDesc.DesktopCoordinates.right  - outputDesc.DesktopCoordinates.left;
      m_height = outputDesc.DesktopCoordinates.bottom - outputDesc.DesktopCoordinates.top;

      output.Release();
      done = true;
      break;
    }

    if (done)
      break;

    adapter.Release();
  }

  if (!done)
  {
    DEBUG_ERROR("Failed to locate a valid output device");
    DeInitialize();
    return false;
  }

  static const D3D_FEATURE_LEVEL featureLevels[] = {
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0,
    D3D_FEATURE_LEVEL_9_3,
    D3D_FEATURE_LEVEL_9_2,
    D3D_FEATURE_LEVEL_9_1
  };

  #if DEBUG
    #define CREATE_FLAGS (D3D11_CREATE_DEVICE_DEBUG)
  #else
    #define CREATE_FLAGS (0)
  #endif

  status = D3D11CreateDevice(
    adapter,
    D3D_DRIVER_TYPE_UNKNOWN,
    NULL,
    CREATE_FLAGS,
    featureLevels, ARRAYSIZE(featureLevels),
    D3D11_SDK_VERSION,
    &m_device,
    &m_featureLevel,
    &m_deviceContext
  );

#undef CREATE_FLAGS

  adapter.Release();

  if (FAILED(status))
  {
    DEBUG_ERROR("Failed to create D3D11 device");
    DeInitialize();
    return false;
  }

  // we try this twice just incase we still get an error
  // on re-initialization
  for(int i = 0; i < 2; ++i)
  {
    status = m_output->DuplicateOutput(m_device, &m_dup);
    if (SUCCEEDED(status))
      break;
    Sleep(200);
  }

  if (FAILED(status))
  {
    DEBUG_ERROR("DuplicateOutput Failed: %08x", (int)status);
    DeInitialize();
    return false;
  }

  D3D11_TEXTURE2D_DESC texDesc;
  ZeroMemory(&texDesc, sizeof(texDesc));
  texDesc.Width              = m_width;
  texDesc.Height             = m_height;
  texDesc.MipLevels          = 1;
  texDesc.ArraySize          = 1;
  texDesc.SampleDesc.Count   = 1;
  texDesc.SampleDesc.Quality = 0;
  texDesc.Usage              = D3D11_USAGE_STAGING;
  texDesc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
  texDesc.BindFlags          = 0;
  texDesc.CPUAccessFlags     = D3D11_CPU_ACCESS_READ;
  texDesc.MiscFlags          = 0;

  status = m_device->CreateTexture2D(&texDesc, NULL, &m_texture);
  if (FAILED(status))
  {
    DEBUG_ERROR("Failed to create texture: %08x", (int)status);
    DeInitialize();
    return false;
  }
  
  m_initialized = true;
  return true;
}

void DXGI::DeInitialize()
{
  if (m_pointer)
  {
    delete[] m_pointer;
    m_pointer = NULL;
    m_pointerBufSize = 0;
  }

  if (m_surfaceMapped)
  {
    m_surface->Unmap();
    m_surfaceMapped = false;
  }

  if (m_surface)
    m_surface.Release();

  if (m_texture)
    m_texture.Release();

  if (m_dup)
    m_dup.Release();

  if (m_output)
    m_output.Release();

  if (m_deviceContext)
    m_deviceContext.Release();

  if (m_device)
    m_device.Release();

  if (m_dxgiFactory)
    m_dxgiFactory.Release();

  m_initialized = false;
}

FrameType DXGI::GetFrameType()
{
  if (!m_initialized)
    return FRAME_TYPE_INVALID;

  return FRAME_TYPE_ARGB;
}

size_t DXGI::GetMaxFrameSize()
{
  if (!m_initialized)
    return 0;

  return (m_width * m_height * 4);
}

GrabStatus DXGI::GrabFrame(FrameInfo & frame)
{
  static TimeReport reportGrabFrame("GrabFrame");
  reportGrabFrame.go();
  if (!m_initialized)
    return GRAB_STATUS_ERROR;

  DXGI_OUTDUPL_FRAME_INFO frameInfo;
  IDXGIResourcePtr res;

  HRESULT status;
  bool    cursorUpdate = false;
  for(int i = 0; i < 2; ++i)
  {
    while(true)
    {
      status = m_dup->AcquireNextFrame(1000, &frameInfo, &res);
      if (status == DXGI_ERROR_WAIT_TIMEOUT)
      {
        if (!m_surfaceMapped)
          break;

        // send the last frame again if we timeout to prevent the client stalling on restart
        frame.width = m_desc.Width;
        frame.height = m_desc.Height;
        frame.pitch  = m_rect.Pitch;
        frame.stride = m_rect.Pitch / 4;

        unsigned int size = m_height * m_rect.Pitch;
        memcpySSE(frame.buffer, m_rect.pBits, size < frame.bufferSize ? size : frame.bufferSize);
        return GRAB_STATUS_OK;
      }

      if (!SUCCEEDED(status))
        break;

      // if we have a mouse update
      if (frameInfo.LastMouseUpdateTime.QuadPart)
      {
        if (
          m_lastMousePos.x != frameInfo.PointerPosition.Position.x ||
          m_lastMousePos.y != frameInfo.PointerPosition.Position.y
        ) {
          cursorUpdate         = true;
          frame.cursor.hasPos  = true;
          frame.cursor.x       = frameInfo.PointerPosition.Position.x;
          frame.cursor.y       = frameInfo.PointerPosition.Position.y;
          m_lastMousePos.x     = frameInfo.PointerPosition.Position.x;
          m_lastMousePos.y     = frameInfo.PointerPosition.Position.y;
        }

        if (m_lastMouseVis != frameInfo.PointerPosition.Visible)
        {
          cursorUpdate   = true;
          m_lastMouseVis = frameInfo.PointerPosition.Visible;
        }

        frame.cursor.visible = m_lastMouseVis == TRUE;
      }

      // if the pointer shape has changed
      if (frameInfo.PointerShapeBufferSize > 0)
      {
        cursorUpdate = true;
        if (m_pointerBufSize < frameInfo.PointerShapeBufferSize)
        {
          if (m_pointer)
            delete[] m_pointer;
          m_pointer        = new BYTE[frameInfo.PointerShapeBufferSize];
          m_pointerBufSize = frameInfo.PointerShapeBufferSize;
        }

        DXGI_OUTDUPL_POINTER_SHAPE_INFO shapeInfo;
        status = m_dup->GetFramePointerShape(m_pointerBufSize, m_pointer, &m_pointerSize, &shapeInfo);
        if (!SUCCEEDED(status))
        {
          m_dup->ReleaseFrame();
          DEBUG_ERROR("Failed to get the new pointer shape: %08x", (int)status);
          return GRAB_STATUS_ERROR;
        }

        switch (shapeInfo.Type)
        {
          case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR       : frame.cursor.type = CURSOR_TYPE_COLOR;        break;
          case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR: frame.cursor.type = CURSOR_TYPE_MASKED_COLOR; break;
          case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME  : frame.cursor.type = CURSOR_TYPE_MONOCHROME;   break;
          default:
            DEBUG_ERROR("Invalid cursor type");
            return GRAB_STATUS_ERROR;
        }

        frame.cursor.hasShape = true;
        frame.cursor.shape    = m_pointer;
        frame.cursor.w        = shapeInfo.Width;
        frame.cursor.h        = shapeInfo.Height;
        frame.cursor.pitch    = shapeInfo.Pitch;
        frame.cursor.dataSize = m_pointerSize;
      }

      if (frameInfo.AccumulatedFrames > 0)
        break;

      m_dup->ReleaseFrame();
      res.Release();

      if (cursorUpdate)
        return GRAB_STATUS_CURSOR;
    }

    if (SUCCEEDED(status))
      break;
    
    HDESK desktop = NULL;
    switch (status)
    {      
      // desktop switch, mode change, switch DWM on or off or Secure Desktop
      case DXGI_ERROR_ACCESS_LOST:
        // see if we can open the desktop, if so request a reinit
        // if not the secure desktop is active so just wait for it
        // instead of aborting out
        desktop = OpenInputDesktop(0, TRUE, GENERIC_READ);
        if (desktop)
        {
          // open suceeded, not on the secure desktop, return to reinit
          CloseDesktop(desktop);
          return GRAB_STATUS_REINIT;
        }
        // fall through

      // this can happen during desktop switches also, not documented by MS though
      case WAIT_ABANDONED:
        do
        {
          desktop = OpenInputDesktop(0, TRUE, GENERIC_READ);
          if (desktop)
            break;
          Sleep(100);
        } while (!desktop);
        CloseDesktop(desktop);
        return GRAB_STATUS_REINIT;

      default:
        // unknown failure
        DEBUG_INFO("AcquireNextFrame failed: %08x", (int)status);
        return GRAB_STATUS_ERROR;
    }
  }

  // retry count exceeded
  if (FAILED(status))
  {
    m_dup->ReleaseFrame();
    DEBUG_ERROR("Failed to acquire next frame");
    return GRAB_STATUS_ERROR;
  }

  ID3D11Texture2DPtr src(res);
  if (!src)
  {
    m_dup->ReleaseFrame();
    DEBUG_ERROR("Failed to get src ID3D11Texture2D");
    return GRAB_STATUS_ERROR;
  }

  src->GetDesc(&m_desc);

  m_deviceContext->CopyResource(m_texture, src);

  m_dup->ReleaseFrame();
  res.Release();
  src.Release();

  if (m_surfaceMapped)
  {
    status = m_surface->Unmap();
    if (FAILED(status))
    {
      DEBUG_ERROR("Failed to unmap surface: %08x", (int)status);
      return GRAB_STATUS_ERROR;
    }
    m_surfaceMapped = false;
    m_surface.Release();
  }

  m_surface = m_texture;
  if (!m_surface)
  {
    DEBUG_ERROR("Failed to get IDXGISurface1");
    return GRAB_STATUS_ERROR;
  }

  status = m_surface->Map(&m_rect, DXGI_MAP_READ);
  if (FAILED(status))
  {
    DEBUG_ERROR("Failed to map surface: %08x", (int)status);
    return GRAB_STATUS_ERROR;
  }
  m_surfaceMapped = true;

  m_width  = m_desc.Width;
  m_height = m_desc.Height;

  frame.width   = m_desc.Width;
  frame.height  = m_desc.Height;
  frame.pitch   = m_rect.Pitch;
  frame.stride  = m_rect.Pitch / 4;

  unsigned int size = m_height * m_rect.Pitch;
  memcpySSE(frame.buffer, m_rect.pBits, size < frame.bufferSize ? size : frame.bufferSize);

  reportGrabFrame.commit();

  return GRAB_STATUS_OK;
}