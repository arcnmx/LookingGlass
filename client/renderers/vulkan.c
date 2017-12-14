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
#include <SDL2/SDL_syswm.h>
#include <X11/Xlib-xcb.h>

#include "debug.h"

typedef struct QueueIndicies
{
  int graphics;
  int present;
}
QueueIndicies;

struct LGR_Vulkan
{
  LG_RendererParams params;
  bool              configured;

  bool              freeInstance;
  VkInstance        instance;

  bool              freeSurface;
  VkSurfaceKHR      surface;
  bool              freeDevice;
  VkDevice          device;

  VkPhysicalDevice  physicalDevice;
  QueueIndicies     queues;
  VkQueue           graphics_q;
  VkQueue           present_q;
};

// forwards
static bool create_instance      (struct LGR_Vulkan * this);
static bool create_surface       (struct LGR_Vulkan * this, SDL_Window * window);
static bool pick_physical_device (struct LGR_Vulkan * this);
static bool create_logical_device(struct LGR_Vulkan * this);

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

  return
    create_instance(this);
}

bool lgr_vulkan_configure(void * opaque, SDL_Window *window, const LG_RendererFormat format)
{
  struct LGR_Vulkan * this = (struct LGR_Vulkan *)opaque;
  if (!this)
    return false;

  if (this->configured)
  {
    DEBUG_ERROR("Already configured, call deconfigure first");
    return false;
  }

  this->configured =
    create_surface       (this, window) &&
    pick_physical_device (this) &&
    create_logical_device(this);

  return this->configured;
}

void lgr_vulkan_deconfigure(void * opaque)
{
  struct LGR_Vulkan * this = (struct LGR_Vulkan *)opaque;
  if (!this)
    return;

  if (this->freeDevice)
  {
    vkDestroyDevice(this->device, NULL);
    this->freeDevice = false;
  }

  if (this->freeSurface)
  {
    vkDestroySurfaceKHR(this->instance, this->surface, NULL);
    this->freeSurface = false;
  }

  this->configured = false;
}

void lgr_vulkan_deinitialize(void * opaque)
{
  struct LGR_Vulkan * this = (struct LGR_Vulkan *)opaque;
  if (!this)
    return;

  if (this->configured)
    lgr_vulkan_deconfigure(opaque);

  if (this->freeInstance)
    vkDestroyInstance(this->instance, NULL);

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

static bool create_instance(struct LGR_Vulkan * this)
{
  VkApplicationInfo appInfo;
  memset(&appInfo, 0, sizeof(VkApplicationInfo));
  appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName   = "Looking Glass";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName        = "No Engine";
  appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion         = VK_API_VERSION_1_0;

  const char * extensionNames[2] =
  {
    "VK_KHR_surface",
    "VK_KHR_xlib_surface",
  };

  const char * layerNames[0] =
  {
  };

  VkInstanceCreateInfo createInfo;
  memset(&createInfo, 0, sizeof(VkInstanceCreateInfo));
  createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo        = &appInfo;
  createInfo.enabledExtensionCount   = 2;
  createInfo.ppEnabledExtensionNames = extensionNames;
  createInfo.enabledLayerCount       = 0;
  createInfo.ppEnabledLayerNames     = layerNames;

  if (vkCreateInstance(&createInfo, NULL, &this->instance) != VK_SUCCESS)
  {
    DEBUG_ERROR("Failed to create the instance");
    return false;
  }

  this->freeInstance = true;
  return true;
}

static bool pick_physical_device(struct LGR_Vulkan * this)
{
  uint32_t deviceCount;
  vkEnumeratePhysicalDevices(this->instance, &deviceCount, NULL);
  if (!deviceCount)
  {
    DEBUG_ERROR("failed to find a GPU with Vulkan support!");
    return false;
  }

  // enumerate the devices and choose one that suits best
  VkPhysicalDevice physicalDevices[deviceCount];
  int32_t          scores[deviceCount];
  int32_t          maxScore = 0;

  vkEnumeratePhysicalDevices(this->instance, &deviceCount, physicalDevices);
  for(int i = 0; i < deviceCount; ++i)
  {
    VkPhysicalDevice           pd = physicalDevices[i];
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures   features;
    scores[i] = 0;

    vkGetPhysicalDeviceProperties(pd, &properties);
    vkGetPhysicalDeviceFeatures  (pd, &features  );

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueFamilyCount, NULL);
    if (!queueFamilyCount)
      continue;

    VkQueueFamilyProperties queueFamilies[queueFamilyCount];
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueFamilyCount, queueFamilies);

    bool complete = false;
    this->queues.graphics = -1;
    this->queues.present  = -1;
    for(int i = 0; i < queueFamilyCount; ++i)
    {
      const VkQueueFamilyProperties f = queueFamilies[i];
      if (this->queues.graphics == -1 && f.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        this->queues.graphics = i;

      if (this->queues.present == -1)
      {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, this->surface, &presentSupport);

        if (presentSupport)
          this->queues.present = i;
      }

      if (this->queues.graphics != -1 && this->queues.present != -1)
      {
        complete = true;
        break;
      }
    }

    if (!complete)
      continue;

    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(pd, NULL, &extensionCount, NULL);
    if (!extensionCount)
      continue;

    complete = false;
    VkExtensionProperties extensions[extensionCount];
    vkEnumerateDeviceExtensionProperties(pd, NULL, &extensionCount, extensions);
    for(int i = 0; i < extensionCount; ++i)
      if (strncmp(
        extensions[i].extensionName,
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        sizeof(VK_KHR_SWAPCHAIN_EXTENSION_NAME)) == 0)
      {
        complete = true;
        break;
      }

    if (!complete)
      continue;

    switch(properties.deviceType)
    {
      case VK_PHYSICAL_DEVICE_TYPE_OTHER:
        scores[i] += 100;
        break;

      case VK_PHYSICAL_DEVICE_TYPE_CPU:
        scores[i] += 200;
        break;

      case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        scores[i] += 300;
        break;

      case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        scores[i] += 400;
        break;

      case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        scores[i] += 500;
        break;

      // invalid deviceType
      default:
        scores[i] -= 100;
        break;
    }

    scores[i] += features.logicOp ? 10 : 0;
    scores[i] += properties.limits.maxImageDimension2D / 1000;

    if (scores[i] > maxScore)
      maxScore = scores[i];
  }

  this->physicalDevice = VK_NULL_HANDLE;
  for(int i = 0; i < deviceCount; ++i)
    if (scores[i] == maxScore)
    {
      this->physicalDevice = physicalDevices[i];
      VkPhysicalDeviceProperties properties;
      vkGetPhysicalDeviceProperties(this->physicalDevice, &properties);

      DEBUG_INFO("Score         : %d"  , scores[i]                            );
      DEBUG_INFO("API Version   : 0x%x", properties.apiVersion                );
      DEBUG_INFO("Driver Version: 0x%x", properties.driverVersion             );
      DEBUG_INFO("Vendor ID     : 0x%x", properties.vendorID                  );
      DEBUG_INFO("Device ID     : 0x%x", properties.deviceID                  );
      DEBUG_INFO("Device Name   : %s"  , properties.deviceName                );
      DEBUG_INFO("maxImageDim2D : %d"  , properties.limits.maxImageDimension2D);
      break;
    }

  if (this->physicalDevice == VK_NULL_HANDLE)
  {
    DEBUG_ERROR("Suitable GPU not found");
    return false;
  }

  return true;
}

static bool create_logical_device(struct LGR_Vulkan * this)
{
  float priority = 1.0f;
  VkDeviceQueueCreateInfo queueInfo[2] =
  {
    {
      .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = this->queues.graphics,
      .queueCount       = 1,
      .pQueuePriorities = &priority
    },
    {
      .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = this->queues.present,
      .queueCount       = 1,
      .pQueuePriorities = &priority
    }
  };

  VkPhysicalDeviceFeatures features;
  memset(&features, 0, sizeof(VkPhysicalDeviceFeatures));

  const char * extensions[1] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

  VkDeviceCreateInfo createInfo;
  memset(&createInfo, 0, sizeof(VkDeviceCreateInfo));
  createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pQueueCreateInfos       = queueInfo;
  createInfo.queueCreateInfoCount    = 2;
  createInfo.pEnabledFeatures        = &features;
  createInfo.enabledExtensionCount   = 1;
  createInfo.ppEnabledExtensionNames = extensions;

  if (vkCreateDevice(this->physicalDevice, &createInfo, NULL, &this->device) != VK_SUCCESS)
  {
    DEBUG_ERROR("Failed to create the logical device");
    return false;
  }
  this->freeDevice = true;

  vkGetDeviceQueue(this->device, this->queues.graphics, 0, &this->graphics_q);
  vkGetDeviceQueue(this->device, this->queues.present , 0, &this->present_q );
  return true;
}

static bool create_surface(struct LGR_Vulkan * this, SDL_Window * window)
{
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);
  if (!SDL_GetWindowWMInfo(window, &info))
  {
    DEBUG_ERROR("Failed to obtain SDL window information");
    return false;
  }

  switch(info.subsystem)
  {
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    case SDL_SYSWM_X11:
      {
        VkXlibSurfaceCreateInfoKHR createInfo;
        memset(&createInfo, 0, sizeof(VkXlibSurfaceCreateInfoKHR));
        createInfo.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        createInfo.dpy    = info.info.x11.display;
        createInfo.window = info.info.x11.window;

        if (vkCreateXlibSurfaceKHR(this->instance, &createInfo, NULL, &this->surface) != VK_SUCCESS)
        {
          DEBUG_ERROR("Failed to create Xcb Surface");
          return false;
        }
        break;
      }
#endif
#if 0
    case SDL_SYSWM_MIR:
      break;

    case SDL_SYSWM_WAYLAND:
      break;

    case SDL_SYSWM_ANDROID:
      break;
#endif
    default:
      DEBUG_ERROR("Unsupported window subsystem");
      return false;
  }

  this->freeSurface = true;
  return true;
}