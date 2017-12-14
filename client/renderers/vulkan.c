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

#include "lg-renderer.h"
#include <vulkan/vulkan.h>

#include "debug.h"

struct LGR_Vulkan
{
  LG_RendererParams params;
  bool              configured;
};

const char * lgr_vulkan_get_name()
{
  return "Vulkan";
}

bool lgr_vulkan_initialize(void ** opaque, const LG_RendererParams params, Uint32 * sdlFlags)
{
  // create our local storage
  *opaque = malloc(sizeof(struct LGR_Vulkan));
  if (!*opaque)
  {
    DEBUG_INFO("Failed to allocate %lu bytes", sizeof(struct LGR_Vulkan));
    return false;
  }
  memset(*opaque, 0, sizeof(struct LGR_Vulkan));

  struct LGR_Vulkan * this = (struct LGR_Vulkan *)*opaque;
  memcpy(&this->params, &params, sizeof(LG_RendererParams));

  return true;
}

bool lgr_vulkan_configure(void * opaque, SDL_Window *window, const LG_RendererFormat format)
{
  return false;
}

void lgr_vulkan_deconfigure(void * opaque)
{
  struct LGR_Vulkan * this = (struct LGR_Vulkan *)opaque;
  if (!this || !this->configured)
    return;

  this->configured = false;
}

void lgr_vulkan_deinitialize(void * opaque)
{
  struct LGR_Vulkan * this = (struct LGR_Vulkan *)opaque;
  if (!this)
    return;

  if (this->configured)
    lgr_vulkan_deconfigure(opaque);

  free(this);
}

bool lgr_vulkan_is_compatible(void * opaque, const LG_RendererFormat format)
{
  struct LGR_Vulkan * this = (struct LGR_Vulkan *)opaque;
  if (!this || !this->configured)
    return false;

  return false;
}

void lgr_vulkan_on_resize(void * opaque, const int width, const int height, const LG_RendererRect destRect)
{
  struct LGR_Vulkan * this = (struct LGR_Vulkan *)opaque;
  if (!this || !this->configured)
    return;

}

bool lgr_vulkan_on_mouse_shape(void * opaque, const LG_RendererCursor cursor, const int width, const int height, const int pitch, const uint8_t * data)
{
  struct LGR_Vulkan * this = (struct LGR_Vulkan *)opaque;
  if (!this || !this->configured)
    return false;

  return false;
}

bool lgr_vulkan_on_mouse_event(void * opaque, const bool visible , const int x, const int y)
{
  struct LGR_Vulkan * this = (struct LGR_Vulkan *)opaque;
  if (!this || !this->configured)
    return false;

  return false;
}

bool lgr_vulkan_on_frame_event(void * opaque, const uint8_t * data)
{
  struct LGR_Vulkan * this = (struct LGR_Vulkan *)opaque;
  if (!this || !this->configured)
    return false;

  return false;
}

bool lgr_vulkan_render(void * opaque)
{
  struct LGR_Vulkan * this = (struct LGR_Vulkan *)opaque;
  if (!this || !this->configured)
    return false;

  return false;
}

const LG_Renderer LGR_Vulkan =
{
  .get_name       = lgr_vulkan_get_name,
  .initialize     = lgr_vulkan_initialize,
  .configure      = lgr_vulkan_configure,
  .deconfigure    = lgr_vulkan_deconfigure,
  .deinitialize   = lgr_vulkan_deinitialize,
  .is_compatible  = lgr_vulkan_is_compatible,
  .on_resize      = lgr_vulkan_on_resize,
  .on_mouse_shape = lgr_vulkan_on_mouse_shape,
  .on_mouse_event = lgr_vulkan_on_mouse_event,
  .on_frame_event = lgr_vulkan_on_frame_event,
  .render         = lgr_vulkan_render
};